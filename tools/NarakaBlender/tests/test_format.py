import importlib.util
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("naraka_format", Path(__file__).parents[1] / "naraka_blender" / "format.py")
format_io = importlib.util.module_from_spec(spec)
spec.loader.exec_module(format_io)


class FormatTests(unittest.TestCase):
    def test_paths_stay_inside_package(self):
        with tempfile.TemporaryDirectory() as root:
            self.assertEqual(format_io.safe_path(root, "meshes/a.mesh"), Path(root).resolve() / "meshes/a.mesh")
            with self.assertRaises(ValueError):
                format_io.safe_path(root, "../other.mesh")

    def test_truncated_arrays_are_rejected(self):
        reader = format_io.Reader(b"\x03\0\0\0\0\0\0\0")
        with self.assertRaises(ValueError):
            reader.array("f")

    def test_unterminated_string_length_is_rejected(self):
        with self.assertRaises(ValueError):
            format_io.Reader(b"\x80" * 5).string()


if __name__ == "__main__":
    unittest.main()
