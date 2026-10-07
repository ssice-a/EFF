"""Run with Blender --background --factory-startup; no .blend is opened/saved."""
import argparse
import importlib.util
from pathlib import Path
import sys
import bpy

root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root))
sys.path.insert(0, str(root/'tools/Blender'))
from tools.nativepack.author_source import AuthorCore, AuthorSource
from tools.nativepack.mesh import native_mesh
from tools.nativepack.type_tree import encode, decode

parser = argparse.ArgumentParser()
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--core', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:])
spec = importlib.util.spec_from_file_location('_static_export_addon', root/'tools/Blender/eiem_blender_addon.py')
addon = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = addon
spec.loader.exec_module(addon)
data = bpy.data.meshes.new('StaticTriangle')
data.from_pydata([(0, 0, 0), (1, 0, 0), (0, 1, 0)], [], [(0, 1, 2)])
uv = data.uv_layers.new(name='UV0')
for loop in data.loops:
    uv.data[loop.index].uv = [(0, 0), (1, 0), (0, 1)][loop.vertex_index]
obj = bpy.data.objects.new('StaticTriangle', data)
bpy.context.scene.collection.objects.link(obj)
# Static renderers do not acquire skinning from unused source Mesh metadata.
obj['eiem_bindposes_json'] = '[[]]'
obj['eiem_bone_hashes_json'] = '[1]'
obj['eiem_bone_paths_json'] = '[]'
snapshot = addon.write_mesh(None, obj, return_snapshot=True)
assert not snapshot['skin'] and not snapshot['bone_paths'] and not snapshot['bindposes']
assert snapshot['vertex_count'] == 3 and len(snapshot['indices']) == 3
source = AuthorSource(args.source, AuthorCore(args.core))
resource = next(r for r in source.manifest['resources'] if r['type'] == 'Mesh')
mesh, precision = native_mesh(source.tree(resource), snapshot, 'static-blender-test')
assert mesh['m_VertexData']['m_VertexCount'].data == 3
assert mesh['m_BonesPerVertex'].data == 0 and not mesh['m_BindPose'].items()
assert precision['weightMaxError'] == 0.
encoded = encode(mesh)
assert encode(decode(mesh.schema, encoded)) == encoded
print('PASS Blender static snapshot -> real native Mesh encoding; no Armature, fake bones or scene save')
