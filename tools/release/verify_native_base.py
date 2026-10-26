"""Verify a pinned Blender native base before an EFF or Blender release."""
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


def verify_native_base(archive_path, dll_protocol_path, manifest_path,
                       expected_sha256=None):
    archive_path = Path(archive_path)
    manifest = json.loads(Path(manifest_path).read_text(encoding='utf-8-sig'))
    if manifest.get('format') != 'EFF_BLENDER_NATIVE_BASE/1':
        raise ValueError('unsupported native-base manifest format')
    actual_size = archive_path.stat().st_size
    digest = hashlib.sha256()
    with archive_path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    actual_sha256 = digest.hexdigest()
    if actual_size != manifest.get('bytes'):
        raise ValueError('native base size differs from native-base.json')
    if actual_sha256.lower() != str(manifest.get('sha256', '')).lower():
        raise ValueError('native base SHA256 differs from native-base.json')
    if expected_sha256 and actual_sha256.lower() != expected_sha256.lower():
        raise ValueError('native base SHA256 differs from the requested artifact')
    url = manifest.get('url', '')
    parsed_url = urlparse(url)
    parts = parsed_url.path.strip('/').split('/')
    if (parsed_url.scheme != 'https' or parsed_url.netloc != 'github.com'
            or len(parts) != 6 or parts[:4] != ['ssice-a', 'EFF', 'releases', 'download']
            or parts[4] != manifest.get('framework')
            or parts[5] != archive_path.name):
        raise ValueError('native-base.json URL must name this immutable EFF release asset on github.com')

    required_files = {
        'EFF_Blender/nativepack/package-protocol.json',
        'EFF_Blender/nativepack/eff_resource_pack.exe',
        'EFF_Blender/nativepack/eff_author_core.dll',
        'EFF_Blender/nativepack/eff_source_index.exe',
        'EFF_Blender/vendor/PIL/__init__.py',
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
            package.read('EFF_Blender/nativepack/package-protocol.json'),
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
    receipt = verify_native_base(args.archive, args.dll_protocol,
                                 args.manifest, args.sha256)
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
