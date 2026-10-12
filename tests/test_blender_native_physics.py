"""Real extracted Prefab integration, opt in with EIEM_PHYSICS_EVIDENCE."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class BlenderNativePhysicsTests(unittest.TestCase):
    def test_source_selection_mapping_survives_saved_pose_offsets(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        source = os.environ.get("EIEM_PHYSICS_MAPPING_EVIDENCE")
        if not blender or not source:
            self.skipTest("Requires EIEM_BLENDER and EIEM_PHYSICS_MAPPING_EVIDENCE")
        with tempfile.TemporaryDirectory(prefix="eiem-mapping-blender-") as directory:
            result = subprocess.run([blender, "--background", "--factory-startup", "--python-exit-code", "1",
                "--python", str(ROOT / "tools/Blender/test_eiem_native_physics_mapping.py"), "--",
                str(ROOT / "tools/Blender/eiem_blender_addon.py"), source, directory],
                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("EFF_NATIVE_SELECTION_MAPPING_OK", result.stdout)

    def test_unified_physics_editor_and_batch_parameter_roundtrip(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        source = os.environ.get("EIEM_PHYSICS_EVIDENCE")
        if not blender or not source:
            self.skipTest("Requires EIEM_BLENDER and EIEM_PHYSICS_EVIDENCE")
        with tempfile.TemporaryDirectory(prefix="eiem-editor-blender-") as directory:
            result = subprocess.run([blender, "--background", "--factory-startup", "--python-exit-code", "1",
                "--python", str(ROOT / "tools/Blender/test_eiem_physics_editor.py"), "--",
                str(ROOT / "tools/Blender/eiem_blender_addon.py"), source, directory],
                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("EIEM_PHYSICS_EDITOR_OK", result.stdout)

    def test_source_edit_roundtrip_and_mesh_incremental_export(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        source = os.environ.get("EIEM_PHYSICS_EVIDENCE")
        if not blender or not source:
            self.skipTest("Requires EIEM_BLENDER and EIEM_PHYSICS_EVIDENCE (Typhoea component export)")
        with tempfile.TemporaryDirectory(prefix="eiem-native-blender-") as directory:
            result = subprocess.run([blender,"--background","--factory-startup","--python-exit-code","1",
                "--python",str(ROOT/"tools/Blender/test_eiem_native_physics.py"),"--",
                str(ROOT/"tools/Blender/eiem_blender_addon.py"),source,directory],
                capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=120)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn("EFF_NATIVE_PHYSICS_OK",result.stdout)

    def test_normal_package_imports_mesh_rig_and_native_physics_together(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        package = os.environ.get("EIEM_PHYSICS_PACKAGE")
        if not blender or not package:
            self.skipTest("Requires EIEM_BLENDER and EIEM_PHYSICS_PACKAGE")
        result = subprocess.run([blender,"--background","--factory-startup","--python-exit-code","1",
            "--python",str(ROOT/"tools/Blender/test_eiem_native_physics_package.py"),"--",
            str(ROOT/"tools/Blender/eiem_blender_addon.py"),package],
            capture_output=True,text=True,encoding="utf-8",errors="replace",timeout=120)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn("EFF_NORMAL_PACKAGE_PHYSICS_OK",result.stdout)


if __name__ == "__main__": unittest.main()
