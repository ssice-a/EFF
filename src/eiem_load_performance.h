#pragma once
#include <atomic>
#include <algorithm>
#include <psapi.h>
void Log(const char*,...);

// Opt-in loading measurements. Production builds have no counters or timers.
namespace EiemLoadPerformance {
enum Stage { File, DependencyAll, DependencyDirect, Prefab, Controls, StreamLifetime, Reload,
 PluginFrame, ControlInput, ControlBindings, StreamRoute, ViewRead, ViewSeek, BufferRead,
 ResourcePrepare, ReloadDrain, DependencyIndex, Logging, ModelRegister, CaptureSnapshot, CaptureRetire, CaptureMatrices, ShapeAnimation, Count };
#if defined(EIEM_LOAD_PERF)
inline constexpr uint64_t histogramUs[]={1,5,10,50,100,250,500,1000,2000,4000,8000,16000,33000,50000,100000,250000,1000000};
inline constexpr size_t histogramBuckets=sizeof(histogramUs)/sizeof(histogramUs[0])+1;
struct Counters {std::atomic<uint64_t> calls{0},ticks{0},longest{0},mainCalls{0},mainTicks{0},mainLongest{0},bytes{0};std::atomic<uint64_t> histogram[histogramBuckets]{};};
inline Counters counters[Count][2];
inline std::atomic<uint64_t> frames[6];
inline std::atomic<uint64_t> wallFrames[6],wallFrameLongest{0},frameNumber{0};
inline std::atomic<DWORD> mainThread{0};
inline uint64_t Frequency(){static uint64_t frequency=[](){LARGE_INTEGER value;QueryPerformanceFrequency(&value);return uint64_t(value.QuadPart);}();return frequency;}
inline uint64_t Clock(){LARGE_INTEGER value;QueryPerformanceCounter(&value);return uint64_t(value.QuadPart);}
inline void Maximum(std::atomic<uint64_t>& value,uint64_t ticks){auto old=value.load(std::memory_order_relaxed);while(old<ticks&&!value.compare_exchange_weak(old,ticks,std::memory_order_relaxed)){}}
inline void ObserveMainThread(){mainThread.store(GetCurrentThreadId(),std::memory_order_relaxed);}
inline void MainFrame(unsigned frame){
 ObserveMainThread();frameNumber.store(frame,std::memory_order_relaxed);
 static thread_local uint64_t previous=0;auto now=Clock();if(previous){auto ticks=now-previous;double ms=double(ticks)*1000/Frequency();unsigned bucket=ms<=8.34?0:ms<=16.67?1:ms<=33.34?2:ms<=50?3:ms<=100?4:5;wallFrames[bucket].fetch_add(1,std::memory_order_relaxed);Maximum(wallFrameLongest,ticks);}previous=now;
}
inline void Record(Stage stage,bool hit,uint64_t ticks){
 auto& value=counters[stage][hit];value.calls.fetch_add(1,std::memory_order_relaxed);value.ticks.fetch_add(ticks,std::memory_order_relaxed);
 Maximum(value.longest,ticks);double us=double(ticks)*1000000/Frequency();size_t bucket=0;while(bucket+1<histogramBuckets&&us>histogramUs[bucket])++bucket;value.histogram[bucket].fetch_add(1,std::memory_order_relaxed);
 if(GetCurrentThreadId()==mainThread.load(std::memory_order_relaxed)){value.mainCalls.fetch_add(1,std::memory_order_relaxed);value.mainTicks.fetch_add(ticks,std::memory_order_relaxed);Maximum(value.mainLongest,ticks);}
}
inline void Bytes(Stage stage,uint64_t bytes,bool hit=true){counters[stage][hit].bytes.fetch_add(bytes,std::memory_order_relaxed);}
struct Scope {
 Stage stage;uint64_t start;bool hit=false;
 explicit Scope(Stage value):stage(value),start(Clock()){}
 ~Scope(){Record(stage,hit,Clock()-start);}
};
inline void Frame(double seconds){
 const double ms=seconds*1000;unsigned bucket=ms<=8.34?0:ms<=16.67?1:ms<=33.34?2:ms<=50?3:ms<=100?4:5;
 frames[bucket].fetch_add(1,std::memory_order_relaxed);
}
inline void Flush(){
 static ULONGLONG previous=0;auto now=GetTickCount64();if(now-previous<5000)return;previous=now;
 const double scale=1000.0/double(Frequency());
 const char* names[]={"file","dependency-all","dependency-direct","prefab","controls","stream-lifetime","reload","plugin-frame","control-input","control-bindings","stream-route","view-read","view-seek","backing-read","resource-prepare","reload-drain","dependency-index","logging","model-register","capture-snapshot","capture-retire","capture-matrices","shape-animation"};
 static_assert(sizeof(names)/sizeof(names[0])==Count,"load performance stage names differ");
 for(unsigned stage=0;stage<Count;++stage)for(unsigned hit=0;hit<2;++hit){auto& value=counters[stage][hit];auto count=value.calls.exchange(0,std::memory_order_relaxed);if(!count)continue;
  auto total=value.ticks.exchange(0,std::memory_order_relaxed),longest=value.longest.exchange(0,std::memory_order_relaxed);
  auto mainCount=value.mainCalls.exchange(0,std::memory_order_relaxed),mainTotal=value.mainTicks.exchange(0,std::memory_order_relaxed),mainMax=value.mainLongest.exchange(0,std::memory_order_relaxed),bytes=value.bytes.exchange(0,std::memory_order_relaxed);
  uint64_t histogram[histogramBuckets],samples=0;for(size_t i=0;i<histogramBuckets;++i){histogram[i]=value.histogram[i].exchange(0,std::memory_order_relaxed);samples+=histogram[i];}uint64_t seen=0;double p95=-1;for(size_t i=0;i<histogramBuckets;++i){seen+=histogram[i];if(samples&&double(seen)>=double(samples)*0.95){p95=i+1<histogramBuckets?double(histogramUs[i])/1000:-1;break;}}
  Log("[DEBUG-loadperf] stage=%s changed=%u calls=%llu totalMs=%.6f averageUs=%.6f maxMs=%.6f mainCalls=%llu mainTotalMs=%.6f mainMaxMs=%.6f p95UpperMs=%.6f bytes=%llu inclusive=1 wallClock=1",names[stage],hit,(unsigned long long)count,double(total)*scale,double(total)*scale*1000/double(count),double(longest)*scale,(unsigned long long)mainCount,double(mainTotal)*scale,double(mainMax)*scale,p95,(unsigned long long)bytes);
 }
 uint64_t values[6];bool any=false;for(unsigned i=0;i<6;++i){values[i]=frames[i].exchange(0,std::memory_order_relaxed);any=any||values[i]!=0;}
 if(any)Log("[DEBUG-loadperf] unityDeltaFrames=1 le8ms=%llu le16ms=%llu le33ms=%llu le50ms=%llu le100ms=%llu over100ms=%llu presentTrace=0",(unsigned long long)values[0],(unsigned long long)values[1],(unsigned long long)values[2],(unsigned long long)values[3],(unsigned long long)values[4],(unsigned long long)values[5]);
 any=false;for(unsigned i=0;i<6;++i){values[i]=wallFrames[i].exchange(0,std::memory_order_relaxed);any=any||values[i]!=0;}
 auto wallMax=wallFrameLongest.exchange(0,std::memory_order_relaxed);if(any)Log("[DEBUG-loadperf] frameHookIntervals=1 le8ms=%llu le16ms=%llu le33ms=%llu le50ms=%llu le100ms=%llu over100ms=%llu maxMs=%.6f mainThread=%lu frame=%llu presentTrace=0",(unsigned long long)values[0],(unsigned long long)values[1],(unsigned long long)values[2],(unsigned long long)values[3],(unsigned long long)values[4],(unsigned long long)values[5],double(wallMax)*scale,mainThread.load(),(unsigned long long)frameNumber.load());
 // These are whole-process counters, never attributed to the DLL alone.
 using MemoryFn=BOOL(WINAPI*)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);static auto memoryFn=reinterpret_cast<MemoryFn>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"K32GetProcessMemoryInfo"));PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);IO_COUNTERS io{};FILETIME created{},exit{},kernel{},user{};
 if(memoryFn&&memoryFn(GetCurrentProcess(),reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&memory),sizeof(memory))&&GetProcessIoCounters(GetCurrentProcess(),&io)&&GetProcessTimes(GetCurrentProcess(),&created,&exit,&kernel,&user)){
  auto number=[](const FILETIME& value){return (uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime;};
  Log("[DEBUG-loadperf] process=1 workingSetBytes=%llu privateBytes=%llu pageFaultCount=%lu readBytes=%llu writeBytes=%llu kernel100ns=%llu user100ns=%llu wholeProcess=1",(unsigned long long)memory.WorkingSetSize,(unsigned long long)memory.PrivateUsage,memory.PageFaultCount,(unsigned long long)io.ReadTransferCount,(unsigned long long)io.WriteTransferCount,(unsigned long long)number(kernel),(unsigned long long)number(user));
 }
}
#else
struct Scope {bool hit=false;explicit Scope(Stage) {}};
inline void ObserveMainThread(){}
inline void MainFrame(unsigned){}
inline void Bytes(Stage,uint64_t,bool=true){}
inline void Frame(double){}
inline void Flush(){}
#endif
} // namespace EiemLoadPerformance
