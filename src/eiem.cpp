#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <commdlg.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "vmd_parser.h"
#include "mmd_player.h"
#include "bone_anim_player.h"
#include "bone_map.h"
#include "il2cpp_api.h"
#include "il2cpp_dump.h"
#include "muscle_player.h"
#include "camera_player.h"


#include "globals.h"
#include "eiem_config.h"
#include "eiem_camera_fade.h"
#include "il2cpp_trace.h"
#include "eiem_visibility_forceoff_probe.h"
#include "scene_dump.h"
#include "eiem_native_physics_diagnostic.h"
#include "model_dump.h"
#include "update_check.h"

#include "audio.h"

#include "smc_face.h"

static int32_t UnboxInt(void *boxed) {
  __try {
    return boxed ? *(int32_t *)((char *)boxed + 16) : 0;
  } __except (1) {
    return 0;
  }
}
static bool UnboxBool(void *boxed) {
  __try {
    return boxed ? *(bool *)((char *)boxed + 16) : false;
  } __except (1) {
    return false;
  }
}

#include "animation.h"
#include "trojan.h"
#include "gui.h"
#include "init.h"

#define APPLEPIE_PLUGIN_IMPL
#include "applepie_mgr.h"

static AP_HotkeyInfo s_apHotkeys[] = {
    { "EIEM GUI", "gui", VK_INSERT },
    { "Reload EIEM mods", "reload", VK_F10 },
};

static AP_PluginInfo s_apPluginInfo = {
    APPLEPIE_PLUGIN_API_VERSION,
    "eiem",
    "EIEM",
    "Endfield MMD",
    "eiem.ini",
    true 
};

APPLEPIE_PLUGIN_EXPORT AP_PluginInfo* AP_GetPluginInfo() {
  return &s_apPluginInfo;
}

APPLEPIE_PLUGIN_EXPORT bool AP_PluginEnable() {
  g_pluginActive = true;
  EiemReportCameraFade();
  Log("[AP] Plugin enabled by manager");
  return true;
}

APPLEPIE_PLUGIN_EXPORT bool AP_PluginDisable() {
  g_pluginActive = false;
  EiemPhysicsStopTrace();
  EiemReportCameraFade();
  if (g_guiVisible) ToggleGui();
  Log("[AP] Plugin disabled by manager");
  return true;
}

APPLEPIE_PLUGIN_EXPORT bool AP_ReloadConfig() {
  EiemRequestModUpdate(EiemModUpdate::Reload, "ApplePie config reload");
  s_apHotkeys[0].currentVK = g_guiToggleVK;
  s_apHotkeys[1].currentVK = g_modReloadVK;
  Log("[AP] Global/mod config reload requested on Unity thread");
  return true;
}

APPLEPIE_PLUGIN_EXPORT int AP_GetHotkeys(AP_HotkeyInfo* outArray, int maxCount) {
  s_apHotkeys[0].currentVK = g_guiToggleVK;
  s_apHotkeys[1].currentVK = g_modReloadVK;
  int count = sizeof(s_apHotkeys) / sizeof(s_apHotkeys[0]);
  if (count > maxCount) count = maxCount;
  for (int i = 0; i < count; i++) outArray[i] = s_apHotkeys[i];
  return count;
}

APPLEPIE_PLUGIN_EXPORT void AP_SetLanguage(const char* langCode) {
  Log("[AP] Language set by manager: %s", langCode ? langCode : "null");
}

static HANDLE g_initThread = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(hModule);
    g_initThread = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
  }
  return TRUE;
}
