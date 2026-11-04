"""Verify the three binary-only assets of a published EFF release."""
import argparse
import json
from pathlib import Path
import zipfile

from package_runtime import verify as verify_runtime
from verify_shared_tools import verify_shared_tools


def verify_release(directory, tag):
    directory = Path(directory)
    runtime = directory / f'EFF_{tag}_dll.zip'
    shared = directory / f'EFF_Shared_Native_Tools_{tag}.zip'
    manifest = directory / 'eff-native-tools.json'
    required = (runtime, shared, manifest)
    missing = [path.name for path in required if not path.is_file()]
    if missing:
        raise FileNotFoundError('release assets are missing: ' + ', '.join(missing))
    unexpected = {path.name for path in directory.iterdir() if path.is_file()} - {path.name for path in required}
    if unexpected:
        raise ValueError('release directory contains unapproved files: ' + ', '.join(sorted(unexpected)))
    verify_runtime(runtime, tag)
    tools = verify_shared_tools(shared, manifest)
    data = json.loads(manifest.read_text(encoding='utf-8-sig'))
    if data.get('framework') != tag:
        raise ValueError('shared tools framework does not match release tag')
    with zipfile.ZipFile(shared) as archive:
        source_files = [name for name in archive.namelist()
                        if name.lower().endswith(('.cpp', '.h', '.cs', '.csproj', '.sln', '.blend'))]
        if source_files:
            raise ValueError('shared tools ZIP includes project source: ' + ', '.join(source_files))
    print('PASS release assets:', tag, 'runtime:', runtime.stat().st_size,
          'shared tools:', tools['bytes'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--tag', required=True)
    args = parser.parse_args()
    verify_release(args.directory, args.tag)


if __name__ == '__main__':
    main()
