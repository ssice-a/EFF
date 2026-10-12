"""Read exported type trees, including Unity managed-reference payloads.

This is an offline evidence reader, not part of the game DLL or Mod exporter.
"""
import json
import struct
from pathlib import Path


def tree(nodes):
    root = dict(nodes[0], children=[])
    stack = [root]
    for item in nodes[1:]:
        node = dict(item, children=[])
        while stack[-1]['level'] >= node['level']:
            stack.pop()
        stack[-1]['children'].append(node)
        stack.append(node)
    return root


class Reader:
    def __init__(self, data, schema):
        self.data, self.offset = data, 0
        self.refs = {(r['type'], r['ns'], r['asm']): tree(r['nodes'])
                     for r in schema.get('references') or []}

    def take(self, count):
        if count < 0 or self.offset + count > len(self.data):
            raise ValueError('Facial resource read exceeds its object boundary')
        value = self.data[self.offset:self.offset + count]
        self.offset += count
        return value

    def scalar(self, fmt):
        return struct.unpack('<' + fmt, self.take(struct.calcsize('<' + fmt)))[0]

    def align(self):
        self.take((-self.offset) % 4)

    def read(self, node, context=None):
        kind, children = node['type'], node['children']
        fmts = {'SInt8': 'b', 'UInt8': 'B', 'bool': '?', 'char': 'B',
                'short': 'h', 'SInt16': 'h', 'UInt16': 'H', 'int': 'i',
                'SInt32': 'i', 'UInt32': 'I', 'SInt64': 'q', 'UInt64': 'Q',
                'float': 'f', 'double': 'd'}
        if kind in fmts:
            value = self.scalar(fmts[kind])
        elif kind == 'string':
            value = self.take(self.scalar('i')).decode('utf-8', 'strict')
            self.align()
        elif kind == 'ReferencedObjectData':
            info = context['type']
            ref = self.refs.get((info['class'], info['ns'], info['asm']))
            if ref is None:
                raise ValueError('Unknown managed reference type: ' + str(info))
            # Reference schemas can include a host registry definition. The
            # referenced payload itself shares the enclosing object's registry;
            # it does not serialize another copy after its ordinary fields.
            payload = dict(ref, children=[c for c in ref['children']
                                         if c['type'] != 'ManagedReferencesRegistry'])
            value = self.read(payload)
        elif children and children[0]['type'] == 'Array':
            array = children[0]
            count = self.scalar('i')
            if not 0 <= count <= len(self.data) - self.offset:
                raise ValueError('Invalid facial array count')
            element = array['children'][1]
            value = [self.read(element) for _ in range(count)]
            if array['flags'] & 0x4000:
                self.align()
        else:
            value = {}
            for child in children:
                value[child['name']] = self.read(child, value)
        if node['flags'] & 0x4000:
            self.align()
        return value


def read_object(stem):
    stem = Path(stem)
    schema = json.loads(Path(str(stem) + '.types.json').read_text(encoding='utf-8'))
    reader = Reader(Path(str(stem) + '.bin').read_bytes(), schema)
    value = reader.read(tree(schema['nodes']))
    if reader.offset != len(reader.data):
        raise ValueError(f'Unconsumed facial object bytes: {reader.offset}/{len(reader.data)}')
    return value


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('stem')
    parser.add_argument('output')
    args = parser.parse_args()
    value = read_object(args.stem)
    Path(args.output).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding='utf-8')
    print('Read complete facial object:', value.get('m_Name', args.stem))
