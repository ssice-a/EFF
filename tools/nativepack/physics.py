"""Offline author inputs for new native bone cloths, independent of live models.

Only templates and author data are carried here. Exact Mesh consumers and all
PFB-local references are resolved by the offline compiler, before deserialization.
"""
import copy
import hashlib
import struct

from .type_tree import Value, encode, from_data, require
from .resource_file import encode_resource, encode_strings

KIND = 5
STRUCTURE = 'EFF_NEW_BONE_CLOTH/path+trs+mesh-consumers+native-templates/le64'


class Writer:
    def __init__(self): self.data = bytearray()
    def n(self, value): self.data.extend(struct.pack('<Q', value))
    def text(self, value):
        data = value.encode('utf-8'); self.n(len(data)); self.data.extend(data)
    def blob(self, value): self.n(len(value)); self.data.extend(value)
    def numbers(self, values):
        self.data.extend(struct.pack('<' + str(len(values)) + 'd', *values))


def empty(schema):
    if schema.type == 'Array': return Value(schema, [])
    if schema.type in ('string', 'TypelessData'): return from_data(schema, '' if schema.type == 'string' else b'')
    if not schema.children: return from_data(schema, 0)
    return Value(schema, {child.name: empty(child) for child in schema.children})


def clear_references(value, keep_script=False):
    if isinstance(value.data, dict):
        if set(value.data) == {'m_FileID', 'm_PathID'}:
            value.set('m_FileID', 0); value.set('m_PathID', 0)
        else:
            for key, child in value.data.items():
                if keep_script and key == 'm_Script': continue
                clear_references(child)
    elif isinstance(value.data, list):
        for child in value.data: clear_references(child)


def parameter(tree, edit):
    parts = edit['path'].split('.')
    require(parts[0] == 'serializeData' and not any(p in ('m_FileID', 'm_PathID') for p in parts),
            'Physics parameter attempts to edit a reference')
    value = tree
    for part in parts:
        if part.isdecimal():
            index = int(part)
            require('m_Curve' in parts, 'Physics cannot resize a native topology array')
            array = value.array_node()
            while len(array.data) <= index:
                array.data.append(copy.deepcopy(array.data[-1]) if array.data else
                                  empty(array.schema.children[1]))
            value = array.data[index]
        else:
            value = value[part]
    require(value.schema.type == 'float' if edit['floating'] else
            value.schema.type in ('int', 'SInt32', 'UInt32', 'bool', 'UInt8', 'SInt8'),
            'Physics parameter type differs: ' + edit['path'])
    value.data = edit['value']; value.atom_bytes = None


def cloth_template(tree, group):
    tree = copy.deepcopy(tree)
    require(tree['serializeData']['clothType'].data == 1, 'Physics requires a native BoneCloth template')
    clear_references(tree, keep_script=True)
    # Donor prebuild topology belongs to the donor's bones. It is never copied
    # to a new chain. Native AutoBuild receives new root/selection references.
    tree.data['serializeData2'] = empty(tree['serializeData2'].schema)
    tree['serializeData']['sourceRenderers'].set_items([])
    tree['serializeData']['paintMaps'].set_items([])
    tree['serializeData']['rootBones'].set_items([])
    tree['serializeData']['ignoreFromRootBones'].set_items([])
    tree['serializeData']['colliderCollisionConstraint']['colliderList'].set_items([])
    # Curve key counts belong to the author, including deletion of donor keys.
    curves = {}
    for edit in group['nativeParameters']:
        parts = edit['path'].split('.')
        if 'm_Curve' in parts:
            at = parts.index('m_Curve')
            key = tuple(parts[:at+1])
            curves[key] = max(curves.get(key, 0), int(parts[at+1]) + 1)
    for path, count in curves.items():
        curve = tree
        for key in path: curve = curve[key]
        curve.array_node().data = curve.items()[:count]
    for edit in group['nativeParameters']: parameter(tree, edit)
    tree['serializeData']['clothType'].data = 1
    tree['serializeData']['connectionMode'].data = 0
    tree.set('m_Enabled', 1)
    tree.set('m_Name', group['name'])
    return tree


def collider_template(tree, collider):
    tree = copy.deepcopy(tree); clear_references(tree, keep_script=True)
    tree.set('m_Enabled', 1); tree.set('m_Name', collider['name'])
    tree.set('center', dict(x=0, y=0, z=0))
    a, b = collider['radius'], collider['endRadius']
    tree.set('size', dict(x=a, y=b if collider['shape'] == 'CAPSULE' else 0,
                         z=collider['span'] + a + b if collider['shape'] == 'CAPSULE' else 0))
    if collider['shape'] == 'CAPSULE':
        tree.set('direction', 1); tree.set('reverseDirection', 0)
        tree.set('radiusSeparation', int(a != b))
        tree.set('alignedOnCenter', int(collider['alignedOnCenter']))
    return tree


def group_mesh_targets(group, mesh_bones, rules, object_parts):
    """A chain follows only source Mesh rules whose authored skin uses it."""
    moving = {n['bone'] for n in group['nodes'] if n['role'] == 'MOVE'}
    parts = {p for obj, used in mesh_bones.items() if moving & used
             for p in object_parts.get(obj, ())}
    result = {(t['sourcePath'], t['sourceAsset']) for rule in rules
              if parts.intersection(rule['parts']) for t in rule['targets']}
    require(result, 'Physics chain has no selected weighted Mesh: ' + group['name'])
    return sorted(result)


def build_resource(source, evidence, document, skeleton, targets):
    """Retain tiny native metadata, with no original Mesh or texture payloads."""
    templates = {}
    for component in evidence['components']:
        if component['type'] == 'BeyondBoneCloth' and component['fields']['serializeData']['clothType'] != 1: continue
        templates.setdefault(component['type'], component)
    files, created = {}, {}

    def add(kind, identity, make):
        require(kind in templates, 'Source package lacks native Physics template: ' + kind)
        cab, path_id = templates[kind]['source'].rsplit(':', 1)
        native = source.file({'native': {'cab': cab}})
        if cab not in files:
            files[cab] = copy.copy(native)
            files[cab].objects = dict(native.objects); files[cab].order = list(native.order)
            files[cab].changed, files[cab].added = set(), set()
        file = files[cab]
        pid = file.allocate('physics:' + document['id'] + ':' + identity)
        file.add_tree(pid, int(path_id), make(native.tree(int(path_id))))
        created.setdefault(cab, set()).add(pid)
        return cab, pid

    groups = []
    for group in document['groups']:
        require(group['id'] in targets and targets[group['id']], 'Physics lacks exact Mesh source targets')
        cab, pid = add('BeyondBoneCloth', group['id'], lambda tree: cloth_template(tree, group))
        groups.append((group, cab, pid))
    colliders = []
    for collider in document['colliders']:
        require(collider['shape'] in ('SPHERE', 'CAPSULE'), 'Unsupported new collider shape')
        kind = 'BeyondBone' + ('Sphere' if collider['shape'] == 'SPHERE' else 'Capsule') + 'Collider'
        cab, pid = add(kind, collider['id'], lambda tree: collider_template(tree, collider))
        colliders.append((collider, cab, pid))
    w = Writer(); w.text(STRUCTURE); w.text(document['id'])
    w.n(len(skeleton))
    for bone, record, is_source in skeleton:
        path, parent, position, rotation, scale = record
        w.text(path); w.text(skeleton[parent][1][0] if parent >= 0 else '')
        w.n(int(is_source)); w.n(int(parent >= 0)); w.numbers((*position, *rotation, *scale))
    w.n(len(files))
    for cab, file in sorted(files.items()):
        file.objects = {pid: file.objects[pid] for pid in created[cab]}
        file.order = sorted(created[cab]); w.text(cab); w.blob(file.build())
    w.n(len(colliders))
    for collider, cab, pid in colliders:
        for value in (collider['id'], collider['name'], collider['bone'], cab): w.text(value)
        w.n(pid & 0xffffffffffffffff)
        w.numbers((*collider['position'], *collider['rotation']))
    w.n(len(groups))
    for group, cab, pid in groups:
        for value in (group['id'], group['name'], cab): w.text(value)
        w.n(pid & 0xffffffffffffffff)
        w.n(len(targets[group['id']]))
        for path, asset in targets[group['id']]: w.text(path); w.text(asset)
        w.n(len(group['nodes']))
        for node in group['nodes']: w.text(node['bone']); w.n(int(node['role'] == 'MOVE'))
        w.n(len(group['colliders']))
        for identity in group['colliders']: w.text(identity)
    return encode_resource(KIND, '00' * 16, {'@physics': bytes(w.data),
                           '@source-prefabs': encode_strings([source.manifest['prefab'],
                              next(t['path'].rsplit('/', 1)[-1] for t in evidence['transforms']
                                   if t['inSelectedPrefab'] and t['parent'] is None)])})
