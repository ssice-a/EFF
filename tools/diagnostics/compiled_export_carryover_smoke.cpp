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
int wmain(int argc,wchar_t** argv){try{
 if(argc==3&&std::wstring(argv[1])==L"--commit-fixture"){auto path=std::filesystem::absolute(argv[2]);auto previous=std::filesystem::exists(path)?EiemModUpdateStore::Read(path).generation:0;auto result=EiemModUpdateStore::Commit(path,previous,{{"discovery/test",Bytes{static_cast<unsigned char>(previous+1)}}});std::cout<<"COMMITTED "<<result.generation<<'\n';return 0;}
 Require(argc==3||argc==4,"compiled_export_carryover_smoke <game> <Mod> [compiler]");auto game=std::filesystem::canonical(argv[1]),mod=std::filesystem::canonical(argv[2]);auto original=Read(mod,game);auto selection=EiemStaticSources::LoadMod(mod,game);
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
 EiemCompiledModWriter::Export(cold,game);auto expected=Read(cold,game);
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
 std::cout<<"PASS export carryover: runtimeAddedPfb=1 roots="<<completed.decisions.at(logical).roots.size()<<" nativeViews="<<actual.artifact.views.size()<<" changedViews="<<changed<<" newMeshMatchesColdCompile=1 logicalRoutingAndDecisionsRetained=1 repeatedExportRetained=1 concurrentCatchupNoOldSourceRewrite=1 catchupPfbMs="<<catchupCosts["export-catchup-pfb"]<<" originalModModified=0 runtimeGeneration="<<appended.generation<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
