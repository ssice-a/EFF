#pragma once
#define NOMINMAX
#include "../../src/eiem_static_resource_sources.h"
#include <iostream>
using namespace EiemNativeInput;
struct FieldSpec {std::string type,name;std::vector<FieldSpec> children;};
FieldSpec Ref(const char* name){return {"PPtr<Object>",name,{{"int","m_FileID",{}},{"SInt64","m_PathID",{}}}};}
FieldSpec Arr(const char* name,FieldSpec item){item.name="data";return {"vector",name,{{"Array","Array",{{"int","size",{}},item}}}};}
FieldSpec Vec(const char* name,bool rotation=false){FieldSpec f{"Vector3f",name,{{"float","x",{}},{"float","y",{}},{"float","z",{}}}};if(rotation)f.children.push_back({"float","w",{}});return f;}
Type Schema(int kind,std::initializer_list<FieldSpec> fields){Type t;t.classId=kind;t.hash[0]=(unsigned char)kind;
 auto add=[&](auto&& self,const FieldSpec& f)->size_t{auto n=t.nodes.size();t.nodes.push_back({f.type,f.name,false,{}});for(const auto& c:f.children){auto child=self(self,c);t.nodes[n].children.push_back(child);}return n;};
 add(add,{"Object","Base",fields});return t;}
void N(Bytes& b,uint64_t v,size_t n){for(size_t i=0;i<n;++i)b.push_back((unsigned char)(v>>(i*8)));}
void Z(Bytes& b,const std::string& s){b.insert(b.end(),s.begin(),s.end());b.push_back(0);}
void Be(Bytes& b,size_t at,uint64_t v,size_t n){for(size_t i=0;i<n;++i)b[at+i]=(unsigned char)(v>>((n-1-i)*8));}
Bytes Bundle(const Bytes& member,const std::string& name){Bytes source(49);auto a=uint32_t(123);EndfieldInput::Put(source,0,a^0x4A92F0CD,4);EndfieldInput::Put(source,4,((4*a&0xFFFF0000)^EndfieldInput::Rol32(a,18)^0xD8B1E637),4);
 auto info=EndfieldInput::Encode32(1);auto size=EndfieldInput::Encode64(source.size());EndfieldInput::Put(source,8,info.second,2);EndfieldInput::Put(source,14,8,4);EndfieldInput::Put(source,18,size.second,4);EndfieldInput::Put(source,22,0xA7F49310,4);EndfieldInput::Put(source,26,info.first,2);EndfieldInput::Put(source,32,info.second,2);EndfieldInput::Put(source,34,size.first,4);EndfieldInput::Put(source,38,info.first,2);
 return EndfieldInput::Encode(source,{{name,0,member}});
}
struct Fixture {
 std::vector<Type> types;std::map<int64_t,std::pair<size_t,Value>> objects;SerializedLayout refs;std::string cab="CAB-static";
 Value& Add(int64_t id,size_t type){return objects.emplace(id,std::make_pair(type,EmptyValue(types.at(type),0))).first->second.second;}
 const Type& T(int64_t id){return types.at(objects.at(id).first);}
 Value& V(int64_t id){return objects.at(id).second;}
 void Pointer(int64_t id,const char* field,int64_t target){SetReference(T(id),Child(T(id),V(id),field),{cab,target},refs,cab);}
  void List(int64_t id,const char* name,std::initializer_list<int64_t> targets,const char* field=nullptr){auto& a=ArrayValue(T(id),Child(T(id),V(id),name));for(auto target:targets){auto v=ArrayElement(T(id),a);SetReference(T(id),field?Child(T(id),v,field):v,{cab,target},refs,cab);a.children.push_back(v);}a.count=a.children.size();}
  void SetList(int64_t id,const char* name,std::initializer_list<int64_t> targets,const char* field=nullptr){auto& a=ArrayValue(T(id),Child(T(id),V(id),name));a.children.clear();a.raw.clear();a.rawOrigins.clear();a.count=0;List(id,name,targets,field);}
 Bytes BytesOf(){Bytes m;Z(m,"fixture");N(m,0,4);N(m,1,1);N(m,types.size(),4);
  for(const auto& t:types){N(m,t.classId,4);N(m,0,1);N(m,65535,2);if(t.classId==114)N(m,0,8),N(m,0,8);m.insert(m.end(),t.hash.begin(),t.hash.end());Bytes strings,nodes;
   auto emit=[&](auto&& self,size_t n,unsigned depth)->void{const auto& f=t.nodes[n];auto offType=strings.size();Z(strings,f.type);auto offName=strings.size();Z(strings,f.name);N(nodes,1,2);N(nodes,depth,1);N(nodes,0,1);N(nodes,offType,4);N(nodes,offName,4);N(nodes,0,8);N(nodes,f.align?0x4000:0,4);N(nodes,0,8);for(auto c:f.children)self(self,c,depth+1);};emit(emit,0,0);
   N(m,t.nodes.size(),4);N(m,strings.size(),4);m.insert(m.end(),nodes.begin(),nodes.end());m.insert(m.end(),strings.begin(),strings.end());N(m,0,4);
  }N(m,objects.size(),4);while(m.size()%4)m.push_back(0);Bytes data;
  for(const auto& o:objects){while(data.size()%8)data.push_back(0);auto bytes=EncodeValue(types[o.second.first],o.second.second);N(m,o.first,8);N(m,data.size(),8);N(m,bytes.size(),4);N(m,o.second.first,4);data.insert(data.end(),bytes.begin(),bytes.end());}
  N(m,0,4);N(m,refs.externals.size(),4);for(const auto& external:refs.externals){Z(m,"");m.insert(m.end(),20,0);Z(m,"archive:/"+external+"/"+external);}
  Bytes b(48);b.insert(b.end(),m.begin(),m.end());while(b.size()%16)b.push_back(0);Be(b,20,m.size(),4);Be(b,32,b.size(),8);b.insert(b.end(),data.begin(),data.end());Be(b,24,b.size(),8);ParseSerialized(b);return b;
 }
 Fixture(){auto info=FieldSpec{"AssetInfo","second",{{"int","preloadIndex",{}},{"int","preloadSize",{}},Ref("asset")}};
  types={Schema(1,{{"string","m_Name",{}},Arr("m_Component",{"ComponentPair","data",{Ref("component")}})}),
   Schema(4,{Ref("m_GameObject"),Ref("m_Father"),Arr("m_Children",Ref("data")),Vec("m_LocalPosition"),Vec("m_LocalScale"),Vec("m_LocalRotation",true)}),
   Schema(33,{Ref("m_GameObject"),Ref("m_Mesh")}),Schema(23,{Ref("m_GameObject"),{"bool","m_Enabled",{}},Arr("m_Materials",Ref("data")),Ref("m_ProbeAnchor"),Ref("m_PerRendererLightingAnchor"),{"StaticBatchInfo","m_StaticBatchInfo",{{"UInt16","firstSubMesh",{}},{"UInt16","subMeshCount",{}}}},Ref("m_StaticBatchRoot"),Ref("m_AdditionalVertexStreams")}),
   Schema(142,{{"string","m_Name",{}},{"string","m_AssetBundleName",{}},Arr("m_Dependencies",{"string","data",{}}),Arr("m_PreloadTable",Ref("data")),Arr("m_Container",{"pair","data",{{"string","first",{}},info}}),Arr("m_HashContainer",{"pair","data",{{"int","first",{}},info}}),{"AssetInfo","m_MainAsset",info.children}}),
   Schema(43,{{"string","m_Name",{}}}),Schema(21,{{"string","m_Name",{}}}),
   Schema(205,{Ref("m_GameObject"),Arr("m_LODs",{"LOD","data",{{"float","screenRelativeHeight",{}},Arr("renderers",{"RendererData","data",{Ref("renderer")}})}})}),
   Schema(114,{Ref("m_GameObject"),{"bool","useCustomRenderers",{}},Arr("customRenderers",Ref("data"))})};
  for(int64_t base:{10,20}){Add(base,0);SetString(T(base),Child(T(base),V(base),"m_Name"),base==10?"A":"B");Add(base+1,1);Pointer(base+1,"m_GameObject",base);SetVector(T(base+1),Child(T(base+1),V(base+1),"m_LocalScale"),{1,1,1});Add(base+2,2);Pointer(base+2,"m_GameObject",base);Pointer(base+2,"m_Mesh",100);Add(base+3,3);Pointer(base+3,"m_GameObject",base);SetInteger(T(base+3),Child(T(base+3),V(base+3),"m_Enabled"),1);List(base+3,"m_Materials",{200});List(base,"m_Component",{base+1,base+2,base+3},"component");}
  Add(100,5);SetString(T(100),Child(T(100),V(100),"m_Name"),"Static");Add(200,6);Add(300,4);SetString(T(300),Child(T(300),V(300),"m_Name"),"fixture");SetString(T(300),Child(T(300),V(300),"m_AssetBundleName"),"fixture.ab");List(300,"m_PreloadTable",{10,11,12,13,20,21,22,23,100,200});SetInteger(T(300),Child(T(300),Child(T(300),V(300),"m_MainAsset"),"preloadSize"),10);
  auto& container=ArrayValue(T(300),Child(T(300),V(300),"m_Container"));for(auto row:std::vector<std::pair<std::string,int64_t>>{{"assets/a.prefab",10},{"assets/b.prefab",20},{"assets/static.mesh",100}}){auto entry=ArrayElement(T(300),container);SetString(T(300),Child(T(300),entry,"first"),row.first);auto& infoValue=Child(T(300),entry,"second");SetReference(T(300),Child(T(300),infoValue,"asset"),{cab,row.second},refs,cab);SetInteger(T(300),Child(T(300),infoValue,"preloadSize"),10);container.children.push_back(entry);}container.count=container.children.size();
  SetInteger(T(13),Child(T(13),Child(T(13),V(13),"m_StaticBatchInfo"),"subMeshCount"),1);Pointer(13,"m_StaticBatchRoot",11);Pointer(13,"m_AdditionalVertexStreams",100);
  Add(14,7);Pointer(14,"m_GameObject",10);auto& lods=ArrayValue(T(14),Child(T(14),V(14),"m_LODs"));auto level=ArrayElement(T(14),lods);auto& rs=ArrayValue(T(14),Child(T(14),level,"renderers"));auto r=ArrayElement(T(14),rs);SetReference(T(14),Child(T(14),r,"renderer"),{cab,13},refs,cab);rs.children={r};rs.count=1;lods.children={level};lods.count=1;
  Add(15,8);Pointer(15,"m_GameObject",10);List(15,"customRenderers",{13});SetInteger(T(15),Child(T(15),V(15),"useCustomRenderers"),1);List(10,"m_Component",{14,15,0},"component");
  Add(30,0);SetString(T(30),Child(T(30),V(30),"m_Name"),"UnusedFilter");Add(31,1);Pointer(31,"m_GameObject",30);Add(32,2);Pointer(32,"m_GameObject",30);Pointer(32,"m_Mesh",100);List(30,"m_Component",{31,32},"component");
 }
};
