#include "../../src/eiem_offline_resource_package.h"
#include <iostream>
using namespace EiemOfflinePackage;
Manifest Fixture(){
 Manifest m;m.assembly=std::string(64,'A');m.baseline=std::string(64,'B');auto& p=m.package;p.id="LZY";p.root="mods/LZY";
 auto mesh=std::make_shared<Resource>();mesh->kind=1;mesh->bytes={1,2,3,4};p.resources.emplace("meshes/body.mesh",mesh);
 auto material=std::make_shared<Resource>();material->kind=2;material->bytes={5,6,7,8};p.resources.emplace("materials/body.mat",material);
 p.meshes.emplace("body",Mesh{"meshes/body.mesh",2,3,{"Root/Bip/Bone"},{"Shape"}});
 p.materials.emplace("body",EiemDirectory::Material{"materials/body.mat",{"assets/body.mat","body"},{}});
 p.defaults.emplace("$switch",0);p.defaults.emplace("$shape",.25);p.persistent={"$switch","$shape"};p.controls.emplace("$shape",Control{"Shape",0,1});
 std::string error;auto expr=EiemExpressionParser("$switch == 0",error).Parse();Require(bool(expr),"fixture expression");p.parts.emplace("body",EiemDirectory::Part{"body","body",expr,{{"Shape","$shape"}}});
 p.rules.emplace("body",Rule{{"body"},{{0,{"assets/body.asset","body"}}},false});p.targetIndex.emplace(Identity{"assets/body.asset","body"},"body");
 p.bindings.push_back(Binding{"switch","$switch","cycle",{VK_NUMPAD1,0},{0,1},1});
 auto& plan=m.plan;plan.key=std::string(64,'C');plan.author=PackageDigest(p);plan.contract=InputContractDigest(p);plan.inputs.push_back(Input{"input-001.vfs",std::string(64,'D'),"main/example.ab",{}, {}});plan.closure={0};
 Root root;root.path="assets/model.prefab";root.markers={{"marker",1}};root.cab="CAB-"+std::string(32,'a');root.source=1;
 EiemRenderAssemblyDescription::Source source;source.cab=root.cab;source.id=2;source.renderer={"4:Body:0/",0};EiemRenderAssemblyDescription::Target target;target.renderer=source.renderer;target.marker="marker";target.bones={"4:Root:0/3:Bip:0/4:Bone:0/"};source.targets.push_back(target);root.assembly.sources.push_back(source);
 plan.roots.push_back(root);plan.parts.emplace("marker",EiemResourceReloadPlan::Part{"body","marker","body",2,{"Root/Bip/Bone"},{"Shape"}});plan.materials.emplace("body",EiemResourceReloadPlan::Material{"body",{{"_BaseMap",true,false,{}}}});plan.meshParts=1;plan.rendererParts=1;plan.materialMatches=1;plan.rendererBoundsHits=2384;return m;
}
int main(){try{
 auto m=Fixture();auto bytes=Encode(m);auto roundtrip=Decode(bytes,"candidate","mods");Require(Encode(roundtrip)==bytes,"manifest changed during roundtrip");
 Require(roundtrip.package.parts.at("body").visible->Evaluate({{"$switch",0}})==1&&roundtrip.package.parts.at("body").visible->Evaluate({{"$switch",1}})==0,"compiled expression behavior changed");
 Require(roundtrip.package.root==std::filesystem::path("mods/LZY")&&roundtrip.plan.parts.at("marker").bones==m.plan.parts.at("marker").bones&&roundtrip.package.persistent==m.package.persistent&&roundtrip.package.bindings.front().key.vk==VK_NUMPAD1,"compiled author metadata lost");
 auto reject=[](const Bytes& b){bool rejected=false;try{Decode(b,"candidate","mods");}catch(const std::exception&){rejected=true;}Require(rejected,"malformed offline manifest accepted");};
 auto bad=bytes;bad[8]^=1;reject(bad);bad=bytes;bad.pop_back();reject(bad);bad=bytes;std::fill(bad.begin(),bad.begin()+8,255);reject(bad);
 auto invalid=m;invalid.package.defaults.at("$shape")=NAN;bool rejected=false;try{Encode(invalid);}catch(const std::exception&){rejected=true;}Require(rejected,"nonfinite author encoded");
 invalid=Fixture();invalid.package.root="mods/..";reject(Encode(invalid));invalid=Fixture();invalid.package.parts.at("body").visible->right.reset();reject(Encode(invalid));
 invalid=Fixture();invalid.plan.author=std::string(64,'F');reject(Encode(invalid));invalid=Fixture();invalid.plan.roots[0].input=8;reject(Encode(invalid));
 auto cache=std::filesystem::temp_directory_path()/("eff-offline-smoke-"+std::to_string(GetCurrentProcessId()));auto folder=cache/"generations"/m.plan.key;std::filesystem::create_directories(folder);Save(folder/"manifest.bin",bytes);Pointer pointer{m.plan.key,Hash(bytes)};Publish(cache,pointer);
 Require(ReadCurrent(cache)==pointer&&ReadManifest(cache,pointer,"mods").plan.author==m.plan.author,"atomic publication not readable");bad=bytes;bad.back()^=1;Save(folder/"manifest.bin",bad);rejected=false;try{ReadManifest(cache,pointer,"mods");}catch(const std::exception&){rejected=true;}Require(rejected&&ReadCurrent(cache)==pointer,"changed payload bypassed published SHA");
 std::filesystem::remove_all(cache);std::cout<<"PASS offline compiled package: exact roundtrip, AST behavior, per-Mesh bones/shapes, keys/persistence/material slots, bounded extents, compiler ABI/arity/foreign root/stale author/partial manifest/SHA rejection, atomic publication\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
