#pragma once
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
namespace EiemInputSpanView {class View;}
namespace EiemEffectNative {
using Bytes=std::vector<unsigned char>;
// Check the actual game envelope before any Unity request is submitted.
// Directory/member validation belongs to the offline writer, not the frame loop.
inline bool Archive(const unsigned char* bytes,size_t size){
 if(!bytes||size<48)return false;
 auto be=[&](size_t p,size_t n){uint64_t v=0;for(size_t i=0;i<n;++i)v=(v<<8)|bytes[p+i];return v;};
 auto rol32=[](uint32_t v,unsigned n){return (v<<n)|(v>>(32-n));};
 auto a=uint32_t(be(0,4))^0x4A92F0CD;
 if(uint32_t(be(4,4))!=((4*a&0xFFFF0000)^rol32(a,18)^0xD8B1E637))return false;
 auto f2=uint32_t(be(10,4));if((be(14,4)^f2)!=8||(be(22,4)^f2^0xA7F49310)!=0x240)return false;
 auto low=uint32_t(be(18,4));auto packed=((be(34,4)^low^0xDAD76848)<<32)|low;
 auto length=((packed<<14)|(packed>>50))^0xA4F1A11747816520ULL;
 auto size32=[&](size_t high,size_t low){auto b=uint16_t(be(low,2));return rol32((uint32_t(uint16_t(be(high,2)^b^0xA121))<<16)|b,14)^0xF74324EE;};
 auto info=size32(38,8),raw=size32(26,32);
 return length==size&&info&&info==raw&&size_t(info)<=size-48&&((size_t(info)+15)&~size_t(15))<size-48;
}
inline bool Archive(const Bytes& bytes){return Archive(bytes.data(),bytes.size());}
inline bool InterchangeArchive(const unsigned char* bytes,size_t size){
 static constexpr unsigned char signature[]={'U','n','i','t','y','F','S',0};
 if(Archive(bytes,size))return true;if(!bytes||size<sizeof(signature))return false;
 for(size_t i=0;i<sizeof(signature);++i)if(bytes[i]!=signature[i])return false;return true;
}
inline bool InterchangeArchive(const Bytes& bytes){return InterchangeArchive(bytes.data(),bytes.size());}
struct Bundle {
 std::string file,hash;std::vector<size_t> dependencies;
 std::shared_ptr<const Bytes> bytes;
 std::shared_ptr<const void> backing;const unsigned char* data=nullptr;size_t size=0;
 // New author packages open a pinned seekable input on actual effect demand.
 // Legacy diagnostic packets can still supply immutable bytes above.
 std::function<std::shared_ptr<const EiemInputSpanView::View>()> open;
 const unsigned char* Data()const{return bytes?bytes->data():data;}
 size_t Size()const{return bytes?bytes->size():size;}
 std::shared_ptr<const void> Owner()const{return bytes?std::shared_ptr<const void>(bytes):backing;}
};
struct Manifest {std::string key,prefab,rootName;size_t root=0,particles=0;std::vector<Bundle> bundles;};
}
