"""Independently verify that a SerializedFile probe only reorders object records."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct


def object_rows(data):
    if data[8:12] != b'\x00\x00\x00\x16' or data[16] != 0:
        raise ValueError('requires v22 little-endian SerializedFile')
    metadata, size, data_offset = struct.unpack_from('>IQQ', data, 20)
    if size != len(data) or not 48 + metadata <= data_offset <= size:
        raise ValueError('invalid extended header')
    position = data.index(b'\0', 48) + 1 + 4
    if data[position] != 1:
        raise ValueError('requires current TypeTree metadata')
    position += 1
    types = struct.unpack_from('<I', data, position)[0]
    position += 4
    for _ in range(types):
        kind = struct.unpack_from('<i', data, position)[0]
        position += 7 + (16 if kind == 114 else 0) + 16
        nodes, strings = struct.unpack_from('<II', data, position)
        position += 8 + nodes * 32 + strings
        dependencies = struct.unpack_from('<I', data, position)[0]
        position += 4 + dependencies * 4
    count = struct.unpack_from('<I', data, position)[0]
    start = (position + 7) & ~3 if count else position + 4
    end = start + count * 24
    if end > 48 + metadata:
        raise ValueError('object records exceed metadata')
    rows = [struct.unpack_from('<qqIi', data, start + i * 24) for i in range(count)]
    if len({row[0] for row in rows}) != count:
        raise ValueError('duplicate PathID')
    for _, relative, length, kind in rows:
        if not 0 <= kind < types or relative < 0 or data_offset + relative + length > size:
            raise ValueError('invalid object record range/type')
    return start, end, rows


def verify(before, after):
    first, last, rows = object_rows(before)
    new_first, new_last, ordered = object_rows(after)
    if (first, last) != (new_first, new_last) or before[:first] != after[:first] or before[last:] != after[last:]:
        raise ValueError('changed bytes outside object records')
    if sorted(rows) != ordered:
        raise ValueError('output records are not the same rows in signed PathID order')
    return dict(format='EFF_OBJECT_ORDER_PROBE_VERIFICATION', version=1, verified=True,
                objects=len(rows), sourceWasSorted=rows == sorted(rows), outputIsSorted=True,
                completeRecordMultisetPreserved=True, allObjectPayloadBytesPreserved=True,
                allHeaderAndOtherMetadataBytesPreserved=True,
                onlyObjectTableRowOrderChanged=before != after,
                sourceSha256=hashlib.sha256(before).hexdigest().upper(),
                outputSha256=hashlib.sha256(after).hexdigest().upper(),
                nativeAcceptanceVerified=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'after', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError('object-order verification report exists')
    report = verify(args.before.read_bytes(), args.after.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
