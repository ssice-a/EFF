#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace Test {
void *source=(void*)0x1000,*candidate=(void*)0x1001,*oldRoot=(void*)0x1002,*newRoot=(void*)0x1003,*restoredRoot=(void*)0x1004;
void *model=(void*)0x1010,*entity=(void*)0x1011,*instance=(void*)0x1012,*manager=(void*)0x1013,*effect=(void*)0x1014;
void *helper=(void*)0x1015,*mainPart=(void*)0x1016;
void *animation=(void*)0x1017,*animationData=(void*)0x1018,*animator=(void*)0x1019,*weapons=(void*)0x101a,*audio=(void*)0x101b,*audioClass=(void*)0x2010;
void* animationRoot=(void*)0x1002;unsigned animationRebinds=0,weaponUnloads=0,weaponReloads=0,weaponRemounts=0,audioInvalidations=0;
int nativeFactory=1;bool ignoreCandidate=false;
void *oldTexture=(void*)0x4103,*newTexture=(void*)0x4104,*textureValue=(void*)0x4103;bool identitySameMesh=false;
void *factoryParent=(void*)0x4100,*factoryPosition=(void*)0x4101,*factoryRotation=(void*)0x4102;
void *oldRenderer=(void*)0x1020,*newRenderer=(void*)0x1021,*oldMesh=(void*)0x1022,*newMesh=(void*)0x1023;
void *material=(void*)0x1030,*shader=(void*)0x1031,*bone=(void*)0x1032,*pathValue=(void*)0x1033,*leaseHandle=(void*)0x1034;
void *createRequest=(void*)0x1040,*bundle=(void*)0x1041,*assetRequest=(void*)0x1042;
void *handleClass=(void*)0x2000,*pathClass=(void*)0x2001,*instanceClass=(void*)0x2002,*configClass=(void*)0x2003,*configBox=(void*)0x2004;
void *prefabTransform=(void*)0x3000,*skeletonRoot=(void*)0x3010,*bip=(void*)0x3020,*renderTransform=(void*)0x3030;
void *left=(void*)0x3040,*right=(void*)0x3050,*leftBone=(void*)0x3060,*rightBone=(void*)0x3070,*foreignRoot=(void*)0x3080;
std::map<void*,void*> transformParents;
std::map<void*,bool> scheduleDone;
std::map<void*,void*> scheduleBundles;
unsigned opened=0,leaseRequests=0;
std::map<uint32_t,void*> handles;uint32_t sequence=0;
unsigned cloneCalls=0,removeCalls=0,discardCalls=0,loadCalls=0,followCalls=0,disposeCalls=0,unloadCalls=0;
bool managersStatic=true;
bool hasInstance=true;unsigned instanceCalls=0;
unsigned char yes=1,no=0,done=0;int two=2,zero=0,badVertex=7,id=0;
bool rejectVertex=false,sourceAlive=true,throwDispose=false,hasHelper=true,hasEntity=true,hasModel=true,hasPart=true,released=false,helperMatches=true,rendererInside=true;
std::string modelPath="assets/model.prefab";void* currentRoot=oldRoot;
std::vector<std::unique_ptr<Bytes>> storage;
void* StringValue(const std::string& value){
 auto b=std::make_unique<Bytes>(20+value.size()*2);int count=int(value.size());memcpy(b->data()+16,&count,4);
 for(size_t i=0;i<value.size();++i){uint16_t c=uint8_t(value[i]);memcpy(b->data()+20+i*2,&c,2);}auto p=b->data();storage.push_back(std::move(b));return p;
}
void* Array(void* value){auto b=std::make_unique<Bytes>(40);uint64_t count=1;memcpy(b->data()+24,&count,8);memcpy(b->data()+32,&value,8);auto p=b->data();storage.push_back(std::move(b));return p;}
uint32_t Root(void* p,bool){handles[++sequence]=p;return sequence;}
void* Target(uint32_t h){return handles.at(h);}
void Free(uint32_t h){Require(handles.erase(h)==1,"fixture handle double free");}
void* GetClass(void* p){return p==audio?audioClass:p==leaseHandle?handleClass:p==pathValue?pathClass:p==configBox?configClass:instanceClass;}
void* Fields(void*,void** it){auto n=uintptr_t(*it);if(n==8)return nullptr;*it=(void*)(n+1);return (void*)(n+1);}
const char* FieldName(void* f){const char* names[]={"","modelManager","effectManager","m_entity","m_mainPart","m_cfg","usePersistentPool","modelPath","m_model"};return names[uintptr_t(f)];}
const char* TypeName(void* t){const char* names[]={"","Beyond.Gameplay.View.ModelManager","Beyond.Gameplay.EffectManager","Beyond.Gameplay.Core.Entity","Beyond.Gameplay.View.ComplexModelViewPart","Beyond.Gameplay.View.BaseModelViewPartData","System.Boolean","System.String","UnityEngine.GameObject"};return names[uintptr_t(t)];}
int FieldFlags(void* f){return (f==(void*)1||f==(void*)2)&&managersStatic?0x16:0x6;}
void FieldValue(void* self,void* f,void* out){Require(!(FieldFlags(f)&0x10)&&self,"instance getter used for a static field");if(f==(void*)5){memset(out,0,64);return;}if(f==(void*)6){*(unsigned char*)out=0;return;}
 auto v=f==(void*)1?manager:f==(void*)2?effect:f==(void*)3?(hasEntity?entity:nullptr):f==(void*)4?(hasPart?mainPart:nullptr):f==(void*)7?StringValue(modelPath):currentRoot;memcpy(out,&v,8);}
void StaticFieldValue(void* f,void* out){Require((FieldFlags(f)&0x10)&&(f==(void*)1||f==(void*)2),"static getter used for an instance field");auto v=f==(void*)1?manager:effect;memcpy(out,&v,8);}
void* NativeClone(void* p){++cloneCalls;return p==candidate&&!ignoreCandidate?newRoot:restoredRoot;}
void* NativePosition(void* p,void* position,void* rotation){Require(position==factoryPosition&&rotation==factoryRotation,"Instantiate position/rotation changed");return NativeClone(p);}
void* NativeParentPosition(void* p,void* parent,void* position,void* rotation){Require(parent==factoryParent,"Instantiate parent changed");return NativePosition(p,position,rotation);}
void* NativeParent(void* p,void* parent,bool world){Require(parent==factoryParent&&world,"Instantiate parent/world-space flag changed");return NativeClone(p);}
void* NativeAsync(void* p,int count,void* parent,void* position,void* rotation){Require(count==1,"Instantiate async count changed");return NativeParentPosition(p,parent,position,rotation);}
void* NativeCreate(void* p){
 using namespace FullResourceReload;
 switch(nativeFactory){
 case 0:return Clone(p);case 1:return Instantiate(p,factoryPosition,factoryRotation);
 case 2:return InstantiateParent(p,factoryParent,factoryPosition,factoryRotation);
 case 3:return InstantiateImmediate(p,factoryParent,true);case 4:return InstantiateAsync(p,1,factoryParent,factoryPosition,factoryRotation);
 default:return CloneParent(p,factoryParent,true);
 }
}
void* Mesh(void* renderer){return renderer==newRenderer||identitySameMesh?newMesh:oldMesh;}
void* Invoke(void* method,void* self,void** args,void** exception){
 *exception=nullptr;auto m=uintptr_t(method);
 if(m==1)return &yes;
 if(m==2){auto p=args[0];return !p||(p==source&&!sourceAlive)?&no:&yes;}
 if(m==3){id=int(uintptr_t(self));return &id;}
 if(m==4)return StringValue(self==material?"body":self==skeletonRoot?"Root":self==bip?"Bip001":self==left?"Left":self==right?"Right":(self==bone||self==leftBone||self==rightBone)?"Bone":"marker");
 if(m==5)return Array(self==newRoot||self==candidate?newRenderer:oldRenderer);
 if(m==6)return self==newMesh&&rejectVertex?&badVertex:&two;
 if(m==7)return &zero;
 if(m==8)return Array(bone);
 if(m==9)return Array(nullptr);
 if(m==10)return bone;
 if(m==11)return Array(material);
 if(m==12)return shader;
 if(m==20){++instanceCalls;return hasInstance?instance:nullptr;}
 if(m==21)return currentRoot;
 if(m==22){++removeCalls;currentRoot=nullptr;return nullptr;}
 if(m==23){Require(!currentRoot,"pool discarded before old owner released");++discardCalls;return nullptr;}
 if(m==24){Require(discardCalls==removeCalls,"model loaded before path-scoped pool discard");++loadCalls;currentRoot=NativeCreate(source);return nullptr;}
 if(m==25){++followCalls;return nullptr;}
 if(m==26)return released?&yes:&no;
 if(m==30){++disposeCalls;if(throwDispose)throw std::runtime_error("fixture Dispose threw after side effect");return nullptr;}
 if(m==31){Require(self,"polled a null native request");auto it=scheduleDone.find(self);return it==scheduleDone.end()?&done:it->second?&yes:&no;}
 if(m==32){auto it=scheduleBundles.find(self);return it==scheduleBundles.end()?bundle:it->second;}
 if(m==33){Require(args&&*(unsigned char*)args[0]==0,"unloaded candidate objects still in use");++unloadCalls;return nullptr;}
 if(m==40){Require(self==oldRenderer||self==newRenderer||self==animator,"Component getter invoked on a GameObject");return renderTransform;}
 if(m==41){Require(self==oldRoot||self==newRoot||self==restoredRoot||self==candidate,"GameObject getter invoked on a Component");return prefabTransform;}
 if(m==42)return transformParents.at(self);
 if(m==50){auto request=(void*)(uintptr_t(0x8000)+ ++opened);scheduleDone[request]=false;scheduleBundles[request]=(void*)(uintptr_t(0x9000)+opened);return request;}
 if(m==51){++leaseRequests;return leaseHandle;}
 if(m==52)return pathValue;
 if(m==53)return hasHelper?helper:nullptr;
 if(m==54)return hasModel?model:nullptr;
 if(m==55)return StringValue(modelPath);
 if(m==56)return helperMatches?helper:nullptr;
 if(m==57)return rendererInside?&yes:&no;
 if(m==58)return assetRequest;
 if(m==59)return candidate;
 if(m==60)return source;
 if(m==61)return &no;
 if(m==62){Require(discardCalls==removeCalls,"display loaded before old root pool discard");++loadCalls;currentRoot=NativeCreate(source);return &yes;}
 if(m==63){Require(args&&args[0]==configBox,"display SetupData did not use preserved value config");return nullptr;}
 if(m==64)return animation;
 if(m==65)return animationData;
 if(m==66)return animator;
 if(m==67)return animationRoot==currentRoot?&yes:&no;
 if(m==68){Require(self==animation&&args&&args[0]==animationData&&*(unsigned char*)args[1]==1,"native animation rebind did not preserve data/resume option");++animationRebinds;animationRoot=currentRoot;return &yes;}
 if(m==69)return weapons;
 if(m==70){Require(self==weapons&&args&&*(unsigned char*)args[0]==1,"weapon unload arguments differ");++weaponUnloads;return nullptr;}
 if(m==71){Require(animationRoot==currentRoot&&self==weapons&&*(unsigned char*)args[0]==1,"weapons reloaded before animation rebound");++weaponReloads;return nullptr;}
 if(m==72){++weaponRemounts;return nullptr;}
 if(m==73)return audio;
 if(m==74){++audioInvalidations;return nullptr;}
 if(m==75)return textureValue;
 throw std::runtime_error("unexpected fixture managed call: "+std::to_string(m));
}
void Setup(){
 il2cpp_gchandle_new=Root;il2cpp_gchandle_get_target=Target;il2cpp_gchandle_free=Free;il2cpp_object_get_class=GetClass;
 RuntimeShapes::Ref::weakHandle=Root;
 il2cpp_class_get_name=[](void* klass){return klass==audioClass?"CharacterAudioComponent":"fixture";};il2cpp_class_get_namespace=[](void*){return "Beyond.Gameplay.Core";};
 il2cpp_object_unbox=[](void* p){return p;};il2cpp_class_get_fields=Fields;il2cpp_class_get_parent=[](void*)->void*{return nullptr;};
 il2cpp_field_get_name=FieldName;il2cpp_field_get_flags=FieldFlags;il2cpp_field_static_get_value=StaticFieldValue;il2cpp_field_get_type=[](void* p){return p;};
 il2cpp_type_get_name=TypeName;il2cpp_free=[](void*){};il2cpp_field_get_value=FieldValue;il2cpp_runtime_invoke=Invoke;
 il2cpp_class_from_type=[](void*)->void*{return configClass;};il2cpp_class_value_size=[](void*,uint32_t* alignment)->int32_t{*alignment=8;return 64;};il2cpp_value_box=[](void*,void*)->void*{return configBox;};
 il2cpp_string_new=[](const char* value)->void*{return StringValue(value);};
 RuntimeShapes::api.main=(void*)1;RuntimeShapes::api.alive=(void*)2;RuntimeShapes::api.name=(void*)4;
 RuntimeShapes::api.vertexCount=(void*)6;RuntimeShapes::api.shapeCount=(void*)7;RuntimeShapes::api.bones=(void*)8;
 RuntimeShapes::api.bindPoses=(void*)9;RuntimeShapes::api.rootBone=(void*)10;RuntimeShapes::originalMeshGetter=Mesh;
 RuntimeShapes::api.transform=(void*)40;
 auto& a=FullResourceReload::api;a.instanceId=(void*)3;a.components=(void*)5;a.materials=(void*)11;a.shader=(void*)12;
 a.texture=(void*)75;
 a.instance=(void*)20;a.getModel=(void*)21;a.remove=(void*)22;a.discard=(void*)23;a.loadModel=(void*)24;a.follow=(void*)25;a.released=(void*)26;
 a.animationCom=(void*)64;a.animationData=(void*)65;a.animationAnimator=(void*)66;a.animationValid=(void*)67;a.animationChange=(void*)68;
 a.weaponCom=(void*)69;a.weaponUnload=(void*)70;a.weaponReload=(void*)71;a.weaponMounts=(void*)72;a.audioCom=(void*)73;a.audioInvalidate=(void*)74;
 a.modelManager=FullResourceReload::BindObjectField(instanceClass,"modelManager","Beyond.Gameplay.View.ModelManager");
 a.effectManager=FullResourceReload::BindObjectField(instanceClass,"effectManager","Beyond.Gameplay.EffectManager");
 a.pathClass=pathClass;a.handleClass=handleClass;a.dispose=(void*)30;a.done=(void*)31;
 a.goTransform=(void*)41;a.parent=(void*)42;
 a.load=(void*)51;a.toPath=(void*)52;a.parentHelper=(void*)53;a.modelCom=(void*)54;a.getPath=(void*)55;a.getHelper=(void*)56;a.isChild=(void*)57;
 a.get=(void*)60;a.error=(void*)61;a.partRelease=(void*)22;a.partLoad=(void*)62;a.partSetup=(void*)63;NativeCandidate::api.assetLoad=(void*)58;NativeCandidate::api.result=(void*)59;
 NativeCandidate::api.done=(void*)31;NativeCandidate::api.bundle=(void*)32;NativeCandidate::api.unload=(void*)33;
 FullResourceReload::originalClone=NativeClone;
 FullResourceReload::originalCloneParent=NativeParent;FullResourceReload::originalInstantiate=NativePosition;FullResourceReload::originalInstantiateParent=NativeParentPosition;
 FullResourceReload::originalInstantiateImmediate=NativeParent;FullResourceReload::originalInstantiateAsync=NativeAsync;
}
std::shared_ptr<FullResourceReload::Prepared> Prepare(){
 auto prepared=std::make_shared<FullResourceReload::Prepared>();prepared->base=2;
 prepared->controls.graph=std::make_shared<const std::vector<Package>>();prepared->config=EiemEntrySettings::Prepare(EiemGlobalConfig{});
 auto plan=std::make_shared<EiemResourceReloadPlan::Plan>();plan->parts.emplace("marker",EiemResourceReloadPlan::Part{"part","marker","material",2,{"Root/Bip001/Bone"},{}});
 plan->materials.emplace("material",EiemResourceReloadPlan::Material{"body",{}});plan->roots.push_back({"assets/model.prefab",0,{{"marker",1}}});prepared->plan=plan;return prepared;
}
FullResourceReload::Attempt MakeAttempt(){
 FullResourceReload::Attempt a;a.prepared=Prepare();
 FullResourceReload::Lease lease;lease.source=RuntimeShapes::Ref::Make(source,false);a.leases.push_back(lease);
 FullResourceReload::Asset asset;asset.prefab=RuntimeShapes::Ref::Make(candidate,false);a.assets.push_back(asset);
 FullResourceReload::Owner owner;owner.entity=RuntimeShapes::Ref::Make(entity,false);owner.component=RuntimeShapes::Ref::Make(model,false);owner.root=RuntimeShapes::Ref::Make(oldRoot,false);
 owner.pathHash=RuntimeShapes::Ref::Make(pathValue,false);owner.path="assets/model.prefab";owner.oldRootId=int(uintptr_t(oldRoot));owner.originalMeshes=FullResourceReload::MeshProfile(oldRoot);a.owners.push_back(owner);return a;
}
void Reset(){
 std::atomic_store(&FullResourceReload::routes,std::shared_ptr<const FullResourceReload::Routes>(std::make_shared<FullResourceReload::Routes>()));
 currentRoot=oldRoot;removeCalls=discardCalls=loadCalls=cloneCalls=followCalls=disposeCalls=unloadCalls=0;rejectVertex=false;sourceAlive=true;throwDispose=false;done=0;generation=2;
 animationRoot=oldRoot;animationRebinds=weaponUnloads=weaponReloads=weaponRemounts=audioInvalidations=0;ignoreCandidate=false;
 identitySameMesh=false;textureValue=oldTexture;
 scheduleDone.clear();scheduleBundles.clear();opened=leaseRequests=0;
 hasHelper=hasEntity=hasModel=hasPart=helperMatches=rendererInside=true;released=false;modelPath="assets/model.prefab";
 RuntimeShapes::bound.clear();RuntimeShapes::boundCount=0;
 FullResourceReload::sourceLeases.clear();FullResourceReload::liveParts.clear();
 RuntimeControls::specs.clear();RuntimeControls::Spec spec;spec.marker="marker";RuntimeControls::specs.emplace("marker",spec);
 transformParents={{prefabTransform,nullptr},{skeletonRoot,prefabTransform},{bip,skeletonRoot},{renderTransform,prefabTransform},{bone,bip},{left,bip},{right,bip},{leftBone,left},{rightBone,right},{foreignRoot,nullptr}};
}
void IdlePoolRegression(){
 using namespace FullResourceReload;Reset();auto a=MakeAttempt();a.owners.clear();currentRoot=nullptr;
 Rebuild(a);Require(a.phase==Phase::Passed&&discardCalls==1&&!removeCalls&&!loadCalls,"F10 did not discard a matched prefab's idle pool without a live owner");Reset();
}
void RootPathRegression(){
 using namespace FullResourceReload;auto prepared=Prepare();
 Require(FindRoot(*prepared->plan,"Assets/Beyond/DynamicAssets/Gameplay/Actors/PostModels/Characters/chr_0032_lizhiyan_postmodel.prefab")==nullptr,
         "unrelated root unexpectedly matched");
 prepared->plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);
 auto mutablePlan=std::const_pointer_cast<EiemResourceReloadPlan::Plan>(prepared->plan);
 mutablePlan->roots.front().path="assets/beyond/dynamicassets/gameplay/actors/postmodels/characters/chr_0032_lizhiyan_postmodel.prefab";
 Require(FindRoot(*mutablePlan,"Assets/Beyond/DynamicAssets/Gameplay/Actors/PostModels/Characters/chr_0032_lizhiyan_postmodel.prefab"),
         "Unity display-case asset path did not match the offline root");
 Require(SameAssetPath("ASSETS\\BEYOND\\actor.prefab","assets/beyond/actor.prefab"),
         "slash and case normalization differs");
}
void ManagerContractRegression(){
 using namespace FullResourceReload;
 for(bool isStatic:{true,false}){
  managersStatic=isStatic;Setup();Reset();
  Require(api.modelManager.isStatic==isStatic&&api.effectManager.isStatic==isStatic,"manager access mode did not follow metadata");
  instanceCalls=0;hasInstance=!isStatic;
  Require(ModelManager()==manager&&ReadManager(api.effectManager)==effect,"manager field getter returned wrong object");
  Require(instanceCalls==(isStatic?0u:2u),"manager getter used GameInstance for static storage or skipped its instance owner");
  hasInstance=true;
  auto a=MakeAttempt();Rebuild(a);
  Require(a.phase==Phase::Passed&&currentRoot==newRoot&&followCalls==1,"manager contract failed during game-owned rebuild");
  bool refused=false;try{BindObjectField(instanceClass,"modelManager","Beyond.Gameplay.EffectManager");}catch(const std::exception&){refused=true;}
  Require(refused,"manager type validation was bypassed when choosing getter");
 }
 managersStatic=true;Setup();Reset();
}
void NativeLifecycleRegression(){
 using namespace FullResourceReload;
 for(int entry=0;entry<6;++entry){
  Reset();nativeFactory=entry;auto a=MakeAttempt();CaptureConsumers(a.owners.front());Rebuild(a);
  Require(a.phase==Phase::Passed&&currentRoot==newRoot&&animationRoot==newRoot,"native factory/animation lifecycle did not migrate owner");
  Require(animationRebinds==1&&weaponUnloads==1&&weaponReloads==1&&weaponRemounts==1&&audioInvalidations==1,"native consumer lifecycle missing or repeated");
 }
 Reset();nativeFactory=1;
 // Reproduce the old release/load-only sequence: a new root exists, but the
 // animation component still targets the previous skeleton (T pose).
 {auto a=MakeAttempt();auto& owner=a.owners.front();CaptureConsumers(owner);RemoveOwner(owner);void* args[]={Self(owner.pathHash,api.pathClass)};Call(api.discard,manager,args);LoadOwner(owner);
  bool refused=false;try{VerifyAnimation(owner,currentRoot);}catch(const std::exception&){refused=true;}
  Require(refused&&animationRoot==oldRoot&&currentRoot!=oldRoot,"fixture did not reproduce stale animation after primitive model reload");
  ReloadConsumers(owner,currentRoot);VerifyAnimation(owner,currentRoot);
 }
 Reset();
 // A wrong factory can return old assets with exactly the same names, sizes
 // and vertex counts. Identity validation must reject, then restore animation.
 {auto a=MakeAttempt();CaptureConsumers(a.owners.front());ignoreCandidate=true;bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
  Require(refused&&currentRoot==restoredRoot&&animationRoot==restoredRoot&&animationRebinds==1,"old-asset rejection did not restore animation during rollback");
  Require(generation==2&&std::atomic_load(&routes)->empty(),"old-asset rejection published candidate state");
 }
 Reset();
}
void TextureIdentityRegression(){
 using namespace FullResourceReload;Reset();auto p=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*p->plan);
 plan->materials.at("material").slots.push_back({"_BaseMap",true,true,{"body",1024,1024,11}});
 plan->materials.at("material").slots.push_back({"_Unused",false,false,{}});
 identitySameMesh=true;textureValue=newTexture;auto expected=PrefabAssetIdentity(candidate,*plan);
 textureValue=oldTexture;auto stale=PrefabAssetIdentity(newRoot,*plan);
 Require(stale.at("marker").first==expected.at("marker").first&&stale!=expected,"same Mesh/name/texture dimensions concealed old Texture object");
 textureValue=newTexture;Require(PrefabAssetIdentity(newRoot,*plan)==expected,"candidate texture identity did not match live root");Reset();
}
void EnumRegression(){
 struct Case{const char* type;int width;uint64_t bits;};
 static Case current;
 for(const auto& item:std::vector<Case>{{"System.Byte",1,0x80},{"System.SByte",1,0xFE},{"System.UInt16",2,0xBEEF},{"System.Int16",2,0xFFEF},{"System.Int32",4,0xFFFFFFFF},{"System.UInt32",4,0xF0000001},{"System.Int64",8,0xF123456789ABCDEF},{"System.UInt64",8,0xFEDCBA9876543210}}){
  current=item;
  il2cpp_class_value_size=[](void*,uint32_t* alignment)->int32_t{*alignment=unsigned(current.width);return current.width;};
  il2cpp_class_get_fields=[](void*,void** iterator)->void*{auto i=uintptr_t(*iterator);if(i==2)return nullptr;*iterator=(void*)(i+1);return (void*)(i+1);};
  il2cpp_class_get_type=[](void*)->void*{return (void*)2;};
  il2cpp_field_get_type=[](void* field)->void*{return field;};
  il2cpp_field_get_name=[](void* field){return field==(void*)1?"value__":"Main";};
  il2cpp_field_get_flags=[](void* field){return field==(void*)1?0:0x50;};
  il2cpp_type_get_name=[](void* type){return type==(void*)1?current.type:"Beyond.Resource.RootCategory";};
  il2cpp_field_static_get_value=[](void*,void* destination){memcpy(destination,&current.bits,current.width);};
  auto value=FullResourceReload::Enum((void*)0x3000,"Main");uint64_t bits=0;memcpy(&bits,&value,current.width);
  Require(bits==current.bits&&value.width==size_t(current.width),"enum argument literal bytes truncated or sign extended");
  Require(value.Data()==&value.bits,"enum managed argument points to descriptor instead of underlying value");
 }
 auto rejects=[&](const char* literal){bool refused=false;try{FullResourceReload::Enum((void*)0x3000,literal);}catch(const std::exception&){refused=true;}Require(refused,"malformed enum accepted");};
 current={"System.Byte",4,0};rejects("Main");
 current={"System.Single",4,0};rejects("Main");
 current={"System.UInt32",4,0};rejects("MissingLiteral");
 Setup();
}
void BonePathRegression(){
 EiemResourceReloadPlan::Part part{"bone-test","marker","material",2,{"Root/Bip001","Root/Bip001/Left/Bone","Root/Bip001/Right/Bone"},{}};
 std::vector<void*> ordered={bip,leftBone,rightBone};
 FullResourceReload::BoneHierarchy hierarchy(newRoot);hierarchy.Verify(newRenderer,ordered,bip,part);
 auto refused=[&](const std::vector<void*>& palette,void* root){bool rejected=false;try{FullResourceReload::BoneHierarchy test(newRoot);test.Verify(newRenderer,palette,root,part);}catch(const std::exception&){rejected=true;}Require(rejected,"invalid full bone path/pointer mapping accepted");};
 refused({bip,rightBone,leftBone},bip); // Same leaf names, wrong per-Mesh slot order.
 refused({bip,leftBone,leftBone},bip); // Duplicate native pointer.
 refused(ordered,bone); // RootBone outside palette.
 transformParents[right]=foreignRoot;refused(ordered,bip); // Foreign instance/hierarchy.
 transformParents[right]=leftBone;transformParents[left]=right;refused(ordered,bip); // Cycle.
 Reset();
}
void BindWorld(){
 RuntimeShapes::Bound binding;binding.renderer=RuntimeShapes::Ref::Make(oldRenderer,true);binding.mesh=RuntimeShapes::Ref::Make(oldMesh,true);binding.marker="marker";
 RuntimeShapes::bound[oldRenderer]=std::move(binding);RuntimeShapes::boundCount=1;
}
void ChangedMeshGateRegression(){
 using namespace FullResourceReload;Reset();
 auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);
 auto part=plan->parts.at("marker");plan->parts.clear();part.marker="new-mesh-marker";plan->parts.emplace(part.marker,part);
 plan->roots.front().markers={{part.marker,1}};prepared->plan=plan;
 enabled=true;RuntimeShapes::enabled=true;BindWorld();
 prepared->pointer={std::string(64,'A'),std::string(64,'B')};pending=prepared;
 auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeCandidate::Clock();ticket->config=prepared->config;ticket->controls=prepared->controls;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));
 Frame();Require(leaseRequests==1&&attempt&&attempt->phase==Phase::Leases,"F10 changed Mesh did not directly request native resources");
 attempt.reset();pending.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});enabled=false;Reset();
}
void PrewarmScheduleRegression(){
 using namespace FullResourceReload;
 auto path=std::filesystem::temp_directory_path()/("eff-prewarm-smoke-"+std::to_string(GetCurrentProcessId()));EiemResourceReloadPlan::Save(path,{1,2,3,4});
 {
  auto hold=std::make_shared<NativeCandidate::FileHold>(path);auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);plan->inputs.clear();plan->closure.clear();
  for(size_t i=0;i<5;++i){EiemResourceReloadPlan::Input input;input.file="input-"+std::to_string(i)+".vfs";input.path=path;plan->inputs.push_back(input);plan->closure.push_back(i);prepared->files.emplace(i,hold);}prepared->plan=plan;
  Attempt a;a.prepared=prepared;a.phase=Phase::Bundles;NativeCandidate::api.load=(void*)50;
  Step(a);Require(opened==4&&a.phase==Phase::Bundles&&!removeCalls&&!cloneCalls,"prewarm must open a bounded batch without exposing a model");
  scheduleDone[(void*)0x8002]=true;Step(a);Require(opened==5&&a.bundles.at(1).done&&!a.bundles.at(0).done,"out-of-order completion stalled on the first Bundle");
  for(auto& request:scheduleDone)request.second=true;Step(a);Require(a.phase==Phase::Assets&&a.bundles.size()==5,"registered native closure did not advance as a batch");
 }
 std::filesystem::remove(path);Reset();
 // A ready graph is inert until F10 targets that exact publication.
 auto a=MakeAttempt();a.phase=Phase::Ready;Step(a);Require(a.phase==Phase::Ready&&!removeCalls&&!loadCalls&&!cloneCalls,"prewarm mutated the current owner before F10");
}
void LatestPublicationRegression(){
 using namespace FullResourceReload;
 enabled=true;RuntimeShapes::enabled=true;pending.reset();
 auto prepared=Prepare();prepared->pointer={std::string(64,'A'),std::string(64,'B')};
 attempt=std::make_unique<Attempt>();attempt->prepared=prepared;attempt->phase=Phase::Ready;
 auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeCandidate::Clock();ticket->controls=prepared->controls;ticket->config=prepared->config;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));
 std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(EiemOfflinePackage::Pointer{std::string(64,'C'),std::string(64,'D')}));
 Frame();Require(attempt->phase==Phase::Ready&&!removeCalls&&!loadCalls,"F10 committed an older ready candidate after newer publication");
 ticket->pointer={};std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));Frame();Require(attempt->phase==Phase::Ready&&!removeCalls,"waiting offline build exposed the previous candidate");
 // A publication or newer F10 arriving between owner discovery and rebuild
 // must also prevent an old candidate from touching the live model.
 for(auto phase:{Phase::Owners,Phase::Rebuild}){attempt->phase=phase;bool refused=false;try{Step(*attempt);}catch(const std::exception&){refused=true;}
  Require(refused&&!removeCalls&&!loadCalls,"superseded candidate mutated the owner after ticket consumption");}
 std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));
 attempt->requestClock=1;ticket->clock=2;attempt->phase=Phase::Rebuild;bool refused=false;try{Step(*attempt);}catch(const std::exception&){refused=true;}
 Require(refused&&!removeCalls,"older F10 mutated an owner after a newer waiting request");
 attempt.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});enabled=false;
}
void StartupNoWorldRegression(){
 using namespace FullResourceReload;
 struct Cleanup {~Cleanup(){attempt.reset();pending.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});enabled=false;Reset();}}cleanup;
 Reset();enabled=true;RuntimeShapes::enabled=true;auto prepared=Prepare();
 prepared->pointer={std::string(64,'A'),std::string(64,'B')};pending=prepared;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));
 for(unsigned i=0;i<128;++i)Frame();
 Require(attempt&&attempt->phase==Phase::AwaitRequest&&!leaseRequests&&!opened&&!removeCalls,"startup without F10 submitted extra resource requests");
 auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeCandidate::Clock();ticket->config=prepared->config;ticket->controls=prepared->controls;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));
 Frame();Require(leaseRequests==1&&attempt->phase==Phase::Leases&&!removeCalls,"explicit F10 did not request resources without a world witness");
 Require(std::atomic_load(&requested)==ticket,"F10 was consumed before candidate load completed");
}
void RetirementRegression(){
 using namespace FullResourceReload;Reset();
 Attempt a;a.prepared=Prepare();a.exposed=true;
 Bundle b;b.done=true;b.owned=true;b.object=RuntimeShapes::Ref::Make(bundle,false);a.bundles.emplace(0,b);
 Lease l;l.done=true;l.shared=true;l.handle=RuntimeShapes::Ref::Make(leaseHandle,false);a.leases.push_back(l);
 RetireGeneration(a);Require(unloadCalls==1&&!disposeCalls&&a.bundles.empty()&&!a.prepared,"superseded generation retained containers or destroyed shared leases");
 RetireGeneration(a);Require(unloadCalls==1,"generation retirement repeated Unload");
 Reset();
}

}
namespace Test {
void FullFrameRegression(bool display){
 using namespace FullResourceReload;Reset();enabled=true;RuntimeShapes::enabled=true;BindWorld();done=1;
 auto file=std::filesystem::temp_directory_path()/("eff-frame-"+std::to_string(GetCurrentProcessId()));EiemResourceReloadPlan::Save(file,{1,2,3,4});
 {
  auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);
  plan->inputs.push_back({"input-001.vfs",{},"bundle",file,{}});plan->closure={0};prepared->plan=plan;
  prepared->files.emplace(0,std::make_shared<NativeCandidate::FileHold>(file));prepared->pointer={std::string(64,'A'),std::string(64,'B')};pending=prepared;
  std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));NativeCandidate::api.load=(void*)50;
  for(unsigned repeat=0;repeat<8;++repeat){
   // Exercise the real discovery -> rebuild call chain with Unity's spelling,
   // rather than only testing the path comparison helper in isolation.
   const char* paths[]={"assets/model.prefab","Assets/Model.prefab","ASSETS\\MODEL.prefab"};
   modelPath=paths[repeat%3];
   generation=2;currentRoot=oldRoot;animationRoot=oldRoot;RuntimeControls::Spec spec;spec.marker="marker";RuntimeControls::specs.emplace("marker",spec);BindWorld();
   if(display){hasHelper=false;liveParts[mainPart]={RuntimeShapes::Ref::Make(mainPart,true),RuntimeShapes::Ref::Make(oldRoot,true),modelPath,false};}
   auto fresh=Prepare();auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeCandidate::Clock();ticket->config=fresh->config;ticket->controls=fresh->controls;
   std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));
   for(unsigned frame=0;frame<100&&generation==2;++frame){for(auto& request:scheduleDone)request.second=true;Frame();}
   Require(attempt&&attempt->phase==Phase::Passed&&generation==3&&currentRoot==newRoot,"F10 Frame did not complete native load -> prefab verify -> discover -> game-owned rebuild -> commit: display="+std::to_string(display)+" repeat="+std::to_string(repeat)+" phase="+std::to_string(attempt?int(attempt->phase):-1)+" reason="+(attempt?attempt->error:"no attempt"));
   Require(leaseRequests==1&&opened==1&&removeCalls==repeat+1&&discardCalls==repeat+1&&loadCalls==repeat+1,"full Frame did not execute exactly one reload transaction");
   Require(followCalls==(display?0u:repeat+1),"display/world follow lifecycle differs");
   Require(animationRebinds==(display?0u:repeat+1)&&(!display?animationRoot==newRoot:true),"full Frame reported success without animation rebind");
  }
  attempt.reset();pending.reset();sourceLeases.clear();liveParts.clear();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});enabled=false;Reset();
 }
 std::filesystem::remove(file);
}
}
int main(){
 using namespace Test;using namespace FullResourceReload;
 try{
  Setup();Reset();EnumRegression();BonePathRegression();RootPathRegression();ManagerContractRegression();NativeLifecycleRegression();TextureIdentityRegression();PrewarmScheduleRegression();LatestPublicationRegression();StartupNoWorldRegression();ChangedMeshGateRegression();RetirementRegression();FullFrameRegression(false);FullFrameRegression(true);IdlePoolRegression();
  auto graph=std::make_shared<const std::vector<Package>>();std::atomic_store(&published,graph);
  // Fail AFTER a new native instance exists. Restore routes and rebuild through
  // the game lifecycle, then verify the old native Mesh identities.
  {auto a=MakeAttempt();rejectVertex=true;bool rejected=false;try{Rebuild(a);}catch(const std::exception&){rejected=true;}
   Require(rejected&&generation==2&&std::atomic_load(&published)==graph&&currentRoot==restoredRoot&&MeshProfile(currentRoot)==a.owners.front().originalMeshes,"failed resource transaction did not restore native owner/graph");
   Require(removeCalls==2&&discardCalls==2&&loadCalls==2&&std::atomic_load(&routes)->empty(),"rollback lifecycle order/routes invalid");
  }
  Reset();
  {auto a=MakeAttempt();Rebuild(a);Require(a.phase==Phase::Passed&&generation==3&&currentRoot==newRoot&&a.cloneCount==1,"valid full resource transaction did not publish migrated owner");
   Require(removeCalls==1&&discardCalls==1&&loadCalls==1&&followCalls==1,"normal game lifecycle called more than once");
   auto count=cloneCalls;sourceAlive=false;Clone(source);Require(cloneCalls==count+1,"retired-source passthrough lost original call");
  }
  Reset();
  {Attempt a;a.phase=Phase::Reject;Lease lease;lease.handle=RuntimeShapes::Ref::Make(leaseHandle,false);a.leases.push_back(lease);
   Bundle b;b.request=RuntimeShapes::Ref::Make(createRequest,false);b.object=RuntimeShapes::Ref::Make(bundle,false);b.owned=true;a.bundles.emplace(0,b);
   Drain(a);Require(!disposeCalls&&!unloadCalls&&a.phase==Phase::Reject,"inflight native work was released");done=1;Drain(a);Require(disposeCalls==1&&unloadCalls==1&&a.phase==Phase::Drained,"completed rejected generation not drained exactly once");
   Drain(a);Require(disposeCalls==1&&unloadCalls==1,"repeated cleanup double disposed/unloaded");
  }
  Reset();
  {Attempt a;a.phase=Phase::Reject;Lease l;l.handle=RuntimeShapes::Ref::Make(leaseHandle,false);l.done=true;a.leases.push_back(l);
   throwDispose=true;bool threw=false;try{Drain(a);}catch(const std::exception&){threw=true;}
   Require(threw&&a.phase==Phase::Drained&&disposeCalls==1,"throwing lease Dispose was not quarantined");Drain(a);Require(disposeCalls==1,"throwing lease Dispose repeated");
  }
  Reset();
  {Attempt a;a.phase=Phase::Reject;a.exposed=true;Lease l;l.handle=RuntimeShapes::Ref::Make(leaseHandle,false);l.done=true;a.leases.push_back(l);Drain(a);Require(!disposeCalls&&a.phase==Phase::Drained,"exposed generation lost game dependency lease");}
  {Attempt a;a.phase=Phase::Reject;a.bundles.emplace(0,Bundle{});Drain(a);Require(a.phase==Phase::Drained,"pre-native load failure polled null request or stuck");}
  std::atomic_store(&routes,std::shared_ptr<const Routes>(std::make_shared<Routes>()));Require(handles.empty(),"fixture GC handles leaked");
  std::cout<<"PASS full resource transaction (mock native APIs): six native prefab factories preserve arguments; native animation resume/weapons/audio lifecycle; stale animation reproduced; old Mesh/Texture identity rejection with animated rollback; exact typed static/instance manager getters; startup zero extra native requests; full Frame world/display -> commit; repeated reloads; full bone paths; enum ABI; superseded tickets; rollback; native retirement\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
