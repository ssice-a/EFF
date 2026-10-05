#pragma once
#include "eiem_resource_directory.h"
#include <atomic>
#include <exception>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>

namespace EiemOfflineCompile {
using namespace EiemDirectory;
inline std::string Digest(const Bytes& bytes){auto h=Sha(bytes.data(),bytes.size());return Hex(h.data(),h.size());}
inline void ParallelInputs(size_t count,const std::function<void(size_t)>& work,size_t maximum=8){
  if(!count)return;
  auto hardware=std::thread::hardware_concurrency();auto workers=std::min(count,std::max<size_t>(1,std::min(maximum,size_t(hardware?hardware:1))));
  if(workers==1){for(size_t i=0;i<count;++i)work(i);return;}
  std::atomic<size_t> next{0};std::atomic<bool> failed{false};std::exception_ptr error;std::mutex errorMutex;
  std::vector<std::thread> pool;pool.reserve(workers);
  auto join=[&]{for(auto& thread:pool)if(thread.joinable())thread.join();};
  try{
    for(size_t i=0;i<workers;++i)pool.emplace_back([&]{
      while(!failed.load()){
        auto index=next.fetch_add(1);if(index>=count)return;
        try{work(index);}catch(...){std::lock_guard<std::mutex> lock(errorMutex);if(!error)error=std::current_exception();failed.store(true);return;}
      }
    });
  }catch(...){failed.store(true);join();throw;}
  join();if(error)std::rethrow_exception(error);
}
// Stage payloads are immutable and addressed by their exact dependencies.
// The final descriptor is the commit record. A failed or interrupted producer
// leaves an unreferenced temporary file, never a cache hit.
struct Stage {Bytes bytes;size_t first=0,second=0,third=0;};
inline bool ReadStage(const std::filesystem::path& cache,const std::string& key,Stage& result){
  if(cache.empty()||!std::filesystem::exists(cache/(key+".tsv")))return false;
  auto data=Read(cache/(key+".tsv"),1024);std::string text(data.begin(),data.end());auto end=text.rfind("sha256\t");
  Require(end!=text.npos&&end&&text[end-1]=='\n',"compile stage descriptor incomplete");
  Require(Trim(text.substr(end+7))==Digest(Bytes(data.begin(),data.begin()+end)),"compile stage descriptor changed");
  std::istringstream rows(text.substr(0,end));std::string header,hash,extra;uint64_t size=0;
  Require(bool(std::getline(rows,header))&&header=="EFF_COMPILE_STAGE\t1\t"+key,"compile stage identity differs");
  Require(bool(rows>>size>>hash>>result.first>>result.second>>result.third)&&!(rows>>extra)&&size<=512ull*1024*1024&&result.first<=100000&&result.second<=100000&&result.third<=100000,"compile stage counts invalid");
  result.bytes=Read(cache/(key+".vfs"),512ull*1024*1024);
  Require(result.bytes.size()==size&&Digest(result.bytes)==hash,"compile stage payload changed");return true;
}
inline void AtomicSave(const std::filesystem::path& path,const Bytes& bytes){
  static std::atomic<uint64_t> serial{0};auto temporary=path;
  temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial.fetch_add(1));
  {std::ofstream out(temporary,std::ios::binary);Require(bool(out),"compile cache create failed");out.write((const char*)bytes.data(),bytes.size());out.flush();Require(bool(out),"compile cache write failed");}
  Require(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING)!=0,"compile cache atomic publication failed");
}
inline void WriteStage(const std::filesystem::path& cache,const std::string& key,const Stage& value){
  if(cache.empty())return;
  AtomicSave(cache/(key+".vfs"),value.bytes);
  std::string text="EFF_COMPILE_STAGE\t1\t"+key+"\n"+std::to_string(value.bytes.size())+"\t"+Digest(value.bytes)+"\t"+std::to_string(value.first)+"\t"+std::to_string(value.second)+"\t"+std::to_string(value.third)+"\n";
  text+="sha256\t"+Digest(Bytes(text.begin(),text.end()))+"\n";AtomicSave(cache/(key+".tsv"),Bytes(text.begin(),text.end()));
}
} // namespace EiemOfflineCompile
