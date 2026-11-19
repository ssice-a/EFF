#pragma once
#include <cstddef>
#include <string>
#include <string_view>

namespace EiemPhysicsComponentIdentity {
constexpr char Prefix[] = "__EFF_";

inline std::string Name(const std::string& package, const std::string& component) {
  return Prefix + package + "_" + component;
}

inline bool IsGeneratedName(std::string_view name) {
  constexpr size_t prefixSize = sizeof(Prefix) - 1;
  if (name.substr(0, prefixSize) != Prefix) return false;

  // A composed component can have a Mod-instance prefix before its author ID.
  const auto separator = name.rfind('_');
  if (separator == std::string_view::npos || separator <= prefixSize ||
      name.size() - separator - 1 != 32) return false;
  for (size_t i = separator + 1; i < name.size(); ++i) {
    const char c = name[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) return false;
  }
  return true;
}

}  // namespace EiemPhysicsComponentIdentity
