import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('local_shared_deploy',
    Path(__file__).with_name('deploy_shared_author_assets.py'))
deploy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy)


class SharedDeploymentTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.old = self.root / 'tools' / ('a' * 64)
        for relative in ['nativepack/' + n for n in (
                'eff_author_assets.dll', 'eff_author_core.dll', 'texconv.exe')] + [
                'vendor/' + n for n in deploy.shared.PIL_INSTALL_FILES] + ['nativepack/kept.txt']:
            path = self.old / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'original')
        (self.root / 'tools/current.json').write_text(json.dumps(dict(
            format=deploy.shared.TOOLS_INSTALL_FORMAT, sha256=self.old.name,
            release='previous')), encoding='utf-8')
        self.before = (self.root / 'tools/current.json').read_bytes()
        self.author = self.root / 'author.dll'
        self.author.write_bytes(b'new author DLL')
        self.settings = patch.object(deploy.shared, 'resource_root', return_value=self.root)
        self.settings.start()
        self.addCleanup(self.settings.stop)

    def test_new_generation_preserves_old_files_and_uses_archive_identity(self):
        with patch.object(deploy.subprocess, 'run') as validate:
            target = deploy.deploy(self.author, self.root / 'receipt')
        validate.assert_called_once()
        self.assertEqual(target.name, deploy.digest(self.root / 'receipt/EFF_Shared_Native_Tools_local.zip'))
        self.assertEqual((target / 'nativepack/eff_author_assets.dll').read_bytes(), b'new author DLL')
        self.assertEqual((target / 'nativepack/kept.txt').read_bytes(), b'original')
        self.assertEqual((self.old / 'nativepack/eff_author_assets.dll').read_bytes(), b'original')
        self.assertEqual(deploy.shared._installed_tools(self.root), target)
        self.assertEqual(deploy.deploy(self.author, self.root / 'noop'), target)

    def test_failed_native_validation_keeps_previous_selection(self):
        with patch.object(deploy.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'test')):
            with self.assertRaises(subprocess.CalledProcessError):
                deploy.deploy(self.author, self.root / 'failed')
        self.assertEqual((self.root / 'tools/current.json').read_bytes(), self.before)
        self.assertEqual(deploy.shared._installed_tools(self.root), self.old)


if __name__ == '__main__':
    unittest.main()
