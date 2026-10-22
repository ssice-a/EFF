"""Compare every decoded runtime part with its exact retained source arrays."""
import importlib.util
import json
import struct
import sys
from pathlib import Path

module_path = Path(__file__).resolve().parents[1] / "naraka_blender/format.py"
spec = importlib.util.spec_from_file_location("naraka_format", module_path)
fmt = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fmt)
root = Path(sys.argv[1]).resolve()
manifest = json.loads((root / 'naraka.json').read_text('utf-8-sig'))

def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]

def same(actual, expected, field):
    assert len(actual) == len(expected), (field, len(actual), len(expected))
    assert all(abs(a - f32(b)) < 1e-6 for a, b in zip(actual, expected)), field

verified = 0
for renderer in manifest['renderers']:
    source_file = root / (renderer['mesh'] + '.runtime.json')
    if not source_file.is_file(): continue
    source = json.loads(source_file.read_text('utf-8-sig'))
    payload = fmt.read_mesh(root / renderer['mesh'])
    assert payload['indices'] == source['m_Indices']
    for field, target, axes in (('position', 'vertices', 'xyz'), ('normal', 'normals', 'xyz'), ('tangent', 'tangents', 'xyzw')):
        same(payload[target], [v[field][axis] for v in source['m_VertexData'] for axis in axes], target)
    if source['m_UVData']:
        same(payload['uvs'][0], [v[axis] for v in source['m_UVData'] for axis in 'xy'], 'UV0')
    else:
        for channel in (0, 1):
            same(payload['uvs'][channel], [v['uv' + str(channel)][axis] for v in source['m_HairUVSetData'] for axis in 'xy'], 'hair UV' + str(channel))
    for exported, original in zip(payload['bindposes'], source['m_BindPoses']):
        same(exported, [original[f'e{row}{col}'] for col in range(4) for row in range(4)], 'bindpose column order')
    assert len(payload['skin']) == len(source['m_AnimSkinData'])
    for exported, original in zip(payload['skin'], source['m_AnimSkinData']):
        same(exported[:4], [original['boneWeight'][axis] for axis in 'xyzw'], 'skin weight')
        assert list(exported[4:]) == [original['boneIndex'][axis] for axis in 'xyzw']
    verified += 1
assert verified == 12, verified
print('RUNTIME PAYLOAD PASS', verified, 'parts: indices, position, normal, tangent, UV0/UV1, bindposes and skin match exact source data')
