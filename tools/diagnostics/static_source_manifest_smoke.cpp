#define NOMINMAX
#include "../../src/eiem_static_resource_sources.h"
#include <iostream>
using namespace EiemStaticSources;
using EiemResourceReloadPlan::Save;
template<class F> void Rejected(F&& function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}Require(rejected,message);}
int main(){
 auto folder=std::filesystem::temp_directory_path()/("eff-static-contract-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 try{
  Require(!std::filesystem::exists(folder),"temporary static source fixture already exists");std::filesystem::create_directories(folder/"game/data");
  Bytes original{1,2,3,4};Save(folder/"game/data/a.chk",original);Save(folder/"game/data/b.chk",original);
  Selection first;first.manifest="index-content-A";first.assets={{"assets/a.mesh","a.ab"}};first.inputs={{"a.ab",{"a.ab",Hash(original),"data/a.chk",0,4}}};first.dependencies={{"a.ab",{}}};first.candidates={"a.ab"};
  auto bytes=Encode(first);Require(Encode(Decode(bytes))==bytes,"static descriptor roundtrip differs");bytes.back()^=1;Rejected([&]{Decode(bytes);},"damaged static descriptor accepted");
  auto outside=first;outside.inputs.at("a.ab").physical="../escape.chk";Rejected([&]{Encode(outside);},"static relative path traversal accepted");
  Package a;a.id="A";a.root=folder/"mods/A";a.targetIndex.emplace(Identity{"assets/a.mesh","meshA"},"ReplaceA");std::filesystem::create_directories(a.root);Save(a.root/"source-inputs.bin",Encode(first));
  auto loaded=LoadMod(a.root,folder/"game");Require(loaded.inputs.at("a.ab").physical==std::filesystem::weakly_canonical(folder/"game/data/a.chk")&&Payload(loaded.inputs.at("a.ab"))==original,"portable source did not resolve against this game's directory");
  auto changed=first;changed.inputs.at("a.ab").physical=std::filesystem::absolute(folder/"game/data/a.chk");Save(a.root/"source-inputs.bin",Encode(changed));Rejected([&]{LoadMod(a.root,folder/"game");},"machine-specific absolute source accepted");Save(a.root/"source-inputs.bin",Encode(first));
  auto second=first;second.manifest="different-index-content";second.assets={{"assets/b.mesh","b.ab"}};second.inputs={{"b.ab",{"b.ab",Hash(original),"data/b.chk",0,4}}};second.dependencies={{"b.ab",{}}};second.candidates={"b.ab"};
  Package b;b.id="B";b.root=folder/"mods/B";b.targetIndex.emplace(Identity{"assets/b.mesh","meshB"},"ReplaceB");std::filesystem::create_directories(b.root);Save(b.root/"source-inputs.bin",Encode(second));
  auto merged=Merge({a,b},folder/"game");Require(merged.assets.size()==2&&merged.inputs.size()==2&&merged.candidates.size()==2,"static multi-character sources omitted a member");
  Require(Merge({a},folder/"game").inputs.size()==1&&Merge({},folder/"game").inputs.empty(),"deleted Mod remained in current static selection");
  b.targetIndex.emplace(Identity{"assets/unprepared.mesh","meshC"},"ReplaceC");Rejected([&]{Merge({a,b},folder/"game");},"new undeclared target silently used an old descriptor");b.targetIndex.erase({"assets/unprepared.mesh","meshC"});
  second.inputs.emplace("a.ab",Entry{"a.ab",std::string(64,'F'),"data/a.chk",0,4});Save(b.root/"source-inputs.bin",Encode(second));Rejected([&]{Merge({a,b},folder/"game");},"inconsistent shared original input accepted");
  Save(folder/"game/data/a.chk",Bytes{9,8,7,6});Rejected([&]{Payload(loaded.inputs.at("a.ab"));},"changed game source was used despite declared content digest");
  std::filesystem::remove_all(folder);std::cout<<"PASS static source descriptors: relative routing, structure/digest validation, exact declaration coverage, multi-character union/add/remove/empty, shared-input conflict and changed source rejection; no game APIs"<<std::endl;return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<std::endl;if(std::filesystem::exists(folder))std::filesystem::remove_all(folder);return 1;}
}
