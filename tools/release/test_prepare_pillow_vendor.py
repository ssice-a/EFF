import sys
from pathlib import Path
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'diagnostics'))
from prepare_pillow_vendor import prepare_vendor


class PillowVendorTests(unittest.TestCase):
    def _wheel(self, path, abi, source):
        with zipfile.ZipFile(path, 'w') as wheel:
            wheel.writestr('PIL/__init__.py', source)
            wheel.writestr('PIL/Image.py', f'ABI = "{abi}"')
            wheel.writestr(f'PIL/_imaging.{abi}-win_amd64.pyd', abi.encode())
            wheel.writestr(f'PIL/_imagingft.{abi}-win_amd64.pyd', abi.encode())

    def test_merges_pure_python_from_cp313_and_extensions_from_both_abis(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cp311 = root / 'pillow-cp311.whl'
            cp313 = root / 'pillow-cp313.whl'
            vendor = root / 'vendor'
            vendor.mkdir()
            (vendor / 'keep.txt').write_text('preserve')
            self._wheel(cp311, 'cp311', 'from_cp311 = True')
            self._wheel(cp313, 'cp313', 'from_cp313 = True')

            prepare_vendor(cp311, cp313, vendor)

            self.assertEqual((vendor / 'PIL' / '__init__.py').read_text(), 'from_cp313 = True')
            self.assertEqual((vendor / 'PIL' / 'Image.py').read_text(), 'ABI = "cp313"')
            for abi in ('cp311', 'cp313'):
                self.assertTrue((vendor / 'PIL' / f'_imaging.{abi}-win_amd64.pyd').is_file())
                self.assertTrue((vendor / 'PIL' / f'_imagingft.{abi}-win_amd64.pyd').is_file())
            self.assertEqual((vendor / 'keep.txt').read_text(), 'preserve')

    def test_rejects_different_native_module_sets_without_replacing_vendor(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            cp311 = root / 'pillow-cp311.whl'
            cp313 = root / 'pillow-cp313.whl'
            vendor = root / 'vendor'
            vendor.mkdir()
            (vendor / 'sentinel.txt').write_text('old vendor')
            self._wheel(cp311, 'cp311', 'old')
            self._wheel(cp313, 'cp313', 'new')
            with zipfile.ZipFile(cp313, 'a') as wheel:
                wheel.writestr('PIL/_extra.cp313-win_amd64.pyd', b'extra')

            with self.assertRaisesRegex(ValueError, 'native modules differ'):
                prepare_vendor(cp311, cp313, vendor)

            self.assertEqual((vendor / 'sentinel.txt').read_text(), 'old vendor')
            self.assertFalse((vendor / 'PIL').exists())


if __name__ == '__main__':
    unittest.main()
