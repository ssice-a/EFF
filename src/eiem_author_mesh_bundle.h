#pragma once
#include "eiem_texture_input_builder.h"
#include "eiem_mesh_input_builder.h"

// Author-side construction. No consumer Prefabs, game install paths or
// runtime Mesh setters are needed to emit an independently loadable Mesh.
namespace EiemAuthorMeshBundle {
using namespace EiemNativeInput;
struct Asset {
 Bytes bytes;
 std::string member,bundle,path,meshName;
 int64_t meshId=0;
 unsigned vertices=0,indices=0,bones=0,shapes=0;
};
inline Bytes EnvelopeHeader(){
 Bytes b(49);EndfieldInput::Put(b,0,0x4A92F0CD,4);EndfieldInput::Put(b,4,0xD8B1E637,4);
 auto info=EndfieldInput::Encode32(1);auto size=EndfieldInput::Encode64(b.size());
 EndfieldInput::Put(b,8,info.second,2);EndfieldInput::Put(b,14,8,4);
 EndfieldInput::Put(b,18,size.second,4);EndfieldInput::Put(b,22,0xA7F49310,4);
 EndfieldInput::Put(b,26,info.first,2);EndfieldInput::Put(b,32,info.second,2);
 EndfieldInput::Put(b,34,size.first,4);EndfieldInput::Put(b,38,info.first,2);
 EndfieldInput::ReadHeader(b);return b;
}
inline Asset Isolate(const Bytes& source,const std::string& sourceCab,int64_t meshId,
 const std::string& path,const Value& mesh,const std::string& token,int expectedClass=43){
 Require(token.size()==64&&token.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,
         "author Mesh bundle token must be a content digest");
 auto layout=ParseSerialized(source);auto found=layout.objects.find(meshId);
 Require(found!=layout.objects.end()&&layout.types.at(found->second.type).classId==expectedClass&&(expectedClass==43||expectedClass==28),
         "author asset template identity is not a local Mesh/Texture2D");
 // A Mesh asset has no external runtime objects. Do not turn a template with
 // unresolved references into a supposedly standalone asset.
 Require(layout.externals.empty(),"standalone Mesh template has external dependencies");
 int64_t bundleId=0;
 for(const auto& row:layout.objects)if(layout.types.at(row.second.type).classId==142){
  Require(!bundleId,"standalone Mesh requires one AssetBundle directory");bundleId=row.first;
 }
 Require(bundleId&&bundleId!=meshId,"standalone Mesh AssetBundle directory absent");
 const auto& mt=layout.types.at(found->second.type);
 const auto& bt=layout.types.at(layout.objects.at(bundleId).type);
 auto bundle=ObjectValue(source,layout,bundleId);auto key="eff/"+token+".ab";
 SetString(bt,Child(bt,bundle,"m_Name"),key);
 SetString(bt,Child(bt,bundle,"m_AssetBundleName"),key);
 auto& dependencies=ArrayValue(bt,Child(bt,bundle,"m_Dependencies"));
 dependencies.children.clear();dependencies.raw.clear();dependencies.rawOrigins.clear();dependencies.count=0;
 auto& preloads=ArrayValue(bt,Child(bt,bundle,"m_PreloadTable"));
 auto pointer=ArrayElement(bt,preloads);SetReference(bt,pointer,{sourceCab,meshId},layout,sourceCab);
 preloads.children={std::move(pointer)};preloads.raw.clear();preloads.rawOrigins.clear();preloads.count=1;
 auto assetInfo=[&](Value& info){
  SetInteger(bt,Child(bt,info,"preloadIndex"),0);SetInteger(bt,Child(bt,info,"preloadSize"),1);
  SetReference(bt,Child(bt,info,"asset"),{sourceCab,meshId},layout,sourceCab);
 };
 bool exactPath=false;
 for(const char* field:{"m_Container","m_HashContainer"}){
  auto& entries=ArrayValue(bt,Child(bt,bundle,field));std::vector<Value> kept;
  for(const auto& entry:entries.children){auto ref=PointerIdentity(bt,Child(bt,Child(bt,entry,"second"),"asset"),layout,sourceCab);
   if(ref!=NativeReference{sourceCab,meshId})continue;
   if(!strcmp(field,"m_Container")){
    const auto& name=Child(bt,entry,"first");auto label=Text(name.raw.data(),name.raw.size());
    if(label!=path)continue;exactPath=true;
   }
   auto copy=entry;assetInfo(Child(bt,copy,"second"));kept.push_back(std::move(copy));
  }
  Require(!kept.empty(),"standalone Mesh lacks an exact string/hash asset directory entry");
  entries.children=std::move(kept);entries.raw.clear();entries.rawOrigins.clear();entries.count=entries.children.size();
 }
 Require(exactPath,"standalone Mesh source path absent");
 auto& main=Child(bt,bundle,"m_MainAsset");SetInteger(bt,Child(bt,main,"preloadIndex"),0);
 SetInteger(bt,Child(bt,main,"preloadSize"),0);SetReference(bt,Child(bt,main,"asset"),{sourceCab,0},layout,sourceCab);
 std::set<int64_t> removed;for(const auto& row:layout.objects)if(row.first!=meshId&&row.first!=bundleId)removed.insert(row.first);
 auto native=BuildSerialized(source,layout,{{meshId,EncodeValue(mt,mesh)},{bundleId,EncodeValue(bt,bundle)}},{},removed);
 auto after=ParseSerialized(native);Require(after.objects.size()==2&&after.externals.empty(),"standalone Mesh retains source objects/dependencies");
 Require(EncodeValue(after.types.at(after.objects.at(meshId).type),ObjectValue(native,after,meshId))==EncodeValue(mt,mesh),
         "standalone Mesh changed native metadata/geometry");
 Asset asset;asset.member="CAB-"+token.substr(0,32);asset.bundle=key;asset.path=path;asset.meshId=meshId;
 asset.meshName=Text(Child(mt,mesh,"m_Name").raw.data(),Child(mt,mesh,"m_Name").raw.size());
 asset.bytes=EndfieldInput::Encode(EnvelopeHeader(),{{asset.member,4,std::move(native)}});
 auto roundtrip=EndfieldInput::Decode(asset.bytes);Require(roundtrip.members.size()==1&&roundtrip.members.front().name==asset.member,
         "standalone Mesh archive roundtrip differs");return asset;
}
inline Asset Build(const Package& package,const Identity& target,const std::string& part,
 const Bytes& source,const std::string& sourceCab,int64_t meshId){
 auto rule=package.targetIndex.find(target);Require(rule!=package.targetIndex.end(),"standalone Mesh target absent from Mod");
 const auto& parts=package.rules.at(rule->second).parts;
 Require(std::find(parts.begin(),parts.end(),part)!=parts.end(),"standalone Mesh Part does not belong to target");
 auto layout=ParseSerialized(source);const auto& type=layout.types.at(layout.objects.at(meshId).type);
 auto assets=SerializedAssets(source,sourceCab,layout);
 Require(std::any_of(assets.begin(),assets.end(),[&](const auto& asset){return asset.path==target.path&&asset.asset==target.asset&&asset.pathId==meshId&&asset.cab==sourceCab&&asset.classId==43;}),
         "standalone Mesh source path+asset does not match template");
 const auto& m=package.meshes.at(package.parts.at(part).mesh);const auto& resource=*package.resources.at(m.resource);
 auto mesh=ApplyMesh(type,ObjectValue(source,layout,meshId),resource,m,RuntimePartName(package,part,target),!m.shapes.empty());
 auto label=package.id+"\n"+part+"\n"+target.path+"\n"+target.asset+"\n";
 auto payload=EncodeValue(type,mesh);payload.insert(payload.end(),label.begin(),label.end());auto hash=Sha(payload.data(),payload.size());
 auto asset=Isolate(source,sourceCab,meshId,target.path,mesh,Hex(hash.data(),hash.size()));
 asset.vertices=m.vertices;asset.indices=m.indices;asset.bones=unsigned(m.bones.size());asset.shapes=unsigned(m.shapes.size());return asset;
}
} // namespace EiemAuthorMeshBundle
