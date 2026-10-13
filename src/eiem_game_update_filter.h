#pragma once
#include "eiem_resource_catalog_delta.h"

// Three layers before any PFB bytes are read. The game seal is made from the
// actually mounted resource-directory content and dependency graph, independent
// of Mod files/targets. A complete offline export certifies its own target set
// against that game seal. Only old-baseline Mods invoke the relevant-candidate
// provider. The native adapter must run preparation outside the frame loop.
namespace EiemGameUpdateFilter {
using namespace EiemNativeInput;
using Stamp=EiemConsumerUpdateRecords::Stamp;
struct GameState {bool ready=false;std::string resources,dependencies;};
inline bool Same(const GameState& a,const GameState& b){return a.ready&&b.ready&&a.resources==b.resources&&a.dependencies==b.dependencies;}
inline void Validate(const GameState& state){Require(!state.ready||(DigestText(state.resources)&&DigestText(state.dependencies)),"game resource seal incomplete");}
struct Coverage {GameState game;std::string targets;bool complete=false;};
struct Relevant {
 std::map<std::string,Stamp> candidates;
 EiemConsumerUpdateRecords::Decisions completed;
 std::set<std::string> previousCandidates;
};
struct Work {
 bool waitForGame=false,usedOfflineCoverage=false,gameChanged=false;
 std::set<std::string> added,recheck,retired;
};
inline Work Select(const GameState& current,const Coverage& exported,const std::string& exportedTargets,
 const std::function<Relevant()>& relevant){
 Validate(current);Validate(exported.game);Require(exported.game.ready&&exported.complete&&DigestText(exported.targets)&&exported.targets==exportedTargets,"Mod lacks complete offline coverage of its declared target set");
 Work work;if(!current.ready){work.waitForGame=true;return work;}
 // Layer 1: an export made from this exact game state is authoritative even
 // when it targets a different Mesh from the previously installed Mod.
 if(Same(current,exported.game)){work.usedOfflineCoverage=true;return work;}
 work.gameChanged=true;Require(bool(relevant),"changed-game relevant-candidate provider absent");auto scoped=relevant();
 // Layer 2: provider uses changed game records + target reverse dependencies.
 // Layer 3: valid positive AND negative decisions are excluded equally.
 for(const auto& candidate:scoped.candidates){EiemConsumerUpdateRecords::Validate(candidate.second);auto old=scoped.completed.find(candidate.first);
  if(old!=scoped.completed.end()&&EiemConsumerUpdateRecords::Valid(old->second,candidate.second))continue;
  if(scoped.previousCandidates.count(candidate.first))work.recheck.insert(candidate.first);else work.added.insert(candidate.first);
 }
 for(const auto& source:scoped.previousCandidates)if(!scoped.candidates.count(source))work.retired.insert(source);
 return work;
}
} // namespace EiemGameUpdateFilter
