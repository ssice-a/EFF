#pragma once
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <filesystem>
#include <stdexcept>
#include <windows.h>

namespace EiemDemandGenerationDiff {
struct Target {
  std::string rule;
  unsigned lod = 0;
  std::string path;
  std::string asset;
  std::set<std::string> parts;
  bool skip = false;

  bool operator<(const Target& other) const {
    return std::tie(rule, lod, path, asset, parts, skip) <
           std::tie(other.rule, other.lod, other.path, other.asset,
                    other.parts, other.skip);
  }
  bool operator==(const Target& other) const {
    return rule == other.rule && lod == other.lod && path == other.path &&
           asset == other.asset && parts == other.parts && skip == other.skip;
  }
};

struct Mod {
  std::string root;
  std::map<std::string, std::string> files;
  std::set<Target> targets;
  std::string resourceDeclarations;

  bool operator==(const Mod& other) const {
    return root == other.root && files == other.files && resourceDeclarations==other.resourceDeclarations &&
           targets == other.targets;
  }
};

using Snapshot = std::map<std::string, Mod>;

inline std::string FileFingerprint(const std::filesystem::path& path) {
  HANDLE file = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    throw std::runtime_error("cannot fingerprint declared resource file: " + path.u8string());
  BY_HANDLE_FILE_INFORMATION info{};
  bool read = GetFileInformationByHandle(file, &info) != 0;
  CloseHandle(file);
  if (!read || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    throw std::runtime_error("declared resource is not a regular file: " + path.u8string());
  auto number = [](DWORD high, DWORD low) {
    return std::to_string((uint64_t(high) << 32) | low);
  };
  return number(info.nFileSizeHigh, info.nFileSizeLow) + "/" +
      number(info.ftLastWriteTime.dwHighDateTime, info.ftLastWriteTime.dwLowDateTime) + "/" +
      std::to_string(info.dwVolumeSerialNumber) + "/" +
      number(info.nFileIndexHigh, info.nFileIndexLow);
}

struct Delta {
  bool changed = false;
  bool targetDeclarationsChanged = false;
  std::set<std::string> packages;
  std::set<std::string> addedPackages;
  std::set<std::string> removedPackages;
  std::set<std::string> modifiedPackages;
  std::set<std::string> controlOnlyPackages;
  std::set<std::pair<std::string, std::string>> targets;
};

inline void AddTargets(Delta& delta, const Mod& mod) {
  for (const auto& target : mod.targets)
    delta.targets.emplace(target.path, target.asset);
}

inline Delta Compare(const Snapshot& previous, const Snapshot& next) {
  Delta delta;
  auto oldMod = previous.begin();
  auto newMod = next.begin();
  while (oldMod != previous.end() || newMod != next.end()) {
    if (newMod == next.end() ||
        (oldMod != previous.end() && oldMod->first < newMod->first)) {
      delta.changed = true;
      delta.packages.insert(oldMod->first);
      delta.removedPackages.insert(oldMod->first);
      delta.targetDeclarationsChanged = delta.targetDeclarationsChanged ||
          !oldMod->second.targets.empty();
      AddTargets(delta, oldMod->second);
      ++oldMod;
      continue;
    }
    if (oldMod == previous.end() || newMod->first < oldMod->first) {
      delta.changed = true;
      delta.packages.insert(newMod->first);
      delta.addedPackages.insert(newMod->first);
      delta.targetDeclarationsChanged = delta.targetDeclarationsChanged ||
          !newMod->second.targets.empty();
      AddTargets(delta, newMod->second);
      ++newMod;
      continue;
    }
    if (!(oldMod->second == newMod->second)) {
      delta.changed = true;
      delta.packages.insert(oldMod->first);
      delta.modifiedPackages.insert(oldMod->first);
      auto oldFiles=oldMod->second.files,newFiles=newMod->second.files;oldFiles.erase("mod.ini");newFiles.erase("mod.ini");
      if(!oldMod->second.resourceDeclarations.empty()&&oldMod->second.root==newMod->second.root&&oldFiles==newFiles&&
         oldMod->second.targets==newMod->second.targets&&oldMod->second.resourceDeclarations==newMod->second.resourceDeclarations)
        delta.controlOnlyPackages.insert(oldMod->first);
      if (oldMod->second.targets != newMod->second.targets) {
        delta.targetDeclarationsChanged = true;
        AddTargets(delta, oldMod->second);
        AddTargets(delta, newMod->second);
      }
    }
    ++oldMod;
    ++newMod;
  }
  return delta;
}
}  // namespace EiemDemandGenerationDiff
