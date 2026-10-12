"""Run the actual Blender exporter, including mixed source/new geometry."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BlenderTangentExportTests(unittest.TestCase):
    def test_preserve_source_and_generate_missing_tangents(self):
        blender = os.environ.get('EIEM_BLENDER') or shutil.which('blender')
        if not blender:
            self.skipTest('Requires EIEM_BLENDER')
        with tempfile.TemporaryDirectory(prefix='eiem-tangent-export-') as directory:
            result = subprocess.run([
                blender, '--background', '--factory-startup', '--python-exit-code', '1',
                '--python', str(ROOT / 'tools/Blender/test_eiem_tangent_export.py'), '--',
                str(ROOT / 'tools/Blender/eiem_blender_addon.py'), directory,
            ], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('EFF_TANGENT_EXPORT_OK', result.stdout)


if __name__ == '__main__':
    unittest.main()
