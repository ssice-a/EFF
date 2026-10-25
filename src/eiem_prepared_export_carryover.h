#pragma once
#include "eiem_prepared_prefab_compiler.h"
#include "eiem_compiled_artifact_append.h"
#include "eiem_compiled_export_carryover.h"

// Catch up an already compiled staging snapshot after the game appended PFBs.
// Existing staged views use the new author; only new candidate sources compile.
namespace EiemPreparedExportCarryover {
using namespace EiemCompiledMod;
inline bool SameRouting(const EiemStaticSources::Selection& left,const EiemStaticSources::Selection& right){
 if(left.assets!=right.assets||left.dependencies!=right.dependencies||left.candidates!=right.candidates||left.inputs.size()!=right.inputs.size())return false;
 for(const auto& row:left.inputs){auto found=right.inputs.find(row.first);if(found==right.inputs.end())return false;const auto& before=row.second;const auto& after=found->second;
  if(before.revision!=after.revision||before.physical!=after.physical||before.offset!=after.offset||before.size!=after.size)return false;}
 return true;
}
// A repeated export normally has no new runtime discovery. Inspect the two
// small indexes and routing records before loading every compiled source view.
// Exact three-way routing is deliberate: partial inherited ranges still need
// Merge's source verification, and a complete index may remove old entries.
inline bool TryUnchanged(const std::filesystem::path& mod,const std::filesystem::path& previous,const std::filesystem::path& game,const Package& author,
 const EiemStaticSources::Selection& current){
 EiemModUpdateStore::File staged(mod/"compiled.bin",false),published(previous/"compiled.bin",false);
 auto stage=EiemModUpdateStore::Read(staged),prior=EiemModUpdateStore::Read(published);
 const char* required[]={"discovery/source-selection","discovery/decisions","discovery/catalog","runtime/order","runtime/common","runtime/author-seal"};
 for(auto key:required)if(!stage.blocks.count(key)||!prior.blocks.count(key))return false;
 if(stage.blocks.count("runtime/resource-bindings")&&!EiemAddressedResources::CurrentBindings(EiemModUpdateStore::Get(staged,stage,"runtime/resource-bindings")))return false;
 auto sealBytes=EiemModUpdateStore::Get(staged,stage,"runtime/author-seal");EiemOfflinePackage::Reader seal{sealBytes};if(seal.S()!=PackageDigest(author))return false;
 for(auto key:{"discovery/decisions","discovery/catalog","runtime/order"})if(stage.blocks.at(key).digest!=prior.blocks.at(key).digest)return false;
 auto selected=EiemCompiledModStore::ReadSourceSelection(EiemModUpdateStore::Get(staged,stage,"discovery/source-selection"),game);
 auto inherited=EiemCompiledModStore::ReadSourceSelection(EiemModUpdateStore::Get(published,prior,"discovery/source-selection"),game);
 EiemStaticSources::Validate(current);if(!SameRouting(current,selected)||!SameRouting(selected,inherited))return false;
 auto orderBytes=EiemModUpdateStore::Get(staged,stage,"runtime/order");EiemOfflinePackage::Reader order{orderBytes};
 Require(order.S()=="EFF_COMPILED_SOURCE_ORDER/identity/le64","source store identity order structure differs");auto count=order.Count();std::set<std::string> represented;
 for(size_t i=0;i<count;++i){auto identity=order.S();Require(represented.insert(identity).second&&stage.blocks.count("runtime/source/"+identity)&&stage.blocks.count("runtime/proof/"+identity)&&prior.blocks.count("runtime/source/"+identity)&&prior.blocks.count("runtime/proof/"+identity),"source store identity order incomplete");}
 Require(order.p==orderBytes.size(),"source store identity order suffix differs");
 auto decisions=EiemConsumerUpdateRecords::Decode(EiemModUpdateStore::Get(staged,stage,"discovery/decisions"));
 for(const auto& candidate:selected.candidates)if(!decisions.count(candidate)&&!represented.count(EiemStaticSources::SourceFile(candidate,selected.inputs.at(candidate).revision)))return false;
 auto common=EiemModUpdateStore::Get(staged,stage,"runtime/common");std::vector<std::string> closure;
 auto verified=EiemCompiledSourceRecords::ReadCommon(common,mod,closure,&author);if(verified.manifest.plan.author!=PackageDigest(author))return false;
 auto priorCommon=EiemCompiledSourceRecords::CommonBytes(EiemModUpdateStore::Get(published,prior,"runtime/common"));EiemOfflinePackage::Reader fields{priorCommon};
 Require(fields.S()=="EFF_COMPILED_SOURCE_COMMON/author+metadata+identity-edges/le64","source-record common structure differs");
 for(size_t i=0;i<4;++i)fields.S();Require(fields.S()==author.id,"concurrent discoveries belong to another Mod");
 selected.manifest=current.manifest;auto sourceBytes=EiemCompiledModStore::SourceSelection(selected,game);
 if(Hash(sourceBytes)!=stage.blocks.at("discovery/source-selection").digest)
  EiemCompiledModStore::PublishSnapshot(mod/"compiled.bin",mod/"compiled.bin",stage.generation,{{"discovery/source-selection",sourceBytes}});
 selected.manifest=EiemStaticSources::LoadMod(mod,game).manifest;sourceBytes=EiemCompiledModStore::SourceSelection(selected,game);
 if(sourceBytes!=EiemNativeInput::Read(mod/"source-inputs.bin"))EiemOfflineCompile::AtomicSave(mod/"source-inputs.bin",sourceBytes);
 return true;
}
inline bool Try(const std::filesystem::path& mod,const std::filesystem::path& previous,const std::filesystem::path& game,const Package& author,
 const EiemStaticSources::Selection& current,const std::function<void(const char*,double)>& cost={},bool currentSelectionComplete=false){
 if(previous.empty()||std::filesystem::weakly_canonical(mod)==std::filesystem::weakly_canonical(previous))return false;
 auto path=mod/"compiled.bin",publishedPath=previous/"compiled.bin";
 if(!std::filesystem::is_regular_file(path)||!std::filesystem::is_regular_file(publishedPath)||!EiemCompiledModStore::IsStore(path)||!EiemCompiledModStore::IsStore(publishedPath)||std::filesystem::hard_link_count(path)!=1)return false;
 {EiemModUpdateStore::File file(path,false);auto snapshot=EiemModUpdateStore::Read(file);if(!snapshot.external)return false;
  if(snapshot.blocks.count("runtime/resource-bindings")&&!EiemAddressedResources::CurrentBindings(EiemModUpdateStore::Get(file,snapshot,"runtime/resource-bindings")))return false;
  auto bytes=EiemModUpdateStore::Get(file,snapshot,"runtime/author-seal");EiemOfflinePackage::Reader seal{bytes};if(seal.S()!=PackageDigest(author))return false;}
 if(EiemResourceReloadPlan::Measure(cost,"export-catchup-metadata",[&]{return TryUnchanged(mod,previous,game,author,current);}))return true;
 auto stage=EiemCompiledModStore::Load(path,mod,game,true);if(!stage.selection||stage.artifact.manifest.plan.author!=PackageDigest(author)||!SharedInputsPruned(stage.artifact))return false;
 auto published=EiemCompiledModStore::Load(publishedPath,previous,game,true);if(!published.selection)return false;
 Require(author.id==published.artifact.manifest.package.id,"concurrent discoveries belong to another Mod");
 auto selected=EiemCompiledExportCarryover::Merge(current,*published.selection,published.artifact,currentSelectionComplete);
 // Core or already known source changes need the ordinary affected-input path.
 for(const auto& row:stage.selection->inputs){auto next=selected.inputs.find(row.first);if(next==selected.inputs.end()||next->second.revision!=row.second.revision)return false;
  auto oldDeps=stage.selection->dependencies.find(row.first),newDeps=selected.dependencies.find(row.first);if((oldDeps==stage.selection->dependencies.end())!=(newDeps==selected.dependencies.end())||(oldDeps!=stage.selection->dependencies.end()&&oldDeps->second!=newDeps->second))return false;}
 for(const auto& row:stage.selection->assets){auto next=selected.assets.find(row.first);if(next==selected.assets.end()||next->second!=row.second)return false;}
 std::map<std::string,const EiemStaticSources::Entry*> ranges;
 for(const auto& row:selected.inputs)ranges.emplace(EiemStaticSources::SourceFile(row.first,row.second.revision),&row.second);
 std::set<std::string> affected;
 for(const auto& source:stage.artifact.manifest.plan.originals){auto found=ranges.find(source.file);Require(found!=ranges.end(),"catchup source missing its verified logical identity");const auto& entry=*found->second;
  if(source.physical!=entry.physical||source.offset!=entry.offset||source.size!=entry.size)affected.insert(source.file);}
 EiemCompiledExportCarryover::RebindSources(stage.artifact,selected);
 auto publishSelection=[&]{auto descriptorSelection=selected;descriptorSelection.manifest=EiemStaticSources::LoadMod(mod,game).manifest;auto bytes=EiemCompiledModStore::SourceSelection(descriptorSelection,game);if(bytes!=EiemNativeInput::Read(mod/"source-inputs.bin"))EiemOfflineCompile::AtomicSave(mod/"source-inputs.bin",bytes);};
 std::set<std::string> represented;for(const auto& source:stage.artifact.manifest.plan.originals)represented.insert(source.file);
 std::set<std::string> pending;for(const auto& candidate:selected.candidates){auto identity=candidate+'\n'+selected.inputs.at(candidate).revision;auto file=Hash(Bytes(identity.begin(),identity.end()))+".vfs";
  if(!stage.selection->candidates.count(candidate)||(!stage.decisions.count(candidate)&&!represented.count(file)))pending.insert(candidate);}
 if(pending.empty()){
  EiemCompiledModStore::SaveAffected(path,stage.generation,stage.artifact,game,affected,{},stage.decisions,EiemCompiledExportCarryover::Catalog(selected),nullptr,&selected);
  publishSelection();return true;
 }
 EiemModUpdateStore::File backing(path,false);auto snapshot=EiemModUpdateStore::Read(backing);if(!snapshot.blocks.count("runtime/prepared-pfb"))return false;
 auto descriptor=EiemModUpdateStore::Get(backing,snapshot,"runtime/prepared-pfb");
 {auto plain=Unpack(descriptor);EiemOfflinePackage::Reader fields{plain};if(fields.S()!=EiemPreparedPrefabCompiler::descriptorStructure)return false;} // Regenerate obsolete descriptors with the full author compiler.
 auto prepared=EiemPreparedPrefabCompiler::Decode(descriptor,std::make_shared<const Artifact>(stage.artifact),selected);
 auto artifact=std::move(stage.artifact);
 for(const auto& source:pending){auto result=EiemResourceReloadPlan::Measure(cost,"export-catchup-pfb",[&]{return EiemPreparedPrefabCompiler::Compile(prepared,{source,EiemSourceAdaptationCompiler::StampFor(author,selected,source)});});
  stage.decisions[source]=result.decision;if(result.artifact){for(const auto& input:result.artifact->manifest.plan.originals)affected.insert(input.file);artifact=EiemCompiledArtifactAppend::Merge(artifact,*result.artifact);}}
 EiemCompiledModStore::SaveAffected(path,stage.generation,artifact,game,affected,{},stage.decisions,EiemCompiledExportCarryover::Catalog(selected),nullptr,&selected);
 publishSelection();return true;
}
} // namespace EiemPreparedExportCarryover
