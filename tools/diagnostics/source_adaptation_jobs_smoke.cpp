#include "../../src/eiem_source_adaptation_jobs.h"
#include <atomic>
#include <iostream>
using namespace EiemSourceAdaptationJobs;
template<class F> void Reject(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Require(rejected,"invalid delivery accepted");}
int main(){try{
 auto hash=[](unsigned char v){auto digest=EiemDirectory::Sha(&v,1);return EiemDirectory::Hex(digest.data(),digest.size());};Stamp stamp{hash(1),hash(2),hash(3),"exact-mesh-owner"};
 std::promise<void> release,started;auto wait=release.get_future().share();auto entered=started.get_future();std::atomic<size_t> builds{0},delivered{0};
 Queue queue([&](const Request& request){++builds;if(request.source=="new-shared-bundle"){started.set_value();wait.wait();}
  Result result;result.decision.stamp=request.stamp;if(request.source=="failed")throw std::runtime_error("source schema differs");
  if(request.source!="negative"){result.decision.roots={{"a.prefab","CAB-a",10,{13}},{"b.prefab","CAB-a",20,{23}}};auto view=std::make_shared<EiemCompiledInputView::Recipe>();view->sourceHash=request.stamp.source;result.view=view;}return result;});
 auto first=queue.Submit({"new-shared-bundle",stamp});entered.wait();auto duplicate=queue.Submit({"new-shared-bundle",stamp});
 auto delivery=std::async(std::launch::async,[&]{auto result=BeforeNativeLoad(duplicate,stamp);++delivered;return result;});
 Require(first.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout&&delivered==0,"pending candidate was delivered as original");
 auto pendingGroup=PollBeforeNativeGroup({{first,stamp}});Require(pendingGroup.state==Delivery::Pending&&pendingGroup.results.empty(),"native dependency group blocked or forwarded an unready original");
 release.set_value();Require(delivery.get().decision.roots.size()==2&&builds==1&&delivered==1,"coalesced first delivery lost roots or rebuilt source");
 Require(PollBeforeNativeGroup({{first,stamp}}).state==Delivery::Replacement,"first ready native group failed to select modified PFB");
 Require(BeforeNativeLoad(queue.Submit({"new-shared-bundle",stamp}),stamp).view&&builds==1,"ready source rebuilt on repeat load");
 auto negative=BeforeNativeLoad(queue.Submit({"negative",stamp}),stamp);Require(negative.decision.roots.empty()&&!negative.view,"verified negative generated a replacement");
 Require(PollBeforeNativeGroup({{queue.Submit({"negative",stamp}),stamp}}).state==Delivery::Original&&PollBeforeNativeGroup({{first,stamp},{queue.Submit({"negative",stamp}),stamp}}).state==Delivery::Replacement,"dependency group lost negative/positive result semantics");
 auto changed=stamp;changed.dependencies=hash(9);Reject([&]{BeforeNativeLoad(first,changed);});
 Reject([&]{PollBeforeNativeGroup({{first,changed}});});
 Require(BeforeNativeLoad(queue.Submit({"changed",changed}),changed).view&&builds==3,"changed dependency did not prepare current view");
 auto failed=queue.Submit({"failed",stamp});Reject([&]{BeforeNativeLoad(failed,stamp);});auto failedBuilds=builds.load();
 Reject([&]{BeforeNativeLoad(queue.Submit({"failed",stamp}),stamp);});Require(builds==failedBuilds,"failed native polling retried unchanged source");
 queue.Retry("failed",stamp);Reject([&]{BeforeNativeLoad(queue.Submit({"failed",stamp}),stamp);});Require(builds==failedBuilds+1,"explicit retry did not prepare source");
 std::cout<<"PASS CPU preparation coalescing, first delivery waits, same-Bundle roots, verified negative forwarding, changed dependencies, stale ticket rejection, failure retention and explicit retry; no Unity API invoked\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
