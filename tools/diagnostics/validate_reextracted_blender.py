"""Background Blender proof using a saved project and an isolated source/output.

Never saves the blend or modifies the real source package/game Mod.
Arguments after --: repository refreshed-source output-directory report-json.
"""
import hashlib
import importlib
import json
import os
from pathlib import Path
import sys
import time

import bpy

repository, source, destination, report = map(Path, sys.argv[sys.argv.index('--') + 1:])
sys.path.insert(0, str(repository / 'tools'))
package = importlib.import_module('Blender')
package.register()
addon = package.eiem_blender_addon
exporter = addon.native_export
file = Path(bpy.data.filepath)
before = hashlib.sha256(file.read_bytes()).hexdigest()
if bpy.context.mode != 'OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')
meshes = [obj for obj in bpy.data.objects if obj.type == 'MESH' and
          obj.data.get('eiem_target_path') and 'zhuangfy' in str(obj.data.get('eiem_target_path'))]
objects = [obj for obj in meshes if 'body_01_lod0' in str(obj.data.get('eiem_target_asset'))]
if not objects:
    objects = meshes[:1]
assert objects, 'Saved project has no bound ZFY Mesh'
objects = objects[:1]
owners = {str(obj.get('eiem_author_package', '')) for obj in objects}
# Redirect only in this disposable process, simulating updated files at the
# original package location while keeping every real author file untouched.
for block in list(bpy.data.objects) + list(bpy.data.materials) + list(bpy.data.images):
    if block.get('eiem_author_package') in owners:
        block['eiem_author_package'] = str(source.resolve())
os.environ['EFF_RELOAD_GAME'] = r'D:\Hypergryph Launcher\games\Endfield Game'
manifest = json.loads((source / 'source/manifest.json').read_text('utf-8-sig'))
os.environ.pop('EFF_SOURCE_INDEX', None)
started = time.perf_counter()
result = exporter.export_native_package(addon, destination, objects,
    source_baseline=str(source), resource_scope='ALL', include_switches=False, lod_levels=[])
assert hashlib.sha256(file.read_bytes()).hexdigest() == before, 'Original project file changed'
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(dict(project=str(file), projectSha256=before, projectUnchanged=True,
    source=str(source), currentIndex=manifest['sourceIndex'], meshes=[o.name for o in objects],
    elapsed=time.perf_counter()-started, result=result), ensure_ascii=False, indent=2), encoding='utf-8')
print('REEXTRACTED-OLD-PROJECT-PASS', report)
