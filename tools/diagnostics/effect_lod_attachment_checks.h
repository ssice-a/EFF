#pragma once
#include "effect_native_mesh_provenance_fixture.h"
namespace EffectLodAttachmentTest {
using namespace Entry;namespace A=RuntimeAttachments;namespace D=EffectDebug;
void* common=(void*)0xB100;std::vector<void*> branches,renderers;
std::map<void*,std::string> names;std::map<void*,void*> meshes;
std::set<void*> inactive;
std::map<void*,std::array<float,3>> positions,scales;
std::map<void*,std::array<float,4>> rotations;
void* sampledRoot=nullptr;float sampledTime=-1;
bool Active(void* object){for(auto p=object;p;){if(inactive.count(p))return false;auto it=Test::transformParents.find(p);p=it==Test::transformParents.end()?nullptr:it->second;}return true;}
void* Invoke(void* method,void* self,void** args,void** error){
 *error=nullptr;auto m=uintptr_t(method);
 if(m==909&&args[0]!=(void*)910)return Test::Array(renderers);
 if(m==4&&names.count(self))return Test::StringValue(names.at(self));
 if(m==1501)return self;
 if(m==1502)return meshes.at(self);
 if(m==79)return Test::Array(meshes.count(self)?std::vector<void*>{self}:std::vector<void*>{});
 if(m==1503)return Active(self)?&Test::yes:&Test::no;
 if(m==1504)return &Test::yes;
 if(m==1505){names[self]=Entry::String(args[0]);Test::transformParents[self]=nullptr;return nullptr;}
 if(m==1507)return self;
 if(m==1508){sampledRoot=args[0];sampledTime=*static_cast<float*>(args[1]);return nullptr;}
 if(m==904)Test::transformParents[self]=args[0];
 if(m==916)positions[self]=*static_cast<std::array<float,3>*>(args[0]);
 if(m==917)rotations[self]=*static_cast<std::array<float,4>*>(args[0]);
 if(m==918)scales[self]=*static_cast<std::array<float,3>*>(args[0]);
 return EffectTest::Invoke(method,self,args,error);
}
void Tick(){for(unsigned i=0;i<12;++i){for(auto& row:Test::scheduleDone)row.second=true;Test::done=1;A::Frame();D::Collect();}}
void Checks(){
 EffectTest::Setup();il2cpp_runtime_invoke=Invoke;
 RuntimeShapes::api.getComponent=(void*)1501;RuntimeShapes::api.filterType=(void*)0x1500;RuntimeShapes::api.filterMesh=(void*)1502;
 RuntimeShapes::api.getEnabled=(void*)1504;RuntimeShapes::originalEnabledGetter=nullptr;D::api.activeHierarchy=(void*)1503;
 il2cpp_object_get_class=[](void* object)->void*{return meshes.count(object)?Test::staticClass:Test::GetClass(object);};
 RuntimeAuthorTransforms::bound=true;RuntimeAuthorTransforms::api.go=(void*)0x1506;RuntimeAuthorTransforms::api.ctor=(void*)1505;RuntimeAuthorTransforms::api.destroy=(void*)908;RuntimeAuthorTransforms::api.add=(void*)1507;RuntimeAuthorTransforms::api.sample=(void*)1508;
 Test::transformParents={{Test::oldRoot,nullptr},{common,Test::oldRoot}};names[common]="Mesh_all";
 EffectAttachment effect;effect.id="particles";effect.variable="$fx";effect.native=EffectTest::Manifest('8');effect.targetPrefab="assets/weapon.prefab";
 effect.rendererPath="Mesh_all/lod0/Sword_lod0";effect.targetPath="assets/sword.fbx";effect.targetAsset="Sword_lod0";effect.position={0,.366f,0};
 auto basis=std::make_shared<EffectLodAnchor>();basis->path="Mesh_all";basis->nodes.resize(2);basis->nodes[1].rotation={-.70710678f,0,0,.70710678f};basis->nodes[1].scale={1,2,1};effect.lodAnchor=basis;
 for(unsigned level=0;level<4;++level){auto branch=(void*)(0xB200+uintptr_t(level));auto renderer=(void*)(0xB300+uintptr_t(level));auto mesh=(void*)(0xB400+uintptr_t(level));branches.push_back(branch);renderers.push_back(renderer);meshes[renderer]=mesh;
  Test::transformParents[branch]=common;Test::transformParents[renderer]=branch;names[branch]="lod"+std::to_string(level);names[renderer]=names[mesh]="Sword_lod"+std::to_string(level);EffectTest::layers[renderer]=9;
  if(level)effect.lodTargets.push_back({"Mesh_all/lod"+std::to_string(level)+"/"+names[mesh],"assets/sword.fbx",names[mesh],{}});
 }
 // Same mesh family but an undeclared sibling must never add a second clone.
 auto extra=(void*)0xB399;renderers.push_back(extra);meshes[extra]=(void*)0xB499;names[extra]="SwordVfx_lod0";names[meshes[extra]]="Sword_lod0";Test::transformParents[extra]=branches[0];
 Package p;p.id="LodEffects";p.effects.emplace(effect.id,effect);p.defaults[effect.variable]=1;
 auto proofScope=std::make_unique<EffectNativeMeshFixture::Scope>();
 std::vector<EiemDemandMeshProvenance::Use> uses;
 for(unsigned lod=0;lod<4;++lod)uses.push_back({{EiemRenderAssemblyDescription::Segment("Mesh_all",0)+EiemRenderAssemblyDescription::Segment("lod"+std::to_string(lod),0)+EiemRenderAssemblyDescription::Segment("Sword_lod"+std::to_string(lod),0),0},{{"assets/sword.fbx","Sword_lod"+std::to_string(lod)}}});
 proofScope->Add(effect.targetPrefab,uses);FullResourceReload::liveParts.at(Test::mainPart).path=effect.targetPrefab;
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{p});RuntimeControls::variables[p.id]=p.defaults;RuntimeControls::specs.clear();
 A::standalone=std::make_shared<const std::vector<Package>>();A::diskEffects.clear();A::overrides.clear();A::revision=RuntimeControls::revision;A::activeStamps=A::CaptureStamps();A::stampsInitialized=true;A::needed=true;A::reconcileClock=NativeBundle::Clock();
 // An owner first encountered at a distant LOD must not need LOD0 instantiated.
 auto all=renderers;renderers={all[3],extra};inactive={branches[0],branches[1],branches[2]};
 A::ModelReady(Test::oldRoot);Require(A::bindings.size()==1,"distant LOD did not bind one effect");
 auto& binding=A::bindings.begin()->second;Require(binding.parent.Get()==common,"static effect anchor is still inside a switchable LOD branch");Tick();
 Require(binding.instance&&Active(binding.instance->object.Get()),"distant LOD effect remained under inactive LOD0");
 auto instance=binding.instance;auto first=instance->object.Get();auto clones=Test::cloneCalls;
 auto parent=EffectTest::cloneParents.at(first);Require(parent!=all[3]&&rotations[parent]==basis->nodes[1].rotation&&scales[parent]==basis->nodes[1].scale,"stable LOD anchor lost source rotation/scale basis");
 Require(positions[first]==effect.position,"stable LOD anchor changed author attachment offset");
 renderers=all;A::ModelReady(Test::oldRoot);Tick();Require(binding.renderers.size()==4&&binding.instance==instance,"late LOD discovery duplicated or restarted effect");
 for(unsigned lod=0;lod<4;++lod){inactive.clear();for(unsigned i=0;i<4;++i)if(i!=lod)inactive.insert(branches[i]);Tick();Require(binding.instance==instance&&Active(first)&&Test::cloneCalls==clones,"LOD switch restarted/disabled shared effect simulation");}
 inactive.insert(branches.begin(),branches.end());A::Frame();Require(!binding.instance,"all invisible LODs kept an effect active");inactive.erase(branches[2]);Tick();Require(binding.instance&&Active(binding.instance->object.Get()),"visible LOD did not resume effect");
 auto originalStamps=A::CaptureStamps();auto changed=p;auto shifted=std::make_shared<EffectLodAnchor>(*basis);shifted->nodes[1].position[0]=2;changed.effects[effect.id].lodAnchor=shifted;
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{changed});Require(A::CaptureStamps()!=originalStamps,"LOD basis edits were absent from reload identity");
 auto previous=binding.instance;A::ModelReady(Test::oldRoot);Tick();Require(binding.instance&&binding.instance!=previous,"LOD basis hot reload kept stale instance");
 Require(positions[EffectTest::cloneParents.at(binding.instance->object.Get())][0]==2,"LOD basis hot reload kept stale transform");previous.reset();instance.reset();
 auto graph=std::make_shared<AuthorTransformHierarchy>();graph->id="LodMount";graph->nodes.push_back({"Pivot","Pivot"});graph->nodes[0].position={0,1,0};
 auto& mounted=changed.effects[effect.id];mounted.hierarchy=graph;mounted.node="Pivot";
 ShapeAnimation clip;clip.id="Spin";clip.duration=2;clip.autoplay=true;clip.speedVariable="$speed";changed.defaults[clip.speedVariable]=1;clip.nativeOutputs.push_back({graph->id,"animations/spin.ab","assets/eff/spin.anim","revision1"});changed.shapeAnimations.emplace(clip.id,clip);
 RuntimeAuthorTransforms::api.animator=RuntimeShapes::Ref::Make((void*)0x1509,false);
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{changed});RuntimeControls::animations[p.id].Prepare(changed);A::ModelReady(Test::oldRoot);Tick();
 Require(binding.mount&&EffectTest::cloneParents.at(binding.instance->object.Get())==binding.mount->nodes.at("Pivot").Get(),"LOD basis bypassed author Empty mount");
 auto nativeClip=std::make_shared<RuntimeAuthorTransforms::Clip>();nativeClip->asset=RuntimeShapes::Ref::Make((void*)0x1510,false);nativeClip->duration=2;binding.mount->loaded[clip.id]=nativeClip;
 RuntimeControls::animations[p.id].Advance(changed,changed.defaults,.5);RuntimeAuthorTransforms::Frame();
 Require(sampledRoot==binding.mount->root.Get()&&sampledTime==.5f,"LOD basis changed native AnimationClip root/path or unified time");
 auto animated=binding.instance;inactive={branches[0],branches[1],branches[2]};Tick();RuntimeAuthorTransforms::Frame();Require(binding.instance==animated&&Active(animated->object.Get()),"animated Empty mount disappeared/restarted across LODs");animated.reset();nativeClip.reset();
 // F10 can change only a native Clip while the Mesh/effect/hierarchy stay
 // identical. Enter through definitionsChanged Frame, never direct Acquire.
 FullResourceReload::liveParts.clear();RuntimeShapes::api.modelRenderers=(void*)909;RuntimeShapes::enabled=true;RuntimeShapes::packageGeneration=Entry::generation.load();
 FullResourceReload::CompleteControlledClone(Test::oldRoot,true,effect.targetPrefab);
 A::activeStamps=A::CaptureStamps();A::stampsInitialized=true;A::revision=RuntimeControls::revision;
 auto priorTree=binding.mount;auto priorInstance=binding.instance;auto priorSignature=priorTree->signature;
 changed.shapeAnimations.at(clip.id).nativeOutputs.front().revision="revision2";
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{changed});RuntimeControls::animations[p.id].Prepare(changed);++RuntimeControls::revision;Tick();
 Require(binding.mount!=priorTree&&binding.mount->signature!=priorSignature&&binding.instance!=priorInstance,"animation-only F10 Frame retained a stale generic mount tree or effect instance");
 Require(priorInstance->removed&&priorInstance->anchorOwner,"animation-only F10 discarded the old mount before deferred native retirement");priorInstance.reset();priorTree.reset();
 A::OwnerReleasing(Test::oldRoot);Require(A::bindings.empty(),"LOD owner release retained effect bindings");
 for(auto object:EffectTest::destroyed)EffectTest::dead.insert(object);D::Collect();RuntimeAuthorTransforms::Frame();
 RuntimeControls::controlGraph.reset();RuntimeControls::variables.clear();RuntimeControls::specs.clear();RuntimeControls::animations.clear();RuntimeAuthorTransforms::api.animator={};il2cpp_runtime_invoke=EffectTest::Invoke;
 proofScope.reset();
 std::cout<<"PASS static effect LOD mounts: distant-only owner, exact consumers, stable source basis, one simulation across 4 LODs, Empty/Clip paths, visibility, reload and release\n";
 EffectTest::Teardown();
}
}
