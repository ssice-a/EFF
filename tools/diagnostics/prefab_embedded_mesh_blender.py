"""Factory-startup Blender regression for a real Prefab-owned Mesh export."""
import argparse
import importlib.util
import json
from pathlib import Path
import sys

import bpy

parser = argparse.ArgumentParser()
parser.add_argument('--source', required=True, type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--writer', type=Path)
parser.add_argument('--addon', type=Path)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
root = args.addon.resolve() if args.addon else Path(__file__).resolve().parents[1] / 'Blender'
spec = importlib.util.spec_from_file_location('EFFEmbeddedMeshRegression', root / '__init__.py',
                                            submodule_search_locations=[str(root)])
package = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = package
spec.loader.exec_module(package)
package.register()
addon = package.eiem_blender_addon
if args.writer:
    original = addon.shared_resources.native_tool_file
    def tool(name):
        return args.writer.resolve() if name == 'eff_author_assets.dll' else original(name)
    addon.shared_resources.native_tool_file = tool
addon.import_package(str(args.source.resolve()), include_physics=False)
mesh = bpy.data.objects.get('MeshS_actor_zhuangfy_body_04_lod0_0')
assert mesh is not None and mesh.type == 'MESH', 'reported Mesh was not imported'
rigs = [modifier.object for modifier in mesh.modifiers
        if modifier.type == 'ARMATURE' and modifier.object is not None]
stats = addon.export_package(args.output.resolve(), mesh_objects=[mesh], armatures=rigs,
    physics_objects=[], lod_levels={0}, include_switches=False,
    source_baseline=str(args.source.resolve()), effect_roots=[])
assert (args.output / 'mod.ini').is_file(), 'Format 3 manifest missing'
print('PASS real Prefab embedded Mesh Blender import/selected Format 3 export:', json.dumps(stats, default=str))
