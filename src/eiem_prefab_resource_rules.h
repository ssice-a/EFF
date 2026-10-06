#pragma once
#include "eiem_offline_resource_package.h"

// Persistent data contains addresses and author-file references only. A game
// loading request reads its pristine source range and applies the same checked
// serialized builders used to validate the rules offline. No native setters.
namespace EiemPrefabRules {
using namespace EiemResourceReloadPlan;
struct Context {
 std::shared_ptr<const Plan> plan;Package package;AssetIndex assets;MeshSchemaIndex schemas;
 std::vector<BuiltMeshPart> meshes;std::map<NativeReference,std::string> markers;
 std::map<NativeReference,Identity> targets;
 RendererBoundsCache bounds;std::mutex buildMutex;std::map<std::string,Bytes> frameBytes;
 Context(std::shared_ptr<const Plan> p,Package author):plan(std::move(p)),package(std::move(author)){
  Require(plan&&plan->rulesOnly&&plan->originals.size()==plan->inputs.size(),"runtime rules source coverage differs");
  if(plan->spanRecipes){Require(plan->memberRecipes.size()==plan->inputs.size(),"runtime member recipe coverage differs");for(const auto& frame:plan->frameRecipes)Require(frameBytes.emplace(frame.resource,EiemResourceRecipe::ExpandFrame(frame,package.resources.at(frame.resource)->bytes)).second,"duplicate runtime frame recipe");return;}
  std::vector<Bytes> templates;for(const auto& source:plan->originals){auto bytes=ReadSource({},source);AddAssets(assets,bytes);AddMeshSchemas(schemas,bytes);templates.push_back(std::move(bytes));}
  ResolveTextureTemplates(package,templates,assets);
  for(const auto& target:package.targetIndex){const auto& asset=assets.at(target.first);targets.emplace(NativeReference{asset.cab,asset.pathId},target.first);const auto& rule=package.rules.at(target.second);if(rule.skip)continue;
   for(size_t i=0;i<rule.parts.size();++i){const auto& id=rule.parts[i];const auto& mesh=package.meshes.at(package.parts.at(id).mesh);auto native=i?AllocateMeshId(asset.cab,package,target.first,id):asset.pathId;
    meshes.push_back({target.first,target.second,id,asset.cab,asset.pathId,native,mesh.vertices,mesh.indices});markers.emplace(NativeReference{asset.cab,native},RuntimePartName(package,id,target.first));}
  }
 }
 Bytes Materialize(size_t index,bool isolated){
  // The cache contains only small bounds data; complete Prefabs and original
  // dependencies are never persisted or retained as another resource copy.
  std::lock_guard<std::mutex> lock(buildMutex);const auto& source=plan->originals.at(index);auto bytes=ReadSource({},source);
  auto original=EndfieldInput::Decode(bytes);
  if(plan->spanRecipes){std::map<std::string,const Bytes*> members;for(const auto& member:original.members)Require(members.emplace(member.name,&member.bytes).second,"duplicate runtime recipe original member");std::vector<EndfieldInput::Member> output;
   for(const auto& recipe:plan->memberRecipes.at(index))output.push_back({recipe.name,recipe.flags,EiemResourceRecipe::Apply(recipe,members,package,frameBytes)});
   if(plan->startupHashes.at(index)!=source.hash)bytes=EndfieldInput::Encode(bytes,output);
  }else{
  std::map<std::pair<std::string,int64_t>,Identity> expected;for(const auto& rule:plan->rendererRules.at(index))Require(expected.emplace(std::make_pair(rule.cab,rule.id),rule.mesh).second,"duplicate rule Renderer address");
  size_t checked=0;for(const auto& member:original.members)if(Serialized(member.bytes)){auto l=ParseSerialized(member.bytes);
   for(auto id:l.order){const auto& t=l.types.at(l.objects.at(id).type);if(t.classId!=137)continue;auto value=ObjectValue(member.bytes,l,id);auto ref=NullableReference(t,Child(t,value,"m_Mesh"),l,member.name);auto selected=targets.find(ref);if(selected==targets.end())continue;
    auto rule=expected.find({member.name,id});Require(rule!=expected.end()&&rule->second.path==selected->second.path&&rule->second.asset==selected->second.asset,"rule Renderer Mesh address/coverage differs");++checked;
   }
  }Require(checked==expected.size(),"rule Renderer anchor absent");
  auto mesh=BuildMeshInput(bytes,package);auto renderer=BuildRendererInput(mesh.bytes,package,assets,meshes,schemas,&bounds);size_t count=0;auto named=Mark(renderer.bytes,markers,count);auto texture=BuildTextureInput(named,package,assets);bytes=std::move(texture.bytes);
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
