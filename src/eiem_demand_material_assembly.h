#pragma once
#include "eiem_demand_native_assembly.h"
#include "eiem_native_preload_closure.h"
#include "eiem_input_span_view.h"

// One current-source-derived Material variant is shared by every matched
// Renderer/Part that uses the same authored Material.
namespace EiemDemandMaterialAssembly {
using namespace EiemNativeInput;
namespace N=EiemDemandNativeAssembly;
namespace P=EiemNativeSerializedSplice;

inline bool SameInput(const EiemNativeStandaloneInput::Input& left,const EiemNativeStandaloneInput::Input& right){
  return left.cab==right.cab&&left.bundle==right.bundle&&left.path==right.path&&
    left.asset==right.asset&&left.origin==right.origin&&left.dependencies==right.dependencies&&left.view&&right.view&&
    (left.view==right.view||EiemInputSpanView::SameBytes(*left.view,*right.view));
}
struct Use {RendererMaterialKey key;std::string material;};
inline std::vector<Use> CollectUses(const N::Author& author,const RendererTargetMatches& matches) {
  std::vector<Use> result;
  for(const auto& consumer:matches.consumers){auto identity=matches.identities.find(CanonicalSerializedAssetReference(consumer.mesh));
    Require(identity!=matches.identities.end(),"native Material consumer Mesh target identity absent: "+consumer.mesh.first+"/"+std::to_string(consumer.mesh.second));
    auto ruleId=author.package.targetIndex.find(identity->second);
    Require(ruleId!=author.package.targetIndex.end(),"native Material consumer target has no author rule: "+identity->second.path+" / "+identity->second.asset);
    auto rule=author.package.rules.find(ruleId->second);
    Require(rule!=author.package.rules.end(),"native Material consumer author rule is absent: "+ruleId->second);
    if(rule->second.skip)continue;
    for(const auto& part:rule->second.parts){auto partEntry=author.package.parts.find(part);
      Require(partEntry!=author.package.parts.end(),"native Material consumer part is absent: "+part);
      const auto& material=partEntry->second.material;if(material.empty())continue;
      result.push_back({{consumer.renderer,part},material});}
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
inline Shared PrepareShared(const N::Author& author,const std::vector<Use>& uses,const PrepareInput& prepare={},
    const EiemDemandAuthorResources::TextureResolver& resolveTexture={}) {
  Shared result;std::map<std::string,NativeReference> built;
  for(const auto& use:uses){
    Require(use.key.first&&!use.key.second.empty()&&!result.references.count(use.key),"native Material Renderer/Part use is invalid or duplicated");
    auto found=built.find(use.material);
    if(found!=built.end()){
      result.references.emplace(use.key,found->second);
      continue;
    }
    Require(bool(prepare),"current game Material source builder is absent");
    auto material=author.GetMaterial(use.material,resolveTexture);auto input=prepare(*material,use);
    Require(input.asset.second&&!input.cab.empty()&&input.asset.first==input.cab&&input.view,
      "native author Material input is incomplete: "+use.material);
    auto previous=result.inputs.emplace(input.cab,input);
    Require(previous.second||SameInput(previous.first->second,input),
      "native author Material CAB collision");
    std::set<NativeReference> closure=input.dependencies;closure.insert(input.asset);auto expectedClosure=closure;
    auto prior=result.closures.emplace(input.asset,std::move(closure));
    Require(prior.second||prior.first->second==expectedClosure,
      "native author Material dependency closure differs");
    built.emplace(use.material,input.asset);
    result.references.emplace(use.key,input.asset);

    for(const auto& dependency:input.dependencies)if(auto texture=author.FindTexture(dependency)){
      if(!texture->input)continue; // Logical game Texture stays owned by the game.
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
    else Require(SameInput(prior->second,row.second),"native Material CAB collision across dependency groups");}
  for(auto& row:source.textures.references){auto prior=destination.textures.references.emplace(row.first,row.second);Require(prior.second||prior.first->second==row.second,"native Texture id resolves to multiple CAB/PathID pairs");}
  for(auto& row:source.textures.inputs){auto prior=destination.textures.inputs.emplace(row.first,row.second);Require(prior.second||prior.first->second==row.second,"native author Texture CAB collision across dependency groups");}
  for(auto& row:source.closures){auto prior=destination.closures.emplace(row.first,row.second);Require(prior.second||prior.first->second==row.second,"native Material closure differs across dependency groups");}
  destination.variants+=source.variants;destination.cacheHits+=source.cacheHits;
}
} // namespace EiemDemandMaterialAssembly
