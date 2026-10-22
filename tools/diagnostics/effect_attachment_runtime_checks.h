#pragma once
namespace EffectAttachmentTest {
using namespace Entry;namespace A=RuntimeAttachments;namespace D=EffectDebug;
bool rendererEnabled=true,rendererActive=true;
bool splitOwners=false;
bool helperCompletion=false;void* helperRoot=nullptr;unsigned helperInitCalls=0,ownerTraversalChecks=0;
void* helperFieldValue=nullptr;void* helperList=nullptr;
decltype(il2cpp_field_get_value) fallbackFieldValue=nullptr;
decltype(il2cpp_class_get_fields) fallbackFields=nullptr;decltype(il2cpp_field_get_name) fallbackFieldName=nullptr;
std::map<void*,std::vector<void*>> ownerRenderers,rendererBones;
std::map<void*,unsigned> rendererScanCalls;
void* HelperListFields(void* klass,void** iterator){if(klass==Test::GetClass(helperList)){auto n=uintptr_t(*iterator);if(n>=2)return nullptr;*iterator=(void*)(n+1);return n?(void*)19:(void*)18;}return fallbackFields(klass,iterator);}
const char* HelperListFieldName(void* field){return field==(void*)18?"_items":field==(void*)19?"_size":fallbackFieldName(field);}
void HelperFields(void* receiver,void* field,void* output){
 if(field==helperFieldValue){*(void**)output=helperList;return;}
 if(helperCompletion&&receiver==helperList){if(field==(void*)18)*(void**)output=Test::Array(ownerRenderers.at(helperRoot));else if(field==(void*)19)*(int*)output=int(ownerRenderers.at(helperRoot).size());else fallbackFieldValue(receiver,field,output);return;}
 fallbackFieldValue(receiver,field,output);
}
std::map<void*,std::string> meshNames,nodeNames;
std::map<void*,void*> rendererMeshes;
std::set<void*> hiddenRenderers;
void* Invoke(void* method,void* self,void** args,void** error){
 if(splitOwners){
  *error=nullptr;auto m=uintptr_t(method);
  if(m==909&&args[0]!=(void*)910){++rendererScanCalls[self];return Test::Array(ownerRenderers[self]);}
  if(m==86)return Test::Array(ownerRenderers.at(self));
  if(m==4){auto mesh=meshNames.find(self);if(mesh!=meshNames.end())return Test::StringValue(mesh->second);auto node=nodeNames.find(self);if(node!=nodeNames.end())return Test::StringValue(node->second);}
  if(m==8)return Test::Array(rendererBones.at(self));
  if(m==10)return rendererBones.at(self).front();
  if(m==920)return hiddenRenderers.count(self)?&Test::no:&Test::yes;
  if(m==57){++ownerTraversalChecks;std::set<void*> seen;for(auto current=self;current&&seen.insert(current).second;){if(current==args[0])return &Test::yes;auto parent=Test::transformParents.find(current);if(parent==Test::transformParents.end())break;current=parent->second;}return &Test::no;}
  if(m==914&&(self==Test::unrelatedHelper||self==(void*)0xFA13||self==(void*)0xFA14))return helperRoot;
 }
 if(uintptr_t(method)==919){*error=nullptr;return self==Test::oldRenderer&&!rendererActive?&Test::no:&Test::yes;}
 if(uintptr_t(method)==920){*error=nullptr;return rendererEnabled?&Test::yes:&Test::no;}
 if(uintptr_t(method)==909&&args[0]!=(void*)910){*error=nullptr;return Test::Array(Test::oldRenderer);}return EffectTest::Invoke(method,self,args,error);
}
void SplitOwnerChecks(){
 // Same source Mesh expands into distinct authored Parts with different bone
 // palettes. Exercise the actual ModelReady call with the wrong Part first.
 splitOwners=true;auto previousParents=Test::transformParents;auto previousGetter=RuntimeShapes::originalMeshGetter;auto previousClone=FullResourceReload::originalCloneParent;auto previousClass=il2cpp_object_get_class;
 il2cpp_object_get_class=[](void* object)->void*{return rendererMeshes.count(object)?Test::skinClass:Test::GetClass(object);};
 bool previousEnabled=RuntimeShapes::enabled.load();auto previousPackageGeneration=RuntimeShapes::packageGeneration;
 Test::transformParents={{Test::oldRoot,nullptr},{Test::unrelatedRoot,nullptr},{Test::newRoot,nullptr},
  {Test::bone,Test::oldRoot},{Test::right,Test::oldRoot},{Test::leftBone,Test::unrelatedRoot},{Test::left,Test::unrelatedRoot},
  {Test::rightBone,Test::newRoot},{Test::bip,Test::newRoot}};
 nodeNames={{Test::bone,"Bone"},{Test::right,"Right"},{Test::leftBone,"Bone"},{Test::left,"Right"},{Test::rightBone,"Bone"},{Test::bip,"Right"}};
 ownerRenderers={{Test::oldRoot,{Test::oldRenderer,Test::newRenderer}},{Test::unrelatedRoot,{Test::unrelatedRenderer}},{Test::newRoot,{Test::auxiliaryRenderer}}};
 rendererBones={{Test::oldRenderer,{Test::bone}},{Test::newRenderer,{Test::bone,Test::right}},{Test::unrelatedRenderer,{Test::leftBone,Test::left}},{Test::auxiliaryRenderer,{Test::rightBone,Test::bip}}};
 rendererMeshes={{Test::oldRenderer,Test::oldMesh},{Test::newRenderer,Test::newMesh},{Test::unrelatedRenderer,Test::newMesh},{Test::auxiliaryRenderer,Test::newMesh}};
 meshNames={{Test::oldMesh,"wrong"},{Test::newMesh,"exact"}};
 RuntimeShapes::originalMeshGetter=[](void* renderer)->void*{return rendererMeshes.at(renderer);};
 EffectTest::layers[Test::newRenderer]=8;EffectTest::layers[Test::unrelatedRenderer]=9;EffectTest::layers[Test::auxiliaryRenderer]=21;
 FullResourceReload::originalCloneParent=[](void* prefab,void* parent,bool world)->void*{
  EffectTest::Check(prefab==Test::candidate&&!world&&!EffectTest::active[prefab],"clone lost inactive prefab or local transform mode");auto clone=(void*)(uintptr_t(0xD000)+ ++Test::cloneCalls);auto child=(void*)(uintptr_t(clone)+0x2000);
  EffectTest::active[clone]=false;EffectTest::cloneParents[clone]=parent;auto particle=(void*)(uintptr_t(clone)+0x1000);EffectTest::particles[clone]={particle};EffectTest::particleSystems.insert(particle);EffectTest::particleGameObjects[particle]=(void*)(uintptr_t(clone)+0x1800);EffectTest::particleRenderers[particle]=(void*)(uintptr_t(clone)+0x3000);
  Test::transformParents[clone]=parent;Test::transformParents[child]=clone;EffectTest::effectChildren[clone]=child;
  EffectTest::requiredLayers[clone]=(parent==Test::right||Test::transformParents.at(parent)==Test::oldRoot)?EffectTest::layers.at(Test::newRenderer):parent==Test::left?EffectTest::layers.at(Test::unrelatedRenderer):EffectTest::layers.at(Test::auxiliaryRenderer);return clone;
 };
 Package p;p.id="SplitOwnerMod";p.parts["PartOther"].mesh="MeshOther";p.meshes["MeshOther"].bones={"Bone"};p.parts["PartPetals"].mesh="MeshPetals";p.meshes["MeshPetals"].bones={"Bone","Right"};
 auto& rule=p.rules["ReplaceCloth"];rule.parts={"PartOther","PartPetals"};rule.targets[0].path="assets/cloth.mesh";rule.targets[0].asset="Cloth";
 p.meshes["MeshPetals"].resource="petals.mesh";auto provenance=std::make_shared<Resource>();provenance->bytes={1,2,3,4};p.resources["petals.mesh"]=provenance;
 for(unsigned level=1;level<=3;++level)rule.targets[level]={"assets/cloth-lods.mesh","ClothLOD"+std::to_string(level)};
 meshNames[Test::newMesh]=RuntimePartName(p,"PartPetals",rule.targets.at(0));
 EffectAttachment effect;effect.id="petals";effect.part="PartPetals";effect.bone="Right";effect.variable="$fx_petals";effect.targetPrefab="assets/author-postmodel.prefab";effect.targetPath="assets/cloth.mesh";effect.targetAsset="Cloth";effect.native=EffectTest::Manifest('5');p.effects={{effect.id,effect}};p.defaults[effect.variable]=1;
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{p});RuntimeControls::variables[p.id]=p.defaults;RuntimeControls::specs.clear();
 RuntimeControls::Spec wrong;wrong.package=p.id;wrong.part="PartOther";wrong.marker="wrong";wrong.vertices=2;RuntimeControls::specs[wrong.marker]=wrong;auto exact=wrong;exact.part="PartPetals";exact.marker=meshNames.at(Test::newMesh);RuntimeControls::specs[exact.marker]=exact;
 A::standalone=std::make_shared<const std::vector<Package>>();A::diskEffects.clear();A::overrides.clear();A::activeStamps=A::CaptureStamps();A::stampsInitialized=true;A::needed=true;A::revision=RuntimeControls::revision;
 RuntimeShapes::enabled=true;RuntimeShapes::packageGeneration=Entry::generation.load();Test::currentRoot=Test::oldRoot;FullResourceReload::RecordPartFinished(Test::mainPart);
 Require(A::bindings.size()==1,"split source Parts selected the wrong bone palette; no effect bound");
 auto& world=A::bindings.at({Test::oldRoot,p.id,effect.id,Test::right});Require(world.renderers.size()==1&&world.renderers.front().Get()==Test::newRenderer&&world.parent.Get()==Test::right,"explicit effect Part matched a sibling from its source rule");
 // The real NPC reaches helper completion with authored Meshes but never a
 // BaseModelViewPart/UI callback. Exercise that same production entry.
 auto previousHelper=RuntimeShapes::originalHelperInit;auto previousHelperField=RuntimeShapes::helperRenderersField;auto previousComponentGo=FullResourceReload::api.componentGo;auto previousReloadEnabled=FullResourceReload::enabled.load();FullResourceReload::enabled=true;
 FullResourceReload::api.componentGo=(void*)914;
 fallbackFieldValue=il2cpp_field_get_value;fallbackFields=il2cpp_class_get_fields;fallbackFieldName=il2cpp_field_get_name;helperFieldValue=(void*)0xFA11;helperList=(void*)0xFA12;helperRoot=Test::unrelatedRoot;helperCompletion=true;
 il2cpp_class_get_fields=HelperListFields;il2cpp_field_get_name=HelperListFieldName;
 il2cpp_field_get_value=HelperFields;RuntimeShapes::helperRenderersField=helperFieldValue;
 RuntimeShapes::originalHelperInit=[](void*,void*){++helperInitCalls;};
 rendererScanCalls.clear();A::reconcileClock=NativeBundle::Clock();
 RuntimeShapes::HelperInitialized(Test::unrelatedHelper,nullptr);
 il2cpp_field_get_value=fallbackFieldValue;il2cpp_class_get_fields=fallbackFields;il2cpp_field_get_name=fallbackFieldName;RuntimeShapes::helperRenderersField=previousHelperField;RuntimeShapes::originalHelperInit=previousHelper;helperCompletion=false;
 A::Frame();Require(helperInitCalls==1&&A::bindings.size()==2,"helper-only NPC completed its Mesh controls but never received an effect owner");
 Require(rendererScanCalls[Test::unrelatedRoot]==1&&rendererScanCalls[Test::oldRoot]==0,"new helper owner caused a full scan instead of registering only its root");
 auto roots=D::Owners();Require(roots.count(FullResourceReload::Id(Test::unrelatedRoot)),"F9 omitted the helper-only NPC owner");roots.clear();
 auto ownerRevision=FullResourceReload::ownerRevision;rendererScanCalls.clear();A::reconcileClock=NativeBundle::Clock();FullResourceReload::HelperReady(Test::unrelatedHelper);A::Frame();
 Require(FullResourceReload::ownerRevision==ownerRevision&&A::bindings.size()==2&&rendererScanCalls[Test::unrelatedRoot]==1&&rendererScanCalls[Test::oldRoot]==0,"repeated helper init scanned unrelated owners or duplicated the NPC lifecycle");
 rendererScanCalls.clear();A::reconcileClock=NativeBundle::Clock();FullResourceReload::HelperReady((void*)0xFA13);A::Frame();
 Require(A::bindings.size()==2&&rendererScanCalls[Test::unrelatedRoot]==1&&rendererScanCalls[Test::oldRoot]==0,"second helper on one NPC scanned unrelated model roots");
 rendererScanCalls.clear();A::reconcileClock=NativeBundle::Clock();FullResourceReload::HelperReady((void*)0xFA14);++RuntimeControls::revision;A::Frame();
 Require(rendererScanCalls[Test::unrelatedRoot]==1&&rendererScanCalls[Test::oldRoot]==0,"unchanged effect definitions triggered a full owner reconciliation after a control graph revision");
 rendererScanCalls.clear();A::reconcileClock=NativeBundle::Clock();A::pendingStandalone=std::make_shared<const std::vector<Package>>(*A::standalone);A::pendingEffects[p.id]=p.effects;A::pendingForceReload=true;A::reloadPending=true;A::Frame();
 Require(rendererScanCalls[Test::unrelatedRoot]==1&&rendererScanCalls[Test::oldRoot]==1,"explicit F9 did not force a full owner reconciliation");
 rendererScanCalls.clear();A::reconcileClock=NativeBundle::Clock();auto savedReleaseForGraphChange=FullResourceReload::originalHelperRelease;FullResourceReload::originalHelperRelease=[](void*,void*){};FullResourceReload::HelperReleased((void*)0xFA14,nullptr);FullResourceReload::originalHelperRelease=savedReleaseForGraphChange;A::Frame();
 std::ostringstream releaseScans;for(const auto& row:rendererScanCalls)releaseScans<<" owner="<<row.first<<" scans="<<row.second;
 Require(rendererScanCalls.empty(),"releasing a settled duplicate helper rescanned model roots:"+releaseScans.str());
 ownerTraversalChecks=0;A::reconcileClock=0;A::Frame();
 Require(ownerTraversalChecks==0,"periodic recovery enumerated owners with no unresolved effect mount");
 auto savedHelperRelease=FullResourceReload::originalHelperRelease;FullResourceReload::originalHelperRelease=[](void*,void*){};
 FullResourceReload::HelperReleased((void*)0xFA13,nullptr);FullResourceReload::originalHelperRelease=savedHelperRelease;
 Require(A::bindings.size()==2,"releasing one helper retired a model still owned by another helper");
 EffectTest::rejectNextUIRenderSet=true;Test::uiOwned.insert(Test::newRoot);FullResourceReload::RecordUIModel(Test::uiLoader,Test::newRoot,"assets/ui-model.prefab",nullptr,nullptr);
 Require(A::bindings.size()==3,"world/NPC/UI roots did not receive independent effect bindings");
 auto tick=[](){for(unsigned i=0;i<12;++i){for(auto& r:Test::scheduleDone)r.second=true;Test::done=1;A::Frame();D::Collect();}};tick();
 auto& npc=A::bindings.at({Test::unrelatedRoot,p.id,effect.id,Test::left});auto& ui=A::bindings.at({Test::newRoot,p.id,effect.id,Test::bip});
 Require(world.instance&&npc.instance&&ui.instance&&world.generation==npc.generation&&npc.generation==ui.generation,"model instances failed to share native resources with independent playback");
 Require(world.instance->object.Get()!=npc.instance->object.Get()&&npc.instance->object.Get()!=ui.instance->object.Get()&&npc.parent.Get()==Test::left&&ui.parent.Get()==Test::bip,"effect used another model instance's skeleton or clone");
 Require(EffectTest::renderInUI[EffectTest::particleRenderers.at(ui.instance->particles.front().Get())],"UI render flag did not recover after activation retry");
 auto uiObject=ui.instance->object.Get();auto oldUi=ui.instance;auto writes=EffectTest::layerWrites;A::Frame();A::Frame();Require(EffectTest::layerWrites==writes,"unchanged effect layers were rewritten every frame");
 auto uiParticle=ui.instance->particles.front().Get(),worldParticle=world.instance->particles.front().Get(),npcParticle=npc.instance->particles.front().Get();
 Require(EffectTest::renderInUI[EffectTest::particleRenderers.at(uiParticle)]&&!EffectTest::renderInUI[EffectTest::particleRenderers.at(worldParticle)]&&!EffectTest::renderInUI[EffectTest::particleRenderers.at(npcParticle)],"UI render retry changed the world/NPC renderer or failed to enable the UI renderer");
 EffectTest::layers[Test::auxiliaryRenderer]=22;A::Frame();Require(ui.instance==oldUi&&EffectTest::layers.at(uiObject)==22&&EffectTest::layers.at(EffectTest::effectChildren.at(uiObject))==22&&EffectTest::layers.at(world.instance->object.Get())==8,"late UI layer assignment did not follow owner or affected world instance");
 // Game scene cleanup can destroy a registered clone without releasing its
 // model. Only that owner must recover, including after an unchanged F9.
 auto worldBefore=world.instance,npcBefore=npc.instance;
 EffectTest::dead.insert(uiObject);Test::transformParents.erase(uiObject);Test::transformParents.erase(EffectTest::effectChildren.at(uiObject));tick();
 Require(ui.instance&&ui.instance!=oldUi&&RuntimeShapes::Alive(ui.instance->object.Get())&&world.instance==worldBefore&&npc.instance==npcBefore,"externally destroyed UI clone was not recovered independently");
 auto disabledUi=ui.instance;EffectTest::active[ui.instance->object.Get()]=false;tick();
 Require(ui.instance&&ui.instance!=disabledUi&&EffectTest::active[ui.instance->object.Get()]&&world.instance==worldBefore&&npc.instance==npcBefore,"externally disabled UI clone did not recover independently");
 auto uiBeforeReload=ui.instance;
 A::pendingStandalone=std::make_shared<const std::vector<Package>>();A::pendingEffects[p.id]=p.effects;A::pendingForceReload=true;A::reloadPending=true;tick();
 Require(ui.instance==uiBeforeReload&&world.instance==worldBefore&&npc.instance==npcBefore,"unchanged F9 interrupted world/NPC/UI playback");
 auto refreshed=effect;refreshed.native=EffectTest::Manifest('6');
 A::pendingStandalone=std::make_shared<const std::vector<Package>>();A::pendingEffects[p.id]={{refreshed.id,refreshed}};A::pendingForceReload=true;A::reloadPending=true;tick();
 Require(world.instance!=worldBefore&&npc.instance!=npcBefore&&ui.instance!=uiBeforeReload&&world.generation==ui.generation&&world.generation==npc.generation,"changed F9 did not update all world/NPC/UI instances independently");
 hiddenRenderers.insert(Test::newRenderer);A::Frame();Require(!world.instance&&npc.instance&&ui.instance,"visible sibling defeated mounted Part visibility or hid unrelated model effects");hiddenRenderers.clear();tick();
 auto worldInstance=world.instance,uiInstance=ui.instance;auto previousHelperRelease=FullResourceReload::originalHelperRelease;FullResourceReload::originalHelperRelease=[](void*,void*){};
 FullResourceReload::HelperReleased(Test::unrelatedHelper,nullptr);FullResourceReload::originalHelperRelease=previousHelperRelease;FullResourceReload::api.componentGo=previousComponentGo;Test::currentRoot=Test::oldRoot;
 Require(A::bindings.size()==2&&world.instance==worldInstance&&ui.instance==uiInstance,"NPC pool release destroyed world/UI effects");
 // Real per-target provenance, rather than reusing LOD0's marker. LODs can
 // arrive after model completion and must only extend the visibility set.
 std::vector<void*> lods;
 for(unsigned level=1;level<=3;++level){auto renderer=(void*)(uintptr_t(0xEC00)+level),mesh=(void*)(uintptr_t(0xED00)+level);lods.push_back(renderer);
  ownerRenderers[Test::oldRoot].push_back(renderer);rendererMeshes[renderer]=mesh;rendererBones[renderer]={Test::bone,Test::right};EffectTest::layers[renderer]=8;
  auto spec=exact;spec.marker=RuntimePartName(p,"PartPetals",rule.targets.at(level));meshNames[mesh]=spec.marker;RuntimeControls::specs[spec.marker]=spec;
 }
 auto cloneCount=Test::cloneCalls;A::ModelReady(Test::oldRoot);tick();
 Require(world.renderers.size()==4&&world.instance==worldInstance&&Test::cloneCalls==cloneCount,"late LOD registration restarted or duplicated the running effect");
 for(auto selected:lods){hiddenRenderers.insert(Test::newRenderer);for(auto renderer:lods)hiddenRenderers.insert(renderer);hiddenRenderers.erase(selected);A::Frame();
  Require(world.instance==worldInstance&&ui.instance==uiInstance&&Test::cloneCalls==cloneCount,"LOD0/1/2/3 visibility switch interrupted playback or another owner");}
 for(auto renderer:lods)hiddenRenderers.insert(renderer);A::Frame();Require(!world.instance&&ui.instance==uiInstance,"all hidden LODs left a mounted effect visible");
 hiddenRenderers.clear();tick();worldInstance=world.instance;cloneCount=Test::cloneCalls;
 ownerRenderers[Test::oldRoot]={Test::oldRenderer,lods.back()};A::ModelReady(Test::oldRoot);tick();
 Require(world.renderers.size()==1&&world.instance==worldInstance&&Test::cloneCalls==cloneCount,"LOD3-only owner required LOD0 or reset its particles");
 ownerRenderers[Test::oldRoot]={Test::oldRenderer,Test::newRenderer};A::ModelReady(Test::oldRoot);tick();
 Require(world.instance==worldInstance,"returning to LOD0 restarted playback");
 // Two real skeleton copies under one root need two clones. Another renderer
 // on the first skeleton is only a LOD/part sibling, not a third emitter.
 auto copyRoot=(void*)0xEA00,copyBone=(void*)0xEA01,copyMount=(void*)0xEA02,copyRenderer=(void*)0xEA03,lodRenderer=(void*)0xEA04;
 Test::transformParents[copyRoot]=Test::oldRoot;Test::transformParents[copyBone]=copyRoot;Test::transformParents[copyMount]=copyRoot;
 nodeNames[copyRoot]="Copy";nodeNames[copyBone]="Bone";nodeNames[copyMount]="Right";
 ownerRenderers[Test::oldRoot].insert(ownerRenderers[Test::oldRoot].end(),{copyRenderer,lodRenderer});
 rendererMeshes[copyRenderer]=rendererMeshes[lodRenderer]=Test::newMesh;
 rendererBones[copyRenderer]={copyBone,copyMount};rendererBones[lodRenderer]={Test::bone,Test::right};
 EffectTest::layers[copyRenderer]=22;EffectTest::layers[lodRenderer]=8;
 A::ModelReady(Test::oldRoot);tick();Require(A::bindings.size()==3,"two skeletons under one model root collapsed into one effect binding");
 auto& copy=A::bindings.at({Test::oldRoot,p.id,effect.id,copyMount});auto copyInstance=copy.instance;
 Require(copyInstance&&copyInstance!=world.instance&&copy.parent.Get()==copyMount&&world.renderers.size()==2&&copy.renderers.size()==1,"LOD grouping duplicated effects or selected another skeleton");
 hiddenRenderers.insert(Test::newRenderer);hiddenRenderers.insert(lodRenderer);A::Frame();
 Require(!world.instance&&copy.instance==copyInstance&&ui.instance==uiInstance,"skeleton copy visibility was coupled to another copy");hiddenRenderers.clear();tick();
 ownerRenderers[Test::oldRoot]={Test::oldRenderer,Test::newRenderer};A::ModelReady(Test::oldRoot);tick();
 Require(A::bindings.size()==2&&copyInstance->destroyAttempted,"removed skeleton copy retained its effect");
 worldInstance=world.instance;
 FullResourceReload::UIUnload(Test::uiLoader,Test::newRoot,nullptr);Require(A::bindings.size()==1&&world.instance==worldInstance,"UI unload destroyed world effect");
 // Skeleton replacement can happen after the last owner callback. A missing
 // binding must be retried on known game-owned models even with no revision.
 EffectTest::dead.insert(Test::right);A::Frame();Require(A::bindings.empty(),"retired skeleton retained old effect binding");
 auto newMount=(void*)0xEB00;nodeNames[newMount]="Right";Test::transformParents.erase(Test::right);Test::transformParents[newMount]=Test::oldRoot;
 rendererBones[Test::newRenderer]={Test::bone,newMount};A::reconcileClock=0;tick();
 Require(A::bindings.size()==1&&A::bindings.begin()->second.parent.Get()==newMount&&A::bindings.begin()->second.instance,"late skeleton rebuild was not rediscovered without an owner revision");A::OwnerReleasing(Test::oldRoot);
 RuntimeShapes::bound.clear();RuntimeShapes::boundCount.store(0);RuntimeShapes::enabled=previousEnabled;RuntimeShapes::packageGeneration=previousPackageGeneration;FullResourceReload::enabled=previousReloadEnabled;FullResourceReload::liveHelperModels.clear();
 RuntimeControls::controlGraph.reset();RuntimeControls::variables.clear();RuntimeControls::specs.clear();RuntimeShapes::originalMeshGetter=previousGetter;FullResourceReload::originalCloneParent=previousClone;il2cpp_object_get_class=previousClass;Test::transformParents=std::move(previousParents);splitOwners=false;EffectTest::requiredLayers.clear();EffectTest::effectChildren.clear();
 ownerRenderers.clear();rendererBones.clear();meshNames.clear();nodeNames.clear();rendererMeshes.clear();
 std::cout<<"PASS split-source effect Part ownership: different bone palettes, independent world/NPC/UI instances, shared resources, visibility and separate release\n";
}
void Checks(){
 il2cpp_runtime_invoke=Invoke;RuntimeShapes::api.getEnabled=(void*)920;D::api.activeHierarchy=(void*)919;EffectTest::dead.clear();EffectTest::badName=false;EffectTest::foreignBundle=false;Test::done=1;
 auto original=EffectTest::Manifest('1');Package p;p.id="AttachmentMod";p.parts["PartBody"].mesh="MeshBody";p.meshes["MeshBody"].bones={"Bone"};p.defaults["$fx_test"]=1;p.controls["$fx_test"]={"effect",0,1};
 EffectAttachment effect;effect.id="test";effect.label="test";effect.part="PartBody";effect.variable="$fx_test";effect.native=original;p.effects.emplace(effect.id,effect);
 Test::transformParents[Test::right]=Test::oldRoot;auto boneEffect=effect;boneEffect.bone="Right";Require(A::Parent(Test::oldRoot,Test::oldRenderer,p,boneEffect)==Test::right,"effect cannot mount a skeleton bone absent from owner Mesh palette");
 RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>(std::vector<Package>{p});RuntimeControls::variables[p.id]=p.defaults;
 RuntimeControls::Spec spec;spec.package=p.id;spec.part="PartBody";spec.marker="marker";RuntimeControls::specs["marker"]=spec;
 A::revision=RuntimeControls::revision;A::ModelReady(Test::oldRoot);Require(A::bindings.size()==1,"effect owner marker did not bind exact Mod part");
 auto tick=[](){for(unsigned frame=0;frame<12;++frame){for(auto& row:Test::scheduleDone)row.second=true;Test::done=1;A::Frame();D::Collect();}};tick();auto& b=A::bindings.begin()->second;
 Require(b.instance&&b.instance->owner.Get()==Test::oldRoot&&EffectTest::cloneParents.at(b.instance->object.Get())==Test::oldRoot,"effect did not mount independent instance at owner root");auto old=b.instance;
 auto replacement=effect;replacement.native=EffectTest::Manifest('2');A::Reload(p.id,replacement);Require(b.instance==old,"effect reload retired visible instance before replacement ready");tick();Require(b.instance!=old&&old->removed&&old->destroyAttempted,"effect reload failed transactional instance swap");
 auto retained=b.instance;auto broken=effect;auto brokenNative=std::const_pointer_cast<D::Manifest>(EffectTest::Manifest('3'));
 auto secondBundle=brokenNative->bundles.front();secondBundle.file="runtime/bundles/second.ab";brokenNative->bundles.push_back(secondBundle);broken.native=brokenNative;
 A::Reload(p.id,broken);for(unsigned i=0;i<10;++i)A::Frame();auto rejectedGeneration=b.generation;
 Require(rejectedGeneration->bundles.size()==2&&rejectedGeneration->bundles[0].request.Get()&&rejectedGeneration->bundles[1].request.Get(),"retry fixture did not submit both native archives");
 auto failedRequest=rejectedGeneration->bundles[0].request.Get(),pendingRequest=rejectedGeneration->bundles[1].request.Get();Test::scheduleBundles[failedRequest]=nullptr;Test::scheduleDone[failedRequest]=true;A::Frame();
 Require(b.failed&&b.instance==retained&&!retained->destroyAttempted,"failed native preparation destroyed last visible effect");auto beforeRetry=Test::opened;
 A::pendingStandalone=std::make_shared<const std::vector<Package>>();A::pendingEffects[p.id]={{broken.id,broken}};A::pendingForceReload=true;A::reloadPending=true;A::Frame();
 Require(Test::opened==beforeRetry&&b.instance==retained&&!rejectedGeneration->finished,"F9 reloaded the same CAB namespace before pending native requests drained");
 Test::scheduleDone[pendingRequest]=true;tick();
 Require(b.instance&&b.instance!=retained&&b.generation==rejectedGeneration&&b.generation->phase==D::Phase::Ready&&retained->destroyAttempted,"F9 could not retry identical effect content after an async native failure drained");
 auto recovered=effect;recovered.native=EffectTest::Manifest('4');A::Reload(p.id,recovered);tick();Require(b.instance!=retained&&retained->destroyAttempted,"valid export failed to recover rejected effect reload");
 auto current=b.instance;auto rejected=effect;rejected.part="PartOther";bool failed=false;try{A::Reload(p.id,rejected);}catch(...){failed=true;}Require(failed&&b.instance==current,"changing owner bypassed export/F10 target validation");
 RuntimeControls::variables[p.id][effect.variable]=0;A::Frame();Require(!b.instance&&current->destroyAttempted,"effect control failed to stop its instance");
 RuntimeControls::variables[p.id][effect.variable]=1;tick();Require(bool(b.instance),"effect control did not resume resident native generation");
 rendererActive=false;A::Frame();Require(!b.instance,"inactive owner Mesh GameObject retained effect");rendererActive=true;tick();Require(bool(b.instance),"visible owner Mesh did not resume effect");
 rendererEnabled=false;A::Frame();Require(!b.instance,"disabled owner Mesh renderer retained effect");rendererEnabled=true;tick();Require(bool(b.instance),"re-enabled owner Mesh did not resume effect");
 // F9 publishes complete declaration maps: an empty one must remain empty on later model events.
 A::pendingStandalone=std::make_shared<const std::vector<Package>>();A::pendingEffects[p.id]={};A::pendingForceReload=true;A::reloadPending=true;A::Frame();Require(A::bindings.empty(),"F9 deletion retained removed effect");A::ModelReady(Test::oldRoot);Require(A::bindings.empty(),"model event resurrected effect deleted by F9");
 A::pendingStandalone=std::make_shared<const std::vector<Package>>();auto mounted=effect;mounted.bone="Right";A::pendingEffects[p.id]={{mounted.id,mounted}};A::pendingForceReload=true;A::reloadPending=true;A::Frame();tick();Require(A::bindings.size()==1&&A::bindings.begin()->second.parent.Get()==Test::right,"F9 did not add/rebind full skeleton mount");
 // Standalone Mod binds a verified original renderer with no character graph/specs.
 A::OwnerReleasing(Test::oldRoot);A::diskEffects.clear();RuntimeControls::controlGraph=std::make_shared<const std::vector<Package>>();RuntimeControls::specs.clear();RuntimeControls::variables.clear();
 auto independent=effect;independent.part="";independent.bone="Right";independent.targetPrefab=Test::modelPath;independent.rendererPath="marker";independent.targetAsset="marker";independent.targetPath="assets/body.mesh";independent.targetBones={"Bone"};p.effects={{effect.id,independent}};
 Test::transformParents[Test::oldRenderer]=Test::oldRoot;A::standalone=std::make_shared<const std::vector<Package>>(std::vector<Package>{p});A::needed=true;A::ModelReady(Test::oldRoot);tick();Require(A::bindings.size()==1&&A::bindings.begin()->second.instance,"standalone effect required character Mod registration");
 A::pendingStandalone=std::make_shared<const std::vector<Package>>();A::pendingEffects.clear();A::pendingForceReload=true;A::reloadPending=true;A::Frame();Require(A::bindings.empty(),"removed standalone Mod retained its instance");
 A::OwnerReleasing(Test::oldRoot);Require(A::bindings.empty(),"effect retained instance after owner entered game pool");
 SplitOwnerChecks();
 for(auto object:EffectTest::destroyed)EffectTest::dead.insert(object);D::Collect();A::overrides.clear();RuntimeControls::controlGraph.reset();RuntimeControls::variables.clear();RuntimeControls::specs.clear();il2cpp_runtime_invoke=EffectTest::Invoke;
 std::cout<<"PASS effect Mod attachments: exact mesh owner, native prepare, independent clone, transactional live update, target validation, controls and detach before pooling\n";
}
}
