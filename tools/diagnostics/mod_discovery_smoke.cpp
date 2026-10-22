#include "../../src/eiem_compiled_mod_set.h"
#include "../../src/eiem_resource_compiler.h"
#include <iostream>

using namespace EiemDirectory;
namespace fs=std::filesystem;
void Marker(const fs::path& path,const std::string& id,bool effect=false){
  fs::create_directories(path);std::ofstream out(path/"mod.ini");
  out<<"[Mod]\nformat=2\nid="<<id<<'\n';if(effect)out<<"type=effect\n";
}
void Discovery(const fs::path& mods){
  auto parent=mods/"lx",child=parent/fs::u8path(u8"花瓣"),grandchild=child/"group/child";
  Marker(parent,"Character");Marker(child,"Petals",true);Marker(grandchild,"Child");
  Marker(mods/"group/other","Other");Marker(mods/".backup/hidden","Hidden");
  Marker(parent/".staging/hidden","Hidden2",true);
  auto all=EiemComposedPackage::Roots(mods,true),characters=EiemComposedPackage::Roots(mods);
  Require(all.size()==4&&characters.size()==3,"recursive discovery lost parent/child/grouping or loaded dot folders");
  Require(std::find(all.begin(),all.end(),fs::canonical(child))!=all.end(),"nested Unicode effect was not found");
  Require(all==EiemComposedPackage::Roots(mods,true)&&std::is_sorted(all.begin(),all.end()),"discovery order unstable");
  auto own=EiemModDirectory::Files(parent);Require(own==std::vector<fs::path>{parent/"mod.ini"},"child Mod files leaked into parent");
  auto stamp=EiemCompiledModSet::AuthorStamps(parent);auto directoryStamp=EiemCompiledModSet::DirectoryStamp(mods);
  {std::ofstream file(child/"resources.bin");file<<"child edit";}
  Require(EiemCompiledModSet::AuthorStamps(parent).size()==stamp.size(),"child edit invalidated parent author stamps");
  Require(EiemCompiledModSet::DirectoryStamp(mods)==directoryStamp,"effect-only edit invalidated character directory stamp");
  Require(!EiemModDirectory::Owns(parent,child/"mod.ini")&&EiemModDirectory::Owns(parent,parent/"mod.ini"),"nested owner scope differs");
  EiemResourceCompiler::Options options;options.mod=parent;auto sourceStamp=EiemResourceCompiler::SourceStamp(options);
  {std::ofstream file(child/"resources.bin");file<<"changed again";}
  Require(EiemResourceCompiler::SourceStamp(options)==sourceStamp,"nested effect changed parent compiler stamp");
  Require(EiemComposedPackage::Roots(mods/"missing").empty(),"missing mods directory rejected");
  std::error_code linkError;fs::create_directory_symlink(parent,mods/"alias",linkError);
  if(!linkError)Require(EiemComposedPackage::Roots(mods,true)==all,"directory alias duplicated packages");
  Package left,right;left.id="Left";right.id="Right";left.root=mods/"left/common";right.root=mods/"right/common";
  Require(EiemComposedPackage::Compose({left,right},true).composed,"equal folder basenames rejected independent IDs");
  std::cout<<"PASS recursive discovery: Unicode, parent+child, grouping, hidden staging, sorted roots, resource and stamp boundaries\n";
}
void Real(const fs::path& mods,const fs::path& character,const fs::path& effect,const fs::path& game){
  auto parent=mods/"lx",child=parent/fs::u8path(u8"花瓣");fs::create_directories(mods);
  fs::copy(character,parent,fs::copy_options::recursive);
  auto original=Load(parent);auto digest=EiemNativeInput::PackageDigest(original);
  fs::copy(effect,child,fs::copy_options::recursive);
  auto loaded=Load(parent);Require(EiemNativeInput::PackageDigest(loaded)==digest,"child effect changed parent package digest");
  Require(Load(child).parts.empty(),"independent effect no longer reads");
  auto graph=EiemCompiledModSet::Read(game,{},mods);Require(graph&&graph->mods.size()==1,"nested effect entered compiled character set");
  Require(EiemCompiledModSet::Unchanged(*graph,mods),"nested parent immediately appears modified");
  auto childIni=Read(child/"mod.ini");{std::ofstream edit(child/"mod.ini",std::ios::app);edit<<"\n; independent edit\n";}
  Require(EiemCompiledModSet::Unchanged(*graph,mods),"nested effect edit forced full character rebuild");
  auto priorArtifact=graph->mods.front();auto refreshed=EiemCompiledModSet::Read(game,graph,mods);
  Require(refreshed->mods.front()==priorArtifact,"parent artifact was not reused");
  fs::create_directories(mods/"group");fs::rename(parent,mods/"group/lx");
  auto moved=EiemCompiledModSet::Read(game,{},mods);Require(moved&&moved->mods.size()==1&&moved->manifest.packages.front().root==fs::canonical(mods/"group/lx"),"nested character compiled package was not relocated");
  std::cout<<"PASS real nested packages: strict parent+effect reads, compiled set, unchanged/reuse, nested character relocation\n";
}
int main(int argc,char** argv){try{
  auto base=fs::temp_directory_path()/("EFF-mod-discovery-"+std::to_string(GetCurrentProcessId()));
  Require(!fs::exists(base),"temporary discovery directory exists");fs::create_directories(base);
  struct Cleanup{fs::path path;~Cleanup(){std::error_code error;fs::remove_all(path,error);}}cleanup{base};
  Discovery(base/"synthetic");
  if(argc==4)Real(base/"real",fs::u8path(argv[1]),fs::u8path(argv[2]),fs::u8path(argv[3]));
  std::cout<<"PASS nested Mod discovery\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
