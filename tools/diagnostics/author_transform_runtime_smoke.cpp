#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
namespace TestMount {
using namespace Entry;namespace A=RuntimeAuthorTransforms;
std::map<uint32_t,void*> handles;uint32_t nextHandle=0;uintptr_t serial=1000;
unsigned char yes=1;unsigned created=0,destroyed=0,sampled=0,animators=0;float time=-1;
std::set<void*> dead;std::map<void*,void*> parents;
uint32_t Handle(void* p,bool){handles[++nextHandle]=p;return nextHandle;}
void* Target(uint32_t h){return handles.at(h);}void Free(uint32_t h){handles.erase(h);}
void* Allocate(void*){++created;return reinterpret_cast<void*>(++serial);}
void* Invoke(void* method,void* self,void** args,void** error){*error=nullptr;switch(uintptr_t(method)){
 case 1:return &yes;case 2:{static unsigned char alive;alive=!dead.count(args[0]);return &alive;}
 case 3:return nullptr;case 4:return self;case 5:parents[self]=args[0];return nullptr;
 case 6:case 7:case 8:return nullptr;
 case 9:++destroyed;dead.insert(args[0]);return nullptr;
 case 10:++animators;return self;case 11:++sampled;time=*static_cast<float*>(args[1]);return nullptr;
 default:throw std::runtime_error("unexpected mock mount API");}}
void Run(){
 il2cpp_runtime_invoke=Invoke;il2cpp_object_unbox=[](void* v){return v;};il2cpp_gchandle_new=Handle;RuntimeShapes::Ref::weakHandle=Handle;il2cpp_gchandle_get_target=Target;il2cpp_gchandle_free=Free;
 il2cpp_object_new=Allocate;il2cpp_string_new=[](const char*)->void*{return reinterpret_cast<void*>(++serial);};
 RuntimeShapes::api.main=(void*)1;RuntimeShapes::api.alive=(void*)2;FullResourceReload::api.goTransform=(void*)4;
 EffectDebug::api.setParent=(void*)5;EffectDebug::api.position=(void*)6;EffectDebug::api.rotation=(void*)7;EffectDebug::api.scale=(void*)8;
 A::bound=true;A::api.ctor=(void*)3;A::api.destroy=(void*)9;A::api.add=(void*)10;A::api.sample=(void*)11;A::api.animator=RuntimeShapes::Ref::Make((void*)100,false);
 auto graph=std::make_shared<AuthorTransformHierarchy>();graph->id="Orbit";graph->nodes.push_back({"Pivot","Pivot"});graph->nodes.push_back({"Pivot/Tip","Tip"});
 Package p;p.id="InstallationA";p.defaults["$speed"]=1;p.persistent.insert("$speed");ShapeAnimation c;c.id="Spin";c.duration=2;c.speedVariable="$speed";c.autoplay=true;c.nativeOutputs.push_back({"Orbit","animations/spin.ab","assets/eff/animations/spin.anim","revision1"});p.shapeAnimations.emplace(c.id,c);
 EffectAttachment effect;effect.id="OrbitEffect";effect.hierarchy=graph;effect.node="Pivot/Tip";p.effects.emplace(effect.id,effect);
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{p});RuntimeControls::animations[p.id].Prepare(p);
 auto before=RuntimeAttachments::CaptureStamps();
 auto first=A::Acquire((void*)200,(void*)201,p,graph),second=A::Acquire((void*)200,(void*)201,p,graph);
 Require(first==second&&created==3&&animators==1,"shared effects duplicated Empty tree/Animator");
 auto clip=std::make_shared<A::Clip>();clip->asset=RuntimeShapes::Ref::Make((void*)300,false);clip->duration=2;first->loaded[c.id]=clip;
 RuntimeControls::animations[p.id].Advance(p,p.defaults,.5);A::Frame();Require(sampled==1&&time==.5f,"native SampleAnimation did not use unified clock");A::Frame();Require(sampled==1,"unchanged timeline sampled repeatedly");
 RuntimeControls::animations[p.id].Pause(c);A::Frame();Require(sampled==1,"pause resampled/reset pose");RuntimeControls::animations[p.id].Stop(c);A::Frame();Require(!first->posed,"stop failed to restore author base pose");
 auto instance=std::make_shared<EffectDebug::Instance>();instance->anchorOwner=first;first.reset();second.reset();Require(destroyed==0,"Empty tree destroyed before retired effect consumer");
 auto updated=p;updated.shapeAnimations.at(c.id).nativeOutputs[0].revision="revision2";
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{updated});
 Require(before!=RuntimeAttachments::CaptureStamps(),"animation-only edit did not invalidate production attachment stamps");
 auto replacement=A::Acquire((void*)200,(void*)201,updated,graph);Require(created==6,"changed animation reused stale runtime tree");
 instance.reset();Require(destroyed==1,"retired effect did not release hierarchy");replacement.reset();Require(destroyed==2,"new hierarchy leaked after final consumer");
 A::Frame();Require(A::trees.empty(),"expired author hierarchy registry leaked");
 A::api.animator={};clip.reset();RuntimeControls::animations.clear();RuntimeControls::controlGraph.reset();Require(handles.empty(),"mount test managed handles leaked");
 std::cout<<"PASS runtime author mounts: shared trees/Animator, unified native sampling, pause/stop, changed generations and effect-consumer retirement\n";
}
}
int main(){try{TestMount::Run();}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
