// Differential proof of single-consumer compilation. Reads the author's
// known source selection; never publishes a game Mod or changes its files.
#include "../../src/eiem_source_adaptation_compiler.h"
#include "../../src/eiem_prepared_prefab_compiler.h"
#include "../../src/eiem_compiled_artifact_append.h"
#include "../../src/eiem_compiled_mod_store.h"
#include "../../src/eiem_compiled_prefab_update.h"
#include <iostream>
using namespace EiemCompiledMod;
int wmain(int argc,wchar_t** argv){try{
 Require(argc==4,"compile_consumer_delta <game> <Mod> <logical-source>");auto game=std::filesystem::canonical(argv[1]),mod=std::filesystem::canonical(argv[2]);auto logical=std::filesystem::path(argv[3]).u8string();
 auto sourceArtifact=Read(mod,game);auto package=Load(mod);auto selection=EiemStaticSources::LoadMod(mod,game);Require(selection.candidates.count(logical),"selected source is not a known consumer candidate");
 auto stamp=EiemSourceAdaptationCompiler::StampFor(package,selection,logical);auto started=std::chrono::steady_clock::now();
 EiemSourceAdaptationJobs::Queue jobs([&](const EiemSourceAdaptationJobs::Request& request){return EiemSourceAdaptationCompiler::Compile(package,selection,game,request);});
 auto ticket=jobs.Submit({logical,stamp});auto result=EiemSourceAdaptationJobs::BeforeNativeLoad(ticket,stamp);
 auto compileMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 if(result.decision.roots.empty()){Require(!result.artifact&&!result.view,"negative delta compiled a replacement");std::cout<<"PASS negative source conclusion; no compilation required\n";return 0;}
 // Both author export and the background adapter use this in-process CPU
 // compiler. No executable, Unity API or published artifact is required.
 const auto& artifact=*result.artifact;
 const auto& entry=selection.inputs.at(logical);auto identity=logical+"\n"+entry.revision;auto file=Hash(Bytes(identity.begin(),identity.end()))+".vfs";
 auto find=[&](const Artifact& a){for(size_t i=0;i<a.manifest.plan.originals.size();++i)if(a.manifest.plan.originals[i].file==file)return i;throw std::runtime_error("consumer delta input absent");};
 auto before=find(sourceArtifact),after=find(artifact);Require(sourceArtifact.views.at(before).startupHash==artifact.views.at(after).startupHash&&sourceArtifact.views.at(before).size==artifact.views.at(after).size,"single-source compile differs from full compilation");
 auto proofOverride=ReadRuntime(mod,game);proofOverride.retainedProofBlocks[before]=proofOverride.proofBlocks->at(before);proofOverride.proofBlocks.reset();
 proofOverride.proofReader=[](size_t)->Bytes{throw std::runtime_error("old backing reader must not serve independently retained replacement proof");};
 HydrateInput(proofOverride,before);Require(proofOverride.hydrated.count(before),"explicit retained source proof was shadowed by old backing reader");
 // Simulate a source not present at author-export time, retaining every other
 // compiled source exactly. Reintroduce it through the actual delta merger.
 auto missing=ReadRuntime(mod,game);auto& incomplete=missing.manifest.plan;Require(missing.members.empty()&&missing.hydrated.empty(),"runtime fixture eagerly expanded old input proofs");
 auto compactProofs=std::make_shared<std::vector<Bytes>>(*missing.proofBlocks);compactProofs->erase(compactProofs->begin()+before);missing.proofBlocks=std::move(compactProofs);
 auto erase=[&](auto& values){values.erase(values.begin()+before);};erase(incomplete.originals);erase(incomplete.inputs);erase(incomplete.startupHashes);erase(incomplete.rendererRules);erase(incomplete.memberRecipes);erase(incomplete.namespaceRecipes);erase(incomplete.inputDependencies);erase(missing.views);erase(missing.inputMembers);erase(missing.addedBundles);erase(missing.verifiedSources);
 for(auto& dependencies:incomplete.inputDependencies){Require(std::find(dependencies.begin(),dependencies.end(),before)==dependencies.end(),"fixture source has incoming dependencies; select a leaf Prefab");for(auto& dependency:dependencies)if(dependency>before)--dependency;}
 auto trimRoots=[&](std::vector<Root>& values){values.erase(std::remove_if(values.begin(),values.end(),[&](const auto& root){return root.input==before;}),values.end());for(auto& root:values)if(root.input>before)--root.input;};trimRoots(incomplete.roots);trimRoots(missing.pristine);
 incomplete.closure.erase(std::remove(incomplete.closure.begin(),incomplete.closure.end(),before),incomplete.closure.end());for(auto& input:incomplete.closure)if(input>before)--input;
 // Shared descriptors are prepared separately; the measured fast slice reads
 // and builds only the candidate PFB, never Mesh/Material/Texture bundles.
 auto preparationStart=std::chrono::steady_clock::now();auto baselineForFast=std::make_shared<const Artifact>(missing);
 auto prepared=EiemPreparedPrefabCompiler::Prepare(baselineForFast,selection);
 auto preparationMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-preparationStart).count();
 auto sharedDescriptor=EiemPreparedPrefabCompiler::Encode(prepared);auto restoredPreparation=EiemPreparedPrefabCompiler::Decode(sharedDescriptor,baselineForFast,selection);
 std::map<std::string,double> fastCosts;auto fastStart=std::chrono::steady_clock::now();auto fast=EiemPreparedPrefabCompiler::Compile(restoredPreparation,{logical,stamp},[&](const char* name,double ms){fastCosts[name]+=ms;});
 auto fastMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-fastStart).count();
 for(const auto& row:fastCosts)std::cout<<"PREPARED-PFB-COST "<<row.first<<' '<<row.second<<'\n';
 Require(fast.artifact&&fast.artifact->views.size()==1&&fast.view->startupHash==sourceArtifact.views.at(before).startupHash&&fast.view->size==sourceArtifact.views.at(before).size,"prepared PFB-only slice differs from full offline native view");
 auto staleSelection=selection;staleSelection.inputs.at(restoredPreparation.coreInputs.begin()->first).revision=Hash(Bytes{8,7,6});bool staleRejected=false;
 try{EiemPreparedPrefabCompiler::Decode(sharedDescriptor,baselineForFast,staleSelection);}catch(const std::exception&){staleRejected=true;}Require(staleRejected,"prepared descriptor accepted changed core resources");
 auto mergeStart=std::chrono::steady_clock::now();auto merged=EiemCompiledArtifactAppend::Merge(missing,artifact);Require(merged.views.size()==sourceArtifact.views.size(),"append changed complete source count");
 auto encoded=Encode(merged,game);auto mergeEncodeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-mergeStart).count();
 Require(missing.members.empty()&&missing.hydrated.empty(),"appending one source expanded an unchanged old proof");
 auto repeated=EiemCompiledArtifactAppend::Merge(merged,artifact);
 Require(Encode(repeated,game)==encoded&&repeated.hydrated==merged.hydrated,"repeated append changed bytes or expanded retained old proofs");
 auto rebuiltProofs=Decode(encoded,mod,game,{},&package);Require(Encode(rebuiltProofs,game)==encoded,"retained proofs/directories differ from full proof re-encoding");
 // Retention is lazy, not inaccessible: later shared-Mod composition or a
 // pristine replay can still expand one old input directly from its block.
 auto deferred=merged;auto oldIndex=size_t(0);HydrateInput(deferred,oldIndex);
 auto fullIndex=oldIndex>=before?oldIndex+1:oldIndex;
 Require(deferred.hydrated.count(oldIndex)&&deferred.manifest.plan.memberRecipes.at(oldIndex).size()==sourceArtifact.manifest.plan.memberRecipes.at(fullIndex).size(),"retained old input could not be hydrated on demand");
 auto restored=Decode(encoded,mod,game,{},&package);restored.retainedProofBlocks=merged.retainedProofBlocks;auto packetReplay=EiemPrefabRules::Context(std::make_shared<const Plan>(restored.manifest.plan),package);
 for(size_t i=0;i<sourceArtifact.views.size();++i){auto source=sourceArtifact.manifest.plan.originals.at(i).file;size_t index=SIZE_MAX;for(size_t j=0;j<restored.views.size();++j)if(restored.manifest.plan.originals.at(j).file==source){index=j;break;}
  Require(index!=SIZE_MAX&&restored.views.at(index).startupHash==sourceArtifact.views.at(i).startupHash&&restored.views.at(index).size==sourceArtifact.views.at(i).size,"appended union changed a native startup view");
 }
 auto mergedIndex=find(restored);Require(Hash(packetReplay.Materialize(mergedIndex,true))==restored.manifest.plan.inputs.at(mergedIndex).hash,"appended source namespace replay differs");
 for(const auto& row:missing.manifest.plan.names.members)Require(restored.manifest.plan.names.members.at(row.first)==row.second,"append renamed a previous CAB");
 auto temporary=std::filesystem::temp_directory_path()/("eff-consumer-append-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(temporary),"append-store fixture exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{temporary};auto store=temporary/"compiled.bin";
 EiemResourceCatalogDelta::Catalog catalog;catalog.domain="fixture-current-game";for(const auto& source:missing.manifest.plan.originals)catalog.entries.emplace(source.file,EiemResourceCatalogDelta::Entry{source.hash,source.physical.u8string()+":"+std::to_string(source.offset),{}});
 auto first=EiemCompiledModStore::Save(store,0,missing,game,{},catalog,&sharedDescriptor);catalog.entries.emplace(file,EiemResourceCatalogDelta::Entry{entry.revision,entry.physical.u8string()+":"+std::to_string(entry.offset),{}});
 auto pinned=EiemCompiledModStore::Load(store,mod,game,true);Require(pinned.artifact.hydrated.empty()&&pinned.artifact.members.empty(),"source store startup expanded proofs");
 auto oldProofReader=pinned.artifact.proofReader;size_t proofReads=0;pinned.artifact.proofReader=[&](size_t i){++proofReads;return oldProofReader(i);};
 EiemConsumerUpdateRecords::Decisions completed{{logical,result.decision}};EiemCompiledModStore::WriteStats writes;auto granularStart=std::chrono::steady_clock::now();
 auto granularMerged=EiemCompiledArtifactAppend::Merge(pinned.artifact,*fast.artifact);
 auto committed=EiemCompiledModStore::SaveAffected(store,first.generation,granularMerged,game,{file},{},completed,catalog,&writes);
 auto granularMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-granularStart).count();
 Require(writes.sourcesEncoded==1&&writes.proofsEncoded==1&&writes.framesEncoded==0&&proofReads==0,"granular append encoded/read old sources or shared author frames");size_t retained=0;
 for(const auto& row:first.blocks)if(row.first.rfind("runtime/",0)==0&&row.first!="runtime/common"&&row.first!="runtime/order"){Require(committed.blocks.at(row.first).offset==row.second.offset,"appending one PFB rewrote an existing compiled block");if(row.first.rfind("runtime/proof/",0)==0)++retained;}
 auto repeatedCommit=EiemCompiledModStore::SaveAffected(store,committed.generation,granularMerged,game,{file},{},completed,catalog);
 Require(repeatedCommit.generation==committed.generation,"repeated granular append created a generation");
 auto restarted=EiemCompiledModStore::Load(store,mod,game,true);Require(restarted.artifact.views.size()==sourceArtifact.views.size()&&EiemConsumerUpdateRecords::Pending({{logical,stamp}},restarted.decisions).empty(),"restart repeated appended PFB classification");
 Require(Encode(restarted.artifact,game)==Encode(granularMerged,game),"granular source records changed prepared slice semantics");
 for(const auto& source:sourceArtifact.manifest.plan.originals){auto expected=size_t(&source-sourceArtifact.manifest.plan.originals.data());size_t observed=SIZE_MAX;
  for(size_t i=0;i<restarted.artifact.views.size();++i)if(restarted.artifact.manifest.plan.originals[i].file==source.file){observed=i;break;}
  Require(observed!=SIZE_MAX&&restarted.artifact.views[observed].startupHash==sourceArtifact.views[expected].startupHash&&restarted.artifact.views[observed].size==sourceArtifact.views[expected].size,"prepared source-record union changed offline native output");}
 auto preparedIndex=find(restarted.artifact);HydrateInput(restarted.artifact,preparedIndex);
 Require(EiemCompiledArtifactAppend::Startup(restarted.artifact,preparedIndex)==EiemCompiledArtifactAppend::Startup(sourceArtifact,before),"range/literal PFB proof differs from cold compiled member replay");
 // A known PFB may stop matching, or be replaced after an update. Retire the
 // whole old source record before adding its replacement, keeping every old
 // unrelated source/proof block and its native namespace unchanged.
 auto pristineReader=EiemCompiledModStore::Load(store,mod,game,true);size_t retirementReads=0;auto retirementReader=pristineReader.artifact.proofReader;
 pristineReader.artifact.proofReader=[&](size_t i){++retirementReads;return retirementReader(i);};
 auto retiredUnion=EiemCompiledPrefabUpdate::Retire(pristineReader.artifact,{file});Require(retiredUnion.views.size()+1==sourceArtifact.views.size()&&retirementReads==1,"retiring one Prefab read unrelated old proofs");
 Require(retiredUnion.manifest.plan.rendererParts<pristineReader.artifact.manifest.plan.rendererParts,"retirement left stale Renderer part metadata");
 auto retiredCommit=EiemCompiledModStore::SaveAffected(store,repeatedCommit.generation,retiredUnion,game,{}, {file},{},catalog);
 Require(!retiredCommit.blocks.count("runtime/source/"+file)&&!retiredCommit.blocks.count("runtime/proof/"+file),"retirement retained stale compiled records");
 for(const auto& row:first.blocks)if(row.first.rfind("runtime/source/",0)==0||row.first.rfind("runtime/proof/",0)==0)
  Require(retiredCommit.blocks.at(row.first).offset==row.second.offset,"leaf retirement rewrote an unrelated source/proof");
 auto afterRetirement=EiemCompiledModStore::Load(store,mod,game,true);auto replacementUnion=EiemCompiledPrefabUpdate::Replace(afterRetirement.artifact,*fast.artifact,{});
 EiemCompiledModStore::WriteStats replacementWrites;auto replacementCommit=EiemCompiledModStore::SaveAffected(store,retiredCommit.generation,replacementUnion,game,{file},{},completed,catalog,&replacementWrites);
 Require(replacementWrites.sourcesEncoded==1&&replacementWrites.proofsEncoded==1&&!replacementWrites.framesEncoded,"leaf replacement rewrote shared author frames or unrelated sources");
 auto afterReplacement=EiemCompiledModStore::Load(store,mod,game,true);Require(Encode(afterReplacement.artifact,game)==Encode(granularMerged,game),"retire/replacement cycle changed runtime views or proofs");
 auto directReplacement=EiemCompiledPrefabUpdate::Replace(pristineReader.artifact,*fast.artifact,{file});
 Require(Encode(directReplacement,game)==Encode(granularMerged,game),"atomic leaf replacement differs from original prepared union");
 auto unsafeRetirement=pristineReader.artifact;unsafeRetirement.manifest.plan.inputDependencies.at(0).push_back(find(unsafeRetirement));bool dependentRejected=false;
 try{EiemCompiledPrefabUpdate::Retire(unsafeRetirement,{file});}catch(const std::exception&){dependentRejected=true;}Require(dependentRejected,"leaf retirement retained an incoming dependency");
 // Change actual source bytes in the diagnostic directory, without touching
 // the game archives. The root's display name does not determine Mesh hits.
 auto changedBundle=EndfieldInput::Decode(EiemStaticSources::Payload(entry));const auto& changedRoot=fast.decision.roots.front();
 auto rootMember=std::find_if(changedBundle.members.begin(),changedBundle.members.end(),[&](const auto& member){return member.name==changedRoot.cab;});Require(rootMember!=changedBundle.members.end(),"changed source fixture root absent");
 auto rootLayout=ParseSerialized(rootMember->bytes);auto rootValue=ObjectValue(rootMember->bytes,rootLayout,changedRoot.id);const auto& rootType=rootLayout.types.at(rootLayout.objects.at(changedRoot.id).type);
 SetString(rootType,Child(rootType,rootValue,"m_Name"),Name(rootType,rootValue)+"_updated_fixture");rootMember->bytes=BuildSerialized(rootMember->bytes,rootLayout,{{changedRoot.id,EncodeValue(rootType,rootValue)}},{});
 auto changedBytes=EndfieldInput::Encode(EiemStaticSources::Payload(entry),changedBundle.members);auto changedPath=temporary/"changed-source.vfs";EiemOfflineCompile::AtomicSave(changedPath,changedBytes);
 auto changedSelection=selection;auto& changedEntry=changedSelection.inputs.at(logical);changedEntry.revision=Hash(changedBytes);changedEntry.physical=changedPath;changedEntry.offset=0;changedEntry.size=changedBytes.size();
 auto changedPrepared=EiemPreparedPrefabCompiler::Decode(sharedDescriptor,baselineForFast,changedSelection);auto changedStamp=EiemSourceAdaptationCompiler::StampFor(package,changedSelection,logical);
 bool staleSourceRejected=false;try{EiemPreparedPrefabCompiler::Compile(changedPrepared,{logical,stamp});}catch(const std::exception&){staleSourceRejected=true;}Require(staleSourceRejected,"changed source accepted stale compilation request");
 auto changedResult=EiemPreparedPrefabCompiler::Compile(changedPrepared,{logical,changedStamp});Require(changedResult.artifact&&changedResult.view->sourceHash==changedEntry.revision&&changedResult.view->startupHash!=fast.view->startupHash,"changed source did not generate a fresh compiled view");
 auto changedUnion=EiemCompiledPrefabUpdate::Replace(pristineReader.artifact,*changedResult.artifact,{file});Require(changedUnion.views.size()==sourceArtifact.views.size(),"changed-byte replacement retained old source revision");
 for(size_t i=0;i<changedUnion.views.size();++i)Require(changedUnion.manifest.plan.originals[i].file!=file,"changed source kept its retired compiled identity");
 for(size_t i=0;i<retiredUnion.views.size();++i)Require(changedUnion.views[i].startupHash==retiredUnion.views[i].startupHash,"changed PFB rebuilt an unrelated native view");
 // Remove every exact Mesh reference. A previous positive must become a
 // completed negative and retire its old replacement, without core rebuild.
 auto negativeBundle=EndfieldInput::Decode(changedBytes);
 for(auto& member:negativeBundle.members)if(Serialized(member.bytes)){auto layout=ParseSerialized(member.bytes);auto matches=MeshConsumers(member.bytes,layout,member.name,changedPrepared.targets,EiemStaticSources::Cab);std::map<int64_t,Bytes> edits;
  for(const auto& match:matches){auto id=match.filter?match.filter:match.renderer;auto value=ObjectValue(member.bytes,layout,id);const auto& type=layout.types.at(layout.objects.at(id).type);SetReference(type,Child(type,value,"m_Mesh"),{member.name,0},layout,member.name);edits[id]=EncodeValue(type,value);}
  if(!edits.empty())member.bytes=BuildSerialized(member.bytes,layout,edits,{});}
 auto negativeBytes=EndfieldInput::Encode(changedBytes,negativeBundle.members);auto negativePath=temporary/"negative-source.vfs";EiemOfflineCompile::AtomicSave(negativePath,negativeBytes);auto negativeSelection=selection;
 auto& negativeEntry=negativeSelection.inputs.at(logical);negativeEntry.revision=Hash(negativeBytes);negativeEntry.physical=negativePath;negativeEntry.offset=0;negativeEntry.size=negativeBytes.size();
 auto negativePrepared=EiemPreparedPrefabCompiler::Decode(sharedDescriptor,baselineForFast,negativeSelection);auto negativeResult=EiemPreparedPrefabCompiler::Compile(negativePrepared,{logical,EiemSourceAdaptationCompiler::StampFor(package,negativeSelection,logical)});
 Require(negativeResult.decision.roots.empty()&&!negativeResult.artifact&&!negativeResult.view,"formerly positive source kept a replacement after Mesh references disappeared");
 std::cout<<"PASS actual revised source bytes: freshStampAndView=1 retiredRevisionAbsent=1 staleRequestRejected=1 positiveToNegative=1 gameArchivesModified=0\n";
 {EiemModUpdateStore::File persisted(store,false);auto snapshot=EiemModUpdateStore::Read(persisted);Require(EiemModUpdateStore::Get(persisted,snapshot,"runtime/prepared-pfb")==sharedDescriptor,"source append rewrote offline shared compiler descriptors");}
 std::set<std::string> roots,observedRoots;for(const auto& root:artifact.manifest.plan.roots)roots.insert(root.path);for(const auto& root:result.decision.roots)observedRoots.insert(root.path);Require(roots==observedRoots,"delta compiled an unrelated Prefab root");
 for(const auto& root:sourceArtifact.manifest.plan.roots)if(root.input==before)Require(roots.count(root.path),"delta omitted a same-source Prefab root");
 auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
 std::cout<<"PASS consumer delta: roots="<<roots.size()<<" selectedInputs="<<artifact.views.size()<<" fullInputs="<<sourceArtifact.views.size()<<" sourceBytes="<<entry.size<<" decisionAndCompileMs="<<compileMs<<" sharedPreparationMs="<<preparationMs<<" preparedPfbOnlyMs="<<fastMs<<" legacyAppendAndEncodeMs="<<mergeEncodeMs<<" granularAppendAndCommitMs="<<granularMs<<" granularSourcesEncoded="<<writes.sourcesEncoded<<" granularBytesPrepared="<<writes.bytesPrepared<<" regressionWallMs="<<elapsed<<" preservedProofBlocks="<<retained<<" oldProofReadsByAppend=0 sharedFramesEncodedByAppend=0 exactStartupViewDigest=1 unrelatedPrefabCompilation=0 singleFileAppend=1 restartSkipsContentDecision=1 leafRetirementAndReplacement=1 incomingDependencyRetirementRejected=1 authorFilesModified=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
