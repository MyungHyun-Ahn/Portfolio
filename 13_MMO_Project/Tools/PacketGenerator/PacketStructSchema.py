import re


STRUCT_NAME = re.compile(r'[A-Z][a-zA-Z0-9]{0,63}')
NAMESPACE = re.compile(r'[A-Z][a-z][a-zA-Z0-9]{0,61}')
FIELD_NAME = re.compile(r'[a-z][a-z0-9]*(?:_[a-z][a-z0-9]*)*')
INTEGER_TYPES = {f'{sign}int{width}': f'std::{sign}int{width}_t'
                 for width in (8, 16, 32, 64) for sign in ('', 'u')}


def parse_struct(document, expected_namespace):
    if not isinstance(document, dict) or set(document) != {'struct'}:
        raise ValueError('expected only the struct key in a standalone schema')
    definition = document['struct']
    if not isinstance(definition, dict) or set(definition) != {'name', 'namespace', 'fields'}:
        raise ValueError('struct requires only name, namespace and fields; it has no packet id or kind')
    name, namespace, fields = definition['name'], definition['namespace'], definition['fields']
    if not isinstance(name, str) or not STRUCT_NAME.fullmatch(name) or name == 'PacketType':
        raise ValueError('invalid struct name')
    if not isinstance(namespace, str) or not NAMESPACE.fullmatch(namespace):
        raise ValueError('invalid struct namespace')
    if namespace != expected_namespace:
        raise ValueError(f'struct namespace must match {expected_namespace}')
    if not isinstance(fields, list) or not fields:
        raise ValueError(f'{name}: expected a nonempty fields list')
    members, result = set(), []
    for index, field in enumerate(fields, 1):
        if not isinstance(field, dict) or set(field) != {'name', 'type'}:
            raise ValueError(f'{name}.fields[{index}]: expected name and type')
        field_name, field_type = field['name'], field['type']
        if not isinstance(field_name, str) or len(field_name) > 64 or not FIELD_NAME.fullmatch(field_name):
            raise ValueError(f'{name}.fields[{index}]: invalid name')
        if not isinstance(field_type, str) or field_type not in INTEGER_TYPES:
            raise ValueError(f'{name}.{field_name}: expected a fixed-width integer type')
        first, *words = field_name.split('_')
        member = 'm_' + first + ''.join(word.capitalize() for word in words)
        if member in members:
            raise ValueError(f'{name}.{field_name}: duplicate field')
        members.add(member)
        result.append(dict(name=field_name, member=member, cpp_type=INTEGER_TYPES[field_type], field_type=field_type))
    return dict(name=name, namespace=namespace, fields=result)
