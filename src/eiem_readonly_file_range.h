#pragma once
#include "eiem_loading_input_substitution.h"

namespace EiemReadonlyFileRange {
using namespace EiemNativeInput;
// The handle excludes writes/deletion for the lifetime of every native stream.
// Mapping reserves address space; original blocks are paged in only on reads.
class Range {
 HANDLE file_=INVALID_HANDLE_VALUE,mapping_=nullptr;const unsigned char* mapped_=nullptr;
 size_t delta_=0,size_=0;InputFileStamp stamp_{};
 void Close(){if(mapped_)UnmapViewOfFile(mapped_);if(mapping_)CloseHandle(mapping_);if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);}
public:
 Range(const std::filesystem::path& path,uint64_t offset,uint64_t size){try{
  file_=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  Require(file_!=INVALID_HANDLE_VALUE,"compiled source mapping open failed");BY_HANDLE_FILE_INFORMATION info{};
  Require(GetFileInformationByHandle(file_,&info)!=0,"compiled source mapping stamp failed");
  stamp_={(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow,(uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime};
  Require(size&&size<=SIZE_MAX&&offset<=stamp_.bytes&&size<=stamp_.bytes-offset,"compiled source mapping range differs");
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
  Require(offset<=Size()&&count<=Size()-offset,"compiled backing read exceeds range");std::memcpy(destination,(bytes?bytes->data():file->Data())+offset,count);
 }
};
} // namespace EiemReadonlyFileRange
