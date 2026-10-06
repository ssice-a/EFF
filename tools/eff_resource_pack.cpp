#include "../src/eiem_offline_resource_package.h"
#include "../src/eiem_offline_cache.h"
#include "../src/eiem_static_resource_sources.h"
#include <iostream>
#include <chrono>
#include <thread>
using namespace EiemOfflinePackage;
struct Options {std::filesystem::path mod,baseline,cache;bool watch=false;DWORD parent=0;bool set=false;};
std::filesystem::path ModDirectory(const Options& o){return o.set?o.mod:o.mod.parent_path();}
std::vector<Package> Authors(const Options& o){return o.set?EiemComposedPackage::LoadSet(o.mod):std::vector<Package>{Load(o.mod)};}
struct Handle {
 HANDLE value=nullptr;bool owned=false;
 explicit Handle(HANDLE v):value(v){}~Handle(){if(owned)ReleaseMutex(value);if(value)CloseHandle(value);}
 Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
bool ParentAlive(const Handle& parent){return !parent.value||WaitForSingleObject(parent.value,0)==WAIT_TIMEOUT;}
void StartupTable(const Options& o,const Package& package,const std::filesystem::path& folder,const std::filesystem::path& destination){
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
bool LockBuild(Handle& single,const Handle& parent){
 for(;;){auto wait=WaitForSingleObject(single.value,250);
  if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){single.owned=true;return true;}
  Require(wait==WAIT_TIMEOUT,"offline build mutex wait failed");if(!ParentAlive(parent))return false;
  // A first discovery can outlast a normal rebuild. Queue behind the writer
  // while the requesting process lives, without aborting valid compilation.
 }
}
std::string SourceStamp(const Options& o){std::map<std::string,InputFileStamp> files;auto roots=o.set?EiemComposedPackage::Roots(o.mod):std::vector<std::filesystem::path>{o.mod};std::string text;
 for(const auto& root:roots){text+=root.filename().u8string()+"\n";for(const auto& entry:std::filesystem::recursive_directory_iterator(root))if(entry.is_regular_file()){
 auto name=entry.path().filename().u8string();if(name=="state.ini"||name.rfind("state.ini.tmp-",0)==0||AuthorAuxiliary(entry.path()))continue;
 auto relative=entry.path().lexically_relative(o.mod).u8string();files.emplace(relative,FileStamp(entry.path()));
 }}for(const auto& row:files)text+=row.first+":"+std::to_string(row.second.bytes)+":"+std::to_string(row.second.written)+"\n";auto hash=Sha((const unsigned char*)text.data(),text.size());return Hex(hash.data(),hash.size());}
Pointer Compile(const Options& o){
 auto started=std::chrono::steady_clock::now();auto ms=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();};
 auto stamp=SourceStamp(o);auto authors=Authors(o);auto package=EiemComposedPackage::Compose(authors,o.set);
 if(o.set)EiemOfflineCache::ClaimSet(o.cache,o.mod);else EiemOfflineCache::Claim(o.cache,package);
 bool hasStatic=std::any_of(authors.begin(),authors.end(),[](const auto& p){return std::filesystem::exists(p.root/"source-inputs.bin");});
 if(hasStatic){auto selected=EiemStaticSources::Merge(authors,o.baseline.parent_path().parent_path());EiemStaticSources::Ensure(selected,o.baseline,o.cache/"sources",package);}
 if(!std::filesystem::exists(o.baseline/"baseline.tsv")&&authors.empty()){std::cout<<"OFFLINE-EMPTY noNativeRoots=1"<<std::endl;return {};}
 Require(std::filesystem::exists(o.baseline/"baseline.tsv"),"static source inputs missing; prepare each Mod's source-inputs.bin with the authoring tool");
 auto author=PackageDigest(package);auto baseline=Hash(Read(o.baseline/"baseline.tsv"));
 // Source hashes/TypeTrees and current loader ranges prove compatibility.
 std::vector<std::string> roots;auto sources=ReadBaseline(o.baseline,roots);
 bool rulesOnly=hasStatic||std::any_of(sources.begin(),sources.end(),[](const auto& s){return !s.physical.empty();});
 auto nonce=std::string(EiemPackageProtocol::manifestStructure)+"/authored-material-clones/"+(rulesOnly?"/runtime-rules/xpress-huffman/":"/full-inputs/")+(EiemPackageProtocol::rootAssembly?"/root-addresses/":"/root-markers/")+(o.set?"authors/":"author/")+baseline;
 if(rulesOnly)nonce+="/namespace-offset-patches/le64/sha256/member-rebuilds/source-author-spans/le64/sha256";
 std::string identity=author+"\n"+InputContractDigest(package)+"\n"+nonce;for(const auto& source:sources)identity+="\n"+source.file+"\n"+source.hash;
 auto key=Hash(Bytes(identity.begin(),identity.end()));auto destination=o.cache/"generations"/key;
 if(std::filesystem::exists(destination/"manifest.bin")){
  auto bytes=Read(destination/"manifest.bin");Pointer pointer{key,Hash(bytes)};auto existing=Decode(bytes,destination,ModDirectory(o));Require(existing.baseline==baseline&&existing.plan.author==author&&existing.composed==o.set,"offline existing generation differs");
  if(!existing.plan.rulesOnly)for(const auto& input:existing.plan.inputs)Require(Hash(Read(input.path))==input.hash,"offline cached input SHA differs");
  else for(const auto& source:existing.plan.originals)Require(Hash(ReadSource(o.baseline,source))==source.hash,"rule original source changed");
  Require(SourceStamp(o)==stamp,"author changed during cache validation");Publish(o.cache,pointer);std::cout<<"OFFLINE-REUSED key="<<key<<" elapsedMs="<<ms()<<std::endl;return pointer;
 }
 // Build into a unique directory. Never remove a previous or partially built
 // generation, and never expose this directory through current until verified.
 auto temporary=o.cache/("building-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 std::map<std::string,double> costs;std::mutex costsMutex;std::function<void(const char*,double)> cost;
 if(GetEnvironmentVariableW(L"EFF_OFFLINE_PROFILE",nullptr,0))cost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(costsMutex);costs[name]+=elapsed;};
 auto plan=Build(package,o.baseline,temporary,nonce,{},[&](const char* stage,size_t n,size_t total){if(!n||n==total)std::cout<<"OFFLINE-PROGRESS phase="<<stage<<" completed="<<n<<'/'<<total<<" elapsedMs="<<ms()<<std::endl;},cost,rulesOnly?std::filesystem::path{}:o.cache/"geometry",rulesOnly);
 std::cout<<"OFFLINE-CACHE appearanceHits="<<plan.appearanceHits<<" appearanceMisses="<<plan.appearanceMisses<<" namespaceHits="<<plan.namespaceHits<<" namespaceMisses="<<plan.namespaceMisses<<" geometryInputHits="<<plan.geometryInputHits<<" geometryInputMisses="<<plan.geometryInputMisses<<std::endl;
 if(!rulesOnly)Measure(cost,"startup-table",[&]{StartupTable(o,package,temporary,destination);return 0;});
 Require(plan.key==key,"offline deterministic key differs");
 Manifest manifest{std::move(plan),std::move(package),baseline};manifest.composed=o.set;if(o.set)manifest.packages=std::move(authors);auto bytes=Measure(cost,"descriptor-encode",[&]{return Encode(manifest);});
 auto checked=Measure(cost,"descriptor-roundtrip",[&]{return Decode(bytes,temporary,ModDirectory(o));});Require(Measure(cost,"descriptor-reencode",[&]{return Encode(checked);})==bytes,"offline compiled descriptor roundtrip differs");
 Measure(cost,"author-recheck",[&]{Require(SourceStamp(o)==stamp&&PackageDigest(EiemComposedPackage::Compose(Authors(o),o.set))==author,"author changed during offline compilation; previous publication retained");return 0;});
 Measure(cost,"descriptor-write",[&]{Save(temporary/"manifest.bin",bytes);return 0;});std::filesystem::create_directories(destination.parent_path());
 Require(!std::filesystem::exists(destination),"offline destination exists without a complete manifest");std::filesystem::rename(temporary,destination);Pointer pointer{key,Hash(bytes)};Publish(o.cache,pointer);
 for(const auto& row:costs)std::cout<<"OFFLINE-COST phase="<<row.first<<" workerMs="<<row.second<<std::endl;
 std::cout<<"OFFLINE-PUBLISHED key="<<key<<" inputs="<<manifest.plan.inputs.size()<<" elapsedMs="<<ms()<<" UnityCalls=0"<<std::endl;return pointer;
}
std::string JsonString(const std::string& value){std::string result="\"";for(unsigned char ch:value){if(ch=='\"'||ch=='\\'){result+='\\';result+=char(ch);}else if(ch<32){const char* hex="0123456789ABCDEF";result+="\\u00";result+=hex[ch>>4];result+=hex[ch&15];}else result+=char(ch);}return result+"\"";}
void SourceRequest(const Package& p){
 std::map<Identity,std::string> sources;for(const auto& t:p.targetIndex)sources.emplace(t.first,"Mesh");for(const auto& m:p.materials)if(!m.second.source.path.empty())sources.emplace(m.second.source,"Material");for(const auto& t:p.textures)if(!t.second.source.path.empty())sources.emplace(t.second.source,"Texture2D");
 std::cout<<"{\"sources\":[";bool first=true;for(const auto& row:sources){if(!first)std::cout<<',';first=false;std::cout<<"{\"kind\":"<<JsonString(row.second)<<",\"path\":"<<JsonString(row.first.path)<<",\"asset\":"<<JsonString(row.first.asset)<<'}';}std::cout<<"]}"<<std::endl;
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==3&&std::wstring(argv[1])==L"--source-request"){SourceRequest(Load(argv[2]));return 0;}
 if(argc==2&&std::wstring(argv[1])==L"--protocol") {
  std::cout<<"{\"format\":\"EFF_OFFLINE_PACKAGE_PROTOCOL\",\"version\":1,\"abi\":"<<EiemPackageProtocol::abi
           <<",\"compiler\":\""<<compiler<<"\",\"authorFormat\":"<<EiemPackageProtocol::authorFormat
           <<",\"resourceVersion\":"<<EiemPackageProtocol::resourceVersion<<",\"modSetVersion\":1,\"staticSourceInputs\":\""<<EiemPackageProtocol::staticSourceInputs<<"\",\"arguments\":[\"author\",\"baseline\",\"cache\"],\"structures\":{"
           <<"\"manifest\":\""<<EiemPackageProtocol::manifestStructure<<"\",\"author\":\""<<EiemPackageProtocol::authorStructure
           <<"\",\"resource\":\""<<EiemPackageProtocol::resourceStructure<<"\",\"current\":\""<<EiemPackageProtocol::currentStructure
           <<"\",\"modSet\":\""<<EiemPackageProtocol::setStructure
           <<"\",\"runtimeRules\":\""<<EiemPackageProtocol::ruleStructure
           <<"\",\"manifestEncoding\":\""<<EiemPackageProtocol::ruleEncoding<<"\",\"manifestInputs\":[\""<<EiemPackageProtocol::ruleEncoding
           <<"\",\"self-described-roots/le64\",\"legacy-root-markers/le64\",\"legacy-root-addresses/le64\"]}}"<<std::endl;
  return 0;
 }
 bool set=argc>1&&std::wstring(argv[1])==L"--mods";int offset=set?1:0;int count=argc-offset;
 Require(count>=4,"eff_resource_pack [--mods] <Mod or mods directory> <baseline> <cache> [--watch | --watch-parent PID]");Options o{argv[1+offset],argv[2+offset],argv[3+offset]};o.set=set;
 for(int i=4+offset;i<argc;++i){std::wstring option=argv[i];if(option==L"--watch")o.watch=true;
  else if(option==L"--watch-parent"){Require(i+1<argc,"offline parent PID absent");o.watch=true;size_t used=0;auto pid=std::stoul(argv[++i],&used);Require(used==wcslen(argv[i])&&pid&&pid<=MAXDWORD,"offline parent PID");o.parent=DWORD(pid);}
  else throw std::runtime_error("unknown offline option");}
 // A removed mods directory is an empty set, including in a running watcher.
 // Preserve its absolute identity without recreating the user's directory.
 o.mod=o.set?std::filesystem::weakly_canonical(std::filesystem::absolute(o.mod)):std::filesystem::canonical(o.mod);o.baseline=std::filesystem::weakly_canonical(std::filesystem::absolute(o.baseline));o.cache=EiemOfflineCache::CanonicalCache(o.cache);
 auto label=InputPath(o.mod.u8string()+"|"+o.cache.lexically_normal().u8string());auto hash=Hash(Bytes(label.begin(),label.end()));auto suffix=std::wstring(hash.begin(),hash.end());
 Handle parent(o.parent?OpenProcess(SYNCHRONIZE,FALSE,o.parent):nullptr);Require(!o.parent||parent.value,"offline parent process unavailable");
 Handle watcher(o.watch?CreateMutexW(nullptr,TRUE,(L"Local\\EFFOfflineWatch-"+suffix).c_str()):nullptr);
 if(o.watch){Require(watcher.value,"offline watcher mutex failed");if(GetLastError()==ERROR_ALREADY_EXISTS){std::cout<<"OFFLINE-WATCH-ALREADY-ACTIVE"<<std::endl;return 0;}watcher.owned=true;}
 // Only a build holds this mutex. Blender one-shot exports can queue behind a
 // watcher, then verify/reuse the exact new author generation under the lock.
 auto cacheIdentity=EiemOfflineCache::PathIdentity(o.cache);auto cacheSuffix=std::wstring(cacheIdentity.begin(),cacheIdentity.end());
 Handle single(CreateMutexW(nullptr,FALSE,(L"Local\\EFFOfflinePack-"+cacheSuffix).c_str()));Require(single.value,"offline build mutex failed");
 std::string attempted;do{if(!ParentAlive(parent))break;try{auto stamp=SourceStamp(o);if(stamp!=attempted){if(o.watch){std::this_thread::sleep_for(std::chrono::milliseconds(500));if(SourceStamp(o)!=stamp)continue;}
  if(!LockBuild(single,parent))break;try{attempted=SourceStamp(o);Compile(o);}catch(...){ReleaseMutex(single.value);single.owned=false;throw;}
  ReleaseMutex(single.value);single.owned=false;
 }}catch(const std::exception& e){std::cerr<<"OFFLINE-REJECT reason="<<e.what()<<" previousGenerationRetained=1"<<std::endl;if(!o.watch)return 1;}
 if(o.watch)std::this_thread::sleep_for(std::chrono::seconds(1));}while(o.watch);return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
