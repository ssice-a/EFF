import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import zipfile

from verify_native_base import verify_native_base


BASE_PROTOCOL = {
    'abi': 35,
    'compiledMod': 'EFF_COMPILED_MOD/2',
    'compiledModStore': 'EFF_MOD_BLOCKS/2',
    'resourceUpdate': 'addressed-resource-rebind/1',
    'structures': {'effectAttachment': 'mesh-owner+bone/le32/1'},
}
DLL_PROTOCOL = {key: value for key, value in BASE_PROTOCOL.items() if key != 'abi'}


class ReleasePreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.archive = self.root / 'native.zip'
        self.dll_protocol = self.root / 'dll-protocol.json'
        self.manifest = self.root / 'native-base.json'
        self.dll_protocol.write_text(json.dumps(DLL_PROTOCOL), encoding='utf-8')
        with zipfile.ZipFile(self.archive, 'w') as package:
            package.writestr('EFF_Blender/nativepack/package-protocol.json',
                             json.dumps(BASE_PROTOCOL))
            package.writestr('EFF_Blender/nativepack/eff_resource_pack.exe',
                             b'compiler')
            package.writestr('EFF_Blender/nativepack/eff_author_core.dll',
                             b'author core')
            package.writestr('EFF_Blender/nativepack/eff_source_index.exe',
                             b'source index')
            package.writestr('EFF_Blender/vendor/PIL/__init__.py', b'')
        digest = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(json.dumps({
            'format': 'EFF_BLENDER_NATIVE_BASE/1',
            'framework': 'v1.10.2',
            'url': 'https://github.com/ssice-a/EFF/releases/download/v1.10.2/'
                   'native.zip',
            'bytes': self.archive.stat().st_size,
            'sha256': digest,
        }), encoding='utf-8')

    def tearDown(self):
        self.temp.cleanup()

    def test_accepts_matching_base_and_dll_protocol(self):
        receipt = verify_native_base(self.archive, self.dll_protocol,
                                     self.manifest)
        self.assertEqual(self.archive.stat().st_size, receipt['bytes'])
        self.assertEqual(json.loads(self.manifest.read_text())['sha256'],
                         receipt['sha256'])
        self.assertEqual('v1.10.2', receipt['framework'])

    def test_rejects_wrong_requested_digest(self):
        with self.assertRaisesRegex(ValueError, 'requested artifact'):
            verify_native_base(self.archive, self.dll_protocol, self.manifest,
                               '0' * 64)

    def test_rejects_non_github_asset_url(self):
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['url'] = manifest['url'].replace(
            'https://github.com/', 'https://example.com/')
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'immutable EFF release asset'):
            verify_native_base(self.archive, self.dll_protocol, self.manifest)

    def test_rejects_manifest_digest_drift(self):
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['sha256'] = '0' * 64
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'SHA256 differs from native-base'):
            verify_native_base(self.archive, self.dll_protocol, self.manifest)

    def test_rejects_protocol_mismatch(self):
        changed = dict(DLL_PROTOCOL, resourceUpdate='different/1')
        self.dll_protocol.write_text(json.dumps(changed), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'differs at resourceUpdate'):
            verify_native_base(self.archive, self.dll_protocol, self.manifest)

    def test_rejects_missing_archive_members(self):
        with zipfile.ZipFile(self.archive, 'w') as package:
            package.writestr('readme.txt', 'not a native base')
        manifest = json.loads(self.manifest.read_text(encoding='utf-8'))
        manifest['bytes'] = self.archive.stat().st_size
        manifest['sha256'] = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.manifest.write_text(json.dumps(manifest), encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'missing required files'):
            verify_native_base(self.archive, self.dll_protocol, self.manifest)


if __name__ == '__main__':
    unittest.main()
