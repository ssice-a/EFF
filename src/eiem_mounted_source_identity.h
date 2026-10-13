#pragma once
#include "eiem_resource_directory.h"

// A mounted catalog seal decides whether a source must be read again. It is
// not the SHA256 of the source bytes used by the compiler. In this game's VFS,
// the CHK name and its contentMD5 are different fields; only contentMD5 seals
// bytes. Without a real per-file digest, changing any byte in a containing CHK
// conservatively invalidates its selected ranges, never the whole game.
namespace EiemMountedSourceIdentity {
using namespace EiemDirectory;
struct Entry {
 std::string physical,chunkName,chunkContentMd5,fileContentMd5;
 uint64_t offset=0,size=0;
 bool encrypted=false;uint32_t ivSeed=0;
};
inline std::string Digest(std::string value){
 Require(value.size()==32,"mounted catalog MD5 absent");
 for(auto& ch:value){Require((ch>='0'&&ch<='9')||(ch>='a'&&ch<='f')||(ch>='A'&&ch<='F'),"mounted catalog MD5 invalid");if(ch>='a'&&ch<='f')ch-=32;}
 return value;
}
inline void Validate(const Entry& entry){
 Require(!entry.physical.empty()&&entry.size&&entry.offset<=INT64_MAX&&entry.size<=uint64_t(INT64_MAX)-entry.offset,"mounted catalog source range invalid");
 Digest(entry.chunkName);Digest(entry.chunkContentMd5);if(!entry.fileContentMd5.empty())Digest(entry.fileContentMd5);
}
inline std::string ContentKey(const Entry& entry){
 Validate(entry);std::string seal;
 if(!entry.fileContentMd5.empty())seal="file-md5/"+Digest(entry.fileContentMd5);
 else seal="chunk-content-md5/"+Digest(entry.chunkContentMd5)+"/range/"+std::to_string(entry.offset);
 seal+="/bytes/"+std::to_string(entry.size)+"/encrypted/"+std::to_string(entry.encrypted);
 if(entry.encrypted)seal+="/iv/"+std::to_string(entry.ivSeed);
 auto hash=Sha((const unsigned char*)seal.data(),seal.size());return Hex(hash.data(),hash.size());
}
inline std::string LocationKey(const Entry& entry){
 Validate(entry);auto path=entry.physical;for(auto& ch:path){if(ch=='\\')ch='/';else if(ch>='A'&&ch<='Z')ch+=32;}
 return path+"/chunk/"+Digest(entry.chunkName)+"/range/"+std::to_string(entry.offset)+"/bytes/"+std::to_string(entry.size);
}
} // namespace EiemMountedSourceIdentity
