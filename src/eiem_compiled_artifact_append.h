#pragma once
#include "eiem_compiled_mod.h"

// First incremental slice: append newly discovered sources to one Mod's
// immutable artifact. Existing sources must still match in content and output;
// replacement/retirement of changed sources is a separate batch operation.
namespace EiemCompiledArtifactAppend {
using namespace EiemCompiledMod;
inline size_t RendererPartCount(const Artifact& a){size_t count=0;const auto& package=a.manifest.package;
 for(const auto& input:a.manifest.plan.rendererRules)for(const auto& renderer:input)count=AddSize(count,package.rules.at(package.targetIndex.at(renderer.mesh)).parts.size());return count;
}
inline Bytes Startup(const Artifact& a,size_t i){
 const auto& p=a.manifest.plan;auto input=ReadSource({},p.originals.at(i));auto original=EndfieldInput::Decode(input);std::map<std::string,const Bytes*> members;
 for(const auto& member:original.members)Require(members.emplace(member.name,&member.bytes).second,"append startup original member repeated");std::vector<EndfieldInput::Member> output;
 for(const auto& recipe:p.memberRecipes.at(i))output.push_back({recipe.name,recipe.flags,EiemResourceRecipe::Apply(recipe,members,a.manifest.package,a.preparedFrames)});
 if(p.startupHashes.at(i)!=p.originals.at(i).hash)input=EndfieldInput::Encode(input,output);
 Require(Hash(input)==p.startupHashes.at(i),"append startup differs from offline validation");return input;
}
inline Artifact Merge(const Artifact& previous,const Artifact& delta){
 const auto& old=previous.manifest.plan;const auto& added=delta.manifest.plan;
 Require(previous.manifest.package.id==delta.manifest.package.id&&old.contract==added.contract&&old.author==added.author,"consumer delta belongs to another author contract");
 Require(previous.manifest.rootAssembly&&delta.manifest.rootAssembly&&old.rulesOnly&&added.rulesOnly&&old.spanRecipes&&added.spanRecipes,"consumer delta lacks independent input proofs");
 Artifact result=previous;auto& plan=result.manifest.plan;result.proofBlocks.reset();result.proofMutex=std::make_shared<std::mutex>();result.inputMembers.clear();result.addedBundles.clear();result.digest.clear();
 bool retainedStamps=previous.verifiedSources.size()==old.inputs.size();if(!retainedStamps)result.verifiedSources.clear();
 for(size_t i=0;i<old.inputs.size();++i){
  auto retained=previous.retainedInputDirectories.find(i);
  if(retained!=previous.retainedInputDirectories.end()){
   Require(previous.retainedProofBlocks.count(i)||previous.proofReader,"previous retained input directory lacks proof");result.retainedInputDirectories[i]=retained->second;
  }else if((previous.proofBlocks||previous.proofReader)&&previous.inputMembers.size()==old.inputs.size()&&previous.addedBundles.size()==old.inputs.size()&&old.inputDependencies.size()==old.inputs.size()){
   if(previous.proofBlocks)result.retainedProofBlocks[i]=previous.proofBlocks->at(i);result.retainedInputDirectories[i]={previous.inputMembers.at(i),previous.addedBundles.at(i),old.inputDependencies.at(i)};
  }else {HydrateInput(previous,i);plan.memberRecipes.at(i)=old.memberRecipes.at(i);plan.namespaceRecipes.at(i)=old.namespaceRecipes.at(i);
   if(previous.proofBlocks){result.retainedProofBlocks[i]=previous.proofBlocks->at(i);result.hydrated.insert(i);}
  }
 }
 result.members=previous.members;
 std::map<std::string,size_t> existing;for(size_t i=0;i<old.originals.size();++i)Require(existing.emplace(old.originals[i].file,i).second,"previous input duplicated");
 std::vector<size_t> mapped(added.inputs.size(),SIZE_MAX),fresh;
 for(size_t i=0;i<added.inputs.size();++i){auto found=existing.find(added.originals.at(i).file);
  if(found!=existing.end()){auto at=found->second;Require(old.originals.at(at).hash==added.originals.at(i).hash&&previous.views.at(at).startupHash==delta.views.at(i).startupHash&&previous.views.at(at).size==delta.views.at(i).size,"append delta modifies an existing source; use affected-source replacement");mapped[i]=at;continue;}
  auto at=plan.inputs.size();mapped[i]=at;fresh.push_back(at);existing.emplace(added.originals.at(i).file,at);HydrateInput(delta,i);
  plan.originals.push_back(added.originals.at(i));plan.inputs.push_back(added.inputs.at(i));plan.startupHashes.push_back(added.startupHashes.at(i));plan.rendererRules.push_back(added.rendererRules.at(i));
  plan.memberRecipes.push_back(added.memberRecipes.at(i));plan.namespaceRecipes.push_back(added.namespaceRecipes.at(i));plan.inputDependencies.emplace_back();result.views.push_back(delta.views.at(i));
  result.hydrated.insert(at);
  if(retainedStamps){const auto& source=added.originals.at(i);auto stamp=FileStamp(source.physical);
   if(delta.verifiedSources.size()!=added.inputs.size()||!SameStamp(stamp,delta.verifiedSources.at(i))){ReadSource({},source);Require(SameStamp(stamp,FileStamp(source.physical)),"new source changed while retaining append stamp");}result.verifiedSources.push_back(stamp);}
  for(const auto& member:delta.members){bool belongs=std::any_of(added.memberRecipes.at(i).begin(),added.memberRecipes.at(i).end(),[&](const auto& recipe){return recipe.name==member.first;});if(belongs)Require(result.members.emplace(member).second,"new source repeats an existing CAB");}
 }
 for(const auto& row:added.names.members)plan.names.members.emplace(row);for(const auto& row:added.names.bundles)plan.names.bundles.emplace(row);
 for(const auto& frame:delta.preparedFrames){auto found=result.preparedFrames.find(frame.first);Require(found==result.preparedFrames.end()||found->second==frame.second,"delta author frame changed");result.preparedFrames.emplace(frame);}
 for(const auto& frame:added.frameRecipes)if(std::none_of(plan.frameRecipes.begin(),plan.frameRecipes.end(),[&](const auto& row){return row.resource==frame.resource;}))plan.frameRecipes.push_back(frame);
 std::map<std::string,size_t> memberInputs;for(size_t i=0;i<plan.memberRecipes.size();++i){auto directory=result.retainedInputDirectories.find(i);
  if(directory!=result.retainedInputDirectories.end()){for(const auto& name:directory->second.members)Require(memberInputs.emplace(name,i).second,"retained CAB belongs to multiple sources");}
  else for(const auto& recipe:plan.memberRecipes.at(i))Require(memberInputs.emplace(recipe.name,i).second,"appended CAB belongs to multiple sources");}
 for(auto i:fresh){std::set<size_t> dependencies;auto add=[&](const std::string& member){auto owner=memberInputs.find(member);if(owner!=memberInputs.end()&&owner->second!=i)dependencies.insert(owner->second);};
  for(const auto& external:plan.inputs.at(i).externals)add(external);for(const auto& recipe:plan.memberRecipes.at(i)){auto proof=result.members.find(recipe.name);if(proof!=result.members.end())for(const auto& external:proof->second.after.externals)add(external);}
  plan.inputDependencies.at(i).assign(dependencies.begin(),dependencies.end());
 }
 auto roots=[&](const std::vector<Root>& source,std::vector<Root>& destination){for(const auto& root:source){auto at=mapped.at(root.input);auto found=std::find_if(destination.begin(),destination.end(),[&](const auto& row){return row.path==root.path;});
   if(found!=destination.end()){Require(found->input==at&&found->cab==root.cab&&found->source==root.source,"delta root conflicts with existing Prefab identity");continue;}auto copy=root;copy.input=at;destination.push_back(std::move(copy));}};
 roots(added.roots,plan.roots);roots(delta.pristine,result.pristine);
 plan.shared.insert(added.shared.begin(),added.shared.end());for(const auto& row:added.parts)plan.parts.emplace(row);for(const auto& row:added.materials)plan.materials.emplace(row);
 std::set<size_t> closure(plan.closure.begin(),plan.closure.end());for(auto i:added.closure)closure.insert(mapped.at(i));plan.closure.assign(closure.begin(),closure.end());
 EiemOfflinePackage::Writer key;key.S(plan.author);key.S(plan.contract);for(const auto& source:plan.originals){key.S(source.file);key.S(source.hash);}plan.key=Hash(key.b);result.manifest.baseline=plan.key;
 // Existing CAB names/proofs are retained. Only new inputs' references to
 // shared existing CABs need remapping to the retained names and a new digest.
 for(auto i:fresh){for(auto& recipe:plan.namespaceRecipes.at(i))for(auto& patch:recipe.patches)patch.after=EiemCompiledInputView::Remap(patch.before,plan.names);
  // Shared frames were already verified/prepared by the author compiler.
  // Never expand every author's mesh frame again to replay one new PFB.
  auto startup=Startup(result,i);auto bundle=EndfieldInput::Decode(startup);
  for(auto& member:bundle.members){if(Serialized(member.bytes)){auto recipe=std::find_if(plan.namespaceRecipes.at(i).begin(),plan.namespaceRecipes.at(i).end(),[&](const auto& row){return row.member==member.name;});Require(recipe!=plan.namespaceRecipes.at(i).end(),"new source namespace recipe absent");member.bytes=EiemLoadingGeneration::ApplyProven(member.bytes,recipe->hash,recipe->patches);}member.name=plan.names.members.at(member.name);}
  plan.inputs.at(i).hash=Hash(EndfieldInput::Encode(startup,bundle.members));
 }
 plan.rendererParts=RendererPartCount(result);Require(result.pristine.size()==plan.roots.size(),"appended Prefab proof coverage differs");return result;
}
} // namespace EiemCompiledArtifactAppend
