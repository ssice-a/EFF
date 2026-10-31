#pragma once
#include "eiem_demand_native_assembly.h"
#include "eiem_native_preload_closure.h"

// One current-source-derived Material variant is shared by every matched
// Renderer/Part that uses the same authored Material.
namespace EiemDemandMaterialAssembly {
using namespace EiemNativeInput;
namespace N=EiemDemandNativeAssembly;
namespace P=EiemNativeSerializedSplice;

struct Use {RendererMaterialKey key;std::string material;};
template<class Project> inline std::vector<Use> CollectUses(const SerializedLayout& layout,const std::string& cab,
    const N::Author& author,const AssetIndex& targets,const Project& project) {
  std::map<NativeReference,Identity> identities;std::set<NativeReference> selected;
  for(const auto& target:targets)if(author.package.targetIndex.count(target.first)){
    auto ref=CanonicalSerializedAssetReference({target.second.cab,target.second.pathId});selected.insert(ref);identities.emplace(ref,target.first);}
  auto consumers=ProjectedMeshConsumers(layout,cab,selected,project,SerializedAssetCabKey);std::vector<Use> result;
  for(const auto& consumer:consumers){auto identity=identities.find(CanonicalSerializedAssetReference(consumer.mesh));
    Require(identity!=identities.end(),"native Material consumer Mesh target identity absent: "+consumer.mesh.first+"/"+std::to_string(consumer.mesh.second));
    auto ruleId=author.package.targetIndex.find(identity->second);
    Require(ruleId!=author.package.targetIndex.end(),"native Material consumer target has no author rule: "+identity->second.path+" / "+identity->second.asset);
    auto rule=author.package.rules.find(ruleId->second);
    Require(rule!=author.package.rules.end(),"native Material consumer author rule is absent: "+ruleId->second);
    if(rule->second.skip)continue;
    for(const auto& part:rule->second.parts){auto partEntry=author.package.parts.find(part);
      Require(partEntry!=author.package.parts.end(),"native Material consumer part is absent: "+part);
      const auto& material=partEntry->second.material;Require(!material.empty(),"native Material consumer Part has no assigned Material");
      (void)author.GetMaterial(material);result.push_back({{consumer.renderer,part},material});}
  }return result;
}
struct Textures {
  std::map<std::string,NativeReference> references;
  std::map<std::string,std::shared_ptr<const EiemDemandAuthorResources::Texture>> inputs;
};
struct Shared {
  RendererMaterialReferences references;
  std::map<std::string,EiemNativeStandaloneInput::Input> inputs;
  Textures textures;
  MaterialPreloadIndex closures;
  size_t variants=0,cacheHits=0;
};
using PrepareInput=std::function<EiemNativeStandaloneInput::Input(const EiemDemandAuthorResources::Material&,const Use&)>;
inline Shared PrepareShared(const N::Author& author,const std::vector<Use>& uses,const PrepareInput& prepare={}) {
  Shared result;std::set<RendererMaterialKey> keys;std::map<std::string,EiemNativeStandaloneInput::Input> built;
  for(const auto& use:uses){
    Require(use.key.first&&!use.key.second.empty()&&keys.insert(use.key).second,"native Material Renderer/Part use is invalid or duplicated");
    auto material=author.GetMaterial(use.material);auto found=built.find(use.material);
    EiemNativeStandaloneInput::Input input;
    if(found==built.end()){Require(bool(prepare),"current game Material source builder is absent");input=prepare(*material,use);}
    else input=found->second;
    if(found==built.end())built.emplace(use.material,input);
    Require(input.asset.second&&!input.cab.empty()&&input.asset.first==input.cab&&input.view,
      "native author Material input is incomplete: "+use.material);
    auto previous=result.inputs.emplace(input.cab,input);
    Require(previous.second||(previous.first->second.asset==input.asset&&previous.first->second.view==input.view),
      "native author Material CAB collision");
    std::set<NativeReference> closure=input.dependencies;closure.insert(input.asset);auto expectedClosure=closure;
    auto prior=result.closures.emplace(input.asset,std::move(closure));
    Require(prior.second||prior.first->second==expectedClosure,
      "native author Material dependency closure differs");
    Require(result.references.emplace(use.key,input.asset).second,"native Material Renderer/Part use was assembled more than once");
    ++result.variants;

    for(const auto& dependency:input.dependencies)if(auto texture=author.FindTexture(dependency)){
      auto inserted=result.textures.inputs.emplace(texture->cab,texture);
      Require(inserted.second||inserted.first->second==texture,"native author Texture CAB collision");
      result.textures.references.emplace(texture->cab+":"+std::to_string(texture->id),dependency);
    }
  }
  result.variants=result.inputs.size();
  return result;
}
inline void Merge(Shared& destination,Shared source) {
  for(auto& row:source.references){auto prior=destination.references.emplace(row.first,row.second);Require(prior.second||prior.first->second==row.second,"native Material Renderer/Part use resolves to multiple assets");}
  for(auto& row:source.inputs){auto prior=destination.inputs.find(row.first);if(prior==destination.inputs.end())destination.inputs.emplace(row.first,row.second);
    else Require(prior->second.asset==row.second.asset&&prior->second.view==row.second.view,"native Material CAB collision across dependency groups");}
  for(auto& row:source.textures.references){auto prior=destination.textures.references.emplace(row.first,row.second);Require(prior.second||prior.first->second==row.second,"native Texture id resolves to multiple CAB/PathID pairs");}
  for(auto& row:source.textures.inputs){auto prior=destination.textures.inputs.emplace(row.first,row.second);Require(prior.second||prior.first->second==row.second,"native author Texture CAB collision across dependency groups");}
  for(auto& row:source.closures){auto prior=destination.closures.emplace(row.first,row.second);Require(prior.second||prior.first->second==row.second,"native Material closure differs across dependency groups");}
  destination.variants+=source.variants;destination.cacheHits+=source.cacheHits;
}
} // namespace EiemDemandMaterialAssembly
