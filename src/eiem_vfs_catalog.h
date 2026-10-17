#pragma once
#include "eiem_loading_input_substitution.h"
#include "eiem_offline_compile_helpers.h"
#include <string_view>

// Shared current-installation routing. Logical Bundle paths select current
// mounted bytes; CHK names and ranges are locations, never resource identities.
namespace EiemVfsCatalog {
using namespace EiemNativeInput;
inline std::string HashBytes(const Bytes& bytes){auto hash=Sha(bytes.data(),bytes.size());return Hex(hash.data(),hash.size());}
inline char Fold(unsigned char c){return c=='\\'?'/':(c>='A'&&c<='Z'?char(c+32):char(c));}
struct Reader {
 const Bytes& b;size_t p=0,limit;
 explicit Reader(const Bytes& data,size_t end=SIZE_MAX):b(data),limit(std::min(end,data.size())){}
 uint64_t N(size_t n){Require(n<=8&&p<=limit&&n<=limit-p,"source index/VFS truncated");auto v=Le(b,p,n);p+=n;return v;}
 std::string_view View(size_t n){Require(n<=limit-p,"source string exceeds encoded extent");auto s=std::string_view((const char*)b.data()+p,n);p+=n;return s;}
 std::string_view S(){uint32_t n=0;for(unsigned i=0;i<5;++i){auto v=unsigned(N(1));Require(i!=4||v<=7,"source index string length invalid");n|=(v&127)<<(i*7);if(!(v&128))return View(n);}throw std::runtime_error("source index string length invalid");}
 size_t Count(size_t minimum=1){auto n=N(4);Require(n<=INT32_MAX&&n<=(limit-p)/minimum,"source count exceeds encoded extent");return size_t(n);}
};
inline uint32_t Rotate(uint32_t v,unsigned n){return (v<<n)|(v>>(32-n));}
inline void ChaCha(Bytes& data,const unsigned char* nonce){
 static const unsigned char key[]={0xE9,0x5B,0x31,0x7A,0xC4,0xF8,0x28,0x56,0x9D,0x23,0xA8,0x6B,0xF2,0x71,0xDC,0xB5,0x3E,0x84,0x6F,0xA7,0x5C,0x92,0x4D,0x67,0x1D,0xBA,0x8E,0x38,0xF4,0xCA,0x52,0xE1};
 auto word=[](const unsigned char* p){uint32_t v;std::memcpy(&v,p,4);return v;};uint32_t state[16]={0x61707865,0x3320646e,0x79622d32,0x6b206574};for(size_t i=0;i<8;++i)state[i+4]=word(key+i*4);state[12]=1;for(size_t i=0;i<3;++i)state[i+13]=word(nonce+i*4);
 for(size_t offset=0;offset<data.size();offset+=64){uint32_t w[16];std::copy_n(state,16,w);auto q=[&](int a,int b,int c,int d){w[a]+=w[b];w[d]=Rotate(w[d]^w[a],16);w[c]+=w[d];w[b]=Rotate(w[b]^w[c],12);w[a]+=w[b];w[d]=Rotate(w[d]^w[a],8);w[c]+=w[d];w[b]=Rotate(w[b]^w[c],7);};
  for(int i=0;i<10;++i){q(0,4,8,12);q(1,5,9,13);q(2,6,10,14);q(3,7,11,15);q(0,5,10,15);q(1,6,11,12);q(2,7,8,13);q(3,4,9,14);}unsigned char block[64];for(size_t i=0;i<16;++i){auto v=w[i]+state[i];std::memcpy(block+i*4,&v,4);}for(size_t i=0;i<std::min<size_t>(64,data.size()-offset);++i)data[offset+i]^=block[i];Require(++state[12]!=0||offset+64>=data.size(),"VFS cipher counter overflow");
 }
}
inline std::string VfsLogical(std::string_view raw){
 std::string path;path.reserve(raw.size());for(unsigned char c:raw)if(c>=0x20&&c<=0x7e)path+=Fold(c);
 auto allowed=[](unsigned char c){return c>=0x20&&c<=0x7e;};
 size_t start=std::string::npos;
 for(size_t i=0;i<path.size();++i){auto prefix=path.compare(i,5,"data/")==0?5:(path.compare(i,7,"assets/")==0?7:0);if(prefix&&i+prefix<path.size()&&allowed(path[i+prefix])){start=i;break;}}
 if(start!=std::string::npos){size_t end=start;while(end<path.size()&&allowed(path[end]))++end;path.resize(end);path.erase(0,start);}
 for(const char* prefix:{"assets/streamingassets/","assets/","data/"})if(path.rfind(prefix,0)==0){path.erase(0,strlen(prefix));break;}
 auto first=path.find_first_not_of('/');if(first==path.npos)return {};path.resize(path.find_last_not_of('/')+1);path.erase(0,first);return path;
}
struct VfsEntry {std::filesystem::path physical;uint64_t offset=0,size=0;std::string md5;bool encrypted=false;std::string lookupName,chunkContentMd5;uint64_t block=0;int32_t blockVersion=0;};
inline std::vector<std::filesystem::path> Files(const std::filesystem::path& root,const wchar_t* extension){std::vector<std::filesystem::path> result;if(!std::filesystem::is_directory(root))return result;for(const auto& row:std::filesystem::recursive_directory_iterator(root))if(row.is_regular_file()&&_wcsicmp(row.path().extension().c_str(),extension)==0)result.push_back(row.path());std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return _wcsicmp(a.c_str(),b.c_str())<0;});return result;}
inline std::string Fingerprint(const std::filesystem::path& root,const std::vector<std::filesystem::path>& blcs){
 std::string text;auto paths=blcs;auto chunks=Files(root,L".chk");paths.insert(paths.end(),chunks.begin(),chunks.end());for(const auto& path:paths){WIN32_FILE_ATTRIBUTE_DATA info{};Require(GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info)!=0,"VFS fingerprint file unavailable");auto size=(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;auto stamp=(uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
  text+=path.lexically_relative(root).generic_u8string()+'|'+std::to_string(size)+'|'+std::to_string(stamp+504911232000000000ull)+'\n';}return HashBytes(Bytes(text.begin(),text.end()));
}
inline std::map<std::string,VfsEntry> ReadVfs(const std::filesystem::path& root,const std::set<std::string>& selected,const std::string& fingerprint={},std::map<uint64_t,int32_t>* versions=nullptr){
 Require(std::filesystem::is_directory(root),"game VFS directory absent");auto files=Files(root,L".blc");if(!fingerprint.empty())Require(Fingerprint(root,files)==fingerprint,"source index no longer describes installed VFS; regenerate it with AnimeStudio");std::map<std::string,VfsEntry> entries;
 struct Parsed {std::map<std::string,VfsEntry> entries;bool any=false;uint64_t block=0;int32_t version=0;};std::vector<Parsed> parsed(files.size());
 EiemOfflineCompile::ParallelInputs(files.size(),[&](size_t index){const auto& file=files[index];auto& output=parsed[index];auto raw=Read(file);Require(raw.size()>=16,"VFS BLC truncated");Bytes data(raw.begin()+12,raw.end());ChaCha(data,raw.data());Reader r(data,data.size()-4);auto version=int32_t(r.N(4));auto code=version<11?version:3;output.version=version<11?int32_t(r.N(4)):version;r.View(size_t(r.N(2)));output.block=r.N(8);r.N(4);r.N(8);r.N(1);auto chunks=r.Count(45);
  for(size_t i=0;i<chunks;++i){auto chunk=r.View(16);auto physical=file.parent_path()/std::filesystem::u8path(Hex((const unsigned char*)chunk.data(),chunk.size())+".chk");auto chunkDigest=r.View(16);r.N(8);r.N(1);if(code>3)r.N(code>=5?8:4);auto count=r.Count(60);
   for(size_t j=0;j<count;++j){auto rawName=r.View(size_t(r.N(2)));auto logical=VfsLogical(rawName);r.N(8);r.View(16);auto digest=r.View(16);auto offset=r.N(8),size=r.N(8);r.N(1);bool encrypted=r.N(1)!=0;if(encrypted)r.N(8);if(code>3)r.N(code>=5?8:4);if(logical.empty())continue;output.any=true;
    if(selected.count(logical)){Require(offset<=INT64_MAX&&size&&size<=INT64_MAX,"VFS source range invalid");output.entries[logical]={physical,offset,size,Hex((const unsigned char*)digest.data(),digest.size()),encrypted,std::string(rawName),Hex((const unsigned char*)chunkDigest.data(),chunkDigest.size()),output.block,output.version};}
   }
  }
 });bool any=false;for(auto& output:parsed){if(versions)Require(versions->emplace(output.block,output.version).second,"duplicate VFS block identity");any=any||output.any;for(auto& row:output.entries)entries[row.first]=std::move(row.second);}Require(any,"game VFS has no logical files");return entries;
}
inline std::string GameFingerprint(const std::filesystem::path& game){
 auto base=game/L"Endfield_Data/StreamingAssets/VFS",hot=game/L"Endfield_Data/Persistent/VFS";auto baseBlcs=Files(base,L".blc"),hotBlcs=Files(hot,L".blc");Require(!baseBlcs.empty(),"game base VFS metadata absent");
 std::string text="EFF_VFS_LAYERS_V1\nstreaming|"+Fingerprint(base,baseBlcs)+"\npersistent|"+(hotBlcs.empty()?std::string("none"):Fingerprint(hot,hotBlcs))+"\n";
 auto add=[&](const char* label,const std::filesystem::path& root,const std::vector<std::filesystem::path>& files){for(const auto& file:files){auto raw=Read(file);text+=std::string(label)+'|'+file.lexically_relative(root).generic_u8string()+'|'+HashBytes(raw)+'\n';}};
 add("streamingBlc",base,baseBlcs);add("persistentBlc",hot,hotBlcs);return "EFF_VFS_LAYERS_V1:"+HashBytes(Bytes(text.begin(),text.end()));
}
inline std::map<std::string,VfsEntry> ReadEffectiveVfs(const std::filesystem::path& game,const std::set<std::string>& selected){
 auto base=game/L"Endfield_Data/StreamingAssets/VFS",hot=game/L"Endfield_Data/Persistent/VFS";std::map<uint64_t,int32_t> baseVersions,hotVersions;
 auto shipped=ReadVfs(base,selected,{},&baseVersions);std::map<std::string,VfsEntry> cached;if(!Files(hot,L".blc").empty())cached=ReadVfs(hot,selected,{},&hotVersions);
 auto active=baseVersions;int32_t baseVersion=0;for(const auto& row:baseVersions)baseVersion=std::max(baseVersion,row.second);
 for(const auto& row:hotVersions){auto prior=active.find(row.first);if((prior!=active.end()&&row.second<prior->second)||(prior==active.end()&&row.second<baseVersion))continue;active[row.first]=row.second;}
 auto available=[](const VfsEntry& e){if(!std::filesystem::is_regular_file(e.physical))return false;auto size=std::filesystem::file_size(e.physical);return e.offset<=size&&e.size<=size-e.offset;};
 std::map<std::string,VfsEntry> result;
 for(const auto& path:selected){auto a=cached.find(path),b=shipped.find(path);const VfsEntry* entry=nullptr;
  if(a!=cached.end()){auto current=active.find(a->second.block);if(current!=active.end()&&current->second==a->second.blockVersion)entry=&a->second;}
  if(a!=cached.end()&&b!=shipped.end()&&a->second.blockVersion==b->second.blockVersion)Require(a->second.md5==b->second.md5&&a->second.size==b->second.size,"same-version VFS content conflict: "+path);
  auto hotBlock=b==shipped.end()?hotVersions.end():hotVersions.find(b->second.block);
  bool replaced=hotBlock!=hotVersions.end()&&hotBlock->second>=b->second.blockVersion&&active.at(b->second.block)==hotBlock->second;
  if(!entry&&b!=shipped.end()&&!replaced&&active.at(b->second.block)==b->second.blockVersion)entry=&b->second;
  Require(entry,"selected source was removed or is absent from the current VFS: "+path);auto chosen=*entry;
  if(!available(chosen)&&a!=cached.end()&&entry==&a->second){auto alternative=chosen;alternative.physical=base/chosen.physical.lexically_relative(hot);if(available(alternative)&&b!=shipped.end()&&b->second.physical==alternative.physical&&b->second.chunkContentMd5==chosen.chunkContentMd5)chosen=std::move(alternative);
   else if(b!=shipped.end()&&b->second.md5==chosen.md5&&b->second.size==chosen.size&&available(b->second))chosen=b->second;}
  Require(available(chosen),"current VFS payload is not downloaded; old different bytes cannot be used: "+path);result.emplace(path,std::move(chosen));
 }return result;
}
inline std::string Md5(const Bytes& bytes){BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE handle=nullptr;unsigned char digest[16];Require(BCryptOpenAlgorithmProvider(&alg,BCRYPT_MD5_ALGORITHM,nullptr,0)>=0,"MD5 provider failed");auto status=BCryptCreateHash(alg,&handle,nullptr,0,nullptr,0,0);for(size_t p=0;p<bytes.size()&&status>=0;){auto n=ULONG(std::min<size_t>(bytes.size()-p,0x10000000));status=BCryptHashData(handle,(PUCHAR)bytes.data()+p,n,0);p+=n;}if(status>=0)status=BCryptFinishHash(handle,digest,16,0);if(handle)BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(alg,0);Require(status>=0,"MD5 source validation failed");return Hex(digest,16);}
} // namespace EiemVfsCatalog
