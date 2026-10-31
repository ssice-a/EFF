#include "../../src/eiem_demand_request_registry.h"
#include <iostream>
#include <thread>
using namespace EiemDemandRequests;
int main(){try{
  Registry catalog(64);
  for(uint64_t i=0;i<50000;++i)catalog.ObserveCandidate(1,i,"unrelated",Candidate::Unknown);
  EiemDirectory::Require(!catalog.Size(),"catalog initialization consumed candidate route capacity");
  for(uint64_t i=0;i<57;++i)EiemDirectory::Require(catalog.ObserveCandidate(1,i,"target/"+std::to_string(i),Candidate::Target).token!=0,"unrelated catalog displaced target routes");
  Registry registry(4);
  auto first=registry.Observe(1,10,"Bundles\\Body.ab",Candidate::Unknown);
  auto second=registry.Observe(1,11,"Bundles/Other.ab",Candidate::No);
  EiemDirectory::Require(first.token&&second.token,"request identity absent");
  EiemDirectory::Require(!registry.Resolve({"body.ab"}).token,"basename was guessed");
  EiemDirectory::Require(registry.Resolve({"BUNDLES/body.ab"}).token==first.token,"exact canonical name differs");
  registry.Observe(1,10,"bundles/body.ab",Candidate::Target);
  registry.Observe(1,10,"bundles/body.ab",Candidate::Yes);
  EiemDirectory::Require(registry.Get(first.token).candidate==Candidate::Target,"target certainty was downgraded");
  EiemDirectory::Require(!registry.Observe(1,10,"new/body.ab",Candidate::Target).token,"same epoch accepted changed directory");
  registry.Observe(1,11,"bundles/other.ab",Candidate::Yes);
  EiemDirectory::Require(registry.Get(second.token).candidate==Candidate::Yes,"dependency result could not promote a route");
  registry.BindVfs(first.token,101);registry.BindVfs(second.token,102);
  EiemDirectory::Require(registry.Vfs(101).token==first.token&&!registry.Vfs(999).token,"native VFS key identity differs");
  auto ambiguous=registry.Observe(2,10,"bundles/body.ab",Candidate::Target);
  EiemDirectory::Require(!registry.Resolve({"bundles/body.ab"}).token,"cross-manifest ambiguity was guessed");
  std::thread submit([&]{registry.BindFile(first.token,"D:\\game\\main.chk",120);});submit.join();
  std::thread complete([&]{EiemDirectory::Require(registry.File("d:/game/main.chk",120).token==first.token,"async association lost");});complete.join();
  registry.BindFile(second.token,"d:/game/main.chk",120);
  EiemDirectory::Require(registry.File("d:/game/main.chk",120).token==second.token,"new explicit request did not replace stale route");
  registry.BindVfs(ambiguous.token,101);
  EiemDirectory::Require(!registry.Vfs(101).token,"ambiguous VFS hash was guessed");
  registry.Retire(1,10);
  EiemDirectory::Require(registry.Vfs(101).token==ambiguous.token&&!registry.BindVfs(first.token,404),"VFS retirement lost exact route or resurrected old token");
  EiemDirectory::Require(registry.Resolve({"bundles/body.ab"}).token==ambiguous.token&&!registry.BindFile(first.token,"d:/game/stale",0).token,"retired identity reused");
  registry.RetireScope(1);
  EiemDirectory::Require(!registry.File("d:/game/main.chk",120).token&&registry.Get(ambiguous.token).token,"scope retirement crossed manifest boundary");
  registry.Observe(3,1,"a",Candidate::No);registry.Observe(3,2,"b",Candidate::No);registry.Observe(3,3,"c",Candidate::No);
  EiemDirectory::Require(!registry.Observe(3,4,"d",Candidate::No).token&&registry.Size()==4,"request bound exceeded");
  registry.RetireScope(3);registry.RetireScope(2);
  EiemDirectory::Require(!registry.Size()&&!registry.Vfs(101).token&&!registry.Vfs(102).token,"retired routes leaked");
  std::cout<<"PASS exact request identity, ambiguity, async file correlation, promotion, retirement and bounded capacity\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
