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
 size_t particleSize=0;
} api;
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
 if(!api.ready)return;
 try{for(const auto& particle:instance.particles){
  if(!RuntimeShapes::Alive(particle.Get()))continue;
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
 if(!api.ready||instance.observationStep>=2)return;
 if(NativeBundle::Ms(instance.observationClock)<(instance.observationStep?3000.:1000.))return;
 auto phase=instance.observationStep?"after-3s":"after-1s";++instance.observationStep;Capture(instance,phase);
}
void Install(void* ps,void* transform,void** assemblies,size_t count) noexcept {
 try{
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
  api.ready=true;Log("[EFFECT-PARAMETERS-READY] snapshots=after-play,after-1s,after-3s readOnly=1");
 }catch(const std::exception& e){api.ready=false;Log("[EFFECT-PARAMETERS-GAP] install=%s playbackPreserved=1",e.what());}
}
}
