"""Compare captured afterimage frames using paths and each Mesh's own bind poses.

Input is a renderdoc-cli draw-inputs export. This diagnoses a common frame
rotation; it does not establish visual acceptance of a new game build.
"""
import argparse
import json
from pathlib import Path
import struct
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload


def bindposes(package, part):
    field = next(f for f in part['nativeFields'] if f['name'] == 'm_BindPose')
    raw = read_payload(package, field)
    count = struct.unpack_from('<I', raw)[0]
    if count != len(part['bonePaths']) or len(raw) != 4 + count * 64:
        raise ValueError('Author bind-pose slot extent')
    return np.frombuffer(raw, dtype='<f4', offset=4).reshape(count, 4, 4).astype(float)


def palette(directory, event, count):
    path = directory / f'event{event}'
    state = json.loads((path / 'draw-inputs.json').read_text())
    descriptor = next(d for d in state['descriptors'] if d['set'] == 3 and d['binding'] == 0)
    raw = (path / descriptor['file']).read_bytes()
    header = np.frombuffer(raw[:16], dtype='<u4')
    row = int(header[1])
    if row % 3 or not (int(header[0]) & 32):
        raise ValueError('Draw does not use the inspected shader palette path')
    rows = np.frombuffer(raw[16:], dtype='<f4').reshape(-1, 4)
    matrices = rows[row:row + (count + 1) * 3].reshape(count + 1, 3, 4)
    result = np.zeros((count + 1, 4, 4))
    result[:, :3] = matrices
    result[:, 3, 3] = 1
    if not np.isfinite(result).all():
        raise ValueError('GPU palette is nonfinite')
    return result[1:], state, path


def audit(args):
    package = read_package(args.mod)
    parts = {p['partId']: p for p in package['parts']}
    body, control = parts[args.body_part], parts[args.control_part]
    b, state, path = palette(args.gpu_dir, args.body_event, len(body['bonePaths']))
    c, _, _ = palette(args.gpu_dir, args.control_event, len(control['bonePaths']))
    # A numerical slot is local to its Mesh. Remove the respective bind pose
    # before comparing the same bone path across the two captured renderers.
    bt = b @ np.linalg.inv(bindposes(package, body))
    ct = c @ np.linalg.inv(bindposes(package, control))
    slots = {path: slot for slot, path in enumerate(body['bonePaths'])}
    if len(slots) != len(body['bonePaths']):
        raise ValueError('Duplicate body bone paths')
    samples = []
    for slot, bone_path in enumerate(control['bonePaths']):
        if bone_path in slots:
            delta = ct[slot] @ np.linalg.inv(bt[slots[bone_path]])
            samples.append(dict(path=bone_path, bodySlot=slots[bone_path], controlSlot=slot,
                                controlFromBody=delta.tolist()))
    if len(samples) < 3:
        raise ValueError('Too few shared paths for frame comparison')
    deltas = np.array([s['controlFromBody'] for s in samples])
    median = np.median(deltas, axis=0)
    residual = np.max(np.abs(deltas - median), axis=(1, 2))
    angle = float(np.degrees(np.arccos(np.clip((np.trace(median[:3, :3]) - 1) / 2, -1, 1))))
    bindings = {v['slot']: v for v in state['vertexBuffers']}
    pos_attr = next(a for a in state['attributes'] if a['location'] == 0)
    pos_binding = bindings[pos_attr['binding']]
    count = body['vertices']
    positions = np.ndarray((count, 3), dtype='<f4',
                           buffer=(path / pos_binding['file']).read_bytes(), offset=pos_attr['offset'],
                           strides=(pos_binding['stride'], 4)).astype(float)
    skin_binding = bindings[2]
    if skin_binding['stride'] != 12:
        raise ValueError('Unexpected skin stream layout')
    skin = np.frombuffer((path / skin_binding['file']).read_bytes(),
                        dtype=[('weights', '<u2', (4,)), ('bones', 'u1', (4,))], count=count)
    if np.any(skin['bones'] >= len(b)) or not np.all(skin['weights'].astype(np.uint32).sum(axis=1) == 65535):
        raise ValueError('GPU weights/indices do not fit this Mesh')
    homogeneous = np.column_stack((positions, np.ones(count)))
    skinned = sum(np.einsum('vij,vj->vi', b[skin['bones'][:,i], :3], homogeneous) *
                  (skin['weights'][:,i].astype(float) / 65535)[:,None] for i in range(4))
    corrected = np.column_stack((skinned, np.ones(count))) @ median[:3].T
    return dict(format='EFF_AFTERIMAGE_FRAME_SPACE_AUDIT', version=1,
                bodyEvent=args.body_event, controlEvent=args.control_event,
                sharedBonePaths=len(samples), medianControlFromBody=median.tolist(),
                medianRotationDegrees=angle,
                matchingPathsWithin001=int((residual < .01).sum()),
                maximumPathResidual=float(residual.max()),
                bodyLocalBoundsSize=np.ptp(skinned,axis=0).tolist(),
                bodyBoundsSizeWithInferredFrame=np.ptp(corrected,axis=0).tolist(),
                samples=samples, visualVerification=False, newBuildVerification=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mod', type=Path, required=True)
    parser.add_argument('--gpu-dir', type=Path, required=True)
    parser.add_argument('--body-event', type=int, required=True)
    parser.add_argument('--control-event', type=int, required=True)
    parser.add_argument('--body-part', default='PartS_actor_lizhiyan_body_01_lod0_0')
    parser.add_argument('--control-part', default='PartS_actor_lizhiyan_cloth_04_lod0_4.bar')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n', encoding='utf8')
    print(json.dumps({k:v for k,v in report.items() if k != 'samples'}))
