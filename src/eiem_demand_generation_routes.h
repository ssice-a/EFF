#pragma once
#include <cstdint>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace EiemDemandGenerationRoutes {
inline std::string RootName(uint64_t generationScope, uint64_t sourceBundle) {
  std::ostringstream name;
  name << "eff/" << generationScope << "/root/" << std::uppercase << std::hex
       << std::setw(16) << std::setfill('0') << sourceBundle << ".ab";
  return name.str();
}

class RootAliases {
  mutable std::mutex mutex_;
  std::map<uint64_t, uint64_t> aliases_;
  std::map<uint64_t, uint64_t> sources_;

 public:
  void Bind(uint64_t sourceBundle, uint64_t alias) {
    if (!sourceBundle || !alias || sourceBundle == alias)
      throw std::runtime_error("generation root alias identity is invalid");
    std::lock_guard<std::mutex> lock(mutex_);
    auto bySource = aliases_.find(sourceBundle);
    if (bySource != aliases_.end() && bySource->second != alias)
      throw std::runtime_error("generation root source was rebound within one generation");
    auto byAlias = sources_.find(alias);
    if (byAlias != sources_.end() && byAlias->second != sourceBundle)
      throw std::runtime_error("generation root alias collides with another source");
    aliases_.emplace(sourceBundle, alias);
    sources_.emplace(alias, sourceBundle);
  }

  bool Alias(uint64_t sourceBundle, uint64_t& alias) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = aliases_.find(sourceBundle);
    if (found == aliases_.end()) return false;
    alias = found->second;
    return true;
  }

  bool Source(uint64_t alias, uint64_t& sourceBundle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = sources_.find(alias);
    if (found == sources_.end()) return false;
    sourceBundle = found->second;
    return true;
  }
};

class GenerationNamespace {
  uint64_t scope_;
  RootAliases roots_;

 public:
  explicit GenerationNamespace(uint64_t scope) : scope_(scope) {
    if (!scope_) throw std::runtime_error("resource generation scope is zero");
  }

  std::string ResourceName(const std::string& cabName) const {
    if (cabName.empty()) throw std::runtime_error("resource generation CAB name is empty");
    return "eff/" + std::to_string(scope_) + "/" + cabName + ".ab";
  }

  std::string RootName(uint64_t sourceBundle) const {
    return EiemDemandGenerationRoutes::RootName(scope_, sourceBundle);
  }

  void BindRoot(uint64_t sourceBundle, uint64_t alias) {
    roots_.Bind(sourceBundle, alias);
  }

  bool Alias(uint64_t sourceBundle, uint64_t& alias) const {
    return roots_.Alias(sourceBundle, alias);
  }

  bool Source(uint64_t alias, uint64_t& sourceBundle) const {
    return roots_.Source(alias, sourceBundle);
  }
};

template <class Roots, class Targets, class Resolve>
void IncludeTargetRoots(Roots& roots, const Targets& targets,
                        Resolve&& resolve) {
  for (const auto& target : targets) {
    const auto bundle = resolve(target.first, target.second);
    if (bundle) roots.insert(bundle);
  }
}

template <class Path, class Load>
bool StartAfterCacheInvalidation(Path& path, Load&& load) {
  if (!path.cacheInvalidated) return false;
  load(path);
  return true;
}
}  // namespace EiemDemandGenerationRoutes
