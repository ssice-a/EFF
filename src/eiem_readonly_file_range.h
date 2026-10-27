#pragma once
#include "eiem_loading_input_substitution.h"
#include "eiem_load_performance.h"
#include <algorithm>
#include <atomic>
#include <cwctype>
#include <mutex>

namespace EiemReadonlyFileRange {
using namespace EiemNativeInput;
// The handle excludes writes/deletion for the lifetime of every native stream.
// Mapping reserves address space; original blocks are paged in only on reads.
class Range {
 HANDLE file_=INVALID_HANDLE_VALUE,mapping_=nullptr;const unsigned char* mapped_=nullptr;
 size_t delta_=0,size_=0;InputFileStamp stamp_{};std::filesystem::path snapshotPath_,snapshotDirectory_;
 static std::filesystem::path SnapshotDirectory(const std::filesystem::path& path){
  auto at=path.parent_path();for(;!at.empty();){auto name=at.filename().wstring();std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return wchar_t(towlower(c));});
   if(name==L"mods")return at.parent_path()/L".eiem-resource-snapshots";auto parent=at.parent_path();if(parent==at)break;at=std::move(parent);}
  return path.parent_path().parent_path().parent_path()/L".eiem-resource-snapshots";
 }
 static void ReapSnapshotLinks(const std::filesystem::path& directory){
  static std::mutex mutex;static std::set<std::filesystem::path> visited;std::lock_guard<std::mutex> lock(mutex);
  auto absolute=std::filesystem::absolute(directory).lexically_normal();if(!visited.insert(absolute).second)return;
  std::error_code ec;std::filesystem::directory_iterator it(directory,ec),end;
  for(;!ec&&it!=end;it.increment(ec)){auto candidate=it->path();auto name=candidate.filename().wstring();if(name.rfind(L"resource-",0)!=0)continue;
   auto stop=name.find(L'-',9);if(stop==name.npos)continue;auto number=name.substr(9,stop-9);if(number.empty()||number.find_first_not_of(L"0123456789")!=number.npos)continue;
   DWORD pid=0;try{auto value=std::stoull(number);if(!value||value>MAXDWORD)continue;pid=DWORD(value);}catch(const std::exception&){continue;}
   if(pid==GetCurrentProcessId())continue;auto process=OpenProcess(SYNCHRONIZE,FALSE,pid);auto openError=GetLastError();bool dead=false;
   if(process){dead=WaitForSingleObject(process,0)==WAIT_OBJECT_0;CloseHandle(process);}else dead=openError==ERROR_INVALID_PARAMETER;
   if(dead){std::error_code ignored;std::filesystem::remove(candidate,ignored);}
  }
 }
 void Close(){if(mapped_)UnmapViewOfFile(mapped_);if(mapping_)CloseHandle(mapping_);if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);
  if(!snapshotPath_.empty())DeleteFileW(snapshotPath_.c_str());if(!snapshotDirectory_.empty())RemoveDirectoryW(snapshotDirectory_.c_str());}
public:
 Range(const std::filesystem::path& path,uint64_t offset,uint64_t size,bool snapshotForReplacement=false){try{
  auto mappedPath=path;
  if(snapshotForReplacement){
   snapshotDirectory_=SnapshotDirectory(path);std::error_code ec;std::filesystem::create_directories(snapshotDirectory_,ec);Require(!ec,"author resource snapshot directory failed");ReapSnapshotLinks(snapshotDirectory_);
   static std::atomic<uint64_t> serial{0};snapshotPath_=snapshotDirectory_/(L"resource-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(serial.fetch_add(1))+path.extension().wstring());
   Require(CreateHardLinkW(snapshotPath_.c_str(),path.c_str(),nullptr)!=0,"author resource snapshot hardlink failed");mappedPath=snapshotPath_;
  }
  file_=CreateFileW(mappedPath.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  Require(file_!=INVALID_HANDLE_VALUE,"compiled source mapping open failed");BY_HANDLE_FILE_INFORMATION info{};
  Require(GetFileInformationByHandle(file_,&info)!=0,"compiled source mapping stamp failed");
  stamp_={(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow,(uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime};
  if(!size){Require(offset<stamp_.bytes,"compiled source mapping range is empty");size=stamp_.bytes-offset;}
  Require(size<=SIZE_MAX&&offset<=stamp_.bytes&&size<=stamp_.bytes-offset,"compiled source mapping range differs");
  SYSTEM_INFO system{};GetSystemInfo(&system);auto begin=offset-offset%system.dwAllocationGranularity;delta_=size_t(offset-begin);size_=size_t(size);
  auto extent=AddSize(delta_,size_);mapping_=CreateFileMappingW(file_,nullptr,PAGE_READONLY,0,0,nullptr);Require(mapping_,"compiled source mapping creation failed");
  mapped_=static_cast<const unsigned char*>(MapViewOfFile(mapping_,FILE_MAP_READ,DWORD(begin>>32),DWORD(begin),extent));Require(mapped_,"compiled source mapping failed");
 }catch(...){Close();throw;}}
 ~Range(){Close();}Range(const Range&)=delete;Range& operator=(const Range&)=delete;
 size_t Size()const{return size_;}const InputFileStamp& Stamp()const{return stamp_;}
 const unsigned char* Data()const{return mapped_+delta_;}
};
struct Buffer {
 std::shared_ptr<const Bytes> bytes;std::shared_ptr<const Range> file;
 Buffer()=default;Buffer(std::shared_ptr<const Bytes> value):bytes(std::move(value)){}
 Buffer(std::shared_ptr<Bytes> value):bytes(std::move(value)){}
 explicit Buffer(std::shared_ptr<const Range> value):file(std::move(value)){}
 explicit operator bool()const{return bool(bytes)||bool(file);}
 size_t Size()const{return bytes?bytes->size():file->Size();}
 void Read(size_t offset,unsigned char* destination,size_t count)const{
  EiemLoadPerformance::Scope performance(EiemLoadPerformance::BufferRead);performance.hit=bool(file);
  Require(offset<=Size()&&count<=Size()-offset,"compiled backing read exceeds range");std::memcpy(destination,(bytes?bytes->data():file->Data())+offset,count);
  EiemLoadPerformance::Bytes(EiemLoadPerformance::BufferRead,count,performance.hit);
 }
};
} // namespace EiemReadonlyFileRange
