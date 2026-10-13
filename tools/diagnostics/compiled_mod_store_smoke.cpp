#include "../../src/eiem_compiled_mod_store.h"
#include <iostream>
using namespace EiemCompiledMod;
void AssemblyDictionaryRegression(){
 using namespace EiemRenderAssemblyDescription;
 Artifact a;Description d;EiemRenderAssemblyDescription::Source source;source.cab="CAB-0123456789abcdef0123456789abcdef";source.id=123;source.renderer={"/body",0};
 Target base;base.renderer=source.renderer;base.bones={"","/root/骨骼","/root/骨骼/child"};base.root="/root";base.hasRoot=true;
 Target extra=base;extra.renderer={"/body/part",1};extra.marker="P";extra.extension=true;source.targets={base,extra};d.sources.push_back(source);
 Lod lod;lod.group={"",0};LodLevel level;level.height=0.5f;level.fade=0;level.renderers={base.renderer,extra.renderer};lod.levels.push_back(level);d.lods.push_back(lod);
 Helper helper;helper.component={"/helper",2};helper.custom=true;helper.renderers=level.renderers;d.helpers.push_back(helper);
 a.manifest.plan.parts.emplace("P",EiemResourceReloadPlan::Part{});
 auto add=[&](const Description& value){Root root;root.cab=source.cab;root.assembly=value;root.markers["P"]=1;a.manifest.plan.roots.push_back(std::move(root));};
 add(d);auto shared=d;for(auto& target:shared.sources[0].targets){target.preparedBones=std::make_shared<const std::vector<std::string>>(target.bones);target.bones.clear();}add(shared);
 auto flag=d;flag.sources[0].targets[1].hasProbe=true;add(flag);
 auto palette=d;std::reverse(palette.sources[0].targets[1].bones.begin(),palette.sources[0].targets[1].bones.end());add(palette);
 auto ordinal=d;ordinal.helpers[0].component.component=3;add(ordinal);
 auto signedZero=d;signedZero.lods[0].levels[0].fade=-0.0f;add(signedZero);
 auto empty=d;empty.sources[0].targets[1].bones.clear();add(empty);
 a.pristine={a.manifest.plan.roots[1],a.manifest.plan.roots[3]};Writer encoded;auto ids=WritePreparedAssemblies(encoded,a);
 Require(ids.roots==std::vector<size_t>{0,0,1,2,3,4,5}&&ids.pristine==std::vector<size_t>{0,2},"assembly dictionary lost exact value/order identity");
 Require(encoded.b.size()==2891&&Hash(encoded.b)=="BF6C3244D259BC435BC9409775109226CC357F94C0573039007D42C4BB4B3AA7","assembly dictionary bytes changed from the original encoder");
 auto plan=a.manifest.plan;EiemOfflinePackage::Reader reader{encoded.b};auto decoded=ReadPreparedAssemblies(reader,plan);Require(reader.p==encoded.b.size()&&decoded.size()==6,"assembly dictionary suffix/count differs");
 for(size_t i=0;i<a.manifest.plan.roots.size();++i){Writer expected,actual;WriteAssembly(expected,a.manifest.plan.roots[i].Assembly());WriteAssembly(actual,plan.roots[i].Assembly());Require(expected.b==actual.b,"assembly dictionary changed Renderer/LOD/helper/palette semantics");}
 for(size_t i=0;i<a.pristine.size();++i){Writer expected,actual;WriteAssembly(expected,a.pristine[i].Assembly());WriteAssembly(actual,*decoded.at(ids.pristine[i]));Require(expected.b==actual.b,"assembly dictionary changed pristine reference");}
}
int wmain(int argc,wchar_t** argv){try{
 AssemblyDictionaryRegression();
 Require(argc==3,"compiled_mod_store_smoke <game> <Mod>");auto game=std::filesystem::canonical(argv[1]),mod=std::filesystem::canonical(argv[2]);auto artifact=Read(mod,game);auto expected=Encode(artifact,game);
 auto root=std::filesystem::temp_directory_path()/("eff-compiled-store-"+std::to_string(GetCurrentProcessId()));Require(std::filesystem::create_directory(root),"compiled store fixture exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{root};auto path=root/"compiled.bin";
 EiemResourceCatalogDelta::Catalog catalog;catalog.domain="verified-current-selection";for(const auto& source:artifact.manifest.plan.originals)catalog.entries.emplace(source.file,EiemResourceCatalogDelta::Entry{source.hash,source.physical.u8string()+":"+std::to_string(source.offset),{}});
 EiemConsumerUpdateRecords::Decisions decisions;auto first=EiemCompiledModStore::Save(path,0,artifact,game,decisions,catalog);auto loaded=EiemCompiledModStore::Load(path,mod,game,false);
 Require(Encode(loaded.artifact,game)==expected&&loaded.generation==first.generation,"block store changed compiled artifact semantics");
 auto source=artifact.manifest.plan.originals.front().file;decisions[source]={{artifact.manifest.plan.originals.front().hash,Hash(Bytes{1}),artifact.manifest.plan.contract,"precise-consumer"},{}};
 auto second=EiemCompiledModStore::SaveDiscovery(path,first.generation,decisions,catalog);Require(second.generation==first.generation+1,"new negative conclusion was not committed");
 size_t retained=0;for(const auto& row:first.blocks)if(row.first.rfind("runtime/",0)==0){Require(second.blocks.at(row.first).offset==row.second.offset,"negative discovery re-encoded immutable runtime block");++retained;}
 auto restarted=EiemCompiledModStore::Load(path,mod,game,true);Require(restarted.decisions.size()==1&&restarted.artifact.members.empty()&&restarted.artifact.views.size()==artifact.views.size(),"runtime restart eagerly hydrated proofs or lost decisions");
 Require(restarted.artifact.digest==loaded.artifact.digest,"negative decision changed the runtime resource identity");
 Require(!restarted.artifact.proofBlocks&&bool(restarted.artifact.proofReader),"runtime store reconstructed a whole proof packet");
 auto same=EiemCompiledModStore::Save(path,second.generation,artifact,game,decisions,catalog);Require(same.generation==second.generation,"unchanged store committed a generation");
 auto compact=EiemModUpdateStore::Compact(path,same.generation);auto final=EiemCompiledModStore::Load(path,mod,game,false);Require(final.generation==compact.generation&&Encode(final.artifact,game)==expected,"compacted Mod changed native view/proof data");
 HydrateInput(restarted.artifact,0);Require(restarted.artifact.hydrated.count(0),"compaction invalidated a pinned old proof reader");
 std::cout<<"PASS compiled single-file store: inputs="<<artifact.views.size()<<" immutableRuntimeBlocks="<<retained<<" nativeViewsAndProofs=exact negativeRecordIncremental=1 runtimeProofsDeferred=1 compactedFile=1 gameModModified=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
