#pragma once
#include "eiem_native_input.h"
#include <functional>
#include <mutex>

// A seekable view of already compiled bytes. Reading never reconstructs a
// member, interprets an object layout, or expands a native compressed block.
namespace EiemInputSpanView {
using namespace EiemNativeInput;
constexpr size_t literalSource=SIZE_MAX;
struct Span {
 size_t source=literalSource,offset=0,size=0;
 std::shared_ptr<const Bytes> literal;
};
using ReadRange=std::function<void(size_t,size_t,unsigned char*,size_t)>;
class View {
 std::vector<Span> spans_;
 std::vector<size_t> ends_;
 ReadRange read_;
 size_t size_=0;
public:
 View(std::vector<Span> spans,const std::vector<size_t>& sourceSizes,ReadRange read):spans_(std::move(spans)),read_(std::move(read)){
  for(const auto& span:spans_){
   Require(span.size,"compiled input view has an empty span");
   if(span.source==literalSource){Require(span.literal&&span.offset<=span.literal->size()&&span.size<=span.literal->size()-span.offset,"compiled input view literal range differs");}
   else Require(read_&&span.source<sourceSizes.size()&&span.offset<=sourceSizes[span.source]&&span.size<=sourceSizes[span.source]-span.offset,"compiled input view source range differs");
   size_=AddSize(size_,span.size);Require(size_<=INT64_MAX,"compiled input view exceeds stream position representation");ends_.push_back(size_);
  }
 }
 size_t Size()const{return size_;}
 size_t ReadAt(size_t position,unsigned char* destination,size_t count)const{
  if(!count||position>=size_)return 0;Require(destination,"compiled input view read destination absent");
  count=std::min(count,size_-position);auto result=count;
  auto index=size_t(std::upper_bound(ends_.begin(),ends_.end(),position)-ends_.begin());
  while(count){const auto& span=spans_.at(index);auto begin=index?ends_[index-1]:size_t(0),local=position-begin;
   auto size=std::min(count,span.size-local),offset=AddSize(span.offset,local);
   if(span.source==literalSource)std::memcpy(destination,span.literal->data()+offset,size);else read_(span.source,offset,destination,size);
   position+=size;destination+=size;count-=size;++index;
  }return result;
 }
};
class Cursor {
 std::shared_ptr<const View> view_;
 std::mutex mutex_;
 int64_t position_=0;
public:
 explicit Cursor(std::shared_ptr<const View> view):view_(std::move(view)){Require(bool(view_),"compiled input cursor view absent");}
 int64_t Length()const{return int64_t(view_->Size());}
 int64_t Seek(int64_t offset,unsigned origin){
  std::lock_guard<std::mutex> lock(mutex_);Require(origin<=2,"compiled input seek origin differs");
  auto base=origin==0?int64_t(0):origin==1?position_:Length();
  Require(offset>=-base&&(offset<=0||base<=INT64_MAX-offset),"compiled input seek position overflow/negative");
  return position_=base+offset;
 }
 size_t Read(unsigned char* destination,size_t count){
  std::lock_guard<std::mutex> lock(mutex_);auto size=view_->ReadAt(size_t(position_),destination,count);
  position_+=int64_t(size);return size;
 }
};
inline std::string Digest(const View& view){
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
 Require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"compiled input view SHA provider failed");
 struct Close{BCRYPT_ALG_HANDLE& algorithm;BCRYPT_HASH_HANDLE& hash;~Close(){if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);}}close{algorithm,hash};
 Require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0,"compiled input view SHA creation failed");
 Bytes block(std::min(view.Size(),size_t(1024*1024)));for(size_t p=0;p<view.Size();){auto count=view.ReadAt(p,block.data(),std::min(block.size(),view.Size()-p));Require(count&&BCryptHashData(hash,block.data(),ULONG(count),0)>=0,"compiled input view SHA update failed");p+=count;}
 std::array<unsigned char,32> digest{};Require(BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)>=0,"compiled input view SHA finish failed");return Hex(digest.data(),digest.size());
}
} // namespace EiemInputSpanView
