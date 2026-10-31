"""Measure full format-3 author export through the in-memory DLL ABI."""
import argparse
import configparser
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools' / 'Blender'))

from nativepack import author_format3
from eiem_native_export import write_format3_native_assets


def hashes(root):
    return {path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob('*') if path.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mod', type=Path, help='complete format-2 author Mod')
    parser.add_argument('sources', type=Path, help='directory containing EFFSOURCE packages')
    parser.add_argument('author_snapshots', type=Path,
                        help='editable snapshot files named by Part/Material/Resource ID')
    parser.add_argument('writer_dll', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--probe-sample', type=Path,
                        help='copy the complete candidate for independent acceptance')
    parser.add_argument('--retain-output', type=Path,
                        help='retain the complete format-3 candidate outside the inputs')
    args = parser.parse_args()

    mod = args.mod.resolve()
    source_root = args.sources.resolve()
    before_mod, before_sources = hashes(mod), hashes(source_root)
    ini_bytes = (mod / 'mod.ini').read_bytes()
    ini = configparser.ConfigParser(interpolation=None, delimiters=('=',), strict=True)
    ini.optionxform = str
    ini.read_string(ini_bytes.decode('utf-8-sig', 'strict'))

    resources = {}
    for section in ini.sections():
        relative = ini[section].get('path')
        if not relative:
            continue
        path = (mod / Path(*relative.split('/'))).resolve()
        if not path.is_relative_to(mod) or not path.is_file():
            raise ValueError('Declared format-2 resource is absent or escapes the Mod: ' + relative)
        resources[relative] = path.read_bytes()

    author_files = {}
    for path in sorted(args.author_snapshots.rglob('*')):
        if not path.is_file():
            continue
        relative = path.relative_to(args.author_snapshots).as_posix()
        if '/' in relative:
            raise ValueError('Editable author snapshots must be flat: ' + relative)
        author_files[relative] = path.read_bytes()
    if not author_files:
        raise ValueError('Editable author snapshot directory is empty')

    sources = sorted(source_root.glob('*.eff'), key=lambda path: path.name.casefold())
    if not sources:
        raise ValueError('No EFFSOURCE packages found under ' + str(source_root))
    templates = author_format3.resolve_templates(None, sources, ini_bytes=ini_bytes)
    before_author = hashes(args.author_snapshots.resolve())

    with tempfile.TemporaryDirectory(prefix='eff-memory-author-benchmark-') as folder:
        candidate = Path(folder) / 'format3'
        started = time.perf_counter()
        writer_report = write_format3_native_assets(
            args.writer_dll, ini_bytes, templates, resources, author_files, candidate)
        elapsed = time.perf_counter() - started
        package = author_format3.read_format3_directory(candidate)
        output = hashes(candidate)
        files = package['_files']
        report = dict(mod=mod.name, wallSeconds=elapsed, writerResult=writer_report,
                      format=3, format3Files=len(output),
                      outputBytes=sum((candidate / name).stat().st_size for name in output),
                      meshCount=sum(name.startswith('native/meshes/') for name in files),
                      materialCount=sum(name.startswith('native/materials/') for name in files),
                      textureCount=sum(name.startswith('textures/') and name.endswith('.dds') for name in files),
                      physicsCount=sum(name.startswith('native/physics/') for name in files),
                      outputSHA256=output, modUnchanged=hashes(mod) == before_mod,
                      sourcesUnchanged=hashes(source_root) == before_sources,
                      authorSnapshotsUnchanged=hashes(args.author_snapshots.resolve()) == before_author,
                      completeModExport=True)
        assert report['modUnchanged'] and report['sourcesUnchanged'] and report['authorSnapshotsUnchanged'], \
            'Read-only benchmark input changed'

        def retain(destination, copy):
            destination = destination.resolve()
            inputs = (mod, source_root, args.author_snapshots.resolve())
            if any(destination == root or destination.is_relative_to(root) for root in inputs):
                raise ValueError('Retained output must be outside author/source inputs')
            if destination.exists():
                raise ValueError('Retained output already exists')
            copy(destination)

        if args.retain_output:
            retain(args.retain_output, lambda destination: shutil.copytree(candidate, destination))
        if args.probe_sample:
            retain(args.probe_sample, lambda destination: shutil.copytree(candidate, destination))
        if args.report:
            args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
        print(json.dumps({key: value for key, value in report.items() if key != 'outputSHA256'},
                         ensure_ascii=False))


if __name__ == '__main__':
    main()
