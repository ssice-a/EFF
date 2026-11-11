#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_author_mesh_bundle.h"
#include "../eff_author_assets.cpp"
#include "../../src/eiem_demand_target_lookup.h"
#include "../../src/eiem_demand_native_assembly.h"
#include <chrono>

void TargetLookupBudgetChecks(){
 Fixture f;const std::string path="assets/large.prefab";const auto& type=f.T(300);
 auto& entries=ArrayValue(type,Child(type,f.V(300),"m_Container"));auto entry=entries.children.front();
 SetString(type,Child(type,entry,"first"),path);auto& info=Child(type,entry,"second");
 SetInteger(type,Child(type,info,"preloadIndex"),0);SetInteger(type,Child(type,info,"preloadSize"),257);entries.children={entry};entries.count=1;
 f.SetList(300,"m_PreloadTable",{100});auto& preloads=ArrayValue(type,Child(type,f.V(300),"m_PreloadTable"));
 for(int64_t id=1000;id<1256;++id){f.objects.emplace(id,f.objects.at(10));auto pointer=ArrayElement(type,preloads);
  SetReference(type,pointer,{f.cab,id},f.refs,f.cab);preloads.children.push_back(std::move(pointer));}preloads.count=preloads.children.size();
 auto archive=EndfieldInput::Encode(EiemAuthorMeshBundle::EnvelopeHeader(),{{f.cab,4,f.BytesOf()}});
 EiemNativeRandomAccess::Reader reader(archive.size(),[&](size_t at,unsigned char* out,size_t size){memcpy(out,archive.data()+at,size);});
 auto begin=std::chrono::steady_clock::now();auto hits=EiemDemandTargetLookup::Find(reader,{path,"Static"});
 auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
 std::cout<<"target discovery preload=257 readBytes="<<reader.stats.encodedBytes<<" reads="<<reader.stats.reads<<" elapsedMs="<<ms<<'\n';
 Require(hits.size()==1&&hits.front().pathId==100,"large preload discovery lost the exact Mesh");
 Require(reader.stats.encodedBytes<archive.size()*8,"request-local target discovery repeatedly reread the member type metadata");
}

void RetargetedNativeMarkerChecks(){
 namespace A=EiemDemandAuthorResources;namespace D=EiemRuntimeAuthorDirectory;namespace Native=EiemDemandNativeAssembly;
 Fixture fixture;auto& bt=fixture.T(300);auto& hashes=ArrayValue(bt,Child(bt,fixture.V(300),"m_HashContainer"));auto hash=ArrayElement(bt,hashes);
 SetInteger(bt,Child(bt,hash,"first"),42);SetReference(bt,Child(bt,Child(bt,hash,"second"),"asset"),{fixture.cab,100},fixture.refs,fixture.cab);hashes.children={hash};hashes.count=1;
 auto source=fixture.BytesOf();auto native=EiemAuthorMeshBundle::Isolate(source,fixture.cab,100,"assets/static.mesh",fixture.V(100),std::string(64,'d'));
 auto root=std::filesystem::temp_directory_path()/("eff-native-retarget-"+std::to_string(GetCurrentProcessId()));Require(!std::filesystem::exists(root),"native retarget fixture already exists");
 struct Cleanup {std::filesystem::path root;~Cleanup(){std::error_code error;std::filesystem::remove_all(root,error);}}cleanup{root};
 auto save=[&](const std::string& path,const Bytes& value){std::filesystem::create_directories((root/path).parent_path());EiemResourceReloadPlan::Save(root/path,value);};
 D::MeshAsset metadata;metadata.part="PartWeapon";metadata.file="meshes/weapon.effres";metadata.path="assets/static.mesh";metadata.marker="Static";metadata.fields={"m_Name"};metadata.vertices=3;metadata.indices=3;metadata.nativeBytes=native.bytes.size();metadata.authorBytes=12;
 auto checksum=Sha(native.bytes.data(),native.bytes.size());metadata.hash=Hex(checksum.data(),checksum.size());Bytes editable{'E','F','F','M','E','S','H',0,6,0,0,0};checksum=Sha(editable.data(),editable.size());metadata.authorHash=Hex(checksum.data(),checksum.size());
 auto payload=native.bytes;payload.insert(payload.end(),editable.begin(),editable.end());save(metadata.file,EiemAuthorResourceFile::Encode(1,EiemAuthorResourceMetadata::EncodeMesh(metadata),payload));
 std::string ini="[Mod]\nformat=3\nid=AuthorWeapon\ncontrols=ini\n\n[MeshWeapon]\npath=meshes/weapon.effres\n\n[PartWeapon]\nmesh=MeshWeapon\nvisible=1\n\n[ReplaceWeapon]\ntarget.path=assets/other.mesh\ntarget.asset=Other\nparts=PartWeapon\n";save("mod.ini",Bytes(ini.begin(),ini.end()));
 auto author=std::make_shared<Native::Author>(std::make_shared<A::Catalog>(root));author->InstallInstance();auto composed=Native::Author::Compose({author});
 auto archive=EndfieldInput::Encode(EiemAuthorMeshBundle::EnvelopeHeader(),{{fixture.cab,4,source}});EiemNativeRandomAccess::Reader reader(archive.size(),[&](size_t at,unsigned char* out,size_t size){memcpy(out,archive.data()+at,size);});
 const auto& member=reader.directory.members.front();auto layout=reader.Layout(member);Identity target{"assets/other.mesh","Other"};AssetIndex assets;assets[target]={};assets[target].cab=fixture.cab;assets[target].pathId=100;assets[target].classId=43;
 auto carrier=Native::PrepareCarrier(reader,0,layout,*composed,assets);auto view=EiemNativeSpliceView::BuildRanges(reader,{carrier.plan.rewrite}).Bind([&](size_t at,unsigned char* out,size_t size){memcpy(out,archive.data()+at,size);});
 Bytes encoded(view->Size());Require(view->ReadAt(0,encoded.data(),encoded.size())==encoded.size(),"native retarget view truncated");auto output=EndfieldInput::Decode(encoded);auto checked=ParseSerialized(output.members.front().bytes);auto mesh=ObjectValue(output.members.front().bytes,checked,100);auto& type=checked.types.at(checked.objects.at(100).type);const auto& name=Child(type,mesh,"m_Name");
 auto expected=RuntimePartName(author->package,"PartWeapon",target);Require(Text(name.raw.data(),name.raw.size())==expected&&expected!="Static","retargeted native Mesh retained the author-time marker");
 std::cout<<"PASS native carrier retarget generates the current installed-instance/Part/target marker while reusing the author resource\n";
}

void EmbeddedTemplateChecks(){
 using namespace EiemAuthorMeshBundle;
 Fixture f;const auto path=std::string("assets/embedded.prefab");auto& type=f.T(300);
 auto& entries=ArrayValue(type,Child(type,f.V(300),"m_Container"));auto entry=entries.children.front();
 SetString(type,Child(type,entry,"first"),path);auto& info=Child(type,entry,"second");
 SetReference(type,Child(type,info,"asset"),{f.cab,10},f.refs,f.cab);
 SetInteger(type,Child(type,info,"preloadIndex"),0);SetInteger(type,Child(type,info,"preloadSize"),2);
 entries.children={entry};entries.count=1;
 f.SetList(300,"m_PreloadTable",{10,100});
 auto& hashes=ArrayValue(type,Child(type,f.V(300),"m_HashContainer"));auto hash=ArrayElement(type,hashes);
 SetInteger(type,Child(type,hash,"first"),42);auto& hashInfo=Child(type,hash,"second");
 SetInteger(type,Child(type,hashInfo,"preloadSize"),2);SetReference(type,Child(type,hashInfo,"asset"),{f.cab,10},f.refs,f.cab);hashes.children={hash};hashes.count=1;
 // Embedded source Prefabs may have unrelated external references. The
 // isolated Mesh must not retain those source objects/dependencies.
 f.refs.externals.push_back("CAB-unrelated");
 const auto bytes=f.BytesOf();auto digest=Sha(bytes.data(),bytes.size());auto checksum=Hex(digest.data(),digest.size());
 const auto file=std::filesystem::absolute("embedded-template-fixture");
 AuthorAssets::Templates templates;templates[{path,"Static"}]={"Mesh",f.cab,checksum,100,file};
 AuthorAssets::Sources sources{templates};sources.files.emplace(file,AuthorAssets::Source{bytes,ParseSerialized(bytes),checksum});
 sources.Get({path,"Static"},"Mesh");
 auto archive=EndfieldInput::Encode(EnvelopeHeader(),{{f.cab,4,bytes}});
 EiemNativeRandomAccess::Reader requested(archive.size(),[&](size_t at,unsigned char* out,size_t size){memcpy(out,archive.data()+at,size);});
 auto hits=EiemDemandTargetLookup::Find(requested,{path,"Static"});
 Require(hits.size()==1&&hits.front().pathId==100&&EiemDemandTargetLookup::Find(requested,{"assets/other.prefab","Static"}).empty()&&EiemDemandTargetLookup::Find(requested,{path,"WrongName"}).empty(),"request-local Mesh discovery did not enforce exact container/name/class");
 Fixture remote;remote.cab="CAB-remote";auto remoteBytes=remote.BytesOf();
 auto external=f;external.refs.externals.push_back(remote.cab);external.SetList(300,"m_PreloadTable",{10});
 auto& preload=ArrayValue(type,Child(type,external.V(300),"m_PreloadTable"));auto externalRef=ArrayElement(type,preload);
 SetReference(type,externalRef,{remote.cab,100},external.refs,external.cab);preload.children.push_back(externalRef);preload.count=2;
 auto externalBytes=external.BytesOf();auto externalArchive=EndfieldInput::Encode(EnvelopeHeader(),{{external.cab,4,externalBytes}});
 auto remoteArchive=EndfieldInput::Encode(EnvelopeHeader(),{{remote.cab,4,remoteBytes}});
 EiemNativeRandomAccess::Reader origin(externalArchive.size(),[&](size_t at,unsigned char* out,size_t size){memcpy(out,externalArchive.data()+at,size);});
 EiemNativeRandomAccess::Reader dependency(remoteArchive.size(),[&](size_t at,unsigned char* out,size_t size){memcpy(out,remoteArchive.data()+at,size);});
 auto cross=EiemDemandTargetLookup::Find(origin,{path,"Static"},[&](const NativeReference& reference,const Identity& identity,EiemNativeInput::Asset& asset){return EiemDemandTargetLookup::ReferencedMesh(dependency,reference,identity,asset);});
 Require(EiemDemandTargetLookup::Find(origin,{path,"Static"}).empty()&&cross.size()==1&&cross.front().cab==remote.cab&&cross.front().pathId==100,"request-local discovery lost exact cross-Bundle preload ownership");
 for(const auto& identity:std::vector<Identity>{{"assets/other.prefab","Static"},{path,"WrongName"}}){
  templates[identity]=templates.at({path,"Static"});Rejected([&]{sources.Get(identity,"Mesh");});}
 templates[{path,"WrongClass"}]={"Mesh",f.cab,checksum,200,file};Rejected([&]{sources.Get({path,"WrongClass"},"Mesh");});
 auto mesh=f.V(100);auto asset=Isolate(bytes,f.cab,100,path,mesh,std::string(64,'b'));
 auto decoded=EndfieldInput::Decode(asset.bytes);auto layout=ParseSerialized(decoded.members.front().bytes);
 auto names=SerializedAssets(decoded.members.front().bytes,decoded.members.front().name,layout);
 Require(layout.objects.size()==2&&layout.externals.empty()&&names.size()==1&&names.front().asset=="Static"&&names.front().path==path,
   "embedded Mesh isolation retained Prefab objects or lost its exact identity");
 f.SetList(300,"m_PreloadTable",{10,200});Rejected([&]{Isolate(f.BytesOf(),f.cab,100,path,mesh,std::string(64,'c'));});
 std::cout<<"PASS embedded Mesh: exact Prefab preload ownership, name/class/PathID rejection and isolated native directory\n";
}

int main(int argc,char** argv){try{
 using namespace EiemAuthorMeshBundle;
 if(argc==9){
  auto package=EiemDirectory::Load(std::filesystem::u8path(argv[1]));auto source=Read(std::filesystem::u8path(argv[2]));
  Identity target{argv[4],argv[5]};auto asset=Build(package,target,argv[6],source,argv[3],std::stoll(argv[7]));
  EiemResourceReloadPlan::Save(std::filesystem::u8path(argv[8]),asset.bytes);
  auto digest=Sha(asset.bytes.data(),asset.bytes.size());
  auto probe=std::string("EFF_PREFAB_ADAPTATION_PROBE/mesh+source+layout/tsv\n")+
   std::filesystem::u8path(argv[8]).filename().u8string()+"\t"+target.path+"\t"+target.asset+"\t"+asset.meshName+"\t"+
   std::to_string(asset.vertices)+"\t"+std::to_string(asset.bones)+"\t"+std::to_string(asset.shapes)+"\t"+Hex(digest.data(),digest.size())+"\n";
  EiemResourceReloadPlan::Save(std::filesystem::u8path(argv[8]).parent_path()/"probe.tsv",Bytes(probe.begin(),probe.end()));
  std::cout<<"PASS independent native Mesh bundle bytes="<<asset.bytes.size()<<" vertices="<<asset.vertices<<" indices="<<asset.indices
   <<" bones="<<asset.bones<<" shapes="<<asset.shapes<<" onlyMeshAndDirectory=1 consumerPrefabs=0 gameAcceptance=unverified\n";return 0;
 }
 Require(argc==1,"expected Mod template CAB path asset Part PathID output");TargetLookupBudgetChecks();EmbeddedTemplateChecks();RetargetedNativeMarkerChecks();
 Fixture f;auto& bt=f.types[4];
 auto& hashes=ArrayValue(bt,Child(bt,f.V(300),"m_HashContainer"));auto entry=ArrayElement(bt,hashes);
 SetInteger(bt,Child(bt,entry,"first"),42);SetReference(bt,Child(bt,Child(bt,entry,"second"),"asset"),{f.cab,100},f.refs,f.cab);
 hashes.children={entry};hashes.count=1;
 auto source=f.BytesOf();auto before=source;auto mesh=f.V(100);SetString(f.types[5],Child(f.types[5],mesh,"m_Name"),"authorMesh");
 auto bundle=Isolate(source,f.cab,100,"assets/static.mesh",mesh,std::string(64,'a'));
 auto decoded=EndfieldInput::Decode(bundle.bytes);auto& bytes=decoded.members.front().bytes;auto layout=ParseSerialized(bytes);
 Require(source==before&&layout.objects.size()==2&&layout.objects.count(100)&&layout.objects.count(300)&&layout.externals.empty(),"standalone writer changed source/retained original objects");
 auto assets=SerializedAssets(bytes,decoded.members.front().name,layout);
 Require(assets.size()==1&&assets.front().path=="assets/static.mesh"&&assets.front().asset=="authorMesh"&&assets.front().classId==43,"standalone asset directory lost native Mesh identity");
 auto tree=ObjectValue(bytes,layout,300);const auto& type=layout.types.at(layout.objects.at(300).type);
 Require(ConstArray(type,Child(type,tree,"m_PreloadTable")).count==1&&ConstArray(type,Child(type,tree,"m_HashContainer")).count==1,"standalone Mesh preload/hash directory differs");
 Require(Isolate(source,f.cab,100,"assets/static.mesh",mesh,std::string(64,'a')).bytes==bundle.bytes,"standalone asset encoding is not deterministic");
 Rejected([&]{Isolate(source,f.cab,100,"assets/absent.mesh",mesh,std::string(64,'a'));});
 Rejected([&]{Isolate(source,f.cab,200,"assets/static.mesh",mesh,std::string(64,'a'));});
 Rejected([&]{Isolate(source,f.cab,100,"assets/static.mesh",mesh,"invalid/token");});
 std::cout<<"PASS standalone author Mesh native metadata, directory, isolation, deterministic encoding and exact source checks\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
