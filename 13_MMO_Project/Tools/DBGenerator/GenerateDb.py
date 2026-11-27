import argparse
import os
from pathlib import Path
import re
import stat
import sys
from tempfile import NamedTemporaryFile

try:
    import yaml
except ImportError:
    sys.exit('DBGenerator: install dependencies from Tools/DBGenerator/requirements.txt.')


DATABASE = Path(__file__).resolve().parents[2] / 'Database'
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

    require_keys(schema, 'schema', ('table', 'record', 'options', 'columns', 'primary_key'))
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


def reject_link(path):
    try:
        info = path.lstat()
    except FileNotFoundError:
        return
    if stat.S_ISLNK(info.st_mode) or getattr(info, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
        raise SchemaError(f'linked path is not supported: {path}')


def write_header(output, content, origin):
    output.parent.mkdir(parents=True, exist_ok=True)
    for existing in output.parent.iterdir():
        if existing.name.casefold() != output.name.casefold():
            continue
        reject_link(existing)
        if existing.name != output.name or not existing.is_file():
            raise SchemaError(f'output collision: {existing.name}')
        previous = existing.read_bytes()
        if previous.splitlines()[:2] != f'{GENERATED_MARKER}{origin}'.encode('utf-8').splitlines():
            raise SchemaError(f'output collision: {existing.name} belongs to another source or is not generated')
        if previous == content:
            return

    # 같은 디렉터리에 완성한 뒤 교체하여 쓰기 실패가 기존 파일을 자르지 않게 한다.
    candidate = None
    try:
        with NamedTemporaryFile(dir=output.parent, prefix='.db-generator-', suffix='.tmp', delete=False) as stream:
            candidate = Path(stream.name)
            stream.write(content)
        os.replace(candidate, output)
    finally:
        if candidate is not None:
            candidate.unlink(missing_ok=True)


def generate(selector):
    match = SCHEMA_PATH.fullmatch(selector)
    if not match or any(DEVICE_NAME.fullmatch(part) for part in match.groups()):
        raise SchemaError('schema path: expected <Database>/<Category>/<File>.yaml relative to YamlSchema')
    logical_db, category, _ = match.groups()
    source = DATABASE / 'YamlSchema' / selector
    destination = DATABASE / 'Generated/Server' / logical_db / category
    for path in (DATABASE, *reversed(source.parents[:3]), source):
        reject_link(path)
    # Windows의 대소문자 별칭이 같은 YAML에 다른 네임스페이스를 부여하지 않게 한다.
    if source.resolve(strict=True).relative_to(DATABASE).as_posix() != f'YamlSchema/{selector}':
        raise SchemaError('schema path: use the exact on-disk spelling')
    schema = read_schema(source)
    output = destination / f"{schema['record']}.h"
    for path in (*reversed(destination.parents[:3]), destination):
        reject_link(path)
    origin = f'// 원본: YamlSchema/{selector}'
    content = render_header(schema, f'Database::{logical_db}::{category}', origin)
    write_header(output, content, origin)
    print(output.relative_to(DATABASE.parent).as_posix())


def main():
    parser = argparse.ArgumentParser(description='Generate one server storage header from a DB schema.')
    parser.add_argument('schema', nargs='?', default='GameDB/Player/Character.yaml',
                        help='Schema path relative to Database/YamlSchema, with forward slashes')
    args = parser.parse_args()
    try:
        generate(args.schema)
    except (SchemaError, yaml.YAMLError, OSError, UnicodeError, RecursionError) as error:
        print(f'DBGenerator: {args.schema}: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
