#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace Fixture {
struct Camera {bool faded=false,fail=false;unsigned pitches=0,clears=0;void* pitchInfo=nullptr;void* clearInfo=nullptr;std::vector<int> order;};
__declspec(noinline) void Pitch(void* self,void* info) {auto& c=*(Camera*)self;++c.pitches;c.pitchInfo=info;c.order.push_back(1);c.faded=true;if(c.fail) throw std::runtime_error("pitch failure");}
__declspec(noinline) void Clear(void* self,void* info) {auto& c=*(Camera*)self;Require(c.faded,"camera clear ran before pitch");++c.clears;c.clearInfo=info;c.order.push_back(2);c.faded=false;}
MInfo pitch{(void*)Pitch},clear{(void*)Clear},duplicate{(void*)Pitch};
bool missingClass=false,missingPitch=false,missingClear=false,badType=false,badStatic=false,badArity=false,ambiguous=false;
void* Methods(void*,void** iterator) {
  auto index=(uintptr_t)*iterator;
  while(index<3) {
    *iterator=(void*)(index+1);
    switch(index++) {case 0:if(!missingPitch)return &pitch;break;case 1:if(!missingClear)return &clear;break;case 2:if(ambiguous)return &duplicate;break;}
  }
  return nullptr;
}
void InstallMetadata() {
  il2cpp_assembly_get_image=[](void* a){return a;};
  il2cpp_image_get_class_count=[](void*)->size_t{return missingClass?0:1;};
  il2cpp_image_get_class=[](void* i,size_t){return i;};
  il2cpp_class_get_name=[](void*){return "CameraMono";};
  il2cpp_class_get_namespace=[](void*){return "Beyond.Gameplay.View";};
  il2cpp_class_get_methods=Methods;
  il2cpp_method_get_name=[](void* m){return m==&clear?"ForceClearDither":"_ProcessDitherByPitch";};
  il2cpp_method_get_param_count=[](void*)->uint32_t{return badArity?1:0;};
  il2cpp_method_get_flags=[](void*,uint32_t* impl)->uint32_t{*impl=0;return badStatic?0x10:0;};
  il2cpp_method_get_return_type=[](void*){return (void*)1;};
  il2cpp_type_get_name=[](void*)->const char* {return _strdup(badType?"System.Boolean":"System.Void");};
  il2cpp_free=[](void* p){free(p);};
}
void Save(const std::filesystem::path& path,const std::string& bytes) {std::ofstream out(path,std::ios::binary);out<<bytes;Require(bool(out),"fixture write failed");}
}
int main() {
  using namespace Fixture;
  try {
    InstallMetadata();Require(MH_Initialize()==MH_OK,"MinHook init failed");void* assemblies[]={(void*)1};
    for(auto flag:{&missingClass,&missingPitch,&missingClear,&badType,&badStatic,&badArity,&ambiguous}) {
      *flag=true;EntryCameraFade::InstallOptional(assemblies,1);*flag=false;
      Require(!EntryCameraFade::hooked && installed.empty(),"incompatible/ambiguous camera metadata accepted");
    }
    clear.mp=nullptr;EntryCameraFade::InstallOptional(assemblies,1);Require(!EntryCameraFade::hooked,"missing native clear accepted");
    clear.mp=pitch.mp;EntryCameraFade::InstallOptional(assemblies,1);Require(!EntryCameraFade::hooked,"recursive clear address accepted");clear.mp=(void*)Clear;
    EntryCameraFade::InstallOptional(assemblies,1);Require(EntryCameraFade::hooked && installed.size()==1,"production camera adapter hook not installed");
    EntryCameraFade::InstallOptional(assemblies,1);Require(installed.size()==1,"camera hook installed twice");
    EiemCameraMethod volatile entry=Pitch;Camera a,b;settings.Publish({});active=true;
    for(int frame=0;frame<100;++frame) {
      entry(&a,&pitch);entry(&b,&pitch);
      Require(!a.faded && !b.faded && a.pitches==frame+1 && b.pitches==frame+1 && a.clears==frame+1 && b.clears==frame+1,"camera clear is not per-evaluation/per-instance");
      Require(a.order[frame*2]==1 && a.order[frame*2+1]==2 && b.order[frame*2]==1 && b.order[frame*2+1]==2,"camera original/clear order wrong");
    }
    Require(a.pitchInfo==&pitch && a.clearInfo==&clear && b.pitchInfo==&pitch && b.clearInfo==&clear,"camera MethodInfo mixed up");
    auto config=EiemGlobalConfig{};config.disableCameraFade=false;settings.Publish(config);entry(&a,&pitch);Require(a.faded && a.clears==100,"disabled camera module clears");
    config.disableCameraFade=true;settings.Publish(config);active=false;entry(&b,&pitch);Require(b.faded && b.clears==100,"manager-disabled camera module clears");
    active=true;entry(&a,&pitch);Require(!a.faded && a.clears==101,"reenabled camera module fails");
    auto clears=EntryCameraFade::clears.load(),evaluations=EntryCameraFade::evaluations.load();a.fail=true;bool thrown=false;
    try {entry(&a,&pitch);}catch(const std::exception&){thrown=true;}
    Require(thrown && a.clears==101 && EntryCameraFade::clears==clears && EntryCameraFade::evaluations==evaluations,"original exception swallowed or failed original followed by clear");
    auto temp=std::filesystem::absolute("diagnostics/resource-directory-entry-20261004/v26-config-reload-camera/camera-fixture-"+std::to_string(GetCurrentProcessId()));
    Require(!std::filesystem::exists(temp),"fixture already exists");auto disk=EiemEntrySettings::Read(temp);Require(disk.disableCameraFade && disk.reload.vk==VK_F10,"shared settings default creation failed");
    Save(temp/"plugin/eff.ini","[Hotkeys]\nreload=Ctrl+F8\n[Graphics]\ndisable_camera_fade=false\n");disk=EiemEntrySettings::Read(temp);settings.Publish(disk);
    Require(settings.Get().reload.vk==VK_F8 && settings.Get().reload.modifiers==MOD_CONTROL && !settings.Get().disableCameraFade,"shared strict parser fails shortcut/camera settings");
    for(auto bad:{"[Graphics]\ndisable_camera_fade=bad\n","[Hotkeys]\nreload=Ctrl+F8\ngui=Shift+F8\n","[Graphics]\ndisable_camera_fade=true\ndisable_camera_fade=false\n"}) {
      Save(temp/"plugin/eff.ini",bad);bool rejected=false;try {EiemEntrySettings::Read(temp);}catch(const std::exception&){rejected=true;}
      Require(rejected && !settings.Get().disableCameraFade && settings.Get().reload.vk==VK_F8,"invalid settings replace prior camera/shortcut settings");
    }
    std::filesystem::remove(temp/"plugin/eff.ini");std::filesystem::remove(temp/"plugin");std::filesystem::remove(temp);
    Require(MH_DisableHook((void*)Pitch)==MH_OK && MH_RemoveHook((void*)Pitch)==MH_OK && MH_Uninitialize()==MH_OK,"MinHook teardown failed");
    std::cout<<"PASS independent resource-entry camera adapter: production Exact metadata rejection, real MinHook, two cameras, original -> same-camera clear per evaluation, distinct MethodInfo, settings/manager gates, original exceptions; shared disk parser/default/chords/invalid retention\n";
  }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
