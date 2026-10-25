#pragma once
#include "eiem_compiled_source_records.h"
#include "eiem_mod_update_store.h"
#include "eiem_consumer_update_records.h"
#include "eiem_resource_catalog_delta.h"
#include "eiem_static_resource_sources.h"
#include <cwctype>

// One file with independently compiled source blocks. Incremental writes never
// encode the previous whole packet or recompress shared author frames.
// Still under local validation; production reader migration is separate.
namespace EiemCompiledModStore {
using namespace EiemCompiledMod;
using Changes=EiemModUpdateStore::Changes;
struct WriteStats {size_t sourcesEncoded=0,proofsEncoded=0,framesEncoded=0,bytesPrepared=0;};
inline bool IsStore(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);Require(bool(in),"compiled export source cannot be opened");char prefix[16]{};in.read(prefix,16);return in.gcount()==16&&(std::equal(prefix,prefix+16,EiemModUpdateStore::magic)||std::equal(prefix,prefix+16,EiemModUpdateStore::externalMagic));
}
inline Bytes SourceSelection(const EiemStaticSources::Selection& selection,const std::filesystem::path& game){
 auto portable=selection;auto base=std::filesystem::weakly_canonical(std::filesystem::absolute(game));
 for(auto& row:portable.inputs){auto path=std::filesystem::weakly_canonical(row.second.physical).lexically_relative(base);Require(!path.empty()&&!path.is_absolute()&&path.generic_u8string().rfind("..",0)!=0,"export carryover source escapes game");row.second.physical=std::move(path);}return EiemStaticSources::Encode(portable);
}
inline EiemStaticSources::Selection ReadSourceSelection(const Bytes& bytes,const std::filesystem::path& game){
 auto selection=EiemStaticSources::Decode(bytes);auto base=std::filesystem::weakly_canonical(std::filesystem::absolute(game));
 for(auto& row:selection.inputs){auto& path=row.second.physical;Require(!path.is_absolute()&&path.generic_u8string().rfind("..",0)!=0,"export carryover source must be game-relative");path=std::filesystem::weakly_canonical(base/path);Require(path.lexically_relative(base).generic_u8string().rfind("..",0)!=0,"export carryover source symlink escapes game");}return selection;
}
inline void VerifySourceSelection(const Artifact& artifact,const EiemStaticSources::Selection& selection){
 std::map<std::string,std::string> represented;for(const auto& row:selection.inputs){auto identity=row.first+'\n'+row.second.revision;represented.emplace(Hash(Bytes(identity.begin(),identity.end()))+".vfs",row.first);}
 const auto& plan=artifact.manifest.plan;for(size_t i=0;i<plan.originals.size();++i){const auto& source=plan.originals[i];auto logical=represented.find(source.file);Require(logical!=represented.end(),"export carryover lacks the logical identity of compiled source: "+source.file);
  if(!plan.rendererRules.at(i).empty()&&std::any_of(plan.roots.begin(),plan.roots.end(),[&](const auto& root){return root.input==i;}))Require(selection.candidates.count(logical->second),"export carryover omits a previously compiled positive candidate: "+logical->second);
 }
}
inline Bytes AuthorSeal(const Artifact& a){Writer w;w.S(a.manifest.plan.author);w.S(a.manifest.plan.contract);for(const auto& frame:a.manifest.plan.frameRecipes)EiemCompiledSourceRecords::Frame(w,frame);return std::move(w.b);}
inline Bytes Order(const Artifact& a){Writer w;w.S("EFF_COMPILED_SOURCE_ORDER/identity/le64");w.N(a.views.size());for(const auto& source:a.manifest.plan.originals)w.S(source.file);return std::move(w.b);}
inline Changes RuntimeRecords(const Artifact& a,const std::filesystem::path& game,const std::set<std::string>* affected=nullptr,WriteStats* stats=nullptr){
 Changes records{{"runtime/common",EiemCompiledSourceRecords::Common(a)},{"runtime/order",Order(a)},{"runtime/author-seal",AuthorSeal(a)}};
 if(a.explicitResources){Require(!a.resourceBindingBytes.empty(),"explicit resource bindings absent");records["runtime/resource-bindings"]=a.resourceBindingBytes;}
 if(!a.sharedSchemas.empty())records["runtime/schema-dictionary"]=EiemCompiledSourceRecords::EncodeSchemas(a.sharedSchemas);
 auto owners=EiemCompiledSourceRecords::MemberInputs(a);auto schemas=EiemCompiledSourceRecords::PrepareSchemas(a);const auto& p=a.manifest.plan;std::set<std::string> found;
 for(size_t i=0;i<p.inputs.size();++i){const auto& identity=p.originals.at(i).file;if(affected&&!affected->count(identity))continue;found.insert(identity);
  auto directory=EiemCompiledSourceRecords::Directory(a,i,owners);records["runtime/source/"+identity]=EiemCompiledSourceRecords::Source(a,i,game,directory);
  records["runtime/proof/"+identity]=EiemCompiledSourceRecords::Proof(a,i,directory,&schemas);if(stats){++stats->sourcesEncoded;++stats->proofsEncoded;}}
 if(affected)Require(found==*affected,"affected source absent from compiled artifact");
 if(!affected)for(const auto& frame:a.preparedFrames){auto key=frame.first.rfind("@bounds:",0)==0?"runtime/generated/":frame.first.rfind("@literal:",0)==0?"runtime/literal/":"runtime/frame/";records[key+frame.first]=Pack(frame.second);if(stats)++stats->framesEncoded;}
 if(stats)for(const auto& row:records)if(row.second)stats->bytesPrepared+=row.second->size();return records;
}
inline EiemModUpdateStore::Snapshot PublishSnapshot(const std::filesystem::path& previous,const std::filesystem::path& path,uint64_t generation,const Changes& changes){
 auto published=EiemModUpdateStore::Fork(previous,path,generation,changes);EiemModUpdateStore::ReapOrphanBlobs(path);return published;
}
inline void Discovery(Changes& records,const EiemConsumerUpdateRecords::Decisions& decisions,const EiemResourceCatalogDelta::Catalog& catalog){
 records["discovery/decisions"]=EiemConsumerUpdateRecords::Encode(decisions);records["discovery/catalog"]=EiemResourceCatalogDelta::Encode(catalog);}
inline EiemModUpdateStore::Snapshot Save(const std::filesystem::path& path,uint64_t generation,const Artifact& artifact,const std::filesystem::path& game,
 const EiemConsumerUpdateRecords::Decisions& decisions,const EiemResourceCatalogDelta::Catalog& catalog,const Bytes* preparedPrefab=nullptr,const EiemStaticSources::Selection* selection=nullptr){
 auto records=RuntimeRecords(artifact,game);Discovery(records,decisions,catalog);
 if(selection){VerifySourceSelection(artifact,*selection);records["discovery/source-selection"]=SourceSelection(*selection,game);}
 if(preparedPrefab)records["runtime/prepared-pfb"]=*preparedPrefab;
 if(std::filesystem::exists(path)){auto old=EiemModUpdateStore::Read(path);Require(old.generation==generation,"compiled store changed during preparation");
  if(!selection&&old.blocks.count("discovery/source-selection")){EiemModUpdateStore::File file(path,false);auto bytes=EiemModUpdateStore::Get(file,old,"discovery/source-selection");VerifySourceSelection(artifact,ReadSourceSelection(bytes,game));records["discovery/source-selection"]=std::move(bytes);}
  for(const auto& row:old.blocks)if(!records.count(row.first))records[row.first]=std::nullopt;}
 return PublishSnapshot(std::filesystem::exists(path)?path:std::filesystem::path{},path,generation,records);
}
struct Loaded {Artifact artifact;EiemConsumerUpdateRecords::Decisions decisions;EiemResourceCatalogDelta::Catalog catalog;uint64_t generation=0;std::optional<EiemStaticSources::Selection> selection;
 bool externalSnapshot=false;DWORD snapshotLinkError=ERROR_SUCCESS;
};
inline EiemModUpdateStore::Snapshot SaveAffected(const std::filesystem::path& path,uint64_t generation,const Artifact& a,const std::filesystem::path& game,
 const std::set<std::string>& affected,const std::set<std::string>& retired,const EiemConsumerUpdateRecords::Decisions& decisions,const EiemResourceCatalogDelta::Catalog& catalog,WriteStats* stats=nullptr,const EiemStaticSources::Selection* selection=nullptr){
 auto snapshot=EiemModUpdateStore::Read(path);Require(snapshot.generation==generation,"compiled source store changed during adaptation");
 auto seal=AuthorSeal(a);auto prior=snapshot.blocks.find("runtime/author-seal");Require(prior!=snapshot.blocks.end()&&prior->second.digest==Hash(seal),"source delta changes author snapshot; publish author resources first");
 auto records=RuntimeRecords(a,game,&affected,stats);std::set<std::string> current;
 for(const auto& source:a.manifest.plan.originals){current.insert(source.file);Require(affected.count(source.file)||(snapshot.blocks.count("runtime/source/"+source.file)&&snapshot.blocks.count("runtime/proof/"+source.file)),"new source omitted from affected records");}
 for(const auto& key:retired){Require(!current.count(key)&&!affected.count(key),"retired source remains active");records["runtime/source/"+key]=std::nullopt;records["runtime/proof/"+key]=std::nullopt;}
 for(const auto& row:snapshot.blocks)if(row.first.rfind("runtime/source/",0)==0){auto key=row.first.substr(strlen("runtime/source/"));Require(current.count(key)||retired.count(key),"removed source omitted from retirement transaction");}
 for(const auto& frame:a.manifest.plan.frameRecipes)Require(snapshot.blocks.count("runtime/frame/"+frame.resource),"source delta cannot introduce author frames; publish author snapshot first");
 // New native consumers may add a bounds context or a pooled TypeTree. Their
 // immutable buffers and dependency metadata commit with the new source.
 std::set<std::string> generated;
 for(const auto& row:a.preparedFrames)if(row.first.rfind("@bounds:",0)==0||row.first.rfind("@literal:",0)==0){auto key=(row.first.rfind("@bounds:",0)==0?"runtime/generated/":"runtime/literal/")+row.first;generated.insert(key);if(!snapshot.blocks.count(key))records[key]=Pack(row.second);}
 for(const auto& row:snapshot.blocks)if((row.first.rfind("runtime/generated/",0)==0||row.first.rfind("runtime/literal/",0)==0)&&!generated.count(row.first))records[row.first]=std::nullopt;
 if(selection){VerifySourceSelection(a,*selection);records["discovery/source-selection"]=SourceSelection(*selection,game);}
 else if(snapshot.blocks.count("discovery/source-selection")){EiemModUpdateStore::File file(path,false);VerifySourceSelection(a,ReadSourceSelection(EiemModUpdateStore::Get(file,snapshot,"discovery/source-selection"),game));}
 Discovery(records,decisions,catalog);return EiemModUpdateStore::Commit(path,generation,records);
}
inline EiemModUpdateStore::Snapshot SaveAuthorRevision(const std::filesystem::path& previous,const std::filesystem::path& path,uint64_t generation,
 const Artifact& a,const std::filesystem::path& game,const std::set<std::string>& affected,const EiemConsumerUpdateRecords::Decisions& decisions,
 const EiemResourceCatalogDelta::Catalog& catalog,const Bytes* preparedPrefab,const EiemStaticSources::Selection& selection,
 const std::set<std::string>& changedResources,WriteStats* stats=nullptr){
 auto snapshot=EiemModUpdateStore::Read(previous);Require(snapshot.generation==generation,"compiled source store changed during author update");
 auto records=RuntimeRecords(a,game,&affected,stats);std::set<std::string> current;
 for(const auto& source:a.manifest.plan.originals){current.insert(source.file);Require(affected.count(source.file)||
  (snapshot.blocks.count("runtime/source/"+source.file)&&snapshot.blocks.count("runtime/proof/"+source.file)),"author revision omitted a new source record");}
 for(const auto& key:affected)Require(current.count(key)&&records.count("runtime/source/"+key)&&records.count("runtime/proof/"+key),"author revision affected source absent");
 for(const auto& row:snapshot.blocks)if(row.first.rfind("runtime/source/",0)==0){auto key=row.first.substr(strlen("runtime/source/"));
  if(!current.count(key)){records["runtime/source/"+key]=std::nullopt;records["runtime/proof/"+key]=std::nullopt;}}
 std::set<std::string> frames;
 for(const auto& frame:a.manifest.plan.frameRecipes){frames.insert(frame.resource);auto key="runtime/frame/"+frame.resource;
  if(!snapshot.blocks.count(key)||changedResources.count(frame.resource))records[key]=Pack(a.preparedFrames.at(frame.resource));}
 for(const auto& row:snapshot.blocks)if(row.first.rfind("runtime/frame/",0)==0&&!frames.count(row.first.substr(strlen("runtime/frame/"))))records[row.first]=std::nullopt;
 std::set<std::string> generated;
 for(const auto& row:a.preparedFrames){const bool bounds=row.first.rfind("@bounds:",0)==0,literal=row.first.rfind("@literal:",0)==0;if(!bounds&&!literal)continue;
  auto key=(bounds?"runtime/generated/":"runtime/literal/")+row.first;generated.insert(key);
  if(!snapshot.blocks.count(key)||(bounds&&!changedResources.empty()))records[key]=Pack(row.second);
 }
 for(const auto& row:snapshot.blocks)if((row.first.rfind("runtime/generated/",0)==0||row.first.rfind("runtime/literal/",0)==0)&&!generated.count(row.first))records[row.first]=std::nullopt;
 if(!a.explicitResources&&snapshot.blocks.count("runtime/resource-bindings"))records["runtime/resource-bindings"]=std::nullopt;
 if(a.sharedSchemas.empty()&&snapshot.blocks.count("runtime/schema-dictionary"))records["runtime/schema-dictionary"]=std::nullopt;
 if(preparedPrefab)records["runtime/prepared-pfb"]=*preparedPrefab;else if(snapshot.blocks.count("runtime/prepared-pfb"))records["runtime/prepared-pfb"]=std::nullopt;
 VerifySourceSelection(a,selection);records["discovery/source-selection"]=SourceSelection(selection,game);Discovery(records,decisions,catalog);
 return PublishSnapshot(previous,path,generation,records);
}
inline EiemModUpdateStore::Snapshot SaveDiscovery(const std::filesystem::path& path,uint64_t generation,
 const EiemConsumerUpdateRecords::Decisions& decisions,const EiemResourceCatalogDelta::Catalog& catalog){
 // Negative conclusions and location-only changes do not encode any native
 // object/proof or rewrite the runtime index.
 return EiemModUpdateStore::Commit(path,generation,{{"discovery/decisions",EiemConsumerUpdateRecords::Encode(decisions)},
  {"discovery/catalog",EiemResourceCatalogDelta::Encode(catalog)}});
}
struct SnapshotLink {
 std::filesystem::path path,directory;DWORD error=ERROR_SUCCESS;std::optional<EiemModUpdateStore::Snapshot> snapshot;
 static void Reap(const std::filesystem::path& cache){
  static std::mutex mutex;static std::set<std::filesystem::path> visited;std::lock_guard<std::mutex> lock(mutex);if(!visited.insert(cache).second)return;
  std::error_code ec;std::filesystem::directory_iterator it(cache,ec),end;
  for(;!ec&&it!=end;it.increment(ec)){auto candidate=it->path();auto name=candidate.filename().wstring();if(name.rfind(L"pin-",0)!=0||candidate.parent_path()!=cache)continue;
   auto stop=name.find(L'-',4);if(stop==name.npos)continue;auto number=name.substr(4,stop-4);if(number.empty()||number.find_first_not_of(L"0123456789")!=number.npos)continue;
   DWORD pid=0;try{auto value=std::stoull(number);if(!value||value>MAXDWORD)continue;pid=DWORD(value);}catch(const std::exception&){continue;}if(pid==GetCurrentProcessId())continue;
   auto process=OpenProcess(SYNCHRONIZE,FALSE,pid);auto openError=GetLastError();bool dead=false;
   if(process){dead=WaitForSingleObject(process,0)==WAIT_OBJECT_0;CloseHandle(process);}else dead=openError==ERROR_INVALID_PARAMETER;
   if(dead){std::error_code ignored;std::filesystem::remove_all(candidate,ignored);}
  }
 }
 void Cleanup()noexcept{if(!directory.empty()){std::error_code ignored;std::filesystem::remove_all(directory,ignored);}path.clear();directory.clear();snapshot.reset();}
 explicit SnapshotLink(const std::filesystem::path& source,bool deferredOnly=false){
  auto absolute=std::filesystem::absolute(source).lexically_normal();auto parent=absolute.parent_path().parent_path();
  // Opening a child file pins its whole directory against rename on Windows,
  // even with FILE_SHARE_DELETE. Open the same bytes through an outside link.
  // Keep the link outside the entire mods tree so nested packages can move too.
  for(auto at=absolute.parent_path();!at.empty();){auto name=at.filename().wstring();std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
   if(name==L"mods"){parent=at.parent_path();break;}auto next=at.parent_path();if(next==at)break;at=std::move(next);}
  auto cache=parent/L".eiem-compiled-snapshots";std::error_code ec;std::filesystem::create_directories(cache,ec);
  if(ec){error=DWORD(ec.value());return;}
  Reap(cache);
  static std::atomic<uint64_t> serial{0};
  for(size_t attempt=0;attempt<8;++attempt){auto candidate=cache/(L"pin-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(serial.fetch_add(1)));
   if(std::filesystem::create_directory(candidate,ec)){directory=std::move(candidate);break;}if(ec){error=DWORD(ec.value());return;}
  }
  if(directory.empty()){error=ERROR_ALREADY_EXISTS;return;}path=directory/L"compiled.bin";
  if(!CreateHardLinkW(path.c_str(),absolute.c_str(),nullptr)){error=GetLastError();Cleanup();return;}
  bool pinned=false;
  try{EiemModUpdateStore::File metadata(path,false);snapshot=EiemModUpdateStore::Read(metadata);pinned=true;
   if(snapshot->external){std::filesystem::create_directory(directory/L"compiled.blocks");std::set<std::string> linked;
    for(const auto& row:snapshot->blocks){if(deferredOnly&&row.first.rfind("runtime/proof/",0)!=0)continue;const auto& external=row.second.external;if(!linked.insert(external).second)continue;
     if(!CreateHardLinkW((directory/std::filesystem::u8path(external)).c_str(),(absolute.parent_path()/std::filesystem::u8path(external)).c_str(),nullptr)){error=GetLastError();pinned=false;break;}
    }
   }
  }catch(const std::exception&){error=ERROR_INVALID_DATA;pinned=false;}
  if(!pinned)Cleanup();
 }
 SnapshotLink(const SnapshotLink&)=delete;
 void Retire()noexcept{if(!path.empty())DeleteFileW(path.c_str());}
 ~SnapshotLink(){Cleanup();}
};
struct Backing {
 SnapshotLink link;
 EiemModUpdateStore::File file;EiemModUpdateStore::Snapshot snapshot;std::vector<std::string> identities;std::mutex mutex;size_t proofReads=0;
 std::filesystem::path sourceDirectory;bool deferredOnly=false;
 std::map<std::string,std::unique_ptr<EiemModUpdateStore::File>> directBlobs;
 explicit Backing(const std::filesystem::path& path,bool proofOnly=false):link(path,proofOnly),file(link.path.empty()?path:link.path,false),snapshot(link.snapshot?*link.snapshot:EiemModUpdateStore::Read(file)),sourceDirectory(std::filesystem::absolute(path).parent_path()),deferredOnly(proofOnly){
  // Mark the outside name for deletion immediately; the handle retains its
  // bytes through compaction and Windows reclaims the link on process exit.
  link.Retire();
  if(link.path.empty()&&snapshot.external)for(const auto& row:snapshot.blocks){if(deferredOnly&&row.first.rfind("runtime/proof/",0)!=0)continue;const auto& external=row.second.external;
   if(!directBlobs.count(external))directBlobs.emplace(external,std::make_unique<EiemModUpdateStore::File>(path.parent_path()/std::filesystem::u8path(external),false));
  }
 }
 Bytes Get(const std::string& key){std::lock_guard<std::mutex> lock(mutex);if(key.rfind("runtime/proof/",0)==0)++proofReads;
  const auto& block=snapshot.blocks.at(key);
  // Consumer Load owns the publication mutex until all immediate metadata is
  // decoded. Only proof files are accessed again after that mutex is released.
  if(snapshot.external&&deferredOnly&&key.rfind("runtime/proof/",0)!=0){EiemModUpdateStore::File blob(sourceDirectory/std::filesystem::u8path(block.external),false);Require(blob.Size()==block.size,"immediate Mod block size differs");auto bytes=blob.ReadAt(0,block.size);Require(Hash(bytes)==block.digest,"immediate Mod block digest differs");return bytes;}
  auto pinned=directBlobs.find(block.external);if(pinned==directBlobs.end())return EiemModUpdateStore::Get(file,snapshot,key);
  auto& blob=*pinned->second;Require(blob.Size()==block.size,"pinned Mod external block size differs");auto bytes=blob.ReadAt(0,block.size);Require(Hash(bytes)==block.digest,"pinned Mod external block digest differs");return bytes;
 }
};
inline Loaded Load(const std::filesystem::path& path,const std::filesystem::path& mod,const std::filesystem::path& game,bool runtime=true,const std::function<void(const char*,double)>& cost={},bool consumerSnapshot=false){
 auto checkpoint=std::chrono::steady_clock::now();auto report=[&](const char* stage){if(cost){auto now=std::chrono::steady_clock::now();cost(stage,std::chrono::duration<double,std::milli>(now-checkpoint).count());checkpoint=now;}};
 // Authors can run in a child process while Blender holds this mutex. Keep
 // their existing complete snapshot behavior; only DLL consumers opt in.
 std::unique_ptr<EiemModUpdateStore::Lock> publication;if(consumerSnapshot)publication=std::make_unique<EiemModUpdateStore::Lock>(path);report("store-publication-lock");
 auto backing=std::make_shared<Backing>(path,consumerSnapshot);report("store-snapshot-pin");auto common=backing->Get("runtime/common"),ordered=backing->Get("runtime/order");std::vector<std::string> closure;
 Loaded result;result.externalSnapshot=!backing->link.path.empty();result.snapshotLinkError=backing->link.error;
 result.artifact=EiemCompiledSourceRecords::ReadCommon(common,mod,closure);auto& a=result.artifact;auto& p=a.manifest.plan;report("store-common");
 if(backing->snapshot.blocks.count("runtime/resource-bindings")){a.explicitResources=p.resourceAddressed=true;a.resourceBindingBytes=backing->Get("runtime/resource-bindings");}
 if(backing->snapshot.blocks.count("runtime/schema-dictionary"))a.sharedSchemas=EiemCompiledSourceRecords::DecodeSchemas(backing->Get("runtime/schema-dictionary"));report("store-schema");
 EiemOfflinePackage::Reader r{ordered};Require(r.S()=="EFF_COMPILED_SOURCE_ORDER/identity/le64","source store order differs");auto count=r.Count();std::map<std::string,size_t> order;
 for(size_t i=0;i<count;++i){auto identity=r.S();Require(order.emplace(identity,i).second,"source store identity repeated");backing->identities.push_back(identity);}
 Require(r.p==ordered.size()&&count,"source store order suffix/coverage invalid");
 auto base=std::filesystem::weakly_canonical(std::filesystem::absolute(game));report("store-order");for(size_t i=0;i<count;++i){auto identity=backing->identities.at(i);Require(backing->snapshot.blocks.count("runtime/proof/"+identity),"source store proof absent");auto source=backing->Get("runtime/source/"+identity);report("store-source-read");EiemCompiledSourceRecords::ReadSourceRecord(source,a,base,order,i);report("store-source-decode");}
 std::set<size_t> unique;for(const auto& identity:closure){auto at=order.at(identity);Require(unique.insert(at).second,"source store closure repeated");p.closure.push_back(at);}
 std::set<std::string> roots;for(const auto& root:p.roots)Require(unique.count(root.input)&&roots.insert(root.path).second,"source store root outside closure/repeated");Require(!roots.empty()&&a.pristine.size()==p.roots.size(),"source store pristine coverage differs");
 for(const auto& frame:p.frameRecipes){auto bytes=Unpack(backing->Get("runtime/frame/"+frame.resource));Require(Hash(bytes)==frame.hash&&a.preparedFrames.emplace(frame.resource,std::move(bytes)).second,"source store shared frame invalid");}
 for(const auto& row:backing->snapshot.blocks)if(row.first.rfind("runtime/generated/",0)==0){auto name=row.first.substr(strlen("runtime/generated/"));Require(a.explicitResources&&name.rfind("@bounds:",0)==0,"generated resource identity differs");auto bytes=Unpack(backing->Get(row.first));Require(bytes.size()==24&&a.preparedFrames.emplace(name,std::move(bytes)).second,"generated bounds extent/identity differs");}
 for(const auto& row:backing->snapshot.blocks)if(row.first.rfind("runtime/literal/",0)==0){auto name=row.first.substr(strlen("runtime/literal/"));Require(a.explicitResources&&name.rfind("@literal:",0)==0,"pooled metadata identity differs");auto bytes=Unpack(backing->Get(row.first));Require(Hash(bytes)==name.substr(9)&&a.preparedFrames.emplace(name,std::move(bytes)).second,"pooled metadata digest/identity differs");}
 report("store-generated");if(a.explicitResources){
  std::map<std::string,std::string> generated;for(const auto& row:a.preparedFrames)generated.emplace(row.first,Hash(row.second));
  for(auto& view:a.views){view.templateHash=view.startupHash;Writer bound;bound.S("EFF_BOUND_NATIVE_TEMPLATE/verified-source+template+resource-versions");bound.S(view.sourceHash);bound.S(view.templateHash);
   std::set<std::pair<unsigned,std::string>> versions;
   for(const auto& span:view.spans)if(span.kind==EiemResourceRecipe::Author){const auto& resource=*a.manifest.package.resources.at(span.source);Require(span.offset<=resource.bytes.size()&&span.size<=resource.bytes.size()-span.offset,"bound author extent differs");versions.emplace(span.kind,span.source);}
   else if(span.kind==EiemResourceRecipe::ExpandedFrame){Require(a.preparedFrames.count(span.source)&&span.offset<=a.preparedFrames.at(span.source).size()&&span.size<=a.preparedFrames.at(span.source).size()-span.offset,"bound generated extent differs");versions.emplace(span.kind,span.source);}
   for(const auto& version:versions){bound.N(version.first);bound.S(version.second);bound.S(version.first==EiemResourceRecipe::Author?ResourceContentHash(*a.manifest.package.resources.at(version.second)):generated.at(version.second));}
   view.startupHash=Hash(bound.b);
  }
 }
 report("store-versions");a.proofReader=[backing](size_t i){return backing->Get("runtime/proof/"+backing->identities.at(i));};
 Writer identity;for(const auto& row:backing->snapshot.blocks)if(row.first.rfind("runtime/",0)==0){identity.S(row.first);identity.S(row.second.digest);}a.digest=Hash(identity.b);
 a.genericResources=std::any_of(a.manifest.package.resources.begin(),a.manifest.package.resources.end(),[](const auto& row){return Begins(row.first,"resources/");});
 result.decisions=EiemConsumerUpdateRecords::Decode(backing->Get("discovery/decisions"));result.catalog=EiemResourceCatalogDelta::Decode(backing->Get("discovery/catalog"));result.generation=backing->snapshot.generation;
 if(backing->snapshot.blocks.count("discovery/source-selection")){result.selection=ReadSourceSelection(backing->Get("discovery/source-selection"),game);VerifySourceSelection(a,*result.selection);}
 report("store-discovery");if(!runtime){for(size_t i=0;i<count;++i)HydrateInput(a,i);for(auto& root:p.roots)ExpandPreparedAssembly(root);for(auto& root:a.pristine)ExpandPreparedAssembly(root);}report("store-proof-hydrate");return result;
}
} // namespace EiemCompiledModStore
