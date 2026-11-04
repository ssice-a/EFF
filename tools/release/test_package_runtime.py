import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import zipfile

import package_runtime


class RuntimePackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.sources = {}
        for index, name in enumerate(package_runtime.RUNTIME_FILES):
            source = self.root / ('source-' + str(index))
            source.write_bytes(('payload-' + str(index)).encode())
            self.sources[name] = source

    def tearDown(self):
        self.temporary.cleanup()

    def test_package_contains_only_runtime_install_files_and_matches_local_outputs(self):
        output = self.root / 'assets'
        with patch.object(package_runtime, 'RUNTIME_FILES', self.sources), \
                patch.object(package_runtime, 'source_version', return_value='v2.0.6'):
            receipt = package_runtime.package('v2.0.6', output)
            archive_path = output / 'EFF_v2.0.6_dll.zip'
            verified = package_runtime.verify(archive_path, 'v2.0.6')

        self.assertEqual(receipt, verified)
        with zipfile.ZipFile(archive_path) as archive:
            self.assertEqual(set(archive.namelist()),
                             set(self.sources) | {package_runtime.EMPTY_DIRECTORY})
            for name, source in self.sources.items():
                self.assertEqual(archive.read(name), source.read_bytes())

    def test_verifier_rejects_runtime_binary_that_differs_from_local_build(self):
        archive_path = self.root / 'altered.zip'
        expected = set(self.sources) | {package_runtime.EMPTY_DIRECTORY}
        with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED) as archive:
            for name, source in self.sources.items():
                payload = source.read_bytes()
                if name == 'plugin/eff.dll':
                    payload += b'-stale'
                archive.writestr(name, payload)
            archive.writestr(package_runtime.EMPTY_DIRECTORY, b'')

        with patch.object(package_runtime, 'RUNTIME_FILES', self.sources), \
                patch.object(package_runtime, 'source_version', return_value='v2.0.6'):
            with self.assertRaisesRegex(ValueError, 'differs from local file: plugin/eff.dll'):
                package_runtime.verify(archive_path, 'v2.0.6')


if __name__ == '__main__':
    unittest.main()
