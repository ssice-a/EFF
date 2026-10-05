#pragma once
#include "eiem_file_io.h"
#include "eiem_keys.h"
#include <fstream>
#include <cstdio>
#include <unordered_set>

#include "eiem_global_settings.h"

static SRWLOCK s_eiemGlobalConfigLock = SRWLOCK_INIT;
static EiemGlobalConfig s_eiemGlobalConfig;
static volatile LONG s_eiemGlobalConfigGeneration = 0;

static EiemGlobalConfig EiemGetGlobalConfig() {
  AcquireSRWLockShared(&s_eiemGlobalConfigLock);
  EiemGlobalConfig result = s_eiemGlobalConfig;
  ReleaseSRWLockShared(&s_eiemGlobalConfigLock);
  return result;
}

static bool LoadEiemConfig() {
  const auto configPath = std::filesystem::u8path(kEiemGlobalConfigPath);
  if (GetFileAttributesW(configPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
    CreateDirectoryW(L"plugin", nullptr);
    HANDLE file = CreateFileW(configPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      DWORD written = 0;
      const DWORD size = (DWORD)std::char_traits<char>::length(kEiemDefaultGlobalConfig);
      if (!WriteFile(file, kEiemDefaultGlobalConfig, size, &written, nullptr) || written != size)
        Log("[CONFIG] Could not write complete default %s", kEiemGlobalConfigPath);
      CloseHandle(file);
    }
  }
  auto input = EiemOpenUtf8Input(kEiemGlobalConfigPath);
  EiemGlobalConfig next;
  std::string error;
  if (!input || !EiemParseGlobalConfig(input, &next, error)) {
    // Invalid edits must not remove the working reload key. No alternative file
    // is searched; the explicitly logged last binding remains until fixed.
    Log("[CONFIG] %s:%s; settings unchanged", kEiemGlobalConfigPath,
        error.empty() ? "Unable to read" : error.c_str());
    return false;
  }
  AcquireSRWLockExclusive(&s_eiemGlobalConfigLock);
  s_eiemGlobalConfig = next;
  g_guiToggleVK = (int)next.gui.vk;
  g_modReloadVK = (int)next.reload.vk;
  InterlockedIncrement(&s_eiemGlobalConfigGeneration);
  ReleaseSRWLockExclusive(&s_eiemGlobalConfigLock);
  Log("[CONFIG] Loaded %s reload_vk=%u modifiers=%u disable_camera_fade=%d",
      kEiemGlobalConfigPath, next.reload.vk, next.reload.modifiers, next.disableCameraFade);
  return true;
}
