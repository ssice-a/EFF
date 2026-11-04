import json
from pathlib import Path
import tempfile
import unittest

import install_blender_package


REQUIRED_FILES = (
    '__init__.py',
    'eiem_blender_addon.py',
    'eiem_release.py',
    'eiem_update_worker.py',
    'eiem_shared_resources.py',
    'nativepack/author_source.py',
    'nativepack/author_format3.py',
    'nativepack/effect_native.py',
    'nativepack/mesh.py',
    'eiem_texture_compression.py',
    'physics_presets/chen_soft_ribbon.json',
)


def write_package(root, revision, label):
    files = {}
    for name in REQUIRED_FILES:
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(f'{label}: {name}', encoding='utf-8')
        files[name] = install_blender_package.updates.digest(path)
    (root / install_blender_package.updates.BUILD_FILE).write_text(
        json.dumps({'format': 'EFF_BLENDER_BUILD/1', 'revision': revision,
                    'version': [0, 50, 3], 'files': files}),
        encoding='utf-8')


class BlenderLocalInstallTests(unittest.TestCase):
    def test_install_uses_transactional_update_and_keeps_user_data_and_backup(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            target = root / 'addons' / 'EFF_Blender'
            package = root / 'package' / 'EFF_Blender'
            target.mkdir(parents=True)
            package.mkdir(parents=True)
            write_package(target, 'a' * 40, 'old')
            (target / 'author-note.txt').write_text('keep this file', encoding='utf-8')
            write_package(package, 'b' * 40, 'new')

            result = install_blender_package.install(package, target)

            self.assertEqual(result['status'], 'installed')
            self.assertEqual(result['revision'], 'b' * 40)
            self.assertEqual((target / '__init__.py').read_text(encoding='utf-8'), 'new: __init__.py')
            self.assertEqual((target / 'author-note.txt').read_text(encoding='utf-8'), 'keep this file')
            backup = Path(result['backup'])
            self.assertTrue((backup / '__init__.py').is_file())
            self.assertEqual((backup / '__init__.py').read_text(encoding='utf-8'), 'old: __init__.py')
            self.assertEqual(install_blender_package.updates.read_build(target)['revision'], 'b' * 40)


if __name__ == '__main__':
    unittest.main()
