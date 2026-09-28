"""Stable path helpers retained by Skeleton authoring/runtime validation."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
SOURCE=r'''
#include "eiem_skin_binding.h"
#include <cassert>
int main() {
  std::string error; std::vector<size_t> slots;
  assert(EiemSkinPathSuffix("Scene/ActorA/Root/Chest", "Root/Chest"));
  assert(!EiemSkinPathSuffix("Scene/ActorA/OtherRoot/Chest", "Root/Chest"));
  const std::vector<std::string> payload={"Root/Chest","Root/Unused","Root/Pelvis","Root/Pelvis/Foot"};
  assert(EiemResolveSkinPathIndices(payload,{"Root/Pelvis/Foot","Root/Unused","Root/Pelvis","Root/Chest"},slots,error));
  assert((slots==std::vector<size_t>{3,1,2,0}));
  assert(!EiemResolveSkinPathIndices(payload,{"Root/Chest","Root/Unused"},slots,error) && slots.empty());
  assert(!EiemResolveSkinPathIndices({"Root/Chest"},{"Root/Chest","Root/Chest"},slots,error));
}
'''

class SkinBindingTests(unittest.TestCase):
    def test_instance_scoped_full_skeleton(self):
        if not shutil.which('cl'): self.skipTest('Requires MSVC')
        with tempfile.TemporaryDirectory(prefix='eiem-skin-binding-') as directory:
            folder=Path(directory); cpp=folder/'test.cpp'; cpp.write_text(SOURCE,encoding='utf-8')
            exe=folder/'test.exe'
            build=subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/utf-8',f'/I{ROOT / "src"}',str(cpp),f'/Fe{exe}'],cwd=folder,capture_output=True,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(build.returncode,0,(build.stdout or '')+(build.stderr or ''))
            self.assertEqual(subprocess.run([str(exe)]).returncode,0)

if __name__=='__main__': unittest.main()
