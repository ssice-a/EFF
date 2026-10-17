#include "../src/eiem_resource_compiler.h"
#include "../src/eiem_compiled_mod_writer.h"
#include "../src/eiem_prepared_prefab_compiler.h"
#include "../src/eiem_prepared_export_carryover.h"
#include "../src/eiem_compiled_object_merge.h"
#include "eff_author_sources.h"
#include <iostream>
#include <thread>
using namespace EiemResourceCompiler;
std::string JsonString(const std::string& value){std::string result="\"";for(unsigned char ch:value){if(ch=='\"'||ch=='\\'){result+='\\';result+=char(ch);}else if(ch<32){const char* hex="0123456789ABCDEF";result+="\\u00";result+=hex[ch>>4];result+=hex[ch&15];}else result+=char(ch);}return result+"\"";}
void SourceRequest(const Package& p){
 std::cout<<EiemAuthorSources::RequestJson(EiemAuthorSources::Collect(p))<<std::endl;
}
void DescribeTargets(const std::filesystem::path& mod,const std::filesystem::path& game,const std::string& mesh={}){
 auto package=Load(mod);auto selection=EiemStaticSources::LoadMod(mod,game);std::set<Identity> targets;
 for(const auto& row:package.targetIndex)if(mesh.empty()||row.first.asset==mesh)targets.insert(row.first);
 Require(!targets.empty(),"target Mesh identity absent from Mod");auto found=EiemStaticSources::FindConsumers(selection,targets);
 std::cout<<"{\"prefabs\":[";bool first=true;for(const auto& root:found.roots){if(!first)std::cout<<',';first=false;std::cout<<JsonString(root);}
 std::cout<<"],\"renderers\":"<<found.renderers<<",\"sourceInputs\":"<<found.sources.size()<<",\"nativePhysicsTypes\":[";first=true;
 for(const auto& source:found.sources){auto bundle=EndfieldInput::Decode(EiemStaticSources::Payload(selection.inputs.at(source)));
  for(const auto& member:bundle.members)if(SerializedHeader(member.bytes)){auto layout=ParseSerialized(member.bytes);std::set<size_t> described;
   for(const auto& object:layout.objects){const auto& type=layout.types.at(object.second.type);if(type.classId!=114||!described.insert(object.second.type).second)continue;
    bool cloth=false,collider=false;for(auto node:type.nodes.front().children){const auto& name=type.nodes.at(node).name;cloth=cloth||name=="serializeData2";collider=collider||name=="radiusSeparation";}
    if(!cloth&&!collider)continue;if(!first)std::cout<<',';first=false;
    std::cout<<"{\"cab\":"<<JsonString(member.name)<<",\"kind\":"<<JsonString(cloth?"cloth":"capsule")<<",\"typeHash\":"<<JsonString(Hex(type.hash.data(),type.hash.size()))<<'}';
   }
  }
 }
 std::cout<<"]}"<<std::endl;
}
void AuditPhysics(const std::filesystem::path& resource,const std::filesystem::path& mod,const std::filesystem::path& game){
 auto package=Load(mod);auto author=EiemDirectory::Decode(Read(resource),0);
 if(std::none_of(package.resources.begin(),package.resources.end(),[&](const auto& row){return row.second->bytes==author.bytes;}))package.resources["resources/audit-physics.bin"]=std::make_shared<Resource>(std::move(author));
 auto documents=EiemPhysicsInput::Documents(package);Require(!documents.empty(),"Physics resource absent");
 auto selection=EiemStaticSources::LoadMod(mod,game);AssetIndex assets;std::set<Identity> targets;
 for(const auto& document:documents)for(const auto& group:document.groups)targets.insert(group.targets.begin(),group.targets.end());
 auto found=EiemStaticSources::FindConsumers(selection,targets);std::vector<EndfieldInput::Bundle> inputs;
 for(const auto& source:found.sources){auto bundle=EndfieldInput::Decode(EiemStaticSources::Payload(selection.inputs.at(source)));for(const auto& member:bundle.members)if(SerializedHeader(member.bytes))for(auto asset:SerializedAssets(member.bytes,member.name,ParseSerialized(member.bytes)))AddAsset(assets,std::move(asset));inputs.push_back(std::move(bundle));}
 size_t changed=0,added=0,cloth=0,colliders=0,preserved=0,deduplicated=0;
 for(const auto& bundle:inputs)for(const auto& member:bundle.members)if(SerializedHeader(member.bytes)){
  auto built=EiemPhysicsInput::MemberBuilder(member.bytes,member.name,package,assets,documents).Build();if(built==member.bytes)continue;
  auto before=ParseSerialized(member.bytes),after=ParseSerialized(built);EiemCompiledMod::VerifyObjectReferences(built,before,after,member.bytes);++changed;
  for(const auto& row:before.objects){auto kind=before.types.at(row.second.type).classId;if(kind==1||kind==4||kind==142)continue;
   Require(after.objects.count(row.first),"Physics removed an original object");const auto& next=after.objects.at(row.first);
   Require(next.size==row.second.size&&std::equal(member.bytes.begin()+row.second.offset,member.bytes.begin()+row.second.offset+row.second.size,built.begin()+next.offset),"Physics modified an unrelated original component/resource");++preserved;
  }
  std::set<NativeReference> refs;for(const auto& target:targets){const auto& asset=assets.at(target);refs.emplace(asset.cab,asset.pathId);}auto consumers=MeshConsumers(member.bytes,before,member.name,refs);Require(!consumers.empty(),"Physics changed a member with no target consumer");
  auto cloneId=before.objects.rbegin()->first;Require(cloneId<INT64_MAX,"diagnostic clone PathID cannot be represented");++cloneId;
  const auto& originalRenderer=before.objects.at(consumers.front().renderer);Bytes renderer(member.bytes.begin()+originalRenderer.offset,member.bytes.begin()+originalRenderer.offset+originalRenderer.size);
  auto duplicate=BuildSerialized(member.bytes,before,{},{{cloneId,originalRenderer.type,std::move(renderer)}});auto duplicateLayout=ParseSerialized(duplicate);
  auto rebuilt=EiemPhysicsInput::MemberBuilder(duplicate,member.name,package,assets,documents).Build();auto rebuiltLayout=ParseSerialized(rebuilt);
  Require(rebuiltLayout.objects.size()-duplicateLayout.objects.size()==after.objects.size()-before.objects.size(),"multiple Renderers duplicated Physics in one skeleton");++deduplicated;
  for(const auto& object:after.objects)if(!before.objects.count(object.first)){++added;const auto& type=after.types.at(object.second.type);if(type.classId!=114)continue;
   bool isCloth=false;for(auto node:type.nodes.front().children)isCloth=isCloth||type.nodes.at(node).name=="serializeData2";if(isCloth)++cloth;else ++colliders;
  }
 }
 std::cout<<"{\"prefabs\":"<<found.roots.size()<<",\"changedMembers\":"<<changed<<",\"addedObjects\":"<<added<<",\"clothComponents\":"<<cloth<<",\"colliderComponents\":"<<colliders<<",\"originalObjectsPreserved\":"<<preserved<<",\"sharedSkeletonDedupChecks\":"<<deduplicated<<"}"<<std::endl;
}
int wmain(int argc,wchar_t** argv){try{
 if(argc==5&&std::wstring(argv[1])==L"--audit-physics"){AuditPhysics(std::filesystem::canonical(argv[2]),std::filesystem::canonical(argv[3]),std::filesystem::canonical(argv[4]));return 0;}
 if((argc==4||argc==5)&&std::wstring(argv[1])==L"--describe-targets"){DescribeTargets(std::filesystem::canonical(argv[2]),std::filesystem::canonical(argv[3]),argc==5?std::filesystem::path(argv[4]).u8string():std::string{});return 0;}
 if(argc>=4&&std::wstring(argv[1])==L"--export-mod"){
  std::filesystem::path previous,index;for(int i=4;i<argc;i+=2){Require(i+1<argc,"export option value absent");auto option=std::wstring(argv[i]);if(option==L"--previous-mod"){Require(previous.empty(),"duplicate previous Mod option");previous=std::filesystem::canonical(argv[i+1]);}else if(option==L"--source-index"){Require(index.empty(),"duplicate source index option");index=std::filesystem::absolute(argv[i+1]);}else Require(false,"unknown author export option");}
  std::map<std::string,double> costs;std::mutex costsMutex;std::function<void(const char*,double)> cost=[&](const char* name,double elapsed){std::lock_guard<std::mutex> lock(costsMutex);costs[name]+=elapsed;};auto start=std::chrono::steady_clock::now();auto mod=std::filesystem::canonical(argv[2]),game=std::filesystem::canonical(argv[3]);
  std::optional<Package> author;std::optional<EiemStaticSources::Selection> selected;
  if(!index.empty()){author=EiemResourceReloadPlan::Measure(cost,"author-read",[&]{return Load(mod);});selected=EiemAuthorSources::Prepare(*author,game,index,cost);auto identity=selected->manifest+'\n';selected->manifest=Hash(Bytes(identity.begin(),identity.end()));}
  bool caughtUp=false;if(!previous.empty()){if(!author)author=Load(mod);if(!selected)selected=EiemStaticSources::Merge({*author},game);caughtUp=EiemPreparedExportCarryover::Try(mod,previous,game,*author,*selected,cost,!index.empty());}
  if(!caughtUp)EiemCompiledModWriter::Export(mod,game,cost,previous,author?&*author:nullptr,selected?&*selected:nullptr,nullptr,
   [](std::shared_ptr<const EiemCompiledMod::Artifact> artifact,const EiemStaticSources::Selection& selection){return EiemPreparedPrefabCompiler::Encode(EiemPreparedPrefabCompiler::Prepare(std::move(artifact),selection));},!index.empty());
  for(const auto& row:costs)std::cout<<"COMPILED-MOD-COST "<<row.first<<' '<<row.second<<'\n';std::cout<<"COMPILED-MOD-COST total "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<"\nCOMPILED-MOD-PUBLISHED"<<std::endl;return 0;
 }
 if((argc==5||argc==6)&&std::wstring(argv[1])==L"--prepare-static-sources"){Require(argc==5||std::wstring(argv[5])==L"--refresh","unknown source preparation option");auto package=Load(argv[4]);auto cost=[](const char* name,double elapsed){std::cout<<"COMPILED-MOD-COST "<<name<<' '<<elapsed<<'\n';};auto selection=EiemAuthorSources::Prepare(package,std::filesystem::canonical(argv[2]),std::filesystem::absolute(argv[3]),cost,argc==6);std::cout<<"STATIC-SOURCES-PREPARED paths="<<selection.assets.size()<<" candidates="<<selection.candidates.size()<<" inputs="<<selection.inputs.size()<<std::endl;return 0;}
 if(argc==3&&std::wstring(argv[1])==L"--source-request"){SourceRequest(Load(argv[2]));return 0;}
 if(argc==2&&std::wstring(argv[1])==L"--protocol") {
  std::cout<<"{\"format\":\"EFF_OFFLINE_PACKAGE_PROTOCOL\",\"version\":1,\"abi\":"<<EiemPackageProtocol::abi
           <<",\"sourcePreparation\":\"in-process/index-vfs+author-declarations\",\"compiledMod\":\""<<EiemCompiledMod::structure<<"\",\"compiledCacheInputs\":\"validated-previous-mod\",\"compiledExportCarryover\":\"validated-source-selection+published-discoveries\",\"compiler\":\""<<compiler<<"\",\"authorFormat\":"<<EiemPackageProtocol::authorFormat
           <<",\"resourceVersion\":"<<EiemPackageProtocol::resourceVersion<<",\"modSetVersion\":1,\"staticSourceInputs\":\""<<EiemPackageProtocol::staticSourceInputs<<"\",\"arguments\":[\"author\",\"baseline\",\"cache\"],\"structures\":{"
           <<"\"manifest\":\""<<EiemPackageProtocol::manifestStructure<<"\",\"author\":\""<<EiemPackageProtocol::authorStructure
           <<"\",\"resource\":\""<<EiemPackageProtocol::resourceStructure<<"\",\"current\":\""<<EiemPackageProtocol::currentStructure
           <<"\",\"modSet\":\""<<EiemPackageProtocol::setStructure
           <<"\",\"runtimeRules\":\""<<EiemPackageProtocol::ruleStructure
           <<"\",\"compiledInputView\":\""<<EiemPackageProtocol::compiledInputView
           <<"\",\"compiledRuntimeIndex\":\""<<EiemPackageProtocol::compiledRuntimeIndex
           <<"\",\"shapeAnimation\":\""<<EiemPackageProtocol::shapeAnimation
           <<"\",\"shapeAnimationPlayback\":\""<<EiemPackageProtocol::shapeAnimationPlayback
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
 }catch(const EiemAuthorSources::IndexNeedsRefresh& e){std::cerr<<"OFFLINE-SOURCE-INDEX-STALE\n"<<e.what()<<std::endl;return 3;}
 catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}
