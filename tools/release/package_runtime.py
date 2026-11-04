"""Create and verify the user-facing EFF runtime ZIP from local build outputs."""
import argparse
import hashlib
import re
from pathlib import Path
import zipfile


ROOT = Path(__file__).resolve().parents[2]
RUNTIME_FILES = {
    'plugin/eff.dll': ROOT / 'bin/resource-directory-entry-format3/eff.dll',
    'plugin/eff.ini': ROOT / 'bin/resource-directory-entry-format3/eff.ini',
    'plugin/package-protocol.json': ROOT / 'bin/resource-directory-entry-format3/package-protocol.json',
    'plugin/LICENSE': ROOT / 'LICENSE',
    'plugin/THIRD_PARTY_NOTICES': ROOT / 'THIRD_PARTY_NOTICES',
    'd3dcompiler_47.dll': ROOT / 'bin/resource-directory-entry-format3/d3dcompiler_47.dll',
    'vulkan-1.dll': ROOT / 'bin/resource-directory-entry-format3/vulkan-1.dll',
}
EMPTY_DIRECTORY = 'plugin/mods/'


def source_version():
    header = (ROOT / 'src/eiem_version.h').read_text(encoding='utf-8')
    parts = []
    for name in ('MAJOR', 'MINOR', 'PATCH'):
        match = re.search(rf'^#define EIEM_VERSION_{name}\s+(\d+)\s*$', header, re.MULTILINE)
        if not match:
            raise ValueError('Missing EIEM_VERSION_' + name)
        parts.append(match.group(1))
    return 'v' + '.'.join(parts)


def verify(archive_path, tag):
    expected = set(RUNTIME_FILES) | {EMPTY_DIRECTORY}
    with zipfile.ZipFile(archive_path) as archive:
        names = [entry.filename for entry in archive.infolist()]
        if len(names) != len(set(names)) or set(names) != expected:
            raise ValueError('runtime ZIP contains unexpected or missing files: '
                             + ', '.join(sorted(set(names) ^ expected)))
        damaged = archive.testzip()
        if damaged:
            raise ValueError('runtime ZIP has a damaged member: ' + damaged)
        for name, source in RUNTIME_FILES.items():
            if archive.read(name) != source.read_bytes():
                raise ValueError('runtime ZIP differs from local file: ' + name)
    if tag != source_version():
        raise ValueError(f'tag {tag} does not match local source {source_version()}')
    return {
        'name': Path(archive_path).name,
        'bytes': Path(archive_path).stat().st_size,
        'sha256': hashlib.sha256(Path(archive_path).read_bytes()).hexdigest(),
    }


def package(tag, output):
    if tag != source_version():
        raise ValueError(f'tag {tag} does not match local source {source_version()}')
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    archive_path = output / f'EFF_{tag}_dll.zip'
    if archive_path.exists():
        raise FileExistsError('Refusing to overwrite a prepared runtime ZIP: ' + str(archive_path))
    with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for name, source in RUNTIME_FILES.items():
            if not source.is_file():
                raise FileNotFoundError('Missing local runtime output: ' + str(source))
            archive.write(source, name)
        archive.writestr(EMPTY_DIRECTORY, b'')
    receipt = verify(archive_path, tag)
    print('PASS runtime package:', archive_path, 'bytes:', receipt['bytes'],
          'sha256:', receipt['sha256'])
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--archive', type=Path)
    args = parser.parse_args()
    if args.verify:
        if not args.archive:
            parser.error('--archive is required with --verify')
        print(verify(args.archive, args.tag))
    else:
        package(args.tag, args.output)


if __name__ == '__main__':
    main()
