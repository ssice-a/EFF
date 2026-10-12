"""Regression for valid UV, color and normal corner seams during real export."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BlenderCornerExportTests(unittest.TestCase):
    def test_corner_mapping_preserves_all_vertex_streams(self):
        blender = os.environ.get('EIEM_BLENDER') or shutil.which('blender')
        if not blender:
            self.skipTest('Requires EIEM_BLENDER')
        with tempfile.TemporaryDirectory(prefix='eiem-corner-export-') as directory:
            result = subprocess.run([
                blender, '--background', '--factory-startup', '--python-exit-code', '1',
                '--python', str(ROOT / 'tools/Blender/test_eiem_corner_export.py'), '--',
                str(ROOT / 'tools/Blender/eiem_blender_addon.py'), directory,
            ], capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('EFF_CORNER_EXPORT_OK', result.stdout)


if __name__ == '__main__':
    unittest.main()
