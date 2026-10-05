#include "eiem_resource_reload_plan.h"
#include <iostream>
#include <chrono>
#include <psapi.h>
using namespace EiemResourceReloadPlan;
int main(int argc,char** argv) {
 try {
  Require(argc==5,"resource_reload_plan_check <Mod> <baseline> <new-output> <nonce>");
  auto start=std::chrono::steady_clock::now();auto elapsed=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
  auto package=Load(std::filesystem::u8path(argv[1]));std::cout<<"AUTHOR elapsedMs="<<elapsed()<<std::endl;
  std::map<std::string,std::pair<double,size_t>> costs;
  auto plan=Build(package,std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[3]),argv[4],{},[&](const char* stage,size_t n,size_t total){if(!n||n==total)std::cout<<"STAGE "<<stage<<" completed="<<n<<'/'<<total<<" elapsedMs="<<elapsed()<<std::endl;},[&](const char* stage,double ms){auto& value=costs[stage];value.first+=ms;++value.second;});
  PROCESS_MEMORY_COUNTERS memory{};Require(GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory)),"memory measurement failed");
  std::cout<<"TIMING elapsedMs="<<elapsed()<<" peakWorkingSetBytes="<<memory.PeakWorkingSetSize<<std::endl;
  FILETIME created{},exited{},kernel{},user{};Require(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user),"CPU measurement failed");
  auto milliseconds=[](FILETIME value){return (double((uint64_t(value.dwHighDateTime)<<32)|value.dwLowDateTime)/10000.);};
  std::cout<<"CPU userMs="<<milliseconds(user)<<" kernelMs="<<milliseconds(kernel)<<std::endl;
  for(const auto& cost:costs)std::cout<<"COST "<<cost.first<<" elapsedMs="<<cost.second.first<<" calls="<<cost.second.second<<std::endl;
  std::cout<<"BOUNDS hits="<<plan.rendererBoundsHits<<" misses="<<plan.rendererBoundsMisses<<std::endl;
  std::cout<<"PASS resource plan key="<<plan.key<<" inputs="<<plan.inputs.size()<<" closure="<<plan.closure.size()<<" shared="<<plan.shared.size()<<" meshParts="<<plan.meshParts<<" rendererParts="<<plan.rendererParts<<" materials="<<plan.materialMatches<<" textures="<<plan.textures<<" opaque="<<plan.opaque<<'\n';
  for(const auto& r:plan.roots)std::cout<<"ROOT\t"<<r.path<<'\t'<<plan.inputs.at(r.input).file<<'\n';
  for(const auto& m:plan.materials) {
   std::cout<<"MATERIAL\t"<<m.first<<'\t'<<m.second.name<<'\n';
   for(const auto& s:m.second.slots)if(s.nonnull)std::cout<<"TEXTURE\t"<<s.property<<'\t'<<(s.local?s.texture.name:"<shared game dependency>")<<'\n';
  }
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
