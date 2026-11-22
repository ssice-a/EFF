#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
namespace TestMount {
using namespace Entry;namespace A=RuntimeAuthorTransforms;
std::map<uint32_t,void*> handles;uint32_t nextHandle=0;uintptr_t serial=1000;
unsigned char yes=1;unsigned created=0,destroyed=0,sampled=0,animators=0;float time=-1;
int frame=100;float delta=.5f;
std::set<void*> dead;std::map<void*,void*> parents;
struct StringObject {char header[16]{};int length;wchar_t text[256];};
StringObject pivotName{{},5,L"Pivot"},tipName{{},3,L"Tip"};
StringObject nativeName{{},6,L"Native"},variedName{{},7,L"Variant"};
std::map<void*,StringObject> routeNames;
struct BoneArray {char header[24]{};size_t size=1;void* bone=nullptr;};
std::map<void*,BoneArray> palettes;
std::map<void*,unsigned> boneWrites;
std::map<void*,float> boneHeights;
using EiemAuthorBoneAnimation::Rotation;
std::map<void*,Rotation> rotations;
std::map<void*,unsigned> rotationWrites;
std::map<void*,std::vector<void*>> children;
uint32_t Handle(void* p,bool){handles[++nextHandle]=p;return nextHandle;}
void* Target(uint32_t h){return handles.at(h);}void Free(uint32_t h){handles.erase(h);}
void* Allocate(void*){++created;return reinterpret_cast<void*>(++serial);}
void* Invoke(void* method,void* self,void** args,void** error){*error=nullptr;switch(uintptr_t(method)){
 case 1:return &yes;case 2:{static unsigned char alive;alive=!dead.count(args[0]);return &alive;}
 case 3:return nullptr;case 4:return self;case 5:parents[self]=args[0];return nullptr;
 case 6:case 7:case 8:return nullptr;
 case 9:++destroyed;dead.insert(args[0]);return nullptr;
 case 10:++animators;return self;case 11:++sampled;time=*static_cast<float*>(args[1]);return nullptr;
 case 12:return &frame;case 13:return &delta;
 case 14:return parents.at(self);case 15:return &palettes.at(self);
 case 16:
  if(auto route=routeNames.find(self);route!=routeNames.end())return &route->second;
  if(self==(void*)403||self==(void*)414)return &nativeName;
  if(self==(void*)413)return &variedName;
  return self==(void*)401||self==(void*)411?&pivotName:&tipName;
 case 17:++boneWrites[self];boneHeights[self]=static_cast<float*>(args[0])[1];return nullptr;
 case 18:{
  ++rotationWrites[self];
  std::copy_n(static_cast<float*>(args[0]),4,rotations[self].begin());
  return nullptr;}
 case 19:return &rotations.at(self);
 case 20:{static int n;n=int(children.at(self).size());return &n;}
 case 21:return children.at(self).at(size_t(*static_cast<int*>(args[0])));
 default:throw std::runtime_error("unexpected mock mount API");}}

void CheckLiveRetarget(EiemAuthorBoneAnimation::Skeleton skeleton, RuntimeControls::Spec spec) {
 EiemAuthorBoneAnimation::Node source;
 source.path="Native";source.parent=0;
 skeleton.nodes.push_back(source);
 EiemAuthorBoneAnimation::SetRetargets(skeleton,{{"Pivot","Native"}});
 spec.package="RetargetWithoutClip";
 RuntimeBones::methods.rotation=(void*)18;
 RuntimeBones::methods.getRotation=(void*)19;
 FullResourceReload::api.childCount=(void*)20;
 FullResourceReload::api.getChild=(void*)21;
 children[(void*)400]={(void*)401,(void*)403,(void*)405};
 children[(void*)410]={(void*)411,(void*)414,(void*)413,(void*)415};
 parents[(void*)403]=(void*)400;
 parents[(void*)413]=(void*)410;
 const auto prefix=EiemAuthorBoneAnimation::RetargetRoutePrefix(EiemComposedPackage::Name(spec.package,spec.skeleton));
 const auto writeRoutes=[&](void* tag,int ordinal){
   auto text=EiemAuthorBoneAnimation::EncodeRetargetRoutes(prefix,{{1,{ordinal}}});
   auto& name=routeNames[tag];
   Require(text.size()<std::size(name.text),"retarget mock name exceeds buffer");
   name.length=int(text.size());std::copy(text.begin(),text.end(),name.text);
 };
 writeRoutes((void*)405,1);
 writeRoutes((void*)415,2);
 rotations[(void*)403]=rotations[(void*)413]={0,0,0,1};
 for(auto renderer:{(void*)501,(void*)502,(void*)503})
   RuntimeBones::BindPalette(renderer,spec,skeleton,{"Pivot/Tip"},{});
 Require(RuntimeBones::bones.size()==2&&RuntimeBones::liveCount==2,"shared retarget bone bound twice");
 RuntimeShapes::enabled=true;
 ++frame;
 RuntimeShapes::Update();
 Require(rotationWrites.empty(),"retarget sampled before LateUpdate");
 RuntimeShapes::originalFrameTick=[](void*,void*){
   rotations[(void*)403]={.6f,0,0,.8f};
   rotations[(void*)413]={0,.6f,0,.8f};
 };
 RuntimeShapes::FrameTick(nullptr,nullptr);
 Require(RuntimeBones::SameRotation(rotations.at((void*)401),rotations.at((void*)403))&&
         RuntimeBones::SameRotation(rotations.at((void*)411),rotations.at((void*)413)),
         "retarget used a bone name, the wrong character instance, or the previous frame");
 ++frame;
 RuntimeShapes::FrameTick(nullptr,nullptr);
 Require(rotationWrites.size()==2&&rotationWrites.at((void*)401)==1&&rotationWrites.at((void*)411)==1,
         "unchanged live rotation triggered another Transform write");
 RuntimeShapes::RetireRenderer((void*)501);
 Require(rotationWrites.at((void*)401)==1,"retiring one LOD reset the surviving LOD's live pose");
 RuntimeShapes::RetireRenderer((void*)502);
 RuntimeShapes::RetireRenderer((void*)503);
 Require(RuntimeBones::bones.empty()&&RuntimeBones::liveCount==0&&rotationWrites.at((void*)401)==2&&
         rotationWrites.at((void*)411)==2&&rotations.at((void*)401)==Rotation{0,0,0,1}&&
         rotations.at((void*)411)==Rotation{0,0,0,1}&&boneWrites.at((void*)401)==2,
         "live rotation did not release its bindings or changed authored position");
 RuntimeShapes::enabled=false;
}

void Run(){
 il2cpp_runtime_invoke=Invoke;il2cpp_object_unbox=[](void* v){return v;};il2cpp_gchandle_new=Handle;RuntimeShapes::Ref::weakHandle=Handle;il2cpp_gchandle_get_target=Target;il2cpp_gchandle_free=Free;
 il2cpp_object_new=Allocate;il2cpp_string_new=[](const char*)->void*{return reinterpret_cast<void*>(++serial);};
 RuntimeShapes::api.main=(void*)1;RuntimeShapes::api.alive=(void*)2;FullResourceReload::api.goTransform=(void*)4;
 RuntimeShapes::api.bones=(void*)15;RuntimeShapes::api.name=(void*)16;
 RuntimeBones::methods={(void*)14,(void*)17,(void*)7,(void*)8};
 EffectDebug::api.setParent=(void*)5;EffectDebug::api.position=(void*)6;EffectDebug::api.rotation=(void*)7;EffectDebug::api.scale=(void*)8;
 A::bound=true;A::api.ctor=(void*)3;A::api.destroy=(void*)9;A::api.add=(void*)10;A::api.sample=(void*)11;A::api.animator=RuntimeShapes::Ref::Make((void*)100,false);
 auto graph=std::make_shared<AuthorTransformHierarchy>();graph->id="Orbit";graph->nodes.push_back({"Pivot","Pivot"});graph->nodes.push_back({"Pivot/Tip","Tip"});
 Package p;p.id="InstallationA";p.defaults["$speed"]=1;p.defaults["$shape"]=0;p.persistent.insert("$speed");ShapeAnimation c;c.id="Spin";c.duration=2;c.speedVariable="$speed";c.autoplay=true;c.tracks["$shape"]={0,1};c.nativeOutputs.push_back({"Orbit","animations/spin.ab","assets/eff/animations/spin.anim","revision1"});p.shapeAnimations.emplace(c.id,c);
 using EiemAuthorBoneAnimation::Node;
 Node root,pivot,tip;pivot.path="Pivot";pivot.parent=0;pivot.source=false;pivot.position[1]=1;
 tip.path="Pivot/Tip";tip.parent=1;tip.source=false;
 EiemAuthorBoneAnimation::Skeleton skeleton;skeleton.nodes={root,pivot,tip};
 c.boneTracks.push_back({"Rig","Pivot",{{0,1,0,0,0,0,1,1,1,1},{0,5,0,0,0,0,1,1,1,1}}});
 p.shapeAnimations.at(c.id)=c;
 EffectAttachment effect;effect.id="OrbitEffect";effect.hierarchy=graph;effect.node="Pivot/Tip";p.effects.emplace(effect.id,effect);
 auto controls=RuntimeControls::Prepare(std::make_shared<const std::vector<Package>>(std::vector<Package>{p}),EiemGlobalConfig{});RuntimeControls::Swap(controls);
 auto before=RuntimeAttachments::CaptureStamps();
 auto first=A::Acquire((void*)200,(void*)201,p,graph),second=A::Acquire((void*)200,(void*)201,p,graph);
 Require(first==second&&created==3&&animators==1,"shared effects duplicated Empty tree/Animator");
 auto clip=std::make_shared<A::Clip>();clip->asset=RuntimeShapes::Ref::Make((void*)300,false);clip->duration=2;first->loaded[c.id]=clip;
 RuntimeShapes::enabled=true;RuntimeShapes::api.frame=(void*)12;RuntimeShapes::api.elapsed=(void*)13;
 RuntimeShapes::packageGeneration=generation.load();
 RuntimeControls::Spec boneSpec;boneSpec.package=p.id;boneSpec.skeleton="Rig";
 parents[(void*)402]=(void*)401;parents[(void*)401]=(void*)400;
 parents[(void*)412]=(void*)411;parents[(void*)411]=(void*)410;
 palettes[(void*)501].bone=(void*)402;palettes[(void*)502].bone=(void*)402;
 palettes[(void*)503].bone=(void*)412;
 for(auto renderer:{(void*)501,(void*)502,(void*)503})
   RuntimeBones::BindPalette(renderer,boneSpec,skeleton,{"Pivot/Tip"},RuntimeControls::animations.at(p.id).bonePaths.at("Rig"));
 Require(RuntimeBones::bones.size()==2,"unkeyed bones were registered by the animation adapter");
 RuntimeShapes::Update();
 Require(sampled==1&&time==.5f&&RuntimeControls::animations.at(p.id).clips.at(c.id).time==time,
   "production Unity frame sampled native output before advancing the shared clock");
 Require(RuntimeControls::ShapeValue(p.id,"$shape")==.25,"production frame numeric/native outputs used different phases");
 Require(boneHeights[(void*)401]==2.f&&boneHeights[(void*)411]==2.f&&boneWrites[(void*)401]==1&&boneWrites[(void*)411]==1&&
   !boneWrites.count((void*)400)&&!boneWrites.count((void*)402),"bone output did not reach the unweighted parent exactly once per instance");
 RuntimeShapes::Update();Require(sampled==1,"duplicate model callbacks advanced/sampled the same Unity frame twice");
 RuntimeShapes::enabled=false;
 RuntimeControls::animations[p.id].Pause(c);A::Frame();RuntimeBones::Frame();
 Require(sampled==1&&boneWrites[(void*)401]==1,"pause resampled/reset pose");
 RuntimeShapes::RetireRenderer((void*)501);Require(boneHeights[(void*)401]==2.f,"retiring one Part reset the surviving Part's pose");
 RuntimeShapes::RetireRenderer((void*)503);Require(boneHeights[(void*)411]==1.f,"retiring sole Part did not restore its pose");
 RuntimeControls::animations[p.id].Stop(c);A::Frame();RuntimeBones::Frame();
 Require(!first->posed&&boneHeights[(void*)401]==1.f,"stop failed to restore author base pose");
 RuntimeShapes::RetireRenderer((void*)502);Require(RuntimeBones::bones.empty(),"retired skeleton registry leaked");
 CheckLiveRetarget(skeleton,boneSpec);
 auto instance=std::make_shared<EffectDebug::Instance>();instance->anchorOwner=first;first.reset();second.reset();Require(destroyed==0,"Empty tree destroyed before retired effect consumer");
 auto updated=p;updated.shapeAnimations.at(c.id).nativeOutputs[0].revision="revision2";
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{updated});
 Require(before!=RuntimeAttachments::CaptureStamps(),"animation-only edit did not invalidate production attachment stamps");
 auto replacement=A::Acquire((void*)200,(void*)201,updated,graph);Require(created==6,"changed animation reused stale runtime tree");
 instance.reset();Require(destroyed==1,"retired effect did not release hierarchy");replacement.reset();Require(destroyed==2,"new hierarchy leaked after final consumer");
 A::Frame();Require(A::trees.empty(),"expired author hierarchy registry leaked");
 A::api.animator={};clip.reset();RuntimeControls::animations.clear();RuntimeControls::controlGraph.reset();Require(handles.empty(),"mount test managed handles leaked");
 std::cout<<"PASS runtime author mounts and late-frame bone retarget: isolated model instances, shared LOD, unchanged rotations and retirement\n";
}
}
int main(){try{TestMount::Run();}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
