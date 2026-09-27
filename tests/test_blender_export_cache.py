import tempfile
import unittest
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "eiem_export_cache_test", ROOT / "tools/Blender/eiem_export_cache.py")
cache_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cache_module)
ResourceCache = cache_module.ResourceCache


class BlenderExportCacheTests(unittest.TestCase):
    def test_same_size_external_edit_is_repaired_and_empty_roots_are_pruned(self):
        with tempfile.TemporaryDirectory(prefix="eiem-export-cache-") as folder:
            destination = Path(folder) / "mod"
            cache = ResourceCache(destination)
            cache.begin_export()
            target = destination / "meshes" / "body.mesh"
            cache.materialize(
                "meshes/body.mesh", ("v1",), target,
                lambda path: path.write_bytes(b"fresh"))

            cache.begin_export()
            target.write_bytes(b"stale")  # same length, different bytes
            cache.materialize(
                "meshes/body.mesh", ("v1",), target,
                lambda path: path.write_bytes(b"producer-not-used"))
            self.assertEqual(target.read_bytes(), b"fresh")

            cache.begin_export()
            cache.prune_destination()
            self.assertFalse(target.parent.exists())


if __name__ == "__main__":
    unittest.main()
