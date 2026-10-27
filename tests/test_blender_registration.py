"""Verify the shipped development package, not only its standalone module."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class BlenderRegistrationTests(unittest.TestCase):
    def test_discovery_and_development_reload(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        if not blender:
            self.skipTest("Requires EIEM_BLENDER")
        with tempfile.TemporaryDirectory(prefix="eiem-registration-") as directory:
            package = Path(directory) / "EIEM_Blender"
            package.mkdir()
            for source in (ROOT / "tools/Blender").glob("*.py"):
                if source.name == "__init__.py" or source.name.startswith("eiem_"):
                    shutil.copy2(source, package / source.name)
            shutil.copytree(ROOT / "tools/nativepack", package / "nativepack",
                            ignore=shutil.ignore_patterns("__pycache__", "*.pyc", "test_*.py", "*_experiment.py"))
            result = subprocess.run([
                blender, "--background", "--factory-startup", "--python-exit-code", "1",
                "--python", str(ROOT / "tools/Blender/test_eiem_registration.py"),
                "--", str(package),
            ], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("EFF_REGISTRATION_OK", result.stdout)


if __name__ == "__main__":
    unittest.main()
