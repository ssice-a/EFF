#pragma once

// Included inside EffectDebug. Bounded, read-only native parameter snapshots.
// A failed observation must never prevent an effect from playing.
namespace Observation {
struct Api {
 bool ready=false;
 void *mainModule=nullptr,*emission=nullptr,*count=nullptr,*time=nullptr,*playing=nullptr;
 void *emitting=nullptr,*stopped=nullptr,*paused=nullptr;
 void *speed=nullptr,*duration=nullptr,*loop=nullptr,*max=nullptr,*space=nullptr,*life=nullptr,*start=nullptr,*rate=nullptr;
 void *mode=nullptr,*low=nullptr,*high=nullptr,*multiplier=nullptr,*timeScale=nullptr,*lossyScale=nullptr;
 void *particleClass=nullptr,*getParticles=nullptr,*particleLife=nullptr,*remainingLife=nullptr,*velocity=nullptr;
 void *worldPosition=nullptr,*bounds=nullptr,*forceOff=nullptr,*visible=nullptr,*renderMode=nullptr,*renderMask=nullptr;
 void *mainCamera=nullptr,*cameraMask=nullptr;
 size_t particleSize=0;
} api;
// Simulation is not evidence of GPU visibility. These queries run only at the
// existing three snapshot boundaries and never change materials or Renderers.
void RenderState(Instance& instance,void* particle,const char* phase) noexcept {
 try{
  if(!EffectDebug::api.getComponent||!EffectDebug::api.particleRendererType.Get())return;
  void* lookup[]={EffectDebug::api.particleRendererType.Get()};
  auto renderer=Ref::Make(Call(EffectDebug::api.getComponent,particle,lookup),false);
  std::ostringstream state;
  if(!RuntimeShapes::Alive(renderer.Get()))state<<"rendererMissing=1";
  else{
   auto flag=[&](void* method){return method?int(RuntimeShapes::Scalar<unsigned char>(method,renderer.Get())):-1;};
   auto vector=[&](const char* name,void* transform){if(!api.worldPosition||!RuntimeShapes::Alive(transform))return;auto p=RuntimeShapes::Scalar<std::array<float,3>>(api.worldPosition,transform);state<<' '<<name<<"=("<<p[0]<<','<<p[1]<<','<<p[2]<<')';};
   auto go=Ref::Make(Call(FullResourceReload::api.componentGo,particle),false);
   auto transform=Ref::Make(Call(FullResourceReload::api.goTransform,go.Get()),false);
   int layer=RuntimeShapes::Scalar<int>(EffectDebug::api.layer,go.Get());
   state<<"renderer="<<renderer.Get()<<" node="<<String(Call(RuntimeShapes::api.name,go.Get()))
        <<" enabled="<<RuntimeShapes::ActualEnabled(renderer.Get())<<" forceOff="<<flag(api.forceOff)<<" visible="<<flag(api.visible)<<" layer="<<layer;
   if(api.renderMode)state<<" mode="<<RuntimeShapes::Scalar<int>(api.renderMode,renderer.Get());
   if(api.renderMask)state<<" renderMask="<<RuntimeShapes::Scalar<uint32_t>(api.renderMask,renderer.Get());
   if(EffectDebug::api.renderInUI)state<<" renderInUI="<<flag(EffectDebug::api.renderInUI);
   vector("particleWorld",transform.Get());vector("mountWorld",instance.parent.Get());
   if(api.bounds){auto b=RuntimeShapes::Scalar<std::array<float,6>>(api.bounds,renderer.Get());state<<" boundsCenter=("<<b[0]<<','<<b[1]<<','<<b[2]<<") boundsExtents=("<<b[3]<<','<<b[4]<<','<<b[5]<<')';}
   if(api.mainCamera&&api.cameraMask){auto camera=Ref::Make(Call(api.mainCamera),false);if(RuntimeShapes::Alive(camera.Get())){auto mask=RuntimeShapes::Scalar<uint32_t>(api.cameraMask,camera.Get());state<<" mainCameraMask="<<mask<<" mainCameraIncludesLayer="<<int(layer>=0&&layer<32&&bool(mask&(uint32_t(1)<<layer)));}else state<<" mainCamera=unavailable";}
   if(FullResourceReload::api.materials){auto array=Ref::Make(Call(FullResourceReload::api.materials,renderer.Get()),false);auto materials=FullResourceReload::ReferenceArray(array.Get());Require(materials.size()<=32,"effect renderer material extent");state<<" materialSlots="<<materials.size();
    for(size_t slot=0;slot<materials.size();++slot){auto material=materials[slot];state<<" material["<<slot<<"]={";if(!RuntimeShapes::Alive(material)){state<<"missing}";continue;}
     auto shader=FullResourceReload::MaterialShader(material);state<<"name:"<<String(Call(RuntimeShapes::api.name,material))<<",shader:"<<shader.name<<",supported:"<<shader.supported<<",passes:"<<shader.passes;
     if(FullResourceReload::api.textureIds&&FullResourceReload::api.textureById){auto ids=Ref::Make(Call(FullResourceReload::api.textureIds,material),false);if(ids.Get()){auto length=il2cpp_array_length(ids.Get());Require(length<=128,"effect texture property extent");for(size_t i=0;i<length;++i){int id=0;std::memcpy(&id,(unsigned char*)ids.Get()+32+i*sizeof(id),sizeof(id));void* args[]={&id};auto texture=Ref::Make(Call(FullResourceReload::api.textureById,material,args),false);state<<",tex["<<id<<"]:";if(!RuntimeShapes::Alive(texture.Get())){state<<"null";continue;}state<<String(Call(RuntimeShapes::api.name,texture.Get()));if(FullResourceReload::api.width&&FullResourceReload::api.height)state<<'('<<RuntimeShapes::Scalar<int>(FullResourceReload::api.width,texture.Get())<<'x'<<RuntimeShapes::Scalar<int>(FullResourceReload::api.height,texture.Get())<<')';}}}
     state<<'}';
    }
   }
  }
  Log("[EFFECT-RENDER] phase=%s owner=%p particle=%p %s gpuVisibilityVerified=0",phase,instance.owner.Get(),particle,state.str().c_str());
 }catch(const std::exception& e){Log("[EFFECT-RENDER-GAP] phase=%s owner=%p particle=%p reason=%s playbackPreserved=1",phase,instance.owner.Get(),particle,e.what());}
}
std::string LiveParticles(void* ps){
 if(!api.getParticles)return "unavailable";
 int count=RuntimeShapes::Scalar<int>(api.count,ps);if(count<=0)return "empty";
 auto values=Ref::Make(il2cpp_array_new(api.particleClass,size_t(std::min(count,256))),false);
 Require(values.Get(),"particle observation array allocation");void* args[]={values.Get()};
 int length=RuntimeShapes::Scalar<int>(api.getParticles,ps,args);
 Require(length>=0&&size_t(length)<=il2cpp_array_length(values.Get()),"particle observation array extent");
 float lifeMin=FLT_MAX,lifeMax=0,remainingMin=FLT_MAX,remainingMax=0,speedMin=FLT_MAX,speedMax=0;
 for(int i=0;i<length;++i){auto p=(unsigned char*)values.Get()+32+size_t(i)*api.particleSize;
  float life=RuntimeShapes::Scalar<float>(api.particleLife,p),remaining=RuntimeShapes::Scalar<float>(api.remainingLife,p);
  auto v=RuntimeShapes::Scalar<std::array<float,3>>(api.velocity,p);float speed=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
  lifeMin=std::min(lifeMin,life);lifeMax=std::max(lifeMax,life);remainingMin=std::min(remainingMin,remaining);remainingMax=std::max(remainingMax,remaining);speedMin=std::min(speedMin,speed);speedMax=std::max(speedMax,speed);
 }
 std::ostringstream out;out<<"sampled:"<<length;if(length)out<<",life:["<<lifeMin<<","<<lifeMax<<"],remaining:["<<remainingMin<<","<<remainingMax<<"],speed:["<<speedMin<<","<<speedMax<<"]";return out.str();
}
std::string Curve(void* getter,void* self){
 auto value=Ref::Make(Call(getter,self),false);Require(value.Get(),"particle curve getter returned null");
 auto raw=il2cpp_object_unbox(value.Get());std::ostringstream out;
 out<<"mode:"<<RuntimeShapes::Scalar<int>(api.mode,raw)
    <<",min:"<<RuntimeShapes::Scalar<float>(api.low,raw)
    <<",max:"<<RuntimeShapes::Scalar<float>(api.high,raw)
    <<",multiplier:";
 if(api.multiplier)out<<RuntimeShapes::Scalar<float>(api.multiplier,raw);else out<<"unavailable";return out.str();
}
void Capture(Instance& instance,const char* phase) noexcept {
 if(!api.ready||!settings.Get().detailedDiagnostics)return;
 try{for(const auto& particle:instance.particles){
  if(!RuntimeShapes::Alive(particle.Get()))continue;
  RenderState(instance,particle.Get(),phase);
  auto main=Ref::Make(Call(api.mainModule,particle.Get()),false),emission=Ref::Make(Call(api.emission,particle.Get()),false);
  Require(main.Get()&&emission.Get(),"particle modules returned null");
  auto raw=il2cpp_object_unbox(main.Get()),eraw=il2cpp_object_unbox(emission.Get());
  auto go=Ref::Make(Call(FullResourceReload::api.componentGo,particle.Get()),false);
  auto transform=Ref::Make(Call(FullResourceReload::api.goTransform,go.Get()),false);
  auto scale=RuntimeShapes::Scalar<std::array<float,3>>(api.lossyScale,transform.Get());
  auto life=Curve(api.life,raw),start=Curve(api.start,raw),rate=Curve(api.rate,eraw),live=LiveParticles(particle.Get());
  auto flag=[&](void* method){return method?int(RuntimeShapes::Scalar<unsigned char>(method,particle.Get())):-1;};
  Log("[EFFECT-PARAMETERS] phase=%s owner=%p instance=%p particle=%p count=%d playing=%d time=%.4f simulationSpeed=%.4f timeScale=%.4f duration=%.4f loop=%d maxParticles=%d simulationSpace=%d worldScale=(%.5f,%.5f,%.5f) lifetime={%s} startSpeed={%s} emissionRate={%s} live={%s} emitting=%d stopped=%d paused=%d nodeActive=%d",
   phase,instance.owner.Get(),instance.object.Get(),particle.Get(),RuntimeShapes::Scalar<int>(api.count,particle.Get()),int(RuntimeShapes::Scalar<unsigned char>(api.playing,particle.Get())),
   RuntimeShapes::Scalar<float>(api.time,particle.Get()),RuntimeShapes::Scalar<float>(api.speed,raw),RuntimeShapes::Scalar<float>(api.timeScale),RuntimeShapes::Scalar<float>(api.duration,raw),
   int(RuntimeShapes::Scalar<unsigned char>(api.loop,raw)),RuntimeShapes::Scalar<int>(api.max,raw),RuntimeShapes::Scalar<int>(api.space,raw),scale[0],scale[1],scale[2],life.c_str(),start.c_str(),rate.c_str(),live.c_str(),
   flag(api.emitting),flag(api.stopped),flag(api.paused),int(RuntimeShapes::Scalar<unsigned char>(EffectDebug::api.activeHierarchy,go.Get())));
 }}catch(const std::exception& e){instance.observationStep=2;Log("[EFFECT-PARAMETERS-GAP] reason=%s playbackPreserved=1",e.what());}
}
void Tick(Instance& instance){
 if(!api.ready||instance.observationStep>=2||!settings.Get().detailedDiagnostics)return;
 if(NativeBundle::Ms(instance.observationClock)<(instance.observationStep?3000.:1000.))return;
 auto phase=instance.observationStep?"after-3s":"after-1s";++instance.observationStep;Capture(instance,phase);
}
void Install(void* ps,void* transform,void** assemblies,size_t count) noexcept {
 try{
  auto renderer=FindClass("UnityEngine","Renderer",assemblies,count),particleRenderer=FindClass("UnityEngine","ParticleSystemRenderer",assemblies,count),camera=FindClass("UnityEngine","Camera",assemblies,count);
  api.worldPosition=Exact(transform,"get_position",{},"UnityEngine.Vector3",false);
  api.bounds=Exact(renderer,"get_bounds",{},"UnityEngine.Bounds",false);api.forceOff=Exact(renderer,"get_forceRenderingOff",{},"System.Boolean",false);api.visible=Exact(renderer,"get_isVisible",{},"System.Boolean",false);
  api.renderMode=Exact(particleRenderer,"get_renderMode",{},"UnityEngine.ParticleSystemRenderMode",false);api.renderMask=Exact(renderer,"get_renderingLayerMask",{},"System.UInt32",false);
  if(camera){api.mainCamera=Exact(camera,"get_main",{},"UnityEngine.Camera",true);api.cameraMask=Exact(camera,"get_cullingMask",{},"System.Int32",false);}
  auto nested=[&](const char* name)->void*{void* iterator=nullptr;while(auto type=il2cpp_class_get_nested_types(ps,&iterator))if(!strcmp(il2cpp_class_get_name(type),name))return type;throw std::runtime_error("particle nested type absent");};
  auto main=nested("MainModule"),emission=nested("EmissionModule"),curve=nested("MinMaxCurve");
  auto type=[](void* klass){auto raw=il2cpp_type_get_name(il2cpp_class_get_type(klass));Require(raw,"particle type name absent");std::string name=raw;il2cpp_free((void*)raw);return name;};auto method=FullResourceReload::Method;
  api.mainModule=method(ps,"get_main",{},type(main).c_str(),false);api.emission=method(ps,"get_emission",{},type(emission).c_str(),false);
  api.count=method(ps,"get_particleCount",{},"System.Int32",false);api.playing=method(ps,"get_isPlaying",{},"System.Boolean",false);api.time=method(ps,"get_time",{},"System.Single",false);
  api.emitting=Exact(ps,"get_isEmitting",{},"System.Boolean",false);api.stopped=Exact(ps,"get_isStopped",{},"System.Boolean",false);api.paused=Exact(ps,"get_isPaused",{},"System.Boolean",false);
  api.speed=method(main,"get_simulationSpeed",{},"System.Single",false);api.duration=method(main,"get_duration",{},"System.Single",false);api.loop=method(main,"get_loop",{},"System.Boolean",false);
  api.max=method(main,"get_maxParticles",{},"System.Int32",false);api.space=method(main,"get_simulationSpace",{},"UnityEngine.ParticleSystemSimulationSpace",false);
  api.life=method(main,"get_startLifetime",{},type(curve).c_str(),false);api.start=method(main,"get_startSpeed",{},type(curve).c_str(),false);api.rate=method(emission,"get_rateOverTime",{},type(curve).c_str(),false);
  api.mode=method(curve,"get_mode",{},"UnityEngine.ParticleSystemCurveMode",false);api.low=method(curve,"get_constantMin",{},"System.Single",false);api.high=method(curve,"get_constantMax",{},"System.Single",false);
  // Some game builds strip this getter. It must not disable count/playing/life
  // observations, and an unavailable multiplier must never be reported as 1.
  api.multiplier=Exact(curve,"get_curveMultiplier",{},"System.Single",false);
  api.timeScale=method(FindClass("UnityEngine","Time",assemblies,count),"get_timeScale",{},"System.Single",true);api.lossyScale=method(transform,"get_lossyScale",{},"UnityEngine.Vector3",false);
  auto particle=nested("Particle");uint32_t alignment=0;api.particleSize=il2cpp_class_value_size(particle,&alignment);
  api.particleLife=Exact(particle,"get_startLifetime",{},"System.Single",false);api.remainingLife=Exact(particle,"get_remainingLifetime",{},"System.Single",false);api.velocity=Exact(particle,"get_velocity",{},"UnityEngine.Vector3",false);
  auto arrayType=type(particle)+"[]";api.getParticles=api.particleLife&&api.remainingLife&&api.velocity?Exact(ps,"GetParticles",{arrayType.c_str()},"System.Int32",false):nullptr;
  if(api.getParticles){Require(api.particleSize>=12&&api.particleSize<=4096,"particle observation value size");api.particleClass=particle;}
  api.ready=true;Log("[EFFECT-PARAMETERS-READY] detailed=%d snapshots=after-play,after-1s,after-3s readOnly=1",int(settings.Get().detailedDiagnostics));
 }catch(const std::exception& e){api.ready=false;Log("[EFFECT-PARAMETERS-GAP] install=%s playbackPreserved=1",e.what());}
}
}
