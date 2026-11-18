#pragma once
#include "eiem_native_random_access.h"
#include "eiem_render_assembly_description.h"

// A proof contains only the requested native prefab's structural addresses
// and verified logical Mesh identities. Native object names never establish
// an identity; container/preload resolution supplies the selected Mesh set.
namespace EiemDemandMeshProvenance {
using namespace EiemNativeInput;
namespace A=EiemRenderAssemblyDescription;
struct Use {A::Address renderer;std::set<Identity> identities;};
struct Prefab {std::string path;std::vector<Use> uses;};
using Meshes=std::map<NativeReference,std::set<Identity>>;
struct RendererMesh {A::Address renderer;NativeReference reference;};
inline std::vector<RendererMesh> ReadRendererMeshes(EiemNativeRandomAccess::Reader& reader,
    const std::string& path,const std::set<Identity>& targets,uint64_t assetHash){
 std::vector<RendererMesh> result;
 bool foundRoot=false;
 for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
  auto layout=reader.Layout(member);std::vector<int64_t> roots;
  const auto cab=SerializedAssetCabKey(member.name);
  for(const auto& entry:reader.Containers(member,layout)){
   const auto& asset=entry.asset;
   if(asset.path==path&&asset.classId==1&&SerializedAssetCabKey(asset.cab)==cab)roots.push_back(asset.pathId);
  }
  if(assetHash){auto hashes=reader.HashAssets(member,layout);auto found=hashes.find(assetHash);
   if(found!=hashes.end()&&found->second.classId==1&&SerializedAssetCabKey(found->second.cab)==SerializedAssetCabKey(member.name)){
    Require(roots.empty()||(roots.size()==1&&roots.front()==found->second.pathId),"prefab logical and native hash identities differ");
    roots={found->second.pathId};
   }
  }
  if(roots.empty())continue;
  Require(roots.size()==1&&!foundRoot,"requested native prefab identity is ambiguous");
  foundRoot=true;
  if(std::none_of(layout.objects.begin(),layout.objects.end(),[&](const auto& row){
    const auto kind=layout.types.at(row.second.type).classId;return kind==23||kind==137;
   }))continue;
  std::map<int64_t,std::shared_ptr<const Value>> values;
  const std::set<std::string> fields={"m_GameObject","m_Name","m_Component","m_Father","m_Children","m_Mesh"};
  for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);
   if(type.classId!=1&&!IsTransformClass(type.classId)&&type.classId!=137&&type.classId!=23&&type.classId!=33)continue;
   values.emplace(id,std::make_shared<const Value>(reader.Project(member,layout,id,fields)));
  }
  auto data=std::make_shared<const A::GraphData>(member.name,std::move(layout),std::move(values));
  A::Graph graph(data,roots.front());
  std::map<int64_t,bool> localMatches;
  for(auto renderer:graph.Renderers()){
   auto mesh=CanonicalSerializedAssetReference(graph.Mesh(renderer));
   if(!mesh.second)continue;
   if(mesh.first==cab){
    auto cached=localMatches.emplace(mesh.second,true);
    if(cached.second){
     auto object=data->layout.objects.find(mesh.second);
     if(object!=data->layout.objects.end()&&data->layout.types.at(object->second.type).classId==43){
      const auto meshFields=reader.Fields(member,data->layout,mesh.second,{"m_Name"});
      auto name=meshFields.find("m_Name");
      if(name!=meshFields.end()){
       const auto value=Text(name->second.raw.data(),name->second.raw.size());
       cached.first->second=std::any_of(targets.begin(),targets.end(),[&](const Identity& target){return target.asset==value;});
      }
     }
    }
    if(!cached.first->second)continue;
   }
   result.push_back({graph.At(renderer),std::move(mesh)});
  }
 }
 return result;
}
} // namespace EiemDemandMeshProvenance
