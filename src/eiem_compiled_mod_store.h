#pragma once
#include "eiem_compiled_source_records.h"
#include "eiem_mod_update_store.h"
#include "eiem_consumer_update_records.h"
#include "eiem_resource_catalog_delta.h"
#include "eiem_static_resource_sources.h"

// One file with independently compiled source blocks. Incremental writes never
// encode the previous whole packet or recompress shared author frames.
// Still under local validation; production reader migration is separate.
namespace EiemCompiledModStore {
using namespace EiemCompiledMod;
using Changes=EiemModUpdateStore::Changes;
struct WriteStats {size_t sourcesEncoded=0,proofsEncoded=0,framesEncoded=0,bytesPrepared=0;};
inline bool IsStore(const std::filesystem::path& path){
 std::ifstream in(path,std::ios::binary);Require(bool(in),"compiled export source cannot be opened");char prefix[16]{};in.read(prefix,16);return in.gcount()==16&&std::equal(prefix,prefix+16,EiemModUpdateStore::magic);
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
 auto owners=EiemCompiledSourceRecords::MemberInputs(a);const auto& p=a.manifest.plan;std::set<std::string> found;
 for(size_t i=0;i<p.inputs.size();++i){const auto& identity=p.originals.at(i).file;if(affected&&!affected->count(identity))continue;found.insert(identity);
  auto directory=EiemCompiledSourceRecords::Directory(a,i,owners);records["runtime/source/"+identity]=EiemCompiledSourceRecords::Source(a,i,game,directory);
  records["runtime/proof/"+identity]=EiemCompiledSourceRecords::Proof(a,i,directory);if(stats){++stats->sourcesEncoded;++stats->proofsEncoded;}}
 if(affected)Require(found==*affected,"affected source absent from compiled artifact");
 if(!affected)for(const auto& frame:p.frameRecipes){const auto& bytes=a.preparedFrames.at(frame.resource);Require(Hash(bytes)==frame.hash,"shared frame digest differs");records["runtime/frame/"+frame.resource]=Pack(bytes);if(stats)++stats->framesEncoded;}
 if(stats)for(const auto& row:records)if(row.second)stats->bytesPrepared+=row.second->size();return records;
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
 return EiemModUpdateStore::Commit(path,generation,records);
}
struct Loaded {Artifact artifact;EiemConsumerUpdateRecords::Decisions decisions;EiemResourceCatalogDelta::Catalog catalog;uint64_t generation=0;std::optional<EiemStaticSources::Selection> selection;};
inline EiemModUpdateStore::Snapshot SaveAffected(const std::filesystem::path& path,uint64_t generation,const Artifact& a,const std::filesystem::path& game,
 const std::set<std::string>& affected,const std::set<std::string>& retired,const EiemConsumerUpdateRecords::Decisions& decisions,const EiemResourceCatalogDelta::Catalog& catalog,WriteStats* stats=nullptr,const EiemStaticSources::Selection* selection=nullptr){
 auto snapshot=EiemModUpdateStore::Read(path);Require(snapshot.generation==generation,"compiled source store changed during adaptation");
 auto seal=AuthorSeal(a);auto prior=snapshot.blocks.find("runtime/author-seal");Require(prior!=snapshot.blocks.end()&&prior->second.digest==Hash(seal),"source delta changes author snapshot; publish author resources first");
 auto records=RuntimeRecords(a,game,&affected,stats);std::set<std::string> current;
 for(const auto& source:a.manifest.plan.originals){current.insert(source.file);Require(affected.count(source.file)||(snapshot.blocks.count("runtime/source/"+source.file)&&snapshot.blocks.count("runtime/proof/"+source.file)),"new source omitted from affected records");}
 for(const auto& key:retired){Require(!current.count(key)&&!affected.count(key),"retired source remains active");records["runtime/source/"+key]=std::nullopt;records["runtime/proof/"+key]=std::nullopt;}
 for(const auto& row:snapshot.blocks)if(row.first.rfind("runtime/source/",0)==0){auto key=row.first.substr(strlen("runtime/source/"));Require(current.count(key)||retired.count(key),"removed source omitted from retirement transaction");}
 for(const auto& frame:a.manifest.plan.frameRecipes)Require(snapshot.blocks.count("runtime/frame/"+frame.resource),"source delta cannot introduce author frames; publish author snapshot first");
 if(selection){VerifySourceSelection(a,*selection);records["discovery/source-selection"]=SourceSelection(*selection,game);}
 else if(snapshot.blocks.count("discovery/source-selection")){EiemModUpdateStore::File file(path,false);VerifySourceSelection(a,ReadSourceSelection(EiemModUpdateStore::Get(file,snapshot,"discovery/source-selection"),game));}
 Discovery(records,decisions,catalog);return EiemModUpdateStore::Commit(path,generation,records);
}
inline EiemModUpdateStore::Snapshot SaveDiscovery(const std::filesystem::path& path,uint64_t generation,
 const EiemConsumerUpdateRecords::Decisions& decisions,const EiemResourceCatalogDelta::Catalog& catalog){
 // Negative conclusions and location-only changes do not encode any native
 // object/proof or rewrite the runtime index.
 return EiemModUpdateStore::Commit(path,generation,{{"discovery/decisions",EiemConsumerUpdateRecords::Encode(decisions)},
  {"discovery/catalog",EiemResourceCatalogDelta::Encode(catalog)}});
}
struct Backing {
 EiemModUpdateStore::File file;EiemModUpdateStore::Snapshot snapshot;std::vector<std::string> identities;std::mutex mutex;size_t proofReads=0;
 explicit Backing(const std::filesystem::path& path):file(path,false),snapshot(EiemModUpdateStore::Read(file)){}
 Bytes Get(const std::string& key){std::lock_guard<std::mutex> lock(mutex);if(key.rfind("runtime/proof/",0)==0)++proofReads;return EiemModUpdateStore::Get(file,snapshot,key);}
};
inline Loaded Load(const std::filesystem::path& path,const std::filesystem::path& mod,const std::filesystem::path& game,bool runtime=true){
 auto backing=std::make_shared<Backing>(path);auto common=backing->Get("runtime/common"),ordered=backing->Get("runtime/order");std::vector<std::string> closure;
 Loaded result;result.artifact=EiemCompiledSourceRecords::ReadCommon(common,mod,closure);auto& a=result.artifact;auto& p=a.manifest.plan;
 EiemOfflinePackage::Reader r{ordered};Require(r.S()=="EFF_COMPILED_SOURCE_ORDER/identity/le64","source store order differs");auto count=r.Count();std::map<std::string,size_t> order;
 for(size_t i=0;i<count;++i){auto identity=r.S();Require(order.emplace(identity,i).second,"source store identity repeated");backing->identities.push_back(identity);}
 Require(r.p==ordered.size()&&count,"source store order suffix/coverage invalid");
 auto base=std::filesystem::weakly_canonical(std::filesystem::absolute(game));for(size_t i=0;i<count;++i){auto identity=backing->identities.at(i);Require(backing->snapshot.blocks.count("runtime/proof/"+identity),"source store proof absent");EiemCompiledSourceRecords::ReadSourceRecord(backing->Get("runtime/source/"+identity),a,base,order,i);}
 std::set<size_t> unique;for(const auto& identity:closure){auto at=order.at(identity);Require(unique.insert(at).second,"source store closure repeated");p.closure.push_back(at);}
 std::set<std::string> roots;for(const auto& root:p.roots)Require(unique.count(root.input)&&roots.insert(root.path).second,"source store root outside closure/repeated");Require(!roots.empty()&&a.pristine.size()==p.roots.size(),"source store pristine coverage differs");
 for(const auto& frame:p.frameRecipes){auto bytes=Unpack(backing->Get("runtime/frame/"+frame.resource));Require(Hash(bytes)==frame.hash&&a.preparedFrames.emplace(frame.resource,std::move(bytes)).second,"source store shared frame invalid");}
 a.proofReader=[backing](size_t i){return backing->Get("runtime/proof/"+backing->identities.at(i));};
 Writer identity;for(const auto& row:backing->snapshot.blocks)if(row.first.rfind("runtime/",0)==0){identity.S(row.first);identity.S(row.second.digest);}a.digest=Hash(identity.b);
 a.genericResources=std::any_of(a.manifest.package.resources.begin(),a.manifest.package.resources.end(),[](const auto& row){return Begins(row.first,"resources/");});
 result.decisions=EiemConsumerUpdateRecords::Decode(backing->Get("discovery/decisions"));result.catalog=EiemResourceCatalogDelta::Decode(backing->Get("discovery/catalog"));result.generation=backing->snapshot.generation;
 if(backing->snapshot.blocks.count("discovery/source-selection")){result.selection=ReadSourceSelection(backing->Get("discovery/source-selection"),game);VerifySourceSelection(a,*result.selection);}
 if(!runtime){for(size_t i=0;i<count;++i)HydrateInput(a,i);for(auto& root:p.roots)ExpandPreparedAssembly(root);for(auto& root:a.pristine)ExpandPreparedAssembly(root);}return result;
}
} // namespace EiemCompiledModStore
