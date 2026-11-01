import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

from verify_shared_tools import verify_shared_tools


class ReleasePreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.archive = self.root / 'EFF_Shared_Native_Tools_v1.10.2.zip'
        self.manifest = self.root / 'eff-native-tools.json'
        with zipfile.ZipFile(self.archive, 'w') as package:
            package.writestr('EFF_Native_Tools/nativepack/eff_resource_pack.exe', b'compiler')
            package.writestr('EFF_Native_Tools/nativepack/eff_source_index.exe', b'index updater')
            package.writestr('EFF_Native_Tools/nativepack/package-protocol.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/eff_author_assets.dll',
                             b'author assets')
            package.writestr('EFF_Native_Tools/nativepack/eff_author_core.dll',
                             b'author core')
            package.writestr('EFF_Native_Tools/nativepack/texconv.exe', b'texconv')
            package.writestr('EFF_Native_Tools/nativepack/eff_migrate_mods.exe', b'migrator')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/eff_migrate_sources.exe', b'exporter')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.dll', b'exporter dll')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.deps.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.runtimeconfig.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/THIRD_PARTY_NOTICES', b'licenses')
            package.writestr('EFF_Native_Tools/vendor/PIL/__init__.py', b'')
            package.writestr('EFF_Native_Tools/vendor/PIL/_imaging.cp311-win_amd64.pyd',
                             b'cp311 imaging')
            package.writestr('EFF_Native_Tools/vendor/PIL/_imaging.cp313-win_amd64.pyd',
                             b'cp313 imaging')
        digest = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(json.dumps({
            'format': 'EFF_SHARED_NATIVE_TOOLS/3',
            'framework': 'v1.10.2',
            'url': 'https://github.com/ssice-a/EFF/releases/download/v1.10.2/'
                   + self.archive.name,
            'bytes': self.archive.stat().st_size,
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
        }), encoding='utf-8')

    def tearDown(self):
        self.temp.cleanup()

    def test_accepts_format3_repair_author_and_migration_tools(self):
        receipt = verify_shared_tools(self.archive, self.manifest)
        self.assertEqual(self.archive.stat().st_size, receipt['bytes'])
        self.assertEqual(json.loads(self.manifest.read_text())['sha256'],
                         receipt['sha256'])
        self.assertEqual('v1.10.2', receipt['framework'])

    def test_rejects_wrong_requested_digest(self):
        with self.assertRaisesRegex(ValueError, 'requested artifact'):
            verify_shared_tools(self.archive, self.manifest, '0' * 64)

    def test_rejects_non_github_asset_url(self):
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['url'] = manifest['url'].replace(
            'https://github.com/', 'https://example.com/')
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'immutable EFF release asset'):
            verify_shared_tools(self.archive, self.manifest)

    def test_rejects_manifest_digest_drift(self):
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['sha256'] = '0' * 64
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'SHA256 differs from manifest'):
            verify_shared_tools(self.archive, self.manifest)

    def test_rejects_missing_repair_compiler(self):
        with zipfile.ZipFile(self.archive, 'w') as package:
            for name in (
                    'eff_source_index.exe', 'package-protocol.json', 'eff_author_assets.dll',
                    'eff_author_core.dll', 'texconv.exe', 'eff_migrate_mods.exe'):
                package.writestr('EFF_Native_Tools/nativepack/' + name, b'tool')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/eff_migrate_sources.exe', b'exporter')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.dll', b'exporter dll')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.deps.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.runtimeconfig.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/THIRD_PARTY_NOTICES', b'licenses')
            package.writestr('EFF_Native_Tools/vendor/PIL/__init__.py', b'')
            package.writestr('EFF_Native_Tools/vendor/PIL/_imaging.cp311-win_amd64.pyd', b'cp311')
            package.writestr('EFF_Native_Tools/vendor/PIL/_imaging.cp313-win_amd64.pyd', b'cp313')
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['bytes'] = self.archive.stat().st_size
        manifest['sha256'] = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'missing required files'):
            verify_shared_tools(self.archive, self.manifest)

    def test_rejects_missing_archive_members(self):
        with zipfile.ZipFile(self.archive, 'w') as package:
            package.writestr('readme.txt', 'not a native base')
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['bytes'] = self.archive.stat().st_size
        manifest['sha256'] = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'missing required files'):
            verify_shared_tools(self.archive, self.manifest)

    def test_rejects_missing_legacy_migrator(self):
        with zipfile.ZipFile(self.archive, 'w') as package:
            package.writestr('EFF_Native_Tools/nativepack/eff_author_assets.dll', b'assets')
            package.writestr('EFF_Native_Tools/nativepack/eff_author_core.dll', b'core')
            package.writestr('EFF_Native_Tools/nativepack/texconv.exe', b'texconv')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/eff_migrate_sources.exe', b'exporter')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.dll', b'exporter dll')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.deps.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.runtimeconfig.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/THIRD_PARTY_NOTICES', b'licenses')
            package.writestr('EFF_Native_Tools/vendor/PIL/__init__.py', b'')
            package.writestr('EFF_Native_Tools/vendor/PIL/_imaging.cp311-win_amd64.pyd', b'cp311')
            package.writestr('EFF_Native_Tools/vendor/PIL/_imaging.cp313-win_amd64.pyd', b'cp313')
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['bytes'] = self.archive.stat().st_size
        manifest['sha256'] = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'missing required files'):
            verify_shared_tools(self.archive, self.manifest)

    def test_rejects_missing_blender_52_pillow_extension(self):
        with zipfile.ZipFile(self.archive, 'w') as package:
            package.writestr('EFF_Native_Tools/nativepack/eff_author_assets.dll',
                             b'author assets')
            package.writestr('EFF_Native_Tools/nativepack/eff_author_core.dll',
                             b'author core')
            package.writestr('EFF_Native_Tools/nativepack/texconv.exe', b'texconv')
            package.writestr('EFF_Native_Tools/nativepack/eff_migrate_mods.exe', b'migrator')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/eff_migrate_sources.exe', b'exporter')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.dll', b'exporter dll')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.deps.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/source-exporter/AnimeStudio.GUI.runtimeconfig.json', b'{}')
            package.writestr('EFF_Native_Tools/nativepack/THIRD_PARTY_NOTICES', b'licenses')
            package.writestr('EFF_Native_Tools/vendor/PIL/__init__.py', b'')
            package.writestr('EFF_Native_Tools/vendor/PIL/_imaging.cp311-win_amd64.pyd',
                             b'cp311 imaging')
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['bytes'] = self.archive.stat().st_size
        manifest['sha256'] = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, '_imaging.cp313-win_amd64.pyd'):
            verify_shared_tools(self.archive, self.manifest)


if __name__ == '__main__':
    unittest.main()
