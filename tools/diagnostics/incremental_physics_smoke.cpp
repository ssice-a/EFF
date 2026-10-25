#include "../../src/eiem_resource_compiler.h"
#include "../../src/eiem_compiled_mod_writer.h"
#include <iostream>
using namespace EiemCompiledModWriter;

bool BumpFirstFloat(const Type& type,Value& value){
 if(type.nodes.at(value.node).type=="float"){
  auto current=FloatValue(type,value);SetFloat(type,value,current+0.125);return true;
 }
 for(auto& child:value.children)if(BumpFirstFloat(type,child))return true;
 return false;
}

bool ChangePhysicsTemplate(Bytes& bytes){
 auto layout=ParseSerialized(bytes);
 for(auto id:layout.order){
  const auto& object=layout.objects.at(id);const auto& type=layout.types.at(object.type);if(type.classId!=114)continue;
  auto value=ObjectValue(bytes,layout,id);if(!BumpFirstFloat(type,value))continue;
  auto replacement=EncodeValue(type,value);bytes=BuildSerialized(bytes,layout,{{id,std::move(replacement)}},{});return true;
 }
 return false;
}

bool ChangePhysicsBlock(Bytes& block){
 EiemPhysicsInput::Reader reader{block};Require(reader.S()==EiemPhysicsInput::structure,"Physics resource structure differs");reader.S();
 auto count=reader.Count();for(size_t i=0;i<count;++i){reader.S();reader.S();reader.B();reader.B();for(size_t j=0;j<10;++j)reader.D();}
 count=reader.Count();
 for(size_t i=0;i<count;++i){
  reader.S();auto size=reader.N();Require(size<=block.size()-reader.p,"Physics template truncated");
  const auto start=reader.p;Bytes templateBytes(block.begin()+start,block.begin()+start+size);
  if(ChangePhysicsTemplate(templateBytes)){
   Require(templateBytes.size()==size,"float parameter unexpectedly changed template size");
   std::copy(templateBytes.begin(),templateBytes.end(),block.begin()+start);return true;
  }
  reader.p+=size;
 }
 return false;
}

void StoreLe(Bytes& bytes,size_t offset,uint64_t value,size_t width){
 Require(offset<=bytes.size()&&width<=bytes.size()-offset,"resource digest offset invalid");
 for(size_t i=0;i<width;++i)bytes[offset+i]=unsigned char(value>>(i*8));
}

bool ChangePhysicsNodePosition(Bytes& block){
 EiemPhysicsInput::Reader reader{block};Require(reader.S()==EiemPhysicsInput::structure,"Physics resource structure differs");reader.S();
 auto count=reader.Count();if(!count)return false;reader.S();reader.S();reader.B();reader.B();auto offset=reader.p;auto bits=reader.N();
 double position;memcpy(&position,&bits,sizeof(position));position+=0.25;memcpy(&bits,&position,sizeof(bits));StoreLe(block,offset,bits,8);return true;
}

void SealResource(Resource& resource,const std::string& changedBlock){
 size_t directory=80,p=directory,count=size_t(Le(resource.bytes,16,4));bool found=false;
 for(size_t i=0;i<count;++i){
  auto nameSize=size_t(Le(resource.bytes,p,2));p+=2;auto name=Text(resource.bytes.data()+p,nameSize);p+=nameSize;
  auto offset=size_t(Le(resource.bytes,p,8)),size=size_t(Le(resource.bytes,p+8,8)),digestOffset=p+16;p+=48;
  if(name!=changedBlock)continue;
  Require(offset<=resource.bytes.size()&&size<=resource.bytes.size()-offset,"resource changed block extent invalid");
  auto digest=Sha(resource.bytes.data()+offset,size);std::copy(digest.begin(),digest.end(),resource.bytes.begin()+digestOffset);
  resource.blocks.at(name).digest=digest;found=true;break;
 }
 Require(found,"resource digest directory entry absent");
 auto body=Sha(resource.bytes.data()+80,resource.bytes.size()-80);std::copy(body.begin(),body.end(),resource.bytes.begin()+48);
 resource.contentHash=Hash(resource.bytes);
 EiemDirectory::Decode(resource.bytes,resource.kind);
}

void ChangePhysicsResource(const std::filesystem::path& mod){
 auto package=EiemResourceCompiler::Load(mod);bool changed=false;
 for(const auto& row:package.resources){
  if(!row.second->blocks.count("@physics"))continue;
  auto resource=std::make_shared<Resource>(*row.second);const auto block=resource->blocks.at("@physics");
  Bytes value(resource->bytes.begin()+block.offset,resource->bytes.begin()+block.offset+block.size);
  Require(ChangePhysicsBlock(value),"no floating-point value found in Physics component templates");
  std::copy(value.begin(),value.end(),resource->bytes.begin()+block.offset);SealResource(*resource,"@physics");
  EiemOfflineCompile::AtomicSave(mod/std::filesystem::u8path(row.first),resource->bytes);changed=true;break;
 }
 Require(changed,"author Mod has no Physics resource");
}

void CopyMod(const std::filesystem::path& source,const std::filesystem::path& destination){
 std::filesystem::create_directories(destination.parent_path());std::filesystem::copy(source,destination,std::filesystem::copy_options::recursive);
}

int wmain(int argc,wchar_t** argv){try{
 Require(argc==3,"incremental_physics_smoke <game> <Physics Mod>");
 auto game=std::filesystem::canonical(argv[1]),source=std::filesystem::canonical(argv[2]);
 auto temporary=std::filesystem::temp_directory_path()/(L"eff-physics-delta-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
 Require(std::filesystem::create_directory(temporary),"Physics regression temporary already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{temporary};
 auto fast=temporary/L"incremental"/source.filename(),full=temporary/L"full"/source.filename();
 CopyMod(source,fast);CopyMod(source,full);ChangePhysicsResource(fast);ChangePhysicsResource(full);
 auto oldPackage=EiemResourceCompiler::Load(source),newPackage=EiemResourceCompiler::Load(fast);
 Require(PhysicsParameterChanges(oldPackage,oldPackage).empty(),"unchanged Physics package selected incremental compile");
 auto mixedPackage=newPackage;mixedPackage.resources.emplace("resources/mixed-test.bin",newPackage.resources.begin()->second);
 Require(PhysicsParameterChanges(mixedPackage,oldPackage).empty(),"mixed resource change selected Physics-only compile");
 auto topologyPackage=newPackage;bool topologyChanged=false;
 for(const auto& row:newPackage.resources)if(row.second->blocks.count("@physics")){
  auto resource=std::make_shared<Resource>(*row.second);const auto block=resource->blocks.at("@physics");
  Bytes value(resource->bytes.begin()+block.offset,resource->bytes.begin()+block.offset+block.size);
  Require(ChangePhysicsNodePosition(value),"Physics document has no node for topology regression");
  std::copy(value.begin(),value.end(),resource->bytes.begin()+block.offset);SealResource(*resource,"@physics");
  topologyPackage.resources[row.first]=std::move(resource);topologyChanged=true;break;
 }
 Require(topologyChanged&&PhysicsParameterChanges(topologyPackage,oldPackage).empty(),"Physics topology change selected parameter-only compile");
 size_t changedResources=0;for(const auto& row:newPackage.resources){auto prior=oldPackage.resources.find(row.first);
  if(prior!=oldPackage.resources.end()&&ResourceContentHash(*row.second)!=ResourceContentHash(*prior->second)){++changedResources;
   std::cout<<"CHANGED-RESOURCE "<<row.first<<" kind="<<row.second->kind<<" blocks="<<row.second->blocks.size()<<"\n";}}
 auto physicsChanges=PhysicsParameterChanges(newPackage,oldPackage);
 std::cout<<"PHYSICS-DELTA changedResources="<<changedResources<<" compatibleResources="<<physicsChanges.size()
          <<" oldDocs="<<EiemPhysicsInput::Documents(oldPackage).size()<<" newDocs="<<EiemPhysicsInput::Documents(newPackage).size()<<"\n";
 auto probe=EiemCompiledMod::ReadRuntime(source,game);auto probeSelection=EiemStaticSources::LoadMod(fast,game);
 std::map<std::string,double> probeCosts;auto probeCost=[&](const char* name,double ms){probeCosts[name]+=ms;};
 auto probeUsed=RecompilePhysicsParameters(probe,newPackage,probeSelection,probeCost);
 std::cout<<"INCREMENTAL-PROBE used="<<probeUsed;for(const auto& row:probeCosts)std::cout<<' '<<row.first<<'='<<row.second;std::cout<<"\n";
 Require(probeUsed,"Physics parameter edit did not pass incremental preflight");
 std::map<std::string,double> costs;auto cost=[&](const char* name,double ms){costs[name]+=ms;};
 Export(fast,game,cost,source);
 auto count=costs.find("incremental-physics-inputs");
 if(count==costs.end()||count->second<=0){for(const auto& row:costs)std::cout<<"PRE-COST "<<row.first<<' '<<row.second<<" ms\n";}
 Require(count!=costs.end()&&count->second>0,"Physics parameter edit did not take the incremental path");
 std::filesystem::remove(full/L"compiled.bin");std::map<std::string,double> fullCosts;auto fullCost=[&](const char* name,double ms){fullCosts[name]+=ms;};
 auto fullStart=std::chrono::steady_clock::now();Export(full,game,fullCost);
 auto fullWall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-fullStart).count();
 auto incremental=EiemCompiledMod::Read(fast,game),complete=EiemCompiledMod::Read(full,game);
 Require(incremental.views.size()==complete.views.size(),"incremental Physics input count differs");
 for(size_t i=0;i<complete.views.size();++i){
  Require(incremental.manifest.plan.originals[i].hash==complete.manifest.plan.originals[i].hash,"incremental Physics source identity differs");
  Require(incremental.views[i].startupHash==complete.views[i].startupHash&&incremental.views[i].size==complete.views[i].size,
      "incremental Physics native view differs from full compile at input "+std::to_string(i));
  Require(incremental.manifest.plan.startupHashes[i]==complete.manifest.plan.startupHashes[i],
      "incremental Physics startup content differs from full compile at input "+std::to_string(i));
 }
 auto incrementalPlan=std::make_shared<const Plan>(incremental.manifest.plan);
 auto completePlan=std::make_shared<const Plan>(complete.manifest.plan);
 EiemPrefabRules::Context incrementalRules(incrementalPlan,incremental.manifest.package);
 EiemPrefabRules::Context completeRules(completePlan,complete.manifest.package);
 for(size_t i=0;i<complete.views.size();++i){
  auto incrementalInput=incrementalRules.Materialize(i,false);
  auto completeInput=completeRules.Materialize(i,false);
  Require(Hash(incrementalInput)==incremental.manifest.plan.startupHashes[i],
      "incremental Physics recipe replay differs at input "+std::to_string(i));
  Require(Hash(completeInput)==complete.manifest.plan.startupHashes[i],
      "full Physics recipe replay differs at input "+std::to_string(i));
  Require(incrementalInput==completeInput,
      "incremental Physics replay bytes differ from full compile at input "+std::to_string(i));
  auto incrementalIsolated=incrementalRules.Materialize(i,true);
  auto completeIsolated=completeRules.Materialize(i,true);
  Require(Hash(incrementalIsolated)==incremental.manifest.plan.inputs[i].hash,
      "incremental Physics isolated recipe replay differs at input "+std::to_string(i));
  Require(Hash(completeIsolated)==complete.manifest.plan.inputs[i].hash,
      "full Physics isolated recipe replay differs at input "+std::to_string(i));
 }
 auto incrementalCanonical=EiemCompiledMod::Encode(incremental,game),completeCanonical=EiemCompiledMod::Encode(complete,game);
 if(incrementalCanonical!=completeCanonical){
  const auto& a=incremental.manifest.plan;const auto& b=complete.manifest.plan;
  std::cout<<"ARTIFACT-DIFF incrementalBytes="<<incrementalCanonical.size()<<" fullBytes="<<completeCanonical.size()
           <<" author="<<(PackageDigest(incremental.manifest.package)==PackageDigest(complete.manifest.package))
           <<" contract="<<(a.contract==b.contract)<<" namespaceKey="<<(a.key==b.key)
           <<" namespaceMaps="<<(a.names.members==b.names.members&&a.names.bundles==b.names.bundles)
           <<" roots="<<a.roots.size()<<'/'<<b.roots.size()<<"\n";
 }
 auto roundtrip=EiemCompiledMod::Decode(incrementalCanonical,fast,game);
 Require(EiemCompiledMod::Encode(roundtrip,game)==incrementalCanonical,"incremental Physics artifact encode/decode is not stable");
 std::cout<<"PASS incremental Physics: changedInputs="<<count->second<<" totalInputs="<<complete.views.size()
          <<" compiledBytes="<<incrementalCanonical.size()<<"\n";
 for(const auto& row:costs)std::cout<<"COST "<<row.first<<' '<<row.second<<" ms\n";
 std::cout<<"FULL-WALL "<<fullWall<<" ms\n";
 for(const auto& row:fullCosts)std::cout<<"FULL-COST "<<row.first<<' '<<row.second<<" ms\n";
 return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
