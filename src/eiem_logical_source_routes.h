#pragma once
#include "eiem_static_resource_sources.h"
#include "eiem_vfs_catalog.h"

namespace EiemLogicalSourceRoutes {
using namespace EiemNativeInput;
using Source=EiemResourceReloadPlan::Source;
using Routes=std::map<std::string,std::string>;
inline void Include(Routes& routes,const EiemStaticSources::Selection& selection){
 for(const auto& row:selection.inputs){auto file=EiemStaticSources::SourceFile(row.first,row.second.revision);auto found=routes.emplace(file,row.first);
  Require(found.second||found.first->second==row.first,"compiled source has conflicting logical paths");}
}
inline void Rebind(std::vector<Source>& sources,const Routes& routes,const std::filesystem::path& game){
 std::set<std::string> requested;
 for(const auto& source:sources){auto found=routes.find(source.file);Require(found!=routes.end(),"compiled source has no logical route; repair the Mod: "+source.file);
  Require(EiemStaticSources::SourceFile(found->second,source.hash)==source.file,"compiled source logical identity differs");requested.insert(found->second);}
 if(requested.empty())return;
 // Read the current two-layer catalog once for the complete set. No whole-game
 // asset index, Unity query, payload decoder or compilation runs here.
 auto current=EiemVfsCatalog::ReadEffectiveVfs(game,requested);
 for(auto& source:sources){const auto& logical=routes.at(source.file);const auto& entry=current.at(logical);
  Require(!entry.encrypted,"outer-encrypted runtime source unsupported: "+logical);
  Require(entry.size==source.size,"game source content changed; repair the Mod offline: "+logical);
  source.physical=entry.physical;source.offset=entry.offset;
 }
}
} // namespace EiemLogicalSourceRoutes
