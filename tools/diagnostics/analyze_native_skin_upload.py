"""Audit native matrix packing snapshots and skin author triangles offline.

The snapshots are CPU data at UnityPlayer's matrix packing boundary. They do
not prove that a later GPU draw binds this buffer or the recorded row bases.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct
import sys

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.type_tree import require


def author_geometry(package, part):
    fields = {f['name']: read_payload(package, f) for f in part['nativeFields']}
    raw = fields['m_VertexData']
    require(len(raw) >= 68, 'author vertex header truncated')
    count, channels = struct.unpack_from('<II', raw)
    require(count == part['vertices'] and channels == 14, 'author vertex count/channels')
    strides = [0, 0, 0]
    for stream, offset, fmt, dimension in struct.iter_unpack('4B', raw[8:64]):
        dim = dimension & 15
        if not dim:
            continue
        require(stream < 3 and fmt in (0, 4, 6), 'author vertex format')
        strides[stream] = max(strides[stream], offset + dim * {0: 4, 4: 2, 6: 1}[fmt])
    starts = [0]
    for stride in strides[:-1]:
        starts.append((starts[-1] + count * stride + 15) & ~15)
    size = struct.unpack_from('<I', raw, 64)[0]
    require(size <= len(raw) - 68 and starts[2] + count * strides[2] <= size and
            strides[0] in (16,40) and strides[2] == 12, 'author position/skin extent')
    data = raw[68:68 + size]
    positions = np.ndarray((count, 3), dtype='<f4', buffer=data, strides=(strides[0], 4)).astype(np.float64)
    skin = np.frombuffer(data, dtype=[('weights', '<u2', (4,)), ('bones', 'u1', (4,))], offset=starts[2], count=count)
    weights = skin['weights'].astype(np.float64) / 65535
    bones = skin['bones'].astype(np.int64)
    require(np.all(skin['weights'].astype(np.uint32).sum(axis=1) == 65535), 'author weight sums')
    require(np.all(bones < len(part['bonePaths'])), 'author bone slot range')
    index_size = 4 if struct.unpack('<I', fields['m_IndexFormat'])[0] else 2
    indices = fields['m_IndexBuffer']
    byte_count = struct.unpack_from('<I', indices)[0]
    require(byte_count == part['indices'] * index_size and len(indices) >= 4 + byte_count, 'author triangle extent')
    triangles = np.frombuffer(indices, dtype='<u4' if index_size == 4 else '<u2', offset=4, count=part['indices']).reshape(-1, 3)
    require(np.all(triangles < count), 'author triangle index range')
    return positions, weights, bones, triangles


def geometry_metrics(geometry, matrices):
    positions, weights, bones, triangles = geometry
    homogeneous = np.column_stack((positions, np.ones(len(positions))))
    skinned = np.zeros_like(positions)
    for influence in range(4):
        transformed = np.einsum('vij,vj->vi', matrices[bones[:, influence]], homogeneous)
        skinned += transformed * weights[:, influence, None]
    require(np.all(np.isfinite(skinned)), 'native skinned vertices nonfinite')
    edges = np.concatenate((triangles[:, (0, 1)], triangles[:, (1, 2)], triangles[:, (2, 0)]))
    before = np.linalg.norm(positions[edges[:, 0]] - positions[edges[:, 1]], axis=1)
    after = np.linalg.norm(skinned[edges[:, 0]] - skinned[edges[:, 1]], axis=1)
    original_extent = np.ptp(positions, axis=0)
    native_extent = np.ptp(skinned, axis=0)
    before_max, after_max = float(before.max()), float(after.max())
    native_diagonal, original_diagonal = float(np.linalg.norm(native_extent)), float(np.linalg.norm(original_extent))
    return dict(originalBoundsSize=original_extent.tolist(), nativeSkinnedBoundsSize=native_extent.tolist(),
                originalBoundsDiagonal=original_diagonal, nativeSkinnedBoundsDiagonal=native_diagonal,
                originalLongestTriangleEdge=before_max, nativeSkinnedLongestTriangleEdge=after_max,
                longestEdgeRatio=after_max / max(before_max, 1e-9),
                longestNativeTriangleVertices=edges[int(after.argmax())].tolist(),
                nativeSkinnedEdge99Percentile=float(np.quantile(after, .99)),
                possibleSevereExplosion=native_diagonal > max(.1, original_diagonal * 20) and
                    after_max > max(.1, before_max * 20))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('session', 'mod', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'native upload result exists')
    package = read_package(args.mod)
    parts = {p['partId']: p for p in package['parts']}
    log = (args.session / 'entry.log').read_text(encoding='utf8')
    rows = [dict(re.findall(r'(\w+)=(\S+)', line)) for line in log.splitlines() if '[DEBUG-skin-v14-UPLOAD]' in line]
    report = dict(format='EFF_NATIVE_SKIN_UPLOAD_RESULT', version=1, snapshots=len(rows),
                  ready='[DEBUG-skin-v14-READY]' in log, inputSubmissions=log.count('[ENTRY-SUBMIT]'),
                  actualGpuDrawObserved=False, originalVisualBugFixed=False,
                  gaps=[line for line in log.splitlines() if '[DEBUG-skin-v14-GAP]' in line], samples=[])
    geometry_cache = {}
    for row in rows:
        count = int(row['count'])
        require(0 < count <= 256 and row['part'] in parts and re.fullmatch(r'skin-upload-\d+', row['file']),
                'native upload record identity/count')
        stem = args.session / row['file']
        input_bytes = Path(str(stem) + '-input4x4.bin').read_bytes()
        packed_bytes = Path(str(stem) + '-packed3x4.bin').read_bytes()
        root_bytes = Path(str(stem) + '-root3x4.bin').read_bytes()
        require(len(input_bytes) == count * 64 and len(packed_bytes) == count * 48 and len(root_bytes) == 48,
                'native matrix snapshot extent')
        raw = np.frombuffer(input_bytes, dtype='<f4').reshape(count, 4, 4).transpose(0, 2, 1)
        packed = np.frombuffer(packed_bytes, dtype='<f4').reshape(count, 3, 4)
        root = np.frombuffer(root_bytes, dtype='<f4').reshape(3, 4)
        mismatches = int(np.count_nonzero(raw[:, :3, :].copy().view('<u4') != packed.view('<u4')))
        require(mismatches == int(row['packedBitMismatches']), 'logged packing mismatch differs from binary snapshot')
        sample = dict(record=row, column4x4ToRow3x4BitMismatches=mismatches,
                      inputNonFinite=int(np.count_nonzero(~np.isfinite(raw))),
                      packedNonFinite=int(np.count_nonzero(~np.isfinite(packed))),
                      rootNonFinite=int(np.count_nonzero(~np.isfinite(root))),
                      zeroPackedMatrices=int(np.count_nonzero(np.all(packed == 0, axis=(1, 2)))),
                      loggedCountMatchesAuthor=count == len(parts[row['part']]['bonePaths']))
        if sample['loggedCountMatchesAuthor'] and not sample['inputNonFinite']:
            if row['part'] not in geometry_cache:
                geometry_cache[row['part']] = author_geometry(package, parts[row['part']])
            sample.update(geometry_metrics(geometry_cache[row['part']], raw[:, :3, :].astype(np.float64)))
        report['samples'].append(sample)
    report.update(renderers=len({r['renderer'] for r in rows}),
                  parts=len({r['part'] for r in rows}),
                  packingMismatchSamples=sum(s['column4x4ToRow3x4BitMismatches'] > 0 for s in report['samples']),
                  nonfiniteSamples=sum(s['inputNonFinite'] > 0 or s['packedNonFinite'] > 0 for s in report['samples']),
                  possibleSevereExplosionSamples=sum(s.get('possibleSevereExplosion', False) for s in report['samples']))
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k not in ('samples', 'gaps')}))


if __name__ == '__main__':
    main()
