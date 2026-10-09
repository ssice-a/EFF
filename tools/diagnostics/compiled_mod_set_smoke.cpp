#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_compiled_mod_set.h"
#include "../../src/eiem_compiled_mod_writer.h"
#include <regex>

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
 auto site=EiemCompiledModSet::RelocateSite(patch,source,layout,output,after,{});Require(std::equal(patch.before.begin(),patch.before.end(),output.begin()+site),"Bundle namespace field moved after preload composition");
}

void ExternalDependencyRegression(){
 using namespace EiemCompiledMod;
 Fixture f;f.cab="CAB-"+std::string(32,'a');
 auto& bundleType=f.types[4];auto nameNode=bundleType.nodes.size();bundleType.nodes.push_back({"string","m_Name",false,{}});bundleType.nodes[0].children.push_back(nameNode);auto bundleName=EmptyValue(bundleType,nameNode);SetString(bundleType,bundleName,"fixture-bundle");f.V(300).children.push_back(std::move(bundleName));
 const std::string originalCab="CAB-"+std::string(32,'b'),firstCab="CAB-"+std::string(32,'c'),secondCab="CAB-"+std::string(32,'d'),sharedCab="CAB-"+std::string(32,'e');
 auto layout=ParseSerialized(f.BytesOf());layout.externals={originalCab};auto before=BuildSerialized(f.BytesOf(),layout,{});
 auto originalRefs=EiemLoadingGraphAudit::ReadExternals(before,ParseSerialized(before));before[originalRefs.front().guidOffset]=0xA5;
 auto original=Bundle(before,f.cab);auto root=std::filesystem::temp_directory_path();auto folder=root/("eff-external-smoke-"+std::to_string(GetCurrentProcessId()));
 Require(std::filesystem::create_directory(folder),"external fixture already exists");struct Cleanup{std::filesystem::path path,root;~Cleanup(){if(path.parent_path()==root)std::filesystem::remove_all(path);}}cleanup{folder,root};auto file=folder/"input";EiemResourceReloadPlan::Save(file,original);
 EiemCompiledModSet::Set set;EiemCompiledModSet::Catalog catalog;catalog.source={"fixture",Hash(original),file,0,original.size()};catalog.members[f.cab].before=ParseSerialized(before);set.catalog={catalog};set.contributors={{{0,0},{1,0}}};set.frames.resize(2);
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
 auto& levels=ArrayValue(f.T(14),Child(f.T(14),f.V(14),"m_LODs"));auto& renderers=ArrayValue(f.T(14),Child(f.T(14),levels.children[0],"renderers"));auto extra=ArrayElement(f.T(14),renderers);SetReference(f.T(14),Child(f.T(14),extra,"renderer"),{f.cab,23},f.refs,f.cab);renderers.children.push_back(extra);renderers.count=2;
 f.types[5]=Schema(43,{{"string","m_Name",{}},{"StreamingInfo","m_StreamData",{{"UInt64","offset",{}},{"UInt32","size",{}},{"string","path",{}}}}});f.objects.at(100).second=EmptyValue(f.types[5],0);auto& originalStream=Child(f.T(100),f.V(100),"m_StreamData");SetInteger(f.T(100),Child(f.T(100),originalStream,"size"),4);SetString(f.T(100),Child(f.T(100),originalStream,"path"),f.cab+".resS");
 auto before=f.BytesOf();Bytes stream{1,2,3,4};auto header=Bundle(before,f.cab);auto original=EndfieldInput::Encode(header,{{f.cab,0,before},{f.cab+".resS",0,stream}});
 auto temp=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());auto folder=temp/("eff-compiled-smoke-"+std::to_string(GetCurrentProcessId()));Require(std::filesystem::create_directory(folder),"fixture already exists");struct Cleanup{std::filesystem::path path,root;~Cleanup(){if(path.parent_path()==root)std::filesystem::remove_all(path);}}cleanup{folder,temp};auto file=folder/"input";EiemResourceReloadPlan::Save(file,original);
 EiemCompiledModSet::Set set;EiemCompiledModSet::Catalog catalog;catalog.source={"fixture",Hash(original),file,0,original.size()};catalog.members[f.cab].before=ParseSerialized(before);set.catalog={catalog};set.contributors={{{0,0},{1,0}}};set.frames.resize(2);set.manifest.plan.names.members={{f.cab,"CAB-"+std::string(32,'b')},{f.cab+".resS","CAB-"+std::string(32,'b')+".resS"}};
 for(int mod=0;mod<2;++mod){auto next=f;int64_t renderer=mod?23:13,addedRenderer=mod?523:413,mesh=mod?102:101,child=mod?501:401;next.List(11,"m_Children",{child});next.Add(child,1);next.Pointer(child,"m_GameObject",mod?500:400);next.Add(mod?500:400,0);next.Add(addedRenderer,3);next.Pointer(addedRenderer,"m_GameObject",mod?500:400);next.List(15,"customRenderers",{addedRenderer});auto& lod=ArrayValue(next.T(14),Child(next.T(14),next.V(14),"m_LODs"));auto& list=ArrayValue(next.T(14),Child(next.T(14),lod.children[0],"renderers"));auto value=ArrayElement(next.T(14),list);SetReference(next.T(14),Child(next.T(14),value,"renderer"),{next.cab,addedRenderer},next.refs,next.cab);list.children.insert(list.children.begin()+(mod?2:1),value);list.count=list.children.size();
  next.List(renderer,"m_Materials",{mod?202:201});next.List(300,"m_PreloadTable",{child,addedRenderer,mesh});SetInteger(next.T(300),Child(next.T(300),Child(next.T(300),next.V(300),"m_MainAsset"),"preloadSize"),13);auto& containers=ArrayValue(next.T(300),Child(next.T(300),next.V(300),"m_Container"));for(auto& row:containers.children)SetInteger(next.T(300),Child(next.T(300),Child(next.T(300),row,"second"),"preloadSize"),13);
  next.Add(mesh,5);auto& info=Child(next.T(mesh),next.V(mesh),"m_StreamData");SetInteger(next.T(mesh),Child(next.T(mesh),info,"offset"),4);SetInteger(next.T(mesh),Child(next.T(mesh),info,"size"),mod?5:3);SetString(next.T(mesh),Child(next.T(mesh),info,"path"),next.cab+".resS");auto edited=next.BytesOf();auto bytes=stream;bytes.insert(bytes.end(),mod?5:3,mod?9:8);auto a=std::make_shared<Artifact>();auto& plan=a->manifest.plan;plan.spanRecipes=plan.rulesOnly=true;plan.startupHashes={Hash(EndfieldInput::Encode(original,{{f.cab,0,edited},{f.cab+".resS",0,bytes}}))};plan.memberRecipes.resize(1);
  for(const auto& member:std::vector<EndfieldInput::Member>{{f.cab,0,edited},{f.cab+".resS",0,bytes}}){EiemResourceRecipe::Member recipe;recipe.name=member.name;recipe.hash=Hash(member.bytes);recipe.size=member.bytes.size();recipe.spans={{EiemResourceRecipe::Literal,{},0,member.bytes.size(),member.bytes}};plan.memberRecipes[0].push_back(std::move(recipe));}
  MemberProof proof;proof.before=ParseSerialized(before);proof.after=ParseSerialized(edited);auto object=ObjectValue(edited,proof.after,mesh);auto& schema=proof.after.types.at(proof.after.objects.at(mesh).type);auto prefix=EncodeValue(schema,Child(schema,object,"m_Name"));proof.streams={{mesh,prefix.size(),8,f.cab+".resS",4}};a->members[f.cab]=proof;set.mods.push_back(a);
 }
 auto merged=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,false));auto layout=ParseSerialized(merged.members[0].bytes);auto& bytes=merged.members[0].bytes;
 Require(layout.objects.count(101)&&layout.objects.count(102)&&layout.objects.count(401)&&layout.objects.count(501),"same-PFB union lost added objects");auto object=ObjectValue(bytes,layout,11);const auto& schema=layout.types[layout.objects.at(11).type];Require(ArrayValue(schema,Child(schema,object,"m_Children")).children.size()==2,"same-PFB children expansion lost a Mod");
 for(auto id:{14,15}){auto value=ObjectValue(bytes,layout,id);const auto& type=layout.types[layout.objects.at(id).type];auto count=id==15?ArrayValue(type,Child(type,value,"customRenderers")).children.size():ArrayValue(type,Child(type,ArrayValue(type,Child(type,value,"m_LODs")).children[0],"renderers")).children.size();Require(count==4,"LOD/helper union lost a Mod");}
 auto bundle=ObjectValue(bytes,layout,300);const auto& bundleType=layout.types[layout.objects.at(300).type];Require(ArrayValue(bundleType,Child(bundleType,bundle,"m_PreloadTable")).children.size()==16&&Integer(Child(bundleType,Child(bundleType,bundle,"m_MainAsset"),"preloadSize"))==16,"preload intervals not recalculated");
 auto mesh=ObjectValue(bytes,layout,102);const auto& meshType=layout.types[layout.objects.at(102).type];Require(Integer(Child(meshType,Child(meshType,mesh,"m_StreamData"),"offset"))==20&&merged.members[1].bytes.size()==25,"second Mod stream offset/alignment differs");
 auto retired=set;retired.contributors[0].clear();Require(EiemCompiledModSet::Materialize(retired,0,false)==original,"removing all Mods did not restore exact pristine input");
 set.contributors[0].resize(1);auto one=EndfieldInput::Decode(EiemCompiledModSet::Materialize(set,0,false));Require(!ParseSerialized(one.members[0].bytes).objects.count(102),"removed Mod's compiled objects survived");
 Rejected([&]{MergeSequence(std::vector<int>{1,2},std::vector<std::vector<int>>{{3,2},{4,2}},[](int a,int b){return a==b;});});
}
void NativeResidencyRegression(){
 using namespace EiemCompiledModSet;Set set;auto& plan=set.manifest.plan;plan.key=std::string(64,'F');
 for(size_t i=0;i<4;++i){Catalog catalog;auto cab="CAB-"+std::string(32,char('a'+i));catalog.source.hash=std::string(64,'0');catalog.input.bundle="bundle-"+std::to_string(i);catalog.members.emplace(cab,MemberProof{});set.catalog.push_back(catalog);
  plan.inputs.push_back(catalog.input);plan.names.members.emplace(cab,"CAB-"+std::string(32,'f'));plan.names.members.emplace(cab+".resS","CAB-"+std::string(32,'f')+".resS");plan.names.bundles.emplace(catalog.input.bundle,"isolated-"+std::to_string(i));plan.startupHashes.push_back(catalog.source.hash);}
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
int main(int argc,char** argv){try{
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
  auto game=std::filesystem::u8path(argv[2]),source=std::filesystem::u8path(argv[3]),parent=std::filesystem::temp_directory_path();auto folder=parent/("eff-compiled-refresh-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(folder),"refresh fixture exists");struct Cleanup{std::filesystem::path parent,folder;~Cleanup(){if(folder.parent_path()==parent)std::filesystem::remove_all(folder);}}cleanup{parent,folder};auto mods=folder/"mods",mod=mods/source.filename();std::filesystem::create_directories(mod);std::filesystem::copy(source,mod,std::filesystem::copy_options::recursive);
  auto before=EiemCompiledModSet::Read(game,{},mods);Require(before&&!before->mods.empty()&&!before->startup.empty(),"refresh fixture has no compiled inputs");auto enabled=before->manifest.packages.front().keySwitchEnabled;auto ini=EiemNativeInput::Read(mod/"mod.ini");std::string text(ini.begin(),ini.end());auto changed=std::regex_replace(text,std::regex("key_switch_enabled[ \\t]*=[^\\r\\n]*"),enabled?"key_switch_enabled=0":"key_switch_enabled=1");Require(text!=changed,"refresh fixture lacks key switch setting");EiemOfflineCompile::AtomicSave(mod/"mod.ini",Bytes(changed.begin(),changed.end()));
  auto clock=std::chrono::steady_clock::now();auto after=EiemCompiledModSet::Read(game,before,mods);double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-clock).count();Require(after->pointer==before->pointer&&after->manifest.packages.front().keySwitchEnabled!=enabled&&EiemCompiledModSet::Unchanged(*after,mods),"control-only INI edit changed resource identity or retained stale controls/stamps");
  Require(after->startup==before->startup&&after->isolated==before->isolated,"control-only refresh rebuilt native resource payloads");before->isolated.clear();after->isolated.clear();auto released=EiemCompiledModSet::Read(game,after,mods);Require(released->isolated.empty(),"refresh kept released native inputs alive");
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
 PreloadIntervalRegression();
 NativeResidencyRegression();
 SharedInputRegression();
 ExternalDependencyRegression();
 auto binding=OriginalBoneBinding({11,22,33},{101,202,303},{33,44,11});
 Require(binding==std::map<size_t,int64_t>{{0,303},{2,101}},"native palette correspondence lost reordered/original bones");
 Rejected([&]{OriginalBoneBinding({11,22},{101},{11});});
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
 if(argc==4&&std::string(argv[1])=="--read-set"){auto set=EiemCompiledModSet::Read(std::filesystem::u8path(argv[2]),{},std::filesystem::u8path(argv[3]));Require(set&&!set->isolated.empty(),"real set was not prepared");auto context=EiemCompiledModSet::Context(set);size_t cachedBytes=0;
 for(size_t i=0;i<set->catalog.size();++i){bool changed=set->manifest.plan.startupHashes[i]!=set->catalog[i].source.hash;Require((set->startup.count(i)!=0)==changed,"startup cache coverage differs from modified inputs");if(changed){Require(context->startupInputs.at(i)==set->startup.at(i)&&Hash(EiemCompiledMod::Unpack(*set->startup.at(i)))==set->manifest.plan.startupHashes[i],"startup publication copied or changed prepared input");cachedBytes+=set->startup.at(i)->size();}}
 std::cout<<"startupCacheMiB="<<double(cachedBytes)/1048576<<" changedInputs="<<set->startup.size()<<'\n';size_t added=0;for(const auto& row:*context->addedDependencies)added+=row.second.size();size_t shared=0;for(const auto& contributors:set->contributors)if(contributors.size()>1)++shared;std::cout<<"real compiled set: mods="<<set->mods.size()<<" inputs="<<set->catalog.size()<<" roots="<<set->manifest.plan.roots.size()<<" sharedInputs="<<shared<<" addedDependencySources="<<context->addedDependencies->size()<<" addedEdges="<<added<<'\n';}
 if(argc==3){auto modDirectory=std::filesystem::u8path(argv[2]);auto roots=EiemComposedPackage::Roots(modDirectory);Require(roots.size()==1,"portability test needs one isolated Mod");auto modRoot=roots.front();auto packet=EiemNativeInput::Read(modRoot/"compiled.bin");auto plain=Unpack(packet);EiemOfflinePackage::Reader report{plain};auto encoded=report.Blob();std::cout<<"packetMiB="<<double(packet.size())/1048576<<" innerMiB="<<double(encoded.size())/1048576<<" proofPlainMiB="<<double(plain.size()-encoded.size())/1048576<<'\n';auto started=std::chrono::steady_clock::now();auto set=EiemCompiledModSet::Read(std::filesystem::u8path(argv[1]),{},modDirectory);Require(set&&set->mods.size()==1&&!set->isolated.empty(),"real independent artifact was not prepared");
  for(size_t i=0;i<set->catalog.size();++i){auto bytes=EiemCompiledModSet::Materialize(*set,i,false);Require(Hash(bytes)==set->manifest.plan.startupHashes[i],"compiled startup replay is nondeterministic");}
  auto moved=modDirectory.parent_path()/"compiled-mod-portability";Require(!std::filesystem::exists(moved),"portability fixture exists");std::filesystem::rename(modRoot,moved);struct Restore{std::filesystem::path from,to;~Restore(){std::filesystem::rename(from,to);}}restore{moved,modRoot};
  auto retired=EiemCompiledModSet::Read(std::filesystem::u8path(argv[1]),set,modDirectory);Require(retired&&retired->mods.empty()&&retired->catalog.size()==set->catalog.size(),"empty directory retirement omitted source inputs");for(size_t i=0;i<retired->catalog.size();++i)Require(retired->manifest.plan.startupHashes[i]==retired->catalog[i].source.hash,"empty set failed pristine source restoration");
  auto portable=EiemCompiledMod::Read(moved,std::filesystem::u8path(argv[1]));Require(portable.manifest.package.root==moved,"portable compiled Mod retained original author folder");Require(!EiemCompiledModSet::Unchanged(*set,modDirectory),"removed Mod not detected");
  std::cout<<"real compiled set: inputs="<<set->catalog.size()<<" roots="<<set->manifest.plan.roots.size()<<" ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count()<<'\n';
 }
 std::cout<<"PASS independent compiled rule sequence/object composition; no resource compiler process or Mesh builder invoked\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
