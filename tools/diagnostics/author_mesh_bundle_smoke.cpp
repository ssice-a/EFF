#define main StaticFixtureMain
#include "static_mesh_assembly_smoke.cpp"
#undef main
#include "../../src/eiem_author_mesh_bundle.h"

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
 Require(argc==1,"expected Mod template CAB path asset Part PathID output");
 Fixture f;auto& bt=f.types[4];auto addString=[&](const char* field){auto n=bt.nodes.size();bt.nodes.push_back({"string",field,false,{}});bt.nodes[0].children.push_back(n);f.V(300).children.push_back(EmptyValue(bt,n));};
 addString("m_Name");addString("m_AssetBundleName");
 auto depsNode=bt.nodes.size();bt.nodes.push_back({"vector","m_Dependencies",false,{depsNode+1}});
 bt.nodes.push_back({"Array","Array",false,{depsNode+2,depsNode+3}});bt.nodes.push_back({"int","size",false,{}});bt.nodes.push_back({"string","data",false,{}});
 bt.nodes[0].children.push_back(depsNode);f.V(300).children.push_back(EmptyValue(bt,depsNode));
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
