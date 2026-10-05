"""Stage a single-variable UV2 storage experiment on the complete input graph."""
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
    for name in ('table', 'mod', 'game', 'checker', 'geometry-report', 'cache', 'output', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    checker, cache = args.checker.resolve(), args.cache.resolve()
    root = args.game.resolve() / 'plugin/resource-export-staging'
    if not cache.is_relative_to(root) or cache == root:
        raise ValueError('candidate cache must be inside resource-export-staging')
    if any(p.exists() for p in (cache, args.output, args.report)):
        raise FileExistsError('candidate destination exists')
    geometry = json.loads(args.geometry_report.read_text(encoding='utf8'))
    if digest(args.table) != geometry['tableSha256']:
        raise ValueError('expected geometry-readback table changed')
    selected = {f['file'] for f in geometry['files'] if f['meshes']}
    subprocess.run([str(checker), '--check-loading-table', str(args.mod.resolve()),
                    str(args.table.resolve()), str(args.game.resolve())], check=True)
    with args.table.open(encoding='utf8', newline='') as stream:
        records = list(csv.reader(stream, delimiter='\t'))
    header, rows = records[0], records[1:]
    if len(header) != 4 or header[:2] != ['EFF_LOADING_INPUT_SUBSTITUTIONS', '1']:
        raise ValueError('requires production-version candidate table')
    cache.mkdir()
    output_rows, files = [header], []
    for row in rows:
        original = Path(row[4])
        if original.name not in selected:
            output_rows.append(row)
            continue
        if digest(original) != row[6]:
            raise ValueError('prepared input changed: ' + original.name)
        target = cache / original.name
        result = subprocess.run([str(checker), '--packed-uv-probe', str(args.mod.resolve()),
                                 str(original), str(target)], check=True, text=True, capture_output=True)
        fields = result.stdout.strip().split('\t')
        if len(fields) != 4 or fields[0] != 'PACKED-UV-PROBE':
            raise ValueError('unexpected packed-UV probe result: ' + result.stdout)
        changed, meshes = int(fields[1]), int(fields[2])
        output_hash = digest(target)
        if not changed and output_hash != row[6]:
            raise ValueError('no-match input changed')
        if output_hash == row[3]:
            raise ValueError('candidate lost its replacement content')
        output_rows.append(row[:4] + [str(target), str(target.stat().st_size), output_hash] if changed else row)
        files.append(dict(file=original.name, beforeSha256=row[6], afterSha256=output_hash,
                          changedMembers=changed, meshes=meshes, bytes=target.stat().st_size,
                          original=str(original), output=str(target)))
        print(f'UV2-CANDIDATE {original.name} meshes={meshes}', flush=True)
    if len(files) != len(selected) or not any(f['meshes'] for f in files):
        raise ValueError('geometry probe selection missing or no flagged UV2 found')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', encoding='utf8', newline='') as stream:
        csv.writer(stream, delimiter='\t', lineterminator='\n').writerows(output_rows)
    subprocess.run([str(checker), '--check-loading-table', str(args.mod.resolve()),
                    str(args.output.resolve()), str(args.game.resolve())], check=True)
    report = dict(format='EFF_PACKED_UV2_CANDIDATE', version=1,
                  substitutions=len(rows), packageDigest=header[3], meshes=sum(f['meshes'] for f in files),
                  changedInputs=sum(bool(f['changedMembers']) for f in files),
                  table=str(args.output.resolve()), tableSha256=digest(args.output), cache=str(cache),
                  inputTableSha256=digest(args.table),
                  experiment='flagged UV2 Float32 to native SNorm8, maximum half-step UV2 quantization',
                  maxAllowedUv2DecodedError=1 / 254 + 1e-7,
                  productionWriterUnchanged=True, independentlyVerified=False, installed=False,
                  tearingFixVerified=False, completeReplacementVerified=False, files=files)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
