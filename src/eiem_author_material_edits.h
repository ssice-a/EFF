#pragma once
#include "eiem_serialized_writer.h"
#include "eiem_material_operations.h"

// Author-side projection of changed serialized Material fields into explicit
// operations on a private runtime copy. Unchanged donor properties are not
// pinned to yesterday's game material, CAB addresses or shader defaults.
namespace EiemAuthorMaterialEdits {
using namespace EiemNativeInput;
using EiemMaterialOperations::Edits;
inline const Value* Find(const Type& type,const Value& parent,const char* name){
 const Value* found=nullptr;for(const auto& value:parent.children)if(type.nodes.at(value.node).name==name){Require(!found,"duplicate Material field");found=&value;}return found;
}
inline std::string String(const Type& type,const Value& value){
 Require(type.nodes.at(value.node).type=="string","Material property name is not a string");return Text(value.raw.data(),value.raw.size());
}
inline double Number(const Type& type,const Value& value){
 const auto& name=type.nodes.at(value.node).type;
 if(name=="float"){Require(value.raw.size()==4,"Material float field width differs");float n;memcpy(&n,value.raw.data(),4);Require(std::isfinite(n),"nonfinite Material property");return n;}
 throw std::runtime_error("Material numeric field is not Float32");
}
template<size_t N>std::array<double,N> Components(const Type& type,const Value& value,const std::array<const char*,N>& names){
 std::array<double,N> result{};for(size_t i=0;i<N;++i)result[i]=Number(type,Child(type,value,names[i]));return result;
}
inline std::map<std::string,const Value*> Map(const Type& type,const Value& value){
 std::map<std::string,const Value*> result;
 for(const auto& entry:ConstArray(type,value).children){auto key=String(type,Child(type,entry,"first"));Require(result.emplace(std::move(key),&Child(type,entry,"second")).second,"duplicate Material property");}return result;
}
inline std::set<std::string> Keywords(const Type& type,const Value& root){
 std::set<std::string> result;
 if(auto valid=Find(type,root,"m_ValidKeywords"))for(const auto& value:ConstArray(type,*valid).children)Require(result.insert(String(type,value)).second,"duplicate Material keyword");
 else if(auto text=Find(type,root,"m_ShaderKeywords")){std::istringstream values(String(type,*text));std::string word;while(values>>word)Require(result.insert(word).second,"duplicate Material keyword");}
 return result;
}
inline Edits Project(const Type& type,const Value& original,const Resource& resource){
 Require(type.classId==21&&resource.kind==2&&resource.type==type.hash,"Material edit schema differs from donor");
 Value current=original;
 const std::set<std::string> allowed{"m_SavedProperties","m_CustomRenderQueue","m_ValidKeywords","m_InvalidKeywords","m_ShaderKeywords","stringTagMap"};
 for(const auto& block:resource.blocks){if(block.first[0]=='@')continue;Require(allowed.count(block.first),"Material field needs an explicit runtime operation: "+block.first);
  auto& field=Child(type,current,block.first);const auto& bytes=block.second;
  Bytes payload(resource.bytes.begin()+bytes.offset,resource.bytes.begin()+bytes.offset+bytes.size);
  field=DecodeValue(type,field.node,payload);Require(EncodeValue(type,field)==payload,"Material edit field roundtrip differs");
 }
 Edits result;
 const auto& before=Child(type,original,"m_SavedProperties");const auto& after=Child(type,current,"m_SavedProperties");
 auto changed=[&](const char* field,const auto& apply){
  auto a=Find(type,before,field),b=Find(type,after,field);Require(bool(a)==bool(b),"Material property map structure changed");if(!b)return;
  auto previous=Map(type,*a),next=Map(type,*b);
  for(const auto& row:previous)Require(next.count(row.first),"Material edit removed a donor property");
  for(const auto& row:next){auto prior=previous.find(row.first);if(prior!=previous.end()&&EncodeValue(type,*prior->second)==EncodeValue(type,*row.second))continue;apply(row.first,*row.second,prior==previous.end()?nullptr:prior->second);}
 };
 changed("m_Floats",[&](const std::string& key,const Value& value,const Value*){result.floats[key]=Number(type,value);});
 changed("m_Ints",[&](const std::string& key,const Value& value,const Value*){Require(value.raw.size()==4,"Material integer field width differs");int32_t n;memcpy(&n,value.raw.data(),4);result.integers[key]=n;});
 changed("m_Colors",[&](const std::string& key,const Value& value,const Value*){result.colors[key]=Components<4>(type,value,{"r","g","b","a"});});
 changed("m_TexEnvs",[&](const std::string& key,const Value& value,const Value* prior){
  Require(prior,"Material texture bindings must be declared in INI");
  Require(EncodeValue(type,Child(type,value,"m_Texture"))==EncodeValue(type,Child(type,*prior,"m_Texture")),"Material texture pointer edit must be declared in INI");
  for(const char* field:{"m_Scale","m_Offset"})if(EncodeValue(type,Child(type,value,field))!=EncodeValue(type,Child(type,*prior,field)))
   (strcmp(field,"m_Scale")?result.offsets:result.scales)[key]=Components<2>(type,Child(type,value,field),{"x","y"});
 });
 auto oldKeys=Keywords(type,original),newKeys=Keywords(type,current);
 for(const auto& key:newKeys)if(!oldKeys.count(key))result.keywords[key]=true;
 for(const auto& key:oldKeys)if(!newKeys.count(key))result.keywords[key]=false;
 if(auto tags=Find(type,current,"stringTagMap")){auto old=Find(type,original,"stringTagMap");Require(old,"Material tag map appeared without a donor");auto previous=Map(type,*old),next=Map(type,*tags);
  for(const auto& row:previous)Require(next.count(row.first),"Material tag removal has no runtime operation");
  for(const auto& row:next){auto prior=previous.find(row.first);if(prior==previous.end()||String(type,*prior->second)!=String(type,*row.second))result.tags[row.first]=String(type,*row.second);}
 }
 if(auto q=Find(type,current,"m_CustomRenderQueue")){auto old=Find(type,original,"m_CustomRenderQueue");Require(old&&q->raw.size()==4,"Material queue structure differs");
  if(q->raw!=old->raw){int32_t n;memcpy(&n,q->raw.data(),4);Require(n>=-1,"Material queue is invalid");result.queue=n;}
 }
 return result;
}
} // namespace EiemAuthorMaterialEdits
