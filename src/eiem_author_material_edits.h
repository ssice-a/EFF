#pragma once
#include "eiem_serialized_writer.h"
#include "eiem_material_operations.h"
#include <cerrno>
#include <cctype>
#include <cstdlib>

// Author-side projection of changed serialized Material fields into explicit
// operations on a private runtime copy. Unchanged donor properties are not
// pinned to yesterday's game material, CAB addresses or shader defaults.
namespace EiemAuthorMaterialEdits {
using namespace EiemNativeInput;
using EiemMaterialOperations::Edits;
struct Author {
 std::string source,name,shader;
 Edits edits;
 std::map<std::string,std::string> textures;
};
inline std::string Trim(std::string value){
 auto first=std::find_if_not(value.begin(),value.end(),[](unsigned char c){return std::isspace(c)!=0;});
 auto last=std::find_if_not(value.rbegin(),value.rend(),[](unsigned char c){return std::isspace(c)!=0;}).base();
 return first<last?std::string(first,last):std::string();
}
inline double ParseNumber(const std::string& value,const std::string& key){
 errno=0;char* end=nullptr;auto parsed=std::strtod(value.c_str(),&end);
 Require(end==value.c_str()+value.size()&&end!=value.c_str()&&errno!=ERANGE&&std::isfinite(parsed),"invalid EFFMAT number: "+key);
 return parsed;
}
inline int64_t ParseInteger(const std::string& value,const std::string& key){
 errno=0;char* end=nullptr;auto parsed=std::strtoll(value.c_str(),&end,10);
 Require(end==value.c_str()+value.size()&&end!=value.c_str()&&errno!=ERANGE,"invalid EFFMAT integer: "+key);
 return parsed;
}
template<size_t N> inline std::array<double,N> ParseComponents(const std::string& value,const std::string& key){
 std::array<double,N> result{};size_t begin=0;
 for(size_t i=0;i<N;++i){auto end=value.find(',',begin);if(i+1==N)end=std::string::npos;
  Require((i+1==N)==(end==std::string::npos),"EFFMAT component count differs: "+key);
  result[i]=ParseNumber(Trim(value.substr(begin,end==std::string::npos?end:end-begin)),key);begin=end==std::string::npos?value.size():end+1;}
 return result;
}
inline Author Parse(const Bytes& bytes){
 const std::string text=Text(bytes.data(),bytes.size());std::map<std::string,std::string> properties;std::istringstream lines(text);std::string line;
 while(std::getline(lines,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();line=Trim(std::move(line));
  if(line.empty()||line.front()=='#'||line.front()==';')continue;auto equals=line.find('=');
  Require(equals!=std::string::npos&&equals>0,"invalid EFFMAT line");auto key=Trim(line.substr(0,equals)),value=Trim(line.substr(equals+1));
  Require(!key.empty()&&properties.emplace(std::move(key),std::move(value)).second,"duplicate EFFMAT property");}
 auto get=[&](const char* key)->std::string{auto found=properties.find(key);return found==properties.end()?std::string():found->second;};
 Require(get("format")=="EFFMAT"&&get("version")=="1"&&get("overrides")=="true"&&!get("source").empty(),"EFFMAT header/source differs");
 Author result;result.source=get("source");result.name=get("name");result.shader=get("shader");
 for(const auto& row:properties){const auto& key=row.first;const auto& value=row.second;
  if(key=="format"||key=="version"||key=="overrides"||key=="source"||key=="name"||key=="shader")continue;
  if(key=="render_queue"){
   auto queue=ParseInteger(value,key);Require(queue==-1||(queue>=0&&queue<=5000),"EFFMAT render queue outside supported range");
   Require(!result.edits.queue,"duplicate EFFMAT render queue");result.edits.queue=int32_t(queue);continue;
  }
  auto dot=key.find('.');auto kind=dot==std::string::npos?key:key.substr(0,dot);auto property=dot==std::string::npos?std::string():key.substr(dot+1);
  Require(!property.empty()&&property.find_first_of("\r\n\t")==std::string::npos,"invalid EFFMAT property name: "+key);
  bool inserted=false;
  if(kind=="float")inserted=result.edits.floats.emplace(property,ParseNumber(value,key)).second;
  else if(kind=="int")inserted=result.edits.integers.emplace(property,ParseInteger(value,key)).second;
  else if(kind=="value4"||kind=="color")inserted=result.edits.colors.emplace(property,ParseComponents<4>(value,key)).second;
  else if(kind=="texture_scale")inserted=result.edits.scales.emplace(property,ParseComponents<2>(value,key)).second;
  else if(kind=="texture_offset")inserted=result.edits.offsets.emplace(property,ParseComponents<2>(value,key)).second;
  else if(kind=="keyword"){
   Require(value=="0"||value=="1"||value=="false"||value=="true","invalid EFFMAT keyword value: "+key);
   inserted=result.edits.keywords.emplace(property,value=="1"||value=="true").second;
  }else if(kind=="tag")inserted=result.edits.tags.emplace(property,value).second;
  else if(kind=="texture"){
   Require(!value.empty()&&result.textures.emplace(property,value).second,"duplicate or empty EFFMAT texture binding: "+key);continue;
  }else throw std::runtime_error("unsupported EFFMAT edit: "+key);
  Require(inserted,"duplicate EFFMAT operation: "+key);
 }
 return result;
}
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
  for(const auto& row:previous)if(!next.count(row.first))result.removed[field].insert(row.first);
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
  for(const auto& row:previous)if(!next.count(row.first))result.removed["stringTagMap"].insert(row.first);
  for(const auto& row:next){auto prior=previous.find(row.first);if(prior==previous.end()||String(type,*prior->second)!=String(type,*row.second))result.tags[row.first]=String(type,*row.second);}
 }
 if(auto q=Find(type,current,"m_CustomRenderQueue")){auto old=Find(type,original,"m_CustomRenderQueue");Require(old&&q->raw.size()==4,"Material queue structure differs");
  if(q->raw!=old->raw){int32_t n;memcpy(&n,q->raw.data(),4);Require(n>=-1,"Material queue is invalid");result.queue=n;}
 }
 return result;
}
} // namespace EiemAuthorMaterialEdits
