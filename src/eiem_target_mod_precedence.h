#pragma once
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace EiemTargetModPrecedence {
using Identity = std::pair<std::string,std::string>;
struct Claim { Identity target; std::string mod; };
struct Selection {
  std::map<Identity,std::string> winner;
  std::map<Identity,std::vector<std::string>> claimants;
  size_t overlappingTargets=0;
};
inline Selection Resolve(const std::vector<Claim>& claims) {
  std::map<Identity,std::set<std::string>> mods;
  for(const auto& claim:claims) {
    if(claim.target.first.empty()||claim.target.second.empty()||claim.mod.empty())
      throw std::runtime_error("target precedence claim is incomplete");
    if(!mods[claim.target].insert(claim.mod).second)
      throw std::runtime_error("one Mod declares the same Mesh target more than once");
  }
  Selection result;
  for(const auto& row:mods) {
    result.winner.emplace(row.first,*row.second.rbegin());
    result.claimants.emplace(row.first,std::vector<std::string>(row.second.begin(),row.second.end()));
    result.overlappingTargets+=row.second.size()>1;
  }
  return result;
}
} // namespace EiemTargetModPrecedence
