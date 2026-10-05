"""Independently verify CPU-retention-only changes after AnimeStudio decoding."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_deployed_skinning import load_files
from verify_cpp_mesh_input import verify_serialized
from nativepack.type_tree import encode, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'after', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(not args.output.exists(), 'readback verification already exists')
    before, after = load_files(args.before), load_files(args.after)
    flags = {'m_IsReadable', 'm_KeepVertices', 'm_KeepIndices'}
    report = dict(format='EFF_READBACK_RETENTION_INDEPENDENT_VERIFICATION', version=1,
                  verified=False, geometryFieldsUnchanged=True, identitiesAndReferencesUnchanged=True,
                  nativeAcceptanceVerified=False, tearingFixVerified=False, files=[])
    for cab, current in after.items():
        expected = before[cab]
        require(expected.order == current.order, 'readback changed object identity/order')
        edits = []
        for pid in expected.order:
            old, new = expected.objects[pid], current.objects[pid]
            require(old['classId'] == new['classId'] and old['typeIndex'] == new['typeIndex'], 'readback changed type')
            if old['payload'] == new['payload']:
                continue
            require(old['classId'] == 43, 'non-Mesh object changed')
            source, result = expected.tree(pid), current.tree(pid)
            changed = [name for name in source.data if encode(source[name]) != encode(result[name])]
            require(set(changed) <= flags and changed, 'geometry/reference change in readback probe')
            for name in flags:
                require(result[name].data == 1, 'readback flag not enabled')
                source.set(name, 1)
            expected.write_tree(pid, source)
            edits.append(dict(pathId=pid, mesh=result['m_Name'].data, changedFields=changed))
        verify_serialized(expected, current.original)
        report['files'].append(dict(cab=cab, objects=len(current.order), meshEdits=edits))
    report.update(verified=True, members=len(after), meshes=sum(len(f['meshEdits']) for f in report['files']))
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
