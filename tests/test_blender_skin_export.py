"""Full shared-skeleton palette expansion through Blender's actual writer."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]

class BlenderSkinExportTests(unittest.TestCase):
    def test_shared_skeleton_palette(self):
        blender=os.environ.get('EIEM_BLENDER') or shutil.which('blender')
        if not blender: self.skipTest('Requires EIEM_BLENDER')
        with tempfile.TemporaryDirectory(prefix='eiem-skin-test-') as directory:
            result=subprocess.run([blender,'--background','--factory-startup','--python-exit-code','1',
                '--python',str(ROOT/'tools/Blender/test_eiem_skin_export.py'),'--',
                str(ROOT/'tools/Blender/eiem_blender_addon.py'),directory],
                capture_output=True,text=True,encoding='utf-8',errors='replace',timeout=90)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('EIEM_SKIN_EXPORT_OK',result.stdout)
            import importlib.util
            module_path = ROOT / 'tools/Blender/eiem_format.py'
            spec = importlib.util.spec_from_file_location('eiem_current_format', module_path)
            fmt = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(fmt)
            mesh = Path(directory) / 'expanded.mesh'
            parsed = fmt.read_mesh(mesh)
            self.assertEqual(parsed['version'], 6)
            self.assertEqual(len(parsed['bone_source_candidates']), len(parsed['bindposes']))
            old = Path(directory) / 'old.mesh'
            data = bytearray(mesh.read_bytes())
            data[8:12] = (5).to_bytes(4, 'little')
            old.write_bytes(data)
            with self.assertRaisesRegex(ValueError, 'unsupported EFF mesh version'):
                fmt.read_mesh(old)

if __name__=='__main__': unittest.main()
