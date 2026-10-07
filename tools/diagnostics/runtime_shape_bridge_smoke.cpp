#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
namespace Test {
using namespace Entry;
unsigned captures=0,releases=0,resets=0,ticks=0,frameTicks=0,inits=0,getters=0;
unsigned shapeCounts=0,filterSets=0,matrixRequests=0;
int shapeResult=2,matrixSize=0;
bool matrixResult=false;
void *lastSelf=nullptr,*lastRoot=nullptr,*lastInfo=nullptr;
int lastMask=0;
bool observeScope=false;
unsigned char mainThread=1;
struct ArrayHeader {void *klass=nullptr,*monitor=nullptr,*bounds=nullptr;uint64_t count=2;};
template<class T> struct PaletteArray {ArrayHeader header;T values[2]{};};
PaletteArray<void*> boneArray;
PaletteArray<Matrix4x4> poseArray;
std::map<void*,Matrix4x4> worlds;
Matrix4x4 rendererInverse{},transformInverse{},captureInverse{};
void* skinningRoot=(void*)0xB0;
std::map<uint32_t,void*> handles;
uint32_t nextHandle=0;
int frame=7;float elapsed=.25f,weight=0;unsigned weightWrites=0;
void* controlledMesh=nullptr;
struct MeshFacts {int vertices=0,shapes=0;std::vector<unsigned char> name;};
std::map<void*,MeshFacts> meshes;
std::map<void*,void*> renderMeshes;
std::map<void*,unsigned char> renderEnabled;
unsigned enabledWrites=0;
struct RendererArray {ArrayHeader header;void* values[128]{};} modelRenderers[2];
void Enabled(void* renderer,bool value) {renderEnabled[renderer]=value;++enabledWrites;}
void MakeMesh(void* mesh,const RuntimeShapes::Spec& spec) {
  MeshFacts facts;facts.vertices=int(spec.vertices);facts.shapes=int(spec.channels.size());
  facts.name.resize(20+spec.marker.size()*2);int length=int(spec.marker.size());
  std::memcpy(facts.name.data()+16,&length,4);
  for(size_t i=0;i<spec.marker.size();++i) {wchar_t c=spec.marker[i];std::memcpy(facts.name.data()+20+i*2,&c,2);}
  meshes[mesh]=std::move(facts);
}
void* Invoke(void* method,void* self,void** args,void** exception) {
  *exception=nullptr;
  if (method==(void*)0x210) return &boneArray;
  if (method==(void*)0x211) return &poseArray;
  if (method==(void*)0x212) return (void*)0xA0;
  if (method==(void*)0x213) return (void*)0xA0;
  if (method==(void*)0x214) return &worlds.at(self);
  if (method==(void*)0x215) return self==(void*)0xB0?&captureInverse:
    self==(void*)0xA0?&transformInverse:&rendererInverse;
  if (method==(void*)0x216) return skinningRoot;
  if (method==(void*)0x301) return &frame;
  if (method==(void*)0x302) return &elapsed;
  if (method==(void*)0x303) return renderMeshes.count(self)?renderMeshes.at(self):controlledMesh;
  if (method==(void*)0x306) return meshes.at(self).name.data();
  if (method==(void*)0x307) return &meshes.at(self).vertices;
  if (method==(void*)0x308) return &renderEnabled.at(self);
  if (method==(void*)0x309) return &meshes.at(self).shapes;
  if (method==(void*)0x310) {Require((self==(void*)0x312||self==(void*)0x313)&&args&&args[0]==(void*)0x311&&*(unsigned char*)args[1]==1,"controls queried outside the completed model or omitted inactive children");return &modelRenderers[self==(void*)0x313?1:0];}
  if (method==(void*)0x304) return &weight;
  if (method==(void*)0x305) {weight=*(float*)args[1];++weightWrites;return nullptr;}
  return method==(void*)2?lastRoot:&mainThread;
}
void* Class(void* object) {return object==&boneArray?(void*)0x201:(void*)0x202;}
void* Element(void* klass) {return klass==(void*)0x201?(void*)0x203:(void*)0x204;}
int32_t ValueSize(void* klass,uint32_t* alignment) {
  *alignment=4;return klass==(void*)0x204?64:72; // Transform references are NOT value-sized 8 bytes.
}
uint32_t NewHandle(void* object,bool) {handles[++nextHandle]=object;return nextHandle;}
void* HandleTarget(uint32_t handle) {return handles.at(handle);}
void FreeHandle(uint32_t handle) {handles.erase(handle);}
void* Unbox(void* object) { return object; }
void* Capture(void* self,void* root,int mask,void* info) {
  ++captures;lastSelf=self;lastRoot=root;lastMask=mask;lastInfo=info;
  if(observeScope) {
    Require(RuntimeShapes::scope!=nullptr,"capture TLS scope absent");
    RuntimeShapes::Finished(self,false);
    Require(!RuntimeShapes::scope->ready.at(self),"false native readiness forced true");
    RuntimeShapes::Finished(self,true);
    Require(RuntimeShapes::scope->ready.at(self),"true native readiness lost");
  }
  return (void*)0x1040;
}
void Release(void* self,void* entity,void* info) { ++releases;lastSelf=self;lastRoot=entity;lastInfo=info; }
void Reset(void* self,void* info) { ++resets;lastSelf=self;lastInfo=info; }
void Tick(void* self,void* info) { ++ticks;lastSelf=self;lastInfo=info; }
void FrameTick(void* self,void* info) { ++frameTicks;lastSelf=self;lastInfo=info; }
UINT downKey=0,downModifiers=0;
bool KeyDown(const EiemKeyChord& key) {return key.vk==downKey && (downModifiers&key.modifiers)==key.modifiers;}
void Init(void* self,void* roots,void*,void*,bool,void* info) { ++inits;lastSelf=self;lastRoot=roots;lastInfo=info; }
void* Getter(void* self) { ++getters;lastSelf=self;return renderMeshes.count(self)?renderMeshes.at(self):controlledMesh?controlledMesh:(void*)0xCC00; }
int ShapeCount(void* self) { ++shapeCounts;lastSelf=self;return shapeResult; }
void FilterSetter(void* self,void* mesh) { ++filterSets;lastSelf=self;lastRoot=mesh; }
bool Matrices(void* self,void* pointer,int size) {
  ++matrixRequests;lastSelf=self;lastRoot=pointer;matrixSize=size;return matrixResult;
}
unsigned gpuTriggers=0,gpuFiles=0;
void RENDERDOC_CC TriggerGpu() { ++gpuTriggers; }
uint32_t RENDERDOC_CC GpuCount() { return gpuFiles; }
uint32_t RENDERDOC_CC GpuActive() { return 0; }
void RENDERDOC_CC GpuPrefix(const char*) {}
uint32_t RENDERDOC_CC GpuSaved(uint32_t index,char* path,uint32_t* length,uint64_t* timestamp) {
  if (index!=0 || !gpuFiles) return 0;
  const char file[]="missing-fixture-frame.rdc";
  if (path) std::memcpy(path,file,sizeof(file));
  if (length) *length=sizeof(file);
  if (timestamp) *timestamp=1234;
  return 1;
}
}
int main(int argc,char** argv) {
  using namespace Entry;
  try {
    RuntimeShapes::originalCapture=Test::Capture;RuntimeShapes::originalRelease=Test::Release;
    RuntimeShapes::originalReset=Test::Reset;RuntimeShapes::originalTick=Test::Tick;
    RuntimeShapes::originalFrameTick=Test::FrameTick;
    RuntimeShapes::originalInit=Test::Init;RuntimeShapes::originalMeshGetter=Test::Getter;
    RuntimeShapes::originalShapeCount=Test::ShapeCount;RuntimeShapes::originalFilterSetter=Test::FilterSetter;
    RuntimeShapes::originalMatrixRequest=Test::Matrices;
    void *self=(void*)0xA1,*root=(void*)0xB2,*info=(void*)0xC3;
    RuntimeShapes::enabled=false;
    Require(RuntimeShapes::Capture(self,root,0x11223344,info)==(void*)0x1040 && Test::captures==1 &&
            Test::lastSelf==self && Test::lastRoot==root && Test::lastInfo==info && Test::lastMask==0x11223344,
            "disabled capture changes native call/ABI/result");
    Require(RuntimeShapes::GetMesh(self)==(void*)0xCC00 && Test::getters==1,"disabled Mesh getter changes result");
    Require(RuntimeShapes::ShapeCount(root)==2 && Test::shapeCounts==1,"disabled gate probe changes count");
    RuntimeShapes::SetFilterMesh(self,root);
    Require(Test::filterSets==1 && Test::lastSelf==self && Test::lastRoot==root,"disabled assignment probe changes arguments");
    Require(!RuntimeShapes::RequestMatrices(self,root,144) && Test::matrixRequests==1 && Test::matrixSize==144,
            "disabled matrix probe changes false result/ABI");
    RuntimeShapes::enabled=true; RuntimeShapes::api.main=nullptr;
    Require(RuntimeShapes::Capture(self,root,-7,info)==(void*)0x1040 && Test::captures==2,
            "failed main-thread API drops/repeats original capture");
    il2cpp_runtime_invoke=Test::Invoke; il2cpp_object_unbox=Test::Unbox; RuntimeShapes::api.main=(void*)1;
    Test::observeScope=true;
    Require(RuntimeShapes::scope==nullptr,"capture TLS leaked before call");
    Require(RuntimeShapes::Capture(self,root,-7,info)==(void*)0x1040 && Test::captures==3 &&
            RuntimeShapes::scope==nullptr && RuntimeShapes::groups.empty(),"capture scope/return/empty ownership");
    RuntimeShapes::GetMesh(self);Require(Test::getters==2,"outside-capture getter called twice");
    { RuntimeShapes::CaptureScope outer;
      auto saved=RuntimeShapes::scope;
      RuntimeShapes::Capture(self,root,2,info);
      Require(RuntimeShapes::scope==saved && Test::captures==4,"nested capture TLS restoration");
      RuntimeShapes::building=true;
      Require(RuntimeShapes::GetMesh(self)==(void*)0xCC00 && Test::getters==3,"snapshot builder getter recursion");
      RuntimeShapes::building=false;
    }
    RuntimeShapes::Release(self,root,info); RuntimeShapes::Reset(self,info);
    RuntimeShapes::Tick(self,info); // Missing frame API is caught after the native tick.
    RuntimeShapes::FrameTick(self,info);
    RuntimeShapes::Init(self,nullptr,nullptr,nullptr,false,info);
    Require(Test::releases==1 && Test::resets==1 && Test::ticks==1 && Test::frameTicks==1 && Test::inits==1 && Test::lastSelf==self && Test::lastInfo==info,
            "native lifecycle callbacks dropped/repeated");
    RuntimeShapes::traceBoundaries=true;RuntimeShapes::api.name=nullptr;RuntimeShapes::api.filterMesh=(void*)2;
    { RuntimeShapes::CaptureScope observed;
      observed.supplied[self]=root;
      // A positive count must still be returned verbatim: the probe does not
      // bypass the game gate or pretend that an authored Mesh has no shapes.
      Require(RuntimeShapes::ShapeCount(root)==2 && Test::shapeCounts==2 && Test::lastSelf==root,
              "active gate probe overrides a positive native count");
      Test::shapeResult=0;
      Require(RuntimeShapes::ShapeCount(root)==0 && Test::shapeCounts==3,"active gate probe overrides zero count");
      RuntimeShapes::SetFilterMesh(self,root);
      Require(Test::filterSets==2 && Test::lastRoot==root,"active assignment repeats/changes native setter");
      Require(!RuntimeShapes::RequestMatrices(self,root,256) && Test::matrixRequests==2,
              "active matrix probe forces success");
      Test::matrixResult=true;
      Require(RuntimeShapes::RequestMatrices(self,root,512) && Test::matrixRequests==3 &&
              Test::lastSelf==self && Test::lastRoot==root && Test::matrixSize==512,
              "active matrix probe changes true result/ABI");
    }
    auto samples=RuntimeShapes::boundarySamples;
    RuntimeShapes::ShapeCount(root);RuntimeShapes::SetFilterMesh(self,root);RuntimeShapes::RequestMatrices(self,root,1024);
    Require(Test::shapeCounts==4 && Test::filterSets==3 && Test::matrixRequests==4 && RuntimeShapes::boundarySamples==samples,
            "outside-Capture boundary probes modify/count unrelated native calls");
    RuntimeShapes::traceBoundaries=false;
    // Exercise the actual public API reader as well as the request adapter.
    // Two Mesh-local slots use distinct transforms and bind poses; reference
    // array layout must not be confused with Transform's object value size.
    {
      using namespace EiemSkinProbe;
      auto api=RuntimeShapes::api;
      il2cpp_object_get_class=Test::Class;il2cpp_class_get_element_class=Test::Element;
      il2cpp_class_value_size=Test::ValueSize;il2cpp_gchandle_new=Test::NewHandle;
      il2cpp_gchandle_get_target=Test::HandleTarget;il2cpp_gchandle_free=Test::FreeHandle;RuntimeShapes::Ref::weakHandle=Test::NewHandle;
      RuntimeShapes::api.bones=(void*)0x210;RuntimeShapes::api.bindPoses=(void*)0x211;
      RuntimeShapes::api.rootBone=(void*)0x212;RuntimeShapes::api.transform=(void*)0x213;
      RuntimeShapes::api.localToWorld=(void*)0x214;RuntimeShapes::api.worldToLocal=(void*)0x215;
      RuntimeShapes::api.skinningRoot=(void*)0x216;
      RuntimeShapes::api.alive=(void*)1;RuntimeShapes::api.transformClass=(void*)0x203;
      auto unity=[](const Matrix& matrix) {Matrix4x4 out{};std::memcpy(out.m,matrix.m,sizeof(out.m));return out;};
      Test::boneArray.values[0]=(void*)0xA2;Test::boneArray.values[1]=(void*)0xA1;
      Test::worlds[(void*)0xA0]=unity(Translation(10,20,30));
      Test::worlds[(void*)0xA1]=unity(Translation(12,20,30));
      Test::worlds[(void*)0xA2]=unity(Translation(10,24,30));
      Test::captureInverse=unity(Translation(-10,-20,-30));
      // Both source Renderer and its Transform can be a quarter turn from
      // the native capture parent. The inspected game parents the snapshot
      // under skinningRoot; it falls back to rootBone when absent.
      const auto quarterTurn=Matrix{{1,0,0,0,0,0,1,0,0,-1,0,0,0,0,0,1}};
      Test::transformInverse=unity(Multiply(quarterTurn,Translation(-10,-20,-30)));
      Test::rendererInverse=Test::transformInverse;
      Test::poseArray.values[0]=unity(Translation(0,-2,0));Test::poseArray.values[1]=unity(Translation(-1,0,0));
      auto palette=RuntimeShapes::CapturePaletteFor(self,root);
      Require(palette.size()==3 && palette[0][3]==10 && palette[1][7]==2 && palette[2][3]==1 && Test::handles.empty(),
              "body afterimage ignores native skinningRoot and uses rotated Renderer frame");
      Test::skinningRoot=nullptr;
      auto fallback=RuntimeShapes::CapturePaletteFor(self,root);
      Require(fallback[1][7]==0 && fallback[1][11]==2 && fallback[2][3]==1,
              "capture fallback does not use actual rootBone parent frame");
      Test::skinningRoot=(void*)0xB0;
      bool rejected=false;Test::poseArray.header.count=1;
      try {RuntimeShapes::CapturePaletteFor(self,root);}catch(...){rejected=true;}
      Require(rejected && Test::handles.empty(),"public palette reader accepts different bind-pose count");
      Test::poseArray.header.count=2;RuntimeShapes::api=api;
    }
    // Real v21 failure pattern: native Request returns false, but Capture
    // already assigned the shape-free Mesh. A zero palette collapses its
    // entire post-VS mesh. Only a complete instance/slot-specific palette
    // permits synchronous success; never override an accepted native job.
    {
      using namespace EiemSkinProbe;
      RuntimeShapes::CaptureScope capture;
      capture.ready[self]=true;capture.supplied[self]=root;
      const auto inv=Translation(-10,-20,-30);
      auto palette=EiemCaptureSkin::Build(Translation(10,20,30),inv,
        {Translation(12,20,30),Translation(10,24,30)},
        {Translation(-1,0,0),Translation(0,-2,0)});
      Require(palette.size()==3 && palette[0][3]==10 && palette[1][3]==1 && palette[2][7]==2,
              "capture matrix local space/root/slot ordering");
      capture.capturePalettes[self]=palette;
      std::array<unsigned char,160> buffer{};buffer.fill(0xA5);
      Test::matrixResult=false;auto calls=Test::matrixRequests;
      Require(RuntimeShapes::RequestMatrices(self,buffer.data()+8,144),
              "refused native shaped request leaves zero afterimage palette");
      Require(Test::matrixRequests==calls+1 && Test::lastSelf==self && Test::lastRoot==buffer.data()+8 &&
              Test::matrixSize==144 && std::memcmp(buffer.data()+8,palette.data(),144)==0,
              "capture fallback ABI/palette payload differs");
      Require(buffer.front()==0xA5 && buffer.back()==0xA5,"capture fallback wrote outside caller payload");
      buffer.fill(0xA5);
      Require(!RuntimeShapes::RequestMatrices(self,buffer.data()+8,143) &&
              !RuntimeShapes::RequestMatrices(self,nullptr,144),"capture fallback accepts wrong extent/null");
      Require(std::all_of(buffer.begin(),buffer.end(),[](auto x){return x==0xA5;}),"invalid capture request modifies buffer");
      Test::matrixResult=true;
      Require(RuntimeShapes::RequestMatrices(self,buffer.data()+8,144) &&
              std::all_of(buffer.begin(),buffer.end(),[](auto x){return x==0xA5;}),"capture fallback overwrites pending native job");
      Test::matrixResult=false;
      capture.ready[self]=false;
      Require(!RuntimeShapes::RequestMatrices(self,buffer.data()+8,144),"capture fallback bypasses pending native request");
      Require(!RuntimeShapes::RequestMatrices(root,buffer.data()+8,144),"capture fallback changes unrelated renderer");
      bool rejected=false;
      try {EiemCaptureSkin::Build(EiemSkinProbe::Identity(),inv,{EiemSkinProbe::Identity()},
             {EiemSkinProbe::Identity(),EiemSkinProbe::Identity()});}catch(...){rejected=true;}
      Require(rejected,"capture palette shares a different Mesh slot count");
      auto invalid=palette;invalid[1][0]=std::numeric_limits<float>::quiet_NaN();
      Require(!EiemCaptureSkin::Deliver(invalid,buffer.data()+8,144),"capture fallback delivers nonfinite palette");
      const auto rotated=Matrix{{0,1,0,0,-1,0,0,0,0,0,1,0,5,6,7,1}};
      const auto rows=EiemCaptureSkin::RowMatrix(rotated);
      Require(rows==EiemCaptureSkin::Rows{0,-1,0,5,1,0,0,6,0,0,1,7},"capture matrix row/column conversion");
    }
    // Input sampling continues on regular frames without any Capture call.
    // Check time-based holds, release/focus behavior and cycle edges through
    // the actual polling function used by Update.
      {
        const auto variables=RuntimeShapes::variables;
        const auto keys=RuntimeShapes::keys;const auto held=RuntimeShapes::held;
        const auto selected=RuntimeControls::selected;RuntimeControls::selected="fixture";
      RuntimeShapes::keys.clear();RuntimeShapes::variables.clear();RuntimeShapes::held.clear();
      RuntimeShapes::variables["fixture"]={{"shape",0},{"switch",0}};
      RuntimeShapes::keys["fixture"]={
        {"increase","shape","hold",{VK_UP,0},{1},1},
        {"decrease","shape","hold",{VK_DOWN,0},{0},1},
        {"cycle","switch","cycle",{VK_NUMPAD1,0},{0,1,2},1}};
      Test::downKey=VK_UP;
      for (int frame=0;frame<4;++frame) RuntimeShapes::PollControls(true,.25,Test::KeyDown);
      Require(RuntimeShapes::variables["fixture"]["shape"]==1,"regular hold does not reach target");
      Test::downKey=VK_DOWN;RuntimeShapes::PollControls(false,.5,Test::KeyDown);
      Require(RuntimeShapes::variables["fixture"]["shape"]==1,"unfocused input changes shape");
      RuntimeShapes::PollControls(true,.5,Test::KeyDown);
      Require(RuntimeShapes::variables["fixture"]["shape"]==.5,"hold decrease lost elapsed time");
      Test::downKey=VK_NUMPAD1;RuntimeShapes::PollControls(true,.25,Test::KeyDown);
      RuntimeShapes::PollControls(true,.25,Test::KeyDown);
      Require(RuntimeShapes::variables["fixture"]["switch"]==1,"cycle repeats while key is held");
      RuntimeShapes::PollControls(false,.25,Test::KeyDown);RuntimeShapes::PollControls(true,.25,Test::KeyDown);
      Require(RuntimeShapes::variables["fixture"]["switch"]==1,"focus return manufactures a held-key press");
      Test::downKey=0;RuntimeShapes::PollControls(false,.25,Test::KeyDown);
      Test::downKey=VK_NUMPAD1;RuntimeShapes::PollControls(true,.25,Test::KeyDown);
      Require(RuntimeShapes::variables["fixture"]["switch"]==2,"cycle loses physical release edge");
        RuntimeShapes::variables=variables;RuntimeShapes::keys=keys;RuntimeShapes::held=held;
        RuntimeControls::selected=selected;
    }
    // Exercise the regular frame hook through Update and the actual weight
    // writer/readback. No Capture or capture-controller Tick occurs here.
    {
      const auto api=RuntimeShapes::api;const auto variables=RuntimeShapes::variables;
      const auto keys=RuntimeShapes::keys;const auto held=RuntimeShapes::held;
      const auto bound=RuntimeShapes::bound;const auto specs=RuntimeShapes::specs;
      const auto lastFrame=RuntimeShapes::lastFrame;
      const auto packageGeneration=RuntimeShapes::packageGeneration;
      const auto boundCount=RuntimeShapes::boundCount.load();
      RuntimeShapes::bound.clear();RuntimeShapes::specs.clear();RuntimeShapes::keys.clear();
      RuntimeShapes::variables={{"fixture",{{"shape",.75}}}};
      RuntimeShapes::specs["marker"]={"fixture","part","marker",1,{"bend"},{{"bend","shape"}}};
      RuntimeShapes::bound[self]={RuntimeShapes::Ref::Make(self,false),RuntimeShapes::Ref::Make(root,false),"marker",{{"bend",0}}};
      RuntimeShapes::api.frame=(void*)0x301;RuntimeShapes::api.elapsed=(void*)0x302;
      RuntimeShapes::api.mesh=(void*)0x303;RuntimeShapes::api.getWeight=(void*)0x304;
      RuntimeShapes::api.setWeight=(void*)0x305;RuntimeShapes::api.alive=(void*)1;
      RuntimeShapes::lastFrame=-1;RuntimeShapes::packageGeneration=generation.load();
      Test::controlledMesh=root;Test::weight=0;Test::weightWrites=0;
      auto nativeTicks=Test::frameTicks;
      RuntimeShapes::FrameTick(self,info);
      Require(Test::frameTicks==nativeTicks+1 && Test::lastSelf==self && Test::lastInfo==info &&
              Test::weightWrites==1 && Test::weight==75,"regular frame fails to apply/read back shape weight without Capture");
      RuntimeShapes::variables["fixture"]["shape"]=.25;
      RuntimeShapes::FrameTick(self,info);
      Require(Test::weightWrites==1 && Test::weight==75,"same Unity frame updates controls twice");
      ++Test::frame;RuntimeShapes::FrameTick(self,info);
      Require(Test::weightWrites==2 && Test::weight==25,"next regular frame fails to update shape");
      RuntimeShapes::bound=bound;RuntimeShapes::specs=specs;RuntimeShapes::api=api;
      RuntimeShapes::variables=variables;RuntimeShapes::keys=keys;RuntimeShapes::held=held;
      RuntimeShapes::lastFrame=lastFrame;RuntimeShapes::packageGeneration=packageGeneration;
      RuntimeShapes::boundCount.store(boundCount);
      Require(Test::handles.empty(),"regular frame test leaks managed handles");
    }
    // Integration against the actual format-2 package: model completion,
    // provenance registration, every switch declaration, per-instance writes,
    // native enable arbitration, physical edges and preference round-trip.
    if (argc==2) {
      auto package=EiemDirectory::Load(std::filesystem::u8path(argv[1]));
      auto temp=std::filesystem::absolute("diagnostics/resource-directory-entry-20261003/v25-part-key-controls/state-fixture-"+std::to_string(GetCurrentProcessId()));
      std::filesystem::create_directories(temp);auto authorRoot=package.root;
      std::filesystem::copy_file(authorRoot/"mod.ini",temp/"mod.ini",std::filesystem::copy_options::overwrite_existing);
      for(const auto& resource:package.resources) {
        auto path=temp/std::filesystem::u8path(resource.first);std::filesystem::create_directories(path.parent_path());
        std::filesystem::copy_file(authorRoot/std::filesystem::u8path(resource.first),path,std::filesystem::copy_options::overwrite_existing);
      }
      package.root=temp;
      package.defaults["$press_test"]=0;
      package.bindings.push_back({"pressFixture","$press_test","press",{'K',MOD_CONTROL},{7},1});
      auto graph=std::make_shared<const std::vector<Package>>(std::vector<Package>{package});
      const auto savedApi=RuntimeShapes::api;const auto savedSpecs=RuntimeShapes::specs;
      const auto savedVariables=RuntimeShapes::variables;const auto savedKeys=RuntimeShapes::keys;
      const auto savedHeld=RuntimeShapes::held;const auto savedGraph=RuntimeShapes::controlGraph;
      const auto savedSetter=RuntimeShapes::originalEnabledSetter;
      RuntimeShapes::LoadControls(graph);
      Require(RuntimeShapes::keys.at(package.id).size()==package.bindings.size(),"non-shape package bindings omitted");
      RuntimeShapes::api.name=(void*)0x306;RuntimeShapes::api.mesh=(void*)0x303;
      RuntimeShapes::api.vertexCount=(void*)0x307;RuntimeShapes::api.getEnabled=(void*)0x308;
      RuntimeShapes::api.shapeCount=(void*)0x309;RuntimeShapes::api.modelRenderers=(void*)0x310;
      RuntimeShapes::api.rendererType=(void*)0x311;RuntimeShapes::api.alive=(void*)1;
      RuntimeShapes::originalEnabledSetter=Test::Enabled;
      std::set<std::string> parts;std::map<std::string,void*> firstRenderer;
      Test::modelRenderers[0].header.count=Test::modelRenderers[1].header.count=0;uintptr_t identity=0x10000;
      for (const auto& row:RuntimeShapes::specs) {
        const auto& spec=row.second;
        if (!spec.visible || !RuntimeVisibility(*spec.visible) || !parts.insert(spec.part).second) continue;
        auto mesh=(void*)identity++;Test::MakeMesh(mesh,spec);
        for(int instance=0;instance<2;++instance) {
          auto renderer=(void*)identity++;Test::renderMeshes[renderer]=mesh;Test::renderEnabled[renderer]=1;
          auto& model=Test::modelRenderers[instance];Require(model.header.count<128,"fixture model renderer extent");model.values[model.header.count++]=renderer;
          if (!instance) firstRenderer[spec.part]=renderer;
        }
      }
      // EntityMaterialController.Init can run before the model part captures
      // its Renderer enabled baseline. Authored defaults must not contaminate
      // that native snapshot or later SetRendererVisible will replay them as
      // native disable requests and permanently suppress hidden parts.
      auto initializeController=[&] {
      const auto savedClass=il2cpp_object_get_class;
      const auto savedFields=il2cpp_class_get_fields;
      const auto savedParent=il2cpp_class_get_parent;
      const auto savedFieldName=il2cpp_field_get_name,savedClassName=il2cpp_class_get_name,savedNamespace=il2cpp_class_get_namespace;
      const auto savedFieldValue=il2cpp_field_get_value;
      il2cpp_object_get_class=[](void* object)->void*{return Test::renderMeshes.count(object)?(void*)0x314:(void*)0x315;};
      il2cpp_class_get_fields=[](void*,void** iterator)->void*{auto index=uintptr_t(*iterator);if(index>=2)return nullptr;*iterator=(void*)(index+1);return(void*)(index+1);};
      il2cpp_field_get_name=[](void* field)->const char*{return field==(void*)1?"_items":"_size";};
      il2cpp_field_get_value=[](void*,void* field,void* output){if(field==(void*)1)*(void**)output=&Test::modelRenderers[0];else *(int*)output=int(Test::modelRenderers[0].header.count);};
      il2cpp_class_get_parent=[](void*)->void*{return nullptr;};
      il2cpp_class_get_name=[](void* klass)->const char*{return klass==(void*)0x314?"SkinnedMeshRenderer":"fixture";};
      il2cpp_class_get_namespace=[](void* klass)->const char*{return klass==(void*)0x314?"UnityEngine":"fixture";};
      RuntimeShapes::Init((void*)0x316,(void*)0x317,nullptr,nullptr,false,nullptr);
      il2cpp_object_get_class=savedClass;il2cpp_class_get_fields=savedFields;il2cpp_class_get_parent=savedParent;
      il2cpp_field_get_name=savedFieldName;il2cpp_class_get_name=savedClassName;il2cpp_class_get_namespace=savedNamespace;il2cpp_field_get_value=savedFieldValue;
      };
      initializeController();
      Require(RuntimeShapes::bound.size()==parts.size(),"material initialization did not register provisional controls");
      for(const auto& binding:RuntimeShapes::bound){
        Require(Test::renderEnabled.at(binding.first),"material Init hid an author part before the game's initial Renderer snapshot");
        const auto& spec=RuntimeShapes::specs.at(binding.second.marker);
        RuntimeShapes::ApplyVisibility(binding.first,RuntimeShapes::bound.at(binding.first),spec);
        Require(Test::renderEnabled.at(binding.first),"a frame before model completion contaminated the native Renderer snapshot");
        RuntimeShapes::SetEnabled(binding.first,false);
        Require(!Test::renderEnabled.at(binding.first),"pending model lost a real native disable request");
        RuntimeShapes::SetEnabled(binding.first,true);
        Require(Test::renderEnabled.at(binding.first),"pending model filtered a native enable before baseline capture");
      }
      auto pendingControls=RuntimeControls::Prepare(graph,settings.Get());
      EntryReload::ApplyControls(pendingControls,Test::KeyDown);
      for(const auto& binding:RuntimeShapes::bound)
        Require(Test::renderEnabled.at(binding.first),"controls-only reload contaminated an unfinished model's enabled snapshot");
      // Native code can call the resolved Unity setter directly. Completion
      // must capture the resulting flag, even when our setter hook did not run.
      auto pendingDisabled=Test::modelRenderers[0].values[0];
      Test::Enabled(pendingDisabled,false);
      const auto nativeSnapshot=Test::renderEnabled;
      RuntimeShapes::RegisterModel((void*)0x312);
      Require(!parts.empty() && RuntimeShapes::bound.size()==parts.size(),"first model control registration incomplete");
      for(const auto& binding:RuntimeShapes::bound) {
        Require(binding.second.modelReady && binding.second.nativeEnabled==bool(nativeSnapshot.at(binding.first)),
                "model completion did not capture the game's final native enabled state");
        RuntimeShapes::SetEnabled(binding.first,nativeSnapshot.at(binding.first)!=0);
      }
      Require(!Test::renderEnabled.at(pendingDisabled),"completed model overrode a real native disable");
      RuntimeShapes::SetEnabled(pendingDisabled,true);
      // Repeated material init/model callbacks must preserve native intent,
      // rather than read an authored hidden flag back as native disabled.
      const auto completedFlags=Test::renderEnabled;
      std::map<void*,bool> completedIntent;
      for(const auto& binding:RuntimeShapes::bound) completedIntent[binding.first]=binding.second.nativeEnabled;
      initializeController();RuntimeShapes::RegisterModel((void*)0x312);
      Require(Test::renderEnabled==completedFlags,"late material Init changed completed control visibility");
      for(const auto& binding:RuntimeShapes::bound)
        Require(binding.second.modelReady && binding.second.nativeEnabled==completedIntent.at(binding.first),
                "repeated initialization treated authored visibility as native disabled");
      RuntimeShapes::RegisterModel((void*)0x313);
      Require(RuntimeShapes::bound.size()==parts.size()*2,"later model control registration incomplete");
      auto apply=[&] {
        for(auto& binding:RuntimeShapes::bound) RuntimeShapes::ApplyVisibility(binding.first,binding.second,RuntimeShapes::specs.at(binding.second.marker));
      };
      auto verify=[&] {
        for(const auto& binding:RuntimeShapes::bound) {
          const auto& spec=RuntimeShapes::specs.at(binding.second.marker);
          bool expected=binding.second.nativeEnabled && spec.visible->Evaluate(RuntimeShapes::variables.at(spec.package))!=0;
          Require(bool(Test::renderEnabled.at(binding.first))==expected,"key state differs from actual per-part public Renderer flag");
        }
      };
      auto press=[&](UINT key,UINT modifiers=0) {
        Test::downKey=0;RuntimeShapes::PollControls(true,.016,Test::KeyDown);
        Test::downKey=key;Test::downModifiers=modifiers;RuntimeShapes::PollControls(true,.016,Test::KeyDown);apply();verify();
      };
      for (const auto& key:package.bindings) if(key.type=="cycle") {
        for(size_t step=0;step<key.values.size();++step) {
          auto before=RuntimeShapes::variables.at(package.id).at(key.variable);
          auto at=std::find(key.values.begin(),key.values.end(),before);
          auto expected=++at==key.values.end()?key.values.front():*at;
          press(key.key.vk,key.key.modifiers);
          Require(RuntimeShapes::variables.at(package.id).at(key.variable)==expected,"declared cycle ordering lost");
          for(int repeat=0;repeat<4;++repeat) RuntimeShapes::PollControls(true,.016,Test::KeyDown);
          Require(RuntimeShapes::variables.at(package.id).at(key.variable)==expected,"held cycle repeats");
        }
      }
      press('K');Require(RuntimeShapes::variables.at(package.id).at("$press_test")==0,"modifier-free chord fires");
      press('K',MOD_CONTROL);Require(RuntimeShapes::variables.at(package.id).at("$press_test")==7,"press chord target omitted");
      press(VK_NUMPAD5);auto renderer=firstRenderer.at("PartS_actor_lizhiyan_cloth_03_lod0_3.lian");
      Require(!Test::renderEnabled.at(renderer),"NUMPAD5 does not hide lian");
      RuntimeShapes::SetEnabled(renderer,true);Require(!Test::renderEnabled.at(renderer),"native enable bypasses hidden author state");
      RuntimeShapes::SetEnabled(renderer,false);press(VK_NUMPAD5);
      Require(!Test::renderEnabled.at(renderer),"author switch re-enables native-disabled Renderer");
      RuntimeShapes::SetEnabled(renderer,true);Require(Test::renderEnabled.at(renderer),"native enable not restored for visible author part");
      press(VK_NUMPAD5);
      auto newRenderer=(void*)identity++;Test::renderMeshes[newRenderer]=Test::renderMeshes.at(renderer);Test::renderEnabled[newRenderer]=1;
      RuntimeShapes::RegisterRenderer(newRenderer,Test::renderMeshes.at(newRenderer));
      Require(!Test::renderEnabled.at(newRenderer),"new instance does not inherit current switch state");
      // Other native Renderer instances never enter the author filter.
      auto unrelated=(void*)identity++;RuntimeShapes::SetEnabled(unrelated,true);
      Require(Test::renderEnabled.at(unrelated),"unrelated native Renderer modified");
      RuntimeShapes::preferences.Flush(Log,true);
      EiemEntryPreferences disk;auto restored=disk.Load(package);
      auto reloaded=EiemDirectory::Load(temp);
      Require(EiemNativeInput::PackageDigest(reloaded)==EiemNativeInput::PackageDigest(package),"state.ini changes/rejects author package digest");
      // A refresh of the unchanged package retains live key state and bindings.
      auto oldPublished=std::atomic_load(&published);auto oldGeneration=generation.load();
      auto oldPackageGeneration=RuntimeShapes::packageGeneration,oldChecked=RuntimeShapes::checkedGeneration;
      std::atomic_store(&published,graph);generation=oldGeneration+1;
      RuntimeShapes::packageGeneration=oldGeneration;RuntimeShapes::checkedGeneration=oldGeneration;
      Require(RuntimeShapes::CurrentGeneration() && RuntimeShapes::variables.at(package.id).at("$switch_a2670be72e524621")==1,"unchanged refresh disables/resets switch state");
      std::atomic_store(&published,oldPublished);generation=oldGeneration;
      RuntimeShapes::packageGeneration=oldPackageGeneration;RuntimeShapes::checkedGeneration=oldChecked;
      for(const auto& key:package.persistent)
        Require(restored.at(key)==RuntimeShapes::variables.at(package.id).at(key),"persisted state failed restart round-trip");
      bool rejected=false;try {EiemEntryPreferences::Validate(package,{{"$switch_83e2d51e4c704061",9}});}catch(...){rejected=true;}
      Require(rejected,"invalid persisted multi-state domain accepted");
      RuntimeShapes::bound.clear();RuntimeShapes::api=savedApi;RuntimeShapes::specs=savedSpecs;
      RuntimeShapes::variables=savedVariables;RuntimeShapes::keys=savedKeys;RuntimeShapes::held=savedHeld;
      RuntimeShapes::controlGraph=savedGraph;RuntimeShapes::originalEnabledSetter=savedSetter;RuntimeShapes::boundCount=0;
      Test::renderMeshes.clear();Test::meshes.clear();Test::renderEnabled.clear();
      Require(Test::handles.empty(),"controlled Renderer lifecycle leaks managed handles");
      std::filesystem::remove(temp/"state.ini");std::filesystem::remove(temp/"mod.ini");
      std::set<std::filesystem::path> folders;
      for(const auto& resource:package.resources) {auto path=temp/std::filesystem::u8path(resource.first);std::filesystem::remove(path);folders.insert(path.parent_path());}
      for(const auto& folder:folders) std::filesystem::remove(folder);
      std::filesystem::remove(temp);
      std::cout<<"PASS actual package key integration: material Init/frame/controls reload preserve native baseline until model completion; native-disable/repeated-init preserved; 9 garments, 18 initial instances, all NUMPAD cycles including 3 states, no held repeat, modifier/press, public visibility readback, native enabled intent, late-instance state, unrelated Renderer, persistent restart round-trip/domain rejection\n";
    }
    RENDERDOC_API_1_6_0 gpu{};
    gpu.TriggerCapture=Test::TriggerGpu;gpu.GetNumCaptures=Test::GpuCount;gpu.GetCapture=Test::GpuSaved;
    gpu.SetCaptureFilePathTemplate=Test::GpuPrefix;gpu.IsFrameCapturing=Test::GpuActive;
    RuntimeShapes::gpuCaptureApi=&gpu;RuntimeShapes::gpuCaptureEnabled=true;
    RuntimeShapes::RequestGpuCapture(0);
    Require(!Test::gpuTriggers && !RuntimeShapes::gpuCaptureTriggered,"GPU capture triggered without native snapshot assignment");
    RuntimeShapes::RequestGpuCapture(1);RuntimeShapes::RequestGpuCapture(9);
    Require(Test::gpuTriggers==1 && RuntimeShapes::gpuCaptureTriggered,"GPU probe captures repeatedly instead of once");
    RuntimeShapes::PollGpuCapture();Require(!RuntimeShapes::gpuCaptureReported,"GPU save claimed before API completion");
    Test::gpuFiles=1;RuntimeShapes::PollGpuCapture();RuntimeShapes::PollGpuCapture();
    Require(RuntimeShapes::gpuCaptureReported && Test::gpuTriggers==1,"GPU save completion/one-shot state lost");
    RuntimeShapes::gpuCaptureEnabled=false;RuntimeShapes::gpuCaptureApi=nullptr;
    std::cout<<"PASS runtime shape bridge: native Capture ABI, readiness/TLS/lifecycle preserved; refused shaped matrix request receives complete synchronous palette; native skinningRoot frame correct despite rotated source Renderer; rootBone fallback; per-Mesh slots/row layout/extent; regular frame applies/readbacks shape without Capture; per-frame deduplication; hold/focus/cycle sampling; native pending jobs untouched; GPU one-shot completion\n";
    return 0;
  } catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<'\n';return 1; }
}
