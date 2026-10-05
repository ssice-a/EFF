"""Freeze and reproduce the accepted loading graph without publishing a generation.

Uses the DLL-side serialized builders and shape provenance preparation. The
manifest distinguishes byte reproducibility from cache isolation and migration.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from nativepack.ini_reader import read_package


def sha(data):
    return hashlib.sha256(data).hexdigest().upper()


def checked_bytes(path, expected=None, size=None):
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError('file changed while freezing: ' + str(path))
    if (expected and sha(data) != expected) or (size is not None and len(data) != size):
        raise ValueError('frozen content differs: ' + str(path))
    return data


def save(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('xb') as file:
        file.write(data)


def run(command, log):
    start = time.perf_counter()
    result = subprocess.run([str(arg) for arg in command], capture_output=True, text=True, encoding='utf8', errors='replace')
    save(log, (result.stdout + result.stderr).encode('utf8'))
    if result.returncode:
        raise RuntimeError(f'{command[0]} exited {result.returncode}: {result.stdout}{result.stderr}')
    print(result.stdout.strip(), flush=True)
    return dict(elapsedMs=(time.perf_counter() - start) * 1000, stdout=result.stdout.strip())


def prepare(args, report):
    mod, graph, output = args.mod.resolve(), args.graph.resolve(), args.output.resolve()
    package = read_package(mod)
    author = {name: checked_bytes(mod / name) for name in sorted(package['_files'])}
    before_author = {name: sha(data) for name, data in author.items()}
    # This second parse checks that the declarations did not change during read.
    if set(read_package(mod)['_files']) != set(author):
        raise ValueError('author resource set changed during freeze')
    frozen_mod = output / 'author'
    for name, data in author.items():
        save(frozen_mod / name, data)
    read_package(frozen_mod)
    if before_author != {name: sha(checked_bytes(mod / name)) for name in author}:
        raise ValueError('author files changed during freeze')
    report['authorFiles'] = [dict(file=name, sha256=sha(data), bytes=len(data)) for name, data in author.items()]
    snapshots_bytes = checked_bytes(graph / 'snapshots.json')
    snapshots = json.loads(snapshots_bytes)
    if snapshots.get('format') != 'EFF_CURRENT_INPUT_SNAPSHOTS':
        raise ValueError('unverified snapshot manifest format')
    save(output / 'pristine' / 'snapshots.json', snapshots_bytes)
    originals = {}
    physical_stamps = {}
    for row in snapshots['inputs']:
        relative = Path(row['file'])
        if relative.is_absolute() or '..' in relative.parts or relative.parts[0] not in ('sources', 'consumers'):
            raise ValueError('snapshot path escapes graph')
        if relative.name in originals:
            raise ValueError('duplicate snapshot input filename')
        data = checked_bytes(graph / relative, row['sha256'], row['bytes'])
        physical = Path(row['physicalPath'])
        stamp = physical.stat()
        before = (stamp.st_size, stamp.st_mtime_ns)
        if str(physical) in physical_stamps and physical_stamps[str(physical)] != before:
            raise ValueError('physical input file changed during freeze')
        if row['offset'] < 0 or row['bytes'] < 1 or row['offset'] + row['bytes'] > stamp.st_size:
            raise ValueError('physical input range outside file')
        with physical.open('rb') as file:
            file.seek(row['offset'])
            if file.read(row['bytes']) != data:
                raise ValueError('snapshot differs from current game input: ' + relative.name)
        after = physical.stat()
        if before != (after.st_size, after.st_mtime_ns):
            raise ValueError('physical input file changed while reading')
        physical_stamps[str(physical)] = before
        save(output / 'pristine' / relative, data)
        originals[relative.name] = (row, data)
    table_bytes = checked_bytes(args.table.resolve())
    save(output / 'accepted-table.tsv', table_bytes)
    rows = list(csv.reader(table_bytes.decode('utf-8-sig').splitlines(), delimiter='\t'))
    if len(rows[0]) != 4 or rows[0][:2] != ['EFF_LOADING_INPUT_SUBSTITUTIONS', '3'] or rows[0][2] != package['modId']:
        raise ValueError('requires accepted version 3 table for current Mod')
    substitutions = {}
    for row in rows[1:]:
        if len(row) != 7:
            raise ValueError('invalid accepted table row')
        path = Path(row[4]); name = path.name
        if name in substitutions or name not in originals:
            raise ValueError('accepted input absent or ambiguous in pristine graph')
        source = originals[name][0]
        if (str(Path(row[0]).resolve()).lower(), int(row[1]), int(row[2]), row[3]) != (
                str(Path(source['physicalPath']).resolve()).lower(), source['offset'], source['bytes'], source['sha256']):
            raise ValueError('accepted table source range differs: ' + name)
        substitutions[name] = checked_bytes(path, row[6], int(row[5]))
    for name, (_, data) in originals.items():
        save(output / 'accepted' / name, substitutions.get(name, data))
    report.update(inputRows=len(substitutions), pristineInputs=len(originals),
                  inputContractDigest=rows[0][3], inputTableSha256=sha(table_bytes),
                  originalPhysicalRangesVerified=True)
    report['package'] = json.loads(run([args.checker, '--package-digest', frozen_mod], output / 'package-digest.log')['stdout'])
    report['tableCheck'] = run([args.checker, '--check-loading-table', frozen_mod, args.table, args.game], output / 'table-check.log')
    report['runs'] = []
    for number in (1, 2):
        rebuilt, prepared = output / f'rebuilt-{number}', output / f'prepared-{number}'
        build = run([args.checker, '--loading-rebuild', frozen_mod, output / 'pristine' / 'sources',
                     output / 'pristine' / 'consumers', rebuilt], output / f'build-{number}.log')
        control = run([args.shape_preparer, rebuilt, frozen_mod, prepared], output / f'shape-{number}.log')
        hashes = {p.name: sha(checked_bytes(p)) for p in sorted(prepared.glob('*.vfs'))}
        if set(hashes) != set(originals):
            raise ValueError('rebuilt input coverage differs')
        report['runs'].append(dict(build=build, shapePreparation=control, hashes=hashes))
    report['deterministicRebuildVerified'] = report['runs'][0]['hashes'] == report['runs'][1]['hashes']
    if not report['deterministicRebuildVerified']:
        raise ValueError('two frozen rebuilds differ')
    accepted = {p.name: sha(checked_bytes(p)) for p in sorted((output / 'accepted').glob('*.vfs'))}
    actual = report['runs'][0]['hashes']
    report['acceptedByteMatches'] = [name for name in accepted if accepted[name] == actual[name]]
    report['acceptedByteDifferences'] = [dict(file=name, acceptedSha256=accepted[name], rebuiltSha256=actual[name])
                                         for name in accepted if accepted[name] != actual[name]]
    report['acceptedContentReproducedExactly'] = not report['acceptedByteDifferences']
    report['preparedAudit'] = run([args.auditor, output / 'prepared-1', output / 'prepared-graph-audit.json'], output / 'prepared-audit.log')
    audit = json.loads((output / 'prepared-graph-audit.json').read_text(encoding='utf8'))
    report['dependencyAudit'] = {key: audit[key] for key in ('externalBoundaryMembers', 'internalReferenceUses',
        'boundaryReferenceUses', 'brokenReferenceUses', 'brokenStreams', 'unparsedObjects', 'boundaryStreams', 'outgoingClosureVerified')}
    for name in author:
        if sha(checked_bytes(mod / name)) != before_author[name]:
            raise ValueError('author changed before candidate ready')
    if sha(checked_bytes(args.table.resolve())) != report['inputTableSha256']:
        raise ValueError('production table changed during preparation')
    for path, before in physical_stamps.items():
        stamp = Path(path).stat()
        if before != (stamp.st_size, stamp.st_mtime_ns):
            raise ValueError('physical inputs changed before candidate ready')
    report.update(status='offline-baseline-prepared', originalPhysicalFilesStable=True,
                  authorSnapshotStable=True, productionTableUnchanged=True,
                  readyForNativeCandidateLoading=False)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('mod', 'graph', 'table', 'game', 'checker', 'shape-preparer', 'auditor', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError('baseline preparation destination exists')
    args.output.mkdir(parents=True)
    report = dict(format='EFF_FULL_RELOAD_OFFLINE_BASELINE', version=1, status='preparing',
                  nativeSubmission=False, productionTablePublished=False, candidateGenerationCreated=False,
                  generationCacheIsolationVerified=False, existingInstanceResourceMigrationVerified=False,
                  completeResourceHotReloadImplemented=False)
    start = time.perf_counter()
    try:
        prepare(args, report)
    except Exception as error:
        report.update(status='failed', error=str(error), readyForNativeCandidateLoading=False)
        raise
    finally:
        report['elapsedMs'] = (time.perf_counter() - start) * 1000
        save(args.output / 'manifest.json', (json.dumps(report, ensure_ascii=False, indent=2) + '\n').encode('utf8'))
    print(json.dumps({k: report[k] for k in ('status','pristineInputs','inputRows','deterministicRebuildVerified',
        'acceptedContentReproducedExactly','readyForNativeCandidateLoading','elapsedMs')}, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
