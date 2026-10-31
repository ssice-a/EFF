#pragma once
#include "eiem_native_serialized_splice.h"
#include "eiem_mesh_target_directory.h"
#include "eiem_renderer_input_builder.h"
#include "eiem_demand_native_assembly.h"

// Demand-driven native-input preparation. Keep the game's asset identity and
// TypeTree while composing authored Mesh fields into the requested CAB.
namespace EiemDemandMeshPreparation {
using namespace EiemNativeInput;
namespace R=EiemNativeRandomAccess;namespace S=EiemNativeSpliceView;namespace P=EiemNativeSerializedSplice;
struct Author {
  EiemMeshTargetDirectory::Target target;
  Asset asset;
  Mesh mesh;
  Type schema;
  std::shared_ptr<const Resource> resource;
  P::Payload object;
  std::vector<uint32_t> sourceBones;
  std::shared_ptr<EiemDemandNativeAssembly::Author> streaming;
  EiemNativeInput::AssetIndex nativeTargets;
  EiemDemandNativeAssembly::Carrier nativeCarrier;
};
inline std::shared_ptr<const Author> ReadNativeAuthor(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    const EiemMeshTargetDirectory::Target& target,Asset asset,const std::filesystem::path& nativeRoot) {
  auto result=std::make_shared<Author>();result->target=target;result->asset=std::move(asset);
  auto catalog=std::make_shared<EiemDemandAuthorResources::Catalog>(nativeRoot);
  Require(catalog->Directory().mod==target.mod,"native author Mod identity differs");
  result->streaming=std::make_shared<EiemDemandNativeAssembly::Author>(catalog);
  result->nativeTargets.emplace(target.source,result->asset);
  result->nativeCarrier=EiemDemandNativeAssembly::PrepareCarrier(reader,memberIndex,layout,*result->streaming,result->nativeTargets);
  Require(result->nativeCarrier.parts.size()==target.parts&&target.parts,"native carrier part count differs from author declaration");
  result->mesh=result->streaming->package.meshes.at(target.partIds.at(0));
  result->schema=layout.types.at(layout.objects.at(result->asset.pathId).type);
  result->sourceBones=result->nativeCarrier.parts.front().sourceBoneHashes;return result;
}
inline std::shared_ptr<const Author> ReadAuthor(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    const EiemMeshTargetDirectory::Target& target,Asset asset) {
  Require(!target.skip&&target.parts==1&&target.partIds.size()==1&&target.resources.size()==2,"demand acceptance requires one declared Mesh part");
  const auto& member=reader.directory.members.at(memberIndex);Require(asset.classId==43&&asset.cab==member.name,"demand acceptance Mesh is not local to carrier");
  const auto& object=layout.objects.at(asset.pathId);const auto& type=layout.types.at(object.type);Require(type.classId==43,"demand acceptance object is not Mesh");
  auto original=reader.ReadAt(AddSize(member.offset,object.offset),object.size,reader.limits.fieldBytes);auto value=DecodeValue(type,0,original);
  auto result=std::make_shared<Author>();result->target=target;result->asset=std::move(asset);result->schema=type;
  result->resource=std::make_shared<const Resource>(EiemDirectory::Decode(EiemDirectory::Read(target.resources[0]),1));const auto& resource=*result->resource;
  auto geometry=resource.blocks.at("@geometry");Require(geometry.size==8,"demand acceptance geometry directory differs");auto& mesh=result->mesh;
  mesh.resource=target.resources[0].filename().u8string();mesh.vertices=unsigned(ResourceLe(resource,geometry.offset,4));mesh.indices=unsigned(ResourceLe(resource,geometry.offset+4,4));
  mesh.bones=Strings(resource,"@bones");mesh.shapes=Strings(resource,"@shapes");
  const auto& palette=ConstArray(type,Child(type,value,"m_BoneNameHashes"));
  for(size_t i=0;i<palette.raw.size();i+=4)result->sourceBones.push_back(uint32_t(Le(palette.raw,i,4)));
  auto modified=ApplyMesh(type,value,resource,mesh,target.source.asset);
  result->object=std::make_shared<const Bytes>(EncodeValue(type,modified));return result;
}
inline P::Plan MeshPlan(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,const Author& author) {
  if(author.streaming)return author.nativeCarrier.plan;
  return P::Build(reader,memberIndex,layout,{{author.asset.pathId,author.object}});
}
struct Consumer {P::Plan plan;size_t matches=0,changedObjects=0;};
inline Consumer PrepareConsumer(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,const Author& author) {
  if(author.streaming){auto built=EiemDemandNativeAssembly::PrepareConsumer(reader,memberIndex,layout,*author.streaming,author.nativeTargets,author.nativeCarrier.parts);
    return {std::move(built.plan),built.result.matches,built.result.changedObjects};}
  const auto& member=reader.directory.members.at(memberIndex);Consumer result;
  auto project=[&](int64_t id,const std::set<std::string>& fields){return reader.Project(member,layout,id,fields);};
  auto references=ProjectedMeshConsumers(layout,member.name,{{SerializedAssetCabKey(author.asset.cab),author.asset.pathId}},project,SerializedAssetCabKey);
  if(references.empty())return result;
  Package package;package.id=author.target.mod;package.targetIndex[author.target.source]=author.target.rule;
  const auto& part=author.target.partIds.at(0);package.rules[author.target.rule].parts={part};
  package.parts[part].mesh="MeshProof";package.parts[part].material="MaterialProof";package.meshes["MeshProof"]=author.mesh;package.resources[author.mesh.resource]=author.resource;
  NativeReference originalMaterial;
  for(const auto& reference:references){auto tree=project(reference.renderer,{"m_Materials"});const auto& type=layout.types.at(layout.objects.at(reference.renderer).type);
    const auto& materials=ConstArray(type,Child(type,tree,"m_Materials"));Require(!materials.children.empty(),"demand acceptance Renderer has no Material");
    auto material=NullableReference(type,materials.children[0],layout,member.name);
    Require(!result.matches||material==originalMaterial,"demand acceptance source Materials differ");originalMaterial=material;++result.matches;
  }
  Identity material{"assets/native-proof.mat","native-proof"};package.materials["MaterialProof"].source=material;AssetIndex assets;assets[author.target.source]=author.asset;
  assets[material].cab=originalMaterial.first;assets[material].pathId=originalMaterial.second;assets[material].classId=21;
  std::vector<BuiltMeshPart> parts{{author.target.source,author.target.rule,part,author.asset.cab,author.asset.pathId,author.asset.pathId,author.mesh.vertices,author.mesh.indices,author.sourceBones}};
  MeshSchemaIndex schemas{{author.schema.hash,author.schema}};RendererMemberBuilder builder(layout,member.name,package,assets,parts,project,[&](int64_t id){return reader.Object(member,layout,id);});builder.SetMeshSchemas(schemas);builder.UseSourceMaterials();auto built=builder.Build();
  Require(built.parts.size()==result.matches&&!built.addedObjects,"demand acceptance Renderer assembly did not match exact consumers");
  for(const auto& bound:built.parts)Require(bound.mesh==author.asset.pathId&&bound.meshCab==author.asset.cab&&bound.bones.size()==author.mesh.bones.size(),"demand acceptance shared Mesh identity/bone binding differs");
  std::map<int64_t,P::Payload> changed;
  for(auto& row:builder.TakeChanges())changed.emplace(row.first,std::make_shared<const Bytes>(std::move(row.second)));
  Require(builder.TakeAdded().empty(),"demand acceptance unexpectedly added objects");
  result.changedObjects=changed.size();result.plan=P::Build(reader,memberIndex,builder.OutputLayout(),changed);return result;
}
} // namespace EiemDemandMeshPreparation
