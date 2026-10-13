#pragma once
#include "eiem_offline_resource_package.h"
#include "eiem_offline_compile_helpers.h"
#include <optional>

// The incremental envelope is independent of the compiled input-view backend.
// One Mod file holds immutable blocks and two checksummed commit slots. A
// writer flushes new blocks/index before publishing the next slot. Runtime
// integration is deliberately separate from this storage primitive.
namespace EiemModUpdateStore {
using namespace EiemDirectory;
using EiemOfflinePackage::Writer;
constexpr size_t slotBytes=88,headerBytes=16+2*slotBytes;
inline constexpr char magic[]="EFF_MOD_BLOCKS/1";
struct Block {uint64_t offset=0,size=0;std::string digest;};
struct Snapshot {uint64_t generation=0;std::map<std::string,Block> blocks;};
class Lock {
 HANDLE handle=nullptr;
 public:
 explicit Lock(const std::filesystem::path& path){auto label=EiemNativeInput::InputPath(std::filesystem::weakly_canonical(std::filesystem::absolute(path)).u8string());
  // Windows paths differing only in case must use the same writer mutex.
  std::transform(label.begin(),label.end(),label.begin(),[](unsigned char c){return char(std::tolower(c));});
  auto digest=EiemOfflineCompile::Digest(Bytes(label.begin(),label.end()));auto name=L"Local\\EFFModBlocks-"+std::wstring(digest.begin(),digest.end());
  handle=CreateMutexW(nullptr,FALSE,name.c_str());Require(handle!=nullptr,"Mod block store mutex failed");auto result=WaitForSingleObject(handle,INFINITE);
  if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED){CloseHandle(handle);handle=nullptr;Require(false,"Mod block store mutex wait failed");}
 }
 Lock(const Lock&)=delete;~Lock(){if(handle){ReleaseMutex(handle);CloseHandle(handle);}}
};
class File {
 HANDLE handle=INVALID_HANDLE_VALUE;
 public:
 explicit File(const std::filesystem::path& path,bool write):handle(CreateFileW(path.c_str(),write?GENERIC_READ|GENERIC_WRITE:GENERIC_READ,
       write?FILE_SHARE_READ:FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,write?OPEN_ALWAYS:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)){
  Require(handle!=INVALID_HANDLE_VALUE,"Mod block store open failed");
 }
 File(const File&)=delete;~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
 uint64_t Size()const{LARGE_INTEGER size{};Require(GetFileSizeEx(handle,&size)&&size.QuadPart>=0,"Mod block store size unavailable");return uint64_t(size.QuadPart);}
 void Seek(uint64_t offset){Require(offset<=INT64_MAX,"Mod block store seek overflow");LARGE_INTEGER at{};at.QuadPart=LONGLONG(offset);Require(SetFilePointerEx(handle,at,nullptr,FILE_BEGIN)!=0,"Mod block store seek failed");}
 Bytes ReadAt(uint64_t offset,uint64_t size){auto length=Size();Require(size<=SIZE_MAX&&offset<=length&&size<=length-offset,"Mod block store read escapes file");Seek(offset);Bytes bytes(size_t(size),0);
  for(size_t p=0;p<bytes.size();){auto n=DWORD(std::min<size_t>(bytes.size()-p,1024*1024));DWORD read=0;Require(ReadFile(handle,bytes.data()+p,n,&read,nullptr)&&read==n,"Mod block store read truncated");p+=n;}return bytes;
 }
 void WriteAt(uint64_t offset,const Bytes& bytes){Require(offset<=INT64_MAX&&bytes.size()<=uint64_t(INT64_MAX)-offset,"Mod block store write overflow");Seek(offset);
  for(size_t p=0;p<bytes.size();){auto n=DWORD(std::min<size_t>(bytes.size()-p,1024*1024));DWORD written=0;Require(WriteFile(handle,bytes.data()+p,n,&written,nullptr)&&written==n,"Mod block store write failed");p+=n;}
 }
 void Flush(){Require(FlushFileBuffers(handle)!=0,"Mod block store flush failed");}
};
inline Snapshot Read(File& file){
 auto header=file.ReadAt(0,headerBytes);Require(std::equal(magic,magic+16,header.begin()),"Mod block store structure differs");std::optional<Snapshot> best;bool anyCommit=false;
 for(size_t slot=0;slot<2;++slot){auto at=16+slot*slotBytes;auto generation=Le(header,at,8);
  if(std::all_of(header.begin()+at,header.begin()+at+slotBytes,[](unsigned char b){return b==0;}))continue;
  anyCommit=true;
  auto checksum=Sha(header.data()+at,slotBytes-32);if(!std::equal(checksum.begin(),checksum.end(),header.begin()+at+slotBytes-32)||!generation)continue;
  try{auto offset=Le(header,at+8,8),size=Le(header,at+16,8);Require(offset>=headerBytes,"Mod block index overlaps header");auto bytes=file.ReadAt(offset,size);auto digest=Sha(bytes.data(),bytes.size());
   Require(std::equal(digest.begin(),digest.end(),header.begin()+at+24),"Mod block index digest differs");EiemOfflinePackage::Reader r{bytes};
   Require(r.S()=="EFF_MOD_BLOCK_INDEX/le64"&&r.N()==generation,"Mod block index structure/generation differs");Snapshot snapshot;snapshot.generation=generation;auto count=r.Count();
   for(size_t i=0;i<count;++i){auto name=r.S();Block block;block.offset=r.N();block.size=r.N();block.digest=r.S();
    Require(!name.empty()&&block.offset>=headerBytes&&block.offset<=offset&&block.size<=offset-block.offset&&block.size<=SIZE_MAX&&EiemNativeInput::DigestText(block.digest),"Mod block index record invalid");
    Require(snapshot.blocks.emplace(name,std::move(block)).second,"Mod block index key duplicated");}
   Require(r.p==bytes.size(),"Mod block index suffix differs");if(!best||snapshot.generation>best->generation)best=std::move(snapshot);
  }catch(const std::exception&){/* Try the other completed generation. */}
 }
 if(!anyCommit)return {};Require(best.has_value(),"Mod block store has no readable commit");return std::move(*best);
}
inline Bytes Get(File& file,const Snapshot& snapshot,const std::string& name){const auto& block=snapshot.blocks.at(name);auto bytes=file.ReadAt(block.offset,block.size);Require(EiemOfflineCompile::Digest(bytes)==block.digest,"Mod block payload digest differs");return bytes;}
inline Snapshot Read(const std::filesystem::path& path){File file(path,false);return Read(file);}
// Nullopt removes a live key; old bytes remain unreferenced until compaction.
using Changes=std::map<std::string,std::optional<Bytes>>;
inline void Initialize(File& file){Bytes header(headerBytes,0);std::copy_n(magic,16,header.begin());file.WriteAt(0,header);file.Flush();}
inline void Publish(File& file,const Snapshot& snapshot){
 Writer index;index.S("EFF_MOD_BLOCK_INDEX/le64");index.N(snapshot.generation);index.N(snapshot.blocks.size());for(const auto& row:snapshot.blocks){index.S(row.first);index.N(row.second.offset);index.N(row.second.size);index.S(row.second.digest);}
 auto offset=file.Size();file.WriteAt(offset,index.b);file.Flush();auto digest=Sha(index.b.data(),index.b.size());
 Writer slot;slot.N(snapshot.generation);slot.N(offset);slot.N(index.b.size());slot.Raw(digest.data(),digest.size());auto checksum=Sha(slot.b.data(),slot.b.size());slot.Raw(checksum.data(),checksum.size());
 Require(slot.b.size()==slotBytes,"Mod block commit width differs");file.WriteAt(16+(snapshot.generation%2)*slotBytes,slot.b);file.Flush();
}
inline Snapshot Commit(const std::filesystem::path& path,uint64_t expectedGeneration,const Changes& changes){
 Lock lock(path);File file(path,true);if(!file.Size())Initialize(file);
 auto snapshot=Read(file);Require(snapshot.generation==expectedGeneration,"Mod block store changed before commit");bool changed=false;
 for(const auto& row:changes){Require(!row.first.empty(),"Mod block key empty");auto old=snapshot.blocks.find(row.first);
  if(!row.second){if(old!=snapshot.blocks.end()){snapshot.blocks.erase(old);changed=true;}continue;}
  auto digest=EiemOfflineCompile::Digest(*row.second);if(old!=snapshot.blocks.end()&&old->second.digest==digest&&old->second.size==row.second->size())continue;
  auto offset=file.Size();file.WriteAt(offset,*row.second);snapshot.blocks[row.first]={offset,row.second->size(),std::move(digest)};changed=true;
 }
 if(!changed)return snapshot;Require(snapshot.generation<UINT64_MAX,"Mod block generation overflow");++snapshot.generation;
 Publish(file,snapshot);return snapshot;
}
inline Snapshot Compact(const std::filesystem::path& path,uint64_t expectedGeneration){
 Lock lock(path);static std::atomic<uint64_t> serial{0};auto temporary=path;temporary+=L".compact-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial.fetch_add(1));
 Require(!std::filesystem::exists(temporary),"Mod block compaction temporary already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){DeleteFileW(path.c_str());}}cleanup{temporary};Snapshot next;
 {File source(path,false);auto current=Read(source);Require(current.generation==expectedGeneration&&current.generation<UINT64_MAX,"Mod block store changed before compaction");
  next.generation=current.generation+1;File target(temporary,true);Initialize(target);
  for(const auto& row:current.blocks){auto bytes=Get(source,current,row.first);auto offset=target.Size();target.WriteAt(offset,bytes);next.blocks.emplace(row.first,Block{offset,bytes.size(),row.second.digest});}
  Publish(target,next);
 }
 // ReplaceFile preserves readers' old, delete-shared file handles. Windows
 // MoveFileEx(REPLACE_EXISTING) rejects that pinned snapshot with ERROR_ACCESS_DENIED.
 // Target blocks and slots were already flushed before atomic publication.
 auto moved=ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr);auto error=moved?ERROR_SUCCESS:GetLastError();
 Require(moved!=0,"Mod block compaction publication failed: "+std::to_string(error));return next;
}
} // namespace EiemModUpdateStore
