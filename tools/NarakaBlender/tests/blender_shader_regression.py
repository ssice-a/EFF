"""Blender 5 regression: independent facing convention and real material dispatch."""
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from naraka_blender.importer import BASIS, convert, local_matrix, bind_matrix
from naraka_blender.materials import MaterialBuilder

package = Path(sys.argv[sys.argv.index('--') + 1])
manifest = json.loads(package.read_text(encoding='utf-8-sig'))
builder = MaterialBuilder(package.parent, manifest, [], profile='REFERENCE')
failures = []
if tuple(convert((0, 0, 1))) != (0, -1, 0):
    failures.append('Unity forward +Z must face Blender front -Y')
if tuple(convert((0, 1, 0))) != (0, 0, 1):
    failures.append('Unity up +Y must remain Blender up +Z')
for source in manifest['materials'].values():
    if 'clothadvanced_' not in source['shader']:
        continue
    mat = builder.build(source)
    status = json.loads(mat['naraka_conversion_status_json'])
    if '_MFX_EMISSIVE_PERIODIC_2' in source['keywords'].split() and mat.get('naraka_dynamic_material') != 'periodic2':
        failures.append(source['name'] + ': packed RG/BA emission dispatched as RGB')
    for prop in ('_AdvIDTex', '_TillingChannelMask', '_MainTexUV2', '_BumpMapUV2', '_SpecGlossMapUV2'):
        if source['textures'].get(prop, {}).get('texture') and prop in status['retainedTextureBindings']:
            failures.append(source['name'] + ': unused ' + prop)
assert not failures, '\n'.join(failures)
print('PASS shader dispatch, used masks, UV2 and independent front convention')
