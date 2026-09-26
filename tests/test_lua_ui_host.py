"""Create/destroy the production DirectComposition host without showing any UI."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_lua_ui import ROOT

SOURCE = r'''
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
static int g_guiToggleVK=VK_INSERT, g_modReloadVK=VK_F10;
static HWND g_gameHwnd=nullptr,g_guiHwnd=nullptr,g_modUiHwnd=nullptr;
static bool g_guiVisible=false,g_modUiVisible=false,g_pluginActive=true;
static bool g_guiRunning=true,g_shutdownRequested=false;
static void Log(const char *,...) {}
static void ReleaseCursorToGui() {}
static void ReturnCursorToGame() {}
#include "eiem_config.h"
#include "eiem_mods.h"
static void EiemQueueModInput(EiemModInputEvent) {}
// External update service is inert in this window-lifecycle fixture.
#define EIEM_VERSION "test"
struct UpdateSnapshot { bool available=false, checking=false, failed=false, latest=false; std::string version,url; };
static UpdateSnapshot EiemGetUpdateSnapshot() { return {}; }
static bool EiemRequestUpdateCheck(bool=false) { return false; }
static bool EiemIgnoreRelease(const std::string &) { return true; }
#include "eiem_ui_host.h"
int main() {
  auto *mainContext=ImGui::CreateContext();
  EiemUiHost host;
  if(!host.InitializeHidden()) return 2;
  if(!g_modUiHwnd || IsWindowVisible(g_modUiHwnd) || g_modUiVisible) return 3;
  if(ImGui::GetCurrentContext()!=mainContext) return 4;
  if(ImGui::GetIO().BackendRendererUserData || ImGui::GetIO().BackendPlatformUserData) return 7;
  // Reload while closed recreates only the Mod context. No window is shown.
  ++s_eiemModGeneration; host.Tick();
  ++s_eiemModGeneration; host.Tick();
  if(IsWindowVisible(g_modUiHwnd) || ImGui::GetCurrentContext()!=mainContext) return 8;
  if(ImGui::GetIO().BackendRendererUserData || ImGui::GetIO().BackendPlatformUserData) return 9;
  HWND old=g_modUiHwnd;
  host.Shutdown();
  if(IsWindow(old) || g_modUiHwnd || ImGui::GetCurrentContext()!=mainContext) return 5;
  if(!host.InitializeHidden()) return 6;
  host.Shutdown(); host.Shutdown();
  ImGui::DestroyContext(mainContext);
  return 0;
}
'''


class LuaUiHostTests(unittest.TestCase):
    def test_hidden_native_host_lifecycle(self):
        if not shutil.which("cl") or not (ROOT / "bin/lua.lib").exists():
            self.skipTest("Requires build.bat and MSVC")
        with tempfile.TemporaryDirectory(prefix="eiem-ui-host-") as temp:
            folder = Path(temp)
            cpp = folder / "host.cpp"
            cpp.write_text(SOURCE, encoding="utf-8")
            exe = folder / "host.exe"
            cmd = ["cl", "/nologo", "/MD", "/EHsc", "/std:c++17", "/utf-8",
                   f"/I{ROOT / 'src'}", f"/I{ROOT / 'deps/imgui'}", f"/I{ROOT / 'deps/lua'}",
                   str(cpp), *(str(ROOT / 'deps/imgui' / name) for name in (
                       'imgui.cpp', 'imgui_draw.cpp', 'imgui_tables.cpp', 'imgui_widgets.cpp',
                       'imgui_impl_win32.cpp', 'imgui_impl_dx11.cpp')),
                   str(ROOT / 'bin/lua.lib'), f"/Fe{exe}", "user32.lib", "gdi32.lib",
                   "imm32.lib", "dwmapi.lib", "d3d11.lib", "dcomp.lib", "dxgi.lib"]
            build = subprocess.run(cmd, cwd=folder, capture_output=True, text=True,
                                   encoding="utf-8", errors="replace", timeout=120)
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(exe)], cwd=folder, capture_output=True, text=True,
                                 encoding="utf-8", errors="replace", timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)


if __name__ == '__main__':
    unittest.main()
