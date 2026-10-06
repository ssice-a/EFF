#include "../../src/eiem_offline_cache.h"
#include <iostream>

void StartupReuseRegression(const std::filesystem::path& folder){
 using namespace EiemResourceReloadPlan;
 auto cached=folder/"stage.vfs",linked=folder/"startup-linked.vfs",fallback=folder/"startup-copy.vfs",changed=folder/"startup-changed.vfs";
 Bytes original{1,2,3,4},replacement{5,6,7,8};Save(cached,original);
 Require(SaveFromCache(linked,cached,original)&&std::filesystem::equivalent(linked,cached),"startup did not reuse the validated cache file");
 EiemOfflineCompile::AtomicSave(cached,replacement);
 Require(Read(linked)==original&&Read(cached)==replacement&&!std::filesystem::equivalent(linked,cached),"cache replacement mutated a published startup input");
 Require(!SaveFromCache(fallback,folder/"missing-stage.vfs",original)&&Read(fallback)==original,"unavailable startup link did not fall back to exact bytes");
 bool refused=false;try{SaveFromCache(changed,cached,original);}catch(const std::exception&){refused=true;}
 Require(refused,"cache changed after validation but was linked into a candidate");
 refused=false;try{SaveFromCache(linked,cached,replacement);}catch(const std::exception&){refused=true;}
 Require(refused&&Read(linked)==original,"startup reuse overwrote an existing published input");
 std::filesystem::remove(cached);Require(Read(linked)==original,"cache deletion invalidated a linked startup input");
 for(const auto& path:{linked,fallback,changed})std::filesystem::remove(path);
}
int wmain() {
 using namespace EiemOfflineCache;
 auto folder=std::filesystem::temp_directory_path()/(L"eff-cache-smoke-"+std::to_wstring(GetCurrentProcessId()));
 try {
  std::filesystem::create_directories(folder/"authors/A");
  std::filesystem::create_directories(folder/"authors/B");
  StartupReuseRegression(folder);
  Package a;a.id="ModA";a.root=folder/"authors/A";
  Package b;b.id="ModB";b.root=folder/"authors/B";
  auto cache=folder/"cache";
  Require(PathIdentity(cache)==PathIdentity(cache/"../cache"),"cache aliases must share one lock");
  Claim(cache,a);auto owner=Read(cache/"owner.tsv");Claim(cache,a);
  auto otherLabel=[](const Bytes& bytes){auto text=Text(bytes.data(),bytes.size());auto begin=text.find('\t')+1,end=text.find('\n',begin);text.replace(begin,end-begin,"999");return Bytes(text.begin(),text.end());};
  owner=otherLabel(owner);Save(cache/"owner.tsv",owner);Claim(cache,a);
  Save(cache/"current.tsv",Bytes{'p','r','e','v','i','o','u','s'});
  auto previous=Read(cache/"current.tsv");
  for(auto candidate:{b,Package(a)}) {
   if(candidate.id==a.id)candidate.root=b.root;
   bool rejected=false;
   try{Claim(cache,candidate);}catch(const std::exception&){rejected=true;}
   Require(rejected,"another Mod/source must not claim the existing cache");
   Require(Read(cache/"owner.tsv")==owner&&Read(cache/"current.tsv")==previous,"rejection must retain cache publication");
  }
  Claim(folder/"cache-B",b);
  Require(Read(folder/"cache-B/owner.tsv")!=owner,"separate Mod caches must retain separate owners");
  auto setCache=folder/"set-cache";ClaimSet(setCache,folder/"authors");auto setOwner=Read(setCache/"owner.tsv");
  setOwner=otherLabel(setOwner);Save(setCache/"owner.tsv",setOwner);ClaimSet(setCache,folder/"authors");
  std::filesystem::remove(folder/"authors/B");ClaimSet(setCache,folder/"authors");
  std::filesystem::create_directories(folder/"authors/B");ClaimSet(setCache,folder/"authors");
  Require(Read(setCache/"owner.tsv")==setOwner,"Mod membership changed the containing set cache owner");
  bool refused=false;try{ClaimSet(setCache,folder/"foreign-authors");}catch(const std::exception&){refused=true;}Require(refused,"foreign Mod set claimed cache");
  std::filesystem::remove(setCache/"owner.tsv");std::filesystem::remove(setCache);
  std::filesystem::remove(cache/"current.tsv");
  std::filesystem::remove(cache/"owner.tsv");std::filesystem::remove(cache);
  std::filesystem::remove(folder/"cache-B/owner.tsv");std::filesystem::remove(folder/"cache-B");
  std::filesystem::remove(folder/"authors/A");std::filesystem::remove(folder/"authors/B");
  std::filesystem::remove(folder/"authors");std::filesystem::remove(folder);
  std::cout<<"OFFLINE-CACHE-SMOKE PASS: owner, path aliases, cross-Mod rejection, unchanged publication; exact startup links, atomic cache replacement, deletion, changed-stage rejection and copy fallback"<<std::endl;
  return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
