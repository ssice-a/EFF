#include "../src/eiem_resource_compiler.h"
#include "../src/eiem_compiled_mod_writer.h"
#include <iostream>
#include <thread>
using namespace EiemResourceCompiler;
std::string JsonString(const std::string& value){std::string result="\"";for(unsigned char ch:value){if(ch=='\"'||ch=='\\'){result+='\\';result+=char(ch);}else if(ch<32){const char* hex="0123456789ABCDEF";result+="\\u00";result+=hex[ch>>4];result+=hex[ch&15];}else result+=char(ch);}return result+"\"";}
void SourceRequest(const Package& p){
 std::map<Identity,std::string> sources;for(const auto& t:p.targetIndex)sources.emplace(t.first,"Mesh");for(const auto& m:p.materials)if(!m.second.source.path.empty())sources.emplace(m.second.source,"Material");for(const auto& t:p.textures)if(!t.second.source.path.empty())sources.emplace(t.second.source,"Texture2D");
 std::cout<<"{\"sources\":[";bool first=true;for(const auto& row:sources){if(!first)std::cout<<',';first=false;std::cout<<"{\"kind\":"<<JsonString(row.second)<<",\"path\":"<<JsonString(row.first.path)<<",\"asset\":"<<JsonString(row.first.asset)<<'}';}std::cout<<"]}"<<std::endl;
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==4&&std::wstring(argv[1])==L"--export-mod"){EiemCompiledModWriter::Export(std::filesystem::canonical(argv[2]),std::filesystem::canonical(argv[3]));std::cout<<"COMPILED-MOD-PUBLISHED"<<std::endl;return 0;}
 if(argc==3&&std::wstring(argv[1])==L"--source-request"){SourceRequest(Load(argv[2]));return 0;}
 if(argc==2&&std::wstring(argv[1])==L"--protocol") {
  std::cout<<"{\"format\":\"EFF_OFFLINE_PACKAGE_PROTOCOL\",\"version\":1,\"abi\":"<<EiemPackageProtocol::abi
           <<",\"compiledMod\":\""<<EiemCompiledMod::structure<<"\",\"compiler\":\""<<compiler<<"\",\"authorFormat\":"<<EiemPackageProtocol::authorFormat
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
 Require(count>=4,"eff_resource_pack [--mods] <Mod or mods directory> <baseline> <cache> [--watch | --watch-parent PID]");Options o{argv[1+offset],argv[2+offset],argv[3+offset]};o.set=set;o.report=[](const std::string& line){std::cout<<line<<std::endl;};
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
