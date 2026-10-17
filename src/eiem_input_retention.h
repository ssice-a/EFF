#pragma once
#include "eiem_resource_directory.h"

// Retain edits and every input that can reach an edit. An unchanged consumer
// also needs isolation when one of its dependencies receives a new CAB name.
// Terminal read-only dependencies keep their original game-owned identity.
namespace EiemInputRetention {
inline std::vector<unsigned char> Select(const std::vector<std::vector<size_t>>& dependencies,
 const std::vector<unsigned char>& changed,const std::vector<size_t>& roots){
 using EiemDirectory::Require;
 Require(dependencies.size()==changed.size(),"input retention coverage differs");
 std::vector<std::vector<size_t>> consumers(dependencies.size());
 for(size_t i=0;i<dependencies.size();++i)for(auto dependency:dependencies[i]){
  Require(dependency<dependencies.size(),"input retention dependency absent");consumers[dependency].push_back(i);
 }
 auto retained=changed;std::vector<size_t> queue;
 for(size_t i=0;i<changed.size();++i)if(changed[i])queue.push_back(i);
 // A retained root can itself be an external dependency of another input.
 for(auto root:roots){Require(root<changed.size(),"input retention root absent");if(!retained[root]){retained[root]=1;queue.push_back(root);}}
 while(!queue.empty()){auto next=queue.back();queue.pop_back();for(auto consumer:consumers[next])if(!retained[consumer]){retained[consumer]=1;queue.push_back(consumer);}}
 return retained;
}
} // namespace EiemInputRetention
