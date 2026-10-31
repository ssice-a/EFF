#pragma once
#include "eiem_author_material_edits.h"
#include "eiem_texture_input_builder.h"

// Replay sparse author operations against CURRENT serialized Material data,
// before native creation. Untouched shader defaults and references survive.
namespace EiemNativeMaterialOperations {
using namespace EiemNativeInput;
namespace A=EiemAuthorMaterialEdits;
inline Value& Entry(const Type& type,Value& map,const std::string& name,bool texture=false){
  auto& list=ArrayValue(type,map);Value* found=nullptr;
  for(auto& entry:list.children)if(A::String(type,Child(type,entry,"first"))==name){Require(!found,"duplicate native Material property: "+name);found=&entry;}
  if(!found){auto value=ArrayElement(type,list);SetString(type,Child(type,value,"first"),name);
    if(texture){auto& scale=Child(type,Child(type,value,"second"),"m_Scale");SetFloat(type,Child(type,scale,"x"),1);SetFloat(type,Child(type,scale,"y"),1);}
    list.children.push_back(std::move(value));list.count=list.children.size();found=&list.children.back();}
  return Child(type,*found,"second");
}
inline void Keywords(const Type& type,Value& material,const EiemMaterialOperations::Edits& edits){
  if(edits.keywords.empty())return;
  auto keys=A::Keywords(type,material);for(const auto& edit:edits.keywords){if(edit.second)keys.insert(edit.first);else keys.erase(edit.first);}
  auto fill=[&](Value& array,const std::set<std::string>& values){auto& list=ArrayValue(type,array);list.children.clear();list.raw.clear();list.rawOrigins.clear();
    for(const auto& key:values){auto value=ArrayElement(type,list);SetString(type,value,key);list.children.push_back(std::move(value));}list.count=list.children.size();};
  if(A::Find(type,material,"m_ValidKeywords"))fill(Child(type,material,"m_ValidKeywords"),keys);
  else{Require(A::Find(type,material,"m_ShaderKeywords"),"current Material keyword representation absent");std::string joined;for(const auto& key:keys){if(!joined.empty())joined+=' ';joined+=key;}SetString(type,Child(type,material,"m_ShaderKeywords"),joined);}
  if(auto invalid=A::Find(type,material,"m_InvalidKeywords")){std::set<std::string> kept;
    for(const auto& value:ConstArray(type,*invalid).children){auto key=A::String(type,value);if(!edits.keywords.count(key))kept.insert(key);}fill(Child(type,material,"m_InvalidKeywords"),kept);}
}
inline Value Apply(const Type& type,Value current,const EiemMaterialOperations::Edits& edits,
    SerializedLayout& layout,const std::string& cab,const std::map<std::string,NativeReference>& textures={}){
  Require(type.classId==21,"native Material operations target class differs");
  auto& saved=Child(type,current,"m_SavedProperties");
  for(const auto& removal:edits.removed){
    Require(removal.first=="m_Floats"||removal.first=="m_Ints"||removal.first=="m_Colors"||removal.first=="m_TexEnvs"||removal.first=="stringTagMap",
      "native Material removal map is unsupported");
    auto& owner=removal.first=="stringTagMap"?current:saved;
    // A game update can remove an entire optional map. Removing a property
    // which is already absent remains an idempotent operation.
    if(!A::Find(type,owner,removal.first.c_str()))continue;
    auto& list=ArrayValue(type,Child(type,owner,removal.first));
    list.children.erase(std::remove_if(list.children.begin(),list.children.end(),[&](const auto& row){return removal.second.count(A::String(type,Child(type,row,"first")))!=0;}),list.children.end());
    list.count=list.children.size();
  }
  for(const auto& edit:edits.floats)SetFloat(type,Entry(type,Child(type,saved,"m_Floats"),edit.first),edit.second);
  for(const auto& edit:edits.integers){Require(edit.second>=INT32_MIN&&edit.second<=INT32_MAX,"native Material integer exceeds Int32");SetInteger(type,Entry(type,Child(type,saved,"m_Ints"),edit.first),uint64_t(edit.second));}
  for(const auto& edit:edits.colors){auto& value=Entry(type,Child(type,saved,"m_Colors"),edit.first);for(size_t i=0;i<4;++i)SetFloat(type,Child(type,value,std::string(1,"rgba"[i])),edit.second[i]);}
  auto vectors=[&](const auto& values,const char* field){for(const auto& edit:values){auto& value=Child(type,Entry(type,Child(type,saved,"m_TexEnvs"),edit.first,true),field);
    SetFloat(type,Child(type,value,"x"),edit.second[0]);SetFloat(type,Child(type,value,"y"),edit.second[1]);}};
  vectors(edits.scales,"m_Scale");vectors(edits.offsets,"m_Offset");
  for(const auto& texture:textures){Require(!texture.first.empty()&&texture.second.second,"native Material texture declaration is empty");
    auto& pointer=Child(type,Entry(type,Child(type,saved,"m_TexEnvs"),texture.first,true),"m_Texture");SetReference(type,pointer,texture.second,layout,cab);}
  Keywords(type,current,edits);
  for(const auto& edit:edits.tags)SetString(type,Entry(type,Child(type,current,"stringTagMap"),edit.first),edit.second);
  if(edits.queue){Require(*edits.queue>=-1,"native Material render queue invalid");SetInteger(type,Child(type,current,"m_CustomRenderQueue"),uint64_t(int64_t(*edits.queue)));}
  return current;
}
} // namespace EiemNativeMaterialOperations
