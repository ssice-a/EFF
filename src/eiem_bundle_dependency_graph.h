#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace EiemBundleDependencies {
using Names=std::map<std::string,std::set<std::string>>;
using Keys=std::map<uint64_t,std::vector<uint64_t>>;

// Keep the game's list in its original order. Follow only new compiled
// edges and the game's native dependencies of those new bundles.
template<class Edges,class Native>
std::vector<uint64_t> Added(uint64_t root,const std::vector<uint64_t>& original,bool direct,Edges edges,Native native){
 // Most native queries contain no compiled edges. Avoid allocating one tree
 // node per native dependency until there is actually something to append.
 std::set<uint64_t> seen;bool initialized=false;
 std::vector<uint64_t> work{root},added;if(!direct)work.insert(work.end(),original.begin(),original.end());
 for(size_t i=0;i<work.size();++i){
  for(auto dependency:edges(work[i])){
   if(!initialized){seen.insert(original.begin(),original.end());seen.insert(root);initialized=true;}
   if(!seen.insert(dependency).second)continue;added.push_back(dependency);
   if(direct)continue;work.push_back(dependency);
   for(auto child:native(dependency))if(seen.insert(child).second){added.push_back(child);work.push_back(child);}
  }
 }
 return added;
}
} // namespace EiemBundleDependencies
