#pragma once
#include "eiem_native_input.h"

namespace EiemNativeInput {
struct InputFileStamp {
  uint64_t bytes = 0, written = 0;
};

inline InputFileStamp FileStamp(const std::filesystem::path& path) {
  WIN32_FILE_ATTRIBUTE_DATA attributes{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) {
    auto error = GetLastError();
    throw std::runtime_error("input file stamp unavailable: " + path.u8string() +
                             " win32=" + std::to_string(error));
  }
  Require(!(attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY),
          "input file is a directory: " + path.u8string());
  return {(uint64_t(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow,
          (uint64_t(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
              attributes.ftLastWriteTime.dwLowDateTime};
}

inline bool SameStamp(const InputFileStamp& a, const InputFileStamp& b) {
  return a.bytes == b.bytes && a.written == b.written;
}

inline bool DigestText(const std::string& value) {
  return value.size() == 64 &&
         value.find_first_not_of("0123456789ABCDEFabcdef") == value.npos;
}
} // namespace EiemNativeInput
