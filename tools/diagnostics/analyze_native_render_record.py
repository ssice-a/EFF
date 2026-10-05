"""Report native Renderer records and device vertex-format adaptation evidence.

These observations precede the actual GPU draw. Missing observations are gaps,
and an unchanged source layout alone does not prove final GPU interpretation.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package
from nativepack.payload import read_payload
from nativepack.type_tree import require


def rows(log, tag):
    return [dict(re.findall(r'(\w+)=(\S+)', line)) for line in log.splitlines() if tag in line]


def stream_layout(channels, vertices):
    """Standard storage widths; packed-frame high flags do not add elements."""
    widths = {0: 4, 1: 2, 2: 1, 3: 1, 4: 2, 5: 2, 6: 1, 7: 1,
              8: 2, 9: 2, 10: 4, 11: 4}
    strides, masks = [0] * 4, [0] * 4
    for attr, (stream, offset, fmt, dimension) in enumerate(channels):
        dim = dimension & 15
        if not dim:
            continue
        require(stream < 4 and fmt in widths and dim <= 4, 'native vertex stream descriptor')
        strides[stream] = max(strides[stream], offset + dim * widths[fmt])
        masks[stream] |= 1 << attr
    start, result = 0, []
    for stream in range(4):
        result.append(dict(stream=stream, start=start, stride=strides[stream], channelMask=masks[stream]))
        start = (start + vertices * strides[stream] + 15) & ~15
    return result


def analyze(log, package):
    parts = {part['partId']: part for part in package['parts']}
    layouts = rows(log, '[DEBUG-skin-v15-LAYOUT]')
    channels = rows(log, '[DEBUG-skin-v15-CHANNEL]')
    streams = rows(log, '[DEBUG-skin-v15-STREAM]')
    records = rows(log, '[DEBUG-skin-v15-RECORD]')
    result = dict(format='EFF_NATIVE_RENDER_RECORD_RESULT', version=1,
                  readyBoundaries=rows(log, '[DEBUG-skin-v15-READY]'),
                  inputSubmissions=log.count('[ENTRY-SUBMIT]'),
                  actualGpuDrawObserved=False, originalVisualBugFixed=False,
                  gaps=[line for line in log.splitlines() if '[DEBUG-skin-v15-GAP]' in line],
                  layoutSamples=[], recordSamples=records, channelSamples=channels, streamSamples=streams)
    for layout in layouts:
        require(layout['part'] in parts, 'native layout author association missing')
        part = parts[layout['part']]
        field = next(f for f in part['nativeFields'] if f['name'] == 'm_VertexData')
        author = read_payload(package, field)
        count, channel_count = struct.unpack_from('<II', author)
        require(channel_count == 14 and len(author) >= 68, 'author vertex layout extent')
        author_channels = list(struct.iter_unpack('4B', author[8:64]))
        observed = [row for row in channels if row['mesh'] == layout['mesh'] and row['part'] == layout['part']]
        native_channels = [(0, 0, 0, 0)] * 14
        differences = []
        for row in observed:
            attr = int(row['attr'])
            require(0 <= attr < 14, 'native channel attribute range')
            actual = tuple(int(row[k]) for k in ('stream', 'offset', 'format', 'dimension'))
            native_channels[attr] = actual
            if actual != author_channels[attr]:
                differences.append(dict(attr=attr, author=list(author_channels[attr]), native=list(actual)))
        expected_streams = stream_layout(native_channels, int(layout['vertices']))
        observed_streams = [row for row in streams if row['mesh'] == layout['mesh'] and row['part'] == layout['part']]
        stream_differences = []
        for row in observed_streams:
            stream = int(row['stream'])
            require(0 <= stream < 4, 'native vertex stream index')
            expected = expected_streams[stream]
            if not expected['channelMask']:
                continue  # Native unused stream starts may remain zero.
            actual = {key: int(row[key]) for key in ('stream', 'start', 'stride', 'channelMask')}
            if actual != expected:
                stream_differences.append(dict(expected=expected, native=actual))
        result['layoutSamples'].append(dict(record=layout,
            nativeVertexCountMatchesAuthor=int(layout['vertices']) == count,
            deviceConversionObserved=layout['conversionObserved'] == '1',
            deviceConversionVertexCountMatchesAuthor=layout['conversionObserved'] == '1' and int(layout['conversionVertices']) == count,
            nativeChannelsDifferentFromAuthor=differences,
            nativeStreamLayoutDifferentFromChannels=stream_differences,
            missingActiveStreamRecords=[row['stream'] for row in expected_streams if row['channelMask'] and
                not any(int(s['stream']) == row['stream'] for s in observed_streams)],
            missingAuthorAttributes=[attr for attr, channel in enumerate(author_channels) if channel[3] & 15 and
                not any(int(row['attr']) == attr for row in observed)]))
    result.update(layoutMeshes=len({row['mesh'] for row in layouts}),
                  layoutParts=len({row['part'] for row in layouts}),
                  nativeRecords=len(records),
                  recordRenderers=len({row['renderer'] for row in records}),
                  recordParts=len({row['part'] for row in records}),
                  recordBaseMismatchSamples=sum(row['basesMatched'] != '1' for row in records),
                  ringOutOfRangeSamples=sum(row['ringInRange'] != '1' for row in records),
                  ringPointerMismatchSamples=sum(row['ringInRange'] == '1' and row['ringPointerMatched'] != '1' for row in records),
                  nativeStreamLayoutMismatchMeshes=sum(bool(row['nativeStreamLayoutDifferentFromChannels'])
                      for row in result['layoutSamples']),
                  unsampledRecordParts=sorted(set(parts) - {row['part'] for row in records}),
                  unobservedDeviceConversionMeshes=sum(row['conversionObserved'] != '1' for row in layouts))
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('session', 'mod', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'native record result already exists')
    report = analyze((args.session / 'entry.log').read_text(encoding='utf-8'), read_package(args.mod))
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if not isinstance(v, list)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
