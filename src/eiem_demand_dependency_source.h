#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace EiemDemandDependencySource {
// Resolve a referenced CAB within the request group and the direct native
// dependencies of bundles already opened for this request.
template<class NativeMap>
std::vector<uint64_t> CandidateKeys(const std::vector<uint64_t>& requestKeys,const NativeMap& native) {
  auto candidates=requestKeys;
  for(const auto& row:native)for(auto key:row.second)
    if(std::find(candidates.begin(),candidates.end(),key)==candidates.end())candidates.push_back(key);
  return candidates;
}
// The known-CAB map is a hint; the opened bundle is always checked for the
// actual CAB before returning it.
template<class CabMap,class KnownMap,class Open>
auto Resolve(const std::string& cab,const std::vector<uint64_t>& keys,CabMap& cabs,
    const KnownMap& known,Open open)->typename CabMap::iterator {
  auto found=cabs.find(cab);if(found!=cabs.end())return found;
  uint64_t indexedKey=0;bool indexedKeyOpened=false;
  auto indexed=known.find(cab);
  if(indexed!=known.end()&&std::find(keys.begin(),keys.end(),indexed->second)!=keys.end()){
    indexedKey=indexed->second;indexedKeyOpened=true;open(indexedKey);
    found=cabs.find(cab);if(found!=cabs.end())return found;
  }
  for(auto key:keys){if(indexedKeyOpened&&key==indexedKey)continue;open(key);found=cabs.find(cab);if(found!=cabs.end())return found;}
  return cabs.end();
}
} // namespace EiemDemandDependencySource
