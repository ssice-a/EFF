"""Stage one native loading control from a freshly checked original disk range.

The raw-copy control changes only the path and offset. The second control
reencodes the container with unchanged members. Installation is separate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('snapshots', 'mod', 'checker', 'game', 'cache'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--input', required=True)
    parser.add_argument('--kind', choices=('raw-copy', 'reencoded-unchanged'), required=True)
    args = parser.parse_args()
    checker = args.checker.resolve()
    cache = args.cache.resolve()
    cache_root = args.game.resolve() / 'plugin' / 'resource-export-staging'
    if not cache.is_relative_to(cache_root) or cache == cache_root:
        raise ValueError('Control cache must be a new directory inside resource-export-staging')
    rows = json.loads(args.snapshots.read_text(encoding='utf8'))['inputs']
    selected = [row for row in rows if Path(row['file']).name == args.input]
    if len(selected) != 1:
        raise ValueError('Select exactly one current input range')
    row = selected[0]
    response = subprocess.run([str(checker), '--package-digest', str(args.mod.resolve())],
                              check=True, capture_output=True, text=True)
    package_hash = json.loads(response.stdout)['packageDigest']
    cache.mkdir(parents=True, exist_ok=False)
    original = cache / 'original.vfs'
    subprocess.run([str(checker), '--native-range', row['physicalPath'], str(row['offset']),
                    str(original)], check=True)
    if original.stat().st_size != row['bytes'] or digest(original) != row['sha256']:
        raise ValueError('Original disk range no longer matches the observed input')
    output = original
    if args.kind == 'reencoded-unchanged':
        output = cache / 'reencoded.vfs'
        subprocess.run([str(checker), '--endfield-rebuild', str(original), str(output)], check=True)
    output_hash = digest(output)
    table = cache / 'probe.tsv'
    header = '\t'.join(('EFF_LOADING_INPUT_SUBSTITUTIONS', '2', 'LZY', package_hash, args.kind))
    entry = '\t'.join((row['physicalPath'], str(row['offset']), str(row['bytes']), row['sha256'],
                       str(output), str(output.stat().st_size), output_hash))
    table.write_bytes((header + '\n' + entry + '\n').encode('utf8'))
    subprocess.run([str(checker), '--check-loading-table', str(args.mod.resolve()),
                    str(table), str(args.game.resolve())], check=True)
    report = dict(format='EFF_LOADING_INPUT_CONTROL', version=1, kind=args.kind,
                  selectedInput=args.input, source=row, output=str(output), outputSha256=output_hash,
                  packageDigest=package_hash, table=str(table), substitutions=1,
                  originalBytesPreserved=args.kind == 'raw-copy', decodedContentPreserved=True,
                  replacementEnabled=False, installed=False, gameAcceptanceVerified=False)
    (cache / 'probe.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
