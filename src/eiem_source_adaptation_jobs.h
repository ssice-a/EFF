#pragma once
#include "eiem_consumer_update_records.h"
#include "eiem_compiled_input_view.h"
#include "eiem_compiled_mod.h"
#include <condition_variable>
#include <deque>
#include <future>
#include <thread>

// CPU/disk-only preparation. This queue does not call Unity, fabricate a
// native async request or silently forward a pending candidate as original.
// The game delivery seam must await the ticket before submitting Unity.
namespace EiemSourceAdaptationJobs {
using namespace EiemNativeInput;
using Stamp=EiemConsumerUpdateRecords::Stamp;
struct Result {
 EiemConsumerUpdateRecords::Decision decision;
 std::shared_ptr<const EiemCompiledInputView::Recipe> view;
 std::shared_ptr<const EiemCompiledMod::Artifact> artifact;size_t input=SIZE_MAX;
};
using Ticket=std::shared_future<Result>;
struct Request {std::string source;Stamp stamp;};
class Queue {
 struct Job {Request request;std::promise<Result> promise;};
 struct Entry {Stamp stamp;Ticket ticket;};
 std::function<Result(const Request&)> build;
 std::mutex mutex;std::condition_variable changed;bool stopping=false;
 std::deque<std::shared_ptr<Job>> pending;std::map<std::string,Entry> entries;std::thread worker;
 void Run(){for(;;){std::shared_ptr<Job> job;
   {std::unique_lock<std::mutex> lock(mutex);changed.wait(lock,[&]{return stopping||!pending.empty();});if(pending.empty())return;job=std::move(pending.front());pending.pop_front();}
   try{auto result=build(job->request);Require(result.decision.stamp==job->request.stamp,"adaptation result belongs to another source/contract");
    Require(result.decision.roots.empty()?!result.view:bool(result.view),"adaptation hit/view coverage differs");
    if(result.view)Require(result.view->sourceHash==job->request.stamp.source,"adaptation view source identity differs");job->promise.set_value(std::move(result));
   }catch(...){job->promise.set_exception(std::current_exception());}
  }}
 public:
 explicit Queue(std::function<Result(const Request&)> compiler):build(std::move(compiler)){
  Require(bool(build),"source adaptation compiler absent");worker=std::thread([this]{Run();});
 }
 Queue(const Queue&)=delete;
 ~Queue(){{std::lock_guard<std::mutex> lock(mutex);stopping=true;}changed.notify_all();if(worker.joinable())worker.join();}
 Ticket Submit(Request request){Require(!request.source.empty(),"source adaptation identity empty");EiemConsumerUpdateRecords::Validate(request.stamp);
  std::lock_guard<std::mutex> lock(mutex);Require(!stopping,"source adaptation queue stopped");auto old=entries.find(request.source);
  if(old!=entries.end()&&old->second.stamp==request.stamp)return old->second.ticket;
  auto job=std::make_shared<Job>();job->request=std::move(request);auto ticket=job->promise.get_future().share();
  entries[job->request.source]={job->request.stamp,ticket};pending.push_back(std::move(job));changed.notify_one();return ticket;
 }
 // An unchanged failed ticket remains failed until explicit retry or a new
 // source/contract; native queue polling cannot launch endless builds.
 void Retry(const std::string& source,const Stamp& stamp){std::lock_guard<std::mutex> lock(mutex);auto found=entries.find(source);
  if(found!=entries.end()&&found->second.stamp==stamp){Require(found->second.ticket.wait_for(std::chrono::milliseconds(0))==std::future_status::ready,"adaptation still preparing");entries.erase(found);}
 }
};
inline Result BeforeNativeLoad(const Ticket& ticket,const Stamp& current){Require(ticket.valid(),"candidate adaptation ticket absent");auto result=ticket.get();Require(result.decision.stamp==current,"resource changed before native delivery");return result;}
enum class Delivery {Pending,Original,Replacement};
struct PollResult {Delivery state=Delivery::Pending;std::vector<Result> results;};
inline PollResult PollBeforeNativeGroup(const std::vector<std::pair<Ticket,Stamp>>& required){
 // Native queue integration must retain the entire dependency group when
 // pending. No get/wait blocks its caller, and no pending source falls through
 // to original Unity loading. Poll only at native queue readiness boundaries.
 for(const auto& item:required){Require(item.first.valid(),"native group has no adaptation ticket");if(item.first.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return {};}
 PollResult result;result.state=Delivery::Original;
 for(const auto& item:required){auto prepared=BeforeNativeLoad(item.first,item.second);if(prepared.view)result.state=Delivery::Replacement;result.results.push_back(std::move(prepared));}
 return result;
}
} // namespace EiemSourceAdaptationJobs
