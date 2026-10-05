"""Take checked current-input snapshots for an automatically located candidate graph."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('selection', 'ranges', 'checker', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError('Current input snapshots exist')
    selection = json.loads(args.selection.read_text(encoding='utf8'))
    ranges = json.loads(args.ranges.read_text(encoding='utf8'))
    if not ranges['indexFingerprintVerified'] or selection['vfsFingerprint'] != ranges['streamingFingerprint']:
        raise ValueError('Current locator fingerprint not verified')
    sources = set(selection['sourceInputs'])
    args.output.mkdir(parents=True)
    for kind in ('sources', 'consumers'):
        (args.output / kind).mkdir()
    report = dict(format='EFF_CURRENT_INPUT_SNAPSHOTS', version=1, nativeSubmission=False, inputs=[])
    for i, row in enumerate(ranges['ranges'], 1):
        kind = 'sources' if row['bundle'] in sources else 'consumers'
        path = args.output / kind / f'input-{i:03d}.vfs'
        result = subprocess.run([str(args.checker.resolve()), '--native-range', row['physicalPath'],
                                 str(row['offset']), str(path.resolve())], capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(row['bundle'] + ': ' + result.stdout + result.stderr)
        data = path.read_bytes()
        if len(data) != row['bytes']:
            raise ValueError('Header/catalog range differs: ' + row['bundle'])
        report['inputs'].append(dict(row, file=path.relative_to(args.output).as_posix(),
                                      sha256=hashlib.sha256(data).hexdigest().upper()))
    (args.output / 'snapshots.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(f"Checked current inputs={len(report['inputs'])}; sources={len(sources)}")


if __name__ == '__main__':
    main()
