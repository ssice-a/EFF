#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace EiemDemandDependencySource {
struct IncompleteSources : std::runtime_error {using std::runtime_error::runtime_error;};

// Resolve a referenced CAB within the request group and the direct native
// dependencies of bundles already opened for this request.
template<class NativeMap>
std::vector<uint64_t> CandidateKeys(const std::vector<uint64_t>& requestKeys,const NativeMap& native) {
  auto candidates=requestKeys;
  std::set<uint64_t> seen(requestKeys.begin(),requestKeys.end());
  for(const auto& row:native)for(auto key:row.second)
    if(seen.insert(key).second)candidates.push_back(key);
  return candidates;
}
// The known-CAB map is a hint; the opened bundle is always checked for the
// actual CAB before returning it.
template<class CabMap,class KnownMap,class Preferred,class Open>
auto Resolve(const std::string& cab,const std::vector<uint64_t>& keys,CabMap& cabs,
    const KnownMap& known,const std::set<uint64_t>& inspected,Preferred preferred,Open open)->typename CabMap::iterator {
  auto found=cabs.find(cab);if(found!=cabs.end())return found;
  std::set<uint64_t> tried;
  std::optional<std::string> firstFailure;
  auto probe=[&](uint64_t key){
    if(!tried.insert(key).second)return cabs.end();
    try{open(key);}catch(const std::runtime_error& error){
      if(!firstFailure)firstFailure=error.what();
      return cabs.end();
    }
    return cabs.find(cab);
  };
  auto indexed=known.find(cab);
  if(indexed!=known.end()&&std::find(keys.begin(),keys.end(),indexed->second)!=keys.end()){
    if((found=probe(indexed->second))!=cabs.end())return found;
  }
  for(auto key:preferred())if(!inspected.count(key)&&std::find(keys.begin(),keys.end(),key)!=keys.end()){
    if((found=probe(key))!=cabs.end())return found;
  }
  for(auto key:keys)if(!inspected.count(key)&&(found=probe(key))!=cabs.end())return found;
  // An unreadable candidate leaves the search incomplete and retryable.
  if(firstFailure)throw IncompleteSources("could not verify all candidates for "+cab+": "+*firstFailure);
  return cabs.end();
}
} // namespace EiemDemandDependencySource
