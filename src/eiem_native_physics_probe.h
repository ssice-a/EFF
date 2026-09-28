#pragma once
#include <climits>
#include "eiem_native_physics_api.h"
#include "eiem_native_physics_events.h"
#include "eiem_native_physics_contract_probe.h"

// On-demand read-only observation, never a retirement fence.

// Only managed lists are read here. Do not dereference NativeArray/Job buffers
// while the solver may own them, and do not infer layouts from metadata indices.
struct EiemPhysicsProbeList {
  EiemUnityRef object;
  void *item=nullptr, *countMethod=nullptr, *field=nullptr;
  int32_t count=0;
  bool Read(void *owner,void *ownerClass,const char *fieldName,const char *elementType) {
    std::string type="System.Collections.Generic.List<"; type+=elementType; type+='>';
    void *value=nullptr;
    field=EiemPhysicsField(ownerClass,fieldName,type.c_str());
    if (!EiemPhysicsReadField(owner,field,value) || !value)
      return false;
    object=EiemUnityRef::Capture(value,false);
    if (!object) return false;
    void *klass=il2cpp_object_get_class(value);
    countMethod=EiemPhysicsMethod(klass,"get_Count","System.Int32",false);
    item=EiemPhysicsMethod(klass,"get_Item",elementType,false,"System.Int32");
    return item && EiemPhysicsReadValue(value,countMethod,count) && count>=0;
  }
  bool Item(int32_t index,void *&value) const {
    void *args[]={&index}; value=nullptr;
    return EiemOnUnityThread() && index>=0 && index<count && InvokeChecked(item,object.Target(),args,&value);
  }
  bool Integer(int32_t index,int32_t &value) const {
    void *boxed=nullptr;
    return Item(index,boxed) && EiemPhysicsUnbox(boxed,value);
  }
  bool BindingAndCountUnchanged(void *owner) const {
    int32_t after=-1; void *current=nullptr;
    return EiemPhysicsReadField(owner,field,current) && current==object.Target() &&
      EiemPhysicsReadValue(current,countMethod,after) && after==count;
  }
};

static void EiemPhysicsWriteBoneSetup(FILE *file,void *process,void *processClass,
                                     void *setupClass,void *idMethod) {
  fputs(",\"boneSetup\":{",file);
  bool building=false,destroying=false;
  void *buildField=EiemPhysicsField(processClass,"isBuild","System.Boolean");
  void *destroyField=EiemPhysicsField(processClass,"isDestory","System.Boolean");
  if (!EiemPhysicsReadField(process,buildField,building) ||
      !EiemPhysicsReadField(process,destroyField,destroying) || building || destroying) {
    fputs("\"status\":\"unavailable-or-transitioning\"}",file); return;
  }
  void *setup=nullptr;
  if (!setupClass || !EiemPhysicsReadField(process,
      EiemPhysicsField(processClass,"boneClothSetupData","BeyondDynamicBone.RenderSetupData"),setup)) {
    fputs("\"status\":\"setup-field-unavailable\"}",file); return;
  }
  if (!setup) { fputs("\"status\":\"no-bone-setup\"}",file); return; }
  auto holdSetup=EiemUnityRef::Capture(setup,false);
  if (!holdSetup || il2cpp_object_get_class(setup)!=setupClass) {
    fputs("\"status\":\"setup-contract-unavailable\"}",file); return;
  }
  EiemPhysicsProbeList transforms,ids,parents,roots;
  if (!transforms.Read(setup,setupClass,"transformList","UnityEngine.Transform") ||
      !ids.Read(setup,setupClass,"transformIdList","System.Int32") ||
      !parents.Read(setup,setupClass,"transformParentIdList","System.Int32") ||
      !roots.Read(setup,setupClass,"rootTransformIdList","System.Int32")) {
    fputs("\"status\":\"managed-bone-lists-unavailable\"}",file); return;
  }
  if (ids.count!=transforms.count || parents.count!=transforms.count) {
    fprintf(file,"\"status\":\"bone-list-length-mismatch\",\"transforms\":%d,\"ids\":%d,\"parents\":%d}",
            transforms.count,ids.count,parents.count); return;
  }
  void *indexMethod=EiemPhysicsMethod(setupClass,"GetTransformIndexFromId","System.Int32",false,"System.Int32");
  fputs("\"status\":\"observed-not-atomic\",\"indexSpace\":\"native-bone-setup-not-mesh-palette\",",file);
  for (const char *name:{"skinBoneCount","renderTransformIndex"}) {
    int32_t value=0; EiemWriteJsonString(file,name); fputc(':',file);
    if (EiemPhysicsReadField(setup,EiemPhysicsField(setupClass,name,"System.Int32"),value)) fprintf(file,"%d",value);
    else fputs("null",file);
    fputc(',',file);
  }
  fputs("\"rootTransformIds\":[",file);
  for (int32_t i=0;i<roots.count;++i) {
    if (i) fputc(',',file);
    int32_t id=0;
    if (roots.Integer(i,id)) fprintf(file,"%d",id); else fputs("null",file);
  }
  fprintf(file,"],\"transformCount\":%d,\"nodes\":[",transforms.count);
  for (int32_t i=0;i<transforms.count;++i) {
    if (i) fputc(',',file);
    fprintf(file,"{\"setupIndex\":%d",i);
    int32_t recorded=0,parent=0,actual=0;
    bool hasRecorded=ids.Integer(i,recorded);
    fputs(",\"recordedId\":",file);
    if (hasRecorded) fprintf(file,"%d",recorded); else fputs("null",file);
    fputs(",\"parentRecordedId\":",file);
    if (parents.Integer(i,parent)) fprintf(file,"%d",parent); else fputs("null",file);
    void *transform=nullptr;
    bool got=transforms.Item(i,transform);
    auto hold=EiemUnityRef::Capture(transform,false);
    int alive=got && hold ? hold.Status() : (got && !transform ? 0 : -1);
    bool hasActual=alive==1 && EiemPhysicsReadValue(transform,idMethod,actual);
    fprintf(file,",\"nativeAlive\":%d,\"instanceId\":",alive);
    if (hasActual) fprintf(file,"%d",actual); else fputs("null",file);
    fputs(",\"idMatches\":",file);
    fputs(hasRecorded && hasActual ? (recorded==actual ? "true":"false") : "null",file);
    fputs(",\"nativeLookupIndex\":",file);
    void *boxed=nullptr; void *args[]={&actual};
    if (hasActual && indexMethod && InvokeChecked(indexMethod,setup,args,&boxed) && boxed) {
      int32_t value=0;
      // Reuse the checked value unboxing path; this is a managed List.IndexOf,
      // not a native-buffer index read or registration call.
      if (EiemPhysicsUnbox(boxed,value)) fprintf(file,"%d",value); else fputs("null",file);
    } else fputs("null",file);
    char hierarchy[2048]={};
    if (alive==1) TraceBuildRendererHierarchy(transform,hierarchy,sizeof(hierarchy));
    fputs(",\"hierarchyLabel\":",file); EiemWriteJsonString(file,hierarchy);
    fputc('}',file);
  }
  bool afterBuild=false,afterDestroy=false; void *afterSetup=nullptr;
  bool unchanged=transforms.BindingAndCountUnchanged(setup) && ids.BindingAndCountUnchanged(setup) &&
    parents.BindingAndCountUnchanged(setup) && roots.BindingAndCountUnchanged(setup) &&
    EiemPhysicsReadField(process,buildField,afterBuild) && EiemPhysicsReadField(process,destroyField,afterDestroy) &&
    !afterBuild && !afterDestroy && EiemPhysicsReadField(process,
      EiemPhysicsField(processClass,"boneClothSetupData","BeyondDynamicBone.RenderSetupData"),afterSetup) && afterSetup==setup;
  // Equal bindings, lengths and flags are not an atomic snapshot (contents could change).
  fprintf(file,"],\"sameBindingsAndLengthsAtEnd\":%s}",unchanged?"true":"false");
}

static void EiemPhysicsWriteContract(FILE *file, void *klass, const char *name) {
  fputs("{\"type\":", file); EiemWriteJsonString(file, name);
  fprintf(file, ",\"available\":%s,\"fields\":[", klass ? "true" : "false");
  void *iterator = nullptr, *field = nullptr; bool first = true;
  while (klass && (field = il2cpp_class_get_fields(klass, &iterator))) {
    if (!first) fputc(',', file); first = false;
    fputs("{\"name\":", file); EiemWriteJsonString(file, il2cpp_field_get_name(field));
    fputs(",\"type\":", file);
    EiemWriteJsonString(file, EiemPhysicsTypeName(il2cpp_field_get_type(field)).c_str());
    fprintf(file, ",\"flags\":%d}", il2cpp_field_get_flags(field));
  }
  fputs("],\"methods\":[", file); iterator = nullptr; first = true;
  void *method = nullptr;
  while (klass && (method = il2cpp_class_get_methods(klass, &iterator))) {
    if (!first) fputc(',', file); first = false;
    fputs("{\"name\":", file); EiemWriteJsonString(file, il2cpp_method_get_name(method));
    fputs(",\"returns\":", file);
    EiemWriteJsonString(file, EiemPhysicsTypeName(il2cpp_method_get_return_type(method)).c_str());
    uint32_t impl = 0;
    fprintf(file, ",\"flags\":%u,\"parameters\":[", il2cpp_method_get_flags(method, &impl));
    for (uint32_t p = 0; p < il2cpp_method_get_param_count(method); ++p) {
      if (p) fputc(',', file);
      EiemWriteJsonString(file, EiemPhysicsTypeName(il2cpp_method_get_param(method, p)).c_str());
    }
    fputs("]}", file);
  }
  fputs("]}", file);
}

static void EiemPhysicsWriteSelection(FILE *file,void *component,void *componentClass,
                                      void *data2Class,void *selectionClass,
                                      void *vertexClass,void *arrayGetItem,
                                      bool includeStates) {
  fputs(",\"selection\":{",file);
  void *getData2=EiemPhysicsMethod(componentClass,"GetSerializeData2",
    "BeyondDynamicBone.ClothSerializeData2",false);
  if (!data2Class || !selectionClass || !vertexClass || !arrayGetItem || !getData2) {
    fputs("\"status\":\"contract-unavailable\"}",file); return;
  }
  void *data2=nullptr;
  if (!InvokeChecked(getData2,component,nullptr,&data2) || !data2 ||
      il2cpp_object_get_class(data2)!=data2Class) {
    fputs("\"status\":\"data2-unavailable\"}",file); return;
  }
  auto keepData2=EiemUnityRef::Capture(data2,false);
  void *selection=nullptr;
  void *selectionField=EiemPhysicsField(data2Class,"selectionData","BeyondDynamicBone.SelectionData");
  if (!keepData2 || !EiemPhysicsReadField(data2,selectionField,selection) || !selection ||
      il2cpp_object_get_class(selection)!=selectionClass) {
    fputs("\"status\":\"selection-unavailable\"}",file); return;
  }
  auto keepSelection=EiemUnityRef::Capture(selection,false);
  void *attributes=nullptr;
  void *attributesField=EiemPhysicsField(selectionClass,"attributes","BeyondDynamicBone.VertexAttribute[]");
  void *countMethod=EiemPhysicsMethod(selectionClass,"get_Count","System.Int32",false);
  int32_t count=-1;
  if (!keepSelection || !EiemPhysicsReadValue(selection,countMethod,count) || count<0 || count>16384 ||
      !EiemPhysicsReadField(selection,attributesField,attributes) || !attributes ||
      il2cpp_array_length(attributes)!=(uintptr_t)count) {
    fputs("\"status\":\"attribute-array-unavailable\"}",file); return;
  }
  auto keepAttributes=EiemUnityRef::Capture(attributes,false);
  void *isFixed=EiemPhysicsMethod(vertexClass,"IsFixed","System.Boolean",false);
  void *isMove=EiemPhysicsMethod(vertexClass,"IsMove","System.Boolean",false);
  void *isInvalid=EiemPhysicsMethod(vertexClass,"IsInvalid","System.Boolean",false);
  if (!keepAttributes || !isFixed || !isMove || !isInvalid) {
    fputs("\"status\":\"attribute-method-unavailable\"}",file); return;
  }
  int32_t fixed=0,move=0,invalid=0,other=0,unreadable=0;
  std::vector<const char *> states;
  if (includeStates) states.reserve((size_t)count);
  for (int32_t index=0;index<count;++index) {
    void *boxed=nullptr,*args[]={&index};
    bool fixedValue=false,moveValue=false,invalidValue=false;
    const char *state="unreadable";
    struct AttributePin {
      uint32_t handle=0;
      ~AttributePin() { if (handle) il2cpp_gchandle_free(handle); }
    } pin;
    if (InvokeChecked(arrayGetItem,attributes,args,&boxed) && boxed &&
        il2cpp_object_get_class(boxed)==vertexClass && il2cpp_gchandle_new &&
        il2cpp_gchandle_get_target && il2cpp_gchandle_free &&
        (pin.handle=il2cpp_gchandle_new(boxed,true))!=0) {
      void *value=EiemPhysicsProbeValueAddress(
          il2cpp_gchandle_get_target(pin.handle));
      if (value && EiemPhysicsReadValue(value,isFixed,fixedValue) &&
          EiemPhysicsReadValue(value,isMove,moveValue) &&
          EiemPhysicsReadValue(value,isInvalid,invalidValue)) {
      if (invalidValue) {++invalid; state="invalid";}
      else if (fixedValue) {++fixed; state="fixed";}
      else if (moveValue) {++move; state="move";}
      else {++other; state="other";}
      } else ++unreadable;
    } else ++unreadable;
    if (includeStates) states.push_back(state);
  }
  fprintf(file,"\"status\":\"observed-not-atomic\",\"count\":%d,\"fixed\":%d,\"move\":%d,\"invalid\":%d,\"other\":%d,\"unreadable\":%d",
          count,fixed,move,invalid,other,unreadable);
  if (includeStates) {
    fputs(",\"states\":[",file);
    for (size_t index=0;index<states.size();++index) {
      if (index) fputc(',',file);
      EiemWriteJsonString(file,states[index]);
    }
    fputc(']',file);
  }
  void *after=nullptr;
  bool unchanged=EiemPhysicsReadField(selection,attributesField,after) && after==attributes &&
    il2cpp_array_length(after)==(uintptr_t)count;
  fprintf(file,",\"sameAttributeArrayAtEnd\":%s}",unchanged?"true":"false");
}

static void EiemPhysicsWriteState(FILE *file, void *component, void *componentClass,
                                   void *processClass,void *setupClass,void *idMethod,void *resultClass,
                                   void *data2Class,void *selectionClass,void *vertexClass,
                                   void *arrayGetItem,bool includeSelectionStates) {
  void *process = nullptr;
  void *field = EiemPhysicsField(componentClass, "process", "BeyondDynamicBone.ClothProcess");
  if (!EiemPhysicsReadField(component, field, process)) {
    fputs("\"status\":\"process-field-unavailable\"", file); return;
  }
  if (!process) { fputs("\"status\":\"no-process\"", file); return; }
  auto keepProcess = EiemUnityRef::Capture(process, false);
  if (!keepProcess || il2cpp_object_get_class(process) != processClass) {
    fputs("\"status\":\"process-contract-unavailable\"", file); return;
  }
  fprintf(file, "\"status\":\"observed-not-atomic\",\"process\":\"%p\"", process);
  for (const char *name : {"isBuild", "isDestory", "isDestoryInternal"}) {
    bool value = false;
    void *f = EiemPhysicsField(processClass, name, "System.Boolean");
    fputc(',', file); EiemWriteJsonString(file, name); fputc(':', file);
    fputs(EiemPhysicsReadField(process, f, value) ? (value ? "true" : "false") : "null", file);
  }
  for (const char *name : {"IsValid", "IsRunning"}) {
    bool value = false;
    void *m = EiemPhysicsMethod(processClass, name, "System.Boolean", false);
    fputc(',', file); EiemWriteJsonString(file, name); fputc(':', file);
    fputs(EiemPhysicsReadValue(process, m, value) ? (value ? "true" : "false") : "null", file);
  }
  int32_t team = 0;
  fputs(",\"teamId\":", file);
  if (EiemPhysicsReadValue(process, EiemPhysicsMethod(processClass, "get_TeamId", "System.Int32", false), team))
    fprintf(file, "%d", team);
  else fputs("null", file);
  EiemPhysicsWriteBuildResult(file,process,processClass,resultClass);
  EiemPhysicsWriteBoneSetup(file,process,processClass,setupClass,idMethod);
  EiemPhysicsWriteSelection(file,component,componentClass,data2Class,selectionClass,
                            vertexClass,arrayGetItem,includeSelectionStates);
  void *animator=nullptr;
  fputs(",\"interlockingAnimatorId\":",file);
  if (EiemPhysicsReadField(process,EiemPhysicsField(processClass,"interlockingAnimator","UnityEngine.Animator"),animator) && animator) {
    auto holdAnimator=EiemUnityRef::Capture(animator,false); int32_t id=0;
    if (holdAnimator && holdAnimator.Status()==1 && EiemPhysicsReadValue(animator,idMethod,id)) fprintf(file,"%d",id);
    else fputs("null",file);
  } else fputs("null",file);
}

static bool EiemWriteNativePhysicsProbe(FILE *file,EiemPhysicsTraceReceipt *receipt=nullptr) {
  // Missing APIs must be reported, not interpreted as 'no physics in scene'.
  if (!file || !EiemOnUnityThread()) return false;
  fputs("{\"schema\":1,\"kind\":\"native-physics-diagnostic-not-mod\","
        "\"consistency\":\"individual-reads-not-a-retirement-fence\",", file);
  fputs("\"lifecycle\":",file);
  if (!EiemWritePhysicsCallTrace(file,receipt)) return false;
  fputc(',',file);
  if (!il2cpp_domain_get || !il2cpp_domain_get_assemblies ||
      !il2cpp_assembly_get_image || !il2cpp_image_get_name || !il2cpp_class_from_name ||
      !il2cpp_image_get_class_count || !il2cpp_image_get_class ||
      !il2cpp_class_get_name || !il2cpp_class_get_namespace ||
      !il2cpp_class_get_fields || !il2cpp_field_get_name || !il2cpp_field_get_type ||
      !il2cpp_field_get_flags || !il2cpp_field_get_value || !il2cpp_class_get_methods ||
      !il2cpp_method_get_name || !il2cpp_method_get_flags || !il2cpp_method_get_param_count ||
      !il2cpp_method_get_param || !il2cpp_method_get_return_type || !il2cpp_type_get_name ||
      !il2cpp_free || !il2cpp_array_length || !il2cpp_class_get_type ||
      !il2cpp_type_get_object || !il2cpp_object_get_class || !il2cpp_object_unbox) {
    fputs("\"status\":\"metadata-api-unavailable\"}", file); return true;
  }
  size_t count = 0;
  void *domain = il2cpp_domain_get();
  void **assemblies = domain ? il2cpp_domain_get_assemblies(domain, &count) : nullptr;
  if (!assemblies || !count) {
    fputs("\"status\":\"domain-unavailable\"}", file); return true;
  }
  const char *classes[] = {"BeyondBoneCloth", "ClothProcess", "ClothSerializeData",
    "ClothSerializeData2", "ColliderComponent", "BeyondBoneCapsuleCollider",
    "BeyondBoneSphereCollider", "BeyondBonePlaneCollider", "TeamManager", "ResultCode",
    "RenderSetupData", "SelectionData", "DynamicBoneTransformManager", "ClothManager",
    "VertexAttribute"};
  void *clothClass = nullptr, *processClass = nullptr, *setupClass=nullptr, *resultClass=nullptr;
  void *data2Class=nullptr,*selectionClass=nullptr,*vertexClass=nullptr;
  void *animatorClass=EiemPhysicsClass(assemblies,count,EiemPhysicsAnimatorImage,"UnityEngine","Animator");
  EiemPhysicsWriteIcallTargets(file,animatorClass);
  fputs("\"contracts\":[", file);
  for (size_t i = 0; i < _countof(classes); ++i) {
    if (i) fputc(',', file);
    void *klass = EiemPhysicsClass(assemblies, count, "BeyondDynamicBone.dll", "BeyondDynamicBone", classes[i]);
    EiemPhysicsWriteContract(file, klass, classes[i]);
    if (i == 0) clothClass = klass;
    if (i == 1) processClass = klass;
    if (!strcmp(classes[i],"ClothSerializeData2")) data2Class=klass;
    if (!strcmp(classes[i],"RenderSetupData")) setupClass=klass;
    if (!strcmp(classes[i],"SelectionData")) selectionClass=klass;
    if (!strcmp(classes[i],"ResultCode")) resultClass=klass;
    if (!strcmp(classes[i],"VertexAttribute")) vertexClass=klass;
  }
  // Expose the actual engine contract without dereferencing RW buffer pointers.
  for (const char *name:{"Animator","AnimationTransformRWBufferHandle"}) {
    fputc(',',file);
    EiemPhysicsWriteContract(file,EiemPhysicsClass(assemblies,count,EiemPhysicsAnimatorImage,"UnityEngine",name),name);
  }
  // A loaded object is not necessarily an active scene instance. Report activity
  // without claiming that active=false distinguishes a template from an instance.
  void *resources = FindClass("UnityEngine", "Resources", assemblies, count);
  void *findAll = EiemPhysicsMethod(resources, "FindObjectsOfTypeAll", "UnityEngine.Object[]", true, "System.Type");
  void *arrayClass = FindClass("System", "Array", assemblies, count);
  void *getItem = EiemPhysicsMethod(arrayClass, "GetValue", "System.Object", false, "System.Int32");
  void *behaviour = FindClass("UnityEngine", "Behaviour", assemblies, count);
  void *activeMethod = EiemPhysicsMethod(behaviour, "get_isActiveAndEnabled", "System.Boolean", false);
  void *unityObject = FindClass("UnityEngine", "Object", assemblies, count);
  void *idMethod = EiemPhysicsMethod(unityObject, "GetInstanceID", "System.Int32", false);
  void *type = clothClass ? il2cpp_type_get_object(il2cpp_class_get_type(clothClass)) : nullptr;
  auto keepType = EiemUnityRef::Capture(type, false);
  void *array = nullptr; void *args[] = {type};
  fputs("],\"scope\":\"all-loaded-cloth-components-including-inactive-and-assets\",", file);
  if (!keepType || !getItem || !InvokeChecked(findAll, nullptr, args, &array) || !array) {
    fputs("\"status\":\"enumeration-unavailable\",\"components\":[]}", file); return true;
  }
  auto keepArray = EiemUnityRef::Capture(array, false);
  if (!keepArray) { fputs("\"status\":\"array-root-failed\",\"components\":[]}", file); return true; }
  const uintptr_t length = il2cpp_array_length(array);
  if (length > INT_MAX) { fputs("\"status\":\"array-index-range\",\"components\":[]}", file); return true; }
  fprintf(file, "\"status\":\"enumerated\",\"count\":%zu,\"components\":[", (size_t)length);
  for (int32_t i = 0; i < (int32_t)length; ++i) {
    if (i) fputc(',', file);
    void *component = nullptr; void *indexArgs[] = {&i};
    if (!InvokeChecked(getItem, array, indexArgs, &component) || !component) {
      fputs("{\"status\":\"item-unavailable\"}", file); continue;
    }
    auto keepComponent = EiemUnityRef::Capture(component, false);
    int alive = keepComponent ? keepComponent.Status() : -1;
    fprintf(file, "{\"component\":\"%p\",\"nativeAlive\":%d,", component, alive);
    if (alive != 1 || il2cpp_object_get_class(component) != clothClass) {
      fputs("\"status\":\"component-unavailable\"}", file); continue;
    }
    char label[512] = {}, hierarchy[2048] = {};
    EiemReadObjectLabel(component, label, sizeof(label));
    TraceBuildRendererHierarchy(component, hierarchy, sizeof(hierarchy));
    fputs("\"name\":", file); EiemWriteJsonString(file, label);
    fputs(",\"hierarchyLabel\":", file); EiemWriteJsonString(file, hierarchy);
    int32_t instanceId = 0; bool active = false;
    fputs(",\"instanceId\":", file);
    if (EiemPhysicsReadValue(component, idMethod, instanceId)) fprintf(file, "%d", instanceId);
    else fputs("null", file);
    fputs(",\"activeAndEnabled\":", file);
    fputs(EiemPhysicsReadValue(component, activeMethod, active) ? (active ? "true" : "false") : "null", file);
    fputc(',', file);
    EiemPhysicsWriteState(file, component, clothClass, processClass,setupClass,idMethod,resultClass,
                          data2Class,selectionClass,vertexClass,getItem,
                          !strncmp(label,"EFF_Physics_",13));
    fputc('}', file);
  }
  fputs("]}\n", file);
  return !ferror(file);
}
