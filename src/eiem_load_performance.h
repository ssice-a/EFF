#pragma once

// Opt-in loading measurements. Production builds have no counters or timers.
namespace EiemLoadPerformance {
enum Stage { File, DependencyAll, DependencyDirect, Prefab, Controls, StreamLifetime, Reload, Count };
#if defined(EIEM_LOAD_PERF)
struct Counters {std::atomic<uint64_t> calls{0},ticks{0},longest{0};};
inline Counters counters[Count][2];
inline std::atomic<uint64_t> frames[6];
inline uint64_t Clock(){LARGE_INTEGER value;QueryPerformanceCounter(&value);return uint64_t(value.QuadPart);}
inline void Record(Stage stage,bool hit,uint64_t ticks){
 auto& value=counters[stage][hit];value.calls.fetch_add(1,std::memory_order_relaxed);value.ticks.fetch_add(ticks,std::memory_order_relaxed);
 auto old=value.longest.load(std::memory_order_relaxed);
 while(old<ticks&&!value.longest.compare_exchange_weak(old,ticks,std::memory_order_relaxed)){}
}
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
 LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);const double scale=1000.0/double(frequency.QuadPart);
 const char* names[]={"file","dependency-all","dependency-direct","prefab","controls","stream-lifetime","reload"};
 for(unsigned stage=0;stage<Count;++stage)for(unsigned hit=0;hit<2;++hit){auto& value=counters[stage][hit];auto count=value.calls.exchange(0,std::memory_order_relaxed);if(!count)continue;
  auto total=value.ticks.exchange(0,std::memory_order_relaxed),longest=value.longest.exchange(0,std::memory_order_relaxed);
  Log("[DEBUG-loadperf] stage=%s changed=%u calls=%llu totalMs=%.6f averageUs=%.6f maxMs=%.6f",names[stage],hit,(unsigned long long)count,double(total)*scale,double(total)*scale*1000/double(count),double(longest)*scale);
 }
 uint64_t values[6];bool any=false;for(unsigned i=0;i<6;++i){values[i]=frames[i].exchange(0,std::memory_order_relaxed);any=any||values[i]!=0;}
 if(any)Log("[DEBUG-loadperf] unityDeltaFrames=1 le8ms=%llu le16ms=%llu le33ms=%llu le50ms=%llu le100ms=%llu over100ms=%llu presentTrace=0",(unsigned long long)values[0],(unsigned long long)values[1],(unsigned long long)values[2],(unsigned long long)values[3],(unsigned long long)values[4],(unsigned long long)values[5]);
}
#else
struct Scope {bool hit=false;explicit Scope(Stage) {}};
inline void Frame(double){}
inline void Flush(){}
#endif
} // namespace EiemLoadPerformance
