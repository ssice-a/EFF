#include "../../src/eiem_prepared_prefab_compiler.h"
#include "../../src/eiem_compiled_prefab_update.h"
#include "../../src/eiem_prepared_export_carryover.h"
#include <iostream>
#include <process.h>
using namespace EiemCompiledMod;
void ChangeMeshBounds(const std::filesystem::path& mod){
 auto author=EiemDirectory::Load(mod);const auto& mesh=author.meshes.begin()->second;auto path=mod/std::filesystem::u8path(mesh.resource);auto bytes=EiemNativeInput::Read(path);auto resource=EiemDirectory::Decode(bytes,1);const auto& bounds=resource.blocks.at("m_LocalAABB");
 Require(bounds.size==24,"Mesh bounds fixture is not a six-float box");float x=0;memcpy(&x,bytes.data()+bounds.offset,4);x+=0.01f;memcpy(bytes.data()+bounds.offset,&x,4);
 size_t p=80;for(size_t i=0;i<resource.blocks.size();++i){auto n=Le(bytes,p,2);p+=2;std::string name((const char*)bytes.data()+p,size_t(n));p+=size_t(n);if(name=="m_LocalAABB"){auto digest=Sha(bytes.data()+bounds.offset,bounds.size);std::copy(digest.begin(),digest.end(),bytes.begin()+p+16);}p+=48;}
 auto body=Sha(bytes.data()+80,bytes.size()-80);std::copy(body.begin(),body.end(),bytes.begin()+48);EiemDirectory::Decode(bytes,1);EiemOfflineCompile::AtomicSave(path,bytes);
}
Package TargetFixture(std::vector<Identity> targets){
 Package package;package.id="mesh-target-fixture";package.iniHash[0]=1;auto resource=std::make_shared<Resource>();resource->kind=1;resource->bytes={1,3,3,7};
 package.resources.emplace("meshes/fixture.mesh",resource);package.meshes.emplace("fixture",Mesh{"meshes/fixture.mesh",3,3,{},{}});
 for(size_t i=0;i<targets.size();++i){auto id="rule"+std::to_string(i),part="part"+std::to_string(i);EiemDirectory::Part authored;authored.mesh="fixture";package.parts.emplace(part,std::move(authored));
  Rule rule;rule.parts={part};rule.targets.emplace(unsigned(i),targets[i]);package.rules.emplace(id,std::move(rule));package.targetIndex.emplace(targets[i],id);}
 return package;
}
EiemStaticSources::Selection TargetSelection(std::initializer_list<std::string> sources){
 EiemStaticSources::Selection selection;selection.manifest="fixture";for(const auto& source:sources){const auto revision=std::string(64,'a');EiemStaticSources::Entry entry;entry.logical=source;entry.revision=revision;entry.physical=std::filesystem::path("fixture")/(source+".vfs");entry.size=16;
  selection.inputs.emplace(source,std::move(entry));selection.candidates.insert(source);}return selection;
}
EiemConsumerUpdateRecords::Decision TargetDecision(const std::string& source,bool positive){
 EiemConsumerUpdateRecords::Decision decision;decision.stamp={std::string(64,'a'),std::string(64,'b'),std::string(64,'c'),"fixture"};if(positive)decision.roots.push_back({"assets/fixture.prefab",source,1,{1}});return decision;
}
int MeshTargetDeltaFixtures(){
 const Identity a{"assets/a.mesh","A"},b{"assets/b.mesh","B"};const std::string shared="assets/shared.pfb",other="assets/other.pfb";
 auto oldAuthor=TargetFixture({a}),addedAuthor=TargetFixture({a,b});auto added=EiemCompiledModWriter::MeshStructureChanges(addedAuthor,oldAuthor);
 Require(added&&added->previousTargets.empty()&&added->currentTargets.size()==1&&added->currentTargets.count(b),"new Mesh target was not isolated");
 Artifact old;old.manifest.package=oldAuthor;old.manifest.plan.originals={{EiemStaticSources::SourceFile(shared,std::string(64,'a')),std::string(64,'a'),"fixture/shared.vfs",0,16}};
 old.manifest.plan.rendererRules={{{"cab-shared",1,a}}};old.manifest.plan.inputDependencies.resize(1);old.manifest.plan.roots.push_back({"assets/shared.prefab",0});
 auto prior=TargetSelection({shared}),current=TargetSelection({shared,other});
 EiemStaticSources::Discovery addDiscovery;addDiscovery.decisions.emplace(shared,TargetDecision(shared,true));addDiscovery.decisions.emplace(other,TargetDecision(other,true));
 addDiscovery.targetConsumers[shared].insert({b});addDiscovery.targetConsumers[other].insert({b});std::set<std::string> replaced,sources,affected;
 Require(EiemCompiledModWriter::MeshStructureSources(old,prior,current,*added,&addDiscovery,true,replaced,sources,affected),"added target sources were not isolated");
 Require(replaced==std::set<std::string>{EiemStaticSources::SourceFile(shared,std::string(64,'a'))}&&sources==std::set<std::string>{shared,other}&&affected.size()==2,
  "new target did not replace its existing PFB and compile a separate new PFB");

 auto oldBoth=TargetFixture({a,b}),removedAuthor=TargetFixture({a});auto removed=EiemCompiledModWriter::MeshStructureChanges(removedAuthor,oldBoth);
 Require(removed&&removed->previousTargets.size()==1&&removed->previousTargets.count(b)&&removed->currentTargets.empty(),"deleted Mesh target was not isolated");
 Artifact priorBoth;priorBoth.manifest.package=oldBoth;priorBoth.manifest.plan.originals={
  {EiemStaticSources::SourceFile(shared,std::string(64,'a')),std::string(64,'a'),"fixture/shared.vfs",0,16},
  {EiemStaticSources::SourceFile(other,std::string(64,'a')),std::string(64,'a'),"fixture/other.vfs",0,16}};
 priorBoth.manifest.plan.rendererRules={{{"cab-shared",1,a},{"cab-shared",2,b}},{{"cab-other",3,b}}};priorBoth.manifest.plan.inputDependencies.resize(2);
 priorBoth.manifest.plan.roots.push_back({"assets/shared.prefab",0});priorBoth.manifest.plan.roots.push_back({"assets/other.prefab",1});
 auto priorBothSelection=TargetSelection({shared,other}),afterDelete=TargetSelection({shared});
 EiemStaticSources::Discovery deleteDiscovery;deleteDiscovery.decisions.emplace(shared,TargetDecision(shared,true));deleteDiscovery.targetConsumers[shared].insert({a});
 replaced.clear();sources.clear();affected.clear();Require(EiemCompiledModWriter::MeshStructureSources(priorBoth,priorBothSelection,afterDelete,*removed,&deleteDiscovery,true,replaced,sources,affected),"deleted target source retirement was not isolated");
 Require(replaced.size()==2&&replaced.count(EiemStaticSources::SourceFile(other,std::string(64,'a')))&&sources==std::set<std::string>{shared}&&affected.size()==1,
  "deleting one target did not retire its PFB and rebuild the shared PFB");
 auto emptyAuthor=TargetFixture({});auto removeLast=EiemCompiledModWriter::MeshStructureChanges(emptyAuthor,oldAuthor);Require(bool(removeLast),"last Mesh target deletion was not classified");
 auto noTargets=TargetSelection({});EiemStaticSources::Discovery emptyDiscovery;replaced.clear();sources.clear();affected.clear();
 Require(EiemCompiledModWriter::MeshStructureSources(old,prior,noTargets,*removeLast,&emptyDiscovery,true,replaced,sources,affected)&&replaced.size()==1&&sources.empty()&&affected.empty(),
  "deleting the last target failed to retire sources without rebuilding");
 auto emptyDelta=EiemCompiledModWriter::EmptyMeshStructureDelta(old);Require(emptyDelta.manifest.plan.inputs.empty()&&emptyDelta.manifest.plan.roots.empty()&&emptyDelta.manifest.plan.author==old.manifest.plan.author,
  "empty structure delta retained stale PFB inputs or author seal");
 std::cout<<"PASS Mesh target structure deltas: addExistingBundle=1 addNewPfb=1 deletePfb=1 sharedPfbRebuilt=1 deleteLastTarget=1 emptyDelta=1\n";return 0;
}
Package SubsetTargets(Package package,const std::set<Identity>& targets){
 for(auto row=package.targetIndex.begin();row!=package.targetIndex.end();)if(!targets.count(row->first))row=package.targetIndex.erase(row);else ++row;
 for(auto rule=package.rules.begin();rule!=package.rules.end();){for(auto target=rule->second.targets.begin();target!=rule->second.targets.end();)if(!targets.count(target->second))target=rule->second.targets.erase(target);else ++target;
  if(rule->second.targets.empty())rule=package.rules.erase(rule);else ++rule;}return package;
}
std::set<Identity> TargetSet(const Package& package){std::set<Identity> result;for(const auto& row:package.targetIndex)result.insert(row.first);return result;}
std::map<std::string,std::pair<std::string,size_t>> ViewSet(const Artifact& artifact){std::map<std::string,std::pair<std::string,size_t>> result;
 for(size_t i=0;i<artifact.views.size();++i)Require(result.emplace(artifact.manifest.plan.originals.at(i).file,std::make_pair(artifact.views[i].startupHash,artifact.views[i].size)).second,"target delta source repeated");return result;}
std::map<std::string,std::map<std::string,size_t>> MarkerSet(const Artifact& artifact){std::map<std::string,std::map<std::string,size_t>> result;
 for(const auto& root:artifact.manifest.plan.roots)for(const auto& source:root.Assembly().sources)for(const auto& target:source.targets)if(!target.marker.empty())++result[root.path][target.marker];return result;}
Artifact CompileStructureDelta(const Artifact& previous,const Package& author,const EiemStaticSources::Selection& selection,const std::set<std::string>& sources,
 const std::set<std::string>& replaced,const std::set<std::string>& changedResources){
 auto revised=std::make_shared<Artifact>(previous);Require(EiemCompiledModWriter::PrepareMeshStructureBaseline(*revised,author),"target fixture author baseline unsupported");
 auto prepared=EiemPreparedPrefabCompiler::Prepare(revised,selection);std::optional<Artifact> combined;
 for(const auto& source:sources){auto request=EiemSourceAdaptationJobs::Request{source,EiemSourceAdaptationCompiler::StampFor(author,selection,source)};auto compiled=EiemPreparedPrefabCompiler::Compile(prepared,request);
  if(!compiled.artifact)continue;if(!combined)combined=*compiled.artifact;else combined=EiemCompiledArtifactAppend::Merge(*combined,*compiled.artifact);}
 if(!combined)combined=EiemCompiledModWriter::EmptyMeshStructureDelta(*revised);auto& plan=combined->manifest.plan;const auto& base=revised->manifest.plan;
 plan.parts=base.parts;plan.materials=base.materials;plan.meshParts=base.meshParts;plan.materialMatches=base.materialMatches;plan.textures=base.textures;plan.opaque=base.opaque;
 plan.rendererBoundsHits=base.rendererBoundsHits;plan.rendererBoundsMisses=base.rendererBoundsMisses;plan.frameRecipes=base.frameRecipes;
 Require(EiemCompiledModWriter::PrepareMeshStructureFrames(*revised,author,changedResources,prepared.schemas),"target fixture frames unsupported");
 plan.frameRecipes=revised->manifest.plan.frameRecipes;
 return EiemCompiledPrefabUpdate::ReplaceAuthorSources(previous,*combined,replaced,changedResources,revised.get());
}
int wmainTargetDelta(const std::filesystem::path& game,const std::filesystem::path& mod){
 auto author=EiemDirectory::Load(mod);auto selection=EiemStaticSources::LoadMod(mod,game);auto allTargets=TargetSet(author);Require(allTargets.size()>=2,"real Mod fixture has fewer than two Mesh targets");
 auto allDiscovery=EiemStaticSources::FindConsumers(selection,allTargets);std::map<Identity,std::set<std::string>> consumers;
 for(const auto& row:allDiscovery.targetConsumers)for(const auto& target:row.second)consumers[target].insert(row.first);
 std::optional<std::pair<Identity,Identity>> chosen;bool foundShared=false;for(const auto& a:consumers)for(const auto& b:consumers)if((a.first.path!=b.first.path||a.first.asset!=b.first.asset)&&
   selection.assets.at(EiemStaticSources::Logical(a.first.path))==selection.assets.at(EiemStaticSources::Logical(b.first.path))){
  bool shared=false,exclusive=false;for(const auto& source:a.second){shared=shared||b.second.count(source);exclusive=exclusive||!b.second.count(source);}for(const auto& source:b.second)exclusive=exclusive||!a.second.count(source);
  if(shared&&exclusive){chosen=std::make_pair(a.first,b.first);foundShared=true;}else if(!chosen&&exclusive)chosen=std::make_pair(a.first,b.first);
  if(foundShared)break;}
 Require(chosen.has_value(),"real Mod fixture targets lack distinguishable PFB consumers");const auto first=chosen->first,second=chosen->second;
 auto run=[&](const std::set<Identity>& beforeTargets,const std::set<Identity>& afterTargets,const char* label){
  auto beforeAuthor=SubsetTargets(author,beforeTargets),afterAuthor=SubsetTargets(author,afterTargets);auto beforeDiscovery=EiemStaticSources::FindConsumers(selection,beforeTargets);
  auto previous=EiemCompiledModWriter::Compile(beforeAuthor,selection,game,{},nullptr,&beforeDiscovery);auto delta=EiemCompiledModWriter::MeshStructureChanges(afterAuthor,beforeAuthor);Require(delta&& !delta->targets.empty(),std::string(label)+" author change was not classified");
  auto afterDiscovery=EiemStaticSources::FindConsumers(selection,afterTargets);std::set<std::string> replaced,sources,affected;
  std::string isolationReason;auto isolated=EiemCompiledModWriter::MeshStructureSources(previous,selection,selection,*delta,&afterDiscovery,true,replaced,sources,affected,&isolationReason);
  Require(isolated,std::string(label)+" source closure was not safely isolated: "+isolationReason);
  auto incremental=CompileStructureDelta(previous,afterAuthor,selection,sources,replaced,delta->resources);auto cold=EiemCompiledModWriter::Compile(afterAuthor,selection,game,{},nullptr,&afterDiscovery);
  auto incrementalViews=ViewSet(incremental),coldViews=ViewSet(cold);if(incrementalViews!=coldViews){std::string detail=" incremental="+std::to_string(incrementalViews.size())+" cold="+std::to_string(coldViews.size());
   for(const auto& row:coldViews){auto found=incrementalViews.find(row.first);if(found==incrementalViews.end()){
    detail+=" missing:"+row.first;auto source=std::find_if(selection.inputs.begin(),selection.inputs.end(),[&](const auto& input){return EiemStaticSources::SourceFile(input.first,input.second.revision)==row.first;});
    if(source!=selection.inputs.end()){detail+="["+source->first+" candidate="+std::to_string(selection.candidates.count(source->first))+" decision="+std::to_string(afterDiscovery.decisions.at(source->first).roots.size());
     auto hits=afterDiscovery.targetConsumers.find(source->first);detail+=" changedTargetHit="+std::to_string(hits!=afterDiscovery.targetConsumers.end()&&std::any_of(hits->second.begin(),hits->second.end(),[&](const auto& target){return EiemStaticSources::Logical(target.path)==EiemStaticSources::Logical(second.path)&&target.asset==second.asset;}))+" rebuilt="+std::to_string(sources.count(source->first))+ "]";}}
    else if(found->second!=row.second)detail+=" stale:"+row.first;}
   for(const auto& row:incrementalViews)if(!coldViews.count(row.first))detail+=" extra:"+row.first;Require(false,std::string(label)+" target delta native views differ from cold compile"+detail);}
  Require(MarkerSet(incremental)==MarkerSet(cold),std::string(label)+" target delta Renderer markers differ from cold compile");
  std::cout<<"PASS real Mesh target "<<label<<": previousPfb="<<previous.views.size()<<" incrementalPfb="<<incremental.views.size()<<" coldPfb="<<cold.views.size()<<" replaced="<<replaced.size()<<" compiled="<<sources.size()<<" reused="<<(previous.views.size()>=replaced.size()?previous.views.size()-replaced.size():0)<<" coldEquivalent=1\n";
 };
 std::set<Identity> one{first},added{first,second};Require(first.path!=second.path||first.asset!=second.asset,"selected duplicate Mesh identity");run(one,added,"add");run(added,one,"delete");return 0;
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==2&&std::wstring(argv[1])==L"--mesh-target-delta-fixtures")return MeshTargetDeltaFixtures();
 if(argc==4&&std::wstring(argv[1])==L"--mesh-target-delta-real")return wmainTargetDelta(std::filesystem::canonical(argv[2]),std::filesystem::canonical(argv[3]));
 if(argc==3&&std::wstring(argv[1])==L"--commit-fixture"){auto path=std::filesystem::absolute(argv[2]);auto previous=std::filesystem::exists(path)?EiemModUpdateStore::Read(path).generation:0;auto result=EiemModUpdateStore::Commit(path,previous,{{"discovery/test",Bytes{static_cast<unsigned char>(previous+1)}}});std::cout<<"COMMITTED "<<result.generation<<'\n';return 0;}
 if(argc==3&&std::wstring(argv[1])==L"--author-read-fixture"){auto path=std::filesystem::absolute(argv[2]);
  // The small storage fixture lacks runtime/common. Enter the real default
  // author Load/pin path under a Python-held publication mutex and return its
  // expected missing-record failure, rather than waiting on the parent's lock.
  try{auto snapshot=EiemCompiledModStore::Load(path,path.parent_path(),path.parent_path());}
  catch(const std::out_of_range&){std::cout<<"AUTHOR-READER-RETURNED\n";return 0;}
  Require(false,"author reader fixture unexpectedly accepted a storage-only index");}
 Require(argc==3||argc==4,"compiled_export_carryover_smoke <game> <Mod> [compiler]");auto game=std::filesystem::canonical(argv[1]),mod=std::filesystem::canonical(argv[2]);auto original=EiemCompiledModStore::Load(mod/"compiled.bin",mod,game,false).artifact;auto selection=EiemStaticSources::LoadMod(mod,game);
 auto compilerPath=argc==4?std::filesystem::canonical(argv[3]):std::filesystem::path{};
 auto exportStage=[&](const std::filesystem::path& stage,const std::filesystem::path& previous){
  if(!compilerPath.empty()){auto stageArg=L"\""+stage.wstring()+L"\"",gameArg=L"\""+game.wstring()+L"\"",previousArg=L"\""+previous.wstring()+L"\"";auto result=_wspawnl(_P_WAIT,compilerPath.c_str(),compilerPath.c_str(),L"--export-mod",stageArg.c_str(),gameArg.c_str(),L"--previous-mod",previousArg.c_str(),static_cast<const wchar_t*>(nullptr));Require(result==0,"installed compiler carryover command failed");}
  else EiemCompiledModWriter::Export(stage,game,{},previous,nullptr,nullptr,nullptr,[](std::shared_ptr<const Artifact> baseline,const EiemStaticSources::Selection& selected){return EiemPreparedPrefabCompiler::Encode(EiemPreparedPrefabCompiler::Prepare(std::move(baseline),selected));});
 };
 std::map<std::string,std::string> logicalByFile;for(const auto& row:selection.inputs){auto identity=row.first+'\n'+row.second.revision;logicalByFile[Hash(Bytes(identity.begin(),identity.end()))+".vfs"]=row.first;}
 size_t leaf=SIZE_MAX;for(const auto& root:original.manifest.plan.roots){auto input=root.input;bool incoming=false;for(const auto& dependencies:original.manifest.plan.inputDependencies)incoming=incoming||std::find(dependencies.begin(),dependencies.end(),input)!=dependencies.end();
  if(!incoming&&selection.candidates.count(logicalByFile.at(original.manifest.plan.originals.at(input).file))){leaf=input;break;}}
 Require(leaf!=SIZE_MAX,"fixture has no leaf PFB");auto file=original.manifest.plan.originals.at(leaf).file,logical=logicalByFile.at(file);
 auto temporary=std::filesystem::temp_directory_path()/("eff-export-carryover-"+std::to_string(GetCurrentProcessId()));Require(std::filesystem::create_directory(temporary),"carryover test directory exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{temporary};auto published=temporary/"published",stage=temporary/"staging",cold=temporary/"cold";
 std::filesystem::copy(mod,published,std::filesystem::copy_options::recursive);auto incomplete=EiemCompiledPrefabUpdate::Retire(original,{file});incomplete.manifest.package.root=published;
 auto oldSelection=selection;oldSelection.inputs.erase(logical);oldSelection.candidates.erase(logical);oldSelection.dependencies.erase(logical);for(auto& row:oldSelection.dependencies)row.second.erase(logical);
 for(auto i=oldSelection.assets.begin();i!=oldSelection.assets.end();)if(i->second==logical)i=oldSelection.assets.erase(i);else ++i;
 EiemOfflineCompile::AtomicSave(published/"source-inputs.bin",EiemCompiledModStore::SourceSelection(oldSelection,game));std::filesystem::remove(published/"compiled.bin");
 auto descriptor=EiemPreparedPrefabCompiler::Encode(EiemPreparedPrefabCompiler::Prepare(std::make_shared<const Artifact>(incomplete),selection));
 auto first=EiemCompiledModStore::Save(published/"compiled.bin",0,incomplete,game,{},EiemCompiledExportCarryover::Catalog(oldSelection),&descriptor,&oldSelection);
 auto concurrentPublished=temporary/"concurrent-published",concurrentStage=temporary/"concurrent-stage";std::filesystem::copy(published,concurrentPublished,std::filesystem::copy_options::recursive);std::filesystem::copy(published,concurrentStage,std::filesystem::copy_options::recursive);ChangeMeshBounds(concurrentStage);
 exportStage(concurrentStage,concurrentPublished);
 // The runtime commits the new PFB and its logical routing inside compiled.bin;
 // the author's original offline source descriptor deliberately stays old.
 auto completed=EiemStaticSources::FindConsumers(selection,[&]{std::set<Identity> targets;for(const auto& row:original.manifest.package.targetIndex)targets.insert(row.first);return targets;}());
 auto full=original;full.manifest.package.root=published;
 auto appended=EiemCompiledModStore::SaveAffected(published/"compiled.bin",first.generation,full,game,{file},{},completed.decisions,EiemCompiledExportCarryover::Catalog(selection),nullptr,&selection);
 auto concurrentFull=full;concurrentFull.manifest.package.root=concurrentPublished;EiemCompiledModStore::SaveAffected(concurrentPublished/"compiled.bin",first.generation,concurrentFull,game,{file},{},completed.decisions,EiemCompiledExportCarryover::Catalog(selection),nullptr,&selection);
 auto stagedBefore=EiemModUpdateStore::Read(concurrentStage/"compiled.bin");std::map<std::string,double> catchupCosts;auto catchupAuthor=EiemDirectory::Load(concurrentStage);
 if(!compilerPath.empty())exportStage(concurrentStage,concurrentPublished);else Require(EiemPreparedExportCarryover::Try(concurrentStage,concurrentPublished,game,catchupAuthor,EiemStaticSources::Merge({catchupAuthor},game),[&](const char* name,double ms){catchupCosts[name]+=ms;}),"concurrent new PFB caused a full author rebuild");
 auto stagedAfter=EiemModUpdateStore::Read(concurrentStage/"compiled.bin");for(const auto& row:stagedBefore.blocks)if(row.first.rfind("runtime/source/",0)==0||row.first.rfind("runtime/proof/",0)==0||row.first.rfind("runtime/frame/",0)==0)Require(stagedAfter.blocks.at(row.first).offset==row.second.offset,"export catchup rewrote an already compiled source/proof/frame");
 auto before=Hash(EiemNativeInput::Read(published/"compiled.bin"));
 std::filesystem::copy(published,stage,std::filesystem::copy_options::recursive);std::filesystem::remove(stage/"compiled.bin");std::filesystem::create_hard_link(published/"compiled.bin",stage/"compiled.bin");ChangeMeshBounds(stage);
 std::filesystem::copy(stage,cold,std::filesystem::copy_options::recursive);std::filesystem::remove(cold/"compiled.bin");EiemOfflineCompile::AtomicSave(cold/"source-inputs.bin",EiemCompiledModStore::SourceSelection(selection,game));
 EiemCompiledModWriter::Export(cold,game);auto expected=EiemCompiledModStore::Load(cold/"compiled.bin",cold,game,false).artifact;
 exportStage(stage,published);
 auto actual=EiemCompiledModStore::Load(stage/"compiled.bin",stage,game,true);Require(actual.selection&&actual.selection->candidates.count(logical)&&actual.decisions.count(logical),"export lost a runtime-discovered candidate or completed decision");
 auto caughtUp=EiemCompiledModStore::Load(concurrentStage/"compiled.bin",concurrentStage,game,true);Require(caughtUp.artifact.views.size()==expected.views.size(),"concurrent export catchup lost sources");for(size_t i=0;i<caughtUp.artifact.views.size();++i){auto source=caughtUp.artifact.manifest.plan.originals.at(i).file;auto at=std::find_if(expected.manifest.plan.originals.begin(),expected.manifest.plan.originals.end(),[&](const auto& row){return row.file==source;});Require(at!=expected.manifest.plan.originals.end()&&caughtUp.artifact.views.at(i).startupHash==expected.views.at(size_t(at-expected.manifest.plan.originals.begin())).startupHash,"concurrent export catchup kept the old author result");}
 Require(actual.artifact.manifest.plan.author==PackageDigest(EiemDirectory::Load(stage))&&actual.artifact.manifest.plan.author!=original.manifest.plan.author,"export retained the previous Mesh author snapshot");
 Require(actual.artifact.views.size()==expected.views.size(),"carried-over export lost source coverage");std::map<std::string,size_t> expectedFiles;for(size_t i=0;i<expected.views.size();++i)expectedFiles[expected.manifest.plan.originals[i].file]=i;
 size_t changed=0;for(size_t i=0;i<actual.artifact.views.size();++i){const auto& source=actual.artifact.manifest.plan.originals[i];auto at=expectedFiles.at(source.file);Require(actual.artifact.views[i].startupHash==expected.views[at].startupHash&&actual.artifact.views[i].size==expected.views[at].size,"carried-over Mesh export differs from complete cold compile");auto old=std::find_if(original.manifest.plan.originals.begin(),original.manifest.plan.originals.end(),[&](const auto& row){return row.file==source.file;});if(old!=original.manifest.plan.originals.end()&&actual.artifact.views[i].startupHash!=original.views[size_t(old-original.manifest.plan.originals.begin())].startupHash)++changed;}
 Require(changed>0,"changed Mesh generated only stale native views");Require(Hash(EiemNativeInput::Read(published/"compiled.bin"))==before,"export modified the published runtime snapshot");
 auto retained=EiemStaticSources::LoadMod(stage,game);Require(retained.candidates.count(logical),"next export would lose the inherited source");
 auto repeated=temporary/"repeated";std::filesystem::copy(stage,repeated,std::filesystem::copy_options::recursive);EiemCompiledModWriter::Export(repeated,game,{},stage);auto again=EiemCompiledModStore::Load(repeated/"compiled.bin",repeated,game,true);Require(again.artifact.views.size()==actual.artifact.views.size()&&again.decisions.count(logical),"unchanged repeated export lost discoveries");
 auto unsafe=selection;unsafe.inputs.at(logical).revision=Hash(Bytes{4,5,6});bool rejected=false;try{EiemCompiledExportCarryover::Merge(unsafe,selection,full);}catch(const std::exception&){rejected=true;}Require(rejected,"conflicting source version silently replaced a verified runtime source");
 rejected=false;try{EiemCompiledExportCarryover::Merge(oldSelection,oldSelection,full);}catch(const std::exception&){rejected=true;}Require(rejected,"export silently dropped a compiled source missing logical provenance");
 auto structureStage=temporary/"structure-stage",structureCold=temporary/"structure-cold";std::filesystem::copy(mod,structureStage,std::filesystem::copy_options::recursive);std::filesystem::copy(structureStage,structureCold,std::filesystem::copy_options::recursive);
 auto structureAuthor=EiemDirectory::Load(structureStage);auto rule=std::find_if(structureAuthor.rules.begin(),structureAuthor.rules.end(),[](const auto& row){return !row.second.skip&&!row.second.parts.empty();});Require(rule!=structureAuthor.rules.end(),"structure fixture has no authored part");
 auto newPart=std::string("zz_structure_regression");while(structureAuthor.parts.count(newPart))newPart+="_x";structureAuthor.parts.emplace(newPart,structureAuthor.parts.at(rule->second.parts.front()));rule->second.parts.push_back(newPart);
 auto previousStore=EiemCompiledModStore::Load(mod/"compiled.bin",mod,game,true);Require(previousStore.selection.has_value(),"structure fixture source selection absent");auto structureSelection=*previousStore.selection;
 auto structureDelta=EiemCompiledModWriter::MeshStructureChanges(structureAuthor,original.manifest.package);Require(structureDelta&&!structureDelta->targets.empty(),"Part addition was not classified as a Mesh structure change");
 std::set<std::string> replaced,structureSources,structureAffected;Require(EiemCompiledModWriter::MeshStructureSources(original,structureSelection,structureSelection,*structureDelta,nullptr,false,replaced,structureSources,structureAffected),"structure fixture sources cannot be isolated");
 auto prepareStructure=[](std::shared_ptr<const Artifact> baseline,const EiemStaticSources::Selection& current){return EiemPreparedPrefabCompiler::Encode(EiemPreparedPrefabCompiler::Prepare(std::move(baseline),current));};
 auto compileStructure=[](std::shared_ptr<const Artifact> baseline,const EiemStaticSources::Selection& current,const std::set<std::string>& sources,const std::function<void(const char*,double)>& cost)->std::optional<EiemCompiledModWriter::MeshStructureCompilation>{
  auto prepared=EiemPreparedPrefabCompiler::Prepare(baseline,current);std::optional<Artifact> combined;EiemConsumerUpdateRecords::Decisions decisions;
  for(const auto& source:sources){EiemSourceAdaptationJobs::Request request{source,EiemSourceAdaptationCompiler::StampFor(baseline->manifest.package,current,source)};auto compiled=EiemPreparedPrefabCompiler::Compile(prepared,request,cost);
   if(!compiled.artifact)return std::nullopt;Require(decisions.emplace(source,compiled.decision).second,"structure fixture source repeated");if(!combined)combined=*compiled.artifact;else combined=EiemCompiledArtifactAppend::Merge(*combined,*compiled.artifact);}
  if(!combined)return EiemCompiledModWriter::MeshStructureCompilation{EiemCompiledModWriter::EmptyMeshStructureDelta(*baseline),{},std::move(prepared.schemas)};auto& plan=combined->manifest.plan;const auto& currentPlan=baseline->manifest.plan;plan.parts=currentPlan.parts;plan.materials=currentPlan.materials;
  plan.meshParts=currentPlan.meshParts;plan.materialMatches=currentPlan.materialMatches;plan.textures=currentPlan.textures;plan.opaque=currentPlan.opaque;
  plan.rendererBoundsHits=currentPlan.rendererBoundsHits;plan.rendererBoundsMisses=currentPlan.rendererBoundsMisses;plan.frameRecipes=currentPlan.frameRecipes;
  return EiemCompiledModWriter::MeshStructureCompilation{std::move(*combined),std::move(decisions),std::move(prepared.schemas)};
 };
 std::map<std::string,double> structureCosts;auto structureCost=[&](const char* name,double ms){structureCosts[name]+=ms;};
  EiemCompiledModWriter::Export(structureStage,game,structureCost,mod,&structureAuthor,&structureSelection,nullptr,prepareStructure,false,compileStructure);
 if(structureCosts["mesh-structure-incremental"]!=1){std::string detail;for(const auto& row:structureCosts)detail+=" "+row.first+"="+std::to_string(row.second);Require(false,"Part addition fell back to full compilation; costs:"+detail);}
 EiemCompiledModWriter::Export(structureCold,game,{}, {},&structureAuthor,&structureSelection,nullptr,prepareStructure,false);auto structureActual=EiemCompiledModStore::Load(structureStage/"compiled.bin",structureStage,game,true);
 auto structureExpected=EiemCompiledModStore::Load(structureCold/"compiled.bin",structureCold,game,true);Require(structureActual.artifact.manifest.plan.author==PackageDigest(structureAuthor)&&
  structureActual.artifact.views.size()==structureExpected.artifact.views.size(),"incremental Mesh structure snapshot differs from cold compile coverage");
 auto markers=[](const Artifact& artifact){std::map<std::string,size_t> counts;for(const auto& root:artifact.manifest.plan.roots)for(const auto& source:root.Assembly().sources)for(const auto& target:source.targets)if(!target.marker.empty())++counts[target.marker];return counts;};
 Require(markers(structureActual.artifact)==markers(structureExpected.artifact),"incremental Mesh structure root markers differ from cold compile");
 auto oldStore=EiemModUpdateStore::Read(mod/"compiled.bin"),newStore=EiemModUpdateStore::Read(structureStage/"compiled.bin");size_t reusedSources=0,rewrittenSources=0;
 for(const auto& row:oldStore.blocks)if(row.first.rfind("runtime/source/",0)==0||row.first.rfind("runtime/proof/",0)==0){auto key=row.first.substr(row.first.find("/",8)+1);
  if(replaced.count(key)){Require(newStore.blocks.at(row.first).digest!=row.second.digest,"affected structure source retained its stale immutable block");++rewrittenSources;}
  else{Require(newStore.blocks.at(row.first).digest==row.second.digest,"unaffected structure source was re-encoded");++reusedSources;}}
 Require(rewrittenSources>0&&reusedSources>0,"structure regression did not exercise selective source replacement");
 auto legacyStage=temporary/"legacy-cache";std::filesystem::copy(mod,legacyStage,std::filesystem::copy_options::recursive);std::filesystem::remove_all(legacyStage/"compiled.blocks");
 Writer legacyPacket;legacyPacket.S("EFF_COMPILED_MOD_LEGACY_PACKET");EiemOfflineCompile::AtomicSave(legacyStage/"compiled.bin",legacyPacket.b);auto legacyAuthor=EiemDirectory::Load(legacyStage);
 std::map<std::string,double> legacyCosts;auto legacyCost=[&](const char* name,double ms){legacyCosts[name]+=ms;};
 EiemCompiledModWriter::Export(legacyStage,game,legacyCost,{},&legacyAuthor,&selection,nullptr,
  [](std::shared_ptr<const Artifact> baseline,const EiemStaticSources::Selection& current){return EiemPreparedPrefabCompiler::Encode(EiemPreparedPrefabCompiler::Prepare(std::move(baseline),current));});
 Require(legacyCosts["legacy-compiled-cache-ignored"]==1&&EiemCompiledModStore::IsStore(legacyStage/"compiled.bin"),"legacy packet cache was not ignored and rewritten as a v2 block store");
 auto legacyResult=EiemCompiledModStore::Load(legacyStage/"compiled.bin",legacyStage,game,false);Require(legacyResult.artifact.manifest.plan.author==PackageDigest(legacyAuthor),"legacy cache cold export produced a different author snapshot");
 std::cout<<"PASS export carryover+Mesh structure: runtimeAddedPfb=1 roots="<<completed.decisions.at(logical).roots.size()<<" nativeViews="<<actual.artifact.views.size()<<" changedViews="<<changed<<" newMeshMatchesColdCompile=1 structureAffected="<<rewrittenSources<<" structureReused="<<reusedSources<<" legacyCacheIgnoredColdV2=1 logicalRoutingAndDecisionsRetained=1 repeatedExportRetained=1 concurrentCatchupNoOldSourceRewrite=1 catchupPfbMs="<<catchupCosts["export-catchup-pfb"]<<" originalModModified=0 runtimeGeneration="<<appended.generation<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
