#pragma once
#include "eiem_bundle_dependency_graph.h"
#include "eiem_offline_resource_package.h"

// Reads author-compiled byte recipes and applies their proven relocations to
// pristine resource input before the game creates objects. No resource builders.
namespace EiemPrefabRules {
using namespace EiemResourceReloadPlan;
struct Context {
 std::shared_ptr<const Plan> plan;Package package;
 std::function<Bytes(size_t,bool)> compiledMaterializer;
 std::shared_ptr<const EiemBundleDependencies::Names> addedDependencies;
 std::map<size_t,std::shared_ptr<const Bytes>> startupInputs;
 bool startupInputsPacked=false;
 Context(std::shared_ptr<const Plan> p,Package author,std::function<Bytes(size_t,bool)> materializer):plan(std::move(p)),package(std::move(author)),compiledMaterializer(std::move(materializer)){}
 std::mutex buildMutex;std::map<std::string,Bytes> frameBytes;
 Context(std::shared_ptr<const Plan> p,Package author):plan(std::move(p)),package(std::move(author)){
  Require(plan&&plan->rulesOnly&&plan->originals.size()==plan->inputs.size(),"runtime rules source coverage differs");
  if(plan->spanRecipes){Require(plan->memberRecipes.size()==plan->inputs.size(),"runtime member recipe coverage differs");for(const auto& frame:plan->frameRecipes)Require(frameBytes.emplace(frame.resource,EiemResourceRecipe::ExpandFrame(frame,package.resources.at(frame.resource)->bytes)).second,"duplicate runtime frame recipe");return;}
  Require(plan->inputs.empty(),"replacement rules lack compiled member recipes; re-export the Mod with Blender");
 }
 Bytes Materialize(size_t index,bool isolated){
  if(compiledMaterializer)return compiledMaterializer(index,isolated);
  std::lock_guard<std::mutex> lock(buildMutex);const auto& source=plan->originals.at(index);auto bytes=ReadSource({},source);
  auto original=EndfieldInput::Decode(bytes);
  if(plan->spanRecipes){std::map<std::string,const Bytes*> members;for(const auto& member:original.members)Require(members.emplace(member.name,&member.bytes).second,"duplicate runtime recipe original member");std::vector<EndfieldInput::Member> output;
   for(const auto& recipe:plan->memberRecipes.at(index))output.push_back({recipe.name,recipe.flags,EiemResourceRecipe::Apply(recipe,members,package,frameBytes)});
   if(plan->startupHashes.at(index)!=source.hash)bytes=EndfieldInput::Encode(bytes,output);
  }
  Require(Hash(bytes)==plan->startupHashes.at(index),"runtime applied rules differ from offline validation: "+source.file);
  if(!isolated)return bytes;
  auto bundle=EndfieldInput::Decode(bytes);size_t recipesUsed=0;
  for(auto& member:bundle.members){if(Serialized(member.bytes)){
   if(plan->namespaceRecipes.empty())member.bytes=EiemLoadingGeneration::PrepareSerialized(member.bytes,plan->names).bytes;
   else{const auto& recipes=plan->namespaceRecipes.at(index);auto recipe=std::find_if(recipes.begin(),recipes.end(),[&](const auto& r){return r.member==member.name;});
    Require(recipe!=recipes.end(),"namespace recipe source member absent");
    member.bytes=EiemLoadingGeneration::ApplyProven(member.bytes,recipe->hash,recipe->patches);++recipesUsed;}
  }member.name=plan->names.members.at(member.name);}
  Require(plan->namespaceRecipes.empty()||recipesUsed==plan->namespaceRecipes.at(index).size(),"namespace recipe serialized member coverage differs");
  bytes=EndfieldInput::Encode(bytes,bundle.members);Require(Hash(bytes)==plan->inputs.at(index).hash,"runtime isolated rules differ from offline validation");return bytes;
 }
};
} // namespace EiemPrefabRules
