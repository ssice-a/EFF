"""Blender-generated mod is evaluated by the actual DLL INI compiler."""
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
static unsigned mask(const EiemModProgram &p) {
  return p.rules[p.standaloneRules[0]].hiddenSubmeshMask;
}
static float shape(const EiemModProgram &p) {
  for (const auto &r : p.rules)
    for (unsigned i = 0; i < r.shapeCount; ++i)
      if (EiemModEquals(r.shapeNames[i], "StockingBlend"))
        return r.shapeWeights[i];
  return -1;
}
int main(int argc, char **argv) {
  CHECK(argc == 3);
  EiemModProgram p;
  std::string error;
  CHECK(EiemModParseFile(argv[1], p, &error));
  EiemCompileModProgram(p);
  CHECK(p.standaloneRules.size() == 1);
  const auto &r = p.rules[p.standaloneRules[0]];
  CHECK(r.hasMesh && EiemModEquals(r.mesh, "MeshSource_MERGED"));
  CHECK(!EiemModEquals(r.handling, "skip"));
  CHECK(mask(p) == (1u << 3)); // default accessory + top, variant hidden
  EiemKeyChord top, accessory, increase, decrease;
  CHECK(EiemParseKeyChord("Ctrl+Alt+Numpad7", &top));
  CHECK(top.vk == VK_NUMPAD7 && top.modifiers == (MOD_CONTROL | MOD_ALT));
  CHECK(EiemParseKeyChord("F7", &accessory));
  CHECK(EiemParseKeyChord("F8", &increase) && EiemParseKeyChord("F9", &decrease));
  CHECK(shape(p) > .79f && shape(p) < .81f);
  for (int i = 0; i < 2; ++i) EiemApplyModKey(p, increase, false, nullptr, nullptr, true, .25);
  CHECK(shape(p) == 1);
  EiemApplyModKey(p, increase, false, nullptr, nullptr, true, .25);
  CHECK(shape(p) == 1); // direction never reverses
  for (int i = 0; i < 8; ++i) EiemApplyModKey(p, decrease, false, nullptr, nullptr, true, .25);
  CHECK(shape(p) == 0);
  EiemApplyModKey(p, decrease, false, nullptr, nullptr, true, .25);
  CHECK(shape(p) == 0);
  EiemCycleModKey(p, top); CHECK(mask(p) == 0); // variant submesh becomes visible
  EiemCycleModKey(p, accessory); CHECK(mask(p) == 1);
  EiemCycleModKey(p, top); CHECK(mask(p) == 13); // empty top style
  EiemCycleModKey(p, accessory); CHECK(mask(p) == 12);
  EiemCycleModKey(p, top); CHECK(mask(p) == 8);
  EiemModProgram off;
  CHECK(EiemModParseFile(argv[2], off, &error));
  EiemCompileModProgram(off);
  CHECK(mask(off) == 12);
  EiemCycleModKey(off, top); CHECK(mask(off) == 8);
  puts("EIEM_BLENDER_DLL_SWITCHES_OK");
}
'''


class BlenderSwitchTests(unittest.TestCase):
    def test_real_separate_save_reload_export_and_native_conditions(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        if not blender or not shutil.which("cl"):
            self.skipTest("Requires EIEM_BLENDER and MSVC developer environment")
        with tempfile.TemporaryDirectory(prefix="eiem-author-controls-") as directory:
            folder = Path(directory)
            result = subprocess.run([
                blender, "--background", "--factory-startup", "--python-exit-code", "1",
                "--python", str(ROOT / "tools/Blender/test_eiem_switches.py"), "--",
                str(ROOT / "tools/Blender/eiem_blender_addon.py"), str(folder),
            ], capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("EIEM_SWITCHES_OK", result.stdout)
            source = folder / "native.cpp"
            source.write_text(SOURCE, encoding="utf-8")
            exe = folder / "native.exe"
            build = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                    f"/I{ROOT / 'src'}", str(source), f"/Fe{exe}"],
                                   cwd=folder, capture_output=True, text=True, encoding="utf-8", errors="replace")
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            result = subprocess.run([str(exe), str(folder / "package/mod.ini"),
                                     str(folder / "default-off/mod.ini")],
                                    capture_output=True, text=True, encoding="utf-8", errors="replace")
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
