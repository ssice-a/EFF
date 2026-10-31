#pragma once
#include "eiem_demand_author_resources.h"
#include "eiem_native_material_operations.h"
#include "eiem_native_standalone_input.h"

// Build an immutable Material variant from the current game object at the
// moment a matched Mesh consumer is prepared.
namespace EiemDemandMaterialBuild {
using namespace EiemNativeInput;
namespace D=EiemRuntimeAuthorDirectory;
namespace R=EiemNativeRandomAccess;
namespace P=EiemNativeSerializedSplice;
namespace S=EiemNativeStandaloneInput;

inline S::Input Build(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    int64_t sourceId,const Identity& logical,const EiemAuthorMaterialEdits::Author& author,
    const std::map<std::string,NativeReference>& textures,R::ReadRange lease) {
  const auto& member=reader.directory.members.at(memberIndex);
  auto object=layout.objects.find(sourceId);
  Require(object!=layout.objects.end()&&layout.types.at(object->second.type).classId==21,
    "current logical Material is absent from its native Bundle");
  const auto& type=layout.types.at(object->second.type);
  auto value=reader.Object(member,layout,sourceId);auto references=layout;
  Require(author.source==logical.path,"EFFMAT source identity differs from Material declaration");
  Require(author.textures.size()==textures.size(),"EFFMAT texture binding set differs from resolved resources");
  for(const auto& binding:author.textures)Require(textures.count(binding.first),
    "EFFMAT texture binding has no resolved author Texture: "+binding.first);
  value=EiemNativeMaterialOperations::Apply(type,std::move(value),author.edits,references,member.name,textures);
  auto dependencies=EiemDemandAuthorResources::MaterialReferences(type,value,references,member.name);
  auto payload=std::make_shared<const Bytes>(EncodeValue(type,value));

  D::Writer identity;identity.S("eff-demand-material-variant/1");identity.S(logical.path);identity.S(logical.asset);
  identity.S(member.name);identity.N(uint64_t(sourceId));identity.N(type.nativeRecord.size());
  identity.b.insert(identity.b.end(),type.nativeRecord.begin(),type.nativeRecord.end());
  identity.N(payload->size());identity.b.insert(identity.b.end(),payload->begin(),payload->end());
  for(const auto& dependency:dependencies){identity.S(dependency.first);identity.N(uint64_t(dependency.second));}
  auto digest=Sha(identity.b.data(),identity.b.size());auto token=Hex(digest.data(),digest.size());
  std::transform(token.begin(),token.end(),token.begin(),[](unsigned char c){return char(std::tolower(c));});

  P::RangePayload edit;edit.pieces.push_back({0,payload->size(),payload,{}});
  return S::Build(reader,memberIndex,layout,sourceId,references,std::move(edit),token,dependencies,std::move(lease));
}
} // namespace EiemDemandMaterialBuild
