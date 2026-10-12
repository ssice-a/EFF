import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_physics_document import compile_reader

ROOT = Path(__file__).resolve().parents[1]


class BlenderPhysicsTests(unittest.TestCase):
    def test_authoring_saved_project_and_cpp_interchange(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        if not blender or not shutil.which("cl"): self.skipTest("Requires EIEM_BLENDER and MSVC")
        with tempfile.TemporaryDirectory(prefix="eiem-physics-blender-") as directory:
            folder = Path(directory)
            result = subprocess.run([blender,"--background","--factory-startup","--python-exit-code","1",
                "--python",str(ROOT/"tools/Blender/test_eiem_physics.py"),"--",
                str(ROOT/"tools/Blender/eiem_blender_addon.py"),directory],
                capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=120)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn("EFF_PHYSICS_AUTHORING_OK",result.stdout)
            exe = compile_reader(folder)
            for file in folder.glob("*.physics"):
                run = subprocess.run([str(exe),str(file),"ok","any-v4-count"],capture_output=True,text=True)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                self.assertEqual(run.stdout.strip(), "2 1")


if __name__ == "__main__": unittest.main()
