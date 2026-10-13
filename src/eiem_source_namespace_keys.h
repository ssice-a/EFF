#pragma once
#include "eiem_offline_resource_package.h"

// A generation still has one transaction key. Its native source namespaces
// depend only on that source's reachable input contents and contributors, so
// adding an unrelated Prefab/Mod cannot rename every already prepared CAB.
namespace EiemSourceNamespaceKeys {
using namespace EiemNativeInput;
struct Source {std::string identity,content;std::set<std::string> contributors;std::vector<size_t> dependencies;};
inline std::vector<std::string> Build(const std::vector<Source>& sources){
 std::set<std::string> identities;for(const auto& source:sources)Require(!source.identity.empty()&&DigestText(source.content)&&identities.insert(source.identity).second,"namespace source identity incomplete/duplicated");
 std::vector<std::string> keys;keys.reserve(sources.size());
 for(size_t i=0;i<sources.size();++i){std::set<size_t> visited;std::vector<size_t> pending{i};std::map<std::string,size_t> closure;
  while(!pending.empty()){auto at=pending.back();pending.pop_back();Require(at<sources.size(),"namespace source dependency absent");if(!visited.insert(at).second)continue;closure.emplace(sources[at].identity,at);
   pending.insert(pending.end(),sources[at].dependencies.begin(),sources[at].dependencies.end());}
  EiemOfflinePackage::Writer w;w.S("EFF_SOURCE_NAMESPACE/source+reachable-content+contributors");w.S(sources[i].identity);
  for(const auto& row:closure){const auto& source=sources[row.second];w.S(source.identity);w.S(source.content);w.Set(source.contributors);
   // Preserve graph topology as well as the reachable content set.
   std::set<std::string> edges;for(auto dependency:source.dependencies){Require(dependency<sources.size(),"namespace source dependency absent");edges.insert(sources[dependency].identity);}w.Set(edges);
  }auto hash=Sha(w.b.data(),w.b.size());keys.push_back(Hex(hash.data(),hash.size()));
 }return keys;
}
} // namespace EiemSourceNamespaceKeys
