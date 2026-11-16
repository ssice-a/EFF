#pragma once
#include "mod_animation_identity.h"

// Opt-in real-Mod experiment. EFF retains its exact clock, inputs and manual
// values. Native Clips are the only animation writers for the claimed shapes.
// Existing scalar interpolation remains a numerical oracle and UI projection.
namespace ModAnimationProbe {
using RuntimeShapes::Ref;
using RuntimeShapes::Call;
using RuntimeShapes::Scalar;
struct Target {
 std::string part,file,asset,hash;
 std::map<std::string,std::string> shapes;
 Ref bundle,clip;
 bool failed=false,quarantined=false;
};
struct Binding {
 Ref renderer,mesh,animator;
 std::string part;
 size_t samples=0,wraps=0;
 double lastTime=0,maxError=0;
 bool paused=false;
};
std::map<std::string,Target> targets;
std::map<void*,Binding> bindings;
NativeAnimationProbe::Api api;
bool prepared=false,prepareFailed=false;
std::string package,animation;
std::filesystem::path packageRoot;
double duration=0;
uint64_t revision=~uint64_t(0);
std::filesystem::path Root(){return gameDirectory/L"plugin/diagnostics/zfy-native-animation";}
void Forget(void* renderer){
 auto found=bindings.find(renderer);if(found==bindings.end())return;
 auto& b=found->second;
 if(RuntimeShapes::Alive(b.animator.Get())){
  unsigned char assets=0;void* args[]={b.animator.Get(),&assets};
  try{Call(api.destroy,nullptr,args);}catch(const std::exception& e){
   // Keep both consumer and its Bundle when native retirement is uncertain.
   targets.at(b.part).failed=targets.at(b.part).quarantined=true;
   Log("[MOD-ANIMATION-PROBE] phase=retire result=FAIL part=%s reason=%s bundleRetained=1",b.part.c_str(),e.what());return;
  }
 }
 Log("[MOD-ANIMATION-PROBE] phase=retire part=%s renderer=%p samples=%zu wraps=%zu maxError=%.9g ownedAnimatorRemoved=1",
     b.part.c_str(),renderer,b.samples,b.wraps,b.maxError);
 bindings.erase(found);
}
void BeginFrame(){
 if(revision!=RuntimeControls::revision){
  // F10 can publish new renderer/mesh generations. Release sampling consumers
  // first, then their Clip bundles, on the same Unity thread.
  std::vector<void*> retiring;for(const auto& row:bindings)retiring.push_back(row.first);
  for(auto r:retiring)Forget(r);
  for(auto& row:targets)if(row.second.bundle.Get()&&!row.second.quarantined){
   NativeBundle::Unload(row.second.bundle.Get(),true);row.second.clip={};row.second.bundle={};
  }
  revision=RuntimeControls::revision;
  if(prepared)Log("[MOD-ANIMATION-PROBE] phase=generation revision=%llu bindingsRebuilt=1",(unsigned long long)revision);
 }
 std::vector<void*> retiring;
 for(const auto& row:bindings){const auto& b=row.second;auto current=RuntimeShapes::bound.find(row.first);
  if(!active||!RuntimeShapes::Alive(b.renderer.Get())||!RuntimeShapes::Alive(b.mesh.Get())||
     current==RuntimeShapes::bound.end()||current->second.renderer.Get()!=b.renderer.Get()||
     current->second.mesh.Get()!=b.mesh.Get())retiring.push_back(row.first);
 }
 for(auto r:retiring)Forget(r);
}
void Prepare(){
 auto sections=EiemDirectory::ParseIni(EiemDirectory::Read(Root()/L"experiment.ini"));
 for(const auto& section:sections){const auto& v=section.values;
  if(section.id=="Experiment"){
   package=v.at("package");animation=v.at("animation");packageRoot=std::filesystem::u8path(v.at("package_root"));
   duration=std::stod(v.at("duration"));Require(std::isfinite(duration)&&duration>0,"invalid Mod Clip duration");
  }else if(section.id.rfind("Target",0)==0){
   Target t;t.part=v.at("part");t.file=v.at("file");t.asset=v.at("asset");t.hash=v.at("sha256");
   Require(std::filesystem::u8path(t.file).filename()==std::filesystem::u8path(t.file)&&t.hash.size()==64,"unsafe Mod Clip input");
   for(const auto& field:v)if(field.first.rfind("shape.",0)==0)t.shapes.emplace(field.first.substr(6),field.second);
   Require(!t.shapes.empty()&&targets.emplace(t.part,std::move(t)).second,"invalid Mod Clip target");
  }else throw std::runtime_error("unknown Mod experiment section");
 }
 Require(!package.empty()&&!animation.empty()&&targets.size()==2&&packageRoot.is_absolute()&&RuntimeControls::controlGraph,"incomplete Mod experiment");
 const auto& p=EiemModAnimationExperiment::Installed(*RuntimeControls::controlGraph,packageRoot,package);
 package=p.id;const auto& clip=p.shapeAnimations.at(animation);
 Require(std::abs(clip.duration-duration)<1e-6&&clip.boneTracks.empty()&&!clip.sampleSource&&!p.program,"unsupported Mod experiment timeline/program");
 for(const auto& row:targets)for(const auto& shape:row.second.shapes)
  Require(p.parts.at(row.first).shapes.at(shape.first)==shape.second&&clip.tracks.count(shape.second),"Mod experiment shape mapping differs");
 api=NativeAnimationProbe::Bind();prepared=true;
 Log("[MOD-ANIMATION-PROBE] phase=ready package=%s animation=%s duration=%.9g targets=%zu clock=EFF keyAndIns=existing realRenderer=1",
     package.c_str(),animation.c_str(),duration,targets.size());
}
void Load(Target& target){
 if(target.clip.Get())return;
 auto bytes=EiemDirectory::Read(Root()/std::filesystem::u8path(target.file));
 Require(!bytes.empty()&&bytes.size()<=1024*1024,"Mod Clip bundle budget differs");
 auto hash=EiemDirectory::Sha(bytes.data(),bytes.size());
 Require(EiemDirectory::Hex(hash.data(),hash.size())==target.hash,"Mod Clip bundle digest differs");
 auto owner=std::make_shared<const EiemNativeInput::Bytes>(std::move(bytes));
 target.bundle=Ref::Make(NativeBundle::LoadBytes(owner,0,false),false);Require(target.bundle.Get(),"Mod Clip bundle load failed");
 auto name=Ref::Make(il2cpp_string_new(target.asset.c_str()),false);void* args[]={name.Get(),api.clipType.Get()};
 target.clip=Ref::Make(Call(api.loadAsset,target.bundle.Get(),args),false);Require(target.clip.Get(),"Mod Clip asset load failed");
 Require(std::abs(Scalar<float>(api.length,target.clip.Get())-duration)<1e-5,"Mod native duration differs");
 Require(!Scalar<unsigned char>(api.legacy,target.clip.Get()),"Mod Clip must use verified non-Legacy backend");
}
std::set<std::string> Apply(void* renderer,void* mesh,const RuntimeControls::Spec& spec){
 std::set<std::string> claimed;
 if(!active||prepareFailed||!NativeBundle::enabled.load())return claimed;
 auto owner=RuntimeControls::controlOwners.find(spec.package);
 if(owner==RuntimeControls::controlOwners.end()||EiemDirectory::DeclaredId(*owner->second)!="ZFY")return claimed;
 try{
  if(!prepared)Prepare();
  auto found=targets.find(spec.part);if(spec.package!=package||found==targets.end()||found->second.failed)return claimed;
  auto& target=found->second;
  const auto& state=RuntimeControls::animations.at(package).clips.at(animation);
  bool playing=state.playing;
  const auto& output=RuntimeControls::animations.at(package).output;
  for(const auto& shape:target.shapes)playing=playing&&output.count(shape.second);
  if(!playing){Forget(renderer);return claimed;}
  Load(target);
  auto current=bindings.find(renderer);
  if(current==bindings.end()){
   auto go=Ref::Make(Call(RuntimeShapes::api.rendererGameObject,renderer),false);
   Require(RuntimeShapes::Alive(go.Get()),"Mod Renderer GameObject is dead");
   void* animatorArg[]={api.animatorType.Get()};
   Require(!Call(RuntimeShapes::api.getComponent,renderer,animatorArg),"refusing to replace a game-owned Animator");
   void* skinArg[]={api.skinType.Get()};
   Require(Call(RuntimeShapes::api.getComponent,renderer,skinArg)==renderer,"ambiguous Mod Renderer target");
   // No game controller is touched. The native API requires an Animator on
   // the sampled object; this component is owned exclusively by the experiment.
   auto animator=Call(api.component,go.Get(),animatorArg);Require(RuntimeShapes::Alive(animator),"Mod sampling Animator creation failed");
   Binding binding;binding.renderer=Ref::Make(renderer,true);binding.mesh=Ref::Make(mesh,true);
   binding.animator=Ref::Make(animator,true);binding.part=spec.part;
   current=bindings.emplace(renderer,std::move(binding)).first;
   Log("[MOD-ANIMATION-PROBE] phase=bind part=%s renderer=%p mesh=%p shapes=%zu ownAnimator=1 gameControllerChanged=0 revision=%llu",
       spec.part.c_str(),renderer,mesh,target.shapes.size(),(unsigned long long)revision);
  }
  auto& b=current->second;
  Require(b.renderer.Get()==renderer&&b.mesh.Get()==mesh&&RuntimeShapes::Alive(b.animator.Get()),"Mod native binding retired");
  auto go=Ref::Make(Call(RuntimeShapes::api.rendererGameObject,renderer),false);
  float time=float(state.time);void* sampleArgs[]={go.Get(),&time};Call(api.sample,target.clip.Get(),sampleArgs);
  const auto& shapeBinding=RuntimeShapes::bound.at(renderer);
  for(const auto& shape:target.shapes){
   int index=shapeBinding.indices.at(shape.first);void* args[]={&index};
   float actual=Scalar<float>(RuntimeShapes::api.getWeight,renderer,args);
   double expected=RuntimeControls::ShapeValue(package,shape.second)*100.;
   double error=std::abs(actual-expected);b.maxError=std::max(b.maxError,error);
   Require(std::isfinite(actual)&&error<0.02,"native Mod shape differs: "+shape.first+" actual="+std::to_string(actual)+" expected="+std::to_string(expected));
   claimed.insert(shape.first);
  }
  ++b.samples;if(state.time<b.lastTime&&!state.paused)++b.wraps;
  if(b.samples==1||b.samples%300==0||b.paused!=state.paused||state.time<b.lastTime)
   Log("[MOD-ANIMATION-PROBE] phase=sample result=PASS part=%s renderer=%p samples=%zu time=%.9g paused=%d wraps=%zu maxError=%.9g nativeWriterOnly=1",
       spec.part.c_str(),renderer,b.samples,state.time,int(state.paused),b.wraps,b.maxError);
  b.lastTime=state.time;b.paused=state.paused;
 }catch(const std::exception& e){
  if(!prepared)prepareFailed=true;
  auto target=targets.find(spec.part);if(target!=targets.end())target->second.failed=true;
  Forget(renderer);claimed.clear();
  Log("[MOD-ANIMATION-PROBE] result=FAIL part=%s reason=%s existingShapeWriterRestored=1",spec.part.c_str(),e.what());
 }
 return claimed;
}
}
