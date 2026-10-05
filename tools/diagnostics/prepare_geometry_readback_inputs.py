"""Stage temporary CPU retention flags for native geometry readback.

Author package and production prepared inputs are never modified. The resulting
candidate keeps the complete replacement graph; only matched Mesh CPU retention
flags differ. Native acceptance and visual verification remain separate.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('table', 'mod', 'game', 'checker', 'cache', 'output', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    checker, cache = args.checker.resolve(), args.cache.resolve()
    root = args.game.resolve() / 'plugin/resource-export-staging'
    if not cache.is_relative_to(root) or cache == root:
        raise ValueError('candidate cache must be inside resource-export-staging')
    if any(p.exists() for p in (cache, args.output, args.report)):
        raise FileExistsError('candidate destination exists')
    subprocess.run([str(checker), '--check-loading-table', str(args.mod.resolve()),
                    str(args.table.resolve()), str(args.game.resolve())], check=True)
    with args.table.open(encoding='utf8', newline='') as stream:
        records = list(csv.reader(stream, delimiter='\t'))
    header, rows = records[0], records[1:]
    if len(header) != 4 or header[:2] != ['EFF_LOADING_INPUT_SUBSTITUTIONS', '1']:
        raise ValueError('requires production-version candidate table')
    cache.mkdir()
    output_rows, files = [header], []
    for index, row in enumerate(rows, 1):
        original = Path(row[4])
        if digest(original) != row[6]:
            raise ValueError('prepared input changed: ' + original.name)
        target = cache / original.name
        result = subprocess.run([str(checker), '--geometry-readback-probe', str(args.mod.resolve()),
                                 str(original), str(target)], check=True, text=True, capture_output=True)
        fields = result.stdout.strip().split('\t')
        if len(fields) != 4 or fields[0] != 'GEOMETRY-READBACK-PROBE':
            raise ValueError('unexpected geometry readback probe result: ' + result.stdout)
        changed, meshes = int(fields[1]), int(fields[2])
        if not changed and digest(target) != row[6]:
            raise ValueError('no-match input changed')
        output_hash = digest(target)
        if output_hash == row[3]:
            raise ValueError('candidate lost its replacement content')
        # Keep all unchanged consumers, materials, textures and unrelated inputs
        # on their already validated production copies.
        output_rows.append(row[:4] + [str(target), str(target.stat().st_size), output_hash] if changed else row)
        files.append(dict(file=original.name, beforeSha256=row[6], afterSha256=output_hash,
                          changedMembers=changed, meshes=meshes, bytes=target.stat().st_size,
                          original=str(original), output=str(target)))
        print(f'READBACK-CANDIDATE {index}/{len(rows)} {original.name} meshes={meshes}', flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', encoding='utf8', newline='') as stream:
        csv.writer(stream, delimiter='\t', lineterminator='\n').writerows(output_rows)
    subprocess.run([str(checker), '--check-loading-table', str(args.mod.resolve()),
                    str(args.output.resolve()), str(args.game.resolve())], check=True)
    report = dict(format='EFF_GEOMETRY_READBACK_CANDIDATE', version=1,
                  substitutions=len(files), packageDigest=header[3], meshes=sum(f['meshes'] for f in files),
                  changedInputs=sum(bool(f['changedMembers']) for f in files),
                  table=str(args.output.resolve()), tableSha256=digest(args.output), cache=str(cache),
                  temporaryFields=['m_IsReadable', 'm_KeepVertices', 'm_KeepIndices'],
                  geometryFieldsIntendedUnchanged=True, independentlyVerified=False, installed=False,
                  tearingFixVerified=False, completeReplacementVerified=False, files=files)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
