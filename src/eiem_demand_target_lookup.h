#pragma once
#include "eiem_native_random_access.h"
#include "eiem_demand_logical_asset_lookup.h"

namespace EiemDemandTargetLookup {
using namespace EiemNativeInput;
// Inspect only a native input actually requested by the game. A dependency
// subasset can lack a public RuntimeManifest record; never infer its identity
// from the exporter-time carrier hint or from an unrelated same-name object.
// The inspection lives only for one native request. Preload entries and
// unresolved targets share parsed metadata; no cache crosses a generation or
// retains the game's inputs after dependency preparation.
class Inspection {
 struct Member {
  SerializedLayout layout;
  bool canContainTarget=false;
  bool hasContainers=false;
  std::map<std::string,std::vector<EiemNativeRandomAccess::Reader::ContainerEntry>> containers;
  std::map<int64_t,std::vector<NativeReference>> preloads;
  std::map<int64_t,Asset> meshes;
  bool hasMeshSources=false;
  std::set<std::string> meshSources;
  explicit Member(SerializedLayout source):layout(std::move(source)){
   // An external container/preload can also name a Mesh in another member.
   canContainTarget=std::any_of(layout.externals.begin(),layout.externals.end(),GameBundleSource)||
     std::any_of(layout.objects.begin(),layout.objects.end(),[&](const auto& object){
       return layout.types.at(object.second.type).classId==43;
     });
  }
 };
 std::map<EiemNativeRandomAccess::Reader*,std::map<size_t,Member>> layouts_;
 Member& Metadata(EiemNativeRandomAccess::Reader& reader,const EiemNativeViewWriter::Member& member){
  auto& layouts=layouts_[&reader];auto found=layouts.find(member.offset);
  if(found==layouts.end())found=layouts.try_emplace(member.offset,reader.Layout(member)).first;
  return found->second;
 }
 const std::map<std::string,std::vector<EiemNativeRandomAccess::Reader::ContainerEntry>>& Containers(
     EiemNativeRandomAccess::Reader& reader,const EiemNativeViewWriter::Member& member){
  auto& metadata=Metadata(reader,member);
  if(!metadata.hasContainers){std::map<std::string,std::vector<EiemNativeRandomAccess::Reader::ContainerEntry>> rows;
   for(auto& entry:reader.Containers(member,metadata.layout))rows[entry.asset.path].push_back(std::move(entry));
   metadata.containers=std::move(rows);metadata.hasContainers=true;
  }return metadata.containers;
 }
 // Only pointers that can lead to a Mesh or its owning hierarchy expand
 // discovery. Unknown component/PPtr<Object> schemas remain conservative;
 // terminal render resources cannot contain a Mesh's logical container.
 static bool MeshPointer(const std::string& type){
  if(type.compare(0,5,"PPtr<"))return false;
  static const std::set<std::string> terminal={"PPtr<Material>","PPtr<Shader>","PPtr<Texture>","PPtr<Texture2D>",
   "PPtr<Texture3D>","PPtr<Cubemap>","PPtr<RenderTexture>","PPtr<ComputeShader>","PPtr<MonoScript>",
   "PPtr<Avatar>","PPtr<AudioClip>","PPtr<Sprite>","PPtr<SpriteAtlas>","PPtr<Font>"};
  return !terminal.count(type);
 }
 static bool HasMeshPointer(const Type& type,size_t node){
  const auto& value=type.nodes.at(node);
  if(value.type.compare(0,5,"PPtr<")==0)return MeshPointer(value.type);
  for(auto child:value.children)if(HasMeshPointer(type,child))return true;
  return false;
 }
public:
 const SerializedLayout& Layout(EiemNativeRandomAccess::Reader& reader,const EiemNativeViewWriter::Member& member){
  return Metadata(reader,member).layout;
 }
 SerializedLayout TakeLayout(EiemNativeRandomAccess::Reader& reader,const EiemNativeViewWriter::Member& member){
  auto readers=layouts_.find(&reader);
  if(readers!=layouts_.end()){
   auto cached=readers->second.extract(member.offset);
   if(cached)return std::move(cached.mapped().layout);
  }
  return reader.Layout(member);
 }
 bool MayContainMeshRoutes(EiemNativeRandomAccess::Reader& reader){
  auto& layouts=layouts_[&reader];
  for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
   auto found=layouts.find(member.offset);
   if(found==layouts.end()){
    auto layout=reader.MeshRoutingLayout(member);
    if(!layout)continue;
    layouts.try_emplace(member.offset,std::move(*layout));return true;
   }
   const auto& layout=found->second.layout;
   if(!layout.externals.empty()||std::any_of(layout.objects.begin(),layout.objects.end(),[&](const auto& object){
      return layout.types.at(object.second.type).classId==43;
   }))return true;
  }
  return false;
 }
 template<class MatchesPath>
 bool NeedsRootMeshDiscovery(EiemNativeRandomAccess::Reader& reader,MatchesPath&& matchesPath){
  if(!MayContainMeshRoutes(reader))return false;
  for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
   const auto& metadata=Metadata(reader,member);const auto& layout=metadata.layout;
   for(const auto& row:layout.objects){
    const auto kind=layout.types.at(row.second.type).classId;
    if(kind==1||kind==4||kind==33||kind==43||kind==137||kind==224||kind==1001)return true;
   }
   if(!metadata.canContainTarget)continue;
   const auto local=SerializedAssetCabKey(member.name);
   for(const auto& [path,entries]:Containers(reader,member)){
    // The exact container might publish a Mesh through its preload interval.
    // Keep external containers too, including external Prefab roots.
    if(matchesPath(path))return true;
    for(const auto& entry:entries)if(entry.asset.pathId){
     const auto cab=SerializedAssetCabKey(entry.asset.cab);
     if(cab!=local&&GameBundleSourceKey(cab))return true;
    }
   }
  }
  return false;
 }
 static bool GameBundleSourceKey(std::string_view key){
  // Engine built-ins are resolved by Unity, not a CAB in the game's native
  // dependency closure; they cannot own an authored assets/packages target.
  return key!="unity default resources"&&key!="unity_builtin_extra";
 }
 static bool GameBundleSource(const std::string& cab){
  return GameBundleSourceKey(SerializedAssetCabKey(cab));
 }
 static void MeshReferences(const Type& type,const Value& value,std::vector<bool>& externalFiles){
  const auto& node=type.nodes.at(value.node);
  if(node.type.compare(0,5,"PPtr<")==0){
   if(!MeshPointer(node.type)||!Integer(Child(type,value,"m_PathID")))return;
   auto externalIndex=Integer(Child(type,value,"m_FileID"));
   Require(externalIndex<=externalFiles.size(),"native Mesh discovery external index differs");
   if(externalIndex)externalFiles[size_t(externalIndex)-1]=true;
   return;
  }
  for(const auto& child:value.children)MeshReferences(type,child,externalFiles);
 }
 std::set<std::string> MeshSources(EiemNativeRandomAccess::Reader& reader){
  std::set<std::string> result;
  for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;
   auto& metadata=Metadata(reader,member);const auto& layout=metadata.layout;
   if(std::none_of(layout.externals.begin(),layout.externals.end(),GameBundleSource))continue;
   if(!metadata.hasMeshSources){std::set<std::string> sources;std::map<size_t,std::set<std::string>> fieldsByType;
    std::vector<bool> externalFiles(layout.externals.size());
    const auto localCab=SerializedAssetCabKey(member.name);
    for(auto id:layout.order){const auto typeIndex=layout.objects.at(id).type;const auto& type=layout.types.at(typeIndex);
     // AssetBundle preload tables mix every container's dependencies. Find()
     // alone examines the exact requested container interval; never walk all
     // preloads just to discover unrelated logical Meshes.
     if(type.classId==142){
      for(const auto& rows:Containers(reader,member))for(const auto& entry:rows.second)if(entry.asset.pathId&&GameBundleSource(entry.asset.cab)&&
          SerializedAssetCabKey(entry.asset.cab)!=localCab)sources.insert(entry.asset.cab);
      continue;
     }
     if(type.classId==21||type.classId==28||type.classId==43||type.classId==48||
        type.classId==83||type.classId==90||type.classId==115||type.classId==128||type.classId==213)continue;
     auto fields=fieldsByType.find(typeIndex);
     if(fields==fieldsByType.end()){std::set<std::string> wanted;
      for(auto child:type.nodes.at(0).children)if(HasMeshPointer(type,child))wanted.insert(type.nodes.at(child).name);
      fields=fieldsByType.emplace(typeIndex,std::move(wanted)).first;}
     if(fields->second.empty())continue;
     reader.VisitFields(member,layout,id,fields->second,
       [&](const std::string&,const Value& field){MeshReferences(type,field,externalFiles);});
    }
    for(size_t i=0;i<externalFiles.size();++i)if(externalFiles[i]){
     const auto& external=layout.externals[i];const auto key=SerializedAssetCabKey(external);
     if(key!=localCab&&GameBundleSourceKey(key))sources.insert(external);
    }
    metadata.meshSources=std::move(sources);metadata.hasMeshSources=true;
   }
   result.insert(metadata.meshSources.begin(),metadata.meshSources.end());
  }return result;
 }
bool ReferencedMesh(EiemNativeRandomAccess::Reader& reader,const NativeReference& reference,const Identity& identity,Asset& asset){
 for(const auto& member:reader.directory.members){if(!(member.flags&4)||SerializedAssetCabKey(member.name)!=SerializedAssetCabKey(reference.first))continue;
  const auto& layout=Layout(reader,member);auto found=layout.objects.find(reference.second);if(found==layout.objects.end())return false;
  const auto& type=layout.types.at(found->second.type);if(type.classId!=43)return false;
  auto& meshes=Metadata(reader,member).meshes;auto cached=meshes.find(reference.second);
  if(cached==meshes.end()){auto fields=reader.Fields(member,layout,reference.second,{"m_Name"});const auto& name=fields.at("m_Name");
    Asset value;value.asset=Text(name.raw.data(),name.raw.size());value.classId=43;value.cab=member.name;value.pathId=reference.second;value.type=type.hash;
    cached=meshes.emplace(reference.second,std::move(value)).first;}
  if(cached->second.asset!=identity.asset)return false;
  asset=cached->second;asset.path=identity.path;return true;
 }return false;
}
template<class Resolve>
std::vector<Asset> Find(EiemNativeRandomAccess::Reader& reader,const Identity& identity,Resolve&& resolve){
 std::map<NativeReference,Asset> matches;
 for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;auto& metadata=Metadata(reader,member);const auto& layout=metadata.layout;
  if(!metadata.canContainTarget)continue;
  const auto& containers=Containers(reader,member);
  auto rows=containers.find(identity.path);if(rows==containers.end())continue;
  std::set<NativeReference> references;
  for(const auto& entry:rows->second){
    auto reference=NativeReference{entry.asset.cab,entry.asset.pathId};
    Asset direct;if(reference.second&&GameBundleSource(reference.first)&&
      (ReferencedMesh(reader,reference,identity,direct)||(SerializedAssetCabKey(reference.first)!=SerializedAssetCabKey(member.name)&&resolve(reference,identity,direct)))){
      auto key=NativeReference{SerializedAssetCabKey(direct.cab),direct.pathId};matches.emplace(std::move(key),std::move(direct));}
    if(!entry.preloadSize)continue;
    auto table=metadata.preloads.find(entry.owner);if(table==metadata.preloads.end())table=metadata.preloads.emplace(entry.owner,reader.Preloads(member,layout,entry.owner)).first;
    const auto& values=table->second;Require(entry.preloadIndex<=values.size()&&entry.preloadSize<=values.size()-entry.preloadIndex,"native container preload range differs");
    for(size_t i=entry.preloadIndex;i<entry.preloadIndex+entry.preloadSize;++i)
      if(values[i].second&&GameBundleSource(values[i].first))references.insert(values[i]);
  }
  for(const auto& reference:references){
   Asset asset;if(ReferencedMesh(reader,reference,identity,asset)||resolve(reference,identity,asset)){
    auto key=NativeReference{SerializedAssetCabKey(asset.cab),asset.pathId};matches.emplace(std::move(key),std::move(asset));}
  }
 }std::vector<Asset> result;for(auto& row:matches)result.push_back(std::move(row.second));return result;
}
};
inline bool ReferencedMesh(EiemNativeRandomAccess::Reader& reader,const NativeReference& reference,const Identity& identity,Asset& asset){
 Inspection inspection;return inspection.ReferencedMesh(reader,reference,identity,asset);
}
template<class Resolve>
inline std::vector<Asset> Find(EiemNativeRandomAccess::Reader& reader,const Identity& identity,Resolve&& resolve){
 Inspection inspection;return inspection.Find(reader,identity,std::forward<Resolve>(resolve));
}
inline std::vector<Asset> Find(EiemNativeRandomAccess::Reader& reader,const Identity& identity){
 return Find(reader,identity,[](const NativeReference&,const Identity&,Asset&){return false;});
}
}
