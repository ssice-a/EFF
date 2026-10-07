#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace Fixture {
struct Renderer {void* mesh=nullptr;unsigned char enabled=1;std::map<int,float> weights;};
std::map<void*,Renderer> renderers;
std::map<uint32_t,void*> handles;
uint32_t nextHandle=0;
unsigned char mainThread=1;
int frame=12;float elapsed=.01f;
unsigned writes=0,failAt=0;
UINT heldVk=0;
void* Invoke(void* method,void* self,void** args,void** exception) {
  *exception=nullptr;
  if(method==(void*)1) return &mainThread;
  if(method==(void*)5) return &frame;
  if(method==(void*)6) return &elapsed;
  auto& r=renderers.at(self);
  if(method==(void*)2) return &r.enabled;
  if(method==(void*)3) return &r.weights.at(*(int*)args[0]);
  if(method==(void*)4) {
    // Fail once AFTER changing the engine property: rollback must include the
    // attempted write, not only previously completed/read-back writes.
    r.weights[*(int*)args[0]]=*(float*)args[1];
    if(++writes==failAt) *exception=(void*)1;
    return nullptr;
  }
  throw std::runtime_error("unexpected fake managed method");
}
void SetEnabled(void* self,bool value) {
  renderers.at(self).enabled=value;
  if(++writes==failAt) throw std::runtime_error("injected setter failure");
}
void* Mesh(void* renderer) {return renderers.at(renderer).mesh;}
bool Sample(const EiemKeyChord& key) {return key.vk==heldVk;}
void Setup() {
  il2cpp_runtime_invoke=Invoke;il2cpp_object_unbox=[](void* o){return o;};
  il2cpp_gchandle_new=[](void* o,bool)->uint32_t {handles[++nextHandle]=o;return nextHandle;};
  il2cpp_gchandle_get_target=[](uint32_t h)->void* {return handles.at(h);};
  il2cpp_gchandle_free=[](uint32_t h){handles.erase(h);};
  RuntimeShapes::Ref::weakHandle=il2cpp_gchandle_new;
  RuntimeShapes::api.main=(void*)1;RuntimeShapes::api.alive=(void*)1;
  RuntimeShapes::api.getEnabled=(void*)2;RuntimeShapes::api.getWeight=(void*)3;RuntimeShapes::api.setWeight=(void*)4;
  RuntimeShapes::originalEnabledSetter=SetEnabled;RuntimeShapes::originalMeshGetter=Mesh;RuntimeShapes::enabled=true;
}
void Add(void* object,void* mesh,const RuntimeControls::Spec& spec) {
  Renderer r;r.mesh=mesh;RuntimeShapes::Bound b{RuntimeShapes::Ref::Make(object,true),RuntimeShapes::Ref::Make(mesh,false),spec.marker,{}};
  b.modelReady=true; // This fixture represents an already completed model.
  for(size_t i=0;i<spec.channels.size();++i) {b.indices[spec.channels[i]]=int(i);r.weights[int(i)]=0;}
  renderers[object]=r;RuntimeShapes::bound[object]=std::move(b);
}
std::string Replace(std::string s,const std::string& old,const std::string& next) {
  size_t pos=0,count=0;while((pos=s.find(old,pos))!=s.npos) {s.replace(pos,old.size(),next);pos+=next.size();++count;}
  Require(count>0,"fixture replacement absent: "+old);return s;
}
void Save(const std::filesystem::path& path,const std::string& bytes) {
  std::ofstream out(path,std::ios::binary);out<<bytes;Require(bool(out),"fixture save failed");
}
std::shared_ptr<const std::vector<Package>> Graph(const Package& p) {return std::make_shared<const std::vector<Package>>(std::vector<Package>{p});}
std::shared_ptr<EiemExpression> Constant(double value) {auto e=std::make_shared<EiemExpression>();e->op=EiemExpression::Number;e->number=value;return e;}
struct Camera {bool faded=false,fail=false;unsigned pitches=0,clears=0;void* pitchInfo=nullptr;void* clearInfo=nullptr;};
__declspec(noinline) void Pitch(void* self,void* info) {auto& c=*(Camera*)self;++c.pitches;c.pitchInfo=info;c.faded=true;if(c.fail) throw std::runtime_error("pitch failure");}
__declspec(noinline) void Clear(void* self,void* info) {auto& c=*(Camera*)self;Require(c.faded,"camera clear ran before pitch");++c.clears;c.clearInfo=info;c.faded=false;}
} // namespace Fixture
int main(int argc,char** argv) {
  using namespace Fixture;
  try {
    Require(argc==3,"usage: entry_reload_smoke <Mod dir> <v3 input table>");
    auto original=EiemDirectory::Load(std::filesystem::u8path(argv[1]));
    auto sandbox=std::filesystem::absolute("diagnostics/resource-directory-entry-20261004/v26-config-reload-camera/reload-fixture-"+std::to_string(GetCurrentProcessId()));
    auto temp=sandbox/"plugin/mods/LZY";
    Require(!std::filesystem::exists(sandbox),"fixture already exists");std::filesystem::create_directories(temp);
    gameDirectory=sandbox;
    auto ini=Read(original.root/"mod.ini");std::string author(ini.begin(),ini.end());author.erase(std::remove(author.begin(),author.end(),'\r'),author.end());Save(temp/"mod.ini",author);
    for(const auto& file:original.resources) {auto path=temp/std::filesystem::u8path(file.first);std::filesystem::create_directories(path.parent_path());std::filesystem::copy_file(original.root/std::filesystem::u8path(file.first),path);}
    auto package=EiemDirectory::Load(temp);auto graph=Graph(package);Setup();settings.Publish({});
    RuntimeControls::LoadControls(graph);std::atomic_store(&published,graph);generation=41;
    RuntimeShapes::packageGeneration=41;RuntimeShapes::checkedGeneration=41;
    // Exercise the real digest/table validator against the prepared native files.
    auto input=std::make_shared<EiemNativeInput::InputSubstitutions>(EiemNativeInput::ReadInputSubstitutions(std::filesystem::u8path(argv[2]),std::filesystem::u8path(argv[1]).parent_path().parent_path().parent_path(),package));
    input->generation=41;std::atomic_store(&substitutions,std::shared_ptr<const EiemNativeInput::InputSubstitutions>(input));
    auto inputDigest=EiemNativeInput::InputContractDigest(package);auto packageDigest=EiemNativeInput::PackageDigest(package);
    size_t expectedSpecs=0;for(const auto& r:package.rules) if(!r.second.skip) expectedSpecs+=r.second.parts.size()*r.second.targets.size();
    Require(RuntimeControls::specs.size()==expectedSpecs && expectedSpecs==43,"all authored part/LOD provenance identities not registered");
    const RuntimeControls::Spec* body=nullptr;const RuntimeControls::Spec* ornament=nullptr;
    for(const auto& s:RuntimeControls::specs) {if(!body && !s.second.channels.empty()) body=&s.second;if(!ornament && s.second.part.find("toushi")!=std::string::npos) ornament=&s.second;}
    Require(body && ornament && body->channels.size()==2,"actual package body/toushi specs missing");
    std::string bodyMarker=body->marker,part=ornament->part,ornamentMarker=ornament->marker;
    void* bodyObject=(void*)0x10;void* ornamentObject=(void*)0x11;
    Add(bodyObject,(void*)0x20,*body);Add(ornamentObject,(void*)0x21,*ornament);
    // Old capture Mesh remains rooted independently of live control graph.
    RuntimeShapes::groups.push_back({RuntimeShapes::Ref::Make((void*)0x30,true),RuntimeShapes::Ref::Make((void*)0x31,true),{{RuntimeShapes::Ref::Make((void*)0x32,false)}}});
    auto snapshotHandle=RuntimeShapes::groups.front().snapshots.front().mesh.handle;
    std::string channel=body->channels.front(),shape=body->controls.at(channel);
    RuntimeControls::variables.at(package.id).at(shape)=.65f;renderers.at(bodyObject).weights[0]=65;
    RuntimeControls::variables.at(package.id).at("$switch_a2670be72e524621")=1;
    // Parse actual INI edits: defaults/bindings/visibility and removed ownership.
    auto edited=Replace(author,"key=NUMPAD5","key=F6");
    edited=Replace(edited,"persist $switch_a2670be72e524621=0","persist $switch_a2670be72e524621=0\n$temporary=7");
    edited=Replace(edited,"[PartS_actor_lizhiyan_cloth_01_lod0_1.toushi]\nmesh=MeshS_actor_lizhiyan_cloth_01_lod0_1.toushi\nmaterial=MaterialM_actor_lizhiyan_cloth_01_1\nvisible=true",
      "[PartS_actor_lizhiyan_cloth_01_lod0_1.toushi]\nmesh=MeshS_actor_lizhiyan_cloth_01_lod0_1.toushi\nmaterial=MaterialM_actor_lizhiyan_cloth_01_1\nvisible=false");
    // CRLF author support.
    Save(temp/"mod.ini",edited);auto changed=EiemDirectory::Load(temp);auto changedGraph=Graph(changed);
    Require(EiemNativeInput::InputContractDigest(changed)==inputDigest && EiemNativeInput::PackageDigest(changed)!=packageDigest,"runtime-only edits change native contract or fail full digest");
    auto config=EiemGlobalConfig{};config.reload={VK_F8,MOD_CONTROL};config.disableCameraFade=false;
    auto candidate=EntryReload::Validate(changedGraph,config);
    Require(generation==41 && settings.Get().disableCameraFade && renderers.at(ornamentObject).enabled,"candidate preparation publishes engine/settings");
    // Preserve valid persistent live values, initialize added ordinary values.
    RuntimeControls::variables.at(package.id)["$temporary"]=99;
    heldVk=VK_F6;EntryReload::ApplyControls(candidate->controls,Sample);
    Require(!renderers.at(ornamentObject).enabled && RuntimeControls::variables.at(package.id).at("$temporary")==7 && RuntimeControls::variables.at(package.id).at("$switch_a2670be72e524621")==1,"live reload visibility/persistence/defaults failed");
    RuntimeControls::PollControls(true,.01,Sample);
    Require(RuntimeControls::variables.at(package.id).at("$switch_a2670be72e524621")==1,"reload manufactures held cycle edge");
    heldVk=0;RuntimeControls::PollControls(true,.01,Sample);heldVk=VK_F6;RuntimeControls::PollControls(true,.01,Sample);
    Require(RuntimeControls::variables.at(package.id).at("$switch_a2670be72e524621")==0,"new binding inactive after reload");
    // Commit through the production frame boundary with removed shape binding.
    auto removed=changed;removed.parts.at(RuntimeControls::specs.at(bodyMarker).part).shapes.erase(channel);
    auto removal=EntryReload::Validate(Graph(removed),config);{std::lock_guard<std::mutex> lock(EntryReload::mutex);EntryReload::pending=removal;}
    EntryReload::Commit();
    Require(generation==42 && settings.Get().reload.vk==VK_F8 && settings.Get().reload.modifiers==MOD_CONTROL && !settings.Get().disableCameraFade,"frame transaction fails settings/generation publish");
    Require(renderers.at(bodyObject).weights[0]==0 && RuntimeControls::specs.at(bodyMarker).channels.size()==2 && RuntimeControls::specs.at(bodyMarker).controls.size()==1,"removed shape ownership not reset or native channels removed");
    Require(RuntimeShapes::groups.size()==1 && RuntimeShapes::groups.front().snapshots.front().mesh.handle==snapshotHandle && *snapshotHandle && input->entries.size()==60,"reload retires active capture snapshots or loading paths");
    // Inject an engine failure after shape+visibility writes, retain old graph,
    // config, input generation and every previously changed engine property.
    auto rollback=removed;rollback.parts.at(part).visible=Constant(1);rollback.parts.at(RuntimeControls::specs.at(bodyMarker).part).shapes=package.parts.at(RuntimeControls::specs.at(bodyMarker).part).shapes;
    rollback.persistent.erase(shape);rollback.defaults[shape]=.9;
    auto beforeGraph=std::atomic_load(&published);auto beforeInputs=std::atomic_load(&substitutions);auto beforeValues=RuntimeControls::variables;
    auto failing=EntryReload::Validate(Graph(rollback),{});writes=0;failAt=2;
    {std::lock_guard<std::mutex> lock(EntryReload::mutex);EntryReload::pending=failing;}EntryReload::Commit();failAt=0;
    Require(generation==42 && std::atomic_load(&published)==beforeGraph && std::atomic_load(&substitutions)==beforeInputs && RuntimeControls::variables==beforeValues && !settings.Get().disableCameraFade && !renderers.at(ornamentObject).enabled && renderers.at(bodyObject).weights[0]==0,"failed transaction changes old config/input/control/engine state");
    // Invalid/redefined persistent domains use new defaults; ordinary values reset.
    auto domain=removed;domain.controls.at(shape).maximum=.25;domain.defaults[shape]=.1;
    for(auto& key:domain.bindings) if(key.variable==shape) key.values={.1};
    auto state=RuntimeControls::Prepare(Graph(domain),config);RuntimeControls::MergeLive(state,Sample);
    Require(state.variables.at(package.id).at(shape)==.1,"out-of-domain persistent value retained");
    for(int scenario=0;scenario<7;++scenario) {
      auto invalid=removed;auto reserved=config;
      if(scenario==0) invalid.meshes.erase(invalid.meshes.begin());
      if(scenario==1) invalid.materials.begin()->second.textures["_BaseMap"]="different";
      if(scenario==2) invalid.textures.begin()->second.width*=2;
      if(scenario==3) invalid.parts.at(part).material="different";
      if(scenario==4) invalid.rules.begin()->second.skip=!invalid.rules.begin()->second.skip;
      if(scenario==5) {auto resource=std::make_shared<Resource>(*invalid.resources.begin()->second);resource->bytes.back()^=1;invalid.resources.begin()->second=resource;}
      if(scenario==6) reserved.reload=invalid.bindings.front().key;
      bool rejected=false;try {EntryReload::Validate(Graph(invalid),reserved);}catch(const std::exception&){rejected=true;}
      Require(rejected && generation==42 && std::atomic_load(&published)==beforeGraph,"changed native contract/reserved key accepted or old generation lost");
    }
    Save(temp/"mod.ini","[Mod]\nformat=bad\n");bool parseRejected=false;try {EiemDirectory::Load(temp);}catch(const std::exception&){parseRejected=true;}
    Require(parseRejected && generation==42,"invalid INI accepted/published");
    {std::lock_guard<std::mutex> lock(EntryReload::mutex);EntryReload::pending=failing;}
    EntryReload::Prepare();
    Require(!EntryReload::pending && generation==42 && std::atomic_load(&published)==beforeGraph,"invalid worker candidate clears live generation or preserves superseded request");
    Save(temp/"mod.ini",edited);Save(sandbox/"plugin/eff.ini","[Graphics]\ndisable_camera_fade=invalid\n");
    EntryReload::Prepare();Require(!EntryReload::pending && settings.Get().reload.vk==VK_F8 && !settings.Get().disableCameraFade,"invalid global config loses prior valid settings");
    // A stale queued request cannot advance/overwrite the newer live generation.
    failing->base=41;{std::lock_guard<std::mutex> lock(EntryReload::mutex);EntryReload::pending=failing;}EntryReload::Commit();Require(generation==42,"stale candidate committed");
    // Candidate publication is consumed exactly at a new Unity frame.
    RuntimeShapes::api.frame=(void*)5;RuntimeShapes::api.elapsed=(void*)6;RuntimeShapes::lastFrame=-1;
    auto firstFrame=EntryReload::Validate(Graph(removed),config);
    {std::lock_guard<std::mutex> lock(EntryReload::mutex);EntryReload::pending=firstFrame;}
    RuntimeShapes::Update();Require(generation==43 && !EntryReload::pending,"new Unity frame does not commit queued reload");
    auto nextFrame=EntryReload::Validate(Graph(removed),config);
    {std::lock_guard<std::mutex> lock(EntryReload::mutex);EntryReload::pending=nextFrame;}
    RuntimeShapes::Update();Require(generation==43 && EntryReload::pending==nextFrame,"duplicate frame commits queued reload twice");
    ++frame;RuntimeShapes::Update();Require(generation==44 && !EntryReload::pending,"following Unity frame does not commit queued reload");
    Require(RuntimeShapes::groups.front().snapshots.front().mesh.handle==snapshotHandle,"frame commit retires native capture snapshot");
    RuntimeShapes::groups.clear();RuntimeShapes::bound.clear();snapshotHandle.reset();Require(handles.empty(),"fixture leaks GC handles");
    RuntimeControls::preferences.Flush(Log,true);
    std::filesystem::remove(temp/"state.ini");std::filesystem::remove(temp/"mod.ini");std::set<std::filesystem::path> folders;
    for(const auto& file:original.resources) {auto path=temp/std::filesystem::u8path(file.first);std::filesystem::remove(path);folders.insert(path.parent_path());}
    for(const auto& folder:folders) std::filesystem::remove(folder);std::filesystem::remove(temp);
    std::filesystem::remove(sandbox/"plugin/mods");std::filesystem::remove(sandbox/"plugin/eff.ini");std::filesystem::remove(sandbox/"plugin");std::filesystem::remove(sandbox);
    std::cout<<"PASS entry configuration reload: actual LZY INI/v3 table; all 43 declared identities; worker/main publication and actual frame deduplication; keys/constant visibility; live persist/domain/defaults; removed shape reset; held edges; injected AFTER-write rollback; stale/invalid/reserved/resource contracts rejected; active snapshots retained; 60 loading rows retained\n";
  } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
