#pragma once
#include "eiem_compiled_artifact_append.h"
#include "eiem_compiled_source_records.h"

// Retire/rebuild leaf Prefab inputs as one immutable union. Core-resource
// changes and inputs with retained dependants need a wider compilation batch;
// never silently retain their old dependency indices or author resources.
namespace EiemCompiledPrefabUpdate {
using namespace EiemCompiledMod;
inline Artifact Retire(const Artifact& previous,const std::set<std::string>& retired,bool allowEmptyRoots=false){
 if(retired.empty())return previous;
 const auto& old=previous.manifest.plan;Require(old.rulesOnly&&old.spanRecipes&&previous.manifest.rootAssembly,"Prefab retirement requires independent source proofs");
 auto owners=EiemCompiledSourceRecords::MemberInputs(previous);std::vector<InputProofDirectory> directories;
 std::set<std::string> found,removedMembers,removedBundles;std::vector<size_t> mapped(old.inputs.size(),SIZE_MAX),retained;
 for(size_t i=0;i<old.inputs.size();++i){directories.push_back(EiemCompiledSourceRecords::Directory(previous,i,owners));
  if(!retired.count(old.originals.at(i).file)){mapped[i]=retained.size();retained.push_back(i);continue;}
  found.insert(old.originals.at(i).file);Require(std::any_of(old.roots.begin(),old.roots.end(),[&](const auto& root){return root.input==i;}),"retirement source is not a compiled Prefab");
  HydrateInput(previous,i);for(const auto& name:directories.back().members){const auto& proof=previous.members.at(name);
   for(const auto& type:proof.before.types)Require(type.classId!=43&&type.classId!=21&&type.classId!=28&&type.classId!=48,"Prefab retirement contains core resources; compile affected dependency batch first");
   removedMembers.insert(name);}
  removedBundles.insert(old.inputs.at(i).bundle);
 }
 Require(found==retired&&(allowEmptyRoots||!retained.empty()),"retired Prefab identity absent or union loses all core inputs");
 for(auto i:retained)for(auto dependency:directories[i].dependencies)Require(dependency<mapped.size()&&mapped[dependency]!=SIZE_MAX,"retained input depends on retired Prefab; compile affected dependency batch first");
 Artifact result=previous;auto& p=result.manifest.plan;result.proofBlocks.reset();result.proofReader={};result.proofMutex=std::make_shared<std::mutex>();result.retainedProofBlocks.clear();result.retainedInputDirectories.clear();result.hydrated.clear();result.digest.clear();
 p.inputs.clear();p.originals.clear();p.startupHashes.clear();p.rendererRules.clear();p.semanticOperations.clear();p.semanticComplete.clear();p.memberRecipes.clear();p.namespaceRecipes.clear();p.inputDependencies.clear();p.closure.clear();p.roots.clear();result.pristine.clear();result.views.clear();result.inputMembers.clear();result.addedBundles.clear();result.verifiedSources.clear();
 auto reader=previous.proofReader;auto blocks=previous.proofBlocks;
 if(reader||blocks)result.proofReader=[reader=std::move(reader),blocks=std::move(blocks),retained](size_t i){auto oldIndex=retained.at(i);return reader?reader(oldIndex):blocks->at(oldIndex);};
 for(auto i:retained){auto directory=directories[i];for(auto& dependency:directory.dependencies)dependency=mapped.at(dependency);
  auto at=p.inputs.size();p.inputs.push_back(old.inputs.at(i));p.originals.push_back(old.originals.at(i));p.startupHashes.push_back(old.startupHashes.at(i));p.rendererRules.push_back(old.rendererRules.at(i));
  p.semanticOperations.push_back(EiemResourceReloadPlan::SemanticOperationsAt(old,i));p.semanticComplete.push_back(EiemResourceReloadPlan::SemanticCompleteAt(old,i));
  p.memberRecipes.push_back(old.memberRecipes.at(i));p.namespaceRecipes.push_back(old.namespaceRecipes.at(i));p.inputDependencies.push_back(directory.dependencies);result.views.push_back(previous.views.at(i));
  result.inputMembers.push_back(directory.members);result.addedBundles.push_back(directory.addedBundles);
  if(previous.verifiedSources.size()==old.inputs.size())result.verifiedSources.push_back(previous.verifiedSources.at(i));
  if(previous.retainedProofBlocks.count(i))result.retainedProofBlocks[at]=previous.retainedProofBlocks.at(i);
  else if(!previous.proofBlocks&&(!previous.proofReader||(previous.inputMembers.size()!=old.inputs.size()&&!previous.retainedInputDirectories.count(i))))
   result.retainedProofBlocks[at]=EiemCompiledSourceRecords::Proof(previous,i,directories[i]);
  if(result.proofReader||result.retainedProofBlocks.count(at))result.retainedInputDirectories[at]=directory;
  if(previous.hydrated.count(i)||(!previous.proofReader&&!previous.proofBlocks&&!previous.retainedProofBlocks.count(i)))result.hydrated.insert(at);
 }
 for(const auto& name:removedMembers){result.members.erase(name);p.shared.erase(name);}
 for(auto it=p.names.members.begin();it!=p.names.members.end();){bool removed=removedMembers.count(it->first)!=0;
  if(!removed)for(const auto& name:removedMembers)if(it->first.rfind(name+".",0)==0){removed=true;break;}
  if(removed)it=p.names.members.erase(it);else ++it;}
 for(const auto& name:removedBundles)p.names.bundles.erase(name);
 auto roots=[&](const std::vector<Root>& from,std::vector<Root>& to){for(auto root:from)if(mapped.at(root.input)!=SIZE_MAX){root.input=mapped.at(root.input);to.push_back(std::move(root));}};
 roots(old.roots,p.roots);roots(previous.pristine,result.pristine);for(auto i:old.closure)if(mapped.at(i)!=SIZE_MAX)p.closure.push_back(mapped.at(i));
 Writer key;key.S(p.author);key.S(p.contract);for(const auto& source:p.originals){key.S(source.file);key.S(source.hash);}p.key=Hash(key.b);result.manifest.baseline=p.key;
 p.rendererParts=EiemCompiledArtifactAppend::RendererPartCount(result);
 EiemCompiledArtifactAppend::PruneGenerated(result);
 Require((allowEmptyRoots||!p.roots.empty())&&result.pristine.size()==p.roots.size(),"Prefab retirement loses current root coverage");return result;
}
inline Artifact Replace(const Artifact& previous,const Artifact& delta,const std::set<std::string>& retired){
 auto retained=Retire(previous,retired);return EiemCompiledArtifactAppend::Merge(retained,delta);
}
inline Artifact ReplaceAuthorSources(const Artifact& previous,const Artifact& delta,const std::set<std::string>& replaced,
 const std::set<std::string>& changedResources,const Artifact* authorSnapshot=nullptr){
 auto retained=Retire(previous,replaced,true);
 if(authorSnapshot)for(const auto& frame:authorSnapshot->manifest.plan.frameRecipes){auto found=authorSnapshot->preparedFrames.find(frame.resource);
  Require(found!=authorSnapshot->preparedFrames.end(),"updated author frame bytes absent");retained.preparedFrames[frame.resource]=found->second;}
 auto result=EiemCompiledArtifactAppend::Merge(retained,delta,true,changedResources);std::set<std::string> members,bundles;
 for(const auto& input:result.manifest.plan.memberRecipes)for(const auto& member:input)members.insert(member.name);
 for(const auto& input:result.manifest.plan.inputs)if(!input.bundle.empty())bundles.insert(input.bundle);
 for(auto row=result.manifest.plan.names.members.begin();row!=result.manifest.plan.names.members.end();)if(!members.count(row->first))row=result.manifest.plan.names.members.erase(row);else ++row;
 for(auto row=result.manifest.plan.names.bundles.begin();row!=result.manifest.plan.names.bundles.end();)if(!bundles.count(row->first))row=result.manifest.plan.names.bundles.erase(row);else ++row;
 return result;
}
} // namespace EiemCompiledPrefabUpdate
