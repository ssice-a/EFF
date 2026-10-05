"""Verify combined outputs against independent results and stage native loader inputs.

Only the process-lifetime cache is written in the game directory. Installing
the table and DLL is separate; original game inputs and Mod files stay intact.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('run', 'mod', 'checker', 'cache', 'table', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if any(p.exists() for p in (args.cache, args.table, args.report)):
        raise FileExistsError('Loading input candidate destination exists')
    verified = json.loads((args.run / 'cpp-current-renderer-input-verification.json').read_text())
    if not verified['verified'] or verified['nativeSubmission']:
        raise ValueError('Independent Renderer evidence not verified')
    texture = json.loads((args.run / 'cpp-texture-input-verification.json').read_text())
    mesh_report = json.loads((args.run.parent / 'restart-v5' / 'cpp-mesh-input-verification.json').read_text())
    if not texture['verified'] or not mesh_report['verified']:
        raise ValueError('Independent Mesh/Texture evidence not verified')
    snapshots = json.loads((args.run / 'current-input-graph' / 'snapshots.json').read_text())
    expected_sources = {}
    previous = args.run.parent / 'restart-v5'
    for source in previous.glob('input-*.vfs'):
        output = previous / 'mesh-rebuilt-inputs' / source.name
        if output.exists():
            expected_sources[digest(source)] = digest(output)
    for source in (args.run / 'material-texture-current-inputs').glob('*.vfs'):
        output = args.run / 'material-texture-rebuilt-inputs' / source.name
        expected_sources[digest(source)] = digest(output)
    rows, checked = [], []
    for row in snapshots['inputs']:
        source = args.run / 'current-input-graph' / row['file']
        output = args.run / 'prepared-loading-inputs' / source.name
        before_hash, after_hash = digest(source), digest(output)
        if before_hash != row['sha256']:
            raise ValueError('Current source snapshot changed')
        if row['file'].startswith('sources/'):
            expected_hash = expected_sources[before_hash]
        else:
            expected_hash = digest(args.run / 'current-consumers-rebuilt-v2' / source.name)
        if after_hash != expected_hash:
            raise ValueError('Combined output differs from independent verification: ' + source.name)
        checked.append(dict(file=source.name, beforeSha256=before_hash, afterSha256=after_hash,
                            changed=before_hash != after_hash))
        if before_hash == after_hash:
            continue
        rows.append((row, output, after_hash))
    response = subprocess.run([str(args.checker.resolve()), '--package-digest', str(args.mod.resolve())],
                              check=True, capture_output=True, text=True)
    package_hash = json.loads(response.stdout)['packageDigest']
    args.cache.mkdir(parents=True)
    table = ['\t'.join(('EFF_LOADING_INPUT_SUBSTITUTIONS', '1', 'LZY', package_hash))]
    for row, output, after_hash in rows:
        destination = args.cache / output.name
        shutil.copy2(output, destination)
        if digest(destination) != after_hash:
            raise ValueError('Staged loading file checksum differs')
        table.append('\t'.join((row['physicalPath'], str(row['offset']), str(row['bytes']), row['sha256'],
                                str(destination.resolve()), str(destination.stat().st_size), after_hash)))
    args.table.write_bytes(('\n'.join(table) + '\n').encode('utf8'))
    report = dict(format='EFF_PREPARED_LOADING_INPUTS', version=1, combinedBytesVerified=True,
                  inputs=len(checked), substitutions=len(rows), unchanged=len(checked) - len(rows),
                  packageDigest=package_hash, table=str(args.table.resolve()), cache=str(args.cache.resolve()),
                  nativeSubmission=False, dllDeployed=False, gameAcceptanceVerified=False,
                  dynamicRegistrationVerified=False, controlsBound=False, F10NativeRebuild=False,
                  files=checked)
    args.report.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps({k: v for k, v in report.items() if k != 'files'}))


if __name__ == '__main__':
    main()
