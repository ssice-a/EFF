"""Verify independently decoded inputs differ only in UV2 storage representation."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np

from audit_deployed_skinning import load_files
from verify_cpp_mesh_input import verify_serialized
from nativepack.type_tree import encode, require


def layout(vertex):
    channels = [c.plain() for c in vertex['m_Channels'].items()]
    count, data = vertex['m_VertexCount'].data, vertex['m_DataSize'].data
    widths = (4, 2, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4)
    strides = [0] * 3
    for c in channels:
        dim = c['dimension'] & 15
        if dim:
            require(c['stream'] < 3 and c['format'] < len(widths), 'UV verification format/stream')
            strides[c['stream']] = max(strides[c['stream']], c['offset'] + dim * widths[c['format']])
    starts = [0]
    for stride in strides[:-1]:
        starts.append((starts[-1] + count * stride + 15) & ~15)
    require(len(data) == (starts[-1] + count * strides[-1] + 15) & ~15, 'UV verification stream data size')
    return channels, count, data, strides, starts


def channel_bytes(descriptor, count, data, strides, starts):
    widths = (4, 2, 1, 1, 2, 2, 1, 1, 2, 2, 4, 4)
    width = (descriptor['dimension'] & 15) * widths[descriptor['format']]
    stream = descriptor['stream']
    return np.ndarray((count, width), dtype='u1', buffer=data,
                      offset=starts[stream] + descriptor['offset'], strides=(strides[stream], 1)).copy()


def verify_vertex(old, new):
    before, count, source, strides, starts = layout(old)
    after, new_count, result, new_strides, new_starts = layout(new)
    require(count == new_count and len(before) == len(after) == 14, 'vertex counts/channels changed')
    for name in old.data:
        if name not in ('m_Channels', 'm_DataSize'):
            require(encode(old[name]) == encode(new[name]), 'non-channel vertex field changed')
    uv = before[6]
    require(uv['dimension'] == 0x24 and uv['format'] == 0 and uv['stream'] == 1,
            'UV probe changed Mesh without flagged Float32 UV2')
    require(after[6] == dict(uv, format=3), 'UV2 descriptor changed beyond format')
    errors = []
    for i, (left, right) in enumerate(zip(before, after)):
        if i == 6:
            expected = channel_bytes(left, count, source, strides, starts).copy().view('<f4').reshape(count, 4)
            signed = channel_bytes(right, count, result, new_strides, new_starts).view('i1').astype(np.float64)
            actual = signed / 127
            error = float(np.max(np.abs(expected - actual)))
            require(np.all(np.isfinite(expected)) and np.all(expected >= -1) and np.all(expected <= 1)
                    and error <= 1 / 254 + 1e-7, 'UV2 quantization exceeds half SNorm8 step')
            require(np.array_equal(signed, np.copysign(np.floor(np.abs(expected.astype(np.float64) * 127) + .5), expected)),
                    'UV2 not quantized to nearest signed value')
            require(np.all(actual >= -1) and np.all(actual <= 1), 'UV2 signed value range')
            errors.append(error)
            continue
        offset = left['offset'] - 12 if left['stream'] == 1 and left['dimension'] & 15 and left['offset'] >= uv['offset'] + 16 else left['offset']
        require(right == dict(left, offset=offset), 'other channel descriptor changed')
        if left['dimension'] & 15:
            require(np.array_equal(channel_bytes(left, count, source, strides, starts),
                                   channel_bytes(right, count, result, new_strides, new_starts)),
                    f'other channel bytes changed: {i}')
    require(source[:starts[1]] == result[:new_starts[1]] and
            source[starts[2]:] == result[new_starts[2]:], 'position/normal/skin streams changed')
    for data, offsets, steps in ((source, starts, strides), (result, new_starts, new_strides)):
        for stream in range(3):
            end = offsets[stream] + count * steps[stream]
            following = offsets[stream + 1] if stream < 2 else len(data)
            require(not any(data[end:following]), 'nonzero stream alignment bytes')
    return dict(vertices=count, maxUv2DecodedError=max(errors),
                quantizedComponents=int(np.count_nonzero(np.abs(expected - actual) > 1e-7)),
                beforeUv2=uv, afterUv2=after[6],
                beforeStrides=strides, afterStrides=new_strides,
                allOtherChannelBytesEqual=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'after', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'packed-UV verification already exists')
    before, after = load_files(args.before), load_files(args.after)
    require(set(before) == set(after), 'Mesh member identities changed')
    report = dict(format='EFF_PACKED_UV2_INDEPENDENT_VERIFICATION', version=1, verified=False,
                  identitiesAndReferencesUnchanged=True, allNonVertexFieldsUnchanged=True,
                  allOtherVertexChannelsByteEqual=True, nativeAcceptanceVerified=False,
                  tearingFixVerified=False, completeReplacementVerified=False, files=[])
    for cab, current in after.items():
        expected = before[cab]
        require(expected.order == current.order, 'UV probe changed object identity/order')
        edits = []
        for pid in expected.order:
            old, new = expected.objects[pid], current.objects[pid]
            require(old['classId'] == new['classId'] and old['typeIndex'] == new['typeIndex'], 'UV probe changed type')
            if old['payload'] == new['payload']:
                continue
            require(old['classId'] == 43, 'non-Mesh object changed')
            source, result = expected.tree(pid), current.tree(pid)
            for field in source.data:
                if field != 'm_VertexData':
                    require(encode(source[field]) == encode(result[field]), 'non-vertex Mesh field changed: ' + field)
            values = verify_vertex(source['m_VertexData'], result['m_VertexData'])
            expected.write_tree(pid, result)
            edits.append(dict(pathId=pid, mesh=result['m_Name'].data, **values))
        verify_serialized(expected, current.original)
        report['files'].append(dict(cab=cab, objects=len(current.order), meshEdits=edits))
    report.update(verified=True, members=len(after), meshes=sum(len(f['meshEdits']) for f in report['files']))
    require(report['meshes'] > 0, 'UV verification contained no changes')
    report['maxUv2DecodedError'] = max(edit['maxUv2DecodedError'] for file in report['files'] for edit in file['meshEdits'])
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
