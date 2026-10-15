#include "../eff_author_sources.h"
#include "../../src/eiem_compiled_mod.h"
#include <iomanip>
#include <iostream>

// Read-only: compare the published source selections with today's mounted VFS.
int wmain(int argc,wchar_t** argv){try{
 using namespace EiemAuthorSources;
 Require(argc==3,"audit_game_update_sources GAME MODS");
 auto game=std::filesystem::path(argv[1]);
 std::map<std::filesystem::path,EiemStaticSources::Selection> selections;std::set<std::string> wanted;
 for(const auto& root:EiemComposedPackage::Roots(argv[2])){auto selection=EiemStaticSources::LoadMod(root,game);for(const auto& row:selection.inputs)wanted.insert(row.first);selections.emplace(root,std::move(selection));}
 for(const auto& item:selections){auto artifact=EiemCompiledMod::ReadRuntime(item.first,game);size_t missing=0;
  for(const auto& source:artifact.manifest.plan.originals){
   auto entry=std::find_if(item.second.inputs.begin(),item.second.inputs.end(),[&](const auto& row){return row.second.physical==source.physical&&row.second.offset==source.offset&&row.second.size==source.size&&row.second.revision==source.hash;});
   Require(entry!=item.second.inputs.end(),"published source has no logical selection");
   bool oldExists=std::filesystem::is_regular_file(source.physical);if(!oldExists)++missing;
   std::cout<<"SOURCE\t"<<item.first.filename().u8string()<<'\t'<<entry->first<<'\t'<<oldExists<<'\t'<<source.physical.u8string()<<'\t'<<source.offset<<'\t'<<source.size<<'\t'<<source.hash<<'\n';
  }
  std::cout<<"SUMMARY\t"<<item.first.filename().u8string()<<"\tinputs="<<artifact.manifest.plan.originals.size()<<"\tmissingOld="<<missing<<std::endl;
 }
 return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
