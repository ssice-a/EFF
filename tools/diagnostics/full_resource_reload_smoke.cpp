#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace Test {
void *propertyArrayClass=(void*)0xAB00,*propertyElementClass=(void*)0xAB01;
std::set<void*> propertyArrays;
std::vector<int32_t> propertyIds{314};std::map<int32_t,void*> propertyTextures;int propertyElementWidth=4;
void *source=(void*)0x1000,*candidate=(void*)0x1001,*oldRoot=(void*)0x1002,*newRoot=(void*)0x1003,*restoredRoot=(void*)0x1004;
void *lazyCandidate=(void*)0x1008,*lazyRoot=(void*)0x1009;
void *model=(void*)0x1010,*entity=(void*)0x1011,*instance=(void*)0x1012,*manager=(void*)0x1013,*effect=(void*)0x1014;
void *helper=(void*)0x1015,*mainPart=(void*)0x1016;
void *auxiliaryRenderer=(void*)0x101d,*auxiliaryHelper=(void*)0x101e;
void *modelClass=(void*)0x2011,*delegateClass=(void*)0x2012,*loadCallback=(void*)0x2013;
void* gameObjectClass=(void*)0x2014;void* resourceSource=(void*)0x1000;std::string resourceLabel="assets/model.prefab";
uint64_t pathHashValue=0,fixtureProxyHash=0;void* proxyCloneResult=nullptr;
void *assetProxyObject=(void*)0x2015,*assetProxyClass=(void*)0x2016;bool instanceHandle=false;
void *unrelatedRenderer=(void*)0x1100,*unrelatedHelper=(void*)0x1101,*unrelatedEntity=(void*)0x1102,*unrelatedModel=(void*)0x1103,*unrelatedRoot=(void*)0x1104;
void *unrelatedPart=(void*)0x1105,*unrelatedConfig=(void*)0x1106;
std::vector<void*> sceneRenderers;
std::map<void*,std::vector<void*>> controlRenderers;
void *animation=(void*)0x1017,*animationData=(void*)0x1018,*animator=(void*)0x1019,*weapons=(void*)0x101a,*audio=(void*)0x101b,*audioClass=(void*)0x2010;
void* animationRoot=(void*)0x1002;unsigned animationRebinds=0,weaponUnloads=0,weaponReloads=0,weaponRemounts=0,audioInvalidations=0;
int nativeFactory=1;bool ignoreCandidate=false;
bool recordPartFinished=false,completePartLoading=true;
bool typedAudioOnly=false;
bool displayOverride=false;unsigned overrideReleases=0;
void* callbackTarget=(void*)0x1010;
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
unsigned opened=0,leaseRequests=0,sceneQueries=0;
unsigned aliveCalls=0;
void* byteClass=(void*)0xAA10;unsigned memoryCalls=0,fileFallbackCalls=0;
std::set<void*> unloadedStreamBundles;
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
void* Array(const std::vector<void*>& values){auto b=std::make_unique<Bytes>(32+values.size()*8);uint64_t count=values.size();memcpy(b->data()+24,&count,8);if(count)memcpy(b->data()+32,values.data(),count*8);auto p=b->data();storage.push_back(std::move(b));return p;}
void* PropertyIds(){auto b=std::make_unique<Bytes>(32+propertyIds.size()*4);uint64_t count=propertyIds.size();memcpy(b->data()+24,&count,8);if(count)memcpy(b->data()+32,propertyIds.data(),count*4);auto p=b->data();propertyArrays.insert(p);storage.push_back(std::move(b));return p;}
uint32_t Root(void* p,bool){handles[++sequence]=p;return sequence;}
void* Target(uint32_t h){return handles.at(h);}
void Free(uint32_t h){Require(handles.erase(h)==1,"fixture handle double free");}
void* GetClass(void* p){return propertyArrays.count(p)?propertyArrayClass:p==assetProxyObject?assetProxyClass:(p==source||p==candidate||p==lazyCandidate||p==unrelatedRoot)?gameObjectClass:p==audio?audioClass:(p==model||p==unrelatedModel)?modelClass:p==loadCallback?delegateClass:p==leaseHandle?handleClass:p==pathValue?pathClass:(p==configBox||p==unrelatedConfig)?configClass:instanceClass;}
void* Fields(void*,void** it){auto n=uintptr_t(*it);if(n==10)return nullptr;*it=(void*)(n+1);return (void*)(n+1);}
const char* FieldName(void* f){const char* names[]={"","modelManager","effectManager","m_entity","m_mainPart","m_cfg","usePersistentPool","modelPath","m_model","m_loadFinishCb","hash"};return names[uintptr_t(f)];}
const char* TypeName(void* t){const char* names[]={"","Beyond.Gameplay.View.ModelManager","Beyond.Gameplay.EffectManager","Beyond.Gameplay.Core.Entity","Beyond.Gameplay.View.ComplexModelViewPart","Beyond.Gameplay.View.BaseModelViewPartData","System.Boolean","System.String","UnityEngine.GameObject","System.Action<System.Boolean,Beyond.Gameplay.View.BaseModelViewPart>","System.UInt64"};return names[uintptr_t(t)];}
int FieldFlags(void* f){return (f==(void*)1||f==(void*)2)&&managersStatic?0x16:0x6;}
void FieldValue(void* self,void* f,void* out){Require(!(FieldFlags(f)&0x10)&&self,"instance getter used for a static field");if(f==(void*)11){memset(out,0,64);return;}if(f==(void*)10){*(uint64_t*)out=pathHashValue;return;}if(f==(void*)5){memset(out,0,64);memcpy(out,&self,8);return;}if(f==(void*)6){*(unsigned char*)out=0;return;}
 auto v=f==(void*)1?manager:f==(void*)2?effect:f==(void*)3?(self==unrelatedHelper?unrelatedEntity:hasEntity?entity:nullptr):f==(void*)4?(self==unrelatedModel?unrelatedPart:hasPart?mainPart:nullptr):f==(void*)7?StringValue(self==unrelatedConfig?"assets/unmodified-character.prefab":modelPath):self==unrelatedPart?unrelatedRoot:currentRoot;memcpy(out,&v,8);}
void StaticFieldValue(void* f,void* out){Require((FieldFlags(f)&0x10)&&(f==(void*)1||f==(void*)2),"static getter used for an instance field");auto v=f==(void*)1?manager:effect;memcpy(out,&v,8);}
void* NativeClone(void* p){++cloneCalls;return p==lazyCandidate?lazyRoot:p==candidate&&!ignoreCandidate?newRoot:restoredRoot;}
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
 if(m==2){++aliveCalls;auto p=args[0];return !p||(p==source&&!sourceAlive)||unloadedStreamBundles.count(p)?&no:&yes;}
 if(m==3){id=int(uintptr_t(self));return &id;}
 if(m==4)return StringValue(self==material?"body":self==skeletonRoot?"Root":self==bip?"Bip001":self==left?"Left":self==right?"Right":(self==bone||self==leftBone||self==rightBone)?"Bone":"marker");
 if(m==5)return Array(self==unrelatedRoot?unrelatedRenderer:self==newRoot||self==candidate||self==lazyCandidate?newRenderer:oldRenderer);
 if(m==6)return self==newMesh&&rejectVertex?&badVertex:&two;
 if(m==7)return &zero;
 if(m==8)return Array(bone);
 if(m==9)return Array(nullptr);
 if(m==10)return bone;
 if(m==11)return Array(material);
 if(m==12)return shader;
 if(m==20){++instanceCalls;return hasInstance?instance:nullptr;}
 if(m==21)return self==unrelatedModel?unrelatedRoot:currentRoot;
 if(m==22){++removeCalls;currentRoot=nullptr;return nullptr;}
 if(m==97){++overrideReleases;++removeCalls;currentRoot=nullptr;return nullptr;}
 if(m==23){Require(!currentRoot,"pool discarded before old owner released");++discardCalls;return nullptr;}
 if(m==24){Require(discardCalls>=removeCalls,"model loaded before path-scoped pool discard");++loadCalls;
  if(recordPartFinished)FullResourceReload::PartLoading(mainPart,configBox,loadCallback,nullptr);else currentRoot=NativeCreate(source);return nullptr;}
 if(m==25){++followCalls;return nullptr;}
 if(m==26)return released?&yes:&no;
 if(m==30){++disposeCalls;if(throwDispose)throw std::runtime_error("fixture Dispose threw after side effect");return nullptr;}
 if(m==31){Require(self,"polled a null native request");auto it=scheduleDone.find(self);return it==scheduleDone.end()?&done:it->second?&yes:&no;}
 if(m==32){auto it=scheduleBundles.find(self);return it==scheduleBundles.end()?bundle:it->second;}
 if(m==33){Require(args&&*(unsigned char*)args[0]==0,"unloaded candidate objects still in use");++unloadCalls;return nullptr;}
 if(m==40){Require(self==oldRenderer||self==newRenderer||self==animator,"Component getter invoked on a GameObject");return renderTransform;}
 if(m==41){Require(self==currentRoot||self==oldRoot||self==newRoot||self==restoredRoot||self==candidate||self==lazyCandidate,"GameObject getter invoked on a Component");return prefabTransform;}
 if(m==42)return transformParents.at(self);
 if(m==50){auto request=(void*)(uintptr_t(0x8000)+ ++opened);scheduleDone[request]=false;scheduleBundles[request]=(void*)(uintptr_t(0x9000)+opened);return request;}
 if(m==87){Require(!self&&args&&args[0]&&*(uint32_t*)args[1]==0&&*(uint32_t*)args[2]==0,"archive stream Bundle load argument ABI differs");auto array=*(void**)args[0];size_t length=0;Require(Entry::ArrayLength(array,&length)&&length==4&&memcmp((char*)array+32,"\1\2\3\4",4)==0,"archive stream Bundle payload differs");++memoryCalls;return Invoke((void*)50,nullptr,nullptr,exception);}
 if(m==88){Require(self&&args[0]&&!*(bool*)args[1],"read-only MemoryStream constructor ABI differs");*(void**)self=args[0];return nullptr;}
 if(m==51){++leaseRequests;return leaseHandle;}
 if(m==52){pathHashValue=std::hash<std::string>{}(FullResourceReload::NormalizeAssetPath(String(args[0])));return pathValue;}
 if(m==53)return self==auxiliaryRenderer?auxiliaryHelper:self==unrelatedRenderer?unrelatedHelper:hasHelper?helper:nullptr;
 if(m==54)return self==unrelatedEntity?unrelatedModel:hasModel?model:nullptr;
 if(m==55)return StringValue(self==unrelatedModel?"assets/unmodified-character.prefab":modelPath);
 if(m==56)return helperMatches?helper:nullptr;
 if(m==57)return rendererInside?&yes:&no;
 if(m==58)return assetRequest;
 if(m==59)return candidate;
 if(m==60)return FullResourceReload::ResourceGet(self,nullptr);
 if(m==89)return StringValue(resourceLabel);
 if(m==90){Require(String(args[0])=="assets/other.prefab","later character selected the first character's prefab label");return lazyCandidate;}
 if(m==92)return assetProxyObject;
 if(m==93)return &fixtureProxyHash;
 if(m==94)return resourceSource;
 if(m==95)return instanceHandle?instance:assetProxyObject;
 if(m==61)return &no;
 if(m==62){Require(discardCalls>=removeCalls,"display loaded before old root pool discard");++loadCalls;currentRoot=NativeCreate(source);FullResourceReload::PartFinished(mainPart,nullptr);return &yes;}
 if(m==63){Require(args&&args[0]==configBox,"display SetupData did not use preserved value config");FullResourceReload::PartConfigured(self,args[0],nullptr);return nullptr;}
 if(m==64)return self==unrelatedEntity?nullptr:animation;
 if(m==65)return animationData;
 if(m==66)return animator;
 if(m==67)return animationRoot==currentRoot?&yes:&no;
 if(m==68){Require(self==animation&&args&&args[0]==animationData&&*(unsigned char*)args[1]==1,"native animation rebind did not preserve data/resume option");++animationRebinds;animationRoot=currentRoot;return &yes;}
 if(m==69)return self==unrelatedEntity?nullptr:weapons;
 if(m==70){Require(self==weapons&&args&&*(unsigned char*)args[0]==1,"weapon unload arguments differ");++weaponUnloads;return nullptr;}
 if(m==71){Require(animationRoot==currentRoot&&self==weapons&&*(unsigned char*)args[0]==1,"weapons reloaded before animation rebound");++weaponReloads;return nullptr;}
 if(m==72){++weaponRemounts;return nullptr;}
 if(m==73)return self==unrelatedEntity?nullptr:typedAudioOnly?instance:audio;
 if(m==96){Require(args&&args[1]&&args[2]&&!*(unsigned char*)args[2],"typed game component query out/warning ABI differs");*(void**)args[1]=self==unrelatedEntity?nullptr:audio;return self==unrelatedEntity?&no:&yes;}
 if(m==74){++audioInvalidations;return nullptr;}
 if(m==75)return textureValue;
 if(m==76||m==77){
  std::vector<void*> children;for(const auto& row:transformParents)if(row.second==self)children.push_back(row.first);
  if(m==77)return children.at(size_t(*(int*)args[0]));
  static int childCount=0;childCount=int(children.size());return &childCount;
 }
 if(m==78)return self; // Local transform/GameObject identity in this fixture.
 if(m==79){Require(self==renderTransform,"direct Renderer lookup used the wrong hierarchy address");return Array(newRenderer);}
 if(m==80){++sceneQueries;return Array(sceneRenderers);}
 if(m==82)return PropertyIds();
 if(m==83){Require(self==material&&args&&args[0],"public Texture ID getter arguments absent");auto property=*(int32_t*)args[0];Require(std::find(propertyIds.begin(),propertyIds.end(),property)!=propertyIds.end(),"public Texture ID getter received wrong ABI value");auto found=propertyTextures.find(property);return found==propertyTextures.end()?textureValue:found->second;}
 if(m==84)return callbackTarget;
 if(m==85)return self==model?entity:unrelatedEntity;
 if(m==86){Require(self&&args&&args[0]==(void*)81&&*(unsigned char*)args[1]==1,"controls queried scene objects instead of inactive model children");
  auto found=controlRenderers.find(self);return found==controlRenderers.end()?Array(self==newRoot?newRenderer:oldRenderer):Array(found->second);}
 throw std::runtime_error("unexpected fixture managed call: "+std::to_string(m));
}
void Setup(){
 il2cpp_gchandle_new=Root;il2cpp_gchandle_get_target=Target;il2cpp_gchandle_free=Free;il2cpp_object_get_class=GetClass;
 RuntimeShapes::Ref::weakHandle=Root;
 il2cpp_object_get_virtual_method=[](void*,void* method)->void*{return displayOverride?(void*)97:method;};
 il2cpp_class_get_name=[](void* klass){return klass==assetProxyClass?"AssetProxy":klass==gameObjectClass?"GameObject":klass==audioClass?"CharacterAudioComponent":klass==modelClass?"BaseModelComponent":klass==delegateClass?"Delegate":"fixture";};
 il2cpp_class_get_namespace=[](void* klass){return klass==assetProxyClass?"":klass==gameObjectClass?"UnityEngine":klass==modelClass?"Beyond.Gameplay.View":klass==delegateClass?"System":"Beyond.Gameplay.Core";};
 il2cpp_object_unbox=[](void* p){return p;};il2cpp_class_get_fields=Fields;il2cpp_class_get_parent=[](void*)->void*{return nullptr;};
 il2cpp_field_get_name=FieldName;il2cpp_field_get_flags=FieldFlags;il2cpp_field_static_get_value=StaticFieldValue;il2cpp_field_get_type=[](void* p){return p;};
 il2cpp_type_get_name=TypeName;il2cpp_free=[](void*){};il2cpp_field_get_value=FieldValue;il2cpp_runtime_invoke=Invoke;
 il2cpp_class_from_type=[](void*)->void*{return configClass;};il2cpp_class_value_size=[](void* klass,uint32_t* alignment)->int32_t{*alignment=8;return klass==propertyElementClass?propertyElementWidth:64;};il2cpp_value_box=[](void* klass,void* data)->void*{return klass==handleClass?leaseHandle:*(void**)data==unrelatedPart?unrelatedConfig:configBox;};
 il2cpp_class_get_element_class=[](void* klass)->void*{return klass==propertyArrayClass?propertyElementClass:nullptr;};
 il2cpp_string_new=[](const char* value)->void*{return StringValue(value);};
 il2cpp_array_new=[](void* klass,size_t size)->void*{Require(klass==byteClass,"memory Bundle array element type differs");auto b=std::make_unique<Bytes>(32+size);uint64_t count=size;memcpy(b->data()+24,&count,8);auto p=b->data();storage.push_back(std::move(b));return p;};
 RuntimeShapes::api.main=(void*)1;RuntimeShapes::api.alive=(void*)2;RuntimeShapes::api.name=(void*)4;
 RuntimeShapes::api.vertexCount=(void*)6;RuntimeShapes::api.shapeCount=(void*)7;RuntimeShapes::api.bones=(void*)8;
 RuntimeShapes::api.bindPoses=(void*)9;RuntimeShapes::api.rootBone=(void*)10;RuntimeShapes::originalMeshGetter=Mesh;
 RuntimeShapes::api.transform=(void*)40;
 RuntimeShapes::api.modelRenderers=(void*)86;RuntimeShapes::api.rendererType=(void*)81;
 auto& a=FullResourceReload::api;a.instanceId=(void*)3;a.components=(void*)5;a.materials=(void*)11;a.shader=(void*)12;
 a.texture=(void*)75;a.textureIds=(void*)82;a.textureById=(void*)83;
 a.instance=(void*)20;a.getModel=(void*)21;a.remove=(void*)22;a.discard=(void*)23;a.loadModel=(void*)24;a.follow=(void*)25;a.released=(void*)26;
 a.animationCom=(void*)64;a.animationData=(void*)65;a.animationAnimator=(void*)66;a.animationValid=(void*)67;a.animationChange=(void*)68;
 a.weaponCom=(void*)69;a.weaponUnload=(void*)70;a.weaponReload=(void*)71;a.weaponMounts=(void*)72;a.componentLookup=(void*)96;a.audioInvalidate=(void*)74;
 a.modelManager=FullResourceReload::BindObjectField(instanceClass,"modelManager","Beyond.Gameplay.View.ModelManager");
 a.effectManager=FullResourceReload::BindObjectField(instanceClass,"effectManager","Beyond.Gameplay.EffectManager");
 a.pathClass=pathClass;a.handleClass=handleClass;a.dispose=(void*)30;a.done=(void*)31;
 a.goTransform=(void*)41;a.parent=(void*)42;
 a.childCount=(void*)76;a.getChild=(void*)77;a.componentGo=(void*)78;a.directComponents=(void*)79;
 a.load=(void*)51;a.toPath=(void*)52;a.modelCom=(void*)54;a.getPath=(void*)55;a.isChild=(void*)57;
 a.delegateTarget=(void*)84;a.modelEntity=(void*)85;
 a.handlePath=(void*)89;a.assetSync=(void*)90;FullResourceReload::originalResourceGet=[](void*,void*){return resourceSource;};
 a.pathHashField=(void*)10;a.proxyAsset=(void*)92;a.proxyHash=(void*)93;a.proxyGet=(void*)94;
 a.handleUntrackedField=(void*)11;a.handleUntrackedClass=configClass;a.untrackedAssetGet=(void*)95;
 FullResourceReload::originalProxyPreload=[](void*,bool asynchronous,void*){proxyCloneResult=asynchronous?FullResourceReload::InstantiateAsync(resourceSource,1,factoryParent,factoryPosition,factoryRotation):FullResourceReload::Clone(resourceSource);};
 a.get=(void*)60;a.error=(void*)61;a.partRelease=(void*)22;a.partLoad=(void*)62;a.partSetup=(void*)63;NativeBundle::api.assetLoad=(void*)58;NativeBundle::api.result=(void*)59;
 NativeBundle::api.done=(void*)31;NativeBundle::api.bundle=(void*)32;NativeBundle::api.unload=(void*)33;
 FullResourceReload::originalClone=NativeClone;
 FullResourceReload::originalPartFinished=[](void*,void*){};
 FullResourceReload::originalBasePartFinished=[](void*,void*){};
 FullResourceReload::originalPartReleased=[](void*,void*){currentRoot=nullptr;};
 FullResourceReload::originalPartLoading=[](void*,void*,void*,void*){if(completePartLoading){currentRoot=NativeCreate(source);FullResourceReload::PartFinished(mainPart,nullptr);}};
 FullResourceReload::originalPartConfigured=[](void*,void*,void*){};
 FullResourceReload::originalPartAdded=[](void*,void*,void*){};
 FullResourceReload::originalCloneParent=NativeParent;FullResourceReload::originalInstantiate=NativePosition;FullResourceReload::originalInstantiateParent=NativeParentPosition;
 FullResourceReload::originalInstantiateImmediate=NativeParent;FullResourceReload::originalInstantiateAsync=NativeAsync;
}
std::shared_ptr<FullResourceReload::Prepared> Prepare(){
 auto prepared=std::make_shared<FullResourceReload::Prepared>();prepared->base=2;
 prepared->controls.graph=std::make_shared<const std::vector<Package>>();prepared->config=EiemEntrySettings::Prepare(EiemGlobalConfig{});
 auto plan=std::make_shared<EiemResourceReloadPlan::Plan>();plan->parts.emplace("marker",EiemResourceReloadPlan::Part{"part","marker","material",2,{"Root/Bip001/Bone"},{}});
 plan->materials.emplace("material",EiemResourceReloadPlan::Material{"body",{}});plan->roots.push_back({"assets/model.prefab",0,{{"marker",1}}});
#ifndef EFF_OFFLINE_PACKAGE_V35
 using namespace EiemRenderAssemblyDescription;
 EiemRenderAssemblyDescription::Source descriptor;descriptor.cab="CAB-fixture";descriptor.id=1;descriptor.renderer={Segment("marker",0),0};
 EiemRenderAssemblyDescription::Target target;target.renderer=descriptor.renderer;target.marker="marker";
 target.bones={Segment("Root",0)+Segment("Bip001",0)+Segment("Bone",0)};target.root=target.bones.front();target.hasRoot=true;
 descriptor.targets.push_back(target);plan->roots.front().assembly.sources.push_back(descriptor);
#endif
 prepared->plan=plan;return prepared;
}
FullResourceReload::Attempt MakeAttempt(){
 FullResourceReload::Attempt a;a.prepared=Prepare();a.selectedRoots.insert(0);
 FullResourceReload::Lease lease;lease.source=RuntimeShapes::Ref::Make(source,false);a.leases.push_back(lease);
 FullResourceReload::Asset asset;asset.prefab=RuntimeShapes::Ref::Make(candidate,false);a.assets.push_back(asset);
 FullResourceReload::Owner owner;owner.entity=RuntimeShapes::Ref::Make(entity,false);owner.component=RuntimeShapes::Ref::Make(model,false);owner.root=RuntimeShapes::Ref::Make(oldRoot,false);
 owner.pathHash=RuntimeShapes::Ref::Make(pathValue,false);owner.path="assets/model.prefab";owner.oldRootId=int(uintptr_t(oldRoot));owner.originalMeshes=FullResourceReload::MeshProfile(oldRoot);a.owners.push_back(owner);return a;
}
void Reset(){
 unloadedStreamBundles.clear();
 propertyArrays.clear();propertyIds={314};propertyTextures.clear();propertyElementWidth=4;
 std::atomic_store(&FullResourceReload::routes,std::shared_ptr<const FullResourceReload::Routes>(std::make_shared<FullResourceReload::Routes>()));
 currentRoot=oldRoot;removeCalls=discardCalls=loadCalls=cloneCalls=followCalls=disposeCalls=unloadCalls=0;rejectVertex=false;sourceAlive=true;throwDispose=false;done=0;generation=2;
 animationRoot=oldRoot;animationRebinds=weaponUnloads=weaponReloads=weaponRemounts=audioInvalidations=0;ignoreCandidate=false;recordPartFinished=false;completePartLoading=true;callbackTarget=model;
 identitySameMesh=false;textureValue=oldTexture;
 scheduleDone.clear();scheduleBundles.clear();opened=leaseRequests=sceneQueries=0;
 hasHelper=hasEntity=hasModel=hasPart=helperMatches=rendererInside=true;released=false;modelPath="assets/model.prefab";
 RuntimeShapes::bound.clear();RuntimeShapes::boundCount=0;
 FullResourceReload::sourceLeases.clear();FullResourceReload::liveParts.clear();
 std::atomic_store(&FullResourceReload::activeNative,std::shared_ptr<FullResourceReload::Attempt>{});FullResourceReload::retained.clear();
 FullResourceReload::observedPrefabs.clear();FullResourceReload::ownerRevision=0;resourceSource=source;resourceLabel="assets/model.prefab";
 instanceHandle=false;
 sceneRenderers.clear();controlRenderers.clear();
 RuntimeControls::specs.clear();RuntimeControls::Spec spec;spec.marker="marker";RuntimeControls::specs.emplace("marker",spec);
 transformParents={{prefabTransform,nullptr},{skeletonRoot,prefabTransform},{bip,skeletonRoot},{renderTransform,prefabTransform},{bone,bip},{left,bip},{right,bip},{leftBone,left},{rightBone,right},{foreignRoot,nullptr}};
}
void IdlePoolRegression(){
 using namespace FullResourceReload;Reset();auto a=MakeAttempt();a.owners.clear();currentRoot=nullptr;
 Rebuild(a);Require(a.phase==Phase::Passed&&discardCalls==1&&!removeCalls&&!loadCalls,"F10 did not discard a matched prefab's idle pool without a live owner");
 Require(Clone(source)==newRoot,"a character first loaded after F10 did not receive the committed prefab");Reset();
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
 Require(stale.at("renderer/0").first==expected.at("renderer/0").first&&stale!=expected,"same Mesh/name/texture dimensions concealed old Texture object");
 textureValue=newTexture;Require(PrefabAssetIdentity(newRoot,*plan)==expected,"candidate texture identity did not match live root");
 plan->composed=true;plan->parts.clear();plan->materials.clear();expected=PrefabAssetIdentity(candidate,*plan);
 textureValue=oldTexture;stale=PrefabAssetIdentity(newRoot,*plan);
 Require(stale.at("renderer/0").first==expected.at("renderer/0").first&&stale!=expected,"pristine restoration accepted old Texture on the candidate Mesh");
 // Preserve signed/zero IDs, null properties and full coverage of unmodified
 // materials even when no authored parts remain in the candidate.
 propertyIds={-7,0,314};propertyTextures={{-7,newTexture},{0,nullptr},{314,oldTexture}};
 expected=PrefabAssetIdentity(candidate,*plan);const auto& slots=expected.at("renderer/0").second;
 Require(slots.at("material/0/textureId/-7")==int(uintptr_t(newTexture))&&slots.at("material/0/textureId/0")==0&&slots.at("material/0/textureId/314")==int(uintptr_t(oldTexture)),"public Texture IDs lost signed/null/full property coverage");
 propertyTextures[314]=newTexture;Require(PrefabAssetIdentity(newRoot,*plan)!=expected,"unchanged-Mesh restoration missed a non-first Texture change");
 propertyIds.clear();Require(PrefabAssetIdentity(candidate,*plan).at("renderer/0").second.size()==1,"zero Texture properties dropped Shader identity");
 propertyIds={314,314};bool rejected=false;try{PrefabAssetIdentity(candidate,*plan);}catch(const std::exception&){rejected=true;}Require(rejected,"duplicate public Texture ID accepted");
 propertyIds={314};propertyElementWidth=8;rejected=false;try{PrefabAssetIdentity(candidate,*plan);}catch(const std::exception&){rejected=true;}Require(rejected,"wrong public property array element layout accepted");Reset();
}
void MaterialBindingRegression(){
 // The game's stripped metadata retains the IDs array and GetTexture(int),
 // plus GetTexturePropertyNames(List<string>); the no-argument names overload
 // is absent. Exercise the same exact binding function used during Install.
 struct MethodRow {MInfo info{};const char* name;const char* result;const char* parameter;};
 static MethodRow rows[]={{ {},"GetTexturePropertyNameIDs","System.Int32[]",nullptr},
  {{},"GetTexture","UnityEngine.Texture","System.String"},
  {{},"GetTexture","UnityEngine.Texture","System.Int32"},
  {{},"GetTexturePropertyNames","System.Void","System.Collections.Generic.List<System.String>"}};
 for(auto& row:rows)row.info.mp=(void*)1;
 auto previous=std::make_tuple(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_type);
 auto restore=[&]{std::tie(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_type)=previous;};
 il2cpp_class_get_methods=[](void*,void** it)->void*{auto index=uintptr_t(*it);if(index==4)return nullptr;*it=(void*)(index+1);return &rows[index];};
 il2cpp_method_get_name=[](void* method)->const char*{return static_cast<MethodRow*>(method)->name;};
 il2cpp_method_get_param_count=[](void* method)->uint32_t{return static_cast<MethodRow*>(method)->parameter?1:0;};
 il2cpp_method_get_flags=[](void*,uint32_t*)->uint32_t{return 0x86;};
 il2cpp_method_get_return_type=[](void* method)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->result);};
 il2cpp_method_get_param=[](void* method,uint32_t)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->parameter);};
 il2cpp_type_get_name=[](void* type)->const char*{return static_cast<const char*>(type);};
 il2cpp_class_get_type=[](void*)->void*{return const_cast<char*>("UnityEngine.Material");};
 try{FullResourceReload::BindMaterialTextureProperties((void*)1);Require(FullResourceReload::api.textureIds==&rows[0]&&FullResourceReload::api.textureById==&rows[2],"Material exact binding chose a different parameter layout");}catch(...){restore();throw;}
 restore();Setup();
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
 binding.modelReady=true;
 RuntimeShapes::bound[oldRenderer]=std::move(binding);RuntimeShapes::boundCount=1;
}
void RecordPart(bool world=true){
 auto previous=completePartLoading;completePartLoading=false;callbackTarget=world?model:instance;
 FullResourceReload::PartLoading(mainPart,configBox,loadCallback,nullptr);FullResourceReload::PartFinished(mainPart,nullptr);
 completePartLoading=previous;
}
void SequentialWorldRegression(){
 using namespace FullResourceReload;Reset();BindWorld();RecordPart();recordPartFinished=true;sceneRenderers={oldRenderer};
 auto originalNewRoot=newRoot;
 struct Cleanup {void* root;~Cleanup(){newRoot=root;Reset();}}cleanup{originalNewRoot};
 for(unsigned repeat=0;repeat<8;++repeat){
  auto a=MakeAttempt();a.owners.clear();a.prepared->base=generation.load();DiscoverOwners(a);Require(!sceneQueries,"world reload queried scene Renderers");
  Require(a.owners.size()==1,"consecutive F10 lost the live model part");
  if(repeat)newRoot=(void*)(uintptr_t(0x1005)+repeat);
  Rebuild(a);
  Require(a.phase==Phase::Passed&&generation==3+repeat,"consecutive F10 did not commit");
  Require(animationRoot==currentRoot&&animationRebinds==repeat+1,
          "consecutive F10 committed a world model as a display part and left animation bound to the old root (T pose)");
  Require(!a.owners.front().viewPart&&followCalls==repeat+1&&weaponReloads==repeat+1&&weaponRemounts==repeat+1,
          "consecutive F10 lost the world's native consumer lifecycle");
  // The game replaces its helper, whose entity link is absent after the
  // primitive reload. Keep the new root/part/animation exactly as committed.
  hasEntity=false;sceneRenderers={newRenderer};
 }
}
void PartOwnerRegression(){
 using namespace FullResourceReload;Reset();completePartLoading=false;hasEntity=false;
 PartLoading(mainPart,configBox,loadCallback,nullptr);
 {auto a=MakeAttempt();a.owners.clear();bool refused=false;try{DiscoverOwners(a);}catch(const std::exception&){refused=true;}
  Require(refused&&!removeCalls&&liveParts.count(mainPart),"F10 accepted a matching in-flight model that could finish from the old prefab after commit");}
 modelPath="assets/unmodified-character.prefab";
 {auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);Require(a.owners.empty()&&liveParts.count(mainPart),"unrelated asynchronous loading blocked the candidate or lost its owner");}
 modelPath="assets/model.prefab";
 // Completion occurs after the game has cleared the callback, so querying
 // its Target here would not be a reliable source of ownership.
 callbackTarget=nullptr;PartFinished(mainPart,nullptr);
 {auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);Require(a.owners.size()==1&&!a.owners.front().viewPart&&a.owners.front().animation.Get()==animation,"completed world part did not retain its load callback owner");}
 auto ownerHandle=*liveParts.at(mainPart).model.handle;handles[ownerHandle]=nullptr;
 {auto a=MakeAttempt();a.owners.clear();bool refused=false;try{DiscoverOwners(a);}catch(const std::exception&){refused=true;}
  Require(refused&&!removeCalls,"retired world owner was reclassified as a display part");}
 handles[ownerHandle]=model;hasPart=false;
 {auto a=MakeAttempt();a.owners.clear();bool refused=false;try{DiscoverOwners(a);}catch(const std::exception&){refused=true;}
  Require(refused&&!removeCalls,"recorded callback bypassed current main-part ownership validation");}
 Reset();hasEntity=false;callbackTarget=instance;PartLoading(mainPart,configBox,loadCallback,nullptr);
 {auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);Require(a.owners.size()==1&&a.owners.front().viewPart&&!a.owners.front().animation.Get(),"ordinary display callback was treated as a world model");}
 Reset();PartConfigured(mainPart,configBox,nullptr);PartFinished(mainPart,nullptr);
 {auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);Require(a.owners.size()==1&&a.owners.front().viewPart&&!a.owners.front().animation.Get(),"direct SetupData display lifecycle was not recorded");}
 Reset();
}
void SynchronousOwnerRegression(){
 using namespace FullResourceReload;Reset();hasEntity=false;
 auto originalNewRoot=newRoot;
 struct Cleanup {void* root;~Cleanup(){newRoot=root;Reset();}}cleanup{originalNewRoot};
 // Real BaseModelComponent.InitSelf/CreateMainPart -> AddPart -> SetupData,
 // followed by SyncInit -> StartSyncLoad. This path has no LoadModel delegate.
 PartAdded(model,mainPart,nullptr);PartConfigured(mainPart,configBox,nullptr);PartFinished(mainPart,nullptr);
 auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);
 Require(a.owners.size()==1&&!a.owners.front().viewPart&&a.owners.front().animation.Get()==animation,
         "synchronous world creation was classified as display and skipped animation rebind (T pose)");
 recordPartFinished=true;Rebuild(a);
 Require(a.phase==Phase::Passed&&animationRebinds==1&&animationRoot==currentRoot,
         "F10 left the synchronous world's animator/blackboard on the retired model");
 // A later configuration of the same attached main part must preserve its
 // authoritative ownership, but a directly configured display remains display.
 PartConfigured(mainPart,configBox,nullptr);PartFinished(mainPart,nullptr);
 a=MakeAttempt();a.prepared->base=generation.load();a.owners.clear();DiscoverOwners(a);newRoot=(void*)0x1005;Rebuild(a);
 Require(a.phase==Phase::Passed&&animationRebinds==2&&animationRoot==currentRoot,
         "second F10 lost synchronous main-part ownership");Reset();
}
void LoadReleaseOwnershipRegression(){
 using namespace FullResourceReload;
 auto saved=originalPartLoading;auto initialNewRoot=newRoot;
 struct Cleanup {PartLoadingFn saved;void* root;~Cleanup(){originalPartLoading=saved;newRoot=root;Reset();}}cleanup{saved,initialNewRoot};
 // The real LoadModel calls virtual ReleaseModel before assigning the new
 // config/callback and entering _DoLoad. SetupData alone does not release.
 for(bool world:{false,true})for(bool synchronous:{false,true}){
  newRoot=initialNewRoot;Reset();callbackTarget=world?model:instance;
  originalPartLoading=[ ](void* self,void*,void*,void*){
   PartReleased(self,nullptr);
   if(completePartLoading){currentRoot=NativeCreate(source);PartFinished(self,nullptr);}
  };
  completePartLoading=synchronous;PartLoading(mainPart,configBox,loadCallback,nullptr);
  if(!synchronous){
   Require(liveParts.count(mainPart)&&liveParts.at(mainPart).ownerRecorded&&!liveParts.at(mainPart).root.handle,
           "LoadModel's cleanup erased its pending ownership record");
   auto pending=MakeAttempt();pending.owners.clear();bool refused=false;try{DiscoverOwners(pending);}catch(const std::exception&){refused=true;}
   Require(refused&&!removeCalls,"F10 accepted a still-loading replacement after ReleaseModel");
   callbackTarget=nullptr;currentRoot=NativeCreate(source);PartFinished(mainPart,nullptr);
  }
  animationRoot=currentRoot;
  auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);
  Require(a.owners.size()==1&&a.owners.front().viewPart!=world,
          "LoadModel -> ReleaseModel -> completion lost world/display ownership");
  Require(!world||a.owners.front().animation.Get()==animation,"world replacement skipped animation capture");
  callbackTarget=world?model:instance;completePartLoading=true;recordPartFinished=true;
  Rebuild(a);
  Require(a.phase==Phase::Passed&&generation==3&&(!world||animationRebinds==1&&animationRoot==currentRoot),
          "first F10 after LoadModel cleanup failed or left the world animation on its previous root");
  newRoot=(void*)0x1005;a=MakeAttempt();a.prepared->base=generation.load();a.owners.clear();DiscoverOwners(a);Rebuild(a);
  Require(a.phase==Phase::Passed&&generation==4&&(!world||animationRebinds==2&&animationRoot==currentRoot),
          "second F10 after LoadModel cleanup lost ownership/animation");
  PartReleased(mainPart,nullptr);
  Require(!liveParts.count(mainPart),"standalone ReleaseModel retained an inactive ownership record");
 }
}
void BoneVerificationCostRegression(){
 using namespace FullResourceReload;Reset();auto p=Prepare();BoneHierarchy hierarchy(oldRoot);
 hierarchy.Verify(oldRenderer,{bone},bone,p->plan->parts.at("marker"));
 auto baseline=aliveCalls;auto clock=NativeBundle::Clock();
 for(unsigned i=0;i<256;++i)hierarchy.Verify(oldRenderer,{bone},bone,p->plan->parts.at("marker"));
 auto calls=aliveCalls-baseline;
 std::cout<<"Bone hierarchy repeated verification: aliveCalls="<<calls<<" ms="<<NativeBundle::Ms(clock)<<'\n';
 Require(calls==0,"unchanged prefab bones repeatedly invoke native liveness checks per Renderer/slot/ancestor");Reset();
}
void AuxiliaryHelperRegression(){
 using namespace FullResourceReload;
 for(bool vanilla:{false,true}){
  Reset();BindWorld();sceneRenderers={auxiliaryRenderer,oldRenderer};
  RecordPart();if(vanilla)RuntimeShapes::bound.clear();
  auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);
  Require(!sceneQueries,"model ownership discovery still scans scene Renderers");
  Require(a.owners.size()==1&&!a.owners.front().viewPart&&a.owners.front().component.Get()==model,
          "same-entity auxiliary helper prevented exact main-model ownership discovery");
  Rebuild(a);Require(a.phase==Phase::Passed&&animationRoot==currentRoot&&animationRebinds==1,
          "same-entity auxiliary helper prevented F10 animated commit");
 }
 Reset();BindWorld();sceneRenderers={auxiliaryRenderer};PartFinished(mainPart,nullptr);
 {auto a=MakeAttempt();a.owners.clear();bool refused=false;try{DiscoverOwners(a);}catch(const std::exception&){refused=true;}
  Require(refused&&!removeCalls&&!sceneQueries,"completion without a load owner was silently treated as a display model");}
 Reset();
}
void OtherCharacterOwnerRegression(){
 using namespace FullResourceReload;Reset();RecordPart();completePartLoading=false;callbackTarget=unrelatedModel;
 PartLoading(unrelatedPart,unrelatedConfig,loadCallback,nullptr);PartFinished(unrelatedPart,nullptr);
 auto capture=[&](const std::vector<std::string>& paths){
  auto a=MakeAttempt();a.owners.clear();auto plan=std::make_shared<Plan>(*a.prepared->plan);plan->roots.clear();
  for(const auto& path:paths)plan->roots.push_back({path,0,{{"marker",1}}});a.prepared->plan=plan;
  DiscoverOwners(a);Require(!sceneQueries,"adding another character Mod triggered Renderer discovery");
  std::set<void*> owners;for(const auto& owner:a.owners){Require(!owner.viewPart,"other character world was classified as display");owners.insert(owner.component.Get());}
  return owners;
 };
 Require(capture({"assets/model.prefab"})==std::set<void*>{model},"unmodified character was captured outside candidate roots");
 // Both characters were recorded before the second character had any Mod.
 // Adding/removing authors changes the candidate roots, not the registry.
 Require(capture({"assets/model.prefab","assets/unmodified-character.prefab"})==std::set<void*>{model,unrelatedModel},"new other-character Mod lost its already loaded vanilla owner");
 Require(capture({"assets/unmodified-character.prefab"})==std::set<void*>{unrelatedModel},"only-other-character candidate incorrectly depended on the initial Mod");
 Require(capture({}).empty(),"unmatched character root was captured");
 Reset();
}
void ModelControlRegistrationRegression(){
 using namespace FullResourceReload;Reset();
 RuntimeShapes::enabled=true;RuntimeShapes::packageGeneration=RuntimeShapes::checkedGeneration=generation.load();
 RuntimeControls::Spec spec;spec.marker="marker";spec.vertices=2;RuntimeControls::specs[spec.marker]=spec;
 struct Cleanup {~Cleanup(){RuntimeShapes::enabled=false;Reset();}}cleanup;
 PartConfigured(mainPart,configBox,nullptr);PartFinished(mainPart,nullptr);
 Require(RuntimeShapes::bound.count(oldRenderer)&&!sceneQueries,"model completion did not register controls without scene discovery");
 auto a=MakeAttempt();a.prepared->controls.specs.emplace(spec.marker,spec);recordPartFinished=true;
 Rebuild(a);
 Require(a.phase==Phase::Passed&&RuntimeShapes::bound.size()==1&&RuntimeShapes::bound.count(newRenderer)&&!sceneQueries,
         "F10 discarded new model controls until periodic scene discovery");
 RuntimeShapes::bound.clear();controlRenderers[newRoot]={};RuntimeShapes::RegisterModel(newRoot);
 Require(RuntimeShapes::bound.empty(),"empty model registered a Renderer from another instance");
 controlRenderers[newRoot]={newRenderer,auxiliaryRenderer,unrelatedRenderer};RuntimeShapes::RegisterModel(newRoot);
 Require(RuntimeShapes::bound.size()==3&&!sceneQueries,"new model children were limited to the old Renderer count");
 Reset();RuntimeControls::specs[spec.marker]=spec;RuntimeShapes::packageGeneration=RuntimeShapes::checkedGeneration=generation.load();
 RecordPart();a=MakeAttempt();a.prepared->controls.specs.emplace(spec.marker,spec);recordPartFinished=true;rejectVertex=true;
 bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
 Require(refused&&generation==2&&currentRoot==restoredRoot&&RuntimeShapes::bound.count(oldRenderer)&&!RuntimeShapes::registrationPaused,
         "failed F10 did not restore the old model control registration");
}
void PartCallbackBindingRegression(){
 struct MethodRow {MInfo info{};const char* name;const char* parameters[2];};
 static MethodRow rows[]={{ {},"Invoke",{"System.Boolean","Beyond.Gameplay.View.BaseModelViewPart"}},
  {{},"LoadModel",{"Beyond.Gameplay.View.BaseModelViewPartData","System.Action<System.Boolean,Beyond.Gameplay.View.BaseModelViewPart>"}}};
 for(auto& row:rows)row.info.mp=(void*)1;
 auto previous=std::make_tuple(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_type,il2cpp_class_from_type);
 auto restore=[&]{std::tie(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_type,il2cpp_class_from_type)=previous;};
 il2cpp_class_get_methods=[](void* klass,void** it)->void*{if(*it)return nullptr;*it=(void*)1;return &rows[klass==delegateClass?0:1];};
 il2cpp_method_get_name=[](void* method)->const char*{return static_cast<MethodRow*>(method)->name;};
 il2cpp_method_get_param_count=[](void*)->uint32_t{return 2;};
 il2cpp_method_get_flags=[](void*,uint32_t*)->uint32_t{return 0x86;};
 il2cpp_method_get_return_type=[](void*)->void*{return const_cast<char*>("System.Void");};
 il2cpp_method_get_param=[](void* method,uint32_t index)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->parameters[index]);};
 il2cpp_type_get_name=[](void* type)->const char*{return type==(void*)9?rows[1].parameters[1]:static_cast<const char*>(type);};
 il2cpp_class_get_type=[](void*)->void*{return const_cast<char*>("fixture");};
 il2cpp_class_from_type=[](void* type)->void*{return type==(void*)9?delegateClass:configClass;};
 try{
  auto callback=FullResourceReload::PartCallbackType(instanceClass);
  Require(Exact(instanceClass,"LoadModel",{"Beyond.Gameplay.View.BaseModelViewPartData",callback.c_str()},"System.Void",false)==&rows[1],"model LoadModel binding did not use the actual callback type");
  rows[0].parameters[1]="UnityEngine.Object";bool refused=false;
  try{FullResourceReload::PartCallbackType(instanceClass);}catch(const std::exception&){refused=true;}
  Require(refused,"model callback with a different argument structure was accepted");
 }catch(...){rows[0].parameters[1]="Beyond.Gameplay.View.BaseModelViewPart";restore();throw;}
 rows[0].parameters[1]="Beyond.Gameplay.View.BaseModelViewPart";restore();Setup();Reset();
}
void ChangedMeshGateRegression(){
 using namespace FullResourceReload;Reset();
 auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);
 auto part=plan->parts.at("marker");plan->parts.clear();part.marker="new-mesh-marker";plan->parts.emplace(part.marker,part);
 plan->roots.front().markers={{part.marker,1}};prepared->plan=plan;
 enabled=true;RuntimeShapes::enabled=true;BindWorld();
 RecordPart();
 prepared->pointer={std::string(64,'A'),std::string(64,'B')};pending=prepared;
 auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeBundle::Clock();ticket->config=prepared->config;ticket->controls=prepared->controls;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));
 Frame();Require(leaseRequests==1&&attempt&&attempt->phase==Phase::Leases,"F10 changed Mesh did not directly request native resources");
 attempt.reset();pending.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});enabled=false;Reset();
}
void PrewarmScheduleRegression(){
 using namespace FullResourceReload;
 auto path=std::filesystem::temp_directory_path()/("eff-prewarm-smoke-"+std::to_string(GetCurrentProcessId()));EiemResourceReloadPlan::Save(path,{1,2,3,4});
 {
  auto hold=std::make_shared<NativeBundle::FileHold>(path);auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);plan->inputs.clear();plan->closure.clear();
  for(size_t i=0;i<5;++i){EiemResourceReloadPlan::Input input;input.file="input-"+std::to_string(i)+".vfs";input.path=path;plan->inputs.push_back(input);plan->closure.push_back(i);prepared->files.emplace(i,hold);}prepared->plan=plan;
  Attempt a;a.prepared=prepared;a.phase=Phase::Bundles;NativeBundle::api.load=(void*)50;
  Step(a);Require(opened==4&&a.phase==Phase::Bundles&&!removeCalls&&!cloneCalls,"prewarm must open a bounded batch without exposing a model");
  scheduleDone[(void*)0x8002]=true;Step(a);Require(opened==5&&a.bundles.at(1).done&&!a.bundles.at(0).done,"out-of-order completion stalled on the first Bundle");
  for(auto& request:scheduleDone)request.second=true;Step(a);Require(a.phase==Phase::Assets&&a.bundles.size()==5,"registered native closure did not advance as a batch");
 }
 std::filesystem::remove(path);Reset();
 // A ready graph is inert until F10 targets that exact publication.
  auto a=MakeAttempt();a.phase=Phase::Ready;Step(a);Require(a.phase==Phase::Ready&&!removeCalls&&!loadCalls&&!cloneCalls,"prewarm mutated the current owner before F10");
}
void CommittedRetryRegression(){
 using namespace FullResourceReload;Reset();enabled=true;RuntimeShapes::enabled=true;RecordPart();recordPartFinished=true;
 auto priorNewRoot=newRoot;
 struct Cleanup{void* root;~Cleanup(){newRoot=root;attempt.reset();pending.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});std::atomic_store(&failedKey,std::shared_ptr<const std::string>{});enabled=false;Reset();}}cleanup{priorNewRoot};
 attempt=std::make_shared<Attempt>(MakeAttempt());attempt->prepared->pointer={std::string(64,'A'),std::string(64,'B')};
 CaptureConsumers(attempt->owners.front());Rebuild(*attempt);auto committed=attempt;auto route=std::atomic_load(&routes);auto firstGeneration=generation.load();
 std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(attempt->prepared->pointer));
 auto request=[&]{auto ticket=std::make_shared<Ticket>();ticket->pointer=attempt->prepared->pointer;ticket->clock=NativeBundle::Clock();ticket->controls=attempt->prepared->controls;ticket->config=attempt->prepared->config;std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));};
 // A UI transition can fail the owner preflight. The candidate is already
 // committed and owns the published routes: never drain or re-open its CABs.
 liveParts.clear();RuntimeShapes::bound[newRenderer].renderer=Ref::Make(newRenderer,true);request();Frame();
 Require(attempt==committed&&attempt->phase==Phase::Passed&&std::atomic_load(&activeNative)==committed&&std::atomic_load(&routes)==route&&generation==firstGeneration&&!std::atomic_load(&failedKey),
         "a rejected second F10 invalidated/drained the committed candidate, forcing duplicate Bundle loads");
 Frame();Require(!unloadCalls&&!opened,"rejected committed candidate retired its live Bundle containers");
 RecordPart();newRoot=(void*)0x1005;request();for(unsigned i=0;i<8&&generation==firstGeneration;++i)Frame();
 Require(generation==firstGeneration+1&&attempt==committed&&attempt->phase==Phase::Passed&&!opened&&animationRoot==currentRoot,
         "retry after the UI preflight repeated native preparation instead of reusing the committed graph: "+attempt->error+" generation="+std::to_string(generation.load()));
}
void ReleasedPoolRegression(){
 using namespace FullResourceReload;Reset();RecordPart();BindWorld();
 auto snapshots=RuntimeShapes::groups.size();PartReleased(mainPart,nullptr);
 Require(!liveParts.count(mainPart)&&!RuntimeShapes::bound.count(oldRenderer)&&RuntimeShapes::Alive(oldRenderer)&&RuntimeShapes::groups.size()==snapshots,
         "game release returned a live Renderer to its pool but left it in active owner/control checks");
 auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);
 Require(a.owners.empty(),"released pooled model still counted as a live owner");
 currentRoot=oldRoot;PartConfigured(mainPart,configBox,nullptr);BasePartFinished(mainPart,nullptr);a.owners.clear();DiscoverOwners(a);
 Require(a.owners.size()==1&&a.owners.front().viewPart,"base display completion did not record the game's reused view part");Reset();
}
void DisplayOverrideRegression(){
 using namespace FullResourceReload;Reset();auto a=MakeAttempt();auto& owner=a.owners.front();owner.viewPart=true;
 displayOverride=true;overrideReleases=0;RemoveOwner(owner);displayOverride=false;
 Require(overrideReleases==1,"display release invoked a fixed ComplexModelViewPart body instead of the actual game's virtual override");Reset();
}
void RuleMemoryRegression(){
 using namespace FullResourceReload;Reset();memoryCalls=fileFallbackCalls=0;
 NativeBundle::api.byteClass=byteClass;NativeBundle::api.streamAsync=(void*)87;NativeBundle::api.streamAsyncIcall=nullptr;NativeBundle::api.streamIcall=nullptr;
 NativeBundle::api.streamClass=(void*)0xAA11;NativeBundle::api.streamCtor=(void*)88;
 il2cpp_object_new=[](void* klass)->void*{Require(klass==NativeBundle::api.streamClass,"archive stream managed class differs");auto b=std::make_unique<Bytes>(8);auto p=b->data();storage.push_back(std::move(b));return p;};
 auto emptyPlan=std::make_shared<EiemResourceReloadPlan::Plan>();emptyPlan->rulesOnly=true;
 auto context=std::make_shared<EiemPrefabRules::Context>(emptyPlan,Package{});
 auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);plan->inputs.resize(1);plan->closure={0};prepared->plan=plan;prepared->rules=context;prepared->ruleInputs.emplace(0,std::make_shared<const Bytes>(Bytes{1,2,3,4}));
 {Attempt a;a.prepared=prepared;a.phase=Phase::Bundles;Step(a);Require(memoryCalls==1&&opened==1&&a.bundles.at(0).bytes.Get()&&a.bundles.at(0).request.Get()&&prepared->ruleInputs.empty(),"F10 rules lost Byte[] roots or retained duplicate worker payloads");scheduleDone.begin()->second=true;Step(a);Require(a.phase==Phase::Assets&&a.bundles.at(0).done&&!a.bundles.at(0).bytes.Get(),"memory Bundle completion did not retire its array/advance to Prefabs");}
 {auto bytes=NativeBundle::Array(Bytes{1,2,3,4});auto request=NativeBundle::LoadMemory(bytes.Get(),0,true);bytes={};NativeBundle::CollectMemory();Require(NativeBundle::memoryRequests.size()==1,"async archive stream retired before completion");scheduleDone[request]=true;NativeBundle::CollectMemory();Require(NativeBundle::memoryRequests.empty()&&!NativeBundle::memoryBundles.empty(),"completed Bundle lost its lazy seek/read stream");}
 auto resolver=il2cpp_resolve_icall;il2cpp_resolve_icall=[](const char* name)->void*{Require(std::string(name)=="UnityEngine.AssetBundle::LoadFromStreamInternal"||std::string(name)=="UnityEngine.AssetBundle::LoadFromStreamAsyncInternal","archive stream icall name differs");return (void*)+[](void* stream,uint32_t crc,uint32_t bufferSize)->void*{auto array=*(void**)stream;Require(crc==0&&bufferSize==0&&memcmp((char*)array+32,"\1\2\3\4",4)==0,"sync stream icall signature/payload differs");return bundle;};};
 NativeBundle::api.stream=nullptr;NativeBundle::api.streamIcall=(NativeBundle::Api::StreamFn)il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadFromStreamInternal");NativeBundle::api.streamAsyncIcall=(NativeBundle::Api::StreamFn)il2cpp_resolve_icall("UnityEngine.AssetBundle::LoadFromStreamAsyncInternal");
 {auto bytes=NativeBundle::Array(Bytes{1,2,3,4});Require(NativeBundle::LoadMemory(bytes.Get(),0,false)==bundle,"sync stream icall returned wrong native object");}il2cpp_resolve_icall=resolver;NativeBundle::api.streamIcall=NativeBundle::api.streamAsyncIcall=nullptr;
 emptyPlan->originals.push_back({"test.vfs",std::string(64,'A'),std::filesystem::path("E:/rule-source.chk"),12,4});
 std::atomic_store(&resourceRules,context);bool wasActive=active.exchange(true);auto fallback=[](void*,uint32_t,uint64_t,void*)->void*{++fileFallbackCalls;return bundle;};
 auto absent=StringValue("E:/unmatched.chk"),matched=StringValue("E:/rule-source.chk");auto calls=memoryCalls;
 Require(ObservePublicFile("rule-unmatched",0,fallback,absent,77,12,nullptr)==bundle&&fileFallbackCalls==1&&memoryCalls==calls,"unmatched file/nonzero CRC intercepted by rules");
 Require(ObservePublicFile("rule-bad-crc",0,fallback,matched,77,12,nullptr)==bundle&&fileFallbackCalls==2,"failed rule returned a null request instead of preserving file load");
 NativeBundle::enabled=true;
 Require(ObservePublicFile("rule-bad-source",0,fallback,matched,0,12,nullptr)==bundle&&fileFallbackCalls==3,"missing original source blocked the game's loading request");
 auto originalFile=file;file=fallback;Require(File(matched,0,12,nullptr)==bundle&&fileFallbackCalls==4,"internal file binding bypassed rule failure passthrough");file=originalFile;
 for(const auto& b:NativeBundle::memoryBundles)unloadedStreamBundles.insert(b.first);NativeBundle::CollectMemory();Require(NativeBundle::memoryBundles.empty(),"unloaded rule Bundles retained archive streams");
 NativeBundle::enabled=false;active=wasActive;std::atomic_store(&resourceRules,std::shared_ptr<EiemPrefabRules::Context>{});Reset();
}
void LatestPublicationRegression(){
 using namespace FullResourceReload;
 enabled=true;RuntimeShapes::enabled=true;pending.reset();
 auto prepared=Prepare();prepared->pointer={std::string(64,'A'),std::string(64,'B')};
 attempt=std::make_unique<Attempt>();attempt->prepared=prepared;attempt->phase=Phase::Ready;
 auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeBundle::Clock();ticket->controls=prepared->controls;ticket->config=prepared->config;
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
 auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeBundle::Clock();ticket->config=prepared->config;ticket->controls=prepared->controls;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));
 Frame();Require(!leaseRequests&&!removeCalls&&attempt->selectedRoots.empty(),"F10 proactively loaded an unvisited original prefab");
}
void BackgroundReadyRegression(){
 using namespace FullResourceReload;
 struct Cleanup {~Cleanup(){attempt.reset();pending.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});enabled=false;Reset();}}cleanup;
 Reset();enabled=true;RuntimeShapes::enabled=true;RecordPart();done=1;
 auto file=std::filesystem::temp_directory_path()/("eff-background-"+std::to_string(GetCurrentProcessId()));EiemResourceReloadPlan::Save(file,{1,2,3,4});
 struct FileCleanup {std::filesystem::path file;~FileCleanup(){FullResourceReload::attempt.reset();FullResourceReload::pending.reset();std::atomic_store(&FullResourceReload::activeNative,std::shared_ptr<Attempt>{});std::filesystem::remove(file);}}files{file};
 auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);
 plan->inputs.push_back({"input.vfs",{},"bundle",file,{}});plan->closure={0};plan->roots.push_back(plan->roots.front());plan->roots.back().path="assets/other.prefab";
 prepared->plan=plan;prepared->files.emplace(0,std::make_shared<NativeBundle::FileHold>(file));NativeBundle::api.load=(void*)50;
 prepared->pointer={std::string(64,'A'),std::string(64,'B')};pending=prepared;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));
 for(unsigned frame=0;frame<256;++frame){for(auto& r:scheduleDone)r.second=true;Frame();}
 Require(attempt&&attempt->phase==Phase::Ready&&opened==prepared->plan->closure.size(),
         "candidate stayed idle until F10, putting all native loads on the refresh path");
 Require(!removeCalls&&!loadCalls&&generation==2&&std::atomic_load(&routes)->empty(),
         "background preparation exposed resources or rebuilt a live model before F10");
 Require(leaseRequests==1&&attempt->selectedRoots.size()==1&&!attempt->assets.at(1).prefab.Get(),"preparation loaded every offline root instead of only the game's loaded prefab");
 auto loaded=opened,leased=leaseRequests;auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;
 ticket->clock=NativeBundle::Clock();ticket->config=prepared->config;ticket->controls=prepared->controls;
 std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));
 for(unsigned frame=0;frame<8&&attempt->phase!=Phase::Passed;++frame)Frame();
 Require(attempt->phase==Phase::Passed&&opened==loaded&&leaseRequests==leased&&animationRebinds==1,
         "F10 repeated resource requests instead of committing the prepared native graph: "+attempt->error);
 resourceLabel="assets/other.prefab";resourceSource=unrelatedRoot;fixtureProxyHash=std::hash<std::string>{}(resourceLabel);
 ProxyPreload(instance,false,nullptr);
 Require(proxyCloneResult==lazyRoot&&attempt->assets.at(1).prefab.Get()==lazyCandidate&&leaseRequests==leased+1&&!proxyContext,
         "game's inlined AssetProxy prefab creation bypassed the offline candidate for a later character");
 Require(ResourceGet(leaseHandle,nullptr)==lazyCandidate&&leaseRequests==leased+1,"ordinary resource handle did not reuse the prepared later-character route");
 instanceHandle=true;Require(ResourceGet(leaseHandle,nullptr)==unrelatedRoot,"an instantiated GameObject handle was replaced with a prefab asset");instanceHandle=false;
 proxyCloneResult=nullptr;ProxyPreload(instance,true,nullptr);Require(proxyCloneResult==lazyRoot&&!proxyContext,"async prefab creation lost the committed candidate or leaked caller context");
 Require(Clone(unrelatedRoot)==lazyRoot,"later source prefab did not register its exact clone route");
}
void OfflineRejectionRegression(){
 using namespace FullResourceReload;Reset();
 auto previous=gameDirectory;auto folder=std::filesystem::absolute("bin")/("eff-offline-reject-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
 Require(std::filesystem::create_directory(folder),"offline reject fixture already exists");
 struct Cleanup {std::filesystem::path game,folder;~Cleanup(){attempt.reset();pending.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});enabled=false;gameDirectory=game;std::filesystem::remove_all(folder);Reset();}}cleanup{previous,folder};
 gameDirectory=folder;enabled=true;RuntimeShapes::enabled=true;auto cache=folder/"plugin/resource-reload-cache";std::filesystem::create_directories(cache);
 auto prepared=Prepare();prepared->pointer={std::string(64,'A'),std::string(64,'B')};
 attempt=std::make_unique<Attempt>();attempt->prepared=prepared;attempt->phase=Phase::Ready;
 for(bool trigger:{false,true}){
  EiemResourceReloadPlan::Save(cache/"current.tsv",{'b','a','d'});
  auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeBundle::Clock();ticket->controls=prepared->controls;ticket->config=prepared->config;
  std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));
  PollOffline(trigger);Require(!std::atomic_load(&requested)&&!std::atomic_load(&latest),"offline failure left a valid ticket/latest pointer to an old ready candidate");
  Frame();Require(attempt->phase==Phase::Ready&&!removeCalls&&!loadCalls,"malformed disk publication committed old candidate");
  std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));Frame();
  Require(attempt->phase==Phase::Ready&&!removeCalls&&!loadCalls,"repairing disk after failure committed without a new F10");
 }
}
void PreparedShortcutRegression(){
 using namespace FullResourceReload;Reset();auto previousGame=gameDirectory;auto previousDisk=diskPrepared;auto previousPointer=lastPrepared;bool previousActive=active;
 auto folder=std::filesystem::absolute("bin")/("reload-shortcut-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(folder),"shortcut fixture already exists");
 struct Cleanup {std::filesystem::path game,folder;std::shared_ptr<Prepared> disk;EiemOfflinePackage::Pointer pointer;bool activeBefore;~Cleanup(){diskPrepared=disk;lastPrepared=pointer;gameDirectory=game;active=activeBefore;enabled=false;std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});std::filesystem::remove_all(folder);Reset();}}cleanup{previousGame,folder,previousDisk,previousPointer,previousActive};
 gameDirectory=folder;active=true;enabled=true;auto cache=folder/"plugin/resource-reload-cache";std::filesystem::create_directories(cache);std::filesystem::create_directories(folder/"plugin/tools");
 // If the production F10 seam unnecessarily invokes a compiler, this file
 // fails process creation. Already-proven unchanged inputs need no process.
 EiemResourceReloadPlan::Save(folder/"plugin/tools/eff_resource_pack.exe",{1,2,3,4});
 auto prepared=Test::Prepare();prepared->graph=prepared->controls.graph;auto plan=std::make_shared<Plan>(*prepared->plan);plan->parts.clear();plan->materials.clear();prepared->plan=plan;
 prepared->pointer={std::string(64,'A'),std::string(64,'B')};diskPrepared=prepared;lastPrepared=prepared->pointer;EiemOfflinePackage::Publish(cache,prepared->pointer);
 FullResourceReload::Prepare();auto ticket=std::atomic_load(&requested);Require(ticket&&ticket->config&&ticket->pointer==prepared->pointer,"unchanged prepared F10 invoked offline compilation instead of creating its commit ticket");
 Require(!opened&&!leaseRequests&&!removeCalls&&!loadCalls,"shortcut preflight performed native model/resource work");
 auto added=folder/"plugin/mods/Added";std::filesystem::create_directories(added);EiemResourceReloadPlan::Save(added/"mod.ini",{'[','M','o','d',']'});
 FullResourceReload::Prepare();Require(!std::atomic_load(&requested)&&!std::atomic_load(&latest),"new Mod bypassed the required compiler and used the previous set");
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
void FullFrameRegression(bool display,bool vanilla=false,bool pristine=false){
 using namespace FullResourceReload;Reset();enabled=true;RuntimeShapes::enabled=true;BindWorld();done=1;
 auto file=std::filesystem::temp_directory_path()/("eff-frame-"+std::to_string(GetCurrentProcessId()));EiemResourceReloadPlan::Save(file,{1,2,3,4});
 {
  auto prepared=Prepare();auto plan=std::make_shared<EiemResourceReloadPlan::Plan>(*prepared->plan);
  if(pristine){plan->composed=true;plan->parts.clear();plan->materials.clear();plan->roots.front().markers.clear();
#ifndef EFF_OFFLINE_PACKAGE_V35
   plan->roots.front().assembly.sources.front().targets.front().marker.clear();
#endif
  }
  plan->inputs.push_back({"input-001.vfs",{},"bundle",file,{}});plan->closure={0};prepared->plan=plan;
  prepared->files.emplace(0,std::make_shared<NativeBundle::FileHold>(file));prepared->pointer={std::string(64,'A'),std::string(64,'B')};pending=prepared;
  std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(prepared->pointer));NativeBundle::api.load=(void*)50;
  for(unsigned repeat=0;repeat<8;++repeat){
   // Exercise the real discovery -> rebuild call chain with Unity's spelling,
   // rather than only testing the path comparison helper in isolation.
   const char* paths[]={"assets/model.prefab","Assets/Model.prefab","ASSETS\\MODEL.prefab"};
   modelPath=paths[repeat%3];
   generation=2;currentRoot=oldRoot;animationRoot=oldRoot;RuntimeControls::Spec spec;spec.marker="marker";RuntimeControls::specs.emplace("marker",spec);BindWorld();
   sceneRenderers={oldRenderer,unrelatedRenderer};
   if(vanilla){RuntimeShapes::bound.clear();RuntimeControls::specs.clear();}
   RecordPart(!display);
   auto fresh=Prepare();auto ticket=std::make_shared<Ticket>();ticket->pointer=prepared->pointer;ticket->clock=NativeBundle::Clock();ticket->config=fresh->config;ticket->controls=fresh->controls;
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
void MembershipPreflightRegression(){
 using namespace FullResourceReload;Reset();
 auto folder=std::filesystem::temp_directory_path()/("eff-membership-"+std::to_string(GetCurrentProcessId()));
 auto previousGame=gameDirectory;gameDirectory=folder;auto mods=folder/"plugin/mods";std::filesystem::create_directories(mods);
 struct Cleanup {std::filesystem::path folder,game;~Cleanup(){gameDirectory=game;std::filesystem::remove_all(folder);}}cleanup{folder,previousGame};
 auto a=MakeAttempt();a.prepared->graph=std::make_shared<const std::vector<Package>>();
 Require(AuthorFilesUnchanged(*a.prepared),"empty installed Mod set failed metadata preflight");
 std::filesystem::create_directory(mods/"NewCharacter");EiemResourceReloadPlan::Save(mods/"NewCharacter/mod.ini",{'x'});
 auto original=std::atomic_load(&published);auto selection=RuntimeControls::selected;auto revision=RuntimeControls::revision;
 bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
 Require(refused&&!removeCalls&&!loadCalls&&generation==2&&std::atomic_load(&published)==original&&RuntimeControls::selected==selection&&RuntimeControls::revision==revision,"membership changed after preflight published stale graph or controls");
 Reset();
}
void PristineIdentityRegression(){
 using namespace FullResourceReload;Reset();auto a=MakeAttempt();auto plan=std::make_shared<Plan>(*a.prepared->plan);
 plan->composed=true;plan->parts.clear();plan->materials.clear();plan->roots.front().markers.clear();
#ifndef EFF_OFFLINE_PACKAGE_V35
 plan->roots.front().assembly.sources.front().targets.front().marker.clear();
#endif
 a.prepared->plan=plan;ignoreCandidate=true;bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
 Require(refused&&generation==2&&currentRoot==restoredRoot&&MeshProfile(currentRoot)==a.owners.front().originalMeshes,"last Mod deletion accepted the old native Mesh despite candidate route");Reset();
}
void ModSetCommitRegression(){
 using namespace FullResourceReload;
 auto mod=[](const char* id){Package p;p.id=id;p.root=std::filesystem::path("fixture")/id;p.defaults={{"$switch",0}};p.persistent={"$switch"};p.bindings.push_back({"style","$switch","cycle",{VK_NUMPAD1,0},{0,1},1});return p;};
 auto graph=[](std::vector<Package> p){return std::make_shared<const std::vector<Package>>(std::move(p));};
 auto empty=graph({});auto initial=RuntimeControls::Prepare(empty,EiemGlobalConfig{});RuntimeControls::Swap(initial);std::atomic_store(&published,empty);
 for(const auto& packages:std::vector<std::vector<Package>>{{mod("A")},{mod("A"),mod("B")},{mod("A")},{},{mod("B")}}){
  Reset();auto next=graph(packages);auto a=MakeAttempt();a.prepared->controls=RuntimeControls::Prepare(next,EiemGlobalConfig{});
  if(packages.empty()){auto plan=std::make_shared<Plan>(*a.prepared->plan);plan->composed=true;plan->parts.clear();plan->materials.clear();plan->roots.front().markers.clear();
#ifndef EFF_OFFLINE_PACKAGE_V35
   plan->roots.front().assembly.sources.front().targets.front().marker.clear();
#endif
   a.prepared->plan=plan;
  }
  auto before=std::atomic_load(&published);auto selection=RuntimeControls::selected;auto panel=RuntimeControls::panel.Read();auto revision=RuntimeControls::revision;
  auto failed=MakeAttempt();failed.prepared->controls=RuntimeControls::Prepare(next,EiemGlobalConfig{});rejectVertex=true;bool refused=false;try{Rebuild(failed);}catch(const std::exception&){refused=true;}
  Require(refused&&std::atomic_load(&published)==before&&RuntimeControls::selected==selection&&RuntimeControls::panel.Read()==panel&&RuntimeControls::revision==revision,"failed Mod membership transaction published controls or selection");
  rejectVertex=false;currentRoot=oldRoot;removeCalls=discardCalls=loadCalls=0;
  Rebuild(a);Require(a.phase==Phase::Passed&&std::atomic_load(&published)==next&&RuntimeControls::controlGraph==next&&RuntimeControls::panel.Read()->graph==next,"Mod membership native commit did not atomically publish graph/UI");
  Require(RuntimeControls::selected==(packages.empty()?"":packages.front().id),"Mod membership native commit default/fallback selection differs");
  if(packages.size()==2){RuntimeControls::selected="B";RuntimeControls::PublishPanel();}
 }
 RuntimeControls::controlGraph.reset();RuntimeControls::keys.clear();RuntimeControls::variables.clear();RuntimeControls::selected.clear();RuntimeControls::panel.Publish({});Reset();
}
}
#ifndef EFF_OFFLINE_PACKAGE_V35
void AssemblyAddressRegression(){
 using namespace Test;using namespace FullResourceReload;
 Reset();auto prepared=Test::Prepare();auto expected=prepared->plan->roots.front();
 VerifyAssemblyPrefab(candidate,expected);
 auto reject=[&](auto edit){auto broken=expected;edit(broken);bool failed=false;try{VerifyAssemblyPrefab(candidate,broken);}catch(const std::exception&){failed=true;}Require(failed,"invalid serialized Renderer address map accepted");};
 reject([](auto& root){root.assembly.sources.front().targets.front().renderer.component=1;});
 reject([](auto& root){root.assembly.sources.front().targets.front().marker="foreign";});
 reject([](auto& root){root.assembly.sources.front().targets.front().bones.front()=EiemRenderAssemblyDescription::Segment("marker",0);});
 reject([](auto& root){root.assembly.sources.front().targets.front().root=EiemRenderAssemblyDescription::Segment("marker",0);});
 Reset();
}
#endif
int main(){
 using namespace Test;using namespace FullResourceReload;
 try{
  Setup();Reset();
#ifndef EFF_OFFLINE_PACKAGE_V35
  AssemblyAddressRegression();
#endif
  auto check=[](const char* name,auto action){try{action();}catch(const std::exception& e){throw std::runtime_error(std::string(name)+": "+e.what());}};
  check("synchronous world owner",SynchronousOwnerRegression);
  check("LoadModel release ownership",LoadReleaseOwnershipRegression);
  check("bone verification cost",BoneVerificationCostRegression);
  check("background candidate readiness",BackgroundReadyRegression);
  check("enum",EnumRegression);check("bones",BonePathRegression);check("root paths",RootPathRegression);check("managers",ManagerContractRegression);
  check("Material property bindings",MaterialBindingRegression);check("native lifecycle",NativeLifecycleRegression);check("typed character audio",[]{typedAudioOnly=true;NativeLifecycleRegression();typedAudioOnly=false;});check("texture identity",TextureIdentityRegression);check("load scheduling",PrewarmScheduleRegression);check("committed retry",CommittedRetryRegression);check("released pool",ReleasedPoolRegression);check("display virtual override",DisplayOverrideRegression);check("rule memory loads",RuleMemoryRegression);
  check("latest publication",LatestPublicationRegression);check("offline failure cancellation",OfflineRejectionRegression);check("prepared shortcut",PreparedShortcutRegression);check("startup",StartupNoWorldRegression);check("Mesh gate",ChangedMeshGateRegression);check("retirement",RetirementRegression);
  check("world Frame",[]{FullFrameRegression(false);});check("display Frame",[]{FullFrameRegression(true);});
  check("consecutive world F10",SequentialWorldRegression);
  check("part callback owners",PartOwnerRegression);
  check("same entity auxiliary helpers",AuxiliaryHelperRegression);
  check("other character without previous Mod bindings",OtherCharacterOwnerRegression);
  check("model completion control registration",ModelControlRegistrationRegression);
  check("part callback binding",PartCallbackBindingRegression);
  check("add first/current vanilla character",[]{FullFrameRegression(false,true);});check("delete last world Mod",[]{FullFrameRegression(false,true,true);});
  check("delete last display Mod",[]{FullFrameRegression(true,true,true);});check("membership preflight",MembershipPreflightRegression);
  check("pristine identity",PristineIdentityRegression);check("Mod set native commit",ModSetCommitRegression);check("idle pools",IdlePoolRegression);
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
  std::cout<<"PASS full resource transaction (mock native APIs): rule archive stream ABI and async/lazy roots; stripped stream methods via engine icalls; failed/unmatched rule preserves original load; six native prefab factories preserve arguments; native animation resume/weapons/audio lifecycle; stale animation reproduced; old Mesh/Texture identity rejection with animated rollback; exact typed static/instance manager getters; startup zero extra native requests; full Frame world/display -> commit; eight consecutive world reloads without resetting state; asynchronous/display/retired callback owners; structural callback binding; full bone paths; enum ABI; superseded tickets; rollback; native retirement\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
