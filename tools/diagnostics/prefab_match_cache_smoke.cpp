#include "../../src/eiem_prefab_match_cache.h"
#include <chrono>
#include <iostream>
using namespace EiemPrefabMatchCache;
int main(){try{
 Cache cache;SourceIdentity source{"assets/new-story.prefab","prefab-content","mesh-and-skeleton-content"};unsigned scans=0;
 const Identity target{"assets/character/cloth.asset","Cloth"},other{"assets/another/cloth.asset","Cloth"};
 auto discover=[&]{++scans;return std::vector<Consumer>{{"/mesh:0",0,137,target},{"/mesh:1",0,137,target},{"/mesh:2",0,23,other}};};
 Require(cache.Get(source,{},"empty-rules",discover).result.matches.empty()&&scans==1,"first unknown Prefab was not discovered/cached negative");
 Rules rules{{target,"mod/cloth"}};
 auto positive=cache.Get(source,rules,"added-mod",discover);
 Require(scans==1&&positive.result.matches.size()==2,"new Mod did not revisit a negative observation or rescanned its hierarchy");
 auto start=std::chrono::steady_clock::now();for(size_t i=0;i<100000;++i)Require(cache.Get(source,rules,"added-mod",discover).result.matches.size()==2,"cached positive result changed");
 Require(scans==1,"hot event cache repeatedly scanned the Prefab");
 auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 auto changed=source;changed.dependencies="updated-mesh-only";cache.Get(changed,rules,"added-mod",discover);Require(scans==2,"dependency-only update reused a stale match");
 changed.content="updated-prefab";cache.Get(changed,rules,"added-mod",discover);Require(scans==3,"Prefab update reused old Renderer addresses");
 Require(cache.Get(changed,{},"removed-mod",discover).result.matches.empty()&&scans==3,"Mod removal left positive matches or rescanned source");
 Cache restarted;auto bytes=Encode(positive.observation);restarted.Restore(Decode(bytes));
 Require(restarted.Get(source,rules,"added-mod",discover).result.matches.size()==2&&scans==3,"persisted observation did not survive a new process cache");
 auto added=source;added.path="assets/never-before-loaded.prefab";cache.Get(added,rules,"added-mod",discover);Require(scans==4&&cache.Size()==2,"newly seen Prefab was ignored because another Prefab was cached");
 bytes.back()^=1;bool rejected=false;try{Decode(bytes);}catch(const std::exception&){rejected=true;}Require(rejected,"corrupt persistent Prefab cache accepted");
 std::cout<<"PASS event cache: unknown Prefab, all Mesh consumers, same-name isolation, negative-to-positive after added Mod, source/dependency invalidation, persistence and corruption rejection; 100000 hot lookups ms="<<ms<<" hierarchyScans=0\n";return 0;
 }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
