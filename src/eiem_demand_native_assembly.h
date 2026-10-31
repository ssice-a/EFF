#pragma once
#include "eiem_demand_author_resources.h"
#include "eiem_composed_package.h"
#include "eiem_renderer_input_builder.h"
#include "eiem_native_edit_transaction.h"
#include "eiem_native_preload_closure.h"

// Byte plans for one actual native input. Geometry stays in author file
// ranges; only current metadata, preload tables and instance bindings change.
namespace EiemDemandNativeAssembly {
using namespace EiemNativeInput;
namespace A=EiemDemandAuthorResources;namespace R=EiemNativeRandomAccess;
namespace G=EiemNativeObjectGraft;namespace P=EiemNativeSerializedSplice;
inline std::vector<EiemPhysicsInput::Document> ComposePhysicsDocuments(
    std::vector<std::pair<std::string,EiemPhysicsInput::Document>>& loaded,const std::set<Identity>& relevant) {
  std::vector<EiemPhysicsInput::Document> result;
  for(auto& owned:loaded){const auto& mod=owned.first;auto document=std::move(owned.second);
    for(auto it=document.groups.begin();it!=document.groups.end();){
      for(auto target=it->targets.begin();target!=it->targets.end();)if(!relevant.count(*target))target=it->targets.erase(target);else ++target;
      if(it->targets.empty())it=document.groups.erase(it);else ++it;
    }
    if(document.groups.empty())continue;
    const auto prefix=EiemComposedPackage::Prefix(mod);document.id=prefix+document.id;
    std::map<std::string,std::string> colliderIds,templateIds;
    std::map<std::string,EiemPhysicsInput::Template> templates;
    for(auto& row:document.templates){auto name=prefix+row.first;templateIds.emplace(row.first,name);row.second.cab=name;templates.emplace(name,std::move(row.second));}
    document.templates=std::move(templates);
    std::map<std::string,EiemPhysicsInput::Collider> colliders;
    for(auto& row:document.colliders){auto name=prefix+row.first;colliderIds.emplace(row.first,name);row.second.id=name;row.second.cab=templateIds.at(row.second.cab);colliders.emplace(name,std::move(row.second));}
    document.colliders=std::move(colliders);
    for(auto& group:document.groups){group.id=prefix+group.id;group.cab=templateIds.at(group.cab);for(auto& collider:group.colliders)collider=colliderIds.at(collider);}
    result.push_back(std::move(document));
  }
  return result;
}
struct Author {
  std::shared_ptr<A::Catalog> catalog;
  std::vector<std::shared_ptr<A::Catalog>> catalogs;
  Package package;
  std::map<std::string,EiemMeshBindingMetadata::Mesh> bindings;
  std::map<std::string,std::string> markers;
  std::map<std::string,std::pair<std::shared_ptr<A::Catalog>,std::string>> meshSources;
  std::map<std::string,std::pair<std::shared_ptr<A::Catalog>,std::string>> materialSources;
  explicit Author(std::shared_ptr<A::Catalog> input):catalog(std::move(input)) {
    catalogs.push_back(catalog);const auto& directory=catalog->Directory();package.id=directory.mod;package.root=catalog->Root();
    for(const auto& asset:directory.meshes){
      package.parts[asset.part].mesh=asset.part;package.parts[asset.part].material=asset.material;
      package.meshes[asset.part]={asset.file,asset.vertices,asset.indices,asset.bones,asset.shapes};
      package.resources.emplace(asset.file,std::make_shared<const Resource>());
      meshSources.emplace(asset.part,std::make_pair(catalog,asset.part));
    }
    for(const auto& material:directory.materials){auto& value=package.materials[material.id];value.source=material.source;value.resource=material.file;
      package.resources.emplace(value.resource,std::make_shared<const Resource>());materialSources.emplace(material.id,std::make_pair(catalog,material.id));}
    for(const auto& rule:directory.rules){package.rules[rule.id]={rule.parts,rule.targets,rule.skip};
      for(const auto& target:rule.targets)Require(package.targetIndex.emplace(target.second,rule.id).second,"native target belongs to multiple rules");}
    EiemAuthorControlMetadata::Apply(directory.runtime,package);catalog->BindEffects(package);
  }
  static std::shared_ptr<Author> Compose(const std::vector<std::shared_ptr<Author>>& authors) {
    Require(!authors.empty(),"native author composition is empty");auto result=std::make_shared<Author>(*authors.front());
    std::vector<Package> packages;packages.reserve(authors.size());for(const auto& author:authors)packages.push_back(author->package);
    // Material payloads are immutable native assets. Their content-addressed
    // CABs can be shared while each Renderer/Part keeps its explicit mapping.
    result->package=EiemComposedPackage::Compose(packages,true,false);result->catalog=authors.front()->catalog;result->catalogs.clear();
    result->bindings.clear();result->markers.clear();result->meshSources.clear();result->materialSources.clear();
    for(const auto& author:authors){const auto& mod=author->catalog->Directory().mod;const auto prefix=EiemComposedPackage::Prefix(mod);
      result->catalogs.push_back(author->catalog);
      for(const auto& source:author->materialSources)result->materialSources.emplace(prefix+source.first,source.second);
      for(const auto& source:author->meshSources){auto part=prefix+source.first;const auto& mesh=result->package.meshes.at(part);
        result->meshSources.emplace(part,source.second);}
    }
    return result;
  }
  std::shared_ptr<const A::Mesh> GetMesh(const std::string& part) {
    const auto& source=meshSources.at(part);auto asset=source.first->GetMesh(source.second);
    auto& declaration=package.meshes.at(part);declaration.vertices=asset->declaration.vertices;
    declaration.indices=asset->declaration.indices;declaration.bones=asset->declaration.bones;
    declaration.shapes=asset->declaration.shapes;bindings[declaration.resource]=asset->declaration.binding;
    markers[part]=asset->declaration.marker;return asset;
  }
  std::shared_ptr<const A::Material> GetMaterial(const std::string& material)const {
    const auto& source=materialSources.at(material);return source.first->GetMaterial(source.second);
  }
  std::shared_ptr<const A::Texture> FindTexture(const NativeReference& reference)const {
    for(const auto& source:catalogs)if(auto texture=source->FindTexture(reference))return texture;return {};
  }
  std::vector<EiemPhysicsInput::Document> PhysicsDocuments(const std::set<Identity>& relevant)const {
    std::vector<std::pair<std::string,EiemPhysicsInput::Document>> loaded;
    for(const auto& source:catalogs){const auto& mod=source->Directory().mod;for(const auto& input:source->GetPhysics(relevant)){
      loaded.emplace_back(mod,input->document);
    }}
    return ComposePhysicsDocuments(loaded,relevant);
  }
};
struct Carrier {
  P::Plan plan;
  std::vector<BuiltMeshPart> parts;
  size_t changedObjects=0,addedObjects=0;
};
inline Carrier StageCarrier(EiemNativeEditTransaction::Member& transaction,R::Reader& reader,size_t memberIndex,
    Author& author,const AssetIndex& targets) {
  const auto& member=reader.directory.members.at(memberIndex);const auto& layout=transaction.SourceLayout();Carrier result;
  Require(member.name==transaction.Cab(),"native carrier transaction belongs to another member");
  std::map<int64_t,P::RangePayload> changed;std::vector<P::AddedRanges> added;
  std::map<std::string,int64_t> shared;NativePreloadExpansion expansion;
  for(const auto& target:targets){auto selected=author.package.targetIndex.find(target.first);if(selected==author.package.targetIndex.end())continue;
    const auto& asset=target.second;if(asset.cab!=member.name||!layout.objects.count(asset.pathId))continue;
    const auto& object=layout.objects.at(asset.pathId);const auto& type=layout.types.at(object.type);Require(type.classId==43,"native carrier target class differs");
    const auto& rule=author.package.rules.at(selected->second);if(rule.skip)continue;
    std::vector<uint32_t> sourceHashes;
    if(std::any_of(type.nodes[0].children.begin(),type.nodes[0].children.end(),[&](size_t n){return type.nodes[n].name=="m_BoneNameHashes";})){
      auto fields=reader.Fields(member,layout,asset.pathId,{"m_BoneNameHashes"});sourceHashes=BoneHashes(type,fields.at("m_BoneNameHashes"));}
    bool first=true;
    for(const auto& part:rule.parts){auto mesh=author.GetMesh(part);auto previous=shared.find(part);int64_t id=0;
      if(previous!=shared.end())id=previous->second;
      else{auto payload=G::Compose(reader,memberIndex,layout,asset.pathId,mesh->object,{mesh->declaration.fields.begin(),mesh->declaration.fields.end()});
        id=first?asset.pathId:AllocateMeshId(member.name,author.package,target.first,part);
        if(first)Require(changed.emplace(id,std::move(payload)).second,"native Mesh replacement aliases another target");
        else{Require(!transaction.Layout().objects.count(id),"native Mesh added identity collides");added.push_back({id,object.type,std::move(payload)});}
        shared.emplace(part,id);
      }
      if(id!=asset.pathId)expansion[{member.name,asset.pathId}].push_back({member.name,id});
      result.parts.push_back({target.first,selected->second,part,member.name,asset.pathId,id,mesh->declaration.vertices,mesh->declaration.indices,sourceHashes});first=false;
    }
  }
  auto output=transaction.Layout();std::set<NativeReference> covered;
  if(!expansion.empty())for(auto id:output.order){const auto& type=output.types.at(output.objects.at(id).type);if(type.classId!=142)continue;
    auto bundle=transaction.Object(id);if(ExpandNativePreloads(type,bundle,output,member.name,expansion,covered)){
      auto bytes=std::make_shared<const Bytes>(EncodeValue(type,bundle));changed[id].pieces.push_back({0,bytes->size(),bytes,{}});}}
  Require(covered.size()==expansion.size(),"native Mesh carrier absent from preload closure");
  result.changedObjects=changed.size();result.addedObjects=added.size();transaction.StageRanges(output,std::move(changed),std::move(added));return result;
}
inline Carrier PrepareCarrier(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    Author& author,const AssetIndex& targets) {
  EiemNativeEditTransaction::Member transaction(reader,memberIndex,layout);
  auto result=StageCarrier(transaction,reader,memberIndex,author,targets);result.plan=transaction.Commit();return result;
}
struct Consumer {P::Plan plan;RendererInputBuild result;size_t physicsDocuments=0;};
inline Consumer StageConsumer(EiemNativeEditTransaction::Member& transaction,
    const Author& author,const AssetIndex& targets,const std::vector<BuiltMeshPart>& parts,
    const RendererMaterialReferences* materials=nullptr,const MaterialPreloadIndex* materialClosures=nullptr) {
  const auto& cab=transaction.Cab();Consumer result;
  std::map<NativeReference,Identity> identities;std::set<NativeReference> refs;
  for(const auto& target:targets)if(author.package.targetIndex.count(target.first)){auto ref=CanonicalSerializedAssetReference({target.second.cab,target.second.pathId});refs.insert(ref);identities.emplace(ref,target.first);}
  auto consumers=ProjectedMeshConsumers(transaction.Layout(),cab,refs,[&](int64_t id,const std::set<std::string>& fields){return transaction.Project(id,fields);},SerializedAssetCabKey);
  std::set<Identity> relevant;for(const auto& consumer:consumers)relevant.insert(identities.at(CanonicalSerializedAssetReference(consumer.mesh)));
  auto documents=author.PhysicsDocuments(relevant);result.physicsDocuments=documents.size();
  if(!documents.empty()){
    EiemPhysicsInput::MemberBuilder builder(transaction.Layout(),cab,author.package,targets,documents,
      [&](int64_t id,const std::set<std::string>& fields){return transaction.Project(id,fields);},[&](int64_t id){return transaction.Object(id);},&relevant);
    Require(builder.Build().empty(),"native Physics assembled a complete member");
    transaction.Stage(builder.OutputLayout(),builder.TakeChanges(),builder.TakeAdded(),builder.AddedTypes(),builder.AddedScripts());
  }
  RendererMemberBuilder builder(transaction.Layout(),cab,author.package,targets,parts,
    [&](int64_t id,const std::set<std::string>& fields){return transaction.Project(id,fields);},
    [&](int64_t id){return transaction.Object(id);});
  builder.SetAuthorMetadata(author.bindings);builder.SetAuthorMarkers(author.markers);
  if(materials)builder.SetMaterialReferences(*materials);else builder.UseSourceMaterials();
  if(materialClosures)builder.SetMaterialClosures(*materialClosures);
  result.result=builder.Build();transaction.Stage(builder.OutputLayout(),builder.TakeChanges(),builder.TakeAdded());
  result.result.changedObjects=transaction.ChangedObjects();result.result.addedObjects=transaction.AddedObjects();return result;
}
inline Consumer PrepareConsumer(R::Reader& reader,size_t memberIndex,const SerializedLayout& layout,
    const Author& author,const AssetIndex& targets,const std::vector<BuiltMeshPart>& parts,
    const RendererMaterialReferences* materials=nullptr,const MaterialPreloadIndex* materialClosures=nullptr) {
  EiemNativeEditTransaction::Member transaction(reader,memberIndex,layout);
  auto result=StageConsumer(transaction,author,targets,parts,materials,materialClosures);result.plan=transaction.Commit();return result;
}
} // namespace EiemDemandNativeAssembly
