#pragma once
#include "../../src/eiem_resource_directory.h"

namespace EiemModAnimationExperiment {
// Match the selected installation, never assume the author's reusable ID is
// the runtime key. Copies of one author must remain independently controllable.
inline const EiemDirectory::Package& Installed(const std::vector<EiemDirectory::Package>& graph,
 const std::filesystem::path& root,const std::string& author){
 const EiemDirectory::Package* found=nullptr;
 for(const auto& p:graph)if(EiemDirectory::DeclaredId(p)==author&&std::filesystem::equivalent(p.root,root)){
  EiemDirectory::Require(!found,"ambiguous Mod experiment installation");found=&p;
 }
 EiemDirectory::Require(found!=nullptr,"Mod experiment installation not registered");return *found;
}
}
