import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import sys
from tempfile import NamedTemporaryFile

try:
    from jinja2 import Environment, FileSystemLoader, StrictUndefined
    import yaml
except ImportError as error:
    sys.exit(f'Packet generator: {error}. Install dependencies from Tools/PacketGenerator/requirements.txt.')


class GenerationError(ValueError):
    pass


class PacketLoader(yaml.SafeLoader):
    def compose_node(self, parent, index):
        event = self.peek_event()
        if isinstance(event, yaml.AliasEvent) or event.anchor is not None or event.tag is not None:
            raise GenerationError(f'YAML anchors, aliases and explicit tags are unsupported (line {event.start_mark.line + 1})')
        return super().compose_node(parent, index)

    def construct_mapping(self, node, deep=False):
        keys = set()
        for key, _ in node.value:
            if key.tag != 'tag:yaml.org,2002:str':
                raise GenerationError(f'YAML mapping key must be a plain string (line {key.start_mark.line + 1})')
            if key.value in keys:
                raise GenerationError(f'duplicate key {key.value!r} (line {key.start_mark.line + 1})')
            keys.add(key.value)
        return super().construct_mapping(node, deep)


NAME = re.compile(r'[A-Za-z][A-Za-z0-9]{0,79}')
FIELD_NAME = re.compile(r'[a-z][a-z0-9]*(?:_[a-z][a-z0-9]*)*')
DEVICE = re.compile(r'CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9]', re.I)
MANIFEST = '.packet-generator.json'
INTEGER_TYPES = {f'{sign}int{width}': f'std::{sign}int{width}_t'
                 for width in (8, 16, 32, 64) for sign in ('', 'u')}
FIXED_TYPES = {'utf16_fixed': ('WCHAR', 'code_units'),
               'bytes_fixed': ('char', 'bytes')}
DIRECTIONS = {'req': 'client_to_server', 'res': 'server_to_client'}
CPP_PARAMETER_RESERVED = set('''
    alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t
    class compl concept const const_cast consteval constexpr constinit continue co_await co_return co_yield
    decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto
    if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected
    public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct
    switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void
    volatile wchar_t while xor xor_eq interface near far small pascal cdecl std
'''.split()) | {'packet', 'sessionId', 'spPacket'}
SIDE_OUTPUTS = {side: {f'{side}Packet{kind}.g.{ext}' for kind in ('Send', 'Dispatch') for ext in ('h', 'cpp')}
                | {f'I{side}PacketHandler.g.h'} for side in ('Client', 'Server')}


def sender_name(name, kind):
    prefix = kind.capitalize()
    if name.startswith(prefix) and len(name) > len(prefix):
        name = name[len(prefix):]
    elif kind == 'noti' and name.endswith(prefix) and len(name) > len(prefix):
        name = name[:-len(prefix)]
    return f'Send{prefix}{name}'


def set_send_parameters(fields):
    # 정상 필드명은 먼저 확보해 접미사를 붙인 이름과 충돌하지 않게 한다.
    names = {field['member'][2:] for field in fields}
    used = names | CPP_PARAMETER_RESERVED
    for field in fields:
        name = field['member'][2:]
        if name in CPP_PARAMETER_RESERVED:
            base = f'{name}Value'
            name, suffix = base, 2
            while name in used:
                name = f'{base}{suffix}'
                suffix += 1
            used.add(name)
        cpp_type = field['cpp_type']
        field['parameter'] = f'const {cpp_type} &{name}' if field['copy_by_ref'] else f'{cpp_type} {name}'
        field['argument'] = name


def check_name(name, context):
    if not isinstance(name, str) or not NAME.fullmatch(name) or DEVICE.fullmatch(name):
        raise GenerationError(f'invalid {context}: {name!r}')


def check_packet_name(name, context):
    check_name(name, context)
    # 열거 값은 PascalCase로 제한해 C++ 키워드와 같은 이름을 막는다.
    if not name[0].isupper() or name == 'EPacketType':
        raise GenerationError(f'invalid {context}: {name!r}')


def mapping(value, required, optional=()):
    if not isinstance(value, dict):
        raise GenerationError('expected a mapping')
    if unknown := value.keys() - set(required) - set(optional):
        raise GenerationError(f'unknown key: {min(unknown)!r}')
    if missing := set(required) - value.keys():
        raise GenerationError(f'{min(missing)} is required')
    return value


def sequence(value):
    if not isinstance(value, list):
        raise GenerationError('expected a list')
    return value


def integer(value, minimum, maximum):
    return type(value) is int and minimum <= value <= maximum


def parse_field(value):
    value = mapping(value, ('name', 'type'), ('bytes', 'code_units'))
    name, field_type = value['name'], value['type']
    if not isinstance(name, str) or not FIELD_NAME.fullmatch(name) or len(name) > 80:
        raise GenerationError(f'invalid field name: {name!r}')
    first, *words = name.split('_')
    member = 'm_' + first + ''.join(word.capitalize() for word in words)
    if not isinstance(field_type, str):
        raise GenerationError(f'unsupported type for {name}')
    if field_type in FIXED_TYPES:
        scalar, size_key = FIXED_TYPES[field_type]
        mapping(value, ('name', 'type', size_key))
        size = value[size_key]
        if not integer(size, 1, 65535):
            raise GenerationError(f'{name}.{size_key} must be a positive integer up to 65535')
        cpp_type = f'std::array<{scalar}, {size}>'
    else:
        mapping(value, ('name', 'type'))
        if field_type in INTEGER_TYPES:
            cpp_type = INTEGER_TYPES[field_type]
        elif field_type == 'utf16':
            cpp_type = 'std::wstring'
        else:
            raise GenerationError(f'unsupported type: {field_type!r}')
    return dict(member=member, cpp_type=cpp_type, copy_by_ref=field_type not in INTEGER_TYPES)


def parse_packet(value):
    value = mapping(value, ('name', 'id', 'kind', 'fields'), ('direction',))
    name, packet_id, kind = value['name'], value['id'], value['kind']
    check_packet_name(name, 'packet name')
    if not integer(packet_id, 1, 65535):
        raise GenerationError('packet id must be an integer from 1 to 65535')
    if kind not in ('req', 'res', 'noti'):
        raise GenerationError(f'invalid packet kind: {kind!r}')
    if kind == 'noti':
        mapping(value, ('name', 'id', 'kind', 'fields', 'direction'))
        direction = value['direction']
        if direction not in ('client_to_server', 'server_to_client'):
            raise GenerationError(f'invalid direction: {direction!r}')
    else:
        mapping(value, ('name', 'id', 'kind', 'fields'))
        direction = DIRECTIONS[kind]
    fields = [parse_field(field) for field in sequence(value['fields'])]
    if len({field['member'] for field in fields}) != len(fields):
        raise GenerationError(f'{name}: duplicate field')
    set_send_parameters(fields)
    is_client = direction == 'client_to_server'
    parameters = [] if is_client else ['UINT64 sessionId']
    parameters.extend(field['parameter'] for field in fields)
    return dict(name=name, id=packet_id, kind=kind, direction=direction, fields=fields,
                send_name=sender_name(name, kind), send_parameters=parameters,
                send_result='bool ' if is_client else 'NetworkLib::CSerializableBuffer<FALSE> *',
                copies_string=any(field['cpp_type'] == 'std::wstring' for field in fields))


def is_link(path):
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    return stat.S_ISLNK(info.st_mode) or bool(getattr(info, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT)


def load_protocol(root):
    files = []
    for path in sorted(root.rglob('*'), key=lambda path: (str(path).casefold(), str(path))):
        if is_link(path):
            raise GenerationError(f'links are not allowed in schema: {path}')
        if path.suffix.lower() in ('.yaml', '.yml') and path.is_file():
            files.append(path)
    packets, reserved, groups, names, ids, outputs = [], [], {}, set(), set(), set()
    sender_names = set()

    def register(entry):
        if entry['id'] in ids:
            raise GenerationError(f'duplicate packet id: {entry["id"]}')
        if entry['name'].casefold() in names:
            raise GenerationError(f'duplicate packet name: {entry["name"]}')
        ids.add(entry['id'])
        names.add(entry['name'].casefold())

    # 렌더링 전에 집계 헤더까지 포함해 Windows의 대소문자 충돌을 확인한다.
    for path in files:
        relative = path.relative_to(root)
        if relative.suffix != '.yaml' or len(relative.parts) != 2:
            raise GenerationError(f'expected <Content>/<Packet>.yaml: {relative}')
        group, stem = relative.parts[0], relative.stem
        check_name(group, 'content directory')
        check_name(stem, 'schema filename')
        if group not in groups:
            aggregator = f'{group}/{group}PacketAll.g.h'.casefold()
            if aggregator in outputs:
                raise GenerationError(f'output name collision: {group}')
            outputs.add(aggregator)
            groups[group] = []
        output = f'{group}/{stem}.g.h'
        if output.casefold() in outputs:
            raise GenerationError(f'output name collision: {output}')
        outputs.add(output.casefold())
        groups[group].append((output, path))

    contents = {}
    for group, schemas in groups.items():
        contents[group] = {}
        for output, path in schemas:
            try:
                document = mapping(yaml.load(path.read_text(encoding='utf-8'), Loader=PacketLoader), ('packets',), ('reserved',))
                local = [parse_packet(value) for value in sequence(document['packets'])]
                kinds = {packet['kind'] for packet in local}
                if len(kinds) != len(local):
                    raise GenerationError('duplicate kind')
                if kinds not in ({'noti'}, {'req', 'res'}, {'req', 'res', 'noti'}):
                    raise GenerationError('req and res must appear together, or use a standalone noti')
                for value in sequence(document.get('reserved', [])):
                    value = mapping(value, ('name', 'id'))
                    check_packet_name(value['name'], 'reserved packet name')
                    if not integer(value['id'], 0, 0):
                        raise GenerationError('reserved packet id must be 0')
                    register(value)
                    reserved.append(value)
                for packet in local:
                    register(packet)
                    sender_key = (packet['direction'], packet['send_name'].casefold())
                    if sender_key in sender_names:
                        raise GenerationError(f'duplicate sender name: {packet["send_name"]}')
                    sender_names.add(sender_key)
                packets.extend(local)
                contents[group][output] = local
            except (GenerationError, UnicodeError, yaml.YAMLError, RecursionError) as error:
                # YAML 오류 전체를 출력하면 원본 데이터까지 노출될 수 있다.
                if isinstance(error, yaml.YAMLError):
                    mark = getattr(error, 'problem_mark', None)
                    error = f'invalid YAML (line {mark.line + 1})' if mark else 'invalid YAML'
                raise GenerationError(f'{path}: {error}') from None
    return contents, packets, reserved


def render_protocol(contents, packets, reserved):
    environment = Environment(loader=FileSystemLoader(Path(__file__).parent / 'templates'), undefined=StrictUndefined,
                              autoescape=False, trim_blocks=True, lstrip_blocks=True, keep_trailing_newline=True)

    def render(template, **values):
        return ('// PacketGenerator generated file. Do not edit.\n' + environment.get_template(template).render(**values)).encode()

    common = {'PacketTypes.g.h': render('types.j2', packets=sorted(reserved + packets, key=lambda packet: packet['id']))}
    includes = ['PacketTypes.g.h']
    for group, headers in contents.items():
        aggregator = f'{group}/{group}PacketAll.g.h'
        includes.append(aggregator)
        common[aggregator] = render('includes.j2', includes=[name.split('/')[1] for name in headers])
        common.update({name: render('structs.j2', packets=local) for name, local in headers.items()})
    common['IncludePacketAll.g.h'] = render('includes.j2', includes=includes)
    sides = []
    for side, send_direction in (('Client', 'client_to_server'), ('Server', 'server_to_client')):
        send = [packet for packet in packets if packet['direction'] == send_direction]
        receive = [packet for packet in packets if packet['direction'] != send_direction]
        sides.append({f'{side}PacketSend.g.{ext}': render(f'send_{ext}.j2', side=side, packets=send)
                      for ext in ('h', 'cpp')} |
                     {f'{side}PacketDispatch.g.{ext}': render(f'dispatch_{ext}.j2', side=side, packets=receive)
                      for ext in ('h', 'cpp')} |
                     {f'I{side}PacketHandler.g.h': render('handler.j2', side=side, packets=receive)})
    return (common, *sides)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def check_output_name(role, name):
    if role != 'Common':
        return name in SIDE_OUTPUTS[role]
    if name in ('PacketTypes.g.h', 'IncludePacketAll.g.h'):
        return True
    parts = name.split('/')
    return (len(parts) == 2 and bool(NAME.fullmatch(parts[0])) and not DEVICE.fullmatch(parts[0])
            and parts[1].endswith('.g.h') and bool(NAME.fullmatch(parts[1][:-4]))
            and not DEVICE.fullmatch(parts[1][:-4]))


def checked_path(root, name):
    target = root
    for part in PurePosixPath(name).parts:
        target = target / part
        if is_link(target):
            raise GenerationError(f'links are not allowed in output: {target}')
    return target


def read_manifest(root, role):
    path = checked_path(root, MANIFEST)
    if not path.exists():
        return {}
    try:
        def unique_map(pairs):
            result = dict(pairs)
            if len(result) != len(pairs):
                raise ValueError('duplicate manifest key')
            return result

        record = json.loads(path.read_text(encoding='utf-8'), object_pairs_hook=unique_map)
        if set(record) != {'version', 'role', 'files'} or record['version'] != 1 or record['role'] != role:
            raise ValueError('invalid manifest version or role')
        files = record['files']
        if not isinstance(files, dict) or len({name.casefold() for name in files}) != len(files):
            raise ValueError('invalid manifest file list')
        for name, checksum in files.items():
            if not check_output_name(role, name) or not re.fullmatch(r'[0-9a-f]{64}', checksum):
                raise ValueError('invalid manifest file entry')
        return files
    except (ValueError, TypeError, KeyError, RecursionError) as error:
        raise GenerationError(f'{path}: invalid output manifest: {error}') from None


def plan_output(root, role, files):
    before = read_manifest(root, role)
    writes, removals = {}, []
    for name, checksum in before.items():
        target = checked_path(root, name)
        # Git의 Windows 체크아웃에서 바뀐 줄바꿈은 수동 수정으로 보지 않는다.
        if target.exists() and (not target.is_file() or digest(target.read_bytes().replace(b'\r\n', b'\n')) != checksum):
            raise GenerationError(f'modified generated output: {target}')
        if name not in files and target.exists():
            removals.append(target)
    old_names = {name.casefold(): name for name in before}
    for name, data in files.items():
        target = checked_path(root, name)
        old = old_names.get(name.casefold())
        if target.exists():
            # 대소문자만 바뀐 경로도 이전 소유 파일과 동일한지 먼저 확인한다.
            if old is None or (old != name and not target.samefile(checked_path(root, old))):
                raise GenerationError(f'unmanaged output already exists: {target}')
        if old != name or not target.exists() or before[old] != digest(data):
            writes[target] = data
    manifest = root / MANIFEST
    if not manifest.exists() or before != {name: digest(data) for name, data in files.items()}:
        record = dict(version=1, role=role, files={name: digest(data) for name, data in files.items()})
        return writes, removals, {manifest: (json.dumps(record, indent=2, sort_keys=True) + '\n').encode()}
    return writes, removals, {}


def publish(plans):
    # 세 출력 묶음의 모든 새 바이트를 준비한 뒤 기존 파일을 변경한다.
    staged = {}
    try:
        for writes, _, manifests in plans:
            for target, data in (writes | manifests).items():
                target.parent.mkdir(parents=True, exist_ok=True)
                with NamedTemporaryFile(dir=target.parent, prefix='.packet-generator-', delete=False) as file:
                    staged[target] = Path(file.name)
                    file.write(data)
        for writes, removals, _ in plans:
            for target in removals:
                target.unlink()
            for target in writes:
                os.replace(staged[target], target)
        for _, _, manifests in plans:
            for target in manifests:
                os.replace(staged[target], target)
    finally:
        for path in staged.values():
            path.unlink(missing_ok=True)


def generate(schema, common, client, server):
    paths = [path.absolute() for path in (schema, common, client, server)]
    for path in paths:
        if any(is_link(parent) for parent in (path, *path.parents)):
            raise GenerationError(f'links are not allowed in generation roots: {path}')
    paths = [path.resolve() for path in paths]
    for index, path in enumerate(paths):
        if any(path.is_relative_to(other) or other.is_relative_to(path) for other in paths[:index]):
            raise GenerationError(f'generation roots overlap: {path}')
    if not paths[0].is_dir():
        raise GenerationError(f'schema directory not found: {paths[0]}')
    outputs = render_protocol(*load_protocol(paths[0]))
    plans = [plan_output(root, role, files) for root, role, files in zip(paths[1:], ('Common', 'Client', 'Server'), outputs)]
    publish(plans)


def main():
    parser = argparse.ArgumentParser(description='Generate C++ packet DTOs, builders and dispatchers from YAML.')
    for option in ('schema-dir', 'common-out', 'client-out', 'server-out'):
        parser.add_argument(f'--{option}', type=Path, required=True)
    args = parser.parse_args()
    try:
        generate(args.schema_dir, args.common_out, args.client_out, args.server_out)
    except (GenerationError, OSError) as error:
        parser.exit(1, f'Packet generator: {error}\n')


if __name__ == '__main__':
    main()
