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
 std::map<EiemNativeRandomAccess::Reader*,std::map<size_t,SerializedLayout>> layouts_;
 const SerializedLayout& Layout(EiemNativeRandomAccess::Reader& reader,const EiemNativeViewWriter::Member& member){
  auto& layouts=layouts_[&reader];auto found=layouts.find(member.offset);
  if(found==layouts.end())found=layouts.emplace(member.offset,reader.Layout(member)).first;
  return found->second;
 }
public:
bool ReferencedMesh(EiemNativeRandomAccess::Reader& reader,const NativeReference& reference,const Identity& identity,Asset& asset){
 for(const auto& member:reader.directory.members){if(!(member.flags&4)||SerializedAssetCabKey(member.name)!=SerializedAssetCabKey(reference.first))continue;
  const auto& layout=Layout(reader,member);auto found=layout.objects.find(reference.second);if(found==layout.objects.end())return false;
  const auto& type=layout.types.at(found->second.type);if(type.classId!=43)return false;
  auto fields=reader.Fields(member,layout,reference.second,{"m_Name"});const auto& name=fields.at("m_Name");
  if(Text(name.raw.data(),name.raw.size())!=identity.asset)return false;
  asset.path=identity.path;asset.asset=identity.asset;asset.classId=43;asset.cab=member.name;asset.pathId=reference.second;asset.type=type.hash;return true;
 }return false;
}
template<class Resolve>
std::vector<Asset> Find(EiemNativeRandomAccess::Reader& reader,const Identity& identity,Resolve&& resolve){
 std::map<NativeReference,Asset> matches;
 for(const auto& member:reader.directory.members){if(!(member.flags&4))continue;const auto& layout=Layout(reader,member);
  for(const auto& asset:reader.Assets(member,layout))if(asset.classId==43&&asset.path==identity.path&&asset.asset==identity.asset)matches.emplace(NativeReference{SerializedAssetCabKey(asset.cab),asset.pathId},asset);
  for(const auto& reference:reader.ContainerPreloads(member,layout,identity.path)){
   Asset asset;if(ReferencedMesh(reader,reference,identity,asset)||resolve(reference,identity,asset))
    matches.emplace(NativeReference{SerializedAssetCabKey(asset.cab),asset.pathId},std::move(asset));
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
