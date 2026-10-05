// Exercise the production disk worker without any managed/runtime API exports.
#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace StartupFixture {
 std::vector<std::unique_ptr<std::string>> strings;std::map<uint32_t,void*> handles;
 void* __cdecl String(const char* text){strings.push_back(std::make_unique<std::string>(text));return strings.back().get();}
 uint32_t __cdecl Root(void* object,bool){auto id=uint32_t(handles.size()+1);handles.emplace(id,object);return id;}
 void* __cdecl Target(uint32_t id){return handles.at(id);}
}
void ConvertRenamedTexture(const std::filesystem::path& converter,const std::filesystem::path& png,bool resetPrepared){
 auto before=EiemOfflinePackage::ReadCurrent(gameDirectory/"plugin/resource-reload-cache");
 auto package=FullResourceReload::diskPrepared->graph->front();auto old=package.root/std::filesystem::u8path(package.textures.at("TextureT_actor_lizhiyan_body_01_D").resource);
 auto quote=[](const std::filesystem::path& value){Require(value.wstring().find(L'\"')==std::wstring::npos,"fixture path contains quote");return L"\""+value.wstring()+L"\"";};
 auto command=quote(converter)+L" --no-pause --mod "+quote(package.root)+L" --texture TextureT_actor_lizhiyan_body_01_D "+quote(png);
 STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION process{};
 Require(CreateProcessW(converter.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)!=0,"fixture converter launch failed");CloseHandle(process.hThread);
 auto wait=WaitForSingleObject(process.hProcess,30000);DWORD code=1;bool completed=GetExitCodeProcess(process.hProcess,&code)!=0;CloseHandle(process.hProcess);
 Require(wait==WAIT_OBJECT_0&&completed&&code==0,"fixture converter failed");
 auto filename=png.filename();filename.replace_extension(".tex");auto output=old.parent_path()/filename;
 Require(!std::filesystem::exists(old)&&std::filesystem::exists(output),"fixture did not rename/remove old texture");
 if(resetPrepared){FullResourceReload::lastPrepared={};FullResourceReload::diskPrepared.reset();}
 FullResourceReload::Prepare();auto after=EiemOfflinePackage::ReadCurrent(gameDirectory/"plugin/resource-reload-cache");auto ticket=std::atomic_load(&FullResourceReload::requested);
 Require(after!=before&&ticket&&ticket->pointer==after&&ticket->controls.graph,"renamed texture F10 did not compile/select new author");
 const auto& loaded=ticket->controls.graph->front();Require(loaded.root/std::filesystem::u8path(loaded.textures.at("TextureT_actor_lizhiyan_body_01_D").resource)==output,"renamed texture F10 retained old resource path");
 std::cout<<"PASS renamed texture F10 oldPathRemoved=1 externalCompiler=1 currentUpdated=1 candidateSelected=1 manifestRead="<<resetPrepared<<" UnityCalls=0\n";
}
int wmain(int argc,wchar_t** argv){try{
 Require(argc==2||argc==4,"offline_loader_smoke <game> [<converter> <PNG>]");gameDirectory=argv[1];generation=1;FullResourceReload::enabled=true;
 if(argc==4){Require(std::filesystem::exists(gameDirectory/".eff-isolated-author-test"),"author mutation test requires isolated fixture marker");logFile=stdout;}
 Require(!il2cpp_runtime_invoke,"disk worker test unexpectedly has Unity API access");
 // Exercise the actual startup manifest selection and all source/output range,
 // SHA and namespace checks. Only managed string rooting is a local fixture.
 il2cpp_string_new=StartupFixture::String;il2cpp_gchandle_new=StartupFixture::Root;il2cpp_gchandle_get_target=StartupFixture::Target;
 LoadInitialPackages();auto pointer=EiemOfflinePackage::ReadCurrent(gameDirectory/"plugin/resource-reload-cache");
 Require(initialInputTable==gameDirectory/"plugin/resource-reload-cache/generations"/pointer.key/"loading-inputs.tsv","startup did not select latest native package");
 size_t startupInputs=0;
 if(argc==2){PrepareInputSubstitutions();Require(pendingSubstitutions&&!pendingSubstitutions->entries.empty(),"latest startup substitution table missing");
  startupInputs=pendingSubstitutions->entries.size();for(const auto& row:pendingSubstitutions->entries)Require(row.second.output.parent_path().filename()=="startup"&&row.second.pathHandle,"startup namespace/path root differs");
 }
 auto start=NativeCandidate::Clock();FullResourceReload::PollOffline();auto prepareMs=NativeCandidate::Ms(start);
 Require(FullResourceReload::pending&&FullResourceReload::pending->files.size()==18&&FullResourceReload::pending->plan->inputs.size()==68,"production offline worker did not validate a complete locked candidate");
 auto reference=RuntimeControls::Prepare(FullResourceReload::pending->graph,EiemEntrySettings::Read(gameDirectory));const auto& compiled=FullResourceReload::pending->controls;
 Require(reference.specs.size()==compiled.specs.size()&&reference.variables==compiled.variables&&reference.keys.size()==compiled.keys.size(),"compiled controls coverage/defaults differ from author builder");
 for(const auto& row:reference.specs){const auto& actual=compiled.specs.at(row.first);const auto& expected=row.second;
  Require(actual.package==expected.package&&actual.part==expected.part&&actual.marker==expected.marker&&actual.vertices==expected.vertices&&actual.channels==expected.channels&&actual.controls==expected.controls&&actual.visible==expected.visible,"compiled control semantics differ");}
 double worst=0,total=0;for(unsigned i=0;i<20;++i){reloadClock=NativeCandidate::Clock();auto request=NativeCandidate::Clock();FullResourceReload::Prepare();auto elapsed=NativeCandidate::Ms(request);total+=elapsed;worst=std::max(worst,elapsed);auto ticket=std::atomic_load(&FullResourceReload::requested);
  Require(ticket&&ticket->config&&ticket->controls.graph&&ticket->pointer==FullResourceReload::pending->pointer,"F10 request failed to select its exact compiled candidate");
 }
 Require(!il2cpp_runtime_invoke,"worker gained managed API access");
 std::cout<<"PASS production offline worker: latestStartupInputs="<<startupInputs<<" startupNativeNamespaceAndPhysicalRangeHashes="<<(argc==2)<<" inputs=68 lockedClosure=18 runtimeTypeTreeBuild=0 repeatedMeshProvenanceHash=0 compiledControlsEqual=1 UnityCalls=0 initialValidationMs="<<prepareMs<<" repeatedF10WorkerWorstMs="<<worst<<" repeatedF10WorkerMeanMs="<<total/20<<" samples=20 nativeMigrationMeasured=0\n";
 if(argc==4){auto converter=std::filesystem::absolute(argv[2]);auto png=std::filesystem::absolute(argv[3]);ConvertRenamedTexture(converter,png,false);
  auto second=png.parent_path()/"RELOAD_SECOND.png";std::filesystem::copy_file(png,second);ConvertRenamedTexture(converter,second,true);
 }
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
