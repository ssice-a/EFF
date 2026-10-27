"""Verify the shared native tools that are distributed by EFF releases."""
import argparse
import hashlib
import json
from pathlib import Path
from urllib.parse import urlparse
import zipfile


PROTOCOL_KEYS = ('compiledMod', 'compiledModStore', 'resourceUpdate')


def _read_json(data, label):
    try:
        return json.loads(data.decode('utf-8-sig'))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(label + ' is not valid JSON') from error


def verify_protocol(base_protocol, dll_protocol):
    if base_protocol.get('abi') != 35:
        raise ValueError('native base must use the production ABI 35')
    for key in PROTOCOL_KEYS:
        if base_protocol.get(key) != dll_protocol.get(key):
            raise ValueError('native base compiler protocol differs at ' + key)
    for key, value in base_protocol.get('structures', {}).items():
        if dll_protocol.get('structures', {}).get(key) != value:
            raise ValueError('native base compiler protocol differs at structures.' + key)


def verify_shared_tools(archive_path, dll_protocol_path, manifest_path,
                        expected_sha256=None):
    archive_path = Path(archive_path)
    manifest = json.loads(Path(manifest_path).read_text(encoding='utf-8-sig'))
    if manifest.get('format') != 'EFF_SHARED_NATIVE_TOOLS/1':
        raise ValueError('unsupported shared native tools manifest format')
    expected_layout = {
        'archiveRoot': 'EFF_Native_Tools',
        'compiler': 'nativepack/eff_resource_pack.exe',
        'indexUpdater': 'nativepack/eff_source_index.exe',
        'authorCore': 'nativepack/eff_author_core.dll',
        'protocol': 'nativepack/package-protocol.json',
    }
    if any(manifest.get(key) != value for key, value in expected_layout.items()):
        raise ValueError('shared native tools manifest has an unsupported file layout')
    actual_size = archive_path.stat().st_size
    digest = hashlib.sha256()
    with archive_path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    actual_sha256 = digest.hexdigest()
    if actual_size != manifest.get('bytes'):
        raise ValueError('shared tools size differs from manifest')
    if actual_sha256.lower() != str(manifest.get('sha256', '')).lower():
        raise ValueError('shared tools SHA256 differs from manifest')
    if expected_sha256 and actual_sha256.lower() != expected_sha256.lower():
        raise ValueError('shared tools SHA256 differs from the requested artifact')
    url = manifest.get('url', '')
    parsed_url = urlparse(url)
    parts = parsed_url.path.strip('/').split('/')
    if (parsed_url.scheme != 'https' or parsed_url.netloc != 'github.com'
            or len(parts) != 6 or parts[:4] != ['ssice-a', 'EFF', 'releases', 'download']
            or parts[4] != manifest.get('framework')
            or parts[5] != archive_path.name):
        raise ValueError('manifest URL must name this immutable EFF release asset on github.com')

    required_files = {
        'EFF_Native_Tools/nativepack/package-protocol.json',
        'EFF_Native_Tools/nativepack/eff_resource_pack.exe',
        'EFF_Native_Tools/nativepack/eff_author_core.dll',
        'EFF_Native_Tools/nativepack/eff_source_index.exe',
        'EFF_Native_Tools/vendor/PIL/__init__.py',
        'EFF_Native_Tools/vendor/PIL/_imaging.cp311-win_amd64.pyd',
        'EFF_Native_Tools/vendor/PIL/_imaging.cp313-win_amd64.pyd',
    }
    with zipfile.ZipFile(archive_path) as package:
        damaged = package.testzip()
        if damaged:
            raise ValueError('native base ZIP has a damaged member: ' + damaged)
        names = set(package.namelist())
        missing = required_files - names
        if missing:
            raise ValueError('native base is missing required files: '
                             + ', '.join(sorted(missing)))
        base_protocol = _read_json(
            package.read('EFF_Native_Tools/nativepack/package-protocol.json'),
            'native base protocol')

    dll_protocol = json.loads(Path(dll_protocol_path).read_text(encoding='utf-8-sig'))
    verify_protocol(base_protocol, dll_protocol)
    return {
        'bytes': actual_size,
        'sha256': actual_sha256,
        'framework': manifest.get('framework'),
        'url': url,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, required=True)
    parser.add_argument('--dll-protocol', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--sha256')
    args = parser.parse_args()
    receipt = verify_shared_tools(args.archive, args.dll_protocol,
                                  args.manifest, args.sha256)
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
