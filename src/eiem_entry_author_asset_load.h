#pragma once

// Developer integration of source-independent native assets. Inputs are
// immutable and verified on the worker before Request. Only pending requests
// are advanced on the Unity thread; ready assets never enter that queue again.
namespace AuthorAssetLoad {
using RuntimeShapes::Ref;using RuntimeShapes::Call;using RuntimeShapes::Scalar;
using Clock=std::chrono::steady_clock;
enum class Phase {Bundle,Asset,Ready,Failed,Cancelled};
struct Key {
 std::string digest,path,kind;
 bool operator<(const Key& b)const{return std::tie(digest,path,kind)<std::tie(b.digest,b.path,b.kind);}
};
struct Input {Key key;std::shared_ptr<const Bytes> bytes;Ref type;std::function<void(void*)> verify;};
struct Load {
 Input input;Phase phase=Phase::Bundle;Ref request,bundle,asset,label;
 bool cancelled=false;std::string error;
 Clock::time_point started=Clock::now();double submitMs=0,workMs=0,maxStepMs=0,wallMs=0;
};
using Ticket=std::shared_ptr<Load>;
inline double Ms(Clock::time_point begin){return std::chrono::duration<double,std::milli>(Clock::now()-begin).count();}
inline bool Pending(const Load& load){return load.phase==Phase::Bundle||load.phase==Phase::Asset;}
class Cache {
 std::map<Key,Ticket> loads;std::deque<Ticket> work;
 void Release(Load& load){
  if(load.bundle.Get()){unsigned char destroy=1;void* args[]={&destroy};Call(NativeBundle::api.unload,load.bundle.Get(),args);}
  load.asset={};load.bundle={};load.request={};load.label={};load.input.bytes.reset();load.input.type={};load.input.verify={};
 }
 void Step(Load& load){
  try{
   if(!Scalar<unsigned char>(NativeBundle::api.done,load.request.Get()))return;
   if(load.phase==Phase::Bundle){
    load.bundle=Ref::Make(Call(NativeBundle::api.bundle,load.request.Get()),false);
    NativeBundle::CompleteMemory(load.request.Get(),load.bundle.Get());load.request={};
    Require(RuntimeShapes::Alive(load.bundle.Get()),"independent author Bundle async load failed");
    if(load.cancelled){Release(load);load.phase=Phase::Cancelled;return;}
    load.label=Ref::Make(il2cpp_string_new(load.input.key.path.c_str()),false);
    void* args[]={load.label.Get(),load.input.type.Get()};
    load.request=Ref::Make(Call(NativeBundle::api.assetLoad,load.bundle.Get(),args),false);
    Require(load.request.Get(),"independent author asset async request absent");load.phase=Phase::Asset;return;
   }
   load.asset=Ref::Make(Call(NativeBundle::api.result,load.request.Get()),false);load.request={};
   Require(RuntimeShapes::Alive(load.asset.Get()),"independent author async asset absent");
   if(load.cancelled){Release(load);load.phase=Phase::Cancelled;return;}
   load.input.verify(load.asset.Get());load.phase=Phase::Ready;load.wallMs=Ms(load.started);
   // NativeBundle retains backing through the Bundle's actual unload event.
   load.input.bytes.reset();load.input.verify={};load.label={};
  }catch(const std::exception& error){
   load.error=error.what();try{Release(load);}catch(const std::exception& cleanup){load.error+="; cleanup: ";load.error+=cleanup.what();}
   load.phase=Phase::Failed;load.wallMs=Ms(load.started);
  }
 }
 public:
 Ticket Request(Input input){
  Require(RuntimeShapes::Main(),"author asset request must use Unity thread");
  auto found=loads.find(input.key);if(found!=loads.end())return found->second;
  Require(input.key.digest.size()==64&&input.key.digest.find_first_not_of("0123456789ABCDEF")==std::string::npos&&
          input.key.path.rfind("assets/",0)==0&&(input.key.kind=="Mesh"||input.key.kind=="Texture2D")&&
          input.bytes&&!input.bytes->empty()&&input.type.Get()&&input.verify,"author async input identity/verification absent");
  auto load=std::make_shared<Load>();load->input=std::move(input);auto start=Clock::now();
  load->request=Ref::Make(NativeBundle::LoadBytes(load->input.bytes,0,true),false);
  Require(load->request.Get(),"independent author Bundle async request absent");
  load->submitMs=Ms(start);load->workMs=load->submitMs;load->maxStepMs=load->submitMs;
  loads.emplace(load->input.key,load);work.push_back(load);return load;
 }
 void Advance(double budgetMs=4){
  if(work.empty())return;
  Require(RuntimeShapes::Main()&&std::isfinite(budgetMs)&&budgetMs>0,"author pending work thread/budget differs");
  auto begin=Clock::now();auto count=work.size();
  for(size_t i=0;i<count&&(i==0||Ms(begin)<budgetMs);++i){
   auto load=work.front();work.pop_front();auto step=Clock::now();Step(*load);
   auto elapsed=Ms(step);load->workMs+=elapsed;load->maxStepMs=std::max(load->maxStepMs,elapsed);
   if(Pending(*load))work.push_back(std::move(load));
  }
 }
 // Caller must have proved these assets unpublished and unreferenced. Pending
 // native work drains to completion before its Bundle is destroyed.
 void CancelUnpublished(const Ticket& load){
  Require(RuntimeShapes::Main()&&load,"author cancellation thread/ticket absent");load->cancelled=true;
  if(load->phase==Phase::Ready){Release(*load);load->phase=Phase::Cancelled;}
 }
 size_t PendingCount()const{return work.size();}
};
}
