#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_compiled_mod_set.h"
#include "../../src/eiem_compiled_mod_writer.h"
#include "../../src/eiem_native_view_writer.h"
#include "../../src/eiem_compiled_input_view_writer.h"
#include <regex>
#include "author_source_smoke.h"

void AssemblySelectionRegression(){
 using namespace EiemRenderAssemblyDescription;using EiemResourceReloadPlan::Root;
 Root pristine;pristine.path="assets/shared.prefab";
 for(int i=0;i<2;++i){EiemRenderAssemblyDescription::Source source;source.cab="fixture-cab";source.id=10+i;source.renderer={"renderer"+std::to_string(i),0};Target target;target.renderer=source.renderer;target.bones={"root/hip","root/hip/tip"};source.targets={target};pristine.assembly.sources.push_back(source);}
 Lod lod;lod.group={"lod",0};LodLevel level;level.height=.5f;for(const auto& source:pristine.assembly.sources)level.renderers.push_back(source.renderer);lod.levels={level};pristine.assembly.lods={lod};
 Helper helper;helper.component={"helper",0};helper.renderers=level.renderers;pristine.assembly.helpers={helper};
 auto left=pristine,right=pristine;auto extra=left.assembly.sources[0].targets[0];extra.renderer={"renderer0/extra",1};extra.marker="left";left.assembly.sources[0].targets.push_back(extra);right.assembly.sources[1].targets[0].marker="right";right.assembly.helpers[0].custom=true;
 std::vector<std::pair<const Root*,std::set<std::pair<std::string,int64_t>>>> variants{{&left,{{"fixture-cab",10}}},{&right,{{"fixture-cab",11}}}};
 auto expected=pristine.assembly;expected.sources[0]=left.assembly.sources[0];expected.sources[1]=right.assembly.sources[1];expected.lods[0].levels[0].renderers={expected.sources[0].targets[0].renderer,expected.sources[0].targets[1].renderer,expected.sources[1].targets[0].renderer};expected.helpers[0].renderers=expected.lods[0].levels[0].renderers;expected.helpers[0].custom=true;
 auto bytes=[](const Description& value){EiemOfflinePackage::Writer writer;EiemOfflinePackage::WriteAssembly(writer,value);return writer.b;};
 Require(bytes(EiemCompiledModSet::MergeAssembly(pristine,variants))==bytes(expected),"single-copy assembly lost bone/LOD/helper/source order");
 Require(bytes(EiemCompiledModSet::MergeAssembly(pristine,{}))==bytes(pristine.assembly),"pristine assembly changed without contributors");
 variants[1].second={{"fixture-cab",10}};Rejected([&]{EiemCompiledModSet::MergeAssembly(pristine,variants);});
 Require(pristine.assembly.sources[0].targets.size()==1&&!pristine.assembly.helpers[0].custom,"assembly composition mutated immutable source roots");
}

void FixedWidthSkipRegression(){
 auto type=Schema(114,{Arr("points",Vec("point")),Arr("matrices",{"Matrix", "matrix",{Vec("a"),Vec("b"),Vec("c")}}),{"UInt8","tail",{}}});
 auto slow=type;PrepareFixedWidths(type);
 for(size_t residue=0;residue<4;++residue)for(size_t count:{size_t(0),size_t(1),size_t(513)}){
  Bytes data(residue);N(data,count,4);data.resize(data.size()+count*12);N(data,count,4);data.resize(data.size()+count*36);N(data,7,1);
  Reader before(data,residue),after(data,residue);SkipValue(slow,0,before,0);SkipValue(type,0,after,0);
  Require(before.p==data.size()&&after.p==before.p,"fixed-width array skip changed object extent");
  data.pop_back();Rejected([&]{Reader r(data,residue);SkipValue(type,0,r,0);});
 }
 auto aligned=Schema(114,{Arr("values",{"Pair","pair",{{"UInt8","flag",{}},{"float","value",{}}}})});
 aligned.nodes[5].align=true;auto generic=aligned;PrepareFixedWidths(aligned);
 for(size_t residue=0;residue<4;++residue){Bytes data(residue);N(data,3,4);
  for(int i=0;i<3;++i){N(data,1,1);while(data.size()%4)data.push_back(0);N(data,i,4);}
  Reader a(data,residue),b(data,residue);SkipValue(generic,0,a,0);SkipValue(aligned,0,b,0);Require(a.p==b.p&&a.p==data.size(),"fixed skip ignored per-element alignment");
 }
 auto malformed=Schema(114,{{"UInt8","flag",{}}});malformed.nodes[0].children={0};Rejected([&]{PrepareFixedWidths(malformed);});
 auto duplicate=Schema(114,{{"UInt8","flag",{}},{"UInt8","flag",{}}});PrepareFixedWidths(duplicate);
 Rejected([&]{DecodeValue(duplicate,0,Bytes{1,2});});
}

Bytes GenericPayload(const Bytes& payload){
 const std::string block="object";Bytes resource(80);
 std::memcpy(resource.data(),"EFFRES",6);WriteInteger(resource,12,37,4);WriteInteger(resource,16,1,4);WriteInteger(resource,20,50+block.size(),4);
 AppendInteger(resource,block.size(),2);resource.insert(resource.end(),block.begin(),block.end());
 auto offset=AlignSize(80+50+block.size(),16);AppendInteger(resource,offset,8);AppendInteger(resource,payload.size(),8);
 auto digest=Sha(payload.data(),payload.size());resource.insert(resource.end(),digest.begin(),digest.end());resource.resize(offset);resource.insert(resource.end(),payload.begin(),payload.end());
 WriteInteger(resource,40,resource.size(),8);digest=Sha(resource.data()+80,resource.size()-80);std::copy(digest.begin(),digest.end(),resource.begin()+48);return resource;
}

void GenericResourceRegression(){
 using namespace EiemCompiledMod;
 auto parent=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
 auto folder=parent/("eff-generic-resource-"+std::to_string(GetCurrentProcessId()));
 Require(std::filesystem::create_directory(folder),"generic resource fixture exists");
 struct Cleanup{std::filesystem::path parent,folder;~Cleanup(){if(folder.parent_path()==parent)std::filesystem::remove_all(folder);}}cleanup{parent,folder};
 std::filesystem::create_directory(folder/"resources");
 auto resource=GenericPayload({3,1,4,1,5});
 EiemResourceReloadPlan::Save(folder/"resources"/"physics.bin",resource);
 std::string ini="[Mod]\nformat=2\nid=Generic\n[Constants]\n[ResourcePhysics]\npath=resources/physics.bin\n";
 EiemResourceReloadPlan::Save(folder/"mod.ini",Bytes(ini.begin(),ini.end()));
 auto package=Load(folder);Require(package.parts.empty()&&package.resources.size()==1&&package.resources.begin()->second->kind==37,"generic payload requires a Mesh/material or lost its kind");
 Writer encoded;WriteAuthor(encoded,package);EiemOfflinePackage::Reader reader{encoded.b};auto decoded=ReadAuthor(reader,parent);
 Require(reader.p==encoded.b.size()&&PackageDigest(decoded)==PackageDigest(package),"generic author resource roundtrip differs");
 Require(EiemCompiledModSet::AuthorStamps(folder).count(folder/"resources"/"physics.bin"),"generic payload excluded from F10 file stamps");
 auto invalid=resource;invalid.back()^=1;Rejected([&]{EiemDirectory::Decode(invalid,0);});
 Rejected([&]{DiskPath(folder,"resources/../escape.bin",0);});
 Rejected([&]{EiemDirectory::Decode(resource,1);});
}

void PristineProofReuseRegression(){
 using namespace EiemCompiledMod;
 Fixture fixture;fixture.cab="CAB-"+std::string(32,'a');
 auto& bundleType=fixture.types[4];auto nameNode=bundleType.nodes.size();bundleType.nodes.push_back({"string","m_Name",false,{}});bundleType.nodes[0].children.push_back(nameNode);auto bundleName=EmptyValue(bundleType,nameNode);SetString(bundleType,bundleName,"fixture-bundle");fixture.V(300).children.push_back(std::move(bundleName));
 auto bytes=fixture.BytesOf();auto bundle=EndfieldInput::Decode(Bundle(bytes,fixture.cab));
 Plan plan;plan.originals.push_back({"fixture",Hash(Bundle(bytes,fixture.cab)),{},0,0});
 plan.names.members[fixture.cab]="CAB-"+std::string(32,'b');
 plan.names.bundles["fixture-bundle"]="merged--bundle";
 auto expected=EiemCompiledModWriter::DescribeInput(0,plan,false,bundle,bundle);
 expected.manifest.plan.originals=plan.originals;
 expected.manifest.plan.names=plan.names;
 std::set<std::string> stages;
 auto reused=EiemCompiledModWriter::DescribeInput(0,plan,false,bundle,bundle,
  [&](const char* name,double){stages.emplace(name);},&expected);
 Require(!stages.count("proof-namespace"),"unchanged pristine namespace proof rebuilt");
 Writer before,after;SchemaWriter bs,as;
 WriteLayout(before,expected.members.at(fixture.cab).before,bs);WriteLayout(after,reused.members.at(fixture.cab).before,as);
 Require(before.b==after.b,"reused pristine layout changed serialized metadata");
 auto names=plan.names;names.members.erase(fixture.cab);names.bundles.clear();
 Require(EiemCompiledModWriter::NamespaceSubset(names,plan.names)&&EiemCompiledModWriter::SelectPristineNamespace(expected.members.at(fixture.cab).pristineNamespace,names).empty(),"removed dependency retained a stale namespace site");
 names=plan.names;names.members["CAB-"+std::string(32,'c')]="CAB-"+std::string(32,'d');
 Require(!EiemCompiledModWriter::NamespaceSubset(names,plan.names),"new dependency reused an incomplete namespace proof");
 auto stale=expected;stale.manifest.plan.originals[0].hash=std::string(64,'0');stages.clear();
 EiemCompiledModWriter::DescribeInput(0,plan,false,bundle,bundle,
  [&](const char* name,double){stages.emplace(name);},&stale);
 Require(stages.count("proof-namespace"),"changed source reused stale namespace proof");
}

void NativeRegistrationRegression(){
 using namespace EiemCompiledMod;
 Fixture f;auto bytes=f.BytesOf();auto layout=ParseSerialized(bytes);
 Bytes originalScript;AppendInteger(originalScript,0,4);AppendInteger(originalScript,200,8);
 bytes=BuildSerialized(bytes,layout,{},{},{},{},{originalScript});layout=ParseSerialized(bytes);
 NativeRegistrations registrations(layout,bytes);
 auto variant=[&](int mod){auto extended=ParseSerialized(bytes);auto type=Schema(114,{Ref("root"),{"float",mod?"radius":"damping",{}}});
  Fixture donor;donor.types={type};donor.objects.clear();donor.Add(-9,0);auto donorBytes=donor.BytesOf();auto parsed=ParseSerialized(donorBytes);auto record=parsed.types.front().nativeRecord;
  WriteInteger(record,5,1,2);WriteInteger(record,parsed.types.front().dependencyOffset-4,1,4);AppendInteger(record,extended.types.size(),4);
  extended.types.push_back(type);extended.externals={"CAB-"+std::string(32,char('C'+mod))};Bytes script;AppendInteger(script,1,4);AppendInteger(script,201+mod,8);
  auto edited=BuildSerialized(bytes,extended,{},{},{},{record},{script});return edited;
 };
 for(int mod=0;mod<2;++mod){auto edited=variant(mod);auto own=ParseSerialized(edited);auto mapping=registrations.Merge(edited,own,{0,size_t(mod+1)});auto target=mapping.back();
  Require(target==layout.types.size()-1&&int16_t(Le(layout.types[target].nativeRecord,5,2))==mod+1,"native script type index was not rebased");
  Require(Le(layout.types[target].nativeRecord,layout.types[target].dependencyOffset,4)==target,"native type dependency was not rebased");
  Require(Le(registrations.scripts.at(mod),0,4)==mod+1,"native script FileID was not rebased");
 }
 auto first=variant(0);auto firstLayout=ParseSerialized(first);auto count=layout.types.size();auto mapping=registrations.Merge(first,firstLayout,{0,1});
 Require(layout.types.size()==count&&mapping.back()==count-2&&registrations.scripts.size()==2,"identical native registration was duplicated");
 layout.externals={"CAB-"+std::string(32,'C'),"CAB-"+std::string(32,'D')};auto merged=BuildSerialized(bytes,layout,{},{},{},registrations.types,registrations.scripts);auto result=ParseSerialized(merged);
 Require(result.types.size()==count&&ScriptRecords(merged,result).size()==3&&ScriptRecords(merged,result).front()==originalScript,"native metadata registration output differs");
 std::set<int64_t> deleted;for(const auto& object:result.objects)deleted.insert(object.first);auto empty=BuildSerialized(merged,result,{},{},deleted);
 Require(ParseSerialized(empty).objects.empty()&&ScriptRecords(empty,ParseSerialized(empty))==ScriptRecords(merged,result),"deleting the final object changed native registration metadata");
 Rejected([&]{VerifyObjectReferences(empty,result,ParseSerialized(empty),merged);});
}

void GenericObjectRegression(){
 using namespace EiemCompiledMod;
 Fixture f;f.cab="CAB-"+std::string(32,'a');f.Add(700,5);
 auto& bundleType=f.types[4];auto nameNode=bundleType.nodes.size();bundleType.nodes.push_back({"string","m_Name",false,{}});bundleType.nodes[0].children.push_back(nameNode);auto bundleName=EmptyValue(bundleType,nameNode);SetString(bundleType,bundleName,"fixture-bundle");f.V(300).children.push_back(std::move(bundleName));
 const auto before=f.BytesOf(),original=Bundle(before,f.cab);auto base=ParseSerialized(before);
 auto parent=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
 auto folder=parent/("eff-generic-objects-"+std::to_string(GetCurrentProcessId()));
 Require(std::filesystem::create_directory(folder),"generic objects fixture exists");
 struct Cleanup{std::filesystem::path parent,folder;~Cleanup(){if(folder.parent_path()==parent)std::filesystem::remove_all(folder);}}cleanup{parent,folder};
 auto file=folder/"input";EiemResourceReloadPlan::Save(file,original);
 EiemCompiledModSet::Set set;EiemCompiledModSet::Catalog catalog;catalog.source={"fixture",Hash(original),file,0,original.size()};{MemberProof proof;proof.before=base;catalog.members[f.cab]=EiemCompiledModSet::PristineMember(proof);}
 set.catalog={catalog};set.contributors={{{0,0},{1,0}}};set.frames.resize(2);set.manifest.plan.names.members[f.cab]="CAB-"+std::string(32,'b');set.manifest.plan.names.bundles["fixture-bundle"]="merged--bundle";
 auto artifact=[&](const Bytes& edited){
  auto a=std::make_shared<Artifact>();auto& p=a->manifest.plan;p.rulesOnly=p.spanRecipes=true;
  p.startupHashes={Hash(EndfieldInput::Encode(original,{{f.cab,0,edited}}))};
  EiemResourceRecipe::Member recipe;recipe.name=f.cab;recipe.hash=Hash(edited);recipe.size=edited.size();recipe.spans={{EiemResourceRecipe::Literal,{},0,edited.size(),edited}};p.memberRecipes={{recipe}};
  EiemLoadingGeneration::Recipe names;EiemLoadingGeneration::PrepareSerialized(edited,set.manifest.plan.names,&names);p.namespaceRecipes={{{f.cab,Hash(edited),std::move(names.patches)}}};
  a->members[f.cab].before=base;a->members[f.cab].after=ParseSerialized(edited);return a;
 };
 for(int mod=0;mod<2;++mod){auto next=f;auto type=next.types.size();
  next.types.push_back(Schema(114,{Ref("m_GameObject"),Ref("root"),{"float",mod?"radius":"damping",{}}}));
  next.types.back().hash[1]=(unsigned char)(mod+1);auto id=int64_t(-500-mod);next.Add(id,type);next.Pointer(id,"m_GameObject",10);next.Pointer(id,"root",11);
  auto& list=ArrayValue(next.T(10),Child(next.T(10),next.V(10),"m_Component"));auto value=ArrayElement(next.T(10),list);SetReference(next.T(10),Child(next.T(10),value,"component"),{f.cab,id},next.refs,f.cab);list.children.push_back(value);list.count=list.children.size();
  if(!mod)next.objects.erase(700);set.mods.push_back(artifact(next.BytesOf()));
 }
 for(bool isolated:{false,true}){auto bundle=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,isolated));const auto& bytes=bundle.members.front().bytes;auto merged=ParseSerialized(bytes);
  Require(merged.types.size()==base.types.size()+2&&merged.objects.count(-500)&&merged.objects.count(-501)&&!merged.objects.count(700),"generic object additions/types/deletion were not composed");
  for(int mod=0;mod<2;++mod){auto id=int64_t(-500-mod);auto value=ObjectValue(bytes,merged,id);auto& type=merged.types.at(merged.objects.at(id).type);
   Require(type.classId==114&&type.hash[1]==mod+1&&Child(type,value,mod?"radius":"damping").raw.size()==4,"new native script schemas were confused");
   Require(NullableReference(type,Child(type,value,"root"),merged,bundle.members.front().name).second==11,"generic new component lost bone reference");
  }
  Require(EiemCompiledModSet::ObjectBytes(bytes,merged.objects.at(100))==EiemCompiledModSet::ObjectBytes(before,base.objects.at(100)),"generic edits changed unselected Mesh bytes");
 }
 for(bool shared:{true,false}){
  auto prepared=set;if(!shared)prepared.contributors[0].resize(1);
  auto expected=EiemCompiledModSet::Materialize(prepared,0,true);
  std::vector<EiemResourceReloadPlan::MemberNamespaceRecipe> names;
  auto startup=EiemCompiledModSet::Materialize(prepared,0,false,&names);
  prepared.manifest.plan.startupHashes={Hash(startup)};
  prepared.catalog[0].source.physical=folder/"unavailable-source";
  auto isolated=EiemCompiledModSet::IsolatePreparedInput(prepared,0,startup,names);
  Require(isolated==expected,"prepared namespace replay changed single/shared native input bytes");
  auto corrupt=startup;corrupt.back()^=1;
  Rejected([&]{EiemCompiledModSet::IsolatePreparedInput(prepared,0,corrupt,names);});
 }
 auto mods=folder/"plugin"/"mods";std::filesystem::create_directories(mods);
 for(size_t m=0;m<set.mods.size();++m){auto mod=mods/("Generic"+std::to_string(m));std::filesystem::create_directories(mod/"resources");
  auto payload=GenericPayload(set.mods[m]->manifest.plan.memberRecipes[0][0].spans.front().literal);EiemResourceReloadPlan::Save(mod/"resources"/"component.bin",payload);
  auto text="[Mod]\nformat=2\nid=Generic"+std::to_string(m)+"\n[Constants]\n[ResourceComponent]\npath=resources/component.bin\n";EiemResourceReloadPlan::Save(mod/"mod.ini",Bytes(text.begin(),text.end()));
  auto a=*set.mods[m];auto& plan=a.manifest.plan;a.manifest.rootAssembly=true;a.manifest.package=Load(mod);a.manifest.baseline=Hash(original);
  plan.key=std::string(64,char('C'+m));plan.author=PackageDigest(a.manifest.package);plan.contract=InputContractDigest(a.manifest.package);
  auto block=a.manifest.package.resources.at("resources/component.bin")->blocks.at("object");plan.memberRecipes[0][0].spans={{EiemResourceRecipe::Author,"resources/component.bin",block.offset,block.size,{}}};
  plan.originals={{"fixture.vfs",Hash(original),file,0,original.size()}};plan.inputs={{"fixture.vfs",plan.startupHashes[0],"fixture-bundle"}};plan.rendererRules.resize(1);plan.closure={0};plan.names=set.manifest.plan.names;
  Root root;root.path="assets/fixture"+std::to_string(m)+".prefab";root.cab=f.cab;root.source=10;plan.roots={root};a.pristine={root};
  auto sourceBundle=EndfieldInput::Decode(original);auto editedBundle=EiemCompiledModSet::Replay(a,0,sourceBundle,{});std::vector<EiemResourceRecipe::Buffer> viewBuffers{{EiemResourceRecipe::Author,"resources/component.bin",&a.manifest.package.resources.at("resources/component.bin")->bytes}};EiemResourceRecipe::Dictionary viewDictionary(std::move(viewBuffers));
  a.views={EiemCompiledInputViewWriter::Build(0,a,original,sourceBundle,editedBundle,viewDictionary)};
  Rejected([&]{EiemCompiledMod::DecodeRuntime({1,2,3},mod,folder);});auto packet=Encode(a,folder);auto corrupt=packet;corrupt.back()^=1;Rejected([&]{EiemCompiledMod::DecodeRuntime(corrupt,mod,folder);});auto roundtrip=EiemCompiledMod::Decode(packet,mod,folder);Require(Encode(roundtrip,folder)==packet&&roundtrip.views.size()==1,"compiled native view codec is not canonical");
  auto runtime=EiemCompiledMod::DecodeRuntime(packet,mod,folder);
  Require(runtime.members.empty()&&runtime.manifest.plan.memberRecipes.at(0).empty(),"runtime reader eagerly restored offline object proofs and member recipes");
  EiemResourceReloadPlan::Save(mod/"compiled.bin",Encode(a,folder));
 }
 auto iniFile=mods/"Generic0/mod.ini";auto savedIni=Read(iniFile);std::atomic<bool> changedDuringRead{false};
 Rejected([&]{EiemCompiledModSet::Read(folder,{}, {},[&](const char* stage,double){if(std::string(stage)=="artifact-manifest"&&!changedDuringRead.exchange(true)){auto changed=savedIni;changed.push_back('\n');Save(iniFile,changed);}});});Save(iniFile,savedIni);
 auto installed=EiemCompiledModSet::Read(folder);Require(installed&&installed->mods.size()==2&&installed->manifest.plan.parts.empty()&&installed->manifest.plan.roots.size()==2&&installed->catalog.front().pristine.size()==2,"shared input lost distinct generic Prefab roots");
 auto stable=EiemCompiledModSet::Read(folder,installed);Require(stable->pointer==installed->pointer&&stable->startup==installed->startup,"unchanged generic set did not reuse prepared inputs");
 auto moved=folder/"Generic0";std::filesystem::rename(mods/"Generic0",moved);auto retired=EiemCompiledModSet::Read(folder,installed);Require(retired->mods.size()==1&&retired->manifest.plan.roots.size()==2&&retired->catalog.front().pristine.size()==2,"generic Mod deletion lost shared original Prefab coverage");
 Require(retired->views.size()==1&&retired->startupViews.size()==1&&retired->isolated.empty()&&retired->startup.empty(),"single contribution rebuilt byte buffers instead of binding its compiled native view");
 Require(retired->manifest.plan.inputs[0].hash.empty()&&retired->manifest.plan.viewIdentities.at(0)==EiemCompiledInputView::Identity(retired->mods.front()->views.at(0),retired->manifest.plan.names)&&EiemInputSpanView::Digest(*retired->startupViews.at(0))==retired->manifest.plan.startupHashes[0],"bound native view byte proof/relocated identity differs");
 auto viewStable=EiemCompiledModSet::Read(folder,retired);Require(viewStable->views==retired->views&&viewStable->startupViews==retired->startupViews,"unchanged native view snapshot did not reuse its bindings");
 auto retiredInput=EndfieldInput::Decode(EiemCompiledModSet::Materialize(*retired,0,false));Require(ParseSerialized(retiredInput.members.front().bytes).objects.count(700),"generic deleting Mod retirement lost original object");
 std::filesystem::rename(moved,mods/"Generic0");auto restored=EiemCompiledModSet::Read(folder,retired);Require(restored->mods.size()==2&&restored->pointer==installed->pointer,"generic Mod addition failed to restore complete set");
 auto saved=set.mods[1];auto changed=f;SetString(changed.T(700),Child(changed.T(700),changed.V(700),"m_Name"),"conflict");set.mods[1]=artifact(changed.BytesOf());
 Rejected([&]{EiemCompiledModSet::Materialize(set,0,false);});set.mods[1]=saved;
 auto missing=f;missing.objects.erase(11);set.mods[1]=artifact(missing.BytesOf());Rejected([&]{EiemCompiledModSet::Materialize(set,0,false);});set.mods[1]=saved;
 auto reused=f;reused.Add(-601,8);reused.Pointer(-601,"m_GameObject",99999);auto bad=artifact(reused.BytesOf());bad->genericResources=true;set.mods[1]=bad;
 Rejected([&]{EiemCompiledModSet::Materialize(set,0,false);});set.mods[1]=saved;
 set.contributors[0]={{1,0}};auto one=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,false));Require(ParseSerialized(one.members.front().bytes).objects.count(700),"removing deleting Mod did not restore original object");
 set.contributors[0].clear();Require(EiemCompiledModSet::Materialize(set,0,false)==original,"empty generic set did not restore pristine input");
}

void PreloadIntervalRegression(){
 using namespace EiemCompiledMod;
 Fixture f;auto& type=f.types[4];auto nameNode=type.nodes.size();type.nodes.push_back({"string","m_Name",false,{}});type.nodes[0].children.push_back(nameNode);auto bundleName=EmptyValue(type,nameNode);SetString(type,bundleName,"fixture-bundle");f.V(300).children.push_back(std::move(bundleName));auto& containers=ArrayValue(type,Child(type,f.V(300),"m_Container"));
 SetInteger(type,Child(type,Child(type,containers.children[0],"second"),"preloadSize"),4);
 SetInteger(type,Child(type,Child(type,containers.children[1],"second"),"preloadIndex"),4);
 SetInteger(type,Child(type,Child(type,containers.children[1],"second"),"preloadSize"),6);
 auto before=EncodeValue(type,f.V(300));std::vector<Bytes> variants;
 for(int mod=0;mod<2;++mod){auto value=f.V(300);auto boundary=size_t(mod?6:4),extra=size_t(mod?1:2);auto& list=ArrayValue(type,Child(type,value,"m_PreloadTable"));std::vector<Value> inserted;
  for(auto id:mod?std::vector<int>{202}:std::vector<int>{200,201}){auto pointer=ArrayElement(type,list);SetReference(type,pointer,{f.cab,id},f.refs,f.cab);inserted.push_back(std::move(pointer));}
  list.children.insert(list.children.begin()+boundary,inserted.begin(),inserted.end());list.count=list.children.size();
  auto update=[&](Value& info){auto first=Integer(Child(type,info,"preloadIndex")),last=first+Integer(Child(type,info,"preloadSize"));auto offset=[&](uint64_t i){return i+(i>=boundary?extra:0);};SetInteger(type,Child(type,info,"preloadIndex"),offset(first));SetInteger(type,Child(type,info,"preloadSize"),offset(last)-offset(first));};
  for(const auto* name:{"m_Container","m_HashContainer"})for(auto& row:ArrayValue(type,Child(type,value,name)).children)update(Child(type,row,"second"));update(Child(type,value,"m_MainAsset"));variants.push_back(EncodeValue(type,value));
 }
 auto merged=DecodeValue(type,0,MergeObject(type,before,variants));const auto& list=ArrayValue(type,Child(type,merged,"m_PreloadTable"));std::vector<int64_t> ids;for(const auto& pointer:list.children)ids.push_back(NullableReference(type,pointer,f.refs,f.cab).second);
 Require(ids==std::vector<int64_t>{10,11,12,13,200,201,20,21,202,22,23,100,200},"preload insertion matching a later original entry lost dependencies");
 const auto& rows=ArrayValue(type,Child(type,merged,"m_Container"));const auto& a=Child(type,rows.children[0],"second");const auto& b=Child(type,rows.children[1],"second");
 Require(Integer(Child(type,a,"preloadIndex"))==0&&Integer(Child(type,a,"preloadSize"))==6&&Integer(Child(type,b,"preloadIndex"))==6&&Integer(Child(type,b,"preloadSize"))==7,"shared preload ranges moved a dependency into another Prefab");
 auto source=f.BytesOf();auto layout=ParseSerialized(source);auto output=BuildSerialized(source,layout,{{300,EncodeValue(type,merged)}});auto after=ParseSerialized(output);auto field=Fields(type,0,source,layout.objects.at(300).offset,source.size(),layout.objects.at(300).offset).at("m_Name");EiemLoadingGeneration::Patch patch{field.offset+4,"fixture-bundle","merged--bundle"};
 std::map<size_t,const Object*> objectSites;for(const auto& object:layout.objects)objectSites.emplace(object.second.offset,&object.second);auto site=EiemCompiledModSet::RelocateSite(patch,source,layout,output,after,{},objectSites);Require(std::equal(patch.before.begin(),patch.before.end(),output.begin()+site),"Bundle namespace field moved after preload composition");
}

void ExternalDependencyRegression(){
 using namespace EiemCompiledMod;
 Fixture f;f.cab="CAB-"+std::string(32,'a');
 auto& bundleType=f.types[4];auto nameNode=bundleType.nodes.size();bundleType.nodes.push_back({"string","m_Name",false,{}});bundleType.nodes[0].children.push_back(nameNode);auto bundleName=EmptyValue(bundleType,nameNode);SetString(bundleType,bundleName,"fixture-bundle");f.V(300).children.push_back(std::move(bundleName));
 const std::string originalCab="CAB-"+std::string(32,'b'),firstCab="CAB-"+std::string(32,'c'),secondCab="CAB-"+std::string(32,'d'),sharedCab="CAB-"+std::string(32,'e');
 auto layout=ParseSerialized(f.BytesOf());layout.externals={originalCab};auto before=BuildSerialized(f.BytesOf(),layout,{});
 auto originalRefs=EiemLoadingGraphAudit::ReadExternals(before,ParseSerialized(before));before[originalRefs.front().guidOffset]=0xA5;
 auto original=Bundle(before,f.cab);auto root=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());auto folder=root/("eff-external-smoke-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 Require(std::filesystem::create_directory(folder),"external fixture already exists");struct Cleanup{std::filesystem::path path,root;~Cleanup(){if(path.parent_path()==root)std::filesystem::remove_all(path);}}cleanup{folder,root};auto file=folder/"input";EiemResourceReloadPlan::Save(file,original);
 EiemCompiledModSet::Set set;EiemCompiledModSet::Catalog catalog;catalog.source={"fixture",Hash(original),file,0,original.size()};{MemberProof proof;proof.before=ParseSerialized(before);catalog.members[f.cab]=EiemCompiledModSet::PristineMember(proof);}set.catalog={catalog};set.contributors={{{0,0},{1,0}}};set.frames.resize(2);
 for(const auto& cab:{f.cab,firstCab,secondCab,sharedCab})set.manifest.plan.names.members[cab]="CAB-"+EiemLoadingGeneration::Token("external-regression",cab,32);
 set.manifest.plan.names.bundles["fixture-bundle"]="merged--bundle";
 for(int mod=0;mod<2;++mod){auto own=ParseSerialized(before);auto id=int64_t(mod?23:13);auto value=ObjectValue(before,own,id);const auto& type=own.types.at(own.objects.at(id).type);auto& materials=ArrayValue(type,Child(type,value,"m_Materials"));
  if(mod)SetReference(type,Child(type,value,"m_ProbeAnchor"),{sharedCab,42},own,f.cab);
  SetReference(type,materials.children.front(),{mod?secondCab:firstCab,-200-mod},own,f.cab);
  if(!mod)SetReference(type,Child(type,value,"m_ProbeAnchor"),{sharedCab,42},own,f.cab);
  auto payload=EncodeValue(type,value);auto edited=BuildSerialized(before,own,{{id,payload}},{{-500-mod,own.objects.at(id).type,payload}});auto a=std::make_shared<Artifact>();auto& plan=a->manifest.plan;plan.spanRecipes=plan.rulesOnly=true;plan.startupHashes={Hash(EndfieldInput::Encode(original,{{f.cab,0,edited}}))};plan.memberRecipes.resize(1);EiemResourceRecipe::Member recipe;recipe.name=f.cab;recipe.hash=Hash(edited);recipe.size=edited.size();recipe.spans={{EiemResourceRecipe::Literal,{},0,edited.size(),edited}};plan.memberRecipes[0].push_back(std::move(recipe));
  EiemLoadingGeneration::Recipe names;EiemLoadingGeneration::PrepareSerialized(edited,set.manifest.plan.names,&names);plan.namespaceRecipes={{{f.cab,Hash(edited),std::move(names.patches)}}};
  a->members[f.cab].before=ParseSerialized(before);a->members[f.cab].after=ParseSerialized(edited);set.mods.push_back(a);
 }
 auto verify=[&](bool isolated){auto bundle=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,isolated));auto& bytes=bundle.members.front().bytes;auto merged=ParseSerialized(bytes);auto name=[&](const std::string& cab){return isolated&&set.manifest.plan.names.members.count(cab)?set.manifest.plan.names.members.at(cab):cab;};
  Require(merged.externals==std::vector<std::string>{name(originalCab),name(firstCab),name(sharedCab),name(secondCab)},"independent external tables were not deduplicated in append order");
  for(int mod=0;mod<2;++mod)for(auto id:{int64_t(mod?23:13),int64_t(-500-mod)}){auto value=ObjectValue(bytes,merged,id);const auto& type=merged.types.at(merged.objects.at(id).type);
   Require(NullableReference(type,ArrayValue(type,Child(type,value,"m_Materials")).children.front(),merged,name(f.cab))==NativeReference{name(mod?secondCab:firstCab),-200-mod},"merged Material FileID points into another Mod's CAB");
   Require(NullableReference(type,Child(type,value,"m_ProbeAnchor"),merged,name(f.cab))==NativeReference{name(sharedCab),42},"shared external PPtr was not rebased");}
  Require(EiemLoadingGraphAudit::ReadExternals(bytes,merged).front().guid=="A5000000000000000000000000000000","original external GUID changed");
  if(!isolated)Require(EiemCompiledModSet::ObjectBytes(bytes,merged.objects.at(100))==EiemCompiledModSet::ObjectBytes(before,ParseSerialized(before).objects.at(100)),"unmodified object bytes changed");
 };
 verify(false);verify(true);
 auto saved=set.contributors[0];set.contributors[0]={{1,0}};auto one=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,false));Require(ParseSerialized(one.members.front().bytes).externals==std::vector<std::string>{originalCab,sharedCab,secondCab},"removed Mod retained its external dependency");
 set.contributors[0].clear();Require(EiemCompiledModSet::Materialize(set,0,false)==original,"empty set did not restore original external metadata");set.contributors[0]=saved;
 auto invalid=ParseSerialized(before);invalid.externals.front()=firstCab;Rejected([&]{BuildSerialized(before,invalid,{});});
}

void SharedInputRegression(){
 using namespace EiemCompiledMod;
 Fixture f;f.cab="CAB-"+std::string(32,'a');f.List(15,"customRenderers",{23});
 auto& fixtureBundleSchema=f.types[4];auto nameNode=fixtureBundleSchema.nodes.size();fixtureBundleSchema.nodes.push_back({"string","m_Name",false,{}});fixtureBundleSchema.nodes[0].children.push_back(nameNode);auto bundleName=EmptyValue(fixtureBundleSchema,nameNode);SetString(fixtureBundleSchema,bundleName,"fixture-bundle");f.V(300).children.push_back(std::move(bundleName));
 auto& levels=ArrayValue(f.T(14),Child(f.T(14),f.V(14),"m_LODs"));auto& renderers=ArrayValue(f.T(14),Child(f.T(14),levels.children[0],"renderers"));auto extra=ArrayElement(f.T(14),renderers);SetReference(f.T(14),Child(f.T(14),extra,"renderer"),{f.cab,23},f.refs,f.cab);renderers.children.push_back(extra);renderers.count=2;
 f.types[5]=Schema(43,{{"string","m_Name",{}},{"StreamingInfo","m_StreamData",{{"UInt64","offset",{}},{"UInt32","size",{}},{"string","path",{}}}}});f.objects.at(100).second=EmptyValue(f.types[5],0);auto& originalStream=Child(f.T(100),f.V(100),"m_StreamData");SetInteger(f.T(100),Child(f.T(100),originalStream,"size"),4);SetString(f.T(100),Child(f.T(100),originalStream,"path"),"archive:/"+f.cab+"/"+f.cab+".resS");
 auto before=f.BytesOf();Bytes stream{1,2,3,4};auto header=Bundle(before,f.cab);auto original=EndfieldInput::Encode(header,{{f.cab,0,before},{f.cab+".resS",0,stream}});
 auto temp=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());auto folder=temp/("eff-compiled-smoke-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(folder),"fixture already exists");struct Cleanup{std::filesystem::path path,root;~Cleanup(){if(path.parent_path()==root)std::filesystem::remove_all(path);}}cleanup{folder,temp};auto file=folder/"input";EiemResourceReloadPlan::Save(file,original);
 EiemCompiledModSet::Set set;EiemCompiledModSet::Catalog catalog;catalog.source={"fixture",Hash(original),file,0,original.size()};{MemberProof proof;proof.before=ParseSerialized(before);catalog.members[f.cab]=EiemCompiledModSet::PristineMember(proof);}set.catalog={catalog};set.contributors={{{0,0},{1,0}}};set.frames.resize(2);set.manifest.plan.names.members={{f.cab,"CAB-"+std::string(32,'b')},{f.cab+".resS","CAB-"+std::string(32,'b')+".resS"}};set.manifest.plan.names.bundles["fixture-bundle"]="merged--bundle";
 for(int mod=0;mod<2;++mod){auto next=f;int64_t renderer=mod?23:13,addedRenderer=mod?523:413,mesh=mod?102:101,child=mod?501:401;next.List(11,"m_Children",{child});next.Add(child,1);next.Pointer(child,"m_GameObject",mod?500:400);next.Add(mod?500:400,0);next.Add(addedRenderer,3);next.Pointer(addedRenderer,"m_GameObject",mod?500:400);next.List(15,"customRenderers",{addedRenderer});auto& lod=ArrayValue(next.T(14),Child(next.T(14),next.V(14),"m_LODs"));auto& list=ArrayValue(next.T(14),Child(next.T(14),lod.children[0],"renderers"));auto value=ArrayElement(next.T(14),list);SetReference(next.T(14),Child(next.T(14),value,"renderer"),{next.cab,addedRenderer},next.refs,next.cab);list.children.insert(list.children.begin()+(mod?2:1),value);list.count=list.children.size();
  next.List(renderer,"m_Materials",{mod?202:201});next.List(300,"m_PreloadTable",{child,addedRenderer,mesh});SetInteger(next.T(300),Child(next.T(300),Child(next.T(300),next.V(300),"m_MainAsset"),"preloadSize"),13);auto& containers=ArrayValue(next.T(300),Child(next.T(300),next.V(300),"m_Container"));for(auto& row:containers.children)SetInteger(next.T(300),Child(next.T(300),Child(next.T(300),row,"second"),"preloadSize"),13);
  next.Add(mesh,5);auto& info=Child(next.T(mesh),next.V(mesh),"m_StreamData");SetInteger(next.T(mesh),Child(next.T(mesh),info,"offset"),4);SetInteger(next.T(mesh),Child(next.T(mesh),info,"size"),mod?5:3);SetString(next.T(mesh),Child(next.T(mesh),info,"path"),"archive:/"+next.cab+"/"+next.cab+".resS");auto edited=next.BytesOf();auto bytes=stream;bytes.insert(bytes.end(),mod?5:3,mod?9:8);auto a=std::make_shared<Artifact>();auto& plan=a->manifest.plan;plan.spanRecipes=plan.rulesOnly=true;plan.startupHashes={Hash(EndfieldInput::Encode(original,{{f.cab,0,edited},{f.cab+".resS",0,bytes}}))};plan.memberRecipes.resize(1);
  for(const auto& member:std::vector<EndfieldInput::Member>{{f.cab,0,edited},{f.cab+".resS",0,bytes}}){EiemResourceRecipe::Member recipe;recipe.name=member.name;recipe.hash=Hash(member.bytes);recipe.size=member.bytes.size();recipe.spans={{EiemResourceRecipe::Literal,{},0,member.bytes.size(),member.bytes}};plan.memberRecipes[0].push_back(std::move(recipe));}
  MemberProof proof;proof.before=ParseSerialized(before);proof.after=ParseSerialized(edited);auto object=ObjectValue(edited,proof.after,mesh);auto& schema=proof.after.types.at(proof.after.objects.at(mesh).type);auto prefix=EncodeValue(schema,Child(schema,object,"m_Name"));proof.streams={{mesh,prefix.size(),8,f.cab+".resS",4}};a->members[f.cab]=proof;
  EiemLoadingGeneration::Recipe names;EiemLoadingGeneration::PrepareSerialized(edited,set.manifest.plan.names,&names);plan.namespaceRecipes={{{f.cab,Hash(edited),std::move(names.patches)}}};set.mods.push_back(a);
 }
 auto merged=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,false));auto layout=ParseSerialized(merged.members[0].bytes);auto& bytes=merged.members[0].bytes;
 Require(layout.objects.count(101)&&layout.objects.count(102)&&layout.objects.count(401)&&layout.objects.count(501),"same-PFB union lost added objects");auto object=ObjectValue(bytes,layout,11);const auto& schema=layout.types[layout.objects.at(11).type];Require(ArrayValue(schema,Child(schema,object,"m_Children")).children.size()==2,"same-PFB children expansion lost a Mod");
 for(auto id:{14,15}){auto value=ObjectValue(bytes,layout,id);const auto& type=layout.types[layout.objects.at(id).type];auto count=id==15?ArrayValue(type,Child(type,value,"customRenderers")).children.size():ArrayValue(type,Child(type,ArrayValue(type,Child(type,value,"m_LODs")).children[0],"renderers")).children.size();Require(count==4,"LOD/helper union lost a Mod");}
 auto bundle=ObjectValue(bytes,layout,300);const auto& bundleType=layout.types[layout.objects.at(300).type];Require(ArrayValue(bundleType,Child(bundleType,bundle,"m_PreloadTable")).children.size()==16&&Integer(Child(bundleType,Child(bundleType,bundle,"m_MainAsset"),"preloadSize"))==16,"preload intervals not recalculated");
 auto mesh=ObjectValue(bytes,layout,102);const auto& meshType=layout.types[layout.objects.at(102).type];Require(Integer(Child(meshType,Child(meshType,mesh,"m_StreamData"),"offset"))==20&&merged.members[1].bytes.size()==25,"second Mod stream offset/alignment differs");
 EiemCompiledModSet::PreparedNamespaces sites;EiemNativeInputOffsets::Layout offsets;
 auto startup=EiemCompiledModSet::Materialize(set,0,false,&sites,&offsets);
 auto isolated=EiemNativeInputOffsets::Relocate(startup,offsets,sites,{},set.manifest.plan.names);
 Require(isolated==EiemCompiledModSet::Materialize(set,0,true),"direct byte relocation differs from full shared object replay");
 Require(EiemNativeInputOffsets::Relocate(isolated,offsets,sites,set.manifest.plan.names,{})==startup,"startup relocation did not restore exact source identities");
 auto second=set.manifest.plan.names;second.members[f.cab]="CAB-"+std::string(32,'c');second.members[f.cab+".resS"]=second.members[f.cab]+".resS";
 Require(EiemNativeInputOffsets::Relocate(isolated,offsets,sites,set.manifest.plan.names,second)==EiemNativeInputOffsets::Relocate(startup,offsets,sites,{},second),"changed generation cannot reuse unchanged native input");
 auto corrupt=isolated;corrupt[offsets.members.front().nameOffset]^=1;Rejected([&]{EiemNativeInputOffsets::Relocate(corrupt,offsets,sites,set.manifest.plan.names,{});});
 auto truncated=offsets;--truncated.size;Rejected([&]{EiemNativeInputOffsets::Relocate(startup,truncated,sites,{},second);});
 auto bad=sites;Require(!bad.empty()&&!bad.front().patches.empty(),"namespace fixture has no byte sites");bad.front().patches.front().offset=startup.size();Rejected([&]{EiemNativeInputOffsets::Relocate(startup,offsets,bad,{},second);});
 auto retired=set;retired.contributors[0].clear();Require(EiemCompiledModSet::Materialize(retired,0,false)==original,"removing all Mods did not restore exact pristine input");
 set.contributors[0].resize(1);auto one=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,false));Require(!ParseSerialized(one.members[0].bytes).objects.count(102),"removed Mod's compiled objects survived");
 Rejected([&]{MergeSequence(std::vector<int>{1,2},std::vector<std::vector<int>>{{3,2},{4,2}},[](int a,int b){return a==b;});});
}
void NativeResidencyRegression(){
 using namespace EiemCompiledModSet;Set set;auto& plan=set.manifest.plan;plan.key=std::string(64,'F');
 for(size_t i=0;i<4;++i){Catalog catalog;auto cab="CAB-"+std::string(32,char('a'+i));catalog.source.hash=std::string(64,'0');catalog.input.bundle="bundle-"+std::to_string(i);catalog.members.emplace(cab,std::make_shared<const MemberProof>());set.catalog.push_back(catalog);
  plan.inputs.push_back(catalog.input);plan.names.members.emplace(cab,"CAB-"+std::string(32,'f'));plan.names.members.emplace(cab+".resS","CAB-"+std::string(32,'f')+".resS");plan.names.bundles.emplace(catalog.input.bundle,"isolated-"+std::to_string(i));plan.startupHashes.push_back(catalog.source.hash);}
 set.sourceNamespaceKeys.assign(set.catalog.size(),plan.key);
 Root root;root.input=0;plan.roots.push_back(root);plan.startupHashes[1]=std::string(64,'1');SelectIsolatedInputs(set);
 Require(plan.closure==std::vector<size_t>({0,1}),"unchanged dependencies were opened as isolated native bundles");
 for(size_t i=0;i<4;++i){auto cab=set.catalog[i].members.begin()->first;bool own=i<2;Require((plan.names.members.at(cab)!=cab)==own,"changed/root and shared CAB identities differ");Require(plan.names.members.at(cab+".resS")==plan.names.members.at(cab)+".resS","stream identity differs from its owning CAB");Require((plan.names.bundles.at(set.catalog[i].input.bundle)!=set.catalog[i].input.bundle)==own,"shared Bundle identity differs");}
 auto names=plan.names.members;SelectIsolatedInputs(set);Require(names==plan.names.members&&plan.closure==std::vector<size_t>({0,1}),"control-only snapshot refresh changed native identity/residency");
 // Removing a Mod still creates an independent pristine Prefab; its old
 // modified resources do not force copies of untouched game dependencies.
 plan.startupHashes[1]=set.catalog[1].source.hash;SelectIsolatedInputs(set);Require(plan.closure==std::vector<size_t>({0}),"retired Mod lost its pristine root or retained unmodified resource copies");
 auto sharedCab=set.catalog[1].members.begin()->first;Require(plan.names.members.at(sharedCab)==sharedCab,"retired author dependency did not return to game identity");
 plan.roots.clear();SelectIsolatedInputs(set);Require(plan.closure.empty(),"unused empty set retained native candidates");
}
void InputSpanViewRegression(){
 using namespace EiemInputSpanView;
 auto sources=std::make_shared<const std::vector<Bytes>>(std::vector<Bytes>{{1,2,3,4,5},{6,7,8,9}});
 auto literal=std::make_shared<const Bytes>(Bytes{10,11});size_t readCalls=0,readBytes=0;
 auto read=[sources,&readCalls,&readBytes](size_t source,size_t offset,unsigned char* target,size_t size){const auto& bytes=sources->at(source);Require(offset<=bytes.size()&&size<=bytes.size()-offset,"view callback requested an invalid source range");++readCalls;readBytes+=size;std::memcpy(target,bytes.data()+offset,size);};
 auto view=std::make_shared<const View>(std::vector<Span>{{0,1,3,{}},{literalSource,0,2,literal},{1,1,3,{}}},std::vector<size_t>{5,4},read);
 Bytes full(view->Size());Require(view->ReadAt(0,full.data(),full.size())==8&&full==Bytes({2,3,4,10,11,7,8,9})&&readBytes==6,"span view read/extent differs");
 Bytes header(2);auto calls=readCalls;Require(view->ReadAt(3,header.data(),2)==2&&header==*literal&&readCalls==calls,"literal read touched an original source");
 Cursor cursor(view);Require(cursor.Seek(-3,2)==5,"view end-relative seek differs");Bytes tail(7);Require(cursor.Read(tail.data(),tail.size())==3&&Bytes(tail.begin(),tail.begin()+3)==Bytes({7,8,9}),"view end clipping differs");
 Require(cursor.Seek(100,0)==100&&!cursor.Read(nullptr,8),"view beyond-end seek/read differs");
 Rejected([&]{cursor.Seek(-101,1);});Rejected([&]{cursor.Seek(INT64_MIN,0);});Rejected([&]{cursor.Seek(0,3);});
 Require(cursor.Seek(INT64_MAX,0)==INT64_MAX,"representable stream position rejected");Rejected([&]{cursor.Seek(1,1);});
 Rejected([&]{View bad({{0,4,2,{}}},{5},read);});Rejected([&]{View bad({{literalSource,1,2,literal}},{},{});});Rejected([&]{View bad({{0,0,0,{}}},{5},read);});
 auto state=std::make_shared<int>(0);auto failing=std::make_shared<const View>(std::vector<Span>{{0,0,1,{}}},std::vector<size_t>{1},[state](size_t,size_t,unsigned char* target,size_t){if(!(*state)++)throw std::runtime_error("read failure");*target=42;});
 Cursor failCursor(failing);unsigned char byte=0;Rejected([&]{failCursor.Read(&byte,1);});Require(failCursor.Seek(0,1)==0&&failCursor.Read(&byte,1)==1&&byte==42,"failed read advanced the stream cursor");
 auto seed=::Bundle(Bytes{1},"seed");Bytes original{10,20,30},compressed{3,10,20,30};EndfieldInput::CryptBlock(compressed);
 auto nativePrefix=std::make_shared<const Bytes>(EiemNativeViewWriter::Prefix(seed,{{3,compressed.size(),5},{3,3,0}},{{"original",0,0,3},{"changed",0,1,5}}));
 auto nativeSources=std::make_shared<const std::vector<Bytes>>(std::vector<Bytes>{compressed,Bytes{40,50,60}});
 size_t nativeReads=0;auto nativeView=std::make_shared<const View>(std::vector<Span>{{literalSource,0,nativePrefix->size(),nativePrefix},{0,0,compressed.size(),{}},{1,0,3,{}}},std::vector<size_t>{compressed.size(),3},[nativeSources,&nativeReads](size_t source,size_t offset,unsigned char* target,size_t count){++nativeReads;std::memcpy(target,nativeSources->at(source).data()+offset,count);});
 Bytes prefixOnly(nativePrefix->size());Require(nativeView->ReadAt(0,prefixOnly.data(),prefixOnly.size())==prefixOnly.size()&&!nativeReads,"native directory read materialized source data");
 Bytes assembled(nativeView->Size());Require(nativeView->ReadAt(0,assembled.data(),assembled.size())==assembled.size()&&nativeReads==2,"native view did not preserve encoded block ranges");
 auto decoded=EndfieldInput::Decode(assembled);Require(decoded.members.size()==2&&decoded.members[0].bytes==original&&decoded.members[1].bytes==Bytes({20,30,40,50,60}),"mixed original compressed/author blocks or overlapping member windows differ");
 auto rawPrefix=EiemNativeViewWriter::Prefix(seed,{{4,4,0}},{{"raw",0,0,4}});auto raw=rawPrefix;raw.insert(raw.end(),{1,2,3,4});
 Require(raw==EndfieldInput::Encode(seed,{{"raw",0,Bytes{1,2,3,4}}}),"uncompressed native view prefix differs from existing encoder");
 Rejected([&]{EiemNativeViewWriter::Prefix(seed,{{4,3,0}},{{"raw",0,0,4}});});Rejected([&]{EiemNativeViewWriter::Prefix(seed,{{4,4,0}},{{"raw",0,3,2}});});
 Rejected([&]{EiemNativeViewWriter::Prefix(seed,{{4,4,0}},{{"raw",0,0,2},{"raw",0,2,2}});});
}
void ProfileOriginalBlocks(const std::filesystem::path& game,const std::filesystem::path& mods){
 using namespace EiemNativeInput;size_t readBytes=0,inputs=0,chunks=0,encoded=0,decoded=0,reusableBytes=0,partialBytes=0;
 std::set<std::string> seen;std::map<size_t,size_t> sizes;
 for(const auto& folder:std::filesystem::directory_iterator(mods))if(folder.is_directory()&&std::filesystem::is_regular_file(folder.path()/"compiled.bin")){
  auto artifact=EiemCompiledMod::Read(folder.path(),game);const auto& plan=artifact.manifest.plan;
  for(size_t input=0;input<plan.originals.size();++input){const auto& source=plan.originals[input];if(!seen.insert(source.file).second)continue;++inputs;
   std::ifstream file(source.physical,std::ios::binary);Require(bool(file),"native block profile source absent");
   auto range=[&](size_t offset,size_t size){Require(offset<=source.size&&size<=source.size-offset,"native block profile range differs");Bytes out(size);file.seekg(std::streamoff(source.offset+offset));file.read((char*)out.data(),std::streamsize(size));Require(bool(file),"native block profile source truncated");readBytes+=size;return out;};
   auto header=EndfieldInput::ReadHeader(range(0,48));Require(header.bytes==source.size,"native block profile header extent differs");
   auto info=range(header.flags&0x80?header.bytes-header.compressedInfo:header.prefix,header.compressedInfo);
   if(header.flags&63){EndfieldInput::CryptBlock(info);info=Lz4(info.data(),info.size(),header.decodedInfo);}
   Reader reader(info,0,true);auto count=EndfieldInput::Count(reader,0x23F77B8A,0x91CE0A4F);Require(count<=(reader.limit-reader.p)/10,"native block profile directory truncated");
   struct Block{size_t offset,size,encoded;};std::vector<Block> blocks;size_t position=0;
   for(size_t i=0;i<count;++i){auto a=uint16_t(reader.Get(2)),b=uint16_t(reader.Get(2)),c=uint16_t(reader.Get(2));reader.Skip(2);auto d=uint16_t(reader.Get(2));auto size=EndfieldInput::Size32(a,c),packed=EndfieldInput::Size32(b,d);blocks.push_back({position,size,packed});position=AddSize(position,size);++chunks;++sizes[size];encoded+=packed;decoded+=size;}
   count=EndfieldInput::Count(reader,0x6B0AE55D,0xE4C1D9F2);std::map<std::string,size_t> members;
   for(size_t i=0;i<count;++i){reader.Skip(8);auto c=uint32_t(reader.Get(4)),d=uint32_t(reader.Get(4));std::string name;for(;;){auto byte=reader.Get(1);if(!byte)break;name+=char(byte^(name.size()^0x97));}reader.Skip(4);Require(members.emplace(name,EndfieldInput::Size64(d,c)).second,"native block profile member duplicate");}
   Require(reader.p==info.size(),"native block profile directory suffix");
   for(const auto& member:plan.memberRecipes.at(input))for(const auto& span:member.spans)if(span.kind==EiemResourceRecipe::Original){
    auto begin=AddSize(members.at(span.source),span.offset),end=AddSize(begin,span.size);size_t reused=0;
    for(const auto& block:blocks)if(block.offset>=begin&&block.offset<=end&&block.size<=end-block.offset){reusableBytes+=block.encoded;reused+=block.size;}
    partialBytes+=span.size-reused;
   }
  }
 }
 std::cout<<"directoryReadBytes="<<readBytes<<" inputs="<<inputs<<" chunks="<<chunks<<" originalEncodedMiB="<<double(encoded)/1048576<<" originalDecodedMiB="<<double(decoded)/1048576<<" reusableEncodedMiB="<<double(reusableBytes)/1048576<<" partialOriginalMiB="<<double(partialBytes)/1048576<<'\n';
 for(const auto& size:sizes)std::cout<<"blockDecodedBytes="<<size.first<<" count="<<size.second<<'\n';
}
void VerifyCompiledView(const EiemCompiledInputView::Recipe& recipe,const EiemCompiledInputView::Buffers& buffers,
 const EiemCompiledMod::Artifact& artifact,size_t index,const EndfieldInput::Bundle& edited,const EiemLoadingGeneration::Names& names){
 auto view=EiemCompiledInputView::Bind(recipe,buffers,names);Bytes bytes(view->Size());Require(view->ReadAt(0,bytes.data(),bytes.size())==bytes.size(),"offline native view read incomplete");auto actual=EndfieldInput::Decode(bytes);
 std::map<std::string,const Bytes*> members;for(const auto& member:actual.members)members.emplace(member.name,&member.bytes);
 for(const auto& member:edited.members)if(EiemResourceReloadPlan::Serialized(member.bytes)){
  auto expectedLayout=ParseSerialized(member.bytes);const auto& actualBytes=*members.at(EiemCompiledInputView::Remap(member.name,names));auto actualLayout=ParseSerialized(actualBytes);
  Require(actualLayout.order==expectedLayout.order&&actualLayout.types.size()==expectedLayout.types.size(),"offline native view object/type order differs");
  for(size_t i=0;i<actualLayout.types.size();++i)Require(actualLayout.types[i].classId==expectedLayout.types[i].classId&&actualLayout.types[i].hash==expectedLayout.types[i].hash,"offline native view schema differs");
  for(auto id:expectedLayout.order){const auto& before=expectedLayout.objects.at(id);const auto& after=actualLayout.objects.at(id);Require(before.size==after.size&&before.type==after.type,"offline native view object record differs");
   Bytes expected(member.bytes.begin()+before.offset,member.bytes.begin()+before.offset+before.size),object(actualBytes.begin()+after.offset,actualBytes.begin()+after.offset+after.size);
   for(const auto& row:artifact.manifest.plan.namespaceRecipes.at(index))if(row.member==member.name)for(const auto& patch:row.patches)if(patch.offset>=before.offset&&patch.offset-before.offset<before.size){auto target=EiemCompiledInputView::Remap(patch.before,names);std::copy(target.begin(),target.end(),expected.begin()+patch.offset-before.offset);}
   for(const auto& site:artifact.members.at(member.name).streams)if(site.object==id)WriteInteger(object,site.offset,site.value,site.width);
   Require(object==expected,"offline native view object bytes differ: "+member.name+":"+std::to_string(id));
  }
  auto expectedReferences=EiemLoadingGraphAudit::ReadReferences(member.bytes,expectedLayout),actualReferences=EiemLoadingGraphAudit::ReadReferences(actualBytes,actualLayout);
  Require(expectedReferences.streams.size()==actualReferences.streams.size()&&expectedReferences.pointers.size()==actualReferences.pointers.size(),"offline native view stream/PPtr coverage differs");
  for(size_t i=0;i<expectedReferences.streams.size();++i){const auto& before=expectedReferences.streams[i];const auto& after=actualReferences.streams[i];Require(before.objectId==after.objectId&&before.field==after.field&&before.size==after.size&&EiemCompiledInputView::Remap(before.path,names)==after.path,"offline native view stream identity differs");if(!before.size)continue;
   auto name=EiemLoadingGraphAudit::Basename(before.path);auto source=std::find_if(edited.members.begin(),edited.members.end(),[&](const auto& row){return row.name==name;});Require(source!=edited.members.end(),"offline native view stream source absent");const auto& target=*members.at(EiemLoadingGraphAudit::Basename(after.path));
   Require(before.offset<=source->bytes.size()&&before.size<=source->bytes.size()-before.offset&&after.offset<=target.size()&&after.size<=target.size()-after.offset,"offline native view streamed payload range differs");
   Require(std::equal(source->bytes.begin()+size_t(before.offset),source->bytes.begin()+size_t(before.offset+before.size),target.begin()+size_t(after.offset)),"offline native view streamed payload differs");
  }
 }
}
void ProfileNativeViews(const std::filesystem::path& game,const std::filesystem::path& mods){
 using namespace EiemCompiledMod;size_t inputs=0,literal=0,original=0,author=0,frameBytes=0,spans=0;std::set<std::string> seen;
 for(const auto& root:EiemComposedPackage::Roots(mods)){auto artifact=Read(root,game);auto frames=std::make_shared<std::map<std::string,Bytes>>();EiemCompiledInputView::Buffers buffers;
  for(const auto& resource:artifact.manifest.package.resources)buffers.emplace(std::make_pair(EiemResourceRecipe::Author,resource.first),std::shared_ptr<const Bytes>(resource.second,&resource.second->bytes));
  for(const auto& frame:artifact.manifest.plan.frameRecipes){auto inserted=frames->emplace(frame.resource,EiemResourceRecipe::ExpandFrame(frame,artifact.manifest.package.resources.at(frame.resource)->bytes));Require(inserted.second,"duplicate offline view frame");frameBytes+=inserted.first->second.size();buffers.emplace(std::make_pair(EiemResourceRecipe::ExpandedFrame,frame.resource),std::shared_ptr<const Bytes>(frames,&inserted.first->second));}
  const auto& plan=artifact.manifest.plan;
  artifact.views.resize(plan.originals.size());artifact.preparedFrames=*frames;
  std::vector<EiemResourceRecipe::Buffer> sourceBuffers;for(const auto& resource:artifact.manifest.package.resources)sourceBuffers.push_back({EiemResourceRecipe::Author,resource.first,&resource.second->bytes});for(const auto& frame:*frames)sourceBuffers.push_back({EiemResourceRecipe::ExpandedFrame,frame.first,&frame.second});EiemResourceRecipe::Dictionary authorDictionary(std::move(sourceBuffers));
  for(size_t i=0;i<plan.originals.size();++i){bool unique=seen.insert(plan.originals[i].file).second;auto input=std::make_shared<const Bytes>(EiemResourceReloadPlan::ReadSource({},plan.originals[i]));buffers[{EiemResourceRecipe::Original,{}}]=input;auto source=EndfieldInput::Decode(*input);auto edited=EiemCompiledModSet::Replay(artifact,i,source,*frames);
   if(!artifact.views.empty()){VerifyCompiledView(artifact.views.at(i),buffers,artifact,i,edited,{});VerifyCompiledView(artifact.views.at(i),buffers,artifact,i,edited,plan.names);}
   auto view=EiemCompiledInputViewWriter::Build(i,artifact,*input,source,edited,authorDictionary);VerifyCompiledView(view,buffers,artifact,i,edited,{});VerifyCompiledView(view,buffers,artifact,i,edited,plan.names);
   if(unique){++inputs;spans+=view.spans.size();for(const auto& span:view.spans){if(span.kind==EiemResourceRecipe::Literal)literal+=span.size;else if(span.kind==EiemResourceRecipe::Original)original+=span.size;else author+=span.size;}}
   artifact.views[i]=std::move(view);
  }auto packet=EiemCompiledMod::Encode(artifact,game);auto decoded=EiemCompiledMod::Decode(packet,root,game);Require(EiemCompiledMod::Encode(decoded,game)==packet,"real native view codec differs after roundtrip");
  std::cout<<"VIEW-VERIFIED mod="<<root.filename().u8string()<<" uniqueInputs="<<inputs<<" previousCompiledMiB="<<double(std::filesystem::file_size(root/"compiled.bin"))/1048576<<" viewCompiledMiB="<<double(packet.size())/1048576<<std::endl;
 }
 std::cout<<"VIEW-PROFILE inputs="<<inputs<<" spans="<<spans<<" literalMiB="<<double(literal)/1048576<<" originalEncodedMiB="<<double(original)/1048576<<" authorReferencedMiB="<<double(author)/1048576<<" offlineFramesMiB="<<double(frameBytes)/1048576<<" streamedPayloadsAndObjects=exact bothNamespaces=1"<<std::endl;
}
void AddressedRecipeRegression(){
 using namespace EiemResourceRecipe;Bytes original(256),payload(128);
 for(size_t i=0;i<original.size();++i)original[i]=uint8_t(i*13+7);
 for(size_t i=0;i<payload.size();++i)payload[i]=uint8_t(i*17+11);
 auto resource=std::make_shared<Resource>();resource->bytes=payload;Package package;package.resources.emplace("author",resource);
 Dictionary source({{Original,"member",&original}}),author({{Author,"author",&payload}});
 EndfieldInput::Member member{"member",0,Bytes{1,2,3,4,5,6,7}};
 member.bytes.insert(member.bytes.end(),original.begin()+64,original.begin()+160);
 member.bytes.insert(member.bytes.end(),payload.begin(),payload.end());
 member.bytes.insert(member.bytes.end(),original.begin()+160,original.end());member.bytes.push_back(99);
 std::vector<OriginalSpan> known{{7,0,64,96},{231,0,160,96}};
 auto recipe=Describe(member,source,author,known);std::map<std::string,const Bytes*> inputs{{"member",&original}};
 Require(Apply(recipe,inputs,package,{})==member.bytes,"addressed original/author/literal reconstruction differs");
 auto generic=Describe(member,source,author);Require(Apply(generic,inputs,package,{})==Apply(recipe,inputs,package,{}),"addressed and searched recipes differ");
 auto invalid=known;invalid[0].offset=65;Rejected([&]{Describe(member,source,author,invalid);});
 invalid=known;invalid[1].output=8;Rejected([&]{Describe(member,source,author,invalid);});
 invalid=known;invalid[0].buffer=1;Rejected([&]{Describe(member,source,author,invalid);});
 invalid=known;invalid[0].size=SIZE_MAX;Rejected([&]{Describe(member,source,author,invalid);});
}
void ValueEqualityRegression(){
 using namespace EiemCompiledMod;Fixture fixture;
 for(const auto& row:fixture.objects){const auto& type=fixture.T(row.first);auto original=fixture.V(row.first);auto check=[&](const Value& variant){Require(EqualCompiledValue(type,original,variant)==(EncodeValue(type,original)==EncodeValue(type,variant)),"compiled typed equality differs from serialized byte equality");};check(original);
  auto padding=original;std::function<void(Value&)> pad=[&](Value& value){value.padding.push_back(71);for(auto& child:value.children)pad(child);};pad(padding);check(padding);
  auto changed=original;std::function<bool(Value&)> edit=[&](Value& value){if(!value.raw.empty()){value.raw.front()^=1;return true;}for(auto& child:value.children)if(edit(child))return true;return false;};if(edit(changed))check(changed);
 }
}
void SourceSnapshotRegression(){
 using namespace EiemResourceReloadPlan;
 const auto root=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());auto folder=root/("eff-source-snapshot-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 Require(std::filesystem::create_directory(folder),"source snapshot fixture exists");struct Cleanup{std::filesystem::path root,path;~Cleanup(){if(path.parent_path()==root)std::filesystem::remove_all(path);}}cleanup{root,folder};
 auto path=folder/"source";Bytes bytes{0,1,2,3,4,5};Save(path,bytes);EiemResourceReloadPlan::Source source{"source",Hash(Bytes{1,2,3}),path,1,3};auto stamp=FileStamp(path);
 VerifySourceSnapshot({},source,stamp);
 bytes.push_back(6);Save(path,bytes);VerifySourceSnapshot({},source,stamp);
 bytes[2]=99;Save(path,bytes);Rejected([&]{VerifySourceSnapshot({},source,stamp);});
}
void ReadonlyRangeRegression(){
 using namespace EiemResourceReloadPlan;auto folder=std::filesystem::absolute("bin")/("mapped-source-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(folder),"mapped fixture exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::filesystem::remove_all(path);}}cleanup{folder};auto path=folder/"source";Bytes bytes(100003);for(size_t i=0;i<bytes.size();++i)bytes[i]=uint8_t(i*37);Save(path,bytes);
 auto mapped=std::make_shared<const EiemReadonlyFileRange::Range>(path,999,90000);Require(SameStamp(mapped->Stamp(),FileStamp(path)),"mapped stamp differs");EiemCompiledInputView::Buffer backing(mapped);Bytes actual(90000);backing.Read(0,actual.data(),actual.size());Require(std::equal(actual.begin(),actual.end(),bytes.begin()+999),"unaligned mapped source bytes differ");
 Rejected([&]{backing.Read(90000,actual.data(),1);});auto write=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);Require(write==INVALID_HANDLE_VALUE,"mapped source allowed live overwrite");
 Rejected([&]{EiemReadonlyFileRange::Range bad(path,100000,4);});mapped.reset();backing={};Save(path,{1,2,3});
}
void CompareCompiledArtifacts(const std::filesystem::path& game,const std::filesystem::path& beforeRoot,const std::filesystem::path& afterRoot){
 auto before=EiemCompiledMod::Read(beforeRoot,game),after=EiemCompiledMod::Read(afterRoot,game);
 const auto& a=before.manifest.plan;const auto& b=after.manifest.plan;
 Require(a.author==b.author&&a.contract==b.contract&&a.key==b.key,"author/contract/key differs");
 Require(a.originals.size()==b.originals.size()&&a.inputs.size()==b.inputs.size()&&a.roots.size()==b.roots.size(),"compiled source/root coverage differs");
 std::cout<<"before bounds="<<a.rendererBoundsHits<<'/'<<a.rendererBoundsMisses<<" after="<<b.rendererBoundsHits<<'/'<<b.rendererBoundsMisses<<'\n';
 EiemPrefabRules::Context oldReplay(std::make_shared<const EiemResourceReloadPlan::Plan>(a),before.manifest.package),newReplay(std::make_shared<const EiemResourceReloadPlan::Plan>(b),after.manifest.package);
 for(size_t i=0;i<a.originals.size();++i){
  Require(a.originals[i].file==b.originals[i].file&&a.originals[i].hash==b.originals[i].hash,"compiled original identity differs");
  Require(a.startupHashes.at(i)==b.startupHashes.at(i)&&a.inputs[i].hash==b.inputs[i].hash,"compiled startup/isolated hash differs");
  Require(oldReplay.Materialize(i,false)==newReplay.Materialize(i,false),"compiled startup replay changed bytes");
  Require(oldReplay.Materialize(i,true)==newReplay.Materialize(i,true),"compiled isolated replay changed bytes");
 }
 Require(a.names.members==b.names.members&&a.names.bundles==b.names.bundles,"compiled namespace identities differ");
 std::cout<<"PASS equivalent compiled startup and isolated replay: inputs="<<a.inputs.size()<<" roots="<<a.roots.size()<<'\n';
}
int main(int argc,char** argv){try{
 if(argc==5&&std::string(argv[1])=="--write-runtime-index"){
  using namespace EiemCompiledMod;auto game=std::filesystem::u8path(argv[2]),source=std::filesystem::u8path(argv[3]),destination=std::filesystem::u8path(argv[4]);
  Require(std::filesystem::weakly_canonical(source)!=std::filesystem::weakly_canonical(destination),"runtime index validation must use a separate directory");
  auto artifact=Read(source,game);auto packet=Encode(artifact,game);auto runtime=DecodeRuntime(packet,source,game);
  Require(runtime.members.empty()&&runtime.views.size()==artifact.views.size(),"runtime index eagerly decoded object proofs");
  auto full=EiemCompiledMod::Decode(packet,source,game);Require(Encode(full,game)==packet,"runtime index full roundtrip differs");
  EiemOfflineCompile::AtomicSave(destination/"compiled.bin",packet);
  std::cout<<"RUNTIME-INDEX mod="<<artifact.manifest.package.id<<" bytes="<<packet.size()<<" sources="<<runtime.inputMembers.size()<<" nativeViews="<<runtime.views.size()<<" eagerProofs="<<runtime.members.size()<<'\n';return 0;
 }
 if(argc==4&&std::string(argv[1])=="--audit-shared-inputs"){
  struct Use {std::string mod,bundle,original,output;};std::map<std::string,std::vector<Use>> uses;
  for(const auto& root:EiemComposedPackage::Roots(std::filesystem::u8path(argv[3]))){
   auto artifact=EiemCompiledMod::Read(root,std::filesystem::u8path(argv[2]));const auto& plan=artifact.manifest.plan;
   for(size_t i=0;i<plan.originals.size();++i)uses[plan.originals[i].file].push_back({artifact.manifest.package.id,plan.inputs.at(i).bundle,plan.originals[i].hash,plan.startupHashes.at(i)});
  }
  size_t shared=0,readOnly=0,singleWriter=0,multipleWriters=0;
  for(const auto& row:uses)if(row.second.size()>1){++shared;size_t changed=0;std::string writers,readers;
   for(const auto& use:row.second){Require(use.original==row.second.front().original,"shared input baseline differs");auto& names=use.original==use.output?readers:writers;if(!names.empty())names+=",";names+=use.mod;changed+=use.original!=use.output;}
   if(!changed)++readOnly;else if(changed==1)++singleWriter;else ++multipleWriters;
   std::cout<<"SHARED input="<<row.first<<" bundle="<<row.second.front().bundle<<" changedMods="<<changed<<" writers="<<writers<<" readers="<<readers<<'\n';
  }
  std::cout<<"SHARED-TOTAL inputs="<<uses.size()<<" shared="<<shared<<" readOnly="<<readOnly<<" singleWriter="<<singleWriter<<" multipleWriters="<<multipleWriters<<" gameLoads=0 writes=0\n";return 0;
 }
 AddressedRecipeRegression();
 SourceSnapshotRegression();ReadonlyRangeRegression();ValueEqualityRegression();
 if(argc==5&&std::string(argv[1])=="--compare-artifacts"){CompareCompiledArtifacts(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[4]));return 0;}
 InputSpanViewRegression();
 if(argc==2&&std::string(argv[1])=="--input-view-smoke"){std::cout<<"PASS compiled input span views: seek/read bounds, source/literal ownership, failed reads, native directory ABI, unchanged encoded blocks, overlapping windows\n";return 0;}
 if(argc==4&&std::string(argv[1])=="--profile-original-blocks"){ProfileOriginalBlocks(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]));return 0;}
 if(argc==4&&std::string(argv[1])=="--profile-native-views"){ProfileNativeViews(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]));return 0;}
 AssemblySelectionRegression();
 if(argc==4&&std::string(argv[1])=="--profile-set"){
  std::mutex mutex;std::map<std::string,double> costs;auto start=std::chrono::steady_clock::now();
  auto set=EiemCompiledModSet::Read(std::filesystem::u8path(argv[2]),{},std::filesystem::u8path(argv[3]),[&](const char* stage,double ms){std::lock_guard<std::mutex> lock(mutex);costs[stage]+=ms;});
  Require(bool(set),"profile has no compiled Mods");std::cout<<"PROFILE wallMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<std::endl;
  for(const auto& row:costs)std::cout<<"PROFILE stage="<<row.first<<" workerSumMs="<<row.second<<std::endl;
  size_t shared=0,sourceBytes=0,viewBytes=0;for(size_t i=0;i<set->catalog.size();++i){if(set->contributors.at(i).size()>1)++shared;sourceBytes+=set->catalog[i].source.size;}
  for(const auto& row:set->views)viewBytes+=row.second->Size();
  std::cout<<"PROFILE inputs="<<set->catalog.size()<<" shared="<<shared<<" nativeViews="<<set->views.size()<<" sourceMiB="<<double(sourceBytes)/1048576<<" viewMiB="<<double(viewBytes)/1048576<<std::endl;
  start=std::chrono::steady_clock::now();auto unchanged=EiemCompiledModSet::Read(std::filesystem::u8path(argv[2]),set,std::filesystem::u8path(argv[3]));Require(unchanged&&unchanged->pointer==set->pointer,"unchanged profile identity differs");
  std::cout<<"PROFILE unchangedRefreshMs="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" gameLoads=0 authorBuilders=0 writes=0"<<std::endl;return 0;
 }
 if(argc==5&&std::string(argv[1])=="--compare-artifacts"){
  using namespace EiemCompiledMod;auto game=std::filesystem::u8path(argv[2]);
  auto left=Read(std::filesystem::u8path(argv[3]),game),right=Read(std::filesystem::u8path(argv[4]),game);
  Require(Encode(left,game)==Encode(right,game),"canonical compiled artifacts differ");
  std::cout<<"PASS canonical compiled artifacts; staging folder names excluded from comparison\n";
  return 0;
 }
 PristineProofReuseRegression();
 if(argc==4&&std::string(argv[1])=="--profile-root-proof"){
  using namespace EiemCompiledMod;
  using Clock=std::chrono::steady_clock;
  auto elapsed=[](Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();};
  auto started=Clock::now();auto artifact=Read(std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[2]));auto& plan=artifact.manifest.plan;
  std::map<size_t,size_t> counts;for(const auto& root:plan.roots)++counts[root.input];Require(!counts.empty(),"profile has no Prefab roots");
  std::cout<<"PROFILE inputs="<<plan.originals.size()<<" roots="<<plan.roots.size()<<" rootInputs="<<counts.size()<<" artifactReadMs="<<elapsed(started)<<std::endl;
  EiemPrefabRules::Context replay(std::make_shared<const Plan>(plan),artifact.manifest.package);double totalSource=0,totalReplay=0,totalWithout=0,totalWith=0;
  for(const auto& input:counts){size_t i=input.first;
  started=Clock::now();auto original=EndfieldInput::Decode(ReadSource({},plan.originals.at(i)));auto sourceMs=elapsed(started);
  started=Clock::now();auto edited=EndfieldInput::Decode(replay.Materialize(i,false));auto replayMs=elapsed(started);
  size_t objects=0;for(const auto& member:original.members)if(Serialized(member.bytes))objects+=ParseSerialized(member.bytes).objects.size();
  started=Clock::now();auto without=EiemCompiledModWriter::DescribeInput(i,plan,false,original,edited);auto withoutMs=elapsed(started);
  started=Clock::now();auto with=EiemCompiledModWriter::DescribeInput(i,plan,true,original,edited);auto withMs=elapsed(started);
  Require(with.pristine.size()==without.pristine.size()&&with.members.size()==without.members.size(),"profile proof coverage changed");
  std::cout<<"PROFILE input="<<i<<" objects="<<objects<<" roots="<<with.pristine.size()<<" sourceMs="<<sourceMs<<" replayMs="<<replayMs<<" withoutAssemblyMs="<<withoutMs<<" withAssemblyMs="<<withMs<<std::endl;
  totalSource+=sourceMs;totalReplay+=replayMs;totalWithout+=withoutMs;totalWith+=withMs;
  }
  std::cout<<"PROFILE-TOTAL sourceMs="<<totalSource<<" replayMs="<<totalReplay<<" withoutAssemblyMs="<<totalWithout<<" withAssemblyMs="<<totalWith<<" rootAssemblyExtraMs="<<totalWith-totalWithout<<" readOnly=1"<<std::endl;return 0;
 }
 if(argc==4&&std::string(argv[1])=="--residency-audit"){
  auto set=EiemCompiledModSet::Read(std::filesystem::u8path(argv[2]),{},std::filesystem::u8path(argv[3]));Require(bool(set),"residency audit has no Mods");
  const auto& plan=set->manifest.plan;std::set<size_t> roots;for(const auto& root:plan.roots)roots.insert(root.input);
  size_t unnecessary=0,bytes=0,shared=0;for(size_t i=0;i<plan.inputs.size();++i)if(plan.startupHashes.at(i)==set->catalog.at(i).source.hash&&!roots.count(i)){++shared;bytes+=set->catalog.at(i).source.size;if(std::find(plan.closure.begin(),plan.closure.end(),i)!=plan.closure.end())++unnecessary;}
  std::cout<<"inputs="<<plan.inputs.size()<<" nativeInputs="<<plan.closure.size()<<" rootInputs="<<roots.size()<<" unchangedDependencyCopies="<<unnecessary<<" sharedDependencies="<<shared<<" sharedSourceMiB="<<double(bytes)/1048576<<std::endl;
  Require(!unnecessary,"background candidate duplicates unchanged game dependency bundles");
  for(size_t i=0;i<plan.inputs.size();++i)if(std::find(plan.closure.begin(),plan.closure.end(),i)==plan.closure.end()){
   Require(!set->isolated.count(i),"unmodified shared dependency retained an isolated payload");
   for(const auto& member:set->catalog.at(i).members)Require(plan.names.members.at(member.first)==member.first,"shared dependency CAB was renamed");
  }
  auto refreshed=EiemCompiledModSet::Read(std::filesystem::u8path(argv[2]),set,std::filesystem::u8path(argv[3]));
  Require(refreshed&&refreshed->manifest.plan.closure==plan.closure&&refreshed->manifest.plan.names.members==plan.names.members&&refreshed->isolated.size()==set->isolated.size(),"unchanged snapshot refresh restored duplicate native inputs");
  std::cout<<"PASS native candidates retain only edited/root inputs; unchanged game dependencies keep their CAB identities through snapshot refresh"<<std::endl;return 0;
 }
 if(argc==4&&std::string(argv[1])=="--refresh-controls"){
  auto game=std::filesystem::u8path(argv[2]),source=std::filesystem::u8path(argv[3]),parent=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());auto folder=parent/("eff-compiled-refresh-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(folder),"refresh fixture exists");struct Cleanup{std::filesystem::path parent,folder;~Cleanup(){if(folder.parent_path()==parent)std::filesystem::remove_all(folder);}}cleanup{parent,folder};auto mods=folder/"mods",mod=mods/source.filename();std::filesystem::create_directories(mod);std::filesystem::copy(source,mod,std::filesystem::copy_options::recursive);
  auto before=EiemCompiledModSet::Read(game,{},mods);Require(before&&!before->mods.empty()&&!before->startup.empty(),"refresh fixture has no compiled inputs");auto enabled=before->manifest.packages.front().keySwitchEnabled;auto ini=EiemNativeInput::Read(mod/"mod.ini");std::string text(ini.begin(),ini.end());auto changed=std::regex_replace(text,std::regex("key_switch_enabled[ \\t]*=[^\\r\\n]*"),enabled?"key_switch_enabled=0":"key_switch_enabled=1");Require(text!=changed,"refresh fixture lacks key switch setting");EiemOfflineCompile::AtomicSave(mod/"mod.ini",Bytes(changed.begin(),changed.end()));
  auto clock=std::chrono::steady_clock::now();auto after=EiemCompiledModSet::Read(game,before,mods);double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-clock).count();Require(after->pointer==before->pointer&&after->manifest.packages.front().keySwitchEnabled!=enabled&&EiemCompiledModSet::Unchanged(*after,mods),"control-only INI edit changed resource identity or retained stale controls/stamps");
  Require(after->startup==before->startup&&after->isolated==before->isolated,"control-only refresh rebuilt native resource payloads");before->isolated.clear();after->isolated.clear();auto released=EiemCompiledModSet::Read(game,after,mods);
  std::vector<std::weak_ptr<const Bytes>> backing;for(const auto& row:after->startup){Require(released->isolated.at(row.first)==row.second,"control-only refresh duplicated the shared native backing");backing.push_back(row.second);}
  before->startup.clear();after->startup.clear();released->startup.clear();released->isolated.clear();for(const auto& row:backing)Require(row.expired(),"released preparation records retained native backing bytes");auto drained=EiemCompiledModSet::Read(game,released,mods);Require(drained->isolated.empty(),"refresh resurrected expired native byte ownership");
  auto resource=mod/std::filesystem::u8path(released->manifest.packages.front().resources.begin()->first);auto corrupt=EiemNativeInput::Read(resource);corrupt.back()^=1;EiemOfflineCompile::AtomicSave(resource,corrupt);Rejected([&]{EiemCompiledModSet::Read(game,released,mods);});Require(!EiemCompiledModSet::Unchanged(*released,mods),"real resource edit bypassed control-only refresh validation");
  std::cout<<"PASS control-only INI refresh shares prepared bytes, updates controls/stamps, preserves consumed-input lifetime, rejects corrupt resource changes; refreshMs="<<elapsed<<'\n';return 0;
 }
 if(argc==4&&std::string(argv[1])=="--bundle-names"){
  std::set<std::string> names;for(const auto& mod:EiemComposedPackage::Roots(std::filesystem::u8path(argv[3]))){auto artifact=EiemCompiledMod::Read(mod,std::filesystem::u8path(argv[2]));for(const auto& input:artifact.manifest.plan.inputs)names.insert(input.bundle);}for(const auto& name:names)std::cout<<name<<'\n';return 0;
 }
 using namespace EiemCompiledMod;
 if(argc==4&&std::string(argv[1])=="--audit-added-dependencies"){
  auto artifact=Read(std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[2]));size_t edges=0;
  for(size_t i=0;i<artifact.manifest.plan.inputs.size();++i){const auto& input=artifact.manifest.plan.inputs[i];
   for(const auto& member:artifact.members){if(i>=artifact.manifest.plan.namespaceRecipes.size())continue;const auto& recipes=artifact.manifest.plan.namespaceRecipes.at(i);if(std::none_of(recipes.begin(),recipes.end(),[&](const auto& r){return r.member==member.first;}))continue;
    for(size_t n=member.second.before.externals.size();n<member.second.after.externals.size();++n){auto cab=member.second.after.externals[n];std::cout<<"from="<<input.bundle<<" file="<<input.file<<" adds="<<cab<<'\n';++edges;}
   }
  }std::cout<<"addedExternalEdges="<<edges<<'\n';return 0;
 }
 if(argc==4&&std::string(argv[1])=="--audit-model-nodes"){
  auto artifact=Read(std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[2]));auto& plan=artifact.manifest.plan;
  EiemPrefabRules::Context replay(std::make_shared<const EiemResourceReloadPlan::Plan>(plan),artifact.manifest.package);size_t nodes=0,fields=0;
  for(size_t i=0;i<plan.originals.size();++i){auto original=EndfieldInput::Decode(ReadSource({},plan.originals[i]));auto edited=EndfieldInput::Decode(replay.Materialize(i,false));
   for(const auto& member:original.members)if(Serialized(member.bytes)){
    auto before=ParseSerialized(member.bytes);auto afterMember=std::find_if(edited.members.begin(),edited.members.end(),[&](const auto& m){return m.name==member.name;});Require(afterMember!=edited.members.end(),"original model member removed");auto after=ParseSerialized(afterMember->bytes);
    for(const auto& object:before.objects){const auto& type=before.types.at(object.second.type);if(type.classId!=1&&type.classId!=4)continue;
     Require(after.objects.count(object.first),"original GameObject/Transform removed");const auto& changedType=after.types.at(after.objects.at(object.first).type);Require(changedType.classId==type.classId,"original model node type changed");
     auto native=ObjectValue(member.bytes,before,object.first),changed=ObjectValue(afterMember->bytes,after,object.first);
     for(auto field:type.nodes.front().children){const auto& name=type.nodes.at(field).name;if(name=="m_Children"||name=="m_Component")continue;
      Require(EncodeValue(type,Child(type,native,name))==EncodeValue(changedType,Child(changedType,changed,name)),"original model node field changed: "+member.name+":"+std::to_string(object.first)+"/"+name);++fields;
     }++nodes;
    }
   }
  }
  Require(nodes>0,"model node audit did not inspect any nodes");std::cout<<"PASS original GameObject/Transform identities, names, parent and local pose preserved: nodes="<<nodes<<" fields="<<fields<<'\n';return 0;
 }
 if(argc==4&&std::string(argv[1])=="--audit-unmodified-skinning"){
  auto artifact=Read(std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[2]));auto& plan=artifact.manifest.plan;
  EiemPrefabRules::Context replay(std::make_shared<const EiemResourceReloadPlan::Plan>(plan),artifact.manifest.package);
  size_t meshes=0,renderers=0,scripts=0,fields=0;
  for(size_t i=0;i<plan.originals.size();++i){
   if(plan.startupHashes.at(i)==plan.originals[i].hash)continue;
   auto original=EndfieldInput::Decode(ReadSource({},plan.originals[i]));auto edited=EndfieldInput::Decode(replay.Materialize(i,false));
   std::set<NativeReference> selectedRenderers;for(const auto& rule:plan.rendererRules.at(i))selectedRenderers.emplace(rule.cab,rule.id);
   for(const auto& member:original.members)if(Serialized(member.bytes)){
    auto before=ParseSerialized(member.bytes);auto found=std::find_if(edited.members.begin(),edited.members.end(),[&](const auto& m){return m.name==member.name;});Require(found!=edited.members.end(),"unmodified skinning member removed");auto after=ParseSerialized(found->bytes);
    std::set<int64_t> selectedMeshes;for(const auto& asset:SerializedAssets(member.bytes,member.name,before))if(asset.classId==43&&artifact.manifest.package.targetIndex.count(Identity{asset.path,asset.asset}))selectedMeshes.insert(asset.pathId);
    for(const auto& row:before.objects){const auto& object=row.second;const auto& type=before.types.at(object.type);auto id=row.first;
     if(type.classId!=137&&type.classId!=43&&type.classId!=114)continue;
     if((type.classId==137&&selectedRenderers.count({member.name,id}))||(type.classId==43&&selectedMeshes.count(id)))continue;
     Require(after.objects.count(id),"unmodified skinning object removed");const auto& changed=after.objects.at(id);const auto& changedType=after.types.at(changed.type);Require(changedType.classId==type.classId,"unmodified skinning object type changed");
     auto label=member.name+":"+std::to_string(id);
     if(type.classId!=114){Require(changed.size==object.size&&std::equal(member.bytes.begin()+object.offset,member.bytes.begin()+object.offset+object.size,found->bytes.begin()+changed.offset),"unselected Mesh/SkinnedMeshRenderer changed: "+label);if(type.classId==43)++meshes;else ++renderers;continue;}
     // Only the game's explicit Renderer reference list is expanded. Animation,
     // cloth, bone-index caches and every other script field must remain intact.
     if(changed.size==object.size&&std::equal(member.bytes.begin()+object.offset,member.bytes.begin()+object.offset+object.size,found->bytes.begin()+changed.offset)){++scripts;fields+=type.nodes.front().children.size();continue;}
     auto native=ObjectValue(member.bytes,before,id),candidate=ObjectValue(found->bytes,after,id);
     for(auto field:type.nodes.front().children){const auto& name=type.nodes.at(field).name;if(name=="customRenderers")continue;
      Require(EncodeValue(type,Child(type,native,name))==EncodeValue(changedType,Child(changedType,candidate,name)),"original script field changed: "+label+"/"+name);++fields;
     }++scripts;
    }
   }
  }
  Require(meshes&&renderers&&scripts,"unmodified skinning audit did not cover Mesh, Renderer and scripts");std::cout<<"PASS unselected Mesh/SkinnedMeshRenderer bytes and script fields preserved: meshes="<<meshes<<" renderers="<<renderers<<" scripts="<<scripts<<" fields="<<fields<<'\n';return 0;
 }
 if(argc==4&&std::string(argv[1])=="--audit-materials"){
  auto artifact=Read(std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[2]));size_t count=0;
  for(const auto& material:artifact.manifest.plan.materials)for(const auto& slot:material.second.slots)if(slot.nonnull){
   Require(slot.local,"authored Material dependency absent from compiled inputs: "+material.first+"/"+slot.property);++count;
  }
  std::cout<<"PASS authored material texture dependencies: "<<count<<'\n';return 0;
 }
 if(argc==4&&std::string(argv[1])=="--audit-shaders"){
  auto artifact=std::make_shared<Artifact>(Read(std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[2])));auto& plan=artifact->manifest.plan;
  EiemPrefabRules::Context replay(std::make_shared<const EiemResourceReloadPlan::Plan>(plan),artifact->manifest.package);
  std::set<NativeReference> shaders;std::set<Identity> checked;size_t preserved=0,assembly=0;
  for(const auto& root:plan.roots){Require(!root.assembly.sources.empty(),"compiled root lacks exact Renderer/bone address descriptor");++assembly;}
  for(size_t i=0;i<plan.originals.size();++i){auto original=EndfieldInput::Decode(ReadSource({},plan.originals[i]));auto edited=EndfieldInput::Decode(replay.Materialize(i,false));
   for(const auto& member:edited.members)if(Serialized(member.bytes)){auto layout=ParseSerialized(member.bytes);auto old=std::find_if(original.members.begin(),original.members.end(),[&](const auto& m){return m.name==member.name;});Require(old!=original.members.end(),"material original member missing");auto before=ParseSerialized(old->bytes);
    for(const auto& asset:SerializedAssets(old->bytes,member.name,before))if(asset.classId==21){auto source=Identity{asset.path,asset.asset};
     for(const auto& material:artifact->manifest.package.materials)if(material.second.source.path==source.path&&material.second.source.asset==source.asset){auto id=AllocateMaterialId(member.name,artifact->manifest.package,material.first);const auto& type=layout.types.at(layout.objects.at(id).type);auto value=ObjectValue(member.bytes,layout,id);auto native=ObjectValue(old->bytes,before,asset.pathId);auto shader=NullableReference(type,Child(type,value,"m_Shader"),layout,member.name);
      Require(shader==NullableReference(type,Child(type,native,"m_Shader"),before,member.name),"compiled material Shader differs from pristine");
      for(const char* field:{"m_ValidKeywords","m_InvalidKeywords","disabledShaderPasses"}){auto it=std::find_if(type.nodes.begin(),type.nodes.end(),[&](const auto& node){return node.name==field;});if(it!=type.nodes.end())Require(EncodeValue(type,Child(type,value,field))==EncodeValue(type,Child(type,native,field)),std::string("compiled Shader keyword/pass field changed: ")+field);}
      shaders.insert(shader);checked.insert(source);++preserved;
     }
    }
   }
  }
  for(auto shader:shaders){bool found=false;for(size_t i=0;i<plan.originals.size()&&!found;++i){auto bundle=EndfieldInput::Decode(ReadSource({},plan.originals[i]));for(const auto& member:bundle.members)if(member.name==shader.first){auto layout=ParseSerialized(member.bytes);found=layout.objects.count(shader.second)&&layout.types.at(layout.objects.at(shader.second).type).classId==48;}}
   Require(found,"Shader object absent from selected source closure: "+shader.first);std::cout<<"Shader="<<shader.first<<":"<<shader.second<<'\n';
  }
  Require(preserved==artifact->manifest.package.materials.size(),"not all authored materials verified");std::cout<<"PASS preserved Shader/keywords/pass fields: materials="<<preserved<<" shaders="<<shaders.size()<<" exactRootAssemblies="<<assembly<<'\n';return 0;
 }
 FixedWidthSkipRegression();
 AuthorSourceRegression();
 GenericResourceRegression();
 NativeRegistrationRegression();
 GenericObjectRegression();
 PreloadIntervalRegression();
 NativeResidencyRegression();
 SharedInputRegression();
 ExternalDependencyRegression();
 auto binding=OriginalBoneBinding({11,22,33},{101,202,303},{33,44,11});
 Require(binding==std::map<size_t,int64_t>{{0,303},{2,101}},"native palette correspondence lost reordered/original bones");
 Require(OriginalBoneBinding({11,22,33,44,55},{101,404},{22,44,11}).empty(),
  "incomplete native palette invented a positional binding");
 Require(OriginalBoneBinding({11},{101,202},{11}).empty(),"longer native palette invented a positional binding");
 Require(OriginalBoneBinding({11},{},{11}).empty(),"empty native palette invented a positional binding");
 Rejected([&]{OriginalBoneBinding({11,11},{101,202},{11});});
 auto equal=[](int a,int b){return a==b;};
 auto sequence=MergeSequence(std::vector<int>{1,2,3},std::vector<std::vector<int>>{{1,4,2,3},{1,2,5,3}},equal);
 Require(sequence.values==std::vector<int>({1,4,2,5,3})&&sequence.offsets==std::vector<size_t>({0,2,4,5}),"independent list expansions lost a Mod");
 auto empty=MergeSequence(std::vector<int>{},std::vector<std::vector<int>>{{4},{5}},equal);Require(empty.values==std::vector<int>({4,5}),"empty parent list lost extensions");
 auto removed=MergeSequence(std::vector<int>{1,2,3},std::vector<std::vector<int>>{{1,3},{1,2,3,4}},equal);Require(removed.values==std::vector<int>({1,3,4}),"skip and independent extension did not compose");
 Fixture fixture;auto before=fixture.BytesOf();auto layout=ParseSerialized(before);auto id=int64_t(11);const auto& type=layout.types.at(layout.objects.at(id).type);auto original=ObjectValue(before,layout,id);auto a=original,b=original;
 auto& ca=ArrayValue(type,Child(type,a,"m_Children"));auto value=ArrayElement(type,ca);SetReference(type,value,{fixture.cab,200},layout,fixture.cab);ca.children.push_back(value);ca.count=ca.children.size();
 auto& cb=ArrayValue(type,Child(type,b,"m_Children"));value=ArrayElement(type,cb);SetReference(type,value,{fixture.cab,300},layout,fixture.cab);cb.children.push_back(value);cb.count=cb.children.size();
 auto merged=MergeObject(type,EncodeValue(type,original),{EncodeValue(type,a),EncodeValue(type,b)});auto tree=DecodeValue(type,0,merged);Require(ArrayValue(type,Child(type,tree,"m_Children")).children.size()==ArrayValue(type,Child(type,original,"m_Children")).children.size()+2,"two Mods on same original Transform lost a child");
 if(argc==4&&(std::string(argv[1])=="--read-set"||std::string(argv[1])=="--namespace-replay-audit")){auto started=std::chrono::steady_clock::now();auto set=EiemCompiledModSet::Read(std::filesystem::u8path(argv[2]),{},std::filesystem::u8path(argv[3]));auto preparedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();Require(set&&(!set->isolated.empty()||!set->views.empty()),"real set was not prepared");auto context=EiemCompiledModSet::Context(set);size_t cachedBytes=0;
 for(size_t i=0;i<set->catalog.size();++i){bool changed=set->manifest.plan.startupHashes[i]!=set->catalog[i].source.hash;Require((set->startup.count(i)+set->startupViews.count(i))==size_t(changed),"startup delivery coverage differs from modified inputs");if(changed){
  if(set->startupViews.count(i)){Require(context->startupViews.at(i)==set->startupViews.at(i)&&EiemInputSpanView::Digest(*context->startupViews.at(i))==set->manifest.plan.startupHashes[i],"startup view binding changed prepared input");}
  else {Require(context->startupInputs.at(i)==set->isolated.at(i)&&Hash(context->Materialize(i,false))==set->manifest.plan.startupHashes[i],"startup alias copied or changed prepared input");cachedBytes+=set->startup.at(i)->size();}}}
 std::cout<<"startupCacheMiB="<<double(cachedBytes)/1048576<<" changedInputs="<<set->startup.size()<<'\n';size_t added=0;for(const auto& row:*context->addedDependencies)added+=row.second.size();size_t shared=0;for(const auto& contributors:set->contributors)if(contributors.size()>1)++shared;std::cout<<"real compiled set: mods="<<set->mods.size()<<" inputs="<<set->catalog.size()<<" roots="<<set->manifest.plan.roots.size()<<" sharedInputs="<<shared<<" addedDependencySources="<<context->addedDependencies->size()<<" addedEdges="<<added<<" preparedMs="<<preparedMs<<'\n';
 if(std::string(argv[1])=="--namespace-replay-audit"){size_t verified=0;for(auto i:set->manifest.plan.closure){
  if(set->views.count(i)){auto writers=EiemCompiledModSet::Writers(*set,i);Require(writers.size()<=1&&!set->contributors[i].empty(),"bound view has no unique source proof");auto contributor=writers.empty()?set->contributors[i].front():writers.front();
   const auto& a=*set->mods.at(contributor.first);auto input=contributor.second;HydrateInput(a,input);auto sourceBytes=ReadSource({},a.manifest.plan.originals.at(input));auto source=EndfieldInput::Decode(sourceBytes);std::map<std::string,const Bytes*> originals;
   for(const auto& member:source.members)originals.emplace(member.name,&member.bytes);auto edited=source;edited.members.clear();for(const auto& recipe:a.manifest.plan.memberRecipes.at(input))edited.members.push_back({recipe.name,recipe.flags,EiemResourceRecipe::Apply(recipe,originals,a.manifest.package,a.preparedFrames)});
   EiemCompiledInputView::Buffers buffers;buffers.emplace(std::make_pair(EiemResourceRecipe::Original,std::string{}),set->sourceBuffers.at(i));for(const auto& resource:a.manifest.package.resources)buffers.emplace(std::make_pair(EiemResourceRecipe::Author,resource.first),std::shared_ptr<const Bytes>(resource.second,&resource.second->bytes));
   for(const auto& frame:a.preparedFrames)buffers.emplace(std::make_pair(EiemResourceRecipe::ExpandedFrame,frame.first),std::shared_ptr<const Bytes>(set->mods.at(contributor.first),&frame.second));VerifyCompiledView(a.views.at(input),buffers,a,input,edited,set->manifest.plan.names);
  }else Require(EiemCompiledModSet::Materialize(*set,i,true)==*set->isolated.at(i),"real prepared namespace output differs from full replay");++verified;}
  std::cout<<"namespaceReplayExactInputs="<<verified<<" viewObjectsAndStreamPayloadsChecked=1\n";}}
 if(argc==3){auto modDirectory=std::filesystem::u8path(argv[2]);auto roots=EiemComposedPackage::Roots(modDirectory);Require(roots.size()==1,"portability test needs one isolated Mod");auto modRoot=roots.front();auto packet=EiemNativeInput::Read(modRoot/"compiled.bin");auto plain=Unpack(packet);EiemOfflinePackage::Reader report{plain};auto encoded=report.Blob();std::cout<<"packetMiB="<<double(packet.size())/1048576<<" innerMiB="<<double(encoded.size())/1048576<<" proofPlainMiB="<<double(plain.size()-encoded.size())/1048576<<'\n';auto started=std::chrono::steady_clock::now();auto set=EiemCompiledModSet::Read(std::filesystem::u8path(argv[1]),{},modDirectory);Require(set&&set->mods.size()==1&&!set->isolated.empty(),"real independent artifact was not prepared");
  for(size_t i=0;i<set->catalog.size();++i){auto bytes=EiemCompiledModSet::Materialize(*set,i,false);Require(Hash(bytes)==set->manifest.plan.startupHashes[i],"compiled startup replay is nondeterministic");}
  auto moved=modDirectory.parent_path()/"compiled-mod-portability";Require(!std::filesystem::exists(moved),"portability fixture exists");std::filesystem::rename(modRoot,moved);struct Restore{std::filesystem::path from,to;~Restore(){std::filesystem::rename(from,to);}}restore{moved,modRoot};
  auto retired=EiemCompiledModSet::Read(std::filesystem::u8path(argv[1]),set,modDirectory);Require(retired&&retired->mods.empty()&&retired->catalog.size()==set->catalog.size(),"empty directory retirement omitted source inputs");for(size_t i=0;i<retired->catalog.size();++i)Require(retired->manifest.plan.startupHashes[i]==retired->catalog[i].source.hash,"empty set failed pristine source restoration");
  auto portable=EiemCompiledMod::Read(moved,std::filesystem::u8path(argv[1]));Require(portable.manifest.package.root==moved,"portable compiled Mod retained original author folder");Require(!EiemCompiledModSet::Unchanged(*set,modDirectory),"removed Mod not detected");
  std::cout<<"real compiled set: inputs="<<set->catalog.size()<<" roots="<<set->manifest.plan.roots.size()<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<'\n';
 }
 std::cout<<"PASS generic resources, native type/script registrations, shared object additions/edits/deletions and compiled Mod membership; existing sequence/dependency composition preserved; no resource compiler process or Mesh builder invoked\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
