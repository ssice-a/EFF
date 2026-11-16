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
inline std::shared_ptr<const Prefab> Read(EiemNativeRandomAccess::Reader& reader,
    const std::string& path,const Meshes& meshes,uint64_t assetHash=0){
 std::shared_ptr<Prefab> result;
 for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
  auto layout=reader.Layout(member);std::vector<int64_t> roots;
  for(const auto& asset:reader.Assets(member,layout))if(asset.path==path&&asset.classId==1&&
      CanonicalSerializedAssetReference({asset.cab,asset.pathId}).first==SerializedAssetCabKey(member.name))roots.push_back(asset.pathId);
  if(assetHash){auto hashes=reader.HashAssets(member,layout);auto found=hashes.find(assetHash);
   if(found!=hashes.end()&&found->second.classId==1&&SerializedAssetCabKey(found->second.cab)==SerializedAssetCabKey(member.name)){
    Require(roots.empty()||(roots.size()==1&&roots.front()==found->second.pathId),"prefab logical and native hash identities differ");
    roots={found->second.pathId};
   }
  }
  if(roots.empty())continue;
  Require(roots.size()==1&&!result,"requested native prefab identity is ambiguous");
  std::map<int64_t,std::shared_ptr<const Value>> values;
  const std::set<std::string> fields={"m_GameObject","m_Name","m_Component","m_Father","m_Children","m_Mesh"};
  for(auto id:layout.order){const auto& type=layout.types.at(layout.objects.at(id).type);
   if(type.classId!=1&&!IsTransformClass(type.classId)&&type.classId!=137&&type.classId!=23&&type.classId!=33)continue;
   values.emplace(id,std::make_shared<const Value>(reader.Project(member,layout,id,fields)));
  }
  auto data=std::make_shared<const A::GraphData>(member.name,std::move(layout),std::move(values));
  A::Graph graph(data,roots.front());result=std::make_shared<Prefab>();result->path=path;
  for(auto renderer:graph.Renderers()){
   auto mesh=CanonicalSerializedAssetReference(graph.Mesh(renderer));auto found=meshes.find(mesh);
   if(found!=meshes.end())result->uses.push_back({graph.At(renderer),found->second});
  }
 }
 return result;
}
} // namespace EiemDemandMeshProvenance
