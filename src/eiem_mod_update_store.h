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
inline constexpr char externalMagic[]="EFF_MOD_BLOCKS/2";
struct Block {uint64_t offset=0,size=0;std::string digest,external;};
struct Snapshot {uint64_t generation=0;std::map<std::string,Block> blocks;bool external=false;};
inline std::string LockPath(const std::filesystem::path& path){auto label=EiemNativeInput::InputPath(std::filesystem::weakly_canonical(std::filesystem::absolute(path)).u8string());return label;}
class Lock {
 HANDLE handle=nullptr;
 public:
 explicit Lock(const std::filesystem::path& path){auto label=LockPath(path);
  // Windows paths differing only in case must use the same writer mutex.
  auto digest=EiemOfflineCompile::Digest(Bytes(label.begin(),label.end()));auto name=L"Local\\EFFModBlocks-"+std::wstring(digest.begin(),digest.end());
  handle=CreateMutexW(nullptr,FALSE,name.c_str());Require(handle!=nullptr,"Mod block store mutex failed");auto result=WaitForSingleObject(handle,INFINITE);
  if(result!=WAIT_OBJECT_0&&result!=WAIT_ABANDONED){CloseHandle(handle);handle=nullptr;Require(false,"Mod block store mutex wait failed");}
 }
 Lock(const Lock&)=delete;~Lock(){if(handle){ReleaseMutex(handle);CloseHandle(handle);}}
};
class ForkLocks {
 std::unique_ptr<Lock> first,second;
 public:
 ForkLocks(const std::filesystem::path& source,const std::filesystem::path& destination){
  if(source.empty()||LockPath(source)==LockPath(destination)){second=std::make_unique<Lock>(destination);return;}
  if(LockPath(source)<LockPath(destination)){first=std::make_unique<Lock>(source);second=std::make_unique<Lock>(destination);}
  else{first=std::make_unique<Lock>(destination);second=std::make_unique<Lock>(source);}
 }
 ForkLocks(const ForkLocks&)=delete;
};
class File {
 HANDLE handle=INVALID_HANDLE_VALUE;
 std::filesystem::path path;
 public:
 explicit File(const std::filesystem::path& path,bool write):handle(CreateFileW(path.c_str(),write?GENERIC_READ|GENERIC_WRITE:GENERIC_READ,
       write?FILE_SHARE_READ:FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,write?OPEN_ALWAYS:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)),path(path){
  Require(handle!=INVALID_HANDLE_VALUE,"Mod block store open failed");
 }
 File(const File&)=delete;~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}
 const std::filesystem::path& Path()const{return path;}
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
 auto header=file.ReadAt(0,headerBytes);auto external=std::equal(externalMagic,externalMagic+16,header.begin());Require(external||std::equal(magic,magic+16,header.begin()),"Mod block store structure differs");std::optional<Snapshot> best;bool anyCommit=false;
 for(size_t slot=0;slot<2;++slot){auto at=16+slot*slotBytes;auto generation=Le(header,at,8);
  if(std::all_of(header.begin()+at,header.begin()+at+slotBytes,[](unsigned char b){return b==0;}))continue;
  anyCommit=true;
  auto checksum=Sha(header.data()+at,slotBytes-32);if(!std::equal(checksum.begin(),checksum.end(),header.begin()+at+slotBytes-32)||!generation)continue;
  try{auto offset=Le(header,at+8,8),size=Le(header,at+16,8);Require(offset>=headerBytes,"Mod block index overlaps header");auto bytes=file.ReadAt(offset,size);auto digest=Sha(bytes.data(),bytes.size());
   Require(std::equal(digest.begin(),digest.end(),header.begin()+at+24),"Mod block index digest differs");EiemOfflinePackage::Reader r{bytes};
   Require(r.S()==(external?"EFF_MOD_BLOCK_INDEX/external-blobs/le64":"EFF_MOD_BLOCK_INDEX/le64")&&r.N()==generation,"Mod block index structure/generation differs");Snapshot snapshot;snapshot.generation=generation;snapshot.external=external;auto count=r.Count();
   for(size_t i=0;i<count;++i){auto name=r.S();Block block;block.offset=r.N();block.size=r.N();block.digest=r.S();
    if(external)block.external=r.S();
    Require(!name.empty()&&block.size<=SIZE_MAX&&EiemNativeInput::DigestText(block.digest),"Mod block index record invalid");
    if(external)Require(block.offset==0&&block.external=="compiled.blocks/"+block.digest+".bin","Mod external block location invalid");
    else Require(block.offset>=headerBytes&&block.offset<=offset&&block.size<=offset-block.offset,"Mod block index extent invalid");
    Require(snapshot.blocks.emplace(name,std::move(block)).second,"Mod block index key duplicated");}
   Require(r.p==bytes.size(),"Mod block index suffix differs");if(!best||snapshot.generation>best->generation)best=std::move(snapshot);
  }catch(const std::exception&){/* Try the other completed generation. */}
 }
 if(!anyCommit){Snapshot empty;empty.external=external;return empty;}Require(best.has_value(),"Mod block store has no readable commit");return std::move(*best);
}
inline Bytes Get(File& file,const Snapshot& snapshot,const std::string& name){const auto& block=snapshot.blocks.at(name);Bytes bytes;
 if(block.external.empty())bytes=file.ReadAt(block.offset,block.size);
 else {File blob(file.Path().parent_path()/std::filesystem::u8path(block.external),false);Require(blob.Size()==block.size,"Mod external block size differs");bytes=blob.ReadAt(0,block.size);}
 Require(EiemOfflineCompile::Digest(bytes)==block.digest,"Mod block payload digest differs");return bytes;
}
inline Snapshot Read(const std::filesystem::path& path){File file(path,false);return Read(file);}
// Nullopt removes a live key; old bytes remain unreferenced until compaction.
using Changes=std::map<std::string,std::optional<Bytes>>;
inline void Initialize(File& file,bool external=false){Bytes header(headerBytes,0);std::copy_n(external?externalMagic:magic,16,header.begin());file.WriteAt(0,header);file.Flush();}
inline void Publish(File& file,const Snapshot& snapshot){
 Writer index;index.S(snapshot.external?"EFF_MOD_BLOCK_INDEX/external-blobs/le64":"EFF_MOD_BLOCK_INDEX/le64");index.N(snapshot.generation);index.N(snapshot.blocks.size());for(const auto& row:snapshot.blocks){index.S(row.first);index.N(row.second.offset);index.N(row.second.size);index.S(row.second.digest);if(snapshot.external)index.S(row.second.external);}
 auto offset=file.Size();file.WriteAt(offset,index.b);file.Flush();auto digest=Sha(index.b.data(),index.b.size());
 Writer slot;slot.N(snapshot.generation);slot.N(offset);slot.N(index.b.size());slot.Raw(digest.data(),digest.size());auto checksum=Sha(slot.b.data(),slot.b.size());slot.Raw(checksum.data(),checksum.size());
 Require(slot.b.size()==slotBytes,"Mod block commit width differs");file.WriteAt(16+(snapshot.generation%2)*slotBytes,slot.b);file.Flush();
}
inline void VerifyBlob(const std::filesystem::path& path,const Block& block){
 File file(path,false);Require(file.Size()==block.size,"immutable Mod block size differs");auto bytes=file.ReadAt(0,block.size);
 Require(EiemOfflineCompile::Digest(bytes)==block.digest,"immutable Mod block digest differs");
}
inline std::filesystem::path BlobTemporary(const std::filesystem::path& path){
 static std::atomic<uint64_t> serial{0};auto temporary=path;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(serial.fetch_add(1));return temporary;
}
inline void PublishBlob(const std::filesystem::path& destination,const Block& block,const Bytes* bytes=nullptr,const std::filesystem::path& source={}){
 std::filesystem::create_directories(destination.parent_path());std::error_code error;
 if(std::filesystem::exists(destination)){
  if(!source.empty()&&std::filesystem::equivalent(source,destination,error)&&!error)return;
  VerifyBlob(destination,block);return;
 }
 if(!source.empty()&&CreateHardLinkW(destination.c_str(),source.c_str(),nullptr))return;
 auto temporary=BlobTemporary(destination);Require(!std::filesystem::exists(temporary),"immutable Mod block temporary already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){DeleteFileW(path.c_str());}}cleanup{temporary};
 if(bytes){Require(bytes->size()==block.size&&EiemOfflineCompile::Digest(*bytes)==block.digest,"new immutable Mod block differs");File output(temporary,true);output.WriteAt(0,*bytes);output.Flush();}
 else {Require(!source.empty()&&CopyFileW(source.c_str(),temporary.c_str(),TRUE)!=0,"immutable Mod block copy failed");VerifyBlob(temporary,block);File output(temporary,true);output.Flush();}
 if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH)){
  auto code=GetLastError();Require(code==ERROR_ALREADY_EXISTS||code==ERROR_FILE_EXISTS,"immutable Mod block publication failed: "+std::to_string(code));VerifyBlob(destination,block);
 }
}
// Build a private v2 root from immutable content blocks. Unchanged blocks are
// linked on the same volume; only a cross-volume block needs a physical copy.
// No byte-for-byte copy of the previous compiled.bin occurs on this path.
inline Snapshot Fork(const std::filesystem::path& previous,const std::filesystem::path& path,uint64_t expectedGeneration,const Changes& changes){
 ForkLocks locks(previous,path);std::unique_ptr<File> prior;Snapshot snapshot;
 if(!previous.empty()){prior=std::make_unique<File>(previous,false);snapshot=Read(*prior);}
 Require(snapshot.generation==expectedGeneration,"Mod block store changed before fork");auto old=snapshot;bool changed=!snapshot.external;
 for(const auto& row:changes){Require(!row.first.empty(),"Mod block key empty");auto at=snapshot.blocks.find(row.first);
  if(!row.second){if(at!=snapshot.blocks.end()){snapshot.blocks.erase(at);changed=true;}continue;}
  auto digest=EiemOfflineCompile::Digest(*row.second);if(at!=snapshot.blocks.end()&&at->second.digest==digest&&at->second.size==row.second->size())continue;
  snapshot.blocks[row.first]={0,row.second->size(),std::move(digest),{}};changed=true;
 }
 snapshot.external=true;
 for(auto& row:snapshot.blocks){auto& block=row.second;auto before=old.blocks.find(row.first);auto supplied=changes.find(row.first);block.external="compiled.blocks/"+block.digest+".bin";block.offset=0;
  auto destination=path.parent_path()/std::filesystem::u8path(block.external);
  if(before!=old.blocks.end()&&before->second.digest==block.digest&&before->second.size==block.size){
   if(!before->second.external.empty())PublishBlob(destination,block,nullptr,previous.parent_path()/std::filesystem::u8path(before->second.external));
   else {Require(bool(prior),"fork lacks previous inline block");auto bytes=Get(*prior,old,row.first);PublishBlob(destination,block,&bytes);}
  }else {Require(supplied!=changes.end()&&bool(supplied->second),"fork lacks new block content");PublishBlob(destination,block,&*supplied->second);}
 }
 if(changed||!snapshot.generation){Require(snapshot.generation<UINT64_MAX,"Mod block generation overflow");++snapshot.generation;}
 auto temporary=BlobTemporary(path);Require(!std::filesystem::exists(temporary),"Mod block root temporary already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){DeleteFileW(path.c_str());}}cleanup{temporary};
 {File target(temporary,true);Initialize(target,true);Publish(target,snapshot);}
 if(!previous.empty())Require(Read(previous).generation==expectedGeneration,"Mod block source changed during fork");
 auto replaced=std::filesystem::exists(path)?ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr):MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH);
 Require(replaced!=0,"Mod block fork publication failed: "+std::to_string(GetLastError()));auto verified=Read(path);
 Require(verified.generation==snapshot.generation&&verified.blocks.size()==snapshot.blocks.size(),"Mod block fork differs after publication");
 for(const auto& row:snapshot.blocks){auto found=verified.blocks.find(row.first);Require(found!=verified.blocks.end()&&found->second.digest==row.second.digest&&found->second.size==row.second.size&&found->second.external==row.second.external,"Mod block fork record differs after publication");}
 return verified;
}
inline uint64_t ReapOrphanBlobs(const std::filesystem::path& path,uint64_t threshold=1024*1024){
 Lock lock(path);File file(path,false);auto snapshot=Read(file);if(!snapshot.external)return 0;
 std::set<std::string> live;for(const auto& row:snapshot.blocks)if(!row.second.external.empty())live.insert(row.second.digest);
 auto directory=path.parent_path()/"compiled.blocks";std::error_code error;std::filesystem::directory_iterator it(directory,error),end;if(error)return 0;
 std::vector<std::pair<std::filesystem::path,uint64_t>> orphan;uint64_t total=0;
 for(;it!=end;it.increment(error)){if(error)break;const auto& entry=*it;if(!entry.is_regular_file(error)||error){error.clear();continue;}
  if(entry.path().extension()!=L".bin")continue;auto wideDigest=entry.path().stem().wstring();if(wideDigest.size()!=64||wideDigest.find_first_not_of(L"0123456789ABCDEFabcdef")!=wideDigest.npos)continue;
  std::string digest;digest.reserve(wideDigest.size());for(auto character:wideDigest)digest.push_back(static_cast<char>(character));if(live.count(digest))continue;
  auto size=entry.file_size(error);if(error){error.clear();continue;}orphan.emplace_back(entry.path(),size);total=AddSize(total,size);
 }
 if(error||total<threshold)return 0;uint64_t removed=0;
 // Consumers pin the published blocks with hard links or open handles before
 // the writer removes old names, so an older read snapshot remains usable.
 for(const auto& row:orphan){error.clear();if(std::filesystem::remove(row.first,error)&&!error)removed=AddSize(removed,row.second);}
 return removed;
}
inline Snapshot Commit(const std::filesystem::path& path,uint64_t expectedGeneration,const Changes& changes){
 Snapshot committed;bool changed=false;
 {
  Lock lock(path);File file(path,true);if(!file.Size())Initialize(file);
  committed=Read(file);Require(committed.generation==expectedGeneration,"Mod block store changed before commit");
  for(const auto& row:changes){Require(!row.first.empty(),"Mod block key empty");auto old=committed.blocks.find(row.first);
   if(!row.second){if(old!=committed.blocks.end()){committed.blocks.erase(old);changed=true;}continue;}
   auto digest=EiemOfflineCompile::Digest(*row.second);if(old!=committed.blocks.end()&&old->second.digest==digest&&old->second.size==row.second->size())continue;
   if(committed.external){Block block{0,row.second->size(),digest,"compiled.blocks/"+digest+".bin"};PublishBlob(path.parent_path()/std::filesystem::u8path(block.external),block,&*row.second);committed.blocks[row.first]=std::move(block);}
   else {auto offset=file.Size();file.WriteAt(offset,*row.second);committed.blocks[row.first]={offset,row.second->size(),std::move(digest),{}};}changed=true;
  }
  if(!changed)return committed;Require(committed.generation<UINT64_MAX,"Mod block generation overflow");++committed.generation;Publish(file,committed);
  auto verified=Read(file);Require(verified.generation==committed.generation&&verified.blocks.size()==committed.blocks.size(),"Mod block commit differs after publication");
  for(const auto& row:committed.blocks){auto found=verified.blocks.find(row.first);Require(found!=verified.blocks.end()&&found->second.digest==row.second.digest&&found->second.size==row.second.size&&found->second.external==row.second.external,"Mod block commit record differs after publication");}
  committed=std::move(verified);
 }
 ReapOrphanBlobs(path);return committed;
}
inline Snapshot Compact(const std::filesystem::path& path,uint64_t expectedGeneration){
 Lock lock(path);static std::atomic<uint64_t> serial{0};auto temporary=path;temporary+=L".compact-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(serial.fetch_add(1));
 Require(!std::filesystem::exists(temporary),"Mod block compaction temporary already exists");
 struct Cleanup{std::filesystem::path path;~Cleanup(){DeleteFileW(path.c_str());}}cleanup{temporary};Snapshot next;
 {File source(path,false);auto current=Read(source);Require(current.generation==expectedGeneration&&current.generation<UINT64_MAX,"Mod block store changed before compaction");
  next.generation=current.generation+1;next.external=current.external;File target(temporary,true);Initialize(target,next.external);
  if(current.external)next.blocks=current.blocks;
  else for(const auto& row:current.blocks){auto bytes=Get(source,current,row.first);auto offset=target.Size();target.WriteAt(offset,bytes);next.blocks.emplace(row.first,Block{offset,bytes.size(),row.second.digest,{}});}
  Publish(target,next);
 }
 // ReplaceFile preserves readers' old, delete-shared file handles. Windows
 // MoveFileEx(REPLACE_EXISTING) rejects that pinned snapshot with ERROR_ACCESS_DENIED.
 // Target blocks and slots were already flushed before atomic publication.
 auto moved=ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr);auto error=moved?ERROR_SUCCESS:GetLastError();
 Require(moved!=0,"Mod block compaction publication failed: "+std::to_string(error));return next;
}
} // namespace EiemModUpdateStore
