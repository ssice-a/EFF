"""Blender shape data and auto-generated bindings consumed by production C++."""
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
int main(int argc,char **argv) {
  if(argc!=3) return 1;
  for(int i=1;i<3;++i) {
    EiemModProgram p; std::string error;
    if(!EiemModParseFile(argv[i],p,&error)) { puts(error.c_str()); return 2; }
    if(p.states[0].uis.size()!=0) return 3;
    unsigned shapes=0;
    for(const auto &r:p.rules) if(r.shapeCount) {
      if(r.shapeCount!=1 || std::string(r.shapeNames[0])!="Inflate" || r.shapeWeights[0]!=.25f || !r.hasMesh) return 4;
      ++shapes;
    }
    if(shapes!=1) return 5;
  }
  return 0;
}
'''


class BlenderShapeTests(unittest.TestCase):
    def test_saved_authoring_export_and_dll_parser(self):
        blender = os.environ.get("EIEM_BLENDER") or shutil.which("blender")
        if not blender or not shutil.which("cl"):
            self.skipTest("Requires EIEM_BLENDER and MSVC")
        with tempfile.TemporaryDirectory(prefix="eiem-blender-shapes-") as directory:
            folder = Path(directory)
            run = subprocess.run([blender, "--background", "--factory-startup", "--python-exit-code", "1",
                                  "--python", str(ROOT / "tools/Blender/test_eiem_shapes.py"), "--",
                                  str(ROOT / "tools/Blender/eiem_blender_addon.py"), str(folder)],
                                 capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=90)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("EIEM_SHAPES_OK", run.stdout)

            source = folder / "test.cpp"
            source.write_text(SOURCE, encoding="utf-8")
            exe = folder / "test.exe"
            build = subprocess.run(["cl", "/nologo", "/EHsc", "/std:c++17", "/utf-8",
                                    f"/I{ROOT / 'src'}", str(source), f"/Fe{exe}"], cwd=folder,
                                   capture_output=True, text=True, encoding="utf-8", errors="replace")
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe), str(folder / "package/mod.ini"), str(folder / "always-ui/mod.ini")],
                                 capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=15)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == "__main__": unittest.main()
