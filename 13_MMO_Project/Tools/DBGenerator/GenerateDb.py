import argparse
import os
from pathlib import Path
import re
import shutil
import stat
import sys
from tempfile import NamedTemporaryFile

try:
    import yaml
except ImportError:
    sys.exit('DBGenerator: install dependencies from Tools/DBGenerator/requirements.txt.')


DATABASE = Path(__file__).resolve().parents[2] / 'Database'
PACKETS = DATABASE.parent / 'Common/Packet'
STORAGE = DATABASE / 'YamlSchema'
SERVER = DATABASE / 'Generated/Server'
ALL_SCHEMAS = '// 원본: YamlSchema/'
SCHEMA_PATH = re.compile(r'([A-Z][a-z][A-Za-z0-9]{0,61})/([A-Z][a-z][A-Za-z0-9]{0,61})/([A-Za-z][A-Za-z0-9]{0,62})\.yaml')
DEVICE_NAME = re.compile(r'CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9]', re.IGNORECASE)
SQL_NAME = re.compile(r'[a-z][a-z0-9_]{0,63}')
FIELD_NAME = re.compile(r'm_[a-z][A-Za-z0-9]{0,61}')
RECORD_NAME = re.compile(r'SDb[A-Z][A-Za-z0-9]{0,60}')
INTEGER_TYPES = {'uint32': 'INT UNSIGNED', 'uint64': 'BIGINT UNSIGNED'}
TABLE_OPTIONS = {'engine': 'InnoDB', 'charset': 'utf8mb4', 'collation': 'utf8mb4_bin'}
GENERATED_MARKER = '// DBGenerator 생성 파일. 직접 수정하지 않는다.\n'


class SchemaError(ValueError):
    pass


class SchemaLoader(yaml.SafeLoader):
    def compose_node(self, parent, index):
        event = self.peek_event()
        if isinstance(event, yaml.AliasEvent) or event.anchor is not None or event.tag is not None:
            raise SchemaError(f'YAML anchors, aliases and explicit tags are unsupported (line {event.start_mark.line + 1})')
        return super().compose_node(parent, index)

    def construct_mapping(self, node, deep=False):
        keys = set()
        for key, _ in node.value:
            if key.tag != 'tag:yaml.org,2002:str':
                raise SchemaError(f'YAML key must be a string (line {key.start_mark.line + 1})')
            if key.value in keys:
                raise SchemaError(f'duplicate YAML key {key.value!r} (line {key.start_mark.line + 1})')
            keys.add(key.value)
        return super().construct_mapping(node, deep)


def require_keys(value, context, required, optional=()):
    if not isinstance(value, dict):
        raise SchemaError(f'{context}: expected a mapping')
    if unknown := value.keys() - set(required) - set(optional):
        raise SchemaError(f'{context}: unknown key {min(unknown)!r}')
    if missing := set(required) - value.keys():
        raise SchemaError(f'{context}: missing key {min(missing)!r}')


def require_name(value, pattern, context):
    if not isinstance(value, str) or not pattern.fullmatch(value):
        raise SchemaError(f'{context}: invalid name')


def read_schema(source):
    with source.open(encoding='utf-8') as stream:
        schema = yaml.load(stream, Loader=SchemaLoader)

    require_keys(schema, 'schema', ('table', 'record', 'options', 'columns', 'primary_key'), ('projection',))
    require_name(schema['table'], SQL_NAME, 'table')
    require_name(schema['record'], RECORD_NAME, 'record')
    if schema['options'] != TABLE_OPTIONS:
        raise SchemaError('options: expected InnoDB, utf8mb4 and utf8mb4_bin')

    columns = schema['columns']
    if not isinstance(columns, list) or not columns:
        raise SchemaError('columns: expected a nonempty list')
    nullable_by_column = {}
    fields = set()
    for index, column in enumerate(columns, 1):
        context = f'columns[{index}]'
        require_keys(column, context, ('name', 'field', 'cpp_type', 'mysql_type', 'nullable'), ('default',))
        name, field = column['name'], column['field']
        require_name(name, SQL_NAME, f'{context}.name')
        context += f' ({name})'
        require_name(field, FIELD_NAME, f'{context}.field')
        if name in nullable_by_column:
            raise SchemaError(f'{context}: duplicate column {name!r}')
        if field in fields:
            raise SchemaError(f'{context}: duplicate field {field!r}')

        cpp_type = column['cpp_type']
        if not isinstance(cpp_type, str) or cpp_type not in INTEGER_TYPES:
            raise SchemaError(f'{context}.cpp_type: expected uint32 or uint64')
        if column['mysql_type'] != INTEGER_TYPES[cpp_type]:
            raise SchemaError(f'{context}.mysql_type: range mismatch; expected {INTEGER_TYPES[cpp_type]}')
        nullable = column['nullable']
        if type(nullable) is not bool:
            raise SchemaError(f'{context}.nullable: expected a boolean')
        if nullable and ('default' not in column or column['default'] is not None):
            raise SchemaError(f'{context}.default: nullable columns require explicit null')
        if not nullable and 'default' in column:
            raise SchemaError(f'{context}.default: non-nullable columns must omit default')
        nullable_by_column[name] = nullable
        fields.add(field)

    primary_key = schema['primary_key']
    if (not isinstance(primary_key, list) or len(primary_key) != 1 or not isinstance(primary_key[0], str)
            or nullable_by_column.get(primary_key[0]) is not False):
        raise SchemaError('primary_key: expected exactly one declared non-nullable integer column')
    return schema


def render_header(schema, namespace, origin):
    lines = [f'{GENERATED_MARKER}{origin}', '#pragma once', '', f'namespace {namespace}', '{', f"struct {schema['record']}", '{']
    for column in schema['columns']:
        cpp_type = f"std::{column['cpp_type']}_t"
        if column['nullable']:
            cpp_type = f'std::optional<{cpp_type}>'
        lines.append(f"\t{cpp_type} {column['field']}{{}};")
    lines.extend(('};', '}', ''))
    return '\n'.join(lines).encode('utf-8')


def read_projection(schema):
    projection = schema.get('projection')
    require_keys(projection, 'projection', ('schema', 'fields'))
    selector = projection['schema']
    match = SCHEMA_PATH.fullmatch(selector) if isinstance(selector, str) else None
    if not match or any(DEVICE_NAME.fullmatch(part) for part in match.groups()):
        raise SchemaError('projection.schema: expected <Protocol>/<Category>/<File>.yaml')
    protocol, category, stem = match.groups()
    namespace = 'WorldServer' if protocol == 'World' else protocol
    relative = f'{protocol}/Schema/{category}/{stem}.yaml'
    public = PACKETS / relative
    for path in (*reversed(public.parents[:5]), public):
        reject_link(path)
    if public.resolve(strict=True).relative_to(PACKETS).as_posix() != relative:
        raise SchemaError('projection.schema: use the exact on-disk spelling')

    # 패킷 생성기와 공개 필드의 해석을 공유하되 생성기 자체는 실행하지 않는다.
    sys.path.insert(0, str(DATABASE.parent / 'Tools/PacketGenerator'))
    try:
        from PacketStructSchema import parse_struct
        with public.open(encoding='utf-8') as stream:
            dto = parse_struct(yaml.load(stream, Loader=SchemaLoader), namespace)
    except (ValueError, ImportError, yaml.YAMLError) as error:
        raise SchemaError(f'projection.schema ({relative}): {error}') from None
    finally:
        sys.path.pop(0)
    expected = f"{schema['record'][3:]}Data"
    if dto['name'] != expected:
        raise SchemaError(f'projection.schema: expected struct {expected}')
    fields = projection['fields']
    if not isinstance(fields, list) or not fields:
        raise SchemaError('projection.fields: expected a nonempty list')
    columns = {column['name']: column for column in schema['columns']}
    targets = {field['name']: field for field in dto['fields']}
    mapped, used = {}, set()
    for index, field in enumerate(fields, 1):
        context = f'projection.fields[{index}]'
        require_keys(field, context, ('column', 'field'), ('null_to',))
        source, target = field['column'], field['field']
        if not isinstance(source, str) or source not in columns:
            raise SchemaError(f'{context}: unknown storage column')
        if not isinstance(target, str) or target not in targets:
            raise SchemaError(f'{context}: unknown public field')
        if source in used or target in mapped:
            raise SchemaError(f'{context}: duplicate column or public field')
        column = columns[source]
        if column['cpp_type'] != targets[target]['field_type']:
            raise SchemaError(f'{context}: type mismatch for {target}')
        if column['nullable']:
            if type(field.get('null_to')) is not int or field['null_to'] != 0:
                raise SchemaError(f'{context}: nullable column requires null_to: 0')
        elif 'null_to' in field:
            raise SchemaError(f'{context}: null_to requires a nullable column')
        mapped[target] = column
        used.add(source)
    if missing := targets.keys() - mapped.keys():
        raise SchemaError(f'projection.fields: missing public field {min(missing)!r}')
    return dto, mapped


def render_projection(schema, namespace, origin):
    dto, mapped = read_projection(schema)
    record = schema['record']
    target = f"::{dto['namespace']}::S{dto['name']}"
    lines = [origin, f'namespace {namespace}', '{',
             '[[nodiscard]]', f'inline {target} To{dto["name"]}(const {record} &record) noexcept', '{', f'\t{target} data{{}};']
    for field in dto['fields']:
        column = mapped[field['name']]
        value = f"record.{column['field']}" + ('.value_or(0)' if column['nullable'] else '')
        lines.append(f"\tdata.{field['member']} = {value};")
    lines.extend(('\treturn data;', '}', '}'))
    return '\n'.join(lines)


def reject_link(path):
    try:
        info = path.lstat()
    except FileNotFoundError:
        return
    if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
        raise SchemaError(f'linked path is not supported: {path}')


def previous_header(output, origin):
    for directory in reversed(output.parents):
        if not directory.is_relative_to(DATABASE):
            continue
        reject_link(directory)
        if directory.exists() and (not directory.is_dir() or directory.resolve().name != directory.name):
            raise SchemaError(f'output directory collision: {directory}')
    if not output.parent.exists():
        return None
    previous = None
    for existing in output.parent.iterdir():
        if existing.name.casefold() != output.name.casefold():
            continue
        reject_link(existing)
        if existing.name != output.name or not existing.is_file():
            raise SchemaError(f'output collision: {existing.name}')
        previous = existing.read_bytes()
        if previous.splitlines()[:2] != f'{GENERATED_MARKER}{origin}'.encode('utf-8').splitlines():
            raise SchemaError(f'output collision: {existing.name} belongs to another source or is not generated')
    return previous


def publish_headers(outputs):
    # 모든 대상의 소유권을 확인한 뒤에만 임시 파일을 만든다.
    changes = [(output, data, previous_header(output, origin)) for output, data, origin in outputs]
    changes = [(output, data, old) for output, data, old in changes if data != old]
    staged, backups, published, created = {}, {}, [], []
    try:
        for output, data, old in changes:
            for directory in reversed(output.parents):
                if directory.is_relative_to(SERVER.parent) and not directory.exists():
                    directory.mkdir()
                    created.append(directory)
            with NamedTemporaryFile(dir=output.parent, prefix='.db-generator-', suffix='.tmp', delete=False) as stream:
                staged[output] = Path(stream.name)
                stream.write(data)
            if old is not None and len(changes) > 1:
                with NamedTemporaryFile(dir=output.parent, prefix='.db-generator-', suffix='.bak', delete=False) as stream:
                    backups[output] = Path(stream.name)
                shutil.copy2(output, backups[output])
        for output, _, _ in changes:
            os.replace(staged[output], output)
            published.append(output)
    except OSError:
        # 파일 묶음의 교체가 실패하면 이미 교체한 것만 역순으로 되돌린다.
        for output in reversed(published):
            backup = backups.pop(output, None)
            try:
                if backup is None:
                    output.unlink()
                else:
                    os.replace(backup, output)
            except OSError as rollback:
                print(f'DBGenerator: rollback failed: {output}, backup: {backup}: {rollback}', file=sys.stderr)
        raise
    finally:
        for path in (*staged.values(), *backups.values()):
            try:
                if path.exists():
                    if os.name == 'nt' and path.suffix == '.bak':
                        path.chmod(stat.S_IWRITE)
                    path.unlink()
            except OSError as cleanup:
                print(f'DBGenerator: temporary file cleanup failed: {path}: {cleanup}', file=sys.stderr)
        if len(published) != len(changes):
            for directory in reversed(created):
                try:
                    directory.rmdir()
                except OSError:
                    pass


def load_record(selector):
    match = SCHEMA_PATH.fullmatch(selector)
    if not match or any(DEVICE_NAME.fullmatch(part) for part in match.groups()):
        raise SchemaError('schema path: expected <Database>/<Category>/<File>.yaml relative to YamlSchema')
    logical_db, category, _ = match.groups()
    source = STORAGE / selector
    for path in (DATABASE, *reversed(source.parents[:3]), source):
        reject_link(path)
    # Windows의 대소문자 별칭이 같은 YAML에 다른 네임스페이스를 부여하지 않게 한다.
    if source.resolve(strict=True).relative_to(DATABASE).as_posix() != f'YamlSchema/{selector}':
        raise SchemaError('schema path: use the exact on-disk spelling')
    try:
        schema = read_schema(source)
    except (SchemaError, yaml.YAMLError, OSError, UnicodeError, RecursionError) as error:
        raise SchemaError(f'{selector}: {error}') from None
    origin = f'// 원본: YamlSchema/{selector}'
    namespace = f'Database::{logical_db}::{category}'
    return SERVER / logical_db / category / f"{schema['record']}.h", schema, namespace, origin


def load_records():
    reject_link(DATABASE)
    reject_link(STORAGE)
    if not STORAGE.is_dir():
        raise SchemaError(f'schema directory not found: {STORAGE}')
    selectors = []

    def fail_walk(error):
        raise error

    for base, dirs, files in os.walk(STORAGE, onerror=fail_walk):
        folder = Path(base)
        for name in dirs:
            reject_link(folder / name)
        for name in files:
            if Path(name).suffix.lower() in ('.yaml', '.yml'):
                selectors.append((folder / name).relative_to(STORAGE).as_posix())
    if not selectors:
        raise SchemaError('no storage schema files')
    records, seen = [], set()
    for selector in sorted(selectors, key=lambda name: (name.casefold(), name)):
        entry = load_record(selector)
        name = entry[0].relative_to(SERVER).as_posix().casefold()
        if name in seen:
            raise SchemaError(f'{selector}: duplicate storage header: {entry[0].name}')
        seen.add(name)
        records.append(entry)
    return records


def generate(selector, projection):
    records = load_records() if selector is None else [load_record(selector)]
    prefix = f'{GENERATED_MARKER}{ALL_SCHEMAS}\n#pragma once\n'
    if projection:
        parts = []
        for _, schema, namespace, origin in records:
            if 'projection' in schema:
                try:
                    parts.append(render_projection(schema, namespace, origin))
                except (SchemaError, OSError, UnicodeError, RecursionError) as error:
                    raise SchemaError(f'{origin}: {error}') from None
        text = prefix if not parts else f'{prefix}\n' + '\n\n'.join(parts) + '\n'
        outputs = [(SERVER / 'DbPacketConversions.h', text.encode('utf-8'), ALL_SCHEMAS)]
    else:
        outputs = [(output, render_header(schema, namespace, origin), origin) for output, schema, namespace, origin in records]
        if selector is None:
            names = sorted(output.relative_to(SERVER).as_posix() for output, _, _ in outputs)
            includes = '\n'.join(f'#include "{name}"' for name in names)
            text = f'{prefix}\n#include <cstdint>\n#include <optional>\n\n{includes}\n'
            outputs.append((SERVER / 'IncludeDbAll.h', text.encode('utf-8'), ALL_SCHEMAS))
    publish_headers(outputs)
    return [output.relative_to(DATABASE.parent).as_posix() for output, _, _ in outputs]


def main():
    parser = argparse.ArgumentParser(description='Generate a server storage header or a public value projection.')
    parser.add_argument('--projection', action='store_true', help='Generate all registered conversions in one server header.')
    parser.add_argument('schema', nargs='?', help='Optional single storage schema path relative to Database/YamlSchema')
    args = parser.parse_args()
    if args.projection and args.schema is not None:
        parser.error('--projection always generates all registered mappings; omit schema')
    try:
        outputs = generate(args.schema, args.projection)
    except (SchemaError, yaml.YAMLError, OSError, UnicodeError, RecursionError) as error:
        print(f'DBGenerator: {args.schema or "YamlSchema"}: {error}', file=sys.stderr)
        return 1
    # 성공 경로 안내에 실패해도 이미 교체한 산출물의 결과는 바꾸지 않는다.
    try:
        print('\n'.join(outputs), flush=True)
    except OSError:
        try:
            sys.stdout.close()
        except OSError:
            pass
    return 0


if __name__ == '__main__':
    sys.exit(main())
