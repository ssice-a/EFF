"""Verify the real source FBX palettes without touching an open Blender scene."""
import json
import sys
from pathlib import Path

import bpy
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from naraka_blender.importer import import_package
from naraka_blender.format import load_package, read_mesh, safe_path

package, baseline, output = map(Path, sys.argv[sys.argv.index('--')+1:])
root, manifest = load_package(package)
old_root, old = load_package(baseline)
old_renderers = {r['id']: r for r in old['renderers']}
unchanged = ('vertices', 'normals', 'tangents', 'colors', 'uvs', 'submeshes',
             'indices', 'skin', 'bindposes', 'bone_hashes', 'blend_vertices',
             'blend_channels', 'blend_frames', 'blend_weights')
for renderer in manifest['renderers']:
    before = old_renderers[renderer['id']]
    assert renderer['materialSlots'] == before['materialSlots']
    assert renderer['submeshSlots'] == before['submeshSlots']
    current_mesh = read_mesh(safe_path(root, renderer['mesh']))
    old_mesh = read_mesh(safe_path(old_root, before['mesh']))
    for key in unchanged:
        assert current_mesh[key] == old_mesh[key], (renderer['name'], key)
    assert len(renderer['bones']) == len(current_mesh['bindposes'])
    assert len(current_mesh['bone_paths']) == len(renderer['bones'])
assert manifest['materials'] == old['materials']
assert manifest['textures'] == old['textures']

result = import_package(package)
collection = bpy.data.collections[result['collection']]
rig = next(o for o in collection.all_objects if o.type == 'ARMATURE')
meshes = {o['naraka_renderer']: o for o in collection.all_objects if o.type == 'MESH'}
assert len(rig.data.bones) == 118
assert not any(b.name.startswith('bone_') for b in rig.data.bones)
nodes = manifest['nodes']
node_to_bone = {i: rig.data.bones[nodes[i]['name']] for r in manifest['renderers'] for i in r['bones']}
maximum = 0.0
for renderer in manifest['renderers']:
    obj = meshes[renderer['id']]
    assert [g.name for g in obj.vertex_groups] == [nodes[i]['name'] for i in renderer['bones']]
    evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    geometry = evaluated.to_mesh()
    try:
        maximum = max(maximum, max(((a.co-b.co).length for a,b in zip(obj.data.vertices,geometry.vertices)),default=0))
    finally:
        evaluated.to_mesh_clear()
for index, bone in node_to_bone.items():
    parent = nodes[index]['parent']
    while parent >= 0 and parent not in node_to_bone:
        parent = nodes[parent]['parent']
    assert bone.parent == node_to_bone.get(parent), bone.name
assert maximum < 1e-5, maximum
assert sum(b.parent is not None for b in rig.data.bones) == 117
output.mkdir(parents=True,exist_ok=True)
report = {'renderers':len(meshes),'bones':len(rig.data.bones),'parentedBones':117,
          'maxRestSkinError':maximum,'originalGeometryWeightsMaterialsUnchanged':True,
          'paletteOrderPreserved':True,'sourceParentingPreserved':True}
(output/'verification.json').write_text(json.dumps(report,indent=2),'utf-8')
print('BONE_IMPORT_PASS',json.dumps(report))
