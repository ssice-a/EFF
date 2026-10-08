#include "../../src/eiem_resource_entry.cpp"
#include <iostream>
using namespace Entry;
namespace Test {
void *propertyArrayClass=(void*)0xAB00,*propertyElementClass=(void*)0xAB01;
std::set<void*> propertyArrays;
std::vector<int32_t> propertyIds{314};std::map<int32_t,void*> propertyTextures;int propertyElementWidth=4;
bool shaderSupported=true;int shaderPasses=2;std::string shaderLabel="Fixture/Character";
void *source=(void*)0x1000,*candidate=(void*)0x1001,*oldRoot=(void*)0x1002,*newRoot=(void*)0x1003,*restoredRoot=(void*)0x1004;
void *lazyCandidate=(void*)0x1008,*lazyRoot=(void*)0x1009;
void *model=(void*)0x1010,*entity=(void*)0x1011,*instance=(void*)0x1012,*manager=(void*)0x1013,*effect=(void*)0x1014;
void *world=(void*)0x1110,*performManager=(void*)0x1111;
bool performActive=false,performPropAlive=false,stalePerformProp=false,unrelatedPerformActive=false;
unsigned performStops=0;
bool characterAnimation=false,performIdleHeld=true,performDashHeld=true;
unsigned performIdlePreloads=0,performDashPreloads=0;
void* characterAnimationClass=(void*)0x2110;
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
void* weaponMountRoot=(void*)0x1002;void* weaponRequestMountRoot=nullptr;
std::vector<int> consumerCalls;
std::map<void*,std::vector<uint64_t>> dependencyLists;
std::map<uint64_t,std::vector<uint64_t>> nativeDependencyLists;
std::map<std::string,uint64_t> dependencyNames;uint64_t dependencyHash=0;bool dependencyHashDiffers=false;void* dependencyManifest=(void*)0x7000;
Bytes dependencyMapped;unsigned dependencyManifestReads=0,dependencyNameChecks=0;
void BuildDependencyManifest(){
 namespace D=StartupDependencies;
 const size_t data=16+dependencyNames.size()*D::api.bundleSize;dependencyMapped.assign(data,0);size_t i=0;
 for(const auto& row:dependencyNames){size_t record=16+i++*D::api.bundleSize;int32_t offset=int32_t(dependencyMapped.size()-data),length=int32_t(row.first.size()*sizeof(wchar_t));
  memcpy(dependencyMapped.data()+record+D::api.bundleNameOffset,&offset,sizeof(offset));memcpy(dependencyMapped.data()+record+D::api.bundleKeyOffset,&row.second,sizeof(row.second));
  auto start=dependencyMapped.size();dependencyMapped.resize(start+sizeof(length)+length);memcpy(dependencyMapped.data()+start,&length,sizeof(length));for(size_t c=0;c<row.first.size();++c){wchar_t value=wchar_t(row.first[c]);memcpy(dependencyMapped.data()+start+sizeof(length)+c*sizeof(value),&value,sizeof(value));}
 }
}
int nativeFactory=1;bool ignoreCandidate=false;
bool recordPartFinished=false,completePartLoading=true;
bool typedAudioOnly=false;
bool displayOverride=false;unsigned overrideReleases=0;
void* callbackTarget=(void*)0x1010;
void *oldTexture=(void*)0x4103,*newTexture=(void*)0x4104,*textureValue=(void*)0x4103;bool identitySameMesh=false;
void *factoryParent=(void*)0x4100,*factoryPosition=(void*)0x4101,*factoryRotation=(void*)0x4102;
void *oldRenderer=(void*)0x1020,*newRenderer=(void*)0x1021,*oldMesh=(void*)0x1022,*newMesh=(void*)0x1023;
bool staticRenderers=false;void *skinClass=(void*)0x2020,*staticClass=(void*)0x2021,*filterObject=(void*)0x2022;
unsigned staticSkinCalls=0;unsigned char staticEnabled=1;
void *uiLoader=(void*)0x2050,*uiDictionary=(void*)0x2051,*uiClosure=(void*)0x2052,*uiCallback=(void*)0x2053,*uiProxy=(void*)0x2054;
void *uiObjectClass=(void*)0x2057,*uiBooleanClass=(void*)0x2058,*uiFactory=(void*)0x2059,*uiPage=(void*)0x205a;
void* uiLuaManager=(void*)0x205b;
void *uiLuaKey=(void*)0x205c,*uiEncryptedCode=(void*)0x205d;void* uiPlainCode=nullptr;bool uiEncryptedOnly=false,uiEncodingMismatch=false;unsigned uiEncryptions=0;
void* uiLuaException=(void*)0x205e;bool uiLuaRaises=false;
void* uiRequestCallback=nullptr;unsigned uiPageBinds=0,nullDelegateTargetCalls=0;bool uiPageAvailable=true,uiPageReject=false;
std::set<void*> uiOwned;void* uiRoot=nullptr;unsigned uiCallbacks=0,uiUnloads=0;bool uiInline=false;
void* uiCompletionDelegate=(void*)0x2055;void* uiProxyRoot=nullptr;void* uiResultRoot=nullptr;bool creatingUI=false,uiLateInstantiate=false;void* uiUntrackedClass=(void*)0x2056;bool uiNativeReady=false,uiDelayed=false,uiCancelled=false;unsigned uiCancelledCompletions=0;
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
void* GetClass(void* p){if(p==animation&&characterAnimation)return characterAnimationClass;if(p==&yes||p==&no)return uiBooleanClass;if(p==uiProxy)return uiUntrackedClass;if(p==oldRenderer||p==newRenderer||p==unrelatedRenderer||p==auxiliaryRenderer)return staticRenderers?staticClass:skinClass;return propertyArrays.count(p)?propertyArrayClass:p==assetProxyObject?assetProxyClass:(p==source||p==candidate||p==lazyCandidate||p==unrelatedRoot)?gameObjectClass:p==audio?audioClass:(p==model||p==unrelatedModel)?modelClass:p==loadCallback?delegateClass:p==leaseHandle?handleClass:p==pathValue?pathClass:(p==configBox||p==unrelatedConfig)?configClass:instanceClass;}
void* Fields(void*,void** it){auto n=uintptr_t(*it);if(n==17)return nullptr;*it=(void*)(n+1);return (void*)(n+1);}
const char* FieldName(void* f){const char* names[]={"","modelManager","effectManager","m_entity","m_mainPart","m_cfg","usePersistentPool","modelPath","m_model","m_loadFinishCb","hash","m_untrackedHandle","m_instance2AssetHandle","<>4__this","parent","callback","requestId","m_disposed"};return names[uintptr_t(f)];}
const char* TypeName(void* t){const char* names[]={"","Beyond.Gameplay.View.ModelManager","Beyond.Gameplay.EffectManager","Beyond.Gameplay.Core.Entity","Beyond.Gameplay.View.ComplexModelViewPart","Beyond.Gameplay.View.BaseModelViewPartData","System.Boolean","System.String","UnityEngine.GameObject","System.Action<System.Boolean,Beyond.Gameplay.View.BaseModelViewPart>","System.UInt64","Beyond.Resource.FAssetProxyUntrackedHandle","Dictionary","Beyond.UI.UIModelLoader","UnityEngine.Transform","System.Action<UnityEngine.GameObject>","System.Int32","System.Boolean"};return names[uintptr_t(t)];}
int FieldFlags(void* f){return (f==(void*)1||f==(void*)2)&&managersStatic?0x16:0x6;}
void FieldValue(void* self,void* f,void* out){Require(!(FieldFlags(f)&0x10)&&self,"instance getter used for a static field");
 if(f==(void*)160){Require(self==instance,"world field read on another receiver");*(void**)out=world;return;}
 if(f==(void*)161){Require(self==world,"perform manager field read on another world");*(void**)out=performManager;return;}
 if(f==(void*)150){*(void**)out=dependencyManifest;return;}
 if(f==(void*)151){*(void**)out=dependencyMapped.empty()?nullptr:dependencyMapped.data();return;}
 if(f==(void*)152){*(int64_t*)out=int64_t(dependencyMapped.size());return;}
 if(f==(void*)153){*(void**)out=dependencyMapped.data()+16+dependencyNames.size()*StartupDependencies::api.bundleSize;return;}
 if(f==(void*)154){++dependencyManifestReads;memset(out,0,StartupDependencies::api.arraySize);int32_t count=int32_t(dependencyNames.size());auto head=dependencyMapped.data()+16;memcpy((char*)out+StartupDependencies::api.arrayCount,&count,sizeof(count));memcpy((char*)out+StartupDependencies::api.arrayHead,&head,sizeof(head));return;}
 if(f==(void*)18){std::vector<void*> values;for(auto value:dependencyLists.at(self))values.push_back((void*)value);auto array=Array(values);propertyArrays.insert(array);*(void**)out=array;return;}
 if(f==(void*)19){*(int*)out=int(dependencyLists.at(self).size());return;}
 if(f==(void*)12){*(void**)out=uiDictionary;return;}if(f==(void*)13){*(void**)out=uiLoader;return;}if(f==(void*)14){*(void**)out=factoryParent;return;}if(f==(void*)15){*(void**)out=uiRequestCallback;return;}if(f==(void*)16){*(int*)out=7;return;}if(f==(void*)17){*(bool*)out=false;return;}
 if(f==(void*)11){memset(out,0,64);return;}if(f==(void*)10){*(uint64_t*)out=pathHashValue;return;}if(f==(void*)5){memset(out,0,64);memcpy(out,&self,8);return;}if(f==(void*)6){*(unsigned char*)out=0;return;}
 auto v=f==(void*)1?manager:f==(void*)2?effect:f==(void*)3?(self==unrelatedHelper?unrelatedEntity:hasEntity?entity:nullptr):f==(void*)4?(self==unrelatedModel?unrelatedPart:hasPart?mainPart:nullptr):f==(void*)7?StringValue(self==unrelatedConfig?"assets/unmodified-character.prefab":modelPath):self==unrelatedPart?unrelatedRoot:currentRoot;memcpy(out,&v,8);}
void StaticFieldValue(void* f,void* out){Require((FieldFlags(f)&0x10)&&(f==(void*)1||f==(void*)2),"static getter used for an instance field");auto v=f==(void*)1?manager:effect;memcpy(out,&v,8);}
void* NativeClone(void* p){++cloneCalls;return p==lazyCandidate?lazyRoot:p==candidate&&!ignoreCandidate?(creatingUI&&uiResultRoot?uiResultRoot:newRoot):restoredRoot;}
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
 if(m==140){dependencyLists[self].clear();return nullptr;}
 if(m==141){dependencyLists.at(self).push_back(*(uint64_t*)args[0]);return nullptr;}
 if(m==142){dependencyHash=dependencyNames.at(String(args[0]));if(dependencyHashDiffers)dependencyHash^=0x7000000000000000ULL;return &dependencyHash;}
 if(m==143){++dependencyNameChecks;for(const auto& row:dependencyNames)if(row.second==*(uint64_t*)args[0])return StringValue(row.first);return nullptr;}
 if(m==144)return dependencyManifest;
 if(m==1)return &yes;
 if(m==2){++aliveCalls;auto p=args[0];return !p||(p==source&&!sourceAlive)||unloadedStreamBundles.count(p)?&no:&yes;}
 if(m==3){id=int(uintptr_t(self));return &id;}
 if(m==4)return StringValue(self==shader?shaderLabel:self==material?"body":self==skeletonRoot?"Root":self==bip?"Bip001":self==left?"Left":self==right?"Right":(self==bone||self==leftBone||self==rightBone)?"Bone":"marker");
 if(m==5)return Array(self==unrelatedRoot?unrelatedRenderer:self==newRoot||self==uiResultRoot||self==candidate||self==lazyCandidate?newRenderer:oldRenderer);
 if(m==6)return self==newMesh&&rejectVertex?&badVertex:&two;
 if(m==7)return &zero;
 if(m==8){Require(!staticRenderers,"static Renderer called get_bones");++staticSkinCalls;return Array(bone);}
 if(m==9)return Array(staticRenderers?std::vector<void*>{}:std::vector<void*>{nullptr});
 if(m==10){Require(!staticRenderers,"static Renderer called get_rootBone");++staticSkinCalls;return bone;}
 if(m==107){Require(args&&args[0]==(void*)109,"static MeshFilter query used wrong type");return filterObject;}
 if(m==108){Require(self==filterObject,"static getter used Renderer instead of MeshFilter");return newMesh;}
 if(m==110)return &staticEnabled;
 if(m==111){Require(self==uiDictionary,"UI ownership query used another loader dictionary");return uiOwned.count(args[0])?&yes:&no;}
 if(m==112)return uiProxyRoot;
 if(m==113){Require(self==uiCallback&&args&&uiOwned.count(args[0]),"UI completion bypassed game loader ownership");++uiCallbacks;if(uiCallbacks==1)uiRoot=args[0];return nullptr;}
 if(m==114)return FullResourceReload::UILoad(self,args[0],args[1],nullptr);
 if(m==115){FullResourceReload::UIUnload(self,args[0],nullptr);return nullptr;}
 if(m==116){if(!uiDelayed)FullResourceReload::UIComplete(uiClosure,uiProxy,nullptr);return nullptr;}
 if(m==117){static int request;request=FullResourceReload::UIAsync(self,args[0],args[1],args[2],nullptr);return &request;}
 if(m==119){Require(self==uiLuaManager,"DoString was invoked without the game's LuaManager instance");return uiEncryptedOnly&&args[0]!=uiEncryptedCode?nullptr:Array(uiFactory);}
 if(m==122){Require(!self,"LuaManager getter was invoked as an instance method");return uiLuaManager;}
 if(m==123){Require(!self,"Lua cipher getter must be static");return uiLuaKey;}
 if(m==124){Require(!self&&args[1]==uiLuaKey,"UI adapter encoding did not use the live game's cipher key");uiPlainCode=args[0];++uiEncryptions;return uiEncryptedCode;}
 if(m==125){Require(!self&&args[0]==uiEncryptedCode,"UI adapter round trip used different encoded content");return uiEncodingMismatch?StringValue("different script"):uiPlainCode;}
 if(m==126){Require(!self,"UI code equality must be static");return args[0]==args[1]?&yes:&no;}
 if(m==127){Require(self==uiLuaException,"UI exception formatting used a different exception");return StringValue("HyperLua undeclared panel member: go");}
 if(m==120){if(uiLuaRaises){*exception=uiLuaException;return nullptr;}auto values=FullResourceReload::ReferenceArray(args[0]);if(self==uiFactory){Require(values.size()==2&&values[0]==uiLoader&&values[1]==uiRoot,"UI page lookup used another owner");return Array(uiPageAvailable?uiPage:nullptr);}
  Require(self==uiPage&&values.size()==1,"UI page used a different binding");if(!values[0])return Array(&yes);
  Require(uiOwned.count(values[0]),"page binding bypassed native loader completion");if(uiPageReject)return Array(&no);++uiPageBinds;uiRoot=values[0];return Array(&yes);}
 if(m==121){auto index=*(int*)args[1];memcpy((unsigned char*)self+32+index*8,&args[0],8);return nullptr;}
 if(m==118)return &no; // UI page activation can occur after native completion.
 if(m==11)return Array(material);
 if(m==12)return shader;
 if(m==130)return shaderSupported?&yes:&no;
 if(m==131)return &shaderPasses;
 if(m==20){++instanceCalls;return hasInstance?instance:nullptr;}
 if(m==21)return self==unrelatedModel?unrelatedRoot:currentRoot;
 if(m==22){++removeCalls;if(performActive&&performPropAlive){performPropAlive=false;stalePerformProp=true;}currentRoot=nullptr;return nullptr;}
 if(m==155){Require(self==performManager&&args&&args[0]==entity&&!args[1],"perform exit used another entity or squad-switch reason");Require(!performActive||(currentRoot&&animationRoot==currentRoot),"perform exit ran after retiring its model/Animator");++performStops;consumerCalls.push_back(155);performActive=performPropAlive=stalePerformProp=false;return nullptr;}
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
 if(m==41){Require(uiOwned.count(self)||self==uiProxyRoot||self==currentRoot||self==oldRoot||self==newRoot||self==restoredRoot||self==candidate||self==lazyCandidate,"GameObject getter invoked on a Component");return prefabTransform;}
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
 if(m==95)return creatingUI||instanceHandle?instance:assetProxyObject;
 if(m==61)return &no;
 if(m==62){Require(discardCalls>=removeCalls,"display loaded before old root pool discard");++loadCalls;currentRoot=NativeCreate(source);FullResourceReload::PartFinished(mainPart,nullptr);return &yes;}
 if(m==63){Require(args&&args[0]==configBox,"display SetupData did not use preserved value config");FullResourceReload::PartConfigured(self,args[0],nullptr);return nullptr;}
 if(m==64)return self==unrelatedEntity?nullptr:animation;
 if(m==65)return animationData;
 if(m==66)return animator;
 if(m==67)return animationRoot==currentRoot?&yes:&no;
 if(m==68){Require(self==animation&&args&&args[0]==animationData&&*(unsigned char*)args[1]==1,"native animation rebind did not preserve data/resume option");consumerCalls.push_back(68);++animationRebinds;animationRoot=currentRoot;
  if(characterAnimation)performIdleHeld=performDashHeld=false;return &yes;}
 if(m==156||m==157){Require(characterAnimation&&self==animation&&animationRoot==currentRoot&&currentRoot,"performance preload preceded animation setup or used another owner");consumerCalls.push_back(int(m));
  if(m==156){performIdleHeld=true;++performIdlePreloads;}else{performDashHeld=true;++performDashPreloads;}return nullptr;}
 if(m==69)return self==unrelatedEntity?nullptr:weapons;
 if(m==70){Require(self==weapons&&args&&*(unsigned char*)args[0]==1,"weapon unload arguments differ");consumerCalls.push_back(70);++weaponUnloads;return nullptr;}
 if(m==71){Require(animationRoot==currentRoot&&self==weapons&&*(unsigned char*)args[0]==1,"weapons reloaded before animation rebound");consumerCalls.push_back(71);weaponRequestMountRoot=weaponMountRoot;++weaponReloads;return nullptr;}
 if(m==72){consumerCalls.push_back(72);weaponMountRoot=currentRoot;++weaponRemounts;return nullptr;}
 if(m==73)return self==unrelatedEntity?nullptr:typedAudioOnly?instance:audio;
 if(m==96){Require(args&&args[1]&&args[2]&&!*(unsigned char*)args[2],"typed game component query out/warning ABI differs");*(void**)args[1]=self==unrelatedEntity?nullptr:audio;return self==unrelatedEntity?&no:&yes;}
 if(m==74){consumerCalls.push_back(74);++audioInvalidations;return nullptr;}
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
 if(m==84){if(!self){++nullDelegateTargetCalls;throw std::runtime_error("Delegate.get_Target invoked with a null receiver");}return self==uiCompletionDelegate?uiClosure:callbackTarget;}
 if(m==85)return self==model||self==animation?entity:unrelatedEntity;
 if(m==86){Require(self&&args&&args[0]==(void*)81&&*(unsigned char*)args[1]==1,"controls queried scene objects instead of inactive model children");
  auto found=controlRenderers.find(self);return found==controlRenderers.end()?Array(self==newRoot||self==uiResultRoot?newRenderer:oldRenderer):Array(found->second);}
 throw std::runtime_error("unexpected fixture managed call: "+std::to_string(m));
}
void Setup(){
 il2cpp_gchandle_new=Root;il2cpp_gchandle_get_target=Target;il2cpp_gchandle_free=Free;il2cpp_object_get_class=GetClass;
 RuntimeShapes::Ref::weakHandle=Root;
 il2cpp_object_get_virtual_method=[](void*,void* method)->void*{return displayOverride?(void*)97:method;};
 il2cpp_class_get_name=[](void* klass){return klass==characterAnimationClass?"CharacterAnimationComponent":klass==uiBooleanClass?"Boolean":klass==skinClass?"SkinnedMeshRenderer":klass==staticClass?"MeshRenderer":klass==assetProxyClass?"AssetProxy":klass==gameObjectClass?"GameObject":klass==audioClass?"CharacterAudioComponent":klass==modelClass?"BaseModelComponent":klass==delegateClass?"Delegate":"fixture";};
 il2cpp_class_get_namespace=[](void* klass){return klass==skinClass||klass==staticClass||klass==gameObjectClass?"UnityEngine":klass==assetProxyClass?"":klass==modelClass||klass==characterAnimationClass?"Beyond.Gameplay.View":klass==delegateClass||klass==uiBooleanClass?"System":"Beyond.Gameplay.Core";};
 il2cpp_object_unbox=[](void* p){return p;};il2cpp_class_get_fields=Fields;il2cpp_class_get_parent=[](void*)->void*{return nullptr;};
 il2cpp_field_get_name=FieldName;il2cpp_field_get_flags=FieldFlags;il2cpp_field_static_get_value=StaticFieldValue;il2cpp_field_get_type=[](void* p){return p;};
 il2cpp_type_get_name=TypeName;il2cpp_free=[](void*){};il2cpp_field_get_value=FieldValue;il2cpp_runtime_invoke=Invoke;
 il2cpp_class_from_type=[](void*)->void*{return configClass;};il2cpp_class_value_size=[](void* klass,uint32_t* alignment)->int32_t{*alignment=8;return klass==propertyElementClass?propertyElementWidth:64;};il2cpp_value_box=[](void* klass,void* data)->void*{return klass==uiUntrackedClass?uiProxy:klass==handleClass?leaseHandle:*(void**)data==unrelatedPart?unrelatedConfig:configBox;};
 il2cpp_class_get_element_class=[](void* klass)->void*{return klass==propertyArrayClass?propertyElementClass:nullptr;};
 il2cpp_string_new=[](const char* value)->void*{return StringValue(value);};
 il2cpp_array_new=[](void* klass,size_t size)->void*{Require(klass==byteClass||klass==uiObjectClass,"managed array element type differs");auto b=std::make_unique<Bytes>(32+size*(klass==uiObjectClass?8:1));uint64_t count=size;memcpy(b->data()+24,&count,8);auto p=b->data();storage.push_back(std::move(b));return p;};
 RuntimeShapes::api.main=(void*)1;RuntimeShapes::api.alive=(void*)2;RuntimeShapes::api.name=(void*)4;
 RuntimeShapes::api.vertexCount=(void*)6;RuntimeShapes::api.shapeCount=(void*)7;RuntimeShapes::api.bones=(void*)8;
 RuntimeShapes::api.bindPoses=(void*)9;RuntimeShapes::api.rootBone=(void*)10;RuntimeShapes::originalMeshGetter=Mesh;
 RuntimeShapes::api.transform=(void*)40;
 RuntimeShapes::api.modelRenderers=(void*)86;RuntimeShapes::api.rendererType=(void*)81;
 auto& a=FullResourceReload::api;a.instanceId=(void*)3;a.components=(void*)5;a.materials=(void*)11;a.shader=(void*)12;
 a.texture=(void*)75;a.textureIds=(void*)82;a.textureById=(void*)83;
 a.instance=(void*)20;a.getModel=(void*)21;a.remove=(void*)22;a.discard=(void*)23;a.loadModel=(void*)24;a.follow=(void*)25;a.released=(void*)26;
 a.animationCom=(void*)64;a.animationData=(void*)65;a.animationAnimator=(void*)66;a.animationValid=(void*)67;a.animationChange=(void*)68;
 a.animationIdlePreload=(void*)156;a.animationDashPreload=(void*)157;
 a.weaponCom=(void*)69;a.weaponUnload=(void*)70;a.weaponReload=(void*)71;a.weaponMounts=(void*)72;a.componentLookup=(void*)96;a.audioInvalidate=(void*)74;
 a.modelManager=FullResourceReload::BindObjectField(instanceClass,"modelManager","Beyond.Gameplay.View.ModelManager");
 a.effectManager=FullResourceReload::BindObjectField(instanceClass,"effectManager","Beyond.Gameplay.EffectManager");
 a.world={(void*)160,false};a.performManager={(void*)161,false};a.performExit=(void*)155;
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
 a.uiLoad=(void*)114;a.uiUnload=(void*)115;a.uiDictionaryContains=(void*)111;a.uiCallbackInvoke=(void*)113;a.uiUntrackedGet=(void*)112;a.uiAsyncLoad=(void*)117;a.handleUntrackedClass=uiUntrackedClass;
 a.uiActiveHierarchy=(void*)118;a.uiLuaInstance=(void*)122;a.uiLuaDo=(void*)119;a.uiLuaCall=(void*)120;a.uiArraySet=(void*)121;a.uiObjectClass=uiObjectClass;
 a.uiLuaKey=(void*)123;a.uiLuaEncode=(void*)124;a.uiLuaDecode=(void*)125;a.uiLuaCodeEquals=(void*)126;
 a.uiLuaExceptionMessage=(void*)127;
 FullResourceReload::uiLifecycleReady=true;
 FullResourceReload::originalUILoad=[](void*,void*,void*,void*)->void*{
  auto root=FullResourceReload::rebuilding?NativeCreate(source):uiRoot;uiOwned.insert(root);uiRoot=root;
  uiNativeReady=false;
  if(RuntimeShapes::enabled)RuntimeShapes::RegisterRenderer(root==newRoot?newRenderer:oldRenderer,root==newRoot?newMesh:oldMesh,false);return root;
 };
 FullResourceReload::originalUIComplete=[](void*,void*,void*){
  if(uiCancelled){++uiCancelledCompletions;return;}
  uiNativeReady=true;
  uiOwned.insert(uiProxyRoot);if(RuntimeShapes::enabled)RuntimeShapes::RegisterRenderer(uiProxyRoot==newRoot||uiProxyRoot==uiResultRoot?newRenderer:oldRenderer,uiProxyRoot==newRoot||uiProxyRoot==uiResultRoot?newMesh:oldMesh,false);
  if(uiRequestCallback){void* args[]={uiProxyRoot};void* error=nullptr;Invoke((void*)113,uiRequestCallback,args,&error);}
 };
 FullResourceReload::originalUIHandleCallback=[](void*,void*,void*){};
 FullResourceReload::originalUIAsync=[](void*,void*,void*,void* callback,void*)->int{
  uiRequestCallback=callback;creatingUI=true;uiProxyRoot=FullResourceReload::rebuilding?(uiLateInstantiate?nullptr:NativeCreate(source)):uiRoot;creatingUI=false;uiCancelled=false;
  creatingUI=true;FullResourceReload::UIHandleCallback(leaseHandle,uiCompletionDelegate,nullptr);creatingUI=false;
  if(uiInline||(FullResourceReload::rebuilding&&!uiDelayed))FullResourceReload::UIComplete(uiClosure,uiProxy,nullptr);return 7;
 };
 FullResourceReload::originalUIUnload=[](void*,void* root,void*){Require(uiOwned.erase(root)==1,"UI loader released an instance twice");++uiUnloads;};
 FullResourceReload::originalUICancel=[](void*,int,void*){uiCancelled=true;};
 FullResourceReload::originalUIDispose=[](void*,void*){uiOwned.clear();uiCancelled=true;};
 FullResourceReload::originalCloneParent=NativeParent;FullResourceReload::originalInstantiate=NativePosition;FullResourceReload::originalInstantiateParent=NativeParentPosition;
 FullResourceReload::originalInstantiateImmediate=NativeParent;FullResourceReload::originalInstantiateAsync=NativeAsync;
}
std::shared_ptr<FullResourceReload::Prepared> Prepare(){
 auto prepared=std::make_shared<FullResourceReload::Prepared>();prepared->base=2;
 prepared->controls.graph=std::make_shared<const std::vector<Package>>();prepared->config=EiemEntrySettings::Prepare(EiemGlobalConfig{});
 auto plan=std::make_shared<EiemResourceReloadPlan::Plan>();plan->parts.emplace("marker",EiemResourceReloadPlan::Part{"part","marker","material",2,{"Root/Bip001/Bone"},{}});
 plan->materials.emplace("material",EiemResourceReloadPlan::Material{"body",{}});plan->roots.push_back({"assets/model.prefab",0,{{"marker",1}}});
 using namespace EiemRenderAssemblyDescription;
 EiemRenderAssemblyDescription::Source descriptor;descriptor.cab="CAB-fixture";descriptor.id=1;descriptor.renderer={Segment("marker",0),0};
 EiemRenderAssemblyDescription::Target target;target.renderer=descriptor.renderer;target.marker="marker";
 target.bones={Segment("Root",0)+Segment("Bip001",0)+Segment("Bone",0)};target.root=target.bones.front();target.hasRoot=true;
 descriptor.targets.push_back(target);plan->roots.front().assembly.sources.push_back(descriptor);
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
 nullDelegateTargetCalls=0;
 performActive=performPropAlive=stalePerformProp=unrelatedPerformActive=false;performStops=0;
 characterAnimation=false;performIdleHeld=performDashHeld=true;performIdlePreloads=performDashPreloads=0;
 unloadedStreamBundles.clear();
 propertyArrays.clear();propertyIds={314};propertyTextures.clear();propertyElementWidth=4;
 std::atomic_store(&FullResourceReload::routes,std::shared_ptr<const FullResourceReload::Routes>(std::make_shared<FullResourceReload::Routes>()));
 currentRoot=oldRoot;removeCalls=discardCalls=loadCalls=cloneCalls=followCalls=disposeCalls=unloadCalls=0;rejectVertex=false;sourceAlive=true;throwDispose=false;done=0;generation=2;
 animationRoot=oldRoot;animationRebinds=weaponUnloads=weaponReloads=weaponRemounts=audioInvalidations=0;ignoreCandidate=false;recordPartFinished=false;completePartLoading=true;callbackTarget=model;
 weaponMountRoot=oldRoot;weaponRequestMountRoot=nullptr;consumerCalls.clear();
 identitySameMesh=false;textureValue=oldTexture;
 scheduleDone.clear();scheduleBundles.clear();opened=leaseRequests=sceneQueries=0;
 hasHelper=hasEntity=hasModel=hasPart=helperMatches=rendererInside=true;released=false;modelPath="assets/model.prefab";
 RuntimeShapes::bound.clear();RuntimeShapes::boundCount=0;
 FullResourceReload::sourceLeases.clear();FullResourceReload::liveParts.clear();
 FullResourceReload::uiPageFactory={};uiPageAvailable=true;uiPageReject=false;uiPageBinds=0;uiRequestCallback=nullptr;FullResourceReload::liveUIModels.clear();FullResourceReload::pendingUIModels.clear();FullResourceReload::uiReloadRequests.clear();FullResourceReload::uiProxyRequests.clear();uiOwned.clear();uiRoot=uiProxyRoot=oldRoot;uiResultRoot=nullptr;creatingUI=uiLateInstantiate=false;uiCallbacks=uiUnloads=uiCancelledCompletions=0;uiInline=false;uiNativeReady=false;uiDelayed=uiCancelled=false;
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
void PerformLifecycleRegression(){
 using namespace FullResourceReload;Reset();
 // The native standby system owns a temporary object parented to the current
 // model. Destroying that model cannot clear the game's separate handle cache.
 {auto a=MakeAttempt();auto& owner=a.owners.front();CaptureConsumers(owner);performActive=performPropAlive=true;
  RemoveOwner(owner);Require(stalePerformProp&&!performPropAlive,"fixture did not reproduce a dangling standby prop after model removal");}
 Reset();performActive=performPropAlive=unrelatedPerformActive=true;
 {auto a=MakeAttempt();CaptureConsumers(a.owners.front());Rebuild(a);
  Require(a.phase==Phase::Passed&&performStops==1&&!performActive&&!stalePerformProp,"reload retired a model without ending its game-owned standby performance");
  Require(unrelatedPerformActive,"reload stopped another character's performance");
  Require(!consumerCalls.empty()&&consumerCalls.front()==155,"perform cleanup followed weapon/model/animation retirement");
  // The next idle performance starts with new objects on the new model.
  performActive=performPropAlive=true;Require(currentRoot==newRoot&&!stalePerformProp,"next standby performance reused the retired prop");}
 Reset();{auto a=MakeAttempt();auto& owner=a.owners.front();owner.viewPart=true;CaptureConsumers(owner);ReleaseConsumers(owner);Require(!performStops,"display-only part stopped a world performance");}
 Reset();
}
void PerformPreloadRegression(){
 using namespace FullResourceReload;Reset();characterAnimation=true;
 // Native CharacterAnimationComponent.ReleaseInternal clears born/custom
 // perform preloads even for ChangeAnimationConfig(..., resume=true).
 // A temporary object inherited by the next perform can remain visible after
 // its outgoing loader exits; without the born preload, the queued Bundle
 // unload destroys its Controller/Mesh later. Reproduce that delayed boundary.
 auto delayedRelease=[&]{performPropAlive=performIdleHeld&&performDashHeld;};
 {auto a=MakeAttempt();CaptureConsumers(a.owners.front());Rebuild(a);performPropAlive=true;delayedRelease();
  Require(a.phase==Phase::Passed&&performPropAlive,"animation rebind left native performance preloads cleared before delayed Bundle unload");
  Require(performIdlePreloads==1&&performDashPreloads==1,"reload omitted or repeated configured performance preloads");
  Require(consumerCalls==std::vector<int>{155,70,68,156,157,74,72,71},"performance preload did not follow new animation setup before async consumers");
  Require(!leaseRequests,"performance restore issued a plugin ResourceManager source request");}
 Reset();characterAnimation=true;
 {auto a=MakeAttempt();CaptureConsumers(a.owners.front());ignoreCandidate=true;bool rejected=false;try{Rebuild(a);}catch(const std::exception&){rejected=true;}
  delayedRelease();Require(rejected&&currentRoot==restoredRoot&&performPropAlive&&performIdlePreloads==1&&performDashPreloads==1,"rollback restored the old model but left its native performance resources unheld");}
 Reset();characterAnimation=true;
 {auto a=MakeAttempt();auto& owner=a.owners.front();owner.viewPart=true;CaptureConsumers(owner);ReloadConsumers(owner,currentRoot);
  Require(!performIdlePreloads&&!performDashPreloads&&!animationRebinds,"display-only owner preloaded world performances");}
 Reset();{auto a=MakeAttempt();CaptureConsumers(a.owners.front());Rebuild(a);Require(!performIdlePreloads&&!performDashPreloads,"non-character animation called CharacterAnimationComponent preload functions");}
 Reset();
}
void AnimationAttachmentProbeRegression(){
 using namespace FullResourceReload;Reset();
 static bool failRead=false;static unsigned parentReads=0;
 static decltype(il2cpp_field_get_value) fallbackFields=nullptr;
 auto previous=std::make_tuple(animationLinkProbe,il2cpp_runtime_invoke,il2cpp_field_get_value);fallbackFields=il2cpp_field_get_value;
 auto restore=[&]{animationLinkProbe=std::get<0>(previous);il2cpp_runtime_invoke=std::get<1>(previous);il2cpp_field_get_value=std::get<2>(previous);Reset();};
 animationLinkProbe.monoField=(void*)250;animationLinkProbe.objectField=(void*)251;animationLinkProbe.ownerField=(void*)252;animationLinkProbe.animationField=(void*)253;
 animationLinkProbe.parentMethod=(void*)254;animationLinkProbe.objectType=Ref::Make((void*)0x6000,false);animationLinkProbe.ready=true;
 il2cpp_field_get_value=[](void* self,void* field,void* out){
  if(field==(void*)250){if(failRead)throw std::runtime_error("fixture animation link read failure");*(void**)out=animationRoot==oldRoot?(void*)0x6010:(void*)0x6011;return;}
  if(field==(void*)251){*(void**)out=self==(void*)0x6010?(void*)0x6020:nullptr;return;}
  if(field==(void*)252){Require(self==(void*)0x6020,"probe read entity on the wrong owner");*(void**)out=entity;return;}
  if(field==(void*)253){*(void**)out=animation;return;}
  fallbackFields(self,field,out);
 };
 il2cpp_runtime_invoke=[](void* method,void* self,void** args,void** exception)->void*{
  if(method==(void*)254){*exception=nullptr;Require((self==(void*)0x6010||self==(void*)0x6011)&&args&&args[0]==(void*)0x6000,"probe parent query ABI changed");++parentReads;return (void*)0x6020;}
  return Invoke(method,self,args,exception);
 };
 try{
  failRead=false;parentReads=0;
  auto before=ReadAnimationLinks(animation);Require(before.entity.Get()==entity&&before.parentEntity.Get()==entity&&before.animation.Get()==animation,"probe missed the initial animation owner");
  auto a=MakeAttempt();CaptureConsumers(a.owners.front());Rebuild(a);
  auto after=ReadAnimationLinks(animation);Require(!after.object.Get()&&!after.entity.Get()&&after.parentEntity.Get()==entity&&after.animation.Get()==animation,"probe concealed a missing cached owner after model replacement");
  Require(a.phase==Phase::Passed&&parentReads>=4&&animationRebinds==1,"read-only link probe altered native reload or skipped its boundaries");
  Reset();parentReads=0;failRead=true;
  {auto a=MakeAttempt();CaptureConsumers(a.owners.front());Rebuild(a);Require(a.phase==Phase::Passed&&animationRebinds==1&&parentReads==0,"diagnostic read failure interrupted the resource transaction");}
  failRead=false;restore();
 }catch(...){failRead=false;restore();throw;}
}
void UnmodifiedSkinProbeRegression(){
 using namespace FullResourceReload;Reset();
 static Matrix4x4 matrix{},inverse{};static SkinState::Bounds bounds{};
 static bool badLinks=false,failMatrix=false;
 auto previous=std::make_tuple(RuntimeShapes::api,api,skinStateProbe,il2cpp_runtime_invoke);
 auto restore=[&]{RuntimeShapes::api=std::get<0>(previous);api=std::get<1>(previous);skinStateProbe=std::get<2>(previous);il2cpp_runtime_invoke=std::get<3>(previous);Reset();};
 for(int i=0;i<16;++i)matrix.m[i]=inverse.m[i]=(i%5==0?1.f:0.f);matrix.m[12]=2.f;matrix.m[13]=3.f;matrix.m[14]=4.f;
 RuntimeShapes::api.getEnabled=(void*)260;skinStateProbe.active=(void*)261;skinStateProbe.forceOff=(void*)262;skinStateProbe.bounds=(void*)263;
 RuntimeShapes::api.bones=(void*)264;RuntimeShapes::api.rootBone=(void*)265;RuntimeShapes::api.skinningRoot=(void*)265;
 RuntimeShapes::api.localToWorld=(void*)266;RuntimeShapes::api.worldToLocal=(void*)267;api.isChild=(void*)268;skinStateProbe.ready=true;
 il2cpp_runtime_invoke=[](void* method,void* self,void** args,void** exception)->void*{
  *exception=nullptr;auto m=uintptr_t(method);
  if(m==260||m==261)return &yes;if(m==262)return &no;if(m==263)return &bounds;
  if(m==264){auto array=Array(badLinks?std::vector<void*>{bone,nullptr,leftBone}:std::vector<void*>{bone});propertyArrays.insert(array);return array;}
  if(m==265)return bone;if(m==266){if(failMatrix)throw std::runtime_error("fixture skin matrix read failure");return &matrix;}if(m==267)return &inverse;
  if(m==268){Require(args&&args[0]==prefabTransform,"skin probe queried outside the exact model root");return self==leftBone?&no:&yes;}
  return Invoke(method,self,args,exception);
 };
 try{
  badLinks=failMatrix=false;auto initial=ReadSkinState(oldRenderer,prefabTransform);
  Require(initial.slots==1&&!initial.missing&&!initial.outside&&initial.rootInside==1&&initial.skinInside==1&&initial.enabled&&initial.active&&!initial.forceOff,"skin probe failed the valid native palette");
  Require(initial.detail.find("2,3,4")!=std::string::npos,"skin probe lost root-relative bone position");
  badLinks=true;auto stale=ReadSkinState(oldRenderer,prefabTransform);
  Require(stale.slots==3&&stale.missing==1&&stale.outside==1,"skin probe concealed a missing or foreign bone in an untouched Mesh");
  failMatrix=true;TraceSkinStates(oldRoot,"assets/model.prefab",{RuntimeShapes::Ref::Make(oldRenderer,true)},"fixture",0);
  // Diagnostic faults remain isolated from the resource transaction.
  failMatrix=false;
  restore();
 }catch(...){restore();throw;}
}
void PerformanceProbeRegression(){
 using namespace FullResourceReload;Reset();
 struct Row {MInfo info{};void* owner;const char* name;const char* result;};
 static Row rows[]={{{},(void*)0xDD10,"get_Values","Fixture.Values"},{{},(void*)0xDD11,"get_Count","System.Int32"},
  {{},(void*)0xDD11,"GetEnumerator","Fixture.Enumerator"},{{},(void*)0xDD12,"MoveNext","System.Boolean"},
  {{},(void*)0xDD12,"get_Current","Fixture.Prop"},{{},(void*)0xDD12,"Dispose","System.Void"}};
 for(auto& row:rows)row.info.mp=(void*)1;
 static int cursor=0,count=2;static unsigned disposals=0;static bool throwNext=false;
 static Bytes requestBytes(24);static void* array=nullptr;
 auto previous=std::make_tuple(performProbe,il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,il2cpp_method_get_flags,
  il2cpp_method_get_return_type,il2cpp_type_get_name,il2cpp_class_get_type,il2cpp_type_get_type,il2cpp_object_get_class,
  il2cpp_class_get_name,il2cpp_class_get_namespace,il2cpp_class_get_parent,il2cpp_object_unbox,il2cpp_runtime_invoke,il2cpp_field_get_value,il2cpp_value_box);
 auto restore=[&]{std::tie(performProbe,il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,il2cpp_method_get_flags,
  il2cpp_method_get_return_type,il2cpp_type_get_name,il2cpp_class_get_type,il2cpp_type_get_type,il2cpp_object_get_class,
  il2cpp_class_get_name,il2cpp_class_get_namespace,il2cpp_class_get_parent,il2cpp_object_unbox,il2cpp_runtime_invoke,il2cpp_field_get_value,il2cpp_value_box)=previous;Reset();};
 il2cpp_class_get_methods=[](void* klass,void** it)->void*{for(auto i=uintptr_t(*it);i<std::size(rows);++i){*it=(void*)(i+1);if(rows[i].owner==klass)return &rows[i];}return nullptr;};
 il2cpp_method_get_name=[](void* method){return static_cast<Row*>(method)->name;};il2cpp_method_get_param_count=[](void*)->uint32_t{return 0;};
 il2cpp_method_get_flags=[](void*,uint32_t*)->uint32_t{return 0x86;};il2cpp_method_get_return_type=[](void* method)->void*{return const_cast<char*>(static_cast<Row*>(method)->result);};
 il2cpp_type_get_name=[](void* type)->const char*{return static_cast<const char*>(type);};il2cpp_class_get_type=[](void* klass){return klass;};
 il2cpp_type_get_type=[](void* type)->int{return type==(void*)0xDD13?0x1d:0x12;};
 il2cpp_object_get_class=[](void* object)->void*{return object==array?(void*)0xDD13:object==(void*)0xDD20?(void*)0xDD10:object==(void*)0xDD21?(void*)0xDD11:object==(void*)0xDD22?(void*)0xDD12:object==animation?(void*)0xDD14:(void*)0xDD15;};
 il2cpp_class_get_parent=[](void* klass)->void*{return klass==(void*)0xDD12?(void*)0xDD16:nullptr;};
 il2cpp_class_get_name=[](void* klass){return klass==(void*)0xDD16?"ValueType":klass==(void*)0xDD14?"CharacterAnimationComponent":"CharPerformHandleBase";};
 il2cpp_class_get_namespace=[](void* klass){return klass==(void*)0xDD16?"System":klass==(void*)0xDD14?"Beyond.Gameplay.View":"Beyond.Gameplay.Core.CharInteractPerform";};
 il2cpp_object_unbox=[](void* object)->void*{return object==(void*)0xDD22?(void*)0xDD23:object;};
 il2cpp_runtime_invoke=[](void* method,void* self,void** args,void** exception)->void*{
  *exception=nullptr;if(method==(void*)0xDD30)return self==(void*)0xDD40?entity:unrelatedEntity;
  if(method==&rows[0]){Require(self==(void*)0xDD20,"probe used a different dictionary");return (void*)0xDD21;}
  if(method==&rows[1])return &count;if(method==&rows[2]){cursor=0;return (void*)0xDD22;}
  Require(self==(void*)0xDD23,"probe did not unbox its value-type enumerator");
  if(method==&rows[3]){if(throwNext)throw std::runtime_error("fixture enum failure");return ++cursor<=2?&yes:&no;}
  if(method==&rows[4])return cursor==1?(void*)0xDD40:(void*)0xDD41;
  if(method==&rows[5]){++disposals;return nullptr;}throw std::runtime_error("unexpected performance probe method");
 };
 il2cpp_field_get_value=[](void* self,void* field,void* out){
  if(field==(void*)0xDD50){Require(self==animation,"request read from another animation component");memcpy(out,requestBytes.data(),requestBytes.size());}
  else if(field==(void*)0xDD51){Require(self==requestBytes.data(),"handle read without boxing the actual request extent");memcpy(out,requestBytes.data(),sizeof(void*));}
  else if(field==(void*)0xDD52){*(void**)out=(void*)0xDD41;}else if(field==(void*)0xDD53){memcpy(out,requestBytes.data()+16,4);}else throw std::runtime_error("unexpected probe field");
 };
 il2cpp_value_box=[](void* klass,void* bytes)->void*{Require(klass==(void*)0xDD17&&!memcmp(bytes,requestBytes.data(),requestBytes.size()),"request boxing changed the value bytes");return requestBytes.data();};
 try{
  array=Array(std::vector<void*>{(void*)0xDD40,(void*)0xDD41});auto refs=ProbeReferences(array);Require(refs.size()==2,"probe lost reference-array elements");
  count=2;disposals=0;throwNext=false;refs=ProbeReferences((void*)0xDD20);Require(refs.size()==2&&refs[1].Get()==(void*)0xDD41&&disposals==1,"probe failed actual dictionary-value enumeration");
  count=1;bool rejected=false;try{ProbeReferences((void*)0xDD20);}catch(const std::exception&){rejected=true;}Require(rejected&&disposals==2,"probe trusted enumeration beyond its real collection count");
  count=2;throwNext=true;rejected=false;try{ProbeReferences((void*)0xDD20);}catch(const std::exception&){rejected=true;}Require(rejected&&disposals==3,"diagnostic enumeration failure leaked its enumerator");throwNext=false;
  performProbe.requestField=(void*)0xDD50;performProbe.handleField=(void*)0xDD51;performProbe.requestClass=(void*)0xDD17;performProbe.requestSize=int(requestBytes.size());
  performProbe.mainHandleField=(void*)0xDD52;performProbe.handleEntity=(void*)0xDD30;performProbe.requestSourceField=(void*)0xDD53;void* handle=(void*)0xDD40;memcpy(requestBytes.data(),&handle,sizeof(handle));uint32_t sourceKind=2;memcpy(requestBytes.data()+16,&sourceKind,sizeof(sourceKind));
  auto request=ProbeCurrentRequest(animation);Require(request.handle.Get()==handle&&request.source==sourceKind,"probe lost current performance source or handle");
  Owner owner;owner.animation=Ref::Make(animation,false);owner.entity=Ref::Make(entity,false);owner.performManager=Ref::Make(performManager,false);
  auto selected=ProbePerformHandles(owner);Require(selected.size()==1&&selected.front().Get()==handle,"probe included another entity's main performance");
  performProbe.ready=true;performProbe.watches[model].started=GetTickCount64()-180001;TracePerformanceFrame();Require(performProbe.watches.empty(),"completed performance observation retained references");
  restore();
 }catch(...){restore();throw;}
}
void PerformanceReleaseProbeRegression(){
 using namespace FullResourceReload;Reset();static unsigned calls=0;static void* target=nullptr;static bool destroy=false;
 auto previous=std::make_tuple(performProbe,originalProbeUnload,originalProbeUnloadAsync,originalProbeUnloadAsset,originalProbeUnloadAssetAsync,probePendingReleases,il2cpp_runtime_invoke);
 auto restore=[&]{std::tie(performProbe,originalProbeUnload,originalProbeUnloadAsync,originalProbeUnloadAsset,originalProbeUnloadAssetAsync,probePendingReleases,il2cpp_runtime_invoke)=previous;Reset();};
 originalProbeUnload=[](void* self,bool all){++calls;target=self;destroy=all;};
 originalProbeUnloadAsync=[](void* self,bool all)->void*{++calls;target=self;destroy=all;return assetRequest;};
 originalProbeUnloadAsset=[](void* self,void* asset){++calls;target=self;Require(asset==oldMesh,"release probe changed asset argument");};
 originalProbeUnloadAssetAsync=[](void* self,void* asset)->void*{++calls;target=self;Require(asset==oldMesh,"release probe changed async asset argument");return assetRequest;};
 try{
  performProbe.ready=true;auto& watch=performProbe.watches[model];ProbeRememberResource(watch,oldMesh,"Attachment/Fixture");
  ProbeRememberResource(watch,oldMesh,"Attachment/Fixture");Require(watch.resources.size()==1,"probe duplicated the same attachment resource");
  {ProbeResourceRelease observation("fixture",bundle,true);Require(observation.before.size()==1&&observation.before[0].id==int(uintptr_t(oldMesh)),"probe failed to select a live native attachment resource");}
  {ProbeResourceRelease observation("fixture",bundle,false,newMesh);Require(observation.before.empty(),"exact asset release probe selected an unrelated resource");}
  calls=0;ProbeUnload(bundle,false);Require(calls==1&&target==bundle&&!destroy,"probe changed Unload(false) or called it twice");
  probePendingReleases.clear();auto result=ProbeUnloadAsync(bundle,true);Require(calls==2&&result==assetRequest&&destroy,"probe changed asynchronous unload arguments/result");
  ProbeUnloadAsset(bundle,oldMesh);Require(calls==3&&target==bundle,"probe changed asset unload receiver/call count");
  result=ProbeUnloadAssetAsync(bundle,oldMesh);Require(calls==4&&result==assetRequest&&probePendingReleases.size()==2,"probe lost actual asynchronous resource requests");
  done=0;TracePerformanceReleases();Require(probePendingReleases.size()==2&&calls==4,"completion observation dropped pending requests or initiated another unload");
  done=1;TracePerformanceReleases();Require(probePendingReleases.empty()&&calls==4,"completion observation initiated another unload or retained completed requests");
  il2cpp_runtime_invoke=[](void*,void*,void**,void**)->void*{throw std::runtime_error("fixture diagnostic read failure");};
  ProbeUnload(bundle,true);Require(calls==5&&destroy,"diagnostic read failure prevented native unload");
  ProbeUnloadAsset(bundle,oldMesh);Require(calls==6,"diagnostic read failure prevented native asset release");
  restore();
 }catch(...){restore();throw;}
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
void UIEncryptedLoadRegression(){
 using namespace FullResourceReload;Reset();uiEncryptedOnly=true;uiEncryptions=0;uiPlainCode=nullptr;
 // The actual game LuaDLL loadbuffer decrypts every long script. A plaintext
 // DoString produces a decryption error and a null result before any mutation.
 auto code=Ref::Make(il2cpp_string_new(UIPageFactorySource),false);void* args[]={code.Get()};
 Require(!Call(api.uiLuaDo,uiLuaManager,args),"fixture did not reproduce plaintext decryption failure");
 try{
  auto page=CaptureUIPage(uiLoader,oldRoot);Require(page.Get()==uiPage&&uiEncryptions==1,"UI page factory was not encoded through the game's cipher");
  Require(CaptureUIPage(uiLoader,oldRoot).Get()==uiPage&&uiEncryptions==1,"cached page factory was needlessly reencoded");
  uiPageFactory={};uiEncodingMismatch=true;bool refused=false;try{CaptureUIPage(uiLoader,oldRoot);}catch(const std::exception&){refused=true;}
  Require(refused&&!uiPageFactory.Get()&&uiRoot==oldRoot&&uiPageBinds==0,"mismatched encoded adapter was executed or changed the page");
 }catch(...){uiEncryptedOnly=uiEncodingMismatch=false;Reset();throw;}
 uiEncryptedOnly=uiEncodingMismatch=false;Reset();
}
void UIPageApiBindingRegression(){
 // Real LuaManager metadata exposes static get_instance, but DoString is an
 // instance method (0x1e6). A direct fixture API handle missed this contract.
 void *managerClass=(void*)0x2060,*functionClass=(void*)0x2061,*arrayClass=(void*)0x2062,*cipherClass=(void*)0x2063,*cryptoClass=(void*)0x2064,*stringClass=(void*)0x2065,*exceptionClass=(void*)0x2066;
 struct MethodRow {MInfo info{};void* owner;const char* name;const char* result;std::vector<const char*> parameters;uint32_t flags;};
 static MethodRow rows[]={{{},managerClass,"get_instance","Beyond.Lua.LuaManager",{},0x896},
  {{},managerClass,"DoString","System.Object[]",{"System.String"},0x1e6},
  {{},functionClass,"Call","System.Object[]",{"System.Object[]"},0x86},
  {{},arrayClass,"SetValue","System.Void",{"System.Object","System.Int32"},0x86},
  {{},cipherClass,"GetKey","System.String",{},0x91},
  {{},cryptoClass,"EncryptToBase64String","System.String",{"System.String","System.String"},0x96},
  {{},cipherClass,"DecryptLuaString","System.String",{"System.String"},0x96},
  {{},stringClass,"Equals","System.Boolean",{"System.String","System.String"},0x96},
  {{},exceptionClass,"get_Message","System.String",{},0x1c6}};
 for(auto& row:rows)row.info.mp=(void*)1;
 auto previous=std::make_tuple(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_type);
 auto restore=[&]{std::tie(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,il2cpp_class_get_type)=previous;};
 il2cpp_class_get_methods=[](void* klass,void** it)->void*{for(auto index=uintptr_t(*it);index<std::size(rows);++index){*it=(void*)(index+1);if(rows[index].owner==klass)return &rows[index];}return nullptr;};
 il2cpp_method_get_name=[](void* method)->const char*{return static_cast<MethodRow*>(method)->name;};
 il2cpp_method_get_param_count=[](void* method)->uint32_t{return uint32_t(static_cast<MethodRow*>(method)->parameters.size());};
 il2cpp_method_get_flags=[](void* method,uint32_t*)->uint32_t{return static_cast<MethodRow*>(method)->flags;};
 il2cpp_method_get_return_type=[](void* method)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->result);};
 il2cpp_method_get_param=[](void* method,uint32_t index)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->parameters.at(index));};
 il2cpp_type_get_name=[](void* type)->const char*{return static_cast<const char*>(type);};
 il2cpp_class_get_type=[](void*)->void*{return const_cast<char*>("Beyond.Lua.LuaManager");};
 try{
  Require(!Exact(managerClass,"DoString",{"System.String"},"System.Object[]",true),"fixture accepted an invented static DoString");
  FullResourceReload::BindUIPageApi(managerClass,functionClass,arrayClass,uiObjectClass,exceptionClass);
  Require(FullResourceReload::api.uiLuaInstance==&rows[0]&&FullResourceReload::api.uiLuaDo==&rows[1]&&FullResourceReload::api.uiLuaCall==&rows[2]&&FullResourceReload::api.uiArraySet==&rows[3]&&FullResourceReload::api.uiLuaExceptionMessage==&rows[8],"UI page binding did not use the actual manager instance and signatures");
  FullResourceReload::BindUIPageCodec(cipherClass,cryptoClass,stringClass);
  Require(FullResourceReload::api.uiLuaKey==&rows[4]&&FullResourceReload::api.uiLuaEncode==&rows[5]&&FullResourceReload::api.uiLuaDecode==&rows[6]&&FullResourceReload::api.uiLuaCodeEquals==&rows[7],"UI encoding bound a different live-game codec layout");
 }catch(...){restore();throw;}
 restore();Setup();
}
void UILuaExceptionRegression(){
 using namespace FullResourceReload;Reset();uiLuaRaises=true;bool refused=false;
 try{CaptureUIPage(uiLoader,oldRoot);}catch(const std::exception& e){refused=true;Require(std::string(e.what()).find("HyperLua undeclared panel member: go")!=std::string::npos,"UI Lua exception lost the game's actual error message");}
 uiLuaRaises=false;Require(refused&&uiRoot==oldRoot&&!uiPageBinds&&!uiUnloads,"UI Lua error changed the displayed character");Reset();
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
 using namespace FullResourceReload;Reset();characterAnimation=true;BindWorld();RecordPart();recordPartFinished=true;sceneRenderers={oldRenderer};
 auto originalNewRoot=newRoot;
 struct Cleanup {void* root;~Cleanup(){newRoot=root;Reset();}}cleanup{originalNewRoot};
 for(unsigned repeat=0;repeat<8;++repeat){
  performActive=performPropAlive=true;unrelatedPerformActive=true;
  auto a=MakeAttempt();a.owners.clear();a.prepared->base=generation.load();DiscoverOwners(a);Require(!sceneQueries,"world reload queried scene Renderers");
  Require(a.owners.size()==1,"consecutive F10 lost the live model part");
  if(repeat)newRoot=(void*)(uintptr_t(0x1005)+repeat);
  Rebuild(a);
  Require(a.phase==Phase::Passed&&generation==3+repeat,"consecutive F10 did not commit");
  Require(performStops==repeat+1&&!performActive&&!stalePerformProp&&unrelatedPerformActive,"consecutive F10 retained a retired standby prop or stopped another entity");
  Require(performIdleHeld&&performDashHeld&&performIdlePreloads==repeat+1&&performDashPreloads==repeat+1,"consecutive F10 left native performance preloads cleared or accumulated duplicate preload calls");
  Require(animationRoot==currentRoot&&animationRebinds==repeat+1,
          "consecutive F10 committed a world model as a display part and left animation bound to the old root (T pose)");
  Require(!a.owners.front().viewPart&&followCalls==repeat+1&&weaponReloads==repeat+1&&weaponRemounts==repeat+1&&weaponRequestMountRoot==currentRoot,
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
void UIControlCompletionRegression(){
 Reset();RuntimeShapes::enabled=true;
 RuntimeShapes::packageGeneration=RuntimeShapes::checkedGeneration=generation.load();
 auto variables=RuntimeShapes::variables;auto setter=RuntimeShapes::originalEnabledSetter;
 struct Cleanup {decltype(variables) values;decltype(setter) original;
  ~Cleanup(){RuntimeShapes::enabled=false;RuntimeShapes::variables=std::move(values);RuntimeShapes::originalEnabledSetter=original;Reset();}
 } cleanup{variables,setter};
 RuntimeShapes::api.getEnabled=(void*)110;staticEnabled=1;
 RuntimeShapes::originalEnabledSetter=[](void*,bool value){staticEnabled=value;};
 auto& spec=RuntimeControls::specs.at("marker");spec.package="UI-test";spec.part="sleeve";spec.vertices=2;
 spec.visible=EiemExpression::Make();spec.visible->op=EiemExpression::Variable;spec.visible->variable="shown";
 RuntimeShapes::variables[spec.package]["shown"]=0;
 // Initialization leaves enabled state unfiltered; the UI loader's completion
 // activates it without BaseModelViewPart or any scene Renderer query.
 RuntimeShapes::RegisterRenderer(oldRenderer,oldMesh,false);
 Require(staticEnabled&&!RuntimeShapes::bound.at(oldRenderer).modelReady,"UI initialization filtered native enabled state too early");
 auto root=FullResourceReload::UILoad(uiLoader,StringValue(modelPath),factoryParent,nullptr);
 Require(root==oldRoot,"UI sync loader changed the native return value");
 RuntimeShapes::ApplyVisibility(oldRenderer,RuntimeShapes::bound.at(oldRenderer),spec);
 Require(!staticEnabled,"completed UI model keeps every Mesh enabled and ignores author visibility");
 RuntimeShapes::variables[spec.package]["shown"]=1;
 RuntimeShapes::ApplyVisibility(oldRenderer,RuntimeShapes::bound.at(oldRenderer),spec);
 Require(staticEnabled,"UI key state did not restore the visible Mesh");
 if(!FullResourceReload::api.uiLuaInstance){
  bool refused=false;try{FullResourceReload::CaptureUIPage(uiLoader,oldRoot);}catch(const std::exception&){refused=true;}
  Require(refused&&FullResourceReload::liveUIModels.count(oldRoot)&&RuntimeShapes::bound.at(oldRenderer).modelReady&&staticEnabled,"missing page transfer API disabled UI key controls or allowed an unsafe F10");
 }
 RuntimeShapes::SetEnabled(oldRenderer,false);
 RuntimeShapes::variables[spec.package]["shown"]=0;RuntimeShapes::ApplyVisibility(oldRenderer,RuntimeShapes::bound.at(oldRenderer),spec);
 RuntimeShapes::variables[spec.package]["shown"]=1;RuntimeShapes::ApplyVisibility(oldRenderer,RuntimeShapes::bound.at(oldRenderer),spec);
 Require(!staticEnabled,"UI author visibility enabled a game-disabled Renderer");
 FullResourceReload::UIUnload(uiLoader,oldRoot,nullptr);
 Require(RuntimeShapes::bound.empty()&&FullResourceReload::liveUIModels.empty(),"UI unload retained instance control bindings");
 for(bool inlineCompletion:{false,true}){
  staticEnabled=1;uiRoot=oldRoot;uiInline=inlineCompletion;RuntimeShapes::variables[spec.package]["shown"]=0;
  FullResourceReload::UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);
  if(!inlineCompletion){Require(FullResourceReload::pendingUIModels.size()==1,"asynchronous UI request was not tracked");FullResourceReload::UIComplete(uiClosure,uiProxy,nullptr);}
  Require(!staticEnabled&&RuntimeShapes::bound.at(oldRenderer).modelReady&&FullResourceReload::pendingUIModels.empty(),"UI async completion did not activate author visibility or retire its pending request");
  Require(FullResourceReload::liveUIModels.at(oldRoot).callback.Get()==uiCallback,"UI completion lost the game's panel callback");
  FullResourceReload::UIUnload(uiLoader,oldRoot,nullptr);
 }
 uiInline=false;FullResourceReload::UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);
 FullResourceReload::UICancel(uiLoader,7,nullptr);Require(FullResourceReload::pendingUIModels.empty(),"cancelled UI request blocks F10");
 Require(!sceneQueries,"UI completion searched the scene for Renderers");
}
void UIControlWithoutPageApiRegression(){
 auto previous=FullResourceReload::api.uiLuaInstance;FullResourceReload::api.uiLuaInstance=nullptr;
 try{UIControlCompletionRegression();}catch(...){FullResourceReload::api.uiLuaInstance=previous;throw;}
 FullResourceReload::api.uiLuaInstance=previous;
}
void UIDelayedCompletionRegression(){
 using namespace FullResourceReload;
 for(int outcome:{0,1,2,3}){
  Reset();UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);UIComplete(uiClosure,uiProxy,nullptr);
  currentRoot=nullptr;auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);uiDelayed=true;
  Rebuild(a);
  Require(a.phase==Phase::UIWait&&generation==2&&uiRoot==oldRoot&&uiOwned.count(oldRoot)&&uiCallbacks==1&&!uiUnloads,
   "delayed UI completion removed the displayed model or rejected before the game could finish next frame");
  auto previous=std::atomic_load(&routes);Rebuild(a);
  Require(a.phase==Phase::UIWait&&uiRoot==oldRoot&&!removeCalls&&!loadCalls&&previous->empty(),"waiting UI job exposed routes, blocked the frame, or released old models");
  if(outcome==2){UICancel(uiLoader,7,nullptr);bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
   Require(refused&&generation==2&&uiRoot==oldRoot&&uiOwned.count(oldRoot)&&uiCallbacks==1,"cancelled request removed the displayed UI");
   UIComplete(uiClosure,uiProxy,nullptr);Require(uiCallbacks==1&&uiCancelledCompletions==1&&uiReloadRequests.empty(),"cancelled late completion reached the page or leaked request state");continue;
  }
  if(outcome==3){UIDispose(uiLoader,nullptr);bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
   Require(refused&&generation==2&&uiCallbacks==1&&!uiReloadRequests.empty(),"disposed UI page was revived by a pending F10");
   UIComplete(uiClosure,uiProxy,nullptr);Require(uiCallbacks==1&&uiReloadRequests.empty(),"disposed UI completion revived the page or retained its request");continue;
  }
  UIComplete(uiClosure,uiProxy,nullptr);
  Require(uiRoot==oldRoot&&uiCallbacks==1&&uiOwned.count(oldRoot),"native completion updated the page before candidate validation");
  rejectVertex=outcome==1;bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
  Require(refused==(outcome==1),"UI candidate validation outcome differs");
  if(outcome==1)Require(generation==2&&uiRoot==oldRoot&&uiOwned.count(oldRoot)&&uiCallbacks==1&&!uiUnloads&&uiCancelledCompletions==1,"invalid asynchronous UI candidate blanked the original display");
  else Require(a.phase==Phase::Passed&&generation==3&&uiRoot==newRoot&&uiCallbacks==1&&uiPageBinds==1&&uiUnloads==1&&uiOwned.count(newRoot)&&!uiOwned.count(oldRoot)&&uiReloadRequests.empty(),"completed UI F10 did not transfer page ownership exactly once");
 }
 Reset();
}
void UILateProxyRoutingRegression(){
 using namespace FullResourceReload;
 Reset();UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);UIComplete(uiClosure,uiProxy,nullptr);
 currentRoot=nullptr;auto a=MakeAttempt();a.owners.clear();DiscoverOwners(a);uiDelayed=uiLateInstantiate=true;
 Rebuild(a);Require(a.phase==Phase::UIWait&&std::atomic_load(&routes)->empty()&&uiRoot==oldRoot,"UI submission changed public routes or display before instantiation");
 auto path=Path(modelPath);il2cpp_field_get_value(path.Get(),api.pathHashField,&fixtureProxyHash);
 // The actual game can start instantiation on the later proxy update, after
 // LoadModelAsync returned and the submission's temporary route was restored.
 creatingUI=true;ProxyPreload(instance,true,nullptr);creatingUI=false;uiProxyRoot=proxyCloneResult;
 ProxyPreload(effect,true,nullptr);
 Require(proxyCloneResult==restoredRoot&&Clone(source)==restoredRoot&&!proxyUIOverride.Get(),"UI request routing escaped into another proxy or the public clone route");
 UIComplete(uiClosure,uiProxy,nullptr);bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
 Require(!refused&&a.phase==Phase::Passed&&uiRoot==newRoot&&generation==3,
  "later UI proxy instantiation used old resources after the submission route ended");
 Reset();
}
void UIDelayedFrameRegression(){
 using namespace FullResourceReload;
 struct Cleanup {~Cleanup(){attempt.reset();pending.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});std::atomic_store(&failedKey,std::shared_ptr<const std::string>{});enabled=false;RuntimeShapes::enabled=false;Reset();}}cleanup;
 for(bool superseded:{false,true}){
  Reset();enabled=true;RuntimeShapes::enabled=true;pending.reset();RuntimeShapes::packageGeneration=RuntimeShapes::checkedGeneration=generation.load();
  RuntimeShapes::api.getEnabled=(void*)110;RuntimeControls::specs.at("marker").vertices=2;
  uiRoot=uiProxyRoot=(void*)0x1006;uiResultRoot=(void*)0x1007;auto oldUI=uiRoot;
  UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);UIComplete(uiClosure,uiProxy,nullptr);
  attempt=std::make_shared<Attempt>(MakeAttempt());auto& a=*attempt;a.prepared->controls.specs=RuntimeControls::specs;
  // World and UI roots are independent consumers of the same prefab.
  CaptureConsumers(a.owners.front());a.owners.clear();
  // Record the normal world lifecycle before discovering both instances.
  recordPartFinished=true;RecordPart();
  DiscoverOwners(a);Require(a.owners.size()==2,"mixed UI/world fixture did not capture both native owners");
  a.prepared->pointer={std::string(64,'A'),std::string(64,'B')};a.phase=Phase::Rebuild;
  std::atomic_store(&latest,std::make_shared<const EiemOfflinePackage::Pointer>(a.prepared->pointer));
  std::atomic_store(&requested,std::shared_ptr<const Ticket>{});uiDelayed=true;
  Frame();Require(a.phase==Phase::UIWait&&currentRoot==oldRoot&&animationRoot==oldRoot&&uiRoot==oldUI&&uiCallbacks==1&&!removeCalls,"mixed F10 released world/UI owners before native UI completion");
  Require(!uiRequestCallback&&!nullDelegateTargetCalls,"mixed F10 inspected a null page callback through Delegate.get_Target");
  // Unrelated models appearing during the wait must not block this Mod.
  liveParts[unrelatedPart]={};liveParts[unrelatedPart].part=RuntimeShapes::Ref::Make(unrelatedPart,true);liveParts[unrelatedPart].root=RuntimeShapes::Ref::Make(unrelatedRoot,true);liveParts[unrelatedPart].path="assets/unmodified-character.prefab";++ownerRevision;
  Frame();Require(a.phase==Phase::UIWait&&currentRoot==oldRoot&&uiCallbacks==1,"UI wait blocked the frame or unrelated lifecycle changed the transaction");
  if(superseded){auto ticket=std::make_shared<Ticket>();ticket->pointer=a.prepared->pointer;ticket->clock=1;std::atomic_store(&requested,std::shared_ptr<const Ticket>(ticket));
   Frame();Require(a.phase==Phase::Reject&&currentRoot==oldRoot&&uiRoot==oldUI&&!removeCalls&&uiCallbacks==1,"superseded pending UI request mutated an old owner");
   Frame();Require(a.phase==Phase::Reject,"native candidate retired while cancelled UI instantiation was still running");
   UIComplete(uiClosure,uiProxy,nullptr);Frame();Require(a.phase==Phase::Drained&&uiCallbacks==1&&uiReloadRequests.empty(),"cancelled UI job did not drain through its native completion");
  }else{UIComplete(uiClosure,uiProxy,nullptr);Frame();
   Require(a.phase==Phase::Passed&&generation==3&&currentRoot==newRoot&&animationRoot==currentRoot&&uiRoot==uiResultRoot&&uiCallbacks==1&&uiPageBinds==1&&uiUnloads==1&&animationRebinds==1,
    "mixed async UI/world Frame did not preserve animation and complete the native page callback: "+a.error);
  }
  attempt.reset();std::atomic_store(&requested,std::shared_ptr<const Ticket>{});
 }
}
void UIModelReloadRegression(){
 using namespace FullResourceReload;
 // The real PhaseBase callback resumes coroutine.waitAsyncRequest once.
 // Native acceptance/active hierarchy alone does not update the page item.
 Reset();UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);UIComplete(uiClosure,uiProxy,nullptr);
 uiOwned.insert(newRoot);{void* args[]={newRoot};Call(api.uiCallbackInvoke,uiCallback,args);}
 UIUnload(uiLoader,oldRoot,nullptr);
 Require(uiCallbacks==2&&uiRoot==oldRoot&&!uiOwned.count(uiRoot)&&uiOwned.count(newRoot),"one-shot continuation did not reproduce native COMMIT with a blank page");
 // Sync-load/replay skips native async parent/component initialization even
 // when the panel delegate itself runs. Lock down that failed game sequence.
 Reset();UILoad(uiLoader,StringValue(modelPath),factoryParent,nullptr);
 {void* args[]={oldRoot};Call(api.uiCallbackInvoke,uiCallback,args);}
 Require(uiCallbacks==1&&!uiNativeReady,"sync-load replay did not reproduce skipped native UI initialization");
 for(bool reject:{false,true}){
  Reset();RuntimeShapes::enabled=true;RuntimeShapes::packageGeneration=RuntimeShapes::checkedGeneration=generation.load();
  struct Cleanup {~Cleanup(){RuntimeShapes::enabled=false;Reset();}} cleanup;
  RuntimeShapes::api.getEnabled=(void*)110;
  RuntimeControls::specs.at("marker").vertices=2;
  UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);UIComplete(uiClosure,uiProxy,nullptr);
  currentRoot=nullptr;auto a=MakeAttempt();a.prepared->controls.specs=RuntimeControls::specs;a.owners.clear();DiscoverOwners(a);
  Require(a.owners.size()==1&&a.owners[0].uiModel&&!sceneQueries,"UI loader instance has no captured game lifecycle owner");
  rejectVertex=reject;bool refused=false;try{Rebuild(a);}catch(const std::exception&){refused=true;}
  Require(refused==reject,"UI transaction failure/result differs");
  if(reject){Require(generation==2&&uiRoot==oldRoot&&uiCallbacks==1&&uiUnloads==0&&uiOwned.count(oldRoot)&&uiCancelledCompletions==1&&RuntimeShapes::bound.count(oldRenderer),"failed UI candidate changed the displayed model or replayed its page callback");}
  else{
   Require(a.phase==Phase::Passed&&generation==3&&uiRoot==newRoot&&uiCallbacks==1&&uiPageBinds==1&&uiUnloads==1&&uiNativeReady&&RuntimeShapes::bound.count(newRenderer),"UI F10 skipped native async completion or did not update the panel's instance reference");
   newRoot=(void*)0x1005;a=MakeAttempt();a.prepared->controls.specs=RuntimeControls::specs;a.prepared->base=generation.load();a.owners.clear();DiscoverOwners(a);Rebuild(a);
   Require(a.phase==Phase::Passed&&generation==4&&uiRoot==newRoot&&uiCallbacks==1&&uiPageBinds==2&&uiUnloads==2&&!sceneQueries,"consecutive UI F10 lost its original panel callback");
  }
  UIDispose(uiLoader,nullptr);Require(liveUIModels.empty()&&RuntimeShapes::bound.empty(),"disposed UI loader retained callbacks or control bindings");
  newRoot=(void*)0x1003;
 }
 Reset();UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);auto a=MakeAttempt();a.owners.clear();bool refused=false;
 try{DiscoverOwners(a);}catch(const std::exception&){refused=true;}Require(refused,"F10 committed over an affected unfinished UI creation");
 UICancel(uiLoader,7,nullptr);a.owners.clear();DiscoverOwners(a);Require(a.owners.empty(),"cancelled UI creation remained a live reload owner");Reset();
 UILoad(uiLoader,StringValue(modelPath),factoryParent,nullptr);uiPageAvailable=false;a=MakeAttempt();a.owners.clear();refused=false;
 try{DiscoverOwners(a);}catch(const std::exception&){refused=true;}
 Require(refused&&uiOwned.count(oldRoot)&&generation==2,"UI model without a page binding was silently replaced behind its caller");Reset();
 UIAsync(uiLoader,StringValue(modelPath),factoryParent,uiCallback,nullptr);UIComplete(uiClosure,uiProxy,nullptr);
 currentRoot=nullptr;a=MakeAttempt();a.owners.clear();DiscoverOwners(a);uiPageReject=true;refused=false;
 try{Rebuild(a);}catch(const std::exception&){refused=true;}
 Require(refused&&uiOwned.count(oldRoot)&&!uiOwned.count(newRoot)&&uiRoot==oldRoot&&uiCallbacks==1&&generation==2,"loader acceptance was committed despite a page refusing the new instance");Reset();
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
 unsigned replayed=0;auto cachedPlan=std::make_shared<EiemResourceReloadPlan::Plan>();cachedPlan->originals=emptyPlan->originals;cachedPlan->startupHashes={std::string(64,'B')};cachedPlan->rendererRules.resize(1);
 auto cached=std::make_shared<EiemPrefabRules::Context>(cachedPlan,Package{},[&](size_t,bool){++replayed;throw std::runtime_error("startup should not replay source on the game thread");return Bytes{};});
 cached->startupInputsPacked=true;cached->startupInputs.emplace(0,std::make_shared<const Bytes>(EiemCompiledMod::Pack(Bytes{1,2,3,4})));std::atomic_store(&resourceRules,cached);
 bool hit=false;Require(LoadRuleInput(matched,0,12,true,hit)&&hit&&!replayed,"prepared startup input was rebuilt on the game's loading thread");
 auto allocations=storage.size();hit=false;Require(LoadRuleInput(matched,0,12,true,hit)&&hit&&!replayed&&storage.size()==allocations+1,"repeated startup load copied another Byte[] instead of reusing the live stream buffer");
 auto submittedCount=memoryCalls;cachedPlan->startupHashes[0]=cachedPlan->originals[0].hash;hit=false;
 Require(!LoadRuleInput(matched,77,12,true,hit)&&!hit&&!replayed&&memoryCalls==submittedCount,"unchanged dependency intercepted the native file/CRC path");
 for(const auto& request:NativeBundle::memoryRequests)scheduleDone[request.first]=true;NativeBundle::CollectMemory();
 for(const auto& b:NativeBundle::memoryBundles)unloadedStreamBundles.insert(b.first);NativeBundle::CollectMemory();Require(NativeBundle::memoryBundles.empty(),"unloaded rule Bundles retained archive streams");
 startupArrays.clear();
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
 for(auto phase:{Phase::Owners,Phase::Rebuild,Phase::UIWait}){attempt->phase=phase;bool refused=false;try{Step(*attempt);}catch(const std::exception&){refused=true;}
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
void CompiledSnapshotRegression(){
 using namespace FullResourceReload;Reset();auto previousGame=gameDirectory;auto previousDisk=diskPrepared;auto previousSet=preparedCompiledSet;auto previousPointer=lastPrepared;bool previousActive=active;
 auto folder=std::filesystem::absolute("bin")/("compiled-snapshot-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));Require(std::filesystem::create_directory(folder),"snapshot fixture already exists");
 struct Cleanup {std::filesystem::path game,folder;std::shared_ptr<Prepared> disk;std::shared_ptr<const EiemCompiledModSet::Set> set;EiemOfflinePackage::Pointer pointer;bool activeBefore;~Cleanup(){attempt.reset();pending.reset();diskPrepared=disk;preparedCompiledSet=set;lastPrepared=pointer;gameDirectory=game;active=activeBefore;enabled=false;std::atomic_store(&requested,std::shared_ptr<const Ticket>{});std::atomic_store(&latest,std::shared_ptr<const EiemOfflinePackage::Pointer>{});std::filesystem::remove_all(folder);Reset();}}cleanup{previousGame,folder,previousDisk,previousSet,previousPointer,previousActive};
 gameDirectory=folder;active=true;enabled=true;std::filesystem::create_directories(folder/"plugin/mods");auto file=folder/"snapshot";EiemResourceReloadPlan::Save(file,{1,2,3});
 auto ready=std::make_shared<EiemCompiledModSet::Set>();ready->pointer={std::string(64,'A'),std::string(64,'B')};ready->manifest.plan.key=ready->pointer.key;ready->files.emplace(file,FileStamp(file));preparedCompiledSet=ready;
 auto before=std::make_shared<Prepared>();before->pointer=preparedCompiledSet->pointer;before->plan=std::make_shared<const Plan>(preparedCompiledSet->manifest.plan);before->graph=std::make_shared<const std::vector<Package>>();before->authorFiles.emplace(file,InputFileStamp{});before->rules=EiemCompiledModSet::Context(preparedCompiledSet);diskPrepared=before;lastPrepared=before->pointer;
 attempt=std::make_unique<Attempt>();attempt->prepared=before;attempt->phase=Phase::Ready;
 PollCompiled(true);Require(diskPrepared!=before&&AuthorFilesUnchanged(*diskPrepared)&&std::atomic_load(&requested),"same-content re-export retained stale author stamps and rejected F10");
 // Native candidate identity is unchanged. Refresh its verified author
 // snapshot without losing the ready model or repeating native loads.
 std::atomic_store(&requested,std::shared_ptr<const Ticket>{});Frame();Require(attempt->phase==Phase::Ready&&AuthorFilesUnchanged(*attempt->prepared)&&!pending&&!opened&&!leaseRequests&&!removeCalls&&!loadCalls,"same-key pending candidate discarded the new snapshot or repeated native work");
 EiemResourceReloadPlan::Save(file,{1,2,3,4});Require(!AuthorFilesUnchanged(*attempt->prepared),"actual post-preflight file edits bypassed the snapshot guard");
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
   plan->roots.front().assembly.sources.front().targets.front().marker.clear();
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
 plan->roots.front().assembly.sources.front().targets.front().marker.clear();
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
   plan->roots.front().assembly.sources.front().targets.front().marker.clear();
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
void SerializedBoneAddressRegression(){
 using namespace Test;using namespace FullResourceReload;
 Reset();auto prepared=Test::Prepare();auto plan=*prepared->plan;
 // A story/low-LOD prefab can bind the same Mesh bone hash to a native
 // Transform whose path differs from the author skeleton. The offline
 // descriptor records that actual binding, including its exact slot order.
 plan.parts.at("marker").bones[0]="Root/Bip001/AuthorBone";
 VerifyPrefab(candidate,plan,plan.roots.front());
 auto reject=[&](auto broken){bool failed=false;try{VerifyPrefab(candidate,broken,broken.roots.front());}catch(const std::exception&){failed=true;}Require(failed,"incorrect native bone address or legacy author path accepted");};
 auto wrong=plan;wrong.roots.front().assembly.sources.front().targets.front().bones.front()=EiemRenderAssemblyDescription::Segment("Root",0)+EiemRenderAssemblyDescription::Segment("Bip001",0)+EiemRenderAssemblyDescription::Segment("Left",0)+EiemRenderAssemblyDescription::Segment("Bone",0);reject(wrong);
 auto legacy=plan;legacy.roots.front().assembly.sources.clear();reject(legacy);
 legacy.parts.at("marker").bones[0]="Root/Bip001/Bone";VerifyPrefab(candidate,legacy,legacy.roots.front());Reset();
}
void MaterialShaderRegression(){
 using namespace Test;using namespace FullResourceReload;
 Reset();api.shaderSupported=(void*)130;api.materialPassCount=(void*)131;auto prepared=Test::Prepare();
 VerifyPrefab(candidate,*prepared->plan,prepared->plan->roots.front());
 auto reject=[&]{bool failed=false;try{VerifyPrefab(candidate,*prepared->plan,prepared->plan->roots.front());}catch(const std::exception&){failed=true;}Require(failed,"live Shader object incorrectly accepted unsupported/error/zero-pass material");};
 shaderSupported=false;reject();shaderSupported=true;shaderPasses=0;reject();shaderPasses=2;shaderLabel="Hidden/InternalErrorShader";reject();shaderLabel="Fixture/Character";
 api.shaderSupported=api.materialPassCount=nullptr;Reset();
}
void WeaponMountBeforeReloadRegression(){
 using namespace Test;using namespace FullResourceReload;
 Reset();auto attempt=MakeAttempt();CaptureConsumers(attempt.owners.front());Rebuild(attempt);
 Require(attempt.phase==Phase::Passed&&weaponRequestMountRoot==currentRoot,"weapon async reload captured retired model mounts before UpdateAllWeaponMountPoint");
 Require(consumerCalls==std::vector<int>{155,70,68,74,72,71},"native perform/model/audio/mount/weapon lifecycle order differs from game");
 Reset();auto rollback=MakeAttempt();CaptureConsumers(rollback.owners.front());ignoreCandidate=true;bool refused=false;try{Rebuild(rollback);}catch(const std::exception&){refused=true;}
 Require(refused&&currentRoot==restoredRoot&&weaponRequestMountRoot==restoredRoot,"weapon reload on rollback captured retired model mounts");
 Require(consumerCalls==std::vector<int>{155,70,155,70,68,74,72,71},"rollback perform/audio/mount/weapon lifecycle order differs from game");Reset();
}
void StartupDependencyRegression(){
 using namespace EiemBundleDependencies;
 EiemBundleDependencies::Keys current{{1,{3}},{2,{4}},{3,{5}},{5,{3}}};std::map<uint64_t,std::vector<uint64_t>> vanilla{{3,{6}},{4,{7}},{6,{}}};
 auto edges=[&](uint64_t key){auto found=current.find(key);return found==current.end()?std::vector<uint64_t>{}:found->second;};
 unsigned queries=0;auto native=[&](uint64_t key){++queries;return vanilla[key];};
 auto direct=Added(1,{2},true,edges,native);Require(direct==std::vector<uint64_t>{3}&&queries==0,"direct dependencies opened resources or flattened native dependencies");
 auto all=Added(1,{2},false,edges,native);Require(all==std::vector<uint64_t>({3,6,4,7,5}),"startup full dependencies omitted foreign material/native transitive dependencies");
 Require(Added(1,{2,3,6,4,7,5},false,edges,native).empty(),"startup dependencies duplicated existing game entries");
 auto old=current;current={{1,{9}}};Require(Added(1,{},true,edges,native)==std::vector<uint64_t>{9},"new Mod set did not use new dependency edges");
 auto retained=[&](uint64_t key){auto found=old.find(key);return found==old.end()?std::vector<uint64_t>{}:found->second;};
 Require(Added(1,{},true,retained,native)==std::vector<uint64_t>{3},"loaded original bundle lost its dependency snapshot after F10");
 current.clear();Require(Added(1,{2},false,edges,native).empty(),"empty Mod set retained new dependency rules");
}
bool NativeDependencyQuery(void*,uint64_t key,void* list,void*){
 auto& output=Test::dependencyLists.at(list);auto found=Test::nativeDependencyLists.find(key);if(found==Test::nativeDependencyLists.end())return false;output.insert(output.end(),found->second.begin(),found->second.end());return true;
}
void StartupDependencyListBindingRegression(){
 using namespace Test;namespace D=StartupDependencies;
 struct MethodRow {MInfo info{};const char* name;const char* result;const char* parameter;void* klass=(void*)700;const char* second=nullptr;};
 static MethodRow rows[]={{{},".ctor","System.Void",nullptr},{{},".ctor","System.Void","System.Int32"},
  {{},"Add","System.Void","System.UInt64"},{{},"System.Collections.IList.Add","System.Int32","System.Object"},
  {{},".ctor","System.Void","Test.ObjectPool<Test.PooledList<System.UInt64>>",(void*)701,"System.Int32"},
  {{},"Allocate","Test.PooledList<System.UInt64>",nullptr,(void*)702},{{},"Dispose","System.Void",nullptr,(void*)701}};
 static unsigned allocated=0,disposed=0;allocated=disposed=0;static int pool;
 Reset();propertyElementWidth=8;auto oldApi=D::api;
 auto previous=std::make_tuple(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,
  il2cpp_object_new,il2cpp_value_box,il2cpp_runtime_invoke,il2cpp_class_get_parent,il2cpp_field_get_value,il2cpp_object_get_class);
 auto restore=[&]{std::tie(il2cpp_class_get_methods,il2cpp_method_get_name,il2cpp_method_get_param_count,
  il2cpp_method_get_flags,il2cpp_method_get_return_type,il2cpp_method_get_param,il2cpp_type_get_name,
  il2cpp_object_new,il2cpp_value_box,il2cpp_runtime_invoke,il2cpp_class_get_parent,il2cpp_field_get_value,il2cpp_object_get_class)=previous;D::api=oldApi;dependencyLists.clear();Reset();};
 il2cpp_class_get_methods=[](void* klass,void** it)->void*{auto index=uintptr_t(*it);while(index<std::size(rows)&&rows[index].klass!=klass)++index;if(index==std::size(rows))return nullptr;*it=(void*)(index+1);return &rows[index];};
 il2cpp_class_get_parent=[](void* klass)->void*{return klass==(void*)701?(void*)700:nullptr;};
 il2cpp_method_get_name=[](void* method)->const char*{return static_cast<MethodRow*>(method)->name;};
 il2cpp_method_get_param_count=[](void* method)->uint32_t{auto row=static_cast<MethodRow*>(method);return row->second?2:row->parameter?1:0;};
 il2cpp_method_get_flags=[](void*,uint32_t*)->uint32_t{return 0x86;};
 il2cpp_method_get_return_type=[](void* method)->void*{return const_cast<char*>(static_cast<MethodRow*>(method)->result);};
 il2cpp_method_get_param=[](void* method,uint32_t index)->void*{auto row=static_cast<MethodRow*>(method);return const_cast<char*>(index?row->second:row->parameter);};
 il2cpp_type_get_name=[](void* type)->const char*{return static_cast<const char*>(type);};
 il2cpp_object_new=[](void*)->void*{auto bytes=std::make_unique<Bytes>(8);auto value=bytes->data();storage.push_back(std::move(bytes));dependencyLists[value]={};return value;};
 il2cpp_field_get_value=[](void* self,void* field,void* out){if(field==(void*)20){Require(dependencyLists.count(self)!=0,"pool field read from another object");void* p=&pool;memcpy(out,&p,8);}else FieldValue(self,field,out);};
 il2cpp_object_get_class=[](void* object)->void*{return object==&pool?(void*)702:dependencyLists.count(object)?StartupDependencies::api.listClass:GetClass(object);};
 il2cpp_value_box=[](void* klass,void* data)->void*{Require(klass==propertyElementClass,"IList bridge boxed a different scalar type");auto bytes=std::make_unique<Bytes>(8);memcpy(bytes->data(),data,8);auto value=bytes->data();storage.push_back(std::move(bytes));return value;};
 il2cpp_runtime_invoke=[](void* method,void* self,void** args,void** exception)->void*{
  *exception=nullptr;auto index=static_cast<MethodRow*>(method)-rows;Require(index>=0&&index<7&&index!=4,"list binder invoked another API or constructed a pooled list directly");
  if(index==5){Require(self==&pool&&!args,"pool allocation call shape differs");++allocated;return il2cpp_object_new((void*)701);}
  if(index==6){Require(!args&&dependencyLists.erase(self)==1,"scratch list was recycled twice");++disposed;return nullptr;}
  if(index<2){Require(index?args&&*(int*)args[0]==0:!args,"list constructor used a different argument shape");dependencyLists[self].clear();}
  else dependencyLists.at(self).push_back(*(uint64_t*)args[0]);return nullptr;
 };
 D::api={};D::api.listClass=(void*)700;D::api.scalarClass=propertyElementClass;D::api.listItems=(void*)18;D::api.listSize=(void*)19;
 try{
  for(bool stripped:{false,true}){
   for(auto& row:rows)row.info.mp=(void*)1;
   if(stripped){rows[0].info.mp=rows[2].info.mp=nullptr;
    Require(!Exact(D::api.listClass,".ctor",{},"System.Void",false)&&!Exact(D::api.listClass,"Add",{"System.UInt64"},"System.Void",false),"fixture did not expose the old startup binding rejection");}
   D::BindListMethods(D::api.listClass,"System.UInt64");D::VerifyListApi();
   Require(D::api.capacityCtor==stripped&&D::api.boxedAdd==stripped,"list binding did not select the available compiled interface");
  }
  rows[2].info.mp=nullptr;rows[3].parameter="System.Int32";bool rejected=false;
  try{D::BindListMethods(D::api.listClass,"System.UInt64");}catch(const std::exception&){rejected=true;}
  Require(rejected,"dependency list accepted an incompatible append argument");rows[3].parameter="System.Object";
  // The real game supplies a PooledList with a two-argument constructor.
  // List methods live on its parent, and the native pool owns scratch lists.
  D::api.listClass=(void*)701;D::api.poolField=(void*)20;D::api.poolClass=(void*)702;
  D::api.poolAllocate=Exact(D::api.poolClass,"Allocate",{},"Test.PooledList<System.UInt64>",false);
  D::api.listDispose=D::Inherited(D::api.listClass,"Dispose",{},"System.Void");
  Require(!Exact(D::api.listClass,".ctor",{},"System.Void",false)&&!Exact(D::api.listClass,".ctor",{"System.Int32"},"System.Void",false),"pooled fixture has a plain List constructor");
  auto prototype=il2cpp_object_new(D::api.listClass);dependencyLists[prototype]={91};
  for(bool boxed:{false,true}){rows[2].info.mp=boxed?nullptr:(void*)1;
   D::BindListMethods(D::api.listClass,"System.UInt64");D::VerifyListApi(prototype);
   Require(D::api.boxedAdd==boxed&&!D::api.listCtor,"pooled list did not use inherited append and pool allocation");
  }
  Require(allocated==2&&disposed==2&&dependencyLists.at(prototype)==std::vector<uint64_t>{91},"pooled dependency scratch lifecycle changed the game's input list");
  bool badAppend=false;
  try{auto scratch=D::NewList(prototype);throw std::runtime_error("injected query failure");}catch(const std::exception&){badAppend=true;}
  Require(badAppend&&allocated==3&&disposed==3,"scratch list was not recycled after query failure");
 }catch(...){rows[3].parameter="System.Object";restore();throw;}
 restore();
}
void StartupDependencyHookRegression(){
 using namespace Test;namespace D=StartupDependencies;
 Reset();propertyElementWidth=8;auto oldApi=D::api;auto oldNew=il2cpp_object_new;auto oldRules=std::atomic_load(&Entry::resourceRules);
 D::api.bundleName=(void*)143;D::api.managerManifest=(void*)144;D::api.listCtor=(void*)140;D::api.listAdd=(void*)141;D::api.listClass=(void*)700;D::api.listItems=(void*)18;D::api.listSize=(void*)19;
 D::api.manifestData=(void*)150;D::api.mappedBase=(void*)151;D::api.mappedLength=(void*)152;D::api.mappedData=(void*)153;D::api.bundles=(void*)154;
 // Different field offsets/stride from the inspected game, to enforce
 // metadata-based traversal rather than baking that build's binary layout.
 D::api.arraySize=24;D::api.arrayCount=4;D::api.arrayHead=16;D::api.bundleSize=64;D::api.bundleNameOffset=20;D::api.bundleKeyOffset=40;
 D::originalAll=D::originalDirect=NativeDependencyQuery;D::originalInit=[](void*,uint64_t,void*,void*){};D::originalClear=[](void*,void*){};D::enabled=true;
 dependencyNames={{"source",1},{"old",2},{"foreign",3},{"texture",4},{"later",9}};nativeDependencyLists={{1,{2}},{3,{4}}};dependencyLists.clear();
 BuildDependencyManifest();dependencyManifestReads=dependencyNameChecks=0;
 il2cpp_object_new=[](void*)->void*{auto value=std::make_unique<Bytes>(8);auto p=value->data();storage.push_back(std::move(value));dependencyLists[p]={};return p;};
 auto makeRules=[](uint64_t next){auto value=std::make_shared<EiemPrefabRules::Context>(Prepare()->plan,EiemNativeInput::Package{},[](size_t,bool){return Bytes{};});auto graph=std::make_shared<EiemBundleDependencies::Names>();(*graph)["source"].insert(next==3?"foreign":"later");value->addedDependencies=graph;return value;};
 std::atomic_store(&Entry::resourceRules,makeRules(3));void* list=il2cpp_object_new(D::api.listClass);void* proxy=(void*)0x7001;
 D::Init(proxy,1,manager,nullptr);
 // A game asset request must see the foreign material and its native texture
 // before it starts LoadAsset, without an extra ResourceManager request.
 auto loads=Test::memoryCalls,leases=leaseRequests;Require(D::All(dependencyManifest,1,list,nullptr),"startup full query lost native success");
 Require(dependencyLists.at(list)==std::vector<uint64_t>({2,3,4}),"game first-load dependency query omitted foreign material before asset creation");
 // The shipped manifest stores a VFS Bundle key, which differs from the
 // callable ManifestDataBinary.HashCodeEx4AutoLower result (PID 24076).
 D::indexes.clear();dependencyHashDiffers=true;dependencyLists[list].clear();
 D::All(dependencyManifest,1,list,nullptr);
 Require(dependencyLists.at(list)==std::vector<uint64_t>({2,3,4}),"stored manifest key was replaced by a different string hash; cold-start borrowed material missing");
 dependencyHashDiffers=false;
 dependencyLists[list].clear();D::Direct(dependencyManifest,1,list,nullptr);Require(dependencyLists.at(list)==std::vector<uint64_t>({2,3}),"native direct dependency ownership was flattened");
 std::atomic_store(&Entry::resourceRules,makeRules(9));dependencyLists[list].clear();D::Direct(dependencyManifest,1,list,nullptr);
 Require(dependencyLists.at(list)==std::vector<uint64_t>({2,3}),"F10 changed dependency release rules for an already loaded game Bundle");
 D::Clear(proxy,nullptr);D::Init(proxy,1,manager,nullptr);dependencyLists[list].clear();D::Direct(dependencyManifest,1,list,nullptr);
 Require(dependencyLists.at(list)==std::vector<uint64_t>({2,9}),"recycled game Bundle did not acquire current Mod dependencies");
 Require(Test::memoryCalls==loads&&leaseRequests==leases,"dependency query actively loaded resources or recursed into ResourceManager");
 auto rules=makeRules(3);auto graph=std::make_shared<EiemBundleDependencies::Names>();(*graph)["source"].insert("missing");rules->addedDependencies=graph;D::Clear(proxy,nullptr);std::atomic_store(&Entry::resourceRules,rules);
 auto reads=dependencyManifestReads,checks=dependencyNameChecks;
 for(int i=0;i<1000;++i){dependencyLists[list].clear();Require(D::All(dependencyManifest,1,list,nullptr),"failed manifest mapping suppressed native result");Require(dependencyLists.at(list)==std::vector<uint64_t>{2},"failed manifest mapping partially appended foreign keys");}
 Require(dependencyManifestReads==reads+1&&dependencyNameChecks==checks,"failed manifest mapping was rebuilt on every queue poll");
 dependencyNames["missing"]=0xFE369E5452DF7026ULL;BuildDependencyManifest();dependencyLists[list].clear();D::Direct(dependencyManifest,1,list,nullptr);
 Require(dependencyLists.at(list)==std::vector<uint64_t>({2,0xFE369E5452DF7026ULL}),"changed manifest did not retry a cached gap / truncated stored high-bit key");
 auto parsed=dependencyManifestReads;for(int i=0;i<1000;++i)Require(D::GetIndex(dependencyManifest,graph)!=nullptr,"ready manifest cache lost index");Require(dependencyManifestReads==parsed,"ready index rescanned manifest per query");
 D::Clear(proxy,nullptr);D::snapshots.clear();D::snapshotKeys.clear();D::indexes.clear();D::enabled=false;D::api=oldApi;il2cpp_object_new=oldNew;std::atomic_store(&Entry::resourceRules,oldRules);dependencyLists.clear();Reset();
}
int main(){
 using namespace Test;using namespace FullResourceReload;
 try{
  Setup();Reset();
  AssemblyAddressRegression();
  SerializedBoneAddressRegression();
  MaterialShaderRegression();
  WeaponMountBeforeReloadRegression();
  StartupDependencyRegression();
  StartupDependencyListBindingRegression();
  StartupDependencyHookRegression();
  auto check=[](const char* name,auto action){try{action();}catch(const std::exception& e){throw std::runtime_error(std::string(name)+": "+e.what());}};
  check("static Renderer verification and visibility",[]{
    Reset();staticRenderers=true;staticSkinCalls=0;staticEnabled=1;
    RuntimeShapes::api.getComponent=(void*)107;RuntimeShapes::api.filterMesh=(void*)108;RuntimeShapes::api.filterType=(void*)109;RuntimeShapes::api.getEnabled=(void*)110;
    auto prepared=Test::Prepare();auto plan=*prepared->plan;plan.parts.at("marker").bones.clear();
    auto& target=plan.roots[0].assembly.sources[0].targets[0];target.bones.clear();target.root.clear();target.hasRoot=false;
    VerifyPrefab(candidate,plan,plan.roots[0]);Require(!staticSkinCalls,"static verification touched skinning APIs");
    Require(!RuntimeShapes::SourceMesh(material),"non-Mesh Renderer invoked skin getter");
    auto specs=RuntimeShapes::specs;auto variables=RuntimeShapes::variables;auto setter=RuntimeShapes::originalEnabledSetter;
    RuntimeShapes::specs["marker"]={"static","part","marker",2,{},{}};auto& spec=RuntimeShapes::specs.at("marker");spec.visible=EiemExpression::Make();spec.visible->op=EiemExpression::Variable;spec.visible->variable="shown";RuntimeShapes::variables["static"]["shown"]=0;
    RuntimeShapes::originalEnabledSetter=[](void*,bool value){staticEnabled=value;};
    RuntimeShapes::RegisterRenderer(newRenderer,RuntimeShapes::SourceMesh(newRenderer));Require(!staticEnabled&&RuntimeShapes::bound.at(newRenderer).filter.Get()==filterObject,"static visibility/filter binding failed");
    RuntimeShapes::variables["static"]["shown"]=1;RuntimeShapes::ApplyVisibility(newRenderer,RuntimeShapes::bound.at(newRenderer),spec);Require(staticEnabled,"static visibility did not return");
    RuntimeShapes::bound.clear();RuntimeShapes::specs=std::move(specs);RuntimeShapes::variables=std::move(variables);RuntimeShapes::originalEnabledSetter=setter;staticRenderers=false;Reset();
  });
  check("UI model completion visibility",UIControlCompletionRegression);
  check("UI delayed completion keeps displayed model",UIDelayedCompletionRegression);
  check("UI asynchronous proxy retains candidate route",UILateProxyRoutingRegression);
  check("UI delayed completion through Frame",UIDelayedFrameRegression);
  check("UI model lifecycle reload",UIModelReloadRegression);
  check("synchronous world owner",SynchronousOwnerRegression);
  check("LoadModel release ownership",LoadReleaseOwnershipRegression);
  check("bone verification cost",BoneVerificationCostRegression);
  check("UI page API binding",UIPageApiBindingRegression);
  check("UI Lua exception details",UILuaExceptionRegression);
  check("UI encrypted script loading",UIEncryptedLoadRegression);
  check("UI controls without page transfer API",UIControlWithoutPageApiRegression);
  check("background candidate readiness",BackgroundReadyRegression);check("compiled same-content snapshot",CompiledSnapshotRegression);
  check("enum",EnumRegression);check("bones",BonePathRegression);check("root paths",RootPathRegression);check("managers",ManagerContractRegression);
  check("Material property bindings",MaterialBindingRegression);check("standby prop lifecycle",PerformLifecycleRegression);check("native performance preloads",PerformPreloadRegression);check("animation attachment probe contract",AnimationAttachmentProbeRegression);check("unmodified skin probe contract",UnmodifiedSkinProbeRegression);check("actual performance attachment probe",PerformanceProbeRegression);check("native resource release probe",PerformanceReleaseProbeRegression);check("native lifecycle",NativeLifecycleRegression);check("typed character audio",[]{typedAudioOnly=true;NativeLifecycleRegression();typedAudioOnly=false;});check("texture identity",TextureIdentityRegression);check("load scheduling",PrewarmScheduleRegression);check("committed retry",CommittedRetryRegression);check("released pool",ReleasedPoolRegression);check("display virtual override",DisplayOverrideRegression);check("rule memory loads",RuleMemoryRegression);
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
  std::cout<<"PASS full resource transaction (mock native APIs): rule archive stream ABI and async/lazy roots; stripped stream methods via engine icalls; failed/unmatched rule preserves original load; six native prefab factories preserve arguments; native animation resume/weapons/audio lifecycle; stale animation reproduced; old Mesh/Texture identity rejection with animated rollback; exact typed static/instance manager getters; startup zero extra native requests; full Frame world/display -> commit; delayed UI completion retains displayed models; mixed UI/world frames, cancellation and supersession; eight consecutive world reloads without resetting state; asynchronous/display/retired callback owners; structural callback binding; full bone paths; enum ABI; superseded tickets; rollback; native retirement\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
