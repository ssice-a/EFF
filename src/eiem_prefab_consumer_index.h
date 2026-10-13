#pragma once
#include "eiem_renderer_input_builder.h"

// Exact root ownership without bone palettes, material arrays or helper trees.
// Build once per SerializedFile, only after finding a selected Mesh consumer.
namespace EiemPrefabConsumers {
using namespace EiemNativeInput;
class Ownership {
 const Bytes& bytes;const SerializedLayout& layout;std::string cab;
 struct Node {int64_t owner=0,parent=0;std::vector<int64_t> children;};
 std::map<int64_t,Node> nodes;std::map<int64_t,int64_t> transforms;
 int64_t Local(const Type& type,const Value& value)const{
  auto ref=NullableReference(type,value,layout,cab);Require(ref.first==cab,"Prefab ownership requires local hierarchy");return ref.second;
 }
 public:
 Ownership(const Bytes& b,const SerializedLayout& l,const std::string& c):bytes(b),layout(l),cab(c){
  for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);if(!IsTransformClass(type.classId))continue;
   auto value=ObjectFields(bytes,layout,id,{"m_GameObject","m_Father","m_Children"});Node node;
   node.owner=Local(type,Child(type,value,"m_GameObject"));node.parent=Local(type,Child(type,value,"m_Father"));
   Require(node.owner&&layout.types.at(layout.objects.at(node.owner).type).classId==1,"Prefab Transform owner is not a GameObject");
   for(const auto& child:ConstArray(type,Child(type,value,"m_Children")).children)node.children.push_back(Local(type,child));
   Require(transforms.emplace(node.owner,id).second,"Prefab ownership duplicate Transform");nodes.emplace(id,std::move(node));
  }
 }
 int64_t Owner(int64_t component)const{
  const auto& type=layout.types.at(layout.objects.at(component).type);
  auto value=ObjectFields(bytes,layout,component,{"m_GameObject"});auto go=Local(type,Child(type,value,"m_GameObject"));
  Require(transforms.count(go),"Prefab consumer owner has no Transform");return go;
 }
 std::set<int64_t> GameObjects(int64_t root)const{
  std::set<int64_t> visited,result;std::vector<int64_t> pending{transforms.at(root)};
  while(!pending.empty()){auto id=pending.back();pending.pop_back();
   Require(visited.insert(id).second,"Prefab ownership repeated hierarchy child/cycle");const auto& node=nodes.at(id);result.insert(node.owner);
   for(auto child:node.children){Require(nodes.at(child).parent==id,"Prefab ownership child/parent differs");pending.push_back(child);}
  }return result;
 }
};
} // namespace EiemPrefabConsumers
