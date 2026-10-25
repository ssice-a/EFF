// Exercise new UI/NPC sources through explicit bindings and the shared schema
// dictionary. All writes stay in an isolated diagnostic store.
#include "../../src/eiem_prepared_prefab_compiler.h"
#include "../../src/eiem_compiled_prefab_update.h"
#include "../../src/eiem_published_compiled_mod.h"
#include "../../src/eiem_prepared_export_carryover.h"
#include <iostream>
using namespace EiemCompiledMod;
namespace fs=std::filesystem;
int wmain(int argc,wchar_t** argv){try{
 Require(argc==3||(argc==4&&std::wstring(argv[3])==L"--legacy"),"prepared_source_store_smoke <game> <explicit-Mod> [--legacy]");auto game=fs::canonical(argv[1]),mod=fs::canonical(argv[2]);bool legacy=argc==4;
 auto initial=EiemPublishedCompiledMod::Read(mod,game);Require(legacy||(initial.store&&initial.artifact.explicitResources&&initial.selection),"fixture requires an explicit source store");
 auto selection=initial.selection?*initial.selection:EiemStaticSources::LoadMod(mod,game);std::map<std::string,std::string> logical;
 for(const auto& row:selection.inputs)logical.emplace(EiemStaticSources::SourceFile(row.first,row.second.revision),row.first);
 auto& original=initial.artifact;const auto& p=original.manifest.plan;size_t leaf=SIZE_MAX;
 for(const auto& root:p.roots){auto i=root.input;bool incoming=false;for(const auto& dependencies:p.inputDependencies)incoming=incoming||std::find(dependencies.begin(),dependencies.end(),i)!=dependencies.end();
  if(incoming||!selection.candidates.count(logical.at(p.originals.at(i).file)))continue;
  HydrateInput(original,i);bool core=false;for(const auto& recipe:p.memberRecipes.at(i)){auto proof=original.members.find(recipe.name);if(proof!=original.members.end())for(const auto& type:proof->second.before.types)core=core||type.classId==43||type.classId==21||type.classId==28||type.classId==48;}
  if(!core){leaf=i;break;}
 }
 Require(leaf!=SIZE_MAX,"fixture lacks a leaf Prefab");auto identity=p.originals.at(leaf).file,source=logical.at(identity);
 auto clean=EiemPublishedCompiledMod::Read(mod,game);auto missing=EiemCompiledPrefabUpdate::Retire(clean.artifact,{identity});
 auto prepared=EiemPreparedPrefabCompiler::Prepare(std::make_shared<const Artifact>(missing),selection);
 auto descriptor=EiemPreparedPrefabCompiler::Encode(prepared);auto restored=EiemPreparedPrefabCompiler::Decode(descriptor,std::make_shared<const Artifact>(missing),selection);
 Require(prepared.markers==restored.markers,"prepared descriptor changed template Mesh markers");for(const auto& row:prepared.markers)Require(original.manifest.plan.parts.count(row.second),"prepared source did not retain the baseline Mesh marker");
 auto result=EiemPreparedPrefabCompiler::Compile(restored,{source,EiemSourceAdaptationCompiler::StampFor(missing.manifest.package,selection,source)});
 if(legacy){Require(result.artifact&&result.artifact->views.size()==1&&result.artifact->explicitResources==original.explicitResources,"legacy prepared source changed resource binding mode");
  Require(EiemCompiledArtifactAppend::Startup(*result.artifact,0)==EiemCompiledArtifactAppend::Startup(original,leaf),"legacy prepared source bytes differ from the published template");
  std::cout<<"PASS legacy prepared source: store="<<initial.store<<" explicit="<<original.explicitResources<<" templateMarkersPreserved=1 descriptorRoundtrip=1 exactNativeReplay=1 gameModModified=0\n";return 0;}
 Require(result.artifact&&result.artifact->explicitResources&&result.artifact->views.size()==1,"new Prefab lost explicit provenance");auto delta=*result.artifact;
 Require(EiemCompiledArtifactAppend::Startup(delta,0)==EiemCompiledArtifactAppend::Startup(original,leaf),"prepared source bytes differ from cold compile");
 // Cache the delta proof against a deliberately different dictionary order.
 // Appending must decode it and re-encode only this source with the old prefix.
 std::reverse(delta.sharedSchemas.begin(),delta.sharedSchemas.end());auto owners=EiemCompiledSourceRecords::MemberInputs(delta);auto directory=EiemCompiledSourceRecords::Directory(delta,0,owners);
 auto packed=EiemCompiledSourceRecords::Proof(delta,0,directory);delta.proofBlocks=std::make_shared<const std::vector<Bytes>>(std::vector<Bytes>{packed});
 delta.members.clear();delta.hydrated.clear();delta.inputMembers={directory.members};delta.addedBundles={directory.addedBundles};
 auto temporary=fs::temp_directory_path()/("eff-prepared-source-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(fs::create_directory(temporary),"diagnostic directory exists");
 struct Cleanup{fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}}cleanup{temporary};auto path=temporary/"compiled.bin";
 auto catalog=EiemCompiledExportCarryover::Catalog(selection);auto first=EiemCompiledModStore::Save(path,0,missing,game,{},catalog,&descriptor,&selection);
 auto pinned=EiemCompiledModStore::Load(path,mod,game,true);auto reader=pinned.artifact.proofReader;size_t reads=0;pinned.artifact.proofReader=[&](size_t i){++reads;return reader(i);};
 auto prefix=pinned.artifact.sharedSchemas;auto merged=EiemCompiledArtifactAppend::Merge(pinned.artifact,delta);
 Require(reads==0,"append hydrated unchanged proofs");Require(merged.explicitResources&&merged.sharedSchemas.size()>=prefix.size(),"append lost resource/schema metadata");
 for(size_t i=0;i<prefix.size();++i)Require(EiemCompiledSourceRecords::SameSchema(prefix[i],merged.sharedSchemas[i]),"append moved an existing schema ID");
 EiemConsumerUpdateRecords::Decisions decisions{{source,result.decision}};EiemCompiledModStore::WriteStats stats;
 auto next=EiemCompiledModStore::SaveAffected(path,first.generation,merged,game,{identity},{},decisions,catalog,&stats,&selection);
 Require(stats.sourcesEncoded==1&&stats.proofsEncoded==1&&reads==0,"source append rewrote or read unrelated proofs");
 for(const auto& row:first.blocks)if(row.first.rfind("runtime/source/",0)==0||row.first.rfind("runtime/proof/",0)==0)Require(next.blocks.at(row.first).digest==row.second.digest,"append rewrote an old source/proof");
 auto loaded=EiemCompiledModStore::Load(path,mod,game,true);size_t added=SIZE_MAX;for(size_t i=0;i<loaded.artifact.views.size();++i)if(loaded.artifact.manifest.plan.originals[i].file==identity)added=i;
 Require(added!=SIZE_MAX&&loaded.artifact.views.size()==original.views.size(),"restart omitted the new Prefab");HydrateInput(loaded.artifact,added);
 Require(EiemCompiledArtifactAppend::Startup(loaded.artifact,added)==EiemCompiledArtifactAppend::Startup(original,leaf),"dictionary-prefix source replay differs after restart");
 auto oldReader=EiemCompiledModStore::Load(path,mod,game,true);auto retired=EiemCompiledPrefabUpdate::Retire(oldReader.artifact,{identity});
 auto removed=EiemCompiledModStore::SaveAffected(path,next.generation,retired,game,{}, {identity},{},catalog,nullptr,&selection);
 Require(!removed.blocks.count("runtime/source/"+identity)&&!removed.blocks.count("runtime/proof/"+identity),"retirement retained the old source");
 HydrateInput(oldReader.artifact,added);Require(EiemCompiledArtifactAppend::Startup(oldReader.artifact,added)==EiemCompiledArtifactAppend::Startup(original,leaf),"retirement invalidated a pinned old snapshot");
 auto replacement=EiemCompiledPrefabUpdate::Replace(retired,delta,{});auto final=EiemCompiledModStore::SaveAffected(path,removed.generation,replacement,game,{identity},{},decisions,catalog,nullptr,&selection);
 auto restarted=EiemCompiledModStore::Load(path,mod,game,true);Require(restarted.artifact.views.size()==original.views.size()&&restarted.generation==final.generation,"retire/replace cycle lost source coverage");
 // A verified complete index owns every routing set, even if all input byte
 // revisions stay the same. Do not union removed metadata back from runtime.
 auto narrowed=selection;narrowed.assets.erase(narrowed.assets.begin());narrowed.candidates.erase(source);narrowed.dependencies.erase(narrowed.dependencies.begin());
 auto authoritative=EiemCompiledExportCarryover::Merge(narrowed,selection,original,true);
 Require(EiemPreparedExportCarryover::SameRouting(authoritative,narrowed),"complete index retained removed assets/candidates/dependencies");
 auto metaPrevious=temporary/"meta-previous",metaStage=temporary/"meta-stage";fs::copy(mod,metaPrevious,fs::copy_options::recursive);
 // Exercise the real cold writer: its first publication must retain positive
 // and negative discovery conclusions rather than rescan them at next export.
 fs::remove(metaPrevious/"compiled.bin");EiemCompiledModWriter::Export(metaPrevious,game);
 auto metaCompiled=EiemCompiledModStore::Load(metaPrevious/"compiled.bin",metaPrevious,game,true);Require(metaCompiled.selection.has_value(),"cold export omitted source selection");auto metaSelection=*metaCompiled.selection;
 for(const auto& candidate:metaSelection.candidates)Require(metaCompiled.decisions.count(candidate),"cold export discarded a completed candidate decision");
 fs::copy(metaPrevious,metaStage,fs::copy_options::recursive);
 auto metaAuthor=EiemDirectory::Load(metaStage);auto beforeMeta=EiemModUpdateStore::Read(metaStage/"compiled.bin");std::map<std::string,double> costs;
 Require(EiemPreparedExportCarryover::Try(metaStage,metaPrevious,game,metaAuthor,metaSelection,[&](const char* name,double ms){costs[name]+=ms;}),"unchanged export lost metadata-only catchup");
 auto afterMeta=EiemModUpdateStore::Read(metaStage/"compiled.bin");Require(beforeMeta.generation==afterMeta.generation&&costs.count("export-catchup-metadata")&&!costs.count("export-catchup-pfb"),"unchanged export unnecessarily rewrote compiled records");
 auto moved=metaSelection;++moved.inputs.begin()->second.offset;Require(!EiemPreparedExportCarryover::TryUnchanged(metaStage,metaPrevious,game,metaAuthor,moved),"metadata fast path ignored a source location change");
 Require(!EiemPreparedExportCarryover::TryUnchanged(metaStage,metaPrevious,game,metaAuthor,narrowed),"metadata fast path ignored complete index removal");
 auto metaSnapshot=EiemModUpdateStore::Read(metaPrevious/"compiled.bin");auto changedDecision=metaCompiled.decisions;if(changedDecision.count(source))changedDecision.erase(source);else changedDecision.emplace(source,result.decision);
 EiemCompiledModStore::PublishSnapshot(metaPrevious/"compiled.bin",metaPrevious/"compiled.bin",metaSnapshot.generation,{{"discovery/decisions",EiemConsumerUpdateRecords::Encode(changedDecision)}});
 Require(!EiemPreparedExportCarryover::TryUnchanged(metaStage,metaPrevious,game,metaAuthor,metaSelection),"metadata fast path discarded a runtime discovery conclusion");
 // Change one represented source's location while keeping its byte revision.
 // A slow rebind must not publish only Common and leave the old source range.
 // The deliberately shifted range must instead be verified and rejected.
 auto metaLocation=temporary/"meta-location";fs::copy(metaStage,metaLocation,fs::copy_options::recursive);
 auto descriptorSelection=EiemStaticSources::LoadMod(metaLocation,game);auto firstSource=metaCompiled.artifact.manifest.plan.originals.front().file;
 auto represented=std::find_if(descriptorSelection.inputs.begin(),descriptorSelection.inputs.end(),[&](const auto& row){return EiemStaticSources::SourceFile(row.first,row.second.revision)==firstSource;});Require(represented!=descriptorSelection.inputs.end(),"location fixture lost the represented source");++represented->second.offset;
 EiemOfflineCompile::AtomicSave(metaLocation/"source-inputs.bin",EiemCompiledModStore::SourceSelection(descriptorSelection,game));auto priorRoot=EiemNativeInput::Read(metaLocation/"compiled.bin");bool rejectedLocation=false;
 try{EiemCompiledModWriter::Export(metaLocation,game,{},metaStage);}catch(const std::exception& error){auto text=std::string(error.what());rejectedLocation=text.find("static source payload differs")!=text.npos||text.find("selected game source range invalid")!=text.npos;}
 Require(rejectedLocation&&EiemNativeInput::Read(metaLocation/"compiled.bin")==priorRoot,"location-only export published stale source records without verifying the new range");
 std::cout<<"PASS explicit prepared source store: inputs="<<original.views.size()<<" oldProofReadsByAppend="<<reads<<" sourcesEncoded="<<stats.sourcesEncoded<<" schemaPrefixStable=1 deltaDictionaryRebased=1 exactNativeReplay=1 retirementAndReplacement=1 completeIndexRemoval=1 unchangedCatchupMs="<<costs["export-catchup-metadata"]<<" metadataGuards=1 locationExportVerified=1 gameModModified=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
