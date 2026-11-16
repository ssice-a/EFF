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
  bool hasContainers=false;
  std::map<std::string,std::vector<EiemNativeRandomAccess::Reader::ContainerEntry>> containers;
  std::map<int64_t,std::vector<NativeReference>> preloads;
  std::map<int64_t,Asset> meshes;
 };
 std::map<EiemNativeRandomAccess::Reader*,std::map<size_t,Member>> layouts_;
 Member& Metadata(EiemNativeRandomAccess::Reader& reader,const EiemNativeViewWriter::Member& member){
  auto& layouts=layouts_[&reader];auto found=layouts.find(member.offset);
  if(found==layouts.end()){Member value;value.layout=reader.Layout(member);found=layouts.emplace(member.offset,std::move(value)).first;}
  return found->second;
 }
 const SerializedLayout& Layout(EiemNativeRandomAccess::Reader& reader,const EiemNativeViewWriter::Member& member){
  return Metadata(reader,member).layout;
 }
public:
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
  if(!metadata.hasContainers){std::map<std::string,std::vector<EiemNativeRandomAccess::Reader::ContainerEntry>> rows;
    for(auto& entry:reader.Containers(member,layout))rows[entry.asset.path].push_back(std::move(entry));
    metadata.containers=std::move(rows);metadata.hasContainers=true;}
  auto rows=metadata.containers.find(identity.path);if(rows==metadata.containers.end())continue;
  std::set<NativeReference> references;
  for(const auto& entry:rows->second){
    auto reference=NativeReference{entry.asset.cab,entry.asset.pathId};
    Asset direct;if(ReferencedMesh(reader,reference,identity,direct)||(SerializedAssetCabKey(entry.asset.cab)!=SerializedAssetCabKey(member.name)&&resolve(reference,identity,direct))){auto key=NativeReference{SerializedAssetCabKey(direct.cab),direct.pathId};matches.emplace(std::move(key),std::move(direct));}
    if(!entry.preloadSize)continue;
    auto table=metadata.preloads.find(entry.owner);if(table==metadata.preloads.end())table=metadata.preloads.emplace(entry.owner,reader.Preloads(member,layout,entry.owner)).first;
    const auto& values=table->second;Require(entry.preloadIndex<=values.size()&&entry.preloadSize<=values.size()-entry.preloadIndex,"native container preload range differs");
    for(size_t i=entry.preloadIndex;i<entry.preloadIndex+entry.preloadSize;++i)if(values[i].second)references.insert(values[i]);
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
