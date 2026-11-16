#pragma once
namespace MaterialAnimationTest {
using namespace Entry;namespace M=RuntimeMaterials;
std::map<void*,std::map<int,float>> blocks,slots,globals;
std::map<void*,std::map<int,std::array<float,4>>> vectors,slotVectors;
std::map<void*,void*> assignedMaterials;std::map<void*,std::map<int,std::array<float,4>>> materialVectors;
std::set<void*> materialClones;unsigned nextMaterial=0;
unsigned writes=0;int propertyId=42;unsigned char empty=0;
void* Invoke(void* method,void* self,void** args,void** error){
 auto m=uintptr_t(method);if(m<1000)return EffectTest::Invoke(method,self,args,error);*error=nullptr;
 Require(GetCurrentThreadId()==EffectTest::unityThread,"material worker called Unity");
 if(m==1000)return Test::Array(Test::material);
 if(m==1001)return &Test::yes;if(m==1002)return &propertyId;if(m==1003)return nullptr;
 if(m==1004){blocks[args[0]]=slots[self];return nullptr;}if(m==1005){blocks[args[0]]=globals[self];return nullptr;}
 if(m==1006){empty=blocks[self].empty();return &empty;}
 if(m==1007){blocks[self][*(int*)args[0]]=*(float*)args[1];return nullptr;}
 if(m==1008){blocks[self]=blocks[args[0]];return nullptr;}
 if(m==1009){std::array<float,4> value;std::copy_n((float*)args[1],4,value.begin());vectors[self][*(int*)args[0]]=value;return nullptr;}
 if(m==1010){auto clone=(void*)(uintptr_t(0xBC00)+ ++nextMaterial);materialClones.insert(clone);return clone;}
 if(m==1011)return assignedMaterials.count(self)?assignedMaterials.at(self):Test::material;
 if(m==1012){assignedMaterials[self]=args[0];return nullptr;}
 if(m==1013){Require(materialClones.count(self),"animation mutated a shared source Material");std::array<float,4> value;std::copy_n((float*)args[1],4,value.begin());materialVectors[self][*(int*)args[0]]=value;return nullptr;}
 if(m==1014){Require(materialClones.count(self),"animation mutated a shared source Material");return nullptr;}
 throw std::runtime_error("unexpected material fixture method");
}
void Checks(){
 il2cpp_runtime_invoke=Invoke;M::api.blockClass=(void*)0xB001;M::api.materials=(void*)1000;M::api.has=(void*)1001;M::api.propertyId=(void*)1002;M::api.ctor=(void*)1003;
 M::api.getSlot=(void*)1004;M::api.getGlobal=(void*)1005;M::api.empty=(void*)1006;M::api.setFloat=(void*)1007;M::api.copy=(void*)1008;M::enabled=true;
 M::api.clone=(void*)1010;M::api.sharedMaterial=(void*)1011;M::api.setSharedMaterial=(void*)1012;M::api.materialVector=(void*)1013;M::api.materialFloat=(void*)1014;
 M::originalSlot=[](void* renderer,void* block,int slot){Require(slot==0,"material animation wrote unrelated slot");slots[renderer]=block?blocks[block]:std::map<int,float>{};slotVectors[renderer]=block?vectors[block]:std::map<int,std::array<float,4>>{};++writes;};
 M::originalGlobal=[](void* renderer,void* block){globals[renderer]=block?blocks[block]:std::map<int,float>{};};
 M::originalGetSlot=[](void* renderer,void* block,int){blocks[block]=slots[renderer];vectors[block]=slotVectors[renderer];};
 Package a,b;a.id="A";b.id="B";a.materialTracks["$a"]={"MaterialBody","_Threshold","FLOAT",0};b.materialTracks["$b"]={"MaterialBody","_Threshold","FLOAT",0};
 auto state=RuntimeControls::Prepare(std::make_shared<const std::vector<Package>>(std::vector<Package>{a,b}),EiemGlobalConfig{});RuntimeControls::Swap(state);
 RuntimeControls::animations[a.id].output["$a"]=.25;RuntimeControls::animations[b.id].output["$b"]=.75;RuntimeControls::Changed(a.id);RuntimeControls::Changed(b.id);
 globals[Test::oldRenderer]={{42,8},{99,12}};globals[Test::newRenderer]={{42,9},{99,15}};
 RuntimeControls::Spec sa;sa.package=a.id;sa.material="MaterialBody";sa.part="PartA";sa.marker="A";auto sb=sa;sb.package=b.id;sb.marker="B";
 M::Register(Test::oldRenderer,Test::oldMesh,sa);M::Register(Test::newRenderer,Test::newMesh,sb);M::revision=RuntimeControls::revision;M::Frame();
 Require(assignedMaterials.at(Test::oldRenderer)!=assignedMaterials.at(Test::newRenderer),"independent model/mod animations shared one mutable Material instance");
 Require(slots[Test::oldRenderer].at(42)==.25f&&slots[Test::newRenderer].at(42)==.75f&&slots[Test::oldRenderer].at(99)==12,"shared material/mod isolation or game global block preservation failed");
 auto gameBlock=(void*)0xB010;blocks[gameBlock]={{42,5},{99,22}};M::SetSlot(Test::oldRenderer,gameBlock,0);
 Require(slots[Test::oldRenderer].at(42)==.25f&&slots[Test::oldRenderer].at(99)==22,"game property block update lost animation or unrelated fields");
 auto readback=(void*)0xB011;M::GetSlot(Test::oldRenderer,readback,0);Require(blocks[readback].at(42)==5&&blocks[readback].at(99)==22,"game readback captured private animated values into its own state");
 RuntimeControls::animations[a.id].output.clear();RuntimeControls::Changed(a.id);M::SetSlot(Test::oldRenderer,gameBlock,0);M::Frame();Require(slots[Test::oldRenderer].at(42)==5&&slots[Test::oldRenderer].at(99)==22,"stop/game slot update did not restore latest game slot block");
 Require(assignedMaterials.at(Test::oldRenderer)==Test::material,"stop retained an animated Material instead of restoring the donor");
 blocks[gameBlock]={{42,17},{99,31}};M::SetGlobal(Test::newRenderer,gameBlock);Require(slots[Test::newRenderer].at(42)==.75f&&slots[Test::newRenderer].at(99)==31,"global game updates lost while animated slot active");
 RuntimeControls::animations[b.id].output.clear();RuntimeControls::Changed(b.id);M::Frame();Require(slots[Test::newRenderer].empty()&&globals[Test::newRenderer].at(42)==17,"stop failed to remove originally absent material block");
 RuntimeControls::animations[b.id].output["$b"]=.75;RuntimeControls::Changed(b.id);M::Frame();auto frozenWrites=writes;M::Frame();Require(writes==frozenWrites,"paused/constant material animation repeated native writes");RuntimeControls::animations[b.id].output.clear();RuntimeControls::Changed(b.id);M::Frame();
 auto before=writes;M::Frame();Require(writes==before,"idle material player kept rewriting native blocks");
 M::Forget(Test::oldRenderer);M::Forget(Test::newRenderer);
 // A live renderer is itself a sample consumer, including after its loader's
 // temporary native Bundle proxy has retired. Exercise the real lazy player,
 // vector setter and per-slot write together instead of injecting output.
 Package uv;uv.id="UV";auto activity=std::make_shared<EiemAnimationSamples::Activity>();
 auto samples=std::make_shared<EiemAnimationSamples::Source>();samples->activity=activity;
 unsigned reads=0;samples->read=[&](){++reads;EiemAnimationSamples::Data data;data.scalar={{1,-1,1},{0,0,0},{0,0,0},{1,-1,1}};return data;};
 ShapeAnimation clip;clip.id="Rotate";clip.duration=2;clip.speedVariable="$speed";clip.loop=clip.autoplay=true;clip.sampleSource=samples;uv.defaults["$speed"]=1;
 for(unsigned i=0;i<4;++i){auto variable="$uv"+std::to_string(i);uv.defaults[variable]=i==0||i==3?1:0;uv.materialTracks[variable]={"MaterialBody","_MainTexUVRotateMat","VECTOR",i};clip.tracks[variable]={};clip.scalarChannels[variable]=i;}
 uv.shapeAnimations.emplace(clip.id,clip);
 auto uvState=RuntimeControls::Prepare(std::make_shared<const std::vector<Package>>(std::vector<Package>{uv}),EiemGlobalConfig{});RuntimeControls::Swap(uvState);
 Require(reads==0,"unmatched material autoplay eagerly read samples");
 M::api.setVector=(void*)1009;auto uvSpec=sa;uvSpec.package=uv.id;
 M::Register(Test::oldRenderer,Test::oldMesh,uvSpec);M::revision=RuntimeControls::revision;
 RuntimeControls::AdvanceAnimations(1);M::Frame();
 Require(reads==1&&slotVectors[Test::oldRenderer].count(42)&&slotVectors[Test::oldRenderer].at(42)[0]==-1,"live material renderer did not activate and apply sampled UV matrix");
 Require(assignedMaterials.count(Test::oldRenderer)&&materialClones.count(assignedMaterials.at(Test::oldRenderer))&&materialVectors[assignedMaterials.at(Test::oldRenderer)].at(42)[0]==-1,"material-buffer rendering still saw a static shared Material while the property block advanced");
 // Publish through the real control revision seam with an existing live
 // Renderer and a new donor, as a material generation handoff can do.
 struct ShapeBindings {decltype(RuntimeShapes::bound) saved;ShapeBindings(){saved.swap(RuntimeShapes::bound);}~ShapeBindings(){RuntimeShapes::bound.clear();saved.swap(RuntimeShapes::bound);}} shapeBindings;
 RuntimeShapes::Bound model;model.renderer=RuntimeShapes::Ref::Make(Test::oldRenderer,true);model.mesh=RuntimeShapes::Ref::Make(Test::oldMesh,false);model.marker=uvSpec.marker;model.modelReady=true;RuntimeShapes::bound.emplace(Test::oldRenderer,std::move(model));
 auto retiredMaterial=assignedMaterials.at(Test::oldRenderer);auto newDonor=(void*)0xBD00;assignedMaterials[Test::oldRenderer]=newDonor;
 auto reload=RuntimeControls::Prepare(std::make_shared<const std::vector<Package>>(std::vector<Package>{uv}),EiemGlobalConfig{});reload.specs.emplace(uvSpec.marker,uvSpec);RuntimeControls::MergeLive(reload);RuntimeControls::Swap(reload);M::Frame();
 Require(EffectTest::destroyed.count(retiredMaterial)&&M::bound.at(Test::oldRenderer).sourceMaterial.Get()==newDonor&&assignedMaterials.at(Test::oldRenderer)!=retiredMaterial,"F10 retained its private animated Material or restored an obsolete donor over the new material");
 Require(RuntimeControls::animations.at(uv.id).clips.at(clip.id).time==1&&materialVectors.at(assignedMaterials.at(Test::oldRenderer)).at(42)[0]==-1,"F10 failed to reapply compatible animation phase to the new Material");
 M::Register(Test::newRenderer,Test::newMesh,uvSpec);M::Forget(Test::oldRenderer);
 RuntimeControls::AdvanceAnimations(.5);M::Frame();Require(activity->owners!=0&&slotVectors[Test::newRenderer].at(42)[0]==0,"one renderer retirement stopped another material instance");
 M::Forget(Test::newRenderer);RuntimeControls::AdvanceAnimations(.1);Require(activity->owners==0&&RuntimeControls::animations.at(uv.id).output.empty(),"final material renderer retirement retained sample ownership");
 for(auto clone:materialClones)Require(EffectTest::destroyed.count(clone),"stopped/released material animation leaked its native clone");
 M::api={};M::enabled=false;RuntimeControls::animations.clear();RuntimeControls::variables.clear();RuntimeControls::effectiveValues.clear();RuntimeControls::continuousChannels.clear();RuntimeControls::controlGraph.reset();il2cpp_runtime_invoke=EffectTest::Invoke;
 std::cout<<"PASS material runtime: two Mods/instances sharing donor, per-slot isolation, game slot/global updates, unrelated fields, stop restoration and idle zero writes\n";
}
}
