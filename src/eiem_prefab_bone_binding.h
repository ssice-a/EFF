#pragma once
#include "eiem_resource_directory.h"

// Resolves author paths inside one concrete Prefab skeleton. Source palette
// order is irrelevant; no bone name is matched across another model scope.
namespace EiemPrefabBoneBinding {
using EiemDirectory::Require;
struct Node {size_t parent=SIZE_MAX;std::string name;};
class Tree {
 const std::vector<Node>& nodes;
 std::vector<std::map<std::string,size_t>> children;
 public:
 explicit Tree(const std::vector<Node>& input):nodes(input),children(input.size()){
  Require(!nodes.empty()&&nodes[0].parent==SIZE_MAX,"bone tree has no Prefab root");
  for(size_t i=1;i<nodes.size();++i){auto p=nodes[i].parent;Require(p<i&&!nodes[i].name.empty()&&nodes[i].name.find('/')==std::string::npos,"bone node parent/path differs");
   auto added=children[p].emplace(nodes[i].name,i);if(!added.second)added.first->second=SIZE_MAX;}
 }
 std::vector<size_t> Chain(size_t node)const{
  Require(node<nodes.size(),"bone/Renderer is outside this Prefab");std::vector<size_t> result;
  while(node!=SIZE_MAX){result.push_back(node);node=nodes[node].parent;}return result;
 }
 size_t Find(size_t anchor,const std::string& path)const{
  if(path.empty())return anchor;size_t start=0;
  while(start<path.size()){auto end=path.find('/',start);auto name=path.substr(start,end==std::string::npos?end:end-start);
   if(name.empty()||name=="."||name=="..")return SIZE_MAX;auto next=children.at(anchor).find(name);
   if(next==children.at(anchor).end()||next->second==SIZE_MAX)return SIZE_MAX;anchor=next->second;
   if(end==std::string::npos)return anchor;start=end+1;if(start==path.size())return SIZE_MAX;
  }return SIZE_MAX;
 }
 std::vector<size_t> Resolve(size_t renderer,size_t skinRoot,const std::vector<std::string>& paths)const{
  if(paths.empty())return {};auto owner=Chain(renderer),roots=Chain(skinRoot);std::vector<std::vector<size_t>> candidates;
  for(auto anchor:roots){if(std::find(owner.begin(),owner.end(),anchor)==owner.end())continue;std::vector<size_t> bones;bool valid=true;
   for(const auto& path:paths){auto node=Find(anchor,path);if(node==SIZE_MAX){valid=false;break;}bones.push_back(node);}
   if(valid&&std::find(candidates.begin(),candidates.end(),bones)==candidates.end())candidates.push_back(std::move(bones));
  }
  Require(candidates.size()==1,"author bone paths have no unique anchor in this Prefab skeleton");
  Require(std::set<size_t>(candidates[0].begin(),candidates[0].end()).size()==candidates[0].size(),"author bone palette has duplicate Transform slots");return candidates.front();
 }
};
}
