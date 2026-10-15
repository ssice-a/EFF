#pragma once
#include "eiem_prepared_prefab_compiler.h"
#include "eiem_compiled_artifact_append.h"
#include "eiem_compiled_export_carryover.h"

// Catch up an already compiled staging snapshot after the game appended PFBs.
// Existing staged views use the new author; only new candidate sources compile.
namespace EiemPreparedExportCarryover {
using namespace EiemCompiledMod;
inline bool Try(const std::filesystem::path& mod,const std::filesystem::path& previous,const std::filesystem::path& game,const Package& author,
 const EiemStaticSources::Selection& current,const std::function<void(const char*,double)>& cost={},bool currentSelectionComplete=false){
 if(previous.empty()||std::filesystem::weakly_canonical(mod)==std::filesystem::weakly_canonical(previous))return false;
 auto path=mod/"compiled.bin",publishedPath=previous/"compiled.bin";
 if(!std::filesystem::is_regular_file(path)||!std::filesystem::is_regular_file(publishedPath)||!EiemCompiledModStore::IsStore(path)||!EiemCompiledModStore::IsStore(publishedPath)||std::filesystem::hard_link_count(path)!=1)return false;
 {EiemModUpdateStore::File file(path,false);auto snapshot=EiemModUpdateStore::Read(file);auto bytes=EiemModUpdateStore::Get(file,snapshot,"runtime/author-seal");EiemOfflinePackage::Reader seal{bytes};if(seal.S()!=PackageDigest(author))return false;}
 auto stage=EiemCompiledModStore::Load(path,mod,game,true);if(!stage.selection||stage.artifact.manifest.plan.author!=PackageDigest(author))return false;
 auto published=EiemCompiledModStore::Load(publishedPath,previous,game,true);if(!published.selection)return false;
 Require(author.id==published.artifact.manifest.package.id,"concurrent discoveries belong to another Mod");
 auto selected=EiemCompiledExportCarryover::Merge(current,*published.selection,published.artifact,currentSelectionComplete);
 // Core or already known source changes need the ordinary affected-input path.
 for(const auto& row:stage.selection->inputs){auto next=selected.inputs.find(row.first);if(next==selected.inputs.end()||next->second.revision!=row.second.revision)return false;
  auto oldDeps=stage.selection->dependencies.find(row.first),newDeps=selected.dependencies.find(row.first);if((oldDeps==stage.selection->dependencies.end())!=(newDeps==selected.dependencies.end())||(oldDeps!=stage.selection->dependencies.end()&&oldDeps->second!=newDeps->second))return false;}
 for(const auto& row:stage.selection->assets){auto next=selected.assets.find(row.first);if(next==selected.assets.end()||next->second!=row.second)return false;}
 auto publishSelection=[&]{auto descriptorSelection=selected;descriptorSelection.manifest=EiemStaticSources::LoadMod(mod,game).manifest;auto bytes=EiemCompiledModStore::SourceSelection(descriptorSelection,game);if(bytes!=EiemNativeInput::Read(mod/"source-inputs.bin"))EiemOfflineCompile::AtomicSave(mod/"source-inputs.bin",bytes);};
 std::set<std::string> represented;for(const auto& source:stage.artifact.manifest.plan.originals)represented.insert(source.file);
 std::set<std::string> pending;for(const auto& candidate:selected.candidates){auto identity=candidate+'\n'+selected.inputs.at(candidate).revision;auto file=Hash(Bytes(identity.begin(),identity.end()))+".vfs";
  if(!stage.selection->candidates.count(candidate)||(!stage.decisions.count(candidate)&&!represented.count(file)))pending.insert(candidate);}
 if(pending.empty()){publishSelection();return true;}
 EiemModUpdateStore::File backing(path,false);auto snapshot=EiemModUpdateStore::Read(backing);if(!snapshot.blocks.count("runtime/prepared-pfb"))return false;
 auto descriptor=EiemModUpdateStore::Get(backing,snapshot,"runtime/prepared-pfb");
 auto prepared=EiemPreparedPrefabCompiler::Decode(descriptor,std::make_shared<const Artifact>(stage.artifact),selected);
 auto artifact=std::move(stage.artifact);std::set<std::string> affected;
 for(const auto& source:pending){auto result=EiemResourceReloadPlan::Measure(cost,"export-catchup-pfb",[&]{return EiemPreparedPrefabCompiler::Compile(prepared,{source,EiemSourceAdaptationCompiler::StampFor(author,selected,source)});});
  stage.decisions[source]=result.decision;if(result.artifact){for(const auto& input:result.artifact->manifest.plan.originals)affected.insert(input.file);artifact=EiemCompiledArtifactAppend::Merge(artifact,*result.artifact);}}
 EiemCompiledModStore::SaveAffected(path,stage.generation,artifact,game,affected,{},stage.decisions,EiemCompiledExportCarryover::Catalog(selected),nullptr,&selected);
 publishSelection();return true;
}
} // namespace EiemPreparedExportCarryover
