"""Stage an object-record-order candidate for an existing validated loading graph.

This only writes a fresh cache and a candidate table. Installation and native
acceptance are separate. Already verified object contents remain unchanged.
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
    checker = args.checker.resolve()
    cache = args.cache.resolve()
    root = args.game.resolve() / 'plugin/resource-export-staging'
    if not cache.is_relative_to(root) or cache == root:
        raise ValueError('candidate cache must be inside resource-export-staging')
    if any(p.exists() for p in (cache, args.output, args.report)):
        raise FileExistsError('candidate destination exists')
    subprocess.run([str(checker), '--check-loading-table', str(args.mod.resolve()),
                    str(args.table.resolve()), str(args.game.resolve())], check=True)
    with args.table.open(encoding='utf8', newline='') as file:
        records = list(csv.reader(file, delimiter='\t'))
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
        result = subprocess.run([str(checker), '--object-order-probe', str(original), str(target)],
                                check=True, text=True, capture_output=True)
        fields = result.stdout.strip().split('\t')
        if fields[0] != 'OBJECT-ORDER-PROBE' or len(fields) != 4:
            raise ValueError('unexpected order probe result')
        output_hash = digest(target)
        if output_hash == row[3]:
            raise ValueError('candidate lost its replacement content')
        output_rows.append(row[:4] + [str(target), str(target.stat().st_size), output_hash])
        files.append(dict(file=original.name, beforeSha256=row[6], afterSha256=output_hash,
                          objects=int(fields[2]), changedMembers=int(fields[1]),
                          bytes=target.stat().st_size))
        print(f'ORDER-CANDIDATE {index}/{len(rows)} {original.name}', flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', encoding='utf8', newline='') as file:
        csv.writer(file, delimiter='\t', lineterminator='\n').writerows(output_rows)
    subprocess.run([str(checker), '--check-loading-table', str(args.mod.resolve()),
                    str(args.output.resolve()), str(args.game.resolve())], check=True)
    report = dict(format='EFF_ORDERED_LOADING_INPUT_CANDIDATE', version=1,
                  substitutions=len(files), packageDigest=header[3],
                  table=str(args.output.resolve()), tableSha256=digest(args.output),
                  cache=str(cache), changedMembers=sum(f['changedMembers'] for f in files),
                  allObjectPayloadBytesPreserved=True, allObjectIdentitiesAndReferencesPreserved=True,
                  sourceGraphContentPreviouslyOfflineVerified=True,
                  sourceGraphNativeAcceptanceVerified=False, objectOrderingCauseConfirmed=False,
                  installed=False, completeReplacementVerified=False, files=files)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
