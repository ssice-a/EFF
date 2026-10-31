"""Package shared EFF native tools for publication by the EFF repository."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[2]
TEXCONV_SHA256 = 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06'
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/Blender'))
import eiem_offline_reload as offline


def _digest(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def _write_file(target, source):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def package(output, compiler=None, dll_protocol=None,
            framework_tag=None, manifest_output=None):
    if not framework_tag:
        raise ValueError('--framework-tag is required for shared tools')
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    compiler = Path(compiler or ROOT / 'bin/resource-pack/eff_resource_pack.exe').resolve()
    protocol = offline._compiler_protocol(compiler)
    if protocol.get('sourcePreparation') != offline.SOURCE_PREPARATION:
        raise ValueError('Shared compiler lacks in-process source preparation')
    if not dll_protocol:
        raise ValueError('--dll-protocol is required')
    declared = json.loads(Path(dll_protocol).read_text(encoding='utf-8-sig'))
    for key in ('compiledMod', 'compiledModStore', 'resourceUpdate'):
        if protocol.get(key) != declared.get(key):
            raise ValueError('Compiler and DLL protocol differ at ' + key)
    for key, value in protocol.get('structures', {}).items():
        if declared.get('structures', {}).get(key) != value:
            raise ValueError('Compiler and DLL protocol differ at structures.' + key)

    archive = output / ('EFF_Shared_Native_Tools_' + framework_tag + '.zip')
    if archive.exists():
        raise FileExistsError('Shared tools archive already exists: ' + str(archive))
    source_files = {
        'nativepack/eff_resource_pack.exe': compiler,
        'nativepack/eff_author_assets.dll': ROOT / 'bin/author-assets/eff_author_assets_format3.dll',
        'nativepack/eff_source_index.exe': ROOT / 'bin/source-index-helper/eff_source_index.exe',
        'nativepack/eff_author_core.dll': ROOT / 'bin/author-core/eff_author_core.dll',
        'nativepack/texconv.exe': ROOT / 'bin/texconv/texconv.exe',
    }
    vendor = ROOT / 'bin/author-core/vendor'
    if not (vendor / 'PIL/__init__.py').is_file():
        raise FileNotFoundError('Build Pillow for Blender Python under bin/author-core/vendor')
    for abi in ('cp311', 'cp313'):
        extension = vendor / 'PIL' / f'_imaging.{abi}-win_amd64.pyd'
        if not extension.is_file():
            raise FileNotFoundError('Pillow vendor is missing Blender ' + abi + ' extension: ' + str(extension))
    for name, source in source_files.items():
        if not source.is_file():
            raise FileNotFoundError('Missing shared native tool: ' + str(source))
    if _digest(source_files['nativepack/texconv.exe']).upper() != TEXCONV_SHA256:
        raise ValueError('DirectXTex texconv.exe version/hash differs from the pinned May 2026 build')

    with tempfile.TemporaryDirectory(prefix='.eff-shared-tools-', dir=output) as temp:
        package_root = Path(temp) / 'EFF_Native_Tools'
        for name, source in source_files.items():
            _write_file(package_root / name, source)
        _write_file(package_root / 'nativepack/THIRD_PARTY_NOTICES',
                    ROOT / 'THIRD_PARTY_NOTICES')
        protocol_path = package_root / 'nativepack/package-protocol.json'
        protocol_path.write_text(json.dumps(protocol, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        shutil.copytree(vendor, package_root / 'vendor',
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as target:
            for path in sorted(package_root.rglob('*')):
                if path.is_file():
                    target.write(path, 'EFF_Native_Tools/' + path.relative_to(package_root).as_posix())

    digest = _digest(archive)
    receipt = {
        'format': 'EFF_SHARED_NATIVE_TOOLS/1',
        'framework': framework_tag,
        'url': 'https://github.com/ssice-a/EFF/releases/download/' + framework_tag + '/' + archive.name,
        'bytes': archive.stat().st_size,
        'sha256': digest,
        'archiveRoot': 'EFF_Native_Tools',
        'compiler': 'nativepack/eff_resource_pack.exe',
        'authorAssets': 'nativepack/eff_author_assets.dll',
        'indexUpdater': 'nativepack/eff_source_index.exe',
        'authorCore': 'nativepack/eff_author_core.dll',
        'protocol': 'nativepack/package-protocol.json',
        'textureCompressor': 'nativepack/texconv.exe',
    }
    manifest_path = Path(manifest_output) if manifest_output else output / 'eff-native-tools.json'
    manifest_path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('PASS shared native tools:', archive, 'bytes:', archive.stat().st_size, 'sha256:', digest)
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--dll-protocol', type=Path, required=True)
    parser.add_argument('--framework-tag', required=True)
    parser.add_argument('--manifest-output', type=Path)
    args = parser.parse_args()
    package(args.output, args.compiler, args.dll_protocol,
            framework_tag=args.framework_tag,
            manifest_output=args.manifest_output)
