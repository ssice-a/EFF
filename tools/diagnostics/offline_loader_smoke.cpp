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
void ModSetAuthorMutation(){
 Require(std::filesystem::exists(gameDirectory/".eff-isolated-author-test"),"Mod set mutation requires isolated fixture marker");
 auto installed=std::atomic_load(&published);RuntimeControls::LoadControls(installed);auto panel=RuntimeControls::panel.Read();
 auto request=[&](size_t count,const std::string& selected){
  FullResourceReload::Prepare();auto ticket=std::atomic_load(&FullResourceReload::requested);
  Require(ticket&&ticket->config&&ticket->pointer==EiemOfflinePackage::ReadCurrent(gameDirectory/"plugin/resource-reload-cache")&&ticket->controls.graph->size()==count&&ticket->controls.selected==selected,"F10 failed to compile/select changed installed author set");
  Require(std::atomic_load(&published)==installed&&RuntimeControls::panel.Read()==panel,"disk-only F10 preparation published live graph/UI without native commit");
 };
 auto mods=gameDirectory/"plugin/mods";std::filesystem::rename(mods/"CharacterB",gameDirectory/"parkedB");request(1,"CharacterA");
 std::filesystem::rename(mods/"CharacterA",gameDirectory/"parkedA");request(0,"");
 std::filesystem::rename(gameDirectory/"parkedB",mods/"CharacterB");request(1,"CharacterB");
 std::filesystem::rename(gameDirectory/"parkedA",mods/"CharacterA");request(2,"CharacterA");
 std::filesystem::rename(mods,gameDirectory/"parkedMods");request(0,"");
 Require(!std::filesystem::exists(mods),"empty Mod set compiler recreated deleted mods directory");
 std::filesystem::rename(gameDirectory/"parkedMods",mods);request(2,"CharacterA");
 std::cout<<"PASS F10 author removal -> empty -> readdition -> multiple Mods: external compiler invoked, exact candidate selected, live controls/UI unchanged before native commit, UnityCalls=0\n";
}
int wmain(int argc,wchar_t** argv){try{
 bool single=argc==3&&std::wstring(argv[2])==L"--single-pass";
 Require(argc==2||argc==4||argc==3&&(single||std::wstring(argv[2])==L"--mod-set-lifecycle"),"offline_loader_smoke <game> [--single-pass | --mod-set-lifecycle | <converter> <PNG>]");gameDirectory=argv[1];generation=1;FullResourceReload::enabled=true;
 if(argc==4){Require(std::filesystem::exists(gameDirectory/".eff-isolated-author-test"),"author mutation test requires isolated fixture marker");logFile=stdout;}
 Require(!il2cpp_runtime_invoke,"disk worker test unexpectedly has Unity API access");
 // Exercise the actual startup manifest selection and all source/output range,
 // SHA and namespace checks. Only managed string rooting is a local fixture.
 il2cpp_string_new=StartupFixture::String;il2cpp_gchandle_new=StartupFixture::Root;il2cpp_gchandle_get_target=StartupFixture::Target;
 LoadInitialPackages();auto pointer=EiemOfflinePackage::ReadCurrent(gameDirectory/"plugin/resource-reload-cache");
 Require(initialInputTable==gameDirectory/"plugin/resource-reload-cache/generations"/pointer.key/"loading-inputs.tsv","startup did not select latest native package");
 size_t startupInputs=0;
 if(argc!=4){PrepareInputSubstitutions();Require(pendingSubstitutions&&(initialComposed||!pendingSubstitutions->entries.empty()),"latest startup substitution table missing");
  startupInputs=pendingSubstitutions->entries.size();for(const auto& row:pendingSubstitutions->entries){Require(row.second.output.parent_path().filename()=="startup"&&row.second.pathHandle&&row.second.file,"startup namespace/path/file root differs");
   if(std::filesystem::exists(gameDirectory/".eff-isolated-author-test")) {
   Require(!DeleteFileW(row.second.output.c_str())&&GetLastError()==ERROR_SHARING_VIOLATION,"startup input can be deleted while game may reopen it");
   HANDLE overwrite=CreateFileW(row.second.output.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
   Require(overwrite==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_SHARING_VIOLATION,"startup input can be overwritten during native lifetime");
   }
  }
 }
 auto start=NativeBundle::Clock();FullResourceReload::PollOffline();auto prepareMs=NativeBundle::Ms(start);
 Require(FullResourceReload::pending&&FullResourceReload::pending->files.size()==FullResourceReload::pending->plan->closure.size()&&!FullResourceReload::pending->plan->inputs.empty(),"production offline worker did not validate a complete locked candidate");
 auto reference=RuntimeControls::Prepare(FullResourceReload::pending->graph,EiemEntrySettings::Read(gameDirectory));const auto& compiled=FullResourceReload::pending->controls;
 Require(reference.specs.size()==compiled.specs.size()&&reference.variables==compiled.variables&&reference.keys.size()==compiled.keys.size(),"compiled controls coverage/defaults differ from author builder");
 for(const auto& row:reference.specs){const auto& actual=compiled.specs.at(row.first);const auto& expected=row.second;
  Require(actual.package==expected.package&&actual.part==expected.part&&actual.marker==expected.marker&&actual.vertices==expected.vertices&&actual.channels==expected.channels&&actual.controls==expected.controls&&actual.visible==expected.visible,"compiled control semantics differ");}
 double worst=0,total=0;unsigned samples=single?1:20;for(unsigned i=0;i<samples;++i){reloadClock=NativeBundle::Clock();auto request=NativeBundle::Clock();FullResourceReload::Prepare();auto elapsed=NativeBundle::Ms(request);total+=elapsed;worst=std::max(worst,elapsed);auto ticket=std::atomic_load(&FullResourceReload::requested);
  Require(ticket&&ticket->config&&ticket->controls.graph&&ticket->pointer==FullResourceReload::pending->pointer,"F10 request failed to select its exact compiled candidate");
 }
 Require(!il2cpp_runtime_invoke,"worker gained managed API access");
 std::cout<<"PASS production offline worker: latestStartupInputs="<<startupInputs<<" startupNativeNamespaceAndPhysicalRangeHashes="<<(argc!=4)<<" startupLifetimeFileLocks="<<(argc!=4)<<" inputs="<<FullResourceReload::pending->plan->inputs.size()<<" lockedClosure="<<FullResourceReload::pending->files.size()<<" runtimeTypeTreeBuild=0 repeatedMeshProvenanceHash=0 compiledControlsEqual=1 UnityCalls=0 initialValidationMs="<<prepareMs<<" repeatedF10WorkerWorstMs="<<worst<<" repeatedF10WorkerMeanMs="<<total/samples<<" samples="<<samples<<" nativeMigrationMeasured=0\n";
 if(argc==4){auto converter=std::filesystem::absolute(argv[2]);auto png=std::filesystem::absolute(argv[3]);ConvertRenamedTexture(converter,png,false);
  auto second=png.parent_path()/"RELOAD_SECOND.png";std::filesystem::copy_file(png,second);ConvertRenamedTexture(converter,second,true);
 }
 if(argc==3&&!single)ModSetAuthorMutation();
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
