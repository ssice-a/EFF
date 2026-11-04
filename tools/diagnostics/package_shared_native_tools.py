"""Package shared EFF native tools for publication by the EFF repository."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parents[2]
TEXCONV_SHA256 = 'DCFDEC10244E02CF5037FBA089C55FB7E1326B1C8181742D77D15FA5CB5EEF06'


def _digest(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def _write_file(target, source):
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def package(output, framework_tag=None, manifest_output=None,
            source_exporter=None, migrator=None):
    if not framework_tag:
        raise ValueError('--framework-tag is required for shared tools')
    output = Path(output).resolve()
    output.mkdir(parents=True, exist_ok=True)
    archive = output / ('EFF_Shared_Native_Tools_' + framework_tag + '.zip')
    if archive.exists():
        raise FileExistsError('Shared tools archive already exists: ' + str(archive))
    source_files = {
        'nativepack/eff_resource_pack.exe': ROOT / 'bin/resource-pack/eff_resource_pack.exe',
        'nativepack/eff_source_index.exe': ROOT / 'bin/source-index-helper/eff_source_index.exe',
        'nativepack/package-protocol.json': ROOT / 'bin/resource-directory-entry-format3/package-protocol.json',
        'nativepack/eff_author_assets.dll': ROOT / 'bin/author-assets/eff_author_assets_format3.dll',
        'nativepack/eff_author_core.dll': ROOT / 'bin/author-core/eff_author_core.dll',
        'nativepack/texconv.exe': ROOT / 'bin/texconv/texconv.exe',
        'nativepack/eff_migrate_mods.exe': Path(migrator) if migrator else ROOT / 'bin/legacy-migrator/dist/eff_migrate_mods.exe',
    }
    source_exporter = Path(source_exporter) if source_exporter else ROOT / 'bin/legacy-source-exporter'
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
    for name in ('eff_migrate_sources.exe', 'AnimeStudio.GUI.dll',
                 'AnimeStudio.GUI.deps.json', 'AnimeStudio.GUI.runtimeconfig.json'):
        if not (source_exporter / name).is_file():
            raise FileNotFoundError('Missing AnimeStudio migration source exporter file: ' + str(source_exporter / name))
    if _digest(source_files['nativepack/texconv.exe']).upper() != TEXCONV_SHA256:
        raise ValueError('DirectXTex texconv.exe version/hash differs from the pinned May 2026 build')

    with tempfile.TemporaryDirectory(prefix='.eff-shared-tools-', dir=output) as temp:
        package_root = Path(temp) / 'EFF_Native_Tools'
        for name, source in source_files.items():
            _write_file(package_root / name, source)
        exporter_package = package_root / 'nativepack/source-exporter'
        for source in sorted(source_exporter.iterdir()):
            if source.is_file() and source.suffix.lower() != '.pdb':
                _write_file(exporter_package / source.name, source)
        _write_file(package_root / 'nativepack/THIRD_PARTY_NOTICES',
                    ROOT / 'THIRD_PARTY_NOTICES')
        shutil.copytree(vendor, package_root / 'vendor',
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
        with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as target:
            for path in sorted(package_root.rglob('*')):
                if path.is_file():
                    target.write(path, 'EFF_Native_Tools/' + path.relative_to(package_root).as_posix())

    digest = _digest(archive)
    receipt = {
        'format': 'EFF_SHARED_NATIVE_TOOLS/3',
        'framework': framework_tag,
        'url': 'https://github.com/ssice-a/EFF/releases/download/' + framework_tag + '/' + archive.name,
        'bytes': archive.stat().st_size,
        'sha256': digest,
        'archiveRoot': 'EFF_Native_Tools',
        'compiler': 'nativepack/eff_resource_pack.exe',
        'indexUpdater': 'nativepack/eff_source_index.exe',
        'protocol': 'nativepack/package-protocol.json',
        'authorAssets': 'nativepack/eff_author_assets.dll',
        'authorCore': 'nativepack/eff_author_core.dll',
        'textureCompressor': 'nativepack/texconv.exe',
        'legacyMigrator': 'nativepack/eff_migrate_mods.exe',
        'migrationSourceExporter': 'nativepack/source-exporter/eff_migrate_sources.exe',
    }
    manifest_path = Path(manifest_output) if manifest_output else output / 'eff-native-tools.json'
    manifest_path.write_text(json.dumps(receipt, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('PASS shared native tools:', archive, 'bytes:', archive.stat().st_size, 'sha256:', digest)
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--framework-tag', required=True)
    parser.add_argument('--manifest-output', type=Path)
    parser.add_argument('--source-exporter', type=Path)
    parser.add_argument('--migrator', type=Path)
    args = parser.parse_args()
    package(args.output, framework_tag=args.framework_tag,
            manifest_output=args.manifest_output,
            source_exporter=args.source_exporter,
            migrator=args.migrator)
