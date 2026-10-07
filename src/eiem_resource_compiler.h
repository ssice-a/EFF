#pragma once
#include "eiem_offline_resource_package.h"
#include "eiem_offline_cache.h"
#include "eiem_static_resource_sources.h"
#include <chrono>

// Native data processing only. Call from a disk worker, never a Unity frame.
namespace EiemResourceCompiler {
using namespace EiemOfflinePackage;
struct Options {std::filesystem::path mod,baseline,cache;bool watch=false;DWORD parent=0;bool set=false;std::function<void(const std::string&)> report;};
inline std::filesystem::path ModDirectory(const Options& o){return o.set?o.mod:o.mod.parent_path();}
inline std::vector<Package> Authors(const Options& o){return o.set?EiemComposedPackage::LoadSet(o.mod):std::vector<Package>{Load(o.mod)};}
struct Handle {
 HANDLE value=nullptr;bool owned=false;
 explicit Handle(HANDLE v):value(v){}~Handle(){if(owned)ReleaseMutex(value);if(value)CloseHandle(value);}
 Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
inline bool ParentAlive(const Handle& parent){return !parent.value||WaitForSingleObject(parent.value,0)==WAIT_TIMEOUT;}
inline void StartupTable(const Options& o,const Package& package,const std::filesystem::path& folder,const std::filesystem::path& destination){
 auto ranges=o.baseline/"source-ranges.tsv";if(!std::filesystem::exists(ranges))return;
 auto bytes=Read(ranges);std::istringstream lines(Text(bytes.data(),bytes.size()));std::string row;
 Require(bool(std::getline(lines,row))&&TableHeader(Trim(row),"EFF_RESOURCE_SOURCE_RANGES"),"startup source range version");
 std::string table="EFF_LOADING_INPUT_SUBSTITUTIONS\t3\t"+package.id+"\t"+InputContractDigest(package)+"\n";
 while(std::getline(lines,row)){row=Trim(row);auto r=TabFields(row);Require(r.size()==5&&std::filesystem::u8path(r[0]).filename()==std::filesystem::u8path(r[0])&&DigestText(r[4]),"startup source range row");
  auto raw=Read(folder/"startup"/std::filesystem::u8path(r[0]));auto hash=Hash(raw);if(hash==r[4])continue;
  table+=r[1]+"\t"+r[2]+"\t"+r[3]+"\t"+r[4]+"\t"+(destination/"startup"/std::filesystem::u8path(r[0])).u8string()+"\t"+std::to_string(raw.size())+"\t"+hash+"\n";
 }
 Save(folder/"loading-inputs.tsv",Bytes(table.begin(),table.end()));
}
inline bool LockBuild(Handle& single,const Handle& parent){
 for(;;){auto wait=WaitForSingleObject(single.value,250);
  if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){single.owned=true;return true;}
  Require(wait==WAIT_TIMEOUT,"offline build mutex wait failed");if(!ParentAlive(parent))return false;
  // A first discovery can outlast a normal rebuild. Queue behind the writer
  // while the requesting process lives, without aborting valid compilation.
 }
}
inline std::string SourceStamp(const Options& o){std::map<std::string,InputFileStamp> files;auto roots=o.set?EiemComposedPackage::Roots(o.mod):std::vector<std::filesystem::path>{o.mod};std::string text;
 for(const auto& root:roots){text+=root.filename().u8string()+"\n";for(const auto& entry:std::filesystem::recursive_directory_iterator(root))if(entry.is_regular_file()){
 auto name=entry.path().filename().u8string();if(name=="state.ini"||name.rfind("state.ini.tmp-",0)==0||AuthorAuxiliary(entry.path()))continue;
 auto relative=entry.path().lexically_relative(o.mod).u8string();files.emplace(relative,FileStamp(entry.path()));
 }}for(const auto& row:files)text+=row.first+":"+std::to_string(row.second.bytes)+":"+std::to_string(row.second.written)+"\n";auto hash=Sha((const unsigned char*)text.data(),text.size());return Hex(hash.data(),hash.size());}
inline Pointer Compile(const Options& o){
 auto started=std::chrono::steady_clock::now();auto ms=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();};
 auto stamp=SourceStamp(o);auto authors=Authors(o);auto package=EiemComposedPackage::Compose(authors,o.set);
 if(o.set)EiemOfflineCache::ClaimSet(o.cache,o.mod);else EiemOfflineCache::Claim(o.cache,package);
 bool hasStatic=std::any_of(authors.begin(),authors.end(),[](const auto& p){return std::filesystem::exists(p.root/"source-inputs.bin");});
 if(hasStatic){auto selected=EiemStaticSources::Merge(authors,o.baseline.parent_path().parent_path());EiemStaticSources::Ensure(selected,o.baseline,o.cache/"sources",package);}
 if(!std::filesystem::exists(o.baseline/"baseline.tsv")&&authors.empty()){{std::ostringstream line;line<<"OFFLINE-EMPTY noNativeRoots=1";if(o.report)o.report(line.str());}return {};}
 Require(std::filesystem::exists(o.baseline/"baseline.tsv"),"static source inputs missing; prepare each Mod's source-inputs.bin with the authoring tool");
 auto author=PackageDigest(package);auto baseline=Hash(Read(o.baseline/"baseline.tsv"));
 // Source hashes/TypeTrees and current loader ranges prove compatibility.
 std::vector<std::string> roots;auto sources=ReadBaseline(o.baseline,roots);
 bool rulesOnly=hasStatic||std::any_of(sources.begin(),sources.end(),[](const auto& s){return !s.physical.empty();});
 auto nonce=std::string(EiemPackageProtocol::manifestStructure)+"/mesh-filter-renderer/authored-material-clones/"+(rulesOnly?"/runtime-rules/xpress-huffman/":"/full-inputs/")+(EiemPackageProtocol::rootAssembly?"/root-addresses/":"/root-markers/")+(o.set?"authors/":"author/")+baseline;
 if(rulesOnly)nonce+="/namespace-offset-patches/le64/sha256/member-rebuilds/source-author-spans/le64/sha256";
 std::string identity=author+"\n"+InputContractDigest(package)+"\n"+nonce;for(const auto& source:sources)identity+="\n"+source.file+"\n"+source.hash;
 auto key=Hash(Bytes(identity.begin(),identity.end()));auto destination=o.cache/"generations"/key;
 if(std::filesystem::exists(destination/"manifest.bin")){
  auto bytes=Read(destination/"manifest.bin");Pointer pointer{key,Hash(bytes)};auto existing=Decode(bytes,destination,ModDirectory(o));Require(existing.baseline==baseline&&existing.plan.author==author&&existing.composed==o.set,"offline existing generation differs");
  if(!existing.plan.rulesOnly)for(const auto& input:existing.plan.inputs)Require(Hash(Read(input.path))==input.hash,"offline cached input SHA differs");
  else for(const auto& source:existing.plan.originals)Require(Hash(ReadSource(o.baseline,source))==source.hash,"rule original source changed");
  Require(SourceStamp(o)==stamp,"author changed during cache validation");Publish(o.cache,pointer);{std::ostringstream line;line<<"OFFLINE-REUSED key="<<key<<" elapsedMs="<<ms();if(o.report)o.report(line.str());}return pointer;
 }
 // Build into a unique directory. Never remove a previous or partially built
 // generation, and never expose this directory through current until verified.
 auto temporary=o.cache/("building-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 std::map<std::string,double> costs;std::mutex costsMutex;std::function<void(const char*,double)> cost;
 if(GetEnvironmentVariableW(L"EFF_OFFLINE_PROFILE",nullptr,0))cost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(costsMutex);costs[name]+=elapsed;};
 auto plan=Build(package,o.baseline,temporary,nonce,{},[&](const char* stage,size_t n,size_t total){if(!n||n==total){std::ostringstream line;line<<"OFFLINE-PROGRESS phase="<<stage<<" completed="<<n<<'/'<<total<<" elapsedMs="<<ms();if(o.report)o.report(line.str());}},cost,rulesOnly?std::filesystem::path{}:o.cache/"geometry",rulesOnly);
 {std::ostringstream line;line<<"OFFLINE-CACHE appearanceHits="<<plan.appearanceHits<<" appearanceMisses="<<plan.appearanceMisses<<" namespaceHits="<<plan.namespaceHits<<" namespaceMisses="<<plan.namespaceMisses<<" geometryInputHits="<<plan.geometryInputHits<<" geometryInputMisses="<<plan.geometryInputMisses;if(o.report)o.report(line.str());}
 if(!rulesOnly)Measure(cost,"startup-table",[&]{StartupTable(o,package,temporary,destination);return 0;});
 Require(plan.key==key,"offline deterministic key differs");
 Manifest manifest{std::move(plan),std::move(package),baseline};manifest.composed=o.set;if(o.set)manifest.packages=std::move(authors);auto bytes=Measure(cost,"descriptor-encode",[&]{return Encode(manifest);});
 auto checked=Measure(cost,"descriptor-roundtrip",[&]{return Decode(bytes,temporary,ModDirectory(o));});Require(Measure(cost,"descriptor-reencode",[&]{return Encode(checked);})==bytes,"offline compiled descriptor roundtrip differs");
 Measure(cost,"author-recheck",[&]{Require(SourceStamp(o)==stamp&&PackageDigest(EiemComposedPackage::Compose(Authors(o),o.set))==author,"author changed during offline compilation; previous publication retained");return 0;});
 Measure(cost,"descriptor-write",[&]{Save(temporary/"manifest.bin",bytes);return 0;});std::filesystem::create_directories(destination.parent_path());
 Require(!std::filesystem::exists(destination),"offline destination exists without a complete manifest");std::filesystem::rename(temporary,destination);Pointer pointer{key,Hash(bytes)};Publish(o.cache,pointer);
 for(const auto& row:costs){std::ostringstream line;line<<"OFFLINE-COST phase="<<row.first<<" workerMs="<<row.second;if(o.report)o.report(line.str());}
 {std::ostringstream line;line<<"OFFLINE-PUBLISHED key="<<key<<" inputs="<<manifest.plan.inputs.size()<<" elapsedMs="<<ms()<<" UnityCalls=0";if(o.report)o.report(line.str());}return pointer;
}

inline Pointer CompileLocked(const Options& o,const std::function<bool()>& alive={}) {
 auto identity=EiemOfflineCache::PathIdentity(o.cache);
 auto suffix=std::wstring(identity.begin(),identity.end());
 Handle single(CreateMutexW(nullptr,FALSE,(L"Local\\EFFOfflinePack-"+suffix).c_str()));
 Require(single.value,"resource build mutex failed");
 for(;;){
  Require(!alive||alive(),"resource build cancelled during shutdown");
  auto wait=WaitForSingleObject(single.value,250);
  if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){single.owned=true;break;}
  Require(wait==WAIT_TIMEOUT,"resource build mutex wait failed");
 }
 Require(!alive||alive(),"resource build cancelled during shutdown");
 return Compile(o);
}
}
