#pragma once
#include "eiem_file_io.h"
#include "eiem_keys.h"
#include <fstream>
#include <cstdio>
#include <unordered_set>

struct EiemGlobalConfig {
  EiemKeyChord reload{VK_F10, 0};
  EiemKeyChord gui{VK_INSERT, 0};
  bool disableCameraFade = true;
};

static constexpr const char *kEiemGlobalConfigPath = "plugin\\eff.ini";
static constexpr const char *kEiemDefaultGlobalConfig =
    "; EFF global settings. Mod controls belong in mods/<name>/mod.ini.\n"
    "; After editing, press the reload shortcut once to load the new binding.\n"
    "[Hotkeys]\n"
    "reload=F10\n"
    "gui=INSERT\n"
    "\n[Graphics]\n"
    "; Disable CameraMono camera fade; Mod mesh/skip state is unchanged.\n"
    "disable_camera_fade=true\n";

// Parse separately from publication/registration so the same API is testable.
static bool EiemParseGlobalConfig(std::istream &input, EiemGlobalConfig *out,
                                  std::string &error) {
  EiemGlobalConfig next;
  std::string line;
  size_t lineNumber = 0;
  std::string section;
  std::unordered_set<std::string> sections;
  std::unordered_set<std::string> seen;
  auto fail = [&](const std::string &message) {
    error = std::to_string(lineNumber) + ": " + message; return false;
  };
  while (std::getline(input, line)) {
    ++lineNumber;
    if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
    EiemKeyTrim(line);
    if (line.empty() || line[0] == ';' || line[0] == '#') continue;
    if (line.front() == '[' && line.back() == ']') {
      if (!_stricmp(line.c_str(), "[Hotkeys]")) section = "hotkeys";
      else if (!_stricmp(line.c_str(), "[Graphics]")) section = "graphics";
      else return fail("Unknown section: " + line);
      if (!sections.insert(section).second) return fail("Duplicate section: " + line);
      continue;
    }
    size_t equals = line.find('=');
    if (section.empty() || equals == std::string::npos) return fail("Expected section and key=value");
    std::string key = line.substr(0, equals), value = line.substr(equals + 1);
    EiemKeyTrim(key); EiemKeyTrim(value);
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (!seen.insert(section + "." + key).second) return fail("Duplicate key: " + key);
    if (section == "graphics") {
      if (key != "disable_camera_fade") return fail("Unknown setting: " + key);
      if (!_stricmp(value.c_str(), "true")) next.disableCameraFade = true;
      else if (!_stricmp(value.c_str(), "false")) next.disableCameraFade = false;
      else return fail("disable_camera_fade expects true or false");
      continue;
    }
    EiemKeyChord chord;
    if (!EiemParseKeyChord(value, &chord)) return fail("Invalid shortcut: " + value);
    if (key == "reload") next.reload = chord;
    else if (key == "gui") next.gui = chord;
    else return fail("Unknown setting: " + key);
  }
  if (input.bad()) return fail("Read failed");
  if (next.reload == next.gui) return fail("reload and gui must use different shortcuts");
  *out = next;
  return true;
}

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
