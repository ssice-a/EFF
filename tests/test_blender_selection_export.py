"""Selected-only author export and camera-off skip, with the actual INI parser."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#include "eiem_mod_document.h"
#include <cstdio>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); return 1; } } while (false)
int main(int argc, char **argv) {
  CHECK(argc == 3);
  EiemModProgram hidden, split;
  std::string error;
  CHECK(EiemModParseFile(argv[1], hidden, &error));
  EiemCompileModProgram(hidden);
  CHECK(hidden.standaloneRules.size() == 1);
  const auto &r = hidden.rules[hidden.standaloneRules[0]];
  CHECK(EiemModEquals(r.handling, "skip") && !r.hasMesh);
  CHECK(EiemModParseFile(argv[2], split, &error));
  EiemCompileModProgram(split);
  CHECK(split.standaloneRules.size() == 1);
  const auto &s = split.rules[split.standaloneRules[0]];
  CHECK(!EiemModEquals(s.handling, "skip") && s.hasMesh);
  CHECK(EiemModEquals(s.mesh, "MeshPartA"));
  puts("EIEM_SELECTION_DLL_OK");
}
'''


class BlenderSelectionExportTests(unittest.TestCase):
    def test_selection_camera_and_native_rules(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        if not blender:
            self.skipTest("Requires EIEM_BLENDER")
        with tempfile.TemporaryDirectory(prefix="eiem-selection-export-") as directory:
            folder = Path(directory)
            result = subprocess.run([
                blender, "--background", "--factory-startup", "--python-exit-code", "1",
                "--python", str(ROOT / "tools/Blender/test_eiem_selection_export.py"), "--",
                str(ROOT / "tools/Blender/eiem_blender_addon.py"), str(folder),
            ], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("EIEM_SELECTION_EXPORT_OK", result.stdout)
            with self.subTest("native DLL parser"):
                if not shutil.which("cl"):
                    self.skipTest("Native parser check requires MSVC")
                source = folder / "native.cpp"
                source.write_text(SOURCE, encoding="utf-8")
                exe = folder / "native.exe"
                build = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                        f"/I{ROOT / 'src'}", str(source), f"/Fe{exe}"],
                                       cwd=folder, capture_output=True, text=True, encoding="utf-8", errors="replace")
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
                result = subprocess.run([str(exe), str(folder / "hide-only/mod.ini"),
                                         str(folder / "split/mod.ini")],
                                        capture_output=True, text=True, encoding="utf-8", errors="replace")
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("EIEM_SELECTION_DLL_OK", result.stdout)


if __name__ == "__main__":
    unittest.main()
