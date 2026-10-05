#include "../src/eiem_offline_resource_package.h"
#include <iostream>
#include <chrono>
#include <thread>
using namespace EiemOfflinePackage;
struct Options {std::filesystem::path mod,baseline,cache,assembly;bool watch=false;DWORD parent=0;};
struct Handle {
 HANDLE value=nullptr;bool owned=false;
 explicit Handle(HANDLE v):value(v){}~Handle(){if(owned)ReleaseMutex(value);if(value)CloseHandle(value);}
 Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
bool ParentAlive(const Handle& parent){return !parent.value||WaitForSingleObject(parent.value,0)==WAIT_TIMEOUT;}
void StartupTable(const Options& o,const Package& package,const std::filesystem::path& folder,const std::filesystem::path& destination){
 auto ranges=o.baseline/"source-ranges.tsv";if(!std::filesystem::exists(ranges))return;
 auto bytes=Read(ranges,256*1024);std::istringstream lines(Text(bytes.data(),bytes.size()));std::string row;
 Require(bool(std::getline(lines,row))&&Trim(row)=="EFF_RESOURCE_SOURCE_RANGES\t1","startup source range version");
 std::string table="EFF_LOADING_INPUT_SUBSTITUTIONS\t3\t"+package.id+"\t"+InputContractDigest(package)+"\n";
 while(std::getline(lines,row)){row=Trim(row);auto r=TabFields(row);Require(r.size()==5&&std::filesystem::u8path(r[0]).filename()==std::filesystem::u8path(r[0])&&DigestText(r[4]),"startup source range row");
  auto raw=Read(folder/"startup"/std::filesystem::u8path(r[0]),inputLimit);auto hash=Hash(raw);if(hash==r[4])continue;
  table+=r[1]+"\t"+r[2]+"\t"+r[3]+"\t"+r[4]+"\t"+(destination/"startup"/std::filesystem::u8path(r[0])).u8string()+"\t"+std::to_string(raw.size())+"\t"+hash+"\n";
 }
 Save(folder/"loading-inputs.tsv",Bytes(table.begin(),table.end()));
}
bool LockBuild(Handle& single,const Handle& parent){
 auto started=GetTickCount64();for(;;){auto wait=WaitForSingleObject(single.value,250);
  if(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED){single.owned=true;return true;}
  Require(wait==WAIT_TIMEOUT,"offline build mutex wait failed");if(!ParentAlive(parent))return false;
  Require(GetTickCount64()-started<120000,"offline build mutex timed out");
 }
}
std::string SourceStamp(const Options& o){std::map<std::string,InputFileStamp> files;for(const auto& entry:std::filesystem::recursive_directory_iterator(o.mod))if(entry.is_regular_file()){
 auto name=entry.path().filename().u8string();if(name=="state.ini"||name.rfind("state.ini.tmp-",0)==0||AuthorAuxiliary(entry.path()))continue;
 auto relative=entry.path().lexically_relative(o.mod).u8string();files.emplace(relative,FileStamp(entry.path()));
 }std::string text;for(const auto& row:files)text+=row.first+":"+std::to_string(row.second.bytes)+":"+std::to_string(row.second.written)+"\n";auto hash=Sha((const unsigned char*)text.data(),text.size());return Hex(hash.data(),hash.size());}
Pointer Compile(const Options& o){
 auto started=std::chrono::steady_clock::now();auto ms=[&]{return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();};
 auto stamp=SourceStamp(o);auto package=Load(o.mod);auto author=PackageDigest(package);auto assembly=Hash(Read(o.assembly));auto baseline=Hash(Read(o.baseline/"baseline.tsv"));
 auto proofBytes=Read(o.baseline/"assembly.sha256",128);auto proof=Trim(Text(proofBytes.data(),proofBytes.size()));Require(proof==assembly,"offline baseline belongs to another GameAssembly");
 std::vector<std::string> roots;auto sources=ReadBaseline(o.baseline,roots);auto nonce=std::string(compiler)+"/"+assembly+"/"+baseline;
 std::string identity=author+"\n"+InputContractDigest(package)+"\n"+nonce;for(const auto& source:sources)identity+="\n"+source.file+"\n"+source.hash;
 auto key=Hash(Bytes(identity.begin(),identity.end()));auto destination=o.cache/"generations"/key;
 if(std::filesystem::exists(destination/"manifest.bin")){
  auto bytes=Read(destination/"manifest.bin",manifestLimit);Pointer pointer{key,Hash(bytes)};auto existing=Decode(bytes,destination,o.mod.parent_path());Require(existing.assembly==assembly&&existing.baseline==baseline&&existing.plan.author==author,"offline existing generation differs");
  for(const auto& input:existing.plan.inputs)Require(Hash(Read(input.path,inputLimit))==input.hash,"offline cached input SHA differs");
  Require(SourceStamp(o)==stamp,"author changed during cache validation");Publish(o.cache,pointer);std::cout<<"OFFLINE-REUSED key="<<key<<" elapsedMs="<<ms()<<std::endl;return pointer;
 }
 // Build into a unique directory. Never remove a previous or partially built
 // generation, and never expose this directory through current until verified.
 auto temporary=o.cache/("building-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 auto plan=Build(package,o.baseline,temporary,nonce,{},[&](const char* stage,size_t n,size_t total){if(!n||n==total)std::cout<<"OFFLINE-PROGRESS phase="<<stage<<" completed="<<n<<'/'<<total<<" elapsedMs="<<ms()<<std::endl;},{},o.cache/"geometry");
 std::cout<<"OFFLINE-CACHE appearanceHits="<<plan.appearanceHits<<" appearanceMisses="<<plan.appearanceMisses<<" namespaceHits="<<plan.namespaceHits<<" namespaceMisses="<<plan.namespaceMisses<<" geometryInputHits="<<plan.geometryInputHits<<" geometryInputMisses="<<plan.geometryInputMisses<<std::endl;
 StartupTable(o,package,temporary,destination);
 Require(plan.key==key,"offline deterministic key differs");Manifest manifest{std::move(plan),std::move(package),assembly,baseline};auto bytes=Encode(manifest);
 auto checked=Decode(bytes,temporary,o.mod.parent_path());Require(Encode(checked)==bytes,"offline compiled descriptor roundtrip differs");
 Require(SourceStamp(o)==stamp&&PackageDigest(Load(o.mod))==author,"author changed during offline compilation; previous publication retained");
 Save(temporary/"manifest.bin",bytes);std::filesystem::create_directories(destination.parent_path());
 Require(!std::filesystem::exists(destination),"offline destination exists without a complete manifest");std::filesystem::rename(temporary,destination);Pointer pointer{key,Hash(bytes)};Publish(o.cache,pointer);
 std::cout<<"OFFLINE-PUBLISHED key="<<key<<" inputs="<<manifest.plan.inputs.size()<<" elapsedMs="<<ms()<<" UnityCalls=0"<<std::endl;return pointer;
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc==5||argc==6||argc==7,"eff_resource_pack <Mod> <baseline> <cache> <GameAssembly.dll> [--watch | --watch-parent PID]");Options o{argv[1],argv[2],argv[3],argv[4],argc>=6};if(o.watch){Require(std::wstring(argv[5])==(argc==7?L"--watch-parent":L"--watch"),"unknown offline option");if(argc==7){size_t used=0;auto pid=std::stoul(argv[6],&used);Require(used==wcslen(argv[6])&&pid&&pid<=MAXDWORD,"offline parent PID");o.parent=DWORD(pid);}}
 o.mod=std::filesystem::canonical(o.mod);o.baseline=std::filesystem::canonical(o.baseline);o.cache=std::filesystem::absolute(o.cache);o.assembly=std::filesystem::canonical(o.assembly);
 auto label=InputPath(o.mod.u8string()+"|"+o.cache.lexically_normal().u8string());auto hash=Hash(Bytes(label.begin(),label.end()));auto suffix=std::wstring(hash.begin(),hash.end());
 Handle parent(o.parent?OpenProcess(SYNCHRONIZE,FALSE,o.parent):nullptr);Require(!o.parent||parent.value,"offline parent process unavailable");
 Handle watcher(o.watch?CreateMutexW(nullptr,TRUE,(L"Local\\EFFOfflineWatch-"+suffix).c_str()):nullptr);
 if(o.watch){Require(watcher.value,"offline watcher mutex failed");if(GetLastError()==ERROR_ALREADY_EXISTS){std::cout<<"OFFLINE-WATCH-ALREADY-ACTIVE"<<std::endl;return 0;}watcher.owned=true;}
 // Only a build holds this mutex. Blender one-shot exports can queue behind a
 // watcher, then verify/reuse the exact new author generation under the lock.
 Handle single(CreateMutexW(nullptr,FALSE,(L"Local\\EFFOfflinePack-"+suffix).c_str()));Require(single.value,"offline build mutex failed");
 std::string attempted;do{if(!ParentAlive(parent))break;try{auto stamp=SourceStamp(o);if(stamp!=attempted){if(o.watch){std::this_thread::sleep_for(std::chrono::milliseconds(500));if(SourceStamp(o)!=stamp)continue;}
  if(!LockBuild(single,parent))break;try{attempted=SourceStamp(o);Compile(o);}catch(...){ReleaseMutex(single.value);single.owned=false;throw;}
  ReleaseMutex(single.value);single.owned=false;
 }}catch(const std::exception& e){std::cerr<<"OFFLINE-REJECT reason="<<e.what()<<" previousGenerationRetained=1"<<std::endl;if(!o.watch)return 1;}
 if(o.watch)std::this_thread::sleep_for(std::chrono::seconds(1));}while(o.watch);return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
