#pragma once
namespace MaterialAnimationTest {
using namespace Entry;namespace M=RuntimeMaterials;
std::map<void*,std::map<int,float>> blocks,slots,globals;
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
 throw std::runtime_error("unexpected material fixture method");
}
void Checks(){
 il2cpp_runtime_invoke=Invoke;M::api.blockClass=(void*)0xB001;M::api.materials=(void*)1000;M::api.has=(void*)1001;M::api.propertyId=(void*)1002;M::api.ctor=(void*)1003;
 M::api.getSlot=(void*)1004;M::api.getGlobal=(void*)1005;M::api.empty=(void*)1006;M::api.setFloat=(void*)1007;M::api.copy=(void*)1008;M::enabled=true;
 M::originalSlot=[](void* renderer,void* block,int slot){Require(slot==0,"material animation wrote unrelated slot");slots[renderer]=block?blocks[block]:std::map<int,float>{};++writes;};
 M::originalGlobal=[](void* renderer,void* block){globals[renderer]=block?blocks[block]:std::map<int,float>{};};
 M::originalGetSlot=[](void* renderer,void* block,int){blocks[block]=slots[renderer];};
 Package a,b;a.id="A";b.id="B";a.materialTracks["$a"]={"MaterialBody","_Threshold","FLOAT",0};b.materialTracks["$b"]={"MaterialBody","_Threshold","FLOAT",0};
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{a,b});RuntimeControls::animations[a.id].output["$a"]=.25;RuntimeControls::animations[b.id].output["$b"]=.75;
 globals[Test::oldRenderer]={{42,8},{99,12}};globals[Test::newRenderer]={{42,9},{99,15}};
 RuntimeControls::Spec sa;sa.package=a.id;sa.material="MaterialBody";sa.part="PartA";sa.marker="A";auto sb=sa;sb.package=b.id;sb.marker="B";
 M::Register(Test::oldRenderer,Test::oldMesh,sa);M::Register(Test::newRenderer,Test::newMesh,sb);M::revision=RuntimeControls::revision;M::Frame();
 Require(slots[Test::oldRenderer].at(42)==.25f&&slots[Test::newRenderer].at(42)==.75f&&slots[Test::oldRenderer].at(99)==12,"shared material/mod isolation or game global block preservation failed");
 auto gameBlock=(void*)0xB010;blocks[gameBlock]={{42,5},{99,22}};M::SetSlot(Test::oldRenderer,gameBlock,0);
 Require(slots[Test::oldRenderer].at(42)==.25f&&slots[Test::oldRenderer].at(99)==22,"game property block update lost animation or unrelated fields");
 auto readback=(void*)0xB011;M::GetSlot(Test::oldRenderer,readback,0);Require(blocks[readback].at(42)==5&&blocks[readback].at(99)==22,"game readback captured private animated values into its own state");
 RuntimeControls::animations[a.id].output.clear();M::Frame();Require(slots[Test::oldRenderer].at(42)==5&&slots[Test::oldRenderer].at(99)==22,"stop did not restore latest game slot block");
 blocks[gameBlock]={{42,17},{99,31}};M::SetGlobal(Test::newRenderer,gameBlock);Require(slots[Test::newRenderer].at(42)==.75f&&slots[Test::newRenderer].at(99)==31,"global game updates lost while animated slot active");
 RuntimeControls::animations[b.id].output.clear();M::Frame();Require(slots[Test::newRenderer].empty()&&globals[Test::newRenderer].at(42)==17,"stop failed to remove originally absent material block");
 RuntimeControls::animations[b.id].output["$b"]=.75;M::Frame();auto frozenWrites=writes;M::Frame();Require(writes==frozenWrites,"paused/constant material animation repeated native writes");RuntimeControls::animations[b.id].output.clear();M::Frame();
 auto before=writes;M::Frame();Require(writes==before,"idle material player kept rewriting native blocks");
 M::Forget(Test::oldRenderer);M::Forget(Test::newRenderer);M::api={};M::enabled=false;RuntimeControls::animations.clear();RuntimeControls::controlGraph.reset();il2cpp_runtime_invoke=EffectTest::Invoke;
 std::cout<<"PASS material runtime: two Mods/instances sharing donor, per-slot isolation, game slot/global updates, unrelated fields, stop restoration and idle zero writes\n";
}
}
