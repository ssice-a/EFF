#pragma once

#include <filesystem>
#include <fstream>
#include <string>

// Strict Mod-local file syntax, independent of either author INI dialect.
// Canonical directory containment still needs to be checked by the caller.
inline bool EiemRelativeFilePath(const std::string& value) {
  if (value.empty() || value.front() == '/' || value.front() == '\\' ||
      value.find(':') != std::string::npos || value.find('\0') != std::string::npos)
    return false;
  size_t begin = 0;
  while (begin < value.size()) {
    const auto end = value.find_first_of("/\\", begin);
    const auto part = value.substr(begin, end == std::string::npos ? end : end - begin);
    if (part.empty() || part == "." || part == "..") return false;
    if (end == std::string::npos) return true;
    begin = end + 1;
  }
  return false;
}

// Mod strings are UTF-8. Convert at the disk boundary so the narrow CRT never
// interprets a filename in the system code page.
static std::ifstream EiemOpenUtf8Input(
    const char *path, std::ios::openmode mode = std::ios::binary) {
  std::ifstream input;
  if (path && path[0]) {
    try {
      input.open(std::filesystem::u8path(path), mode);
    } catch (const std::exception &) {
      input.setstate(std::ios::failbit);
    }
  } else {
    input.setstate(std::ios::failbit);
  }
  return input;
}
