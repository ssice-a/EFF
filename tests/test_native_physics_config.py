"""Detached native config preparation, exercised without a Unity/game process."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_native_physics_probe import SOURCE as HOST

ROOT = Path(__file__).resolve().parents[1]
SOURCE = HOST.split('\nint main(', 1)[0] + r'''
#define IL2CPP_ARRAY_DATA 0x20
#include "eiem_native_physics_config.h"
static Klass dataClass{"ClothSerializeData","BeyondDynamicBone",{{".ctor","System.Void",0,{}}},{}},
  configData2Class{"ClothSerializeData2","BeyondDynamicBone",{{".ctor","System.Void",0,{}}},{}},
  curveDataClass{"CurveSerializeData","BeyondDynamicBone",
    {{"SetValue","System.Void",0,{"System.Single","UnityEngine.AnimationCurve"}}},{}},
  transformClass{"Transform","UnityEngine",{{"get_parent","UnityEngine.Transform",0,{}},
    {"get_childCount","System.Int32",0,{}},{"GetChild","UnityEngine.Transform",0,{"System.Int32"}}},{}},
  animationCurveClass{"AnimationCurve","UnityEngine",
    {{".ctor","System.Void",0,{"UnityEngine.Keyframe[]"}},
     {"get_keys","UnityEngine.Keyframe[]",0,{}}},{}},
  keyframeClass{"Keyframe","UnityEngine",{},{}},
  float3Class{"float3","Unity.Mathematics",{},{}},
  enumClass{"ClothType","BeyondDynamicBone.ClothProcess",{},{}},
  angleClass{"AngleLimitConstraintData","BeyondDynamicBone",{},{}},
  listClass{"List","System.Collections.Generic",{{".ctor","System.Void",0,{}},
    {"Add","System.Void",0,{"UnityEngine.Transform"}},{"get_Count","System.Int32",0,{}},
    {"get_Item","UnityEngine.Transform",0,{"System.Int32"}}},{}};
static Image coreImage{"UnityEngine.CoreModule.dll",{&transformClass,&animationCurveClass,&keyframeClass}};
static void *allAssemblies[]={&nativeImage,&unityImage,&coreImage};
struct Object {
  Klass *klass=nullptr; bool alive=true;
  Object *parent=nullptr,*rootsList=nullptr,*ignoresList=nullptr;
  Object *radiusData=nullptr,*radiusCurve=nullptr; void *radiusKeys=nullptr;
  Object *angleData=nullptr,*angleCurveData=nullptr;
  Vector3 gravityDirection{}; bool angleEnabled=false; float angleStiffness=0;
  int32_t clothType=-1,connectionMode=-1;
  float radiusValue=91; bool radiusUseCurve=false;
  float scalars[5]={91,92,93,94,95};
  std::vector<void *> items;
  std::vector<Object *> children;
};
static Object root{&transformClass},child{&transformClass},ignored{&transformClass},
  boundary{&transformClass},secondRoot{&transformClass},secondChild{&transformClass},
  foreign{&transformClass};
static std::vector<std::unique_ptr<Object>> objects;
static std::string mode;
static int created=0,written=0,ctorCalls=0,unexpectedCalls=0,liveChecks=0,secondDataCount=0;
static void *ConfigCall(void *m,void *o,void **args,void **exc) {
  *exc=nullptr;
  static bool alive; static int32_t count;
  if(m==(void *)1) {++liveChecks; alive=((Object *)args[0])->alive;return &alive;}
  std::string name=((Method *)m)->name;
  auto *object=(Object *)o;
  if(name==".ctor") {
    ++ctorCalls;
    if(mode=="ctor-failure" && object->klass==&configData2Class)*exc=(void *)1;
    if(mode=="second-group-failure" && object->klass==&configData2Class && ++secondDataCount==2)*exc=(void *)1;
    if(object->klass==&dataClass && !*exc) {
      if(mode!="default-list-missing")object->rootsList=(Object *)il2cpp_object_new(&listClass);
      object->ignoresList=mode=="default-list-alias"?object->rootsList:(Object *)il2cpp_object_new(&listClass);
      if(mode=="default-list-wrong-type" && object->rootsList)object->rootsList->klass=&dataClass;
      object->radiusData=(Object *)il2cpp_object_new(&curveDataClass);
      object->angleData=(Object *)il2cpp_object_new(&angleClass);
      object->angleData->angleCurveData=(Object *)il2cpp_object_new(&curveDataClass);
      if(mode=="native-object-missing")object->angleData=nullptr;
    }
    if(object->klass==&animationCurveClass && !*exc)object->radiusKeys=args[0];
    return nullptr;
  }
  if(name=="SetValue") {
    object->radiusValue=*(float *)args[0];object->radiusCurve=(Object *)args[1];
    if(mode=="radius-readback-mismatch")object->radiusValue=17;
    if(mode=="native-curve-readback-mismatch" && object->radiusValue==45)object->radiusValue=17;
    if(mode=="radius-set-exception")*exc=(void *)1;
    return nullptr;
  }
  if(name=="get_keys") return object->radiusKeys;
  if(name=="get_parent") return object->parent;
  if(name=="get_childCount") {count=(int32_t)object->children.size();return &count;}
  if(name=="GetChild") return object->children.at(*(int32_t *)args[0]);
  if(name=="Add") {
    object->items.push_back(args[0]);
    if(mode=="list-count-mismatch")object->items.push_back(args[0]);
    if(mode=="node-dies")root.alive=false;
    if(mode=="add-exception")*exc=(void *)1;
    return nullptr;
  }
  if(name=="get_Count") {count=int32_t(object->items.size());return &count;}
  if(name=="get_Item")return mode=="list-item-mismatch"?(void *)&foreign:object->items.at(*(int32_t *)args[0]);
  ++unexpectedCalls; *exc=(void *)1;return nullptr;
}
static void ConfigureHost() {
  Setup();
  nativeImage.classes.erase(std::remove(nativeImage.classes.begin(),nativeImage.classes.end(),&data2Class),nativeImage.classes.end());
  nativeImage.classes.push_back(&dataClass);nativeImage.classes.push_back(&configData2Class);
  nativeImage.classes.push_back(&curveDataClass);nativeImage.classes.push_back(&float3Class);
  dataClass.fields.push_back({"rootBones","System.Collections.Generic.List<UnityEngine.Transform>"});
  dataClass.fields.push_back({"ignoreFromRootBones","System.Collections.Generic.List<UnityEngine.Transform>"});
  dataClass.fields.push_back({"clothType","BeyondDynamicBone.ClothProcess.ClothType"});
  dataClass.fields.push_back({"connectionMode","BeyondDynamicBone.RenderSetupData.BoneConnectionMode"});
  dataClass.fields.push_back({"radius","BeyondDynamicBone.CurveSerializeData"});
  dataClass.fields.push_back({"gravityDirection","Unity.Mathematics.float3"});
  dataClass.fields.push_back({"angleLimitConstraint","BeyondDynamicBone.AngleLimitConstraintData"});
  angleClass.fields.push_back({"useAngleLimit","System.Boolean"});
  angleClass.fields.push_back({"stiffness","System.Single"});
  angleClass.fields.push_back({"limitAngle","BeyondDynamicBone.CurveSerializeData"});
  curveDataClass.fields.push_back({"value","System.Single"});
  curveDataClass.fields.push_back({"useCurve","System.Boolean"});
  curveDataClass.fields.push_back({"curve","UnityEngine.AnimationCurve"});
  for(size_t i=0;i<5;++i)dataClass.fields.push_back({EiemPhysicsConfigApi::ScalarName(i),"System.Single"});
  child.parent=&root;ignored.parent=&root;boundary.parent=&child;
  secondChild.parent=&secondRoot;
  root.children={&child,&ignored};child.children={&boundary};secondRoot.children={&secondChild};
  il2cpp_class_from_type=+[](void *t)->void * {
    const char *name=(const char *)t;
    if(!strcmp(name,"System.Collections.Generic.List<UnityEngine.Transform>"))return &listClass;
    if(!strcmp(name,"BeyondDynamicBone.AngleLimitConstraintData"))return &angleClass;
    if(!strcmp(name,"BeyondDynamicBone.CurveSerializeData"))return &curveDataClass;
    if(!strcmp(name,"BeyondDynamicBone.ClothProcess.ClothType"))return &enumClass;
    if(!strcmp(name,"Unity.Mathematics.float3"))return &float3Class;
    return nullptr;
  };
  il2cpp_class_get_type=+[](void *k)->void * {
    if(k!=&listClass)abort();return (void *)"System.Collections.Generic.List<UnityEngine.Transform>";
  };
  il2cpp_object_new=+[](void *k)->void * {
    ++created;
    if(mode=="allocation-failure")return nullptr;
    objects.push_back(std::make_unique<Object>());
    objects.back()->klass=(Klass *)k;return objects.back().get();
  };
  il2cpp_class_value_size=+[](void *k,uint32_t *align)->int32_t {
    *align=4;
    if(k==&keyframeClass)return 28;
    if(k==&enumClass)return 4;
    if(k==&float3Class)return mode=="gravity-layout-mismatch"?16:12;
    return 0;
  };
  il2cpp_type_get_type=+[](void *t)->int {
    const char *name=(const char *)t;
    if(!strcmp(name,"System.Boolean"))return 0x02;
    if(!strcmp(name,"System.Int32"))return 0x08;
    if(!strcmp(name,"System.Single"))return 0x0c;
    if(!strcmp(name,"UnityEngine.Vector3")||!strcmp(name,"Unity.Mathematics.float3"))return 0x11;
    if(!strcmp(name,"BeyondDynamicBone.ClothProcess.ClothType"))return 0x11;
    return 0x12;
  };
  il2cpp_array_new=+[](void *k,size_t count)->void * {
    if(k!=&keyframeClass)abort();
    auto bytes=new unsigned char[IL2CPP_ARRAY_DATA+count*28]();
    *(uintptr_t *)(bytes+24)=count;return bytes;
  };
  il2cpp_array_length=+[](void *array)->uintptr_t {return *(uintptr_t *)((char *)array+24);};
  il2cpp_object_get_class=+[](void *o)->void * {return ((Object *)o)->klass;};
  il2cpp_field_set_value=+[](void *o,void *f,void *v) {
    auto &object=*(Object *)o;
    if(object.klass==&curveDataClass) {
      std::string name=((Field *)f)->name;
      if(name=="useCurve") {object.radiusUseCurve=*(bool *)v;return;}
      abort();
    }
    if(object.klass==&angleClass) {
      std::string name=((Field *)f)->name;
      if(name=="useAngleLimit") {object.angleEnabled=*(bool *)v;return;}
      if(name=="stiffness") {object.angleStiffness=*(float *)v;return;}
      abort();
    }
    if(object.klass!=&dataClass)abort(); // no write to a bone or live component
    ++written; std::string name=((Field *)f)->name;
    if(name=="rootBones"||name=="ignoreFromRootBones")abort();
    if(name=="clothType") {object.clothType=*(int32_t *)v;return;}
    if(name=="connectionMode") {object.connectionMode=*(int32_t *)v;return;}
    if(name=="gravityDirection") {object.gravityDirection=*(Vector3 *)v;return;}
    for(size_t i=0;i<5;++i)if(name==EiemPhysicsConfigApi::ScalarName(i)) {
      object.scalars[i]=*(float *)v;
      if(mode=="scalar-mismatch" && i==2)object.scalars[i]=17;
      if(mode=="scalar-exception")RaiseException(0xe1000062,0,0,nullptr);
      return;
    }
    abort();
  };
  il2cpp_field_get_value=+[](void *o,void *f,void *out) {
    auto &object=*(Object *)o;std::string name=((Field *)f)->name;
    if(name=="rootBones") {*(Object **)out=object.rootsList;return;}
    if(name=="ignoreFromRootBones") {*(Object **)out=object.ignoresList;return;}
    if(name=="clothType") {*(int32_t *)out=object.clothType;return;}
    if(name=="connectionMode") {*(int32_t *)out=object.connectionMode;return;}
    if(name=="radius") {*(Object **)out=object.radiusData;return;}
    if(name=="gravityDirection") {*(Vector3 *)out=object.gravityDirection;return;}
    if(name=="angleLimitConstraint") {*(Object **)out=object.angleData;return;}
    if(object.klass==&curveDataClass) {
      if(name=="value") {*(float *)out=object.radiusValue;return;}
      if(name=="useCurve") {*(bool *)out=object.radiusUseCurve;return;}
      if(name=="curve") {*(Object **)out=object.radiusCurve;return;}
    }
    if(object.klass==&angleClass) {
      if(name=="useAngleLimit") {*(bool *)out=object.angleEnabled;return;}
      if(name=="stiffness") {*(float *)out=object.angleStiffness;return;}
      if(name=="limitAngle") {*(Object **)out=object.angleCurveData;return;}
    }
    for(size_t i=0;i<5;++i)if(name==EiemPhysicsConfigApi::ScalarName(i)) {*(float *)out=object.scalars[i];return;}
    abort();
  };
  il2cpp_runtime_invoke=ConfigCall;
}
int main(int argc,char **argv) {
  if(argc!=2)return 1; ConfigureHost();std::string error;
  EiemPhysicsConfigApi api;CHECK(api.Resolve(allAssemblies,3,error));
  EiemPhysicsDocument document;
  document.version=5;
  document.id=std::string(32,'a');document.skeleton="skeletons/test.skeleton";
  EiemPhysicsAuthorGroup group;
  group.id=std::string(32,'b');group.name="Minimal";
  // Deliberately child first: index zero is not necessarily the fixed root.
  group.nodes={{"Hair/Tip",1},{"Hair",0},{"Hair/Ignore",2},
               {"Skirt",0},{"Skirt/Tip",1}};
  const float params[]={9.8f,0.2f,0.4f,0.8f,1.0f};std::copy(params,params+5,group.parameters);
  group.radius.value=.025f;group.radius.useCurve=1;
  group.radius.keys={{0,1,0,-.5f,2,1.0f/3.0f,.2f},{1,.25f,-.5f,0,1,.4f,1.0f/3.0f}};
  auto floating=[&](const char *path,double value) {EiemPhysicsAuthorParameter p;p.path=path;p.floating=1;p.floatingValue=value;group.nativeParameters.push_back(p);};
  auto integer=[&](const char *path,int64_t value) {EiemPhysicsAuthorParameter p;p.path=path;p.integerValue=value;group.nativeParameters.push_back(p);};
  floating("serializeData.gravityDirection.x",.1);floating("serializeData.gravityDirection.y",-.9);floating("serializeData.gravityDirection.z",.2);
  integer("serializeData.clothType",2);
  integer("serializeData.angleLimitConstraint.useAngleLimit",1);
  floating("serializeData.angleLimitConstraint.stiffness",.75);
  floating("serializeData.angleLimitConstraint.limitAngle.value",45);integer("serializeData.angleLimitConstraint.limitAngle.useCurve",1);
  for(int i=0;i<2;++i) {
    std::string base="serializeData.angleLimitConstraint.limitAngle.curve.m_Curve."+std::to_string(i)+".";
    floating((base+"time").c_str(),i);floating((base+"value").c_str(),i?0.5:1.0);
    floating((base+"inSlope").c_str(),0);floating((base+"outSlope").c_str(),0);
    integer((base+"weightedMode").c_str(),0);floating((base+"inWeight").c_str(),1.0/3.0);floating((base+"outWeight").c_str(),1.0/3.0);
  }
  integer("serializeData.angleLimitConstraint.limitAngle.curve.m_PreInfinity",2);
  integer("serializeData.angleLimitConstraint.limitAngle.curve.m_PostInfinity",2);
  integer("serializeData.angleLimitConstraint.limitAngle.curve.m_RotationOrder",4);
  document.groups.push_back(group);
  std::unordered_map<std::string,void *> bindings={{"Hair",&root},{"Hair/Tip",&child},{"Hair/Ignore",&ignored},
    {"Skirt",&secondRoot},{"Skirt/Tip",&secondChild}};
  mode=argv[1];
  if(mode=="resolve-type") {
    dataClass.fields[0].type="System.Object";
    CHECK(!api.Resolve(allAssemblies,3,error));CHECK(created==0);return 0;
  }
  if(mode=="resolve-duplicate") {
    listClass.methods.push_back(listClass.methods[1]);
    CHECK(!api.Resolve(allAssemblies,3,error));CHECK(created==0);return 0;
  }
  if(mode=="resolve-radius-type") {
    curveDataClass.fields[0].type="System.Object";
    CHECK(!api.Resolve(allAssemblies,3,error));CHECK(created==0);return 0;
  }
  if(mode=="resolve-off-thread") {onThread=false;CHECK(!api.Resolve(allAssemblies,3,error));CHECK(created==0);return 0;}
  {
    EiemPhysicsConfigDraft draft;
    const auto requestMode=mode;mode="";
    CHECK(draft.Prepare(api,document,bindings,error));
    CHECK(draft.Groups().size()==1);
    auto oldData=draft.Groups()[0].data.Target();auto oldData2=draft.Groups()[0].data2.Target();
    const size_t rooted=roots.size();const int before=created;
    CHECK(((Object *)oldData)->rootsList->items.size()==2 &&
      ((Object *)oldData)->rootsList->items[0]==&root &&
      ((Object *)oldData)->rootsList->items[1]==&secondRoot);
    CHECK(((Object *)oldData)->ignoresList->items.size()==2 &&
      ((Object *)oldData)->ignoresList->items[0]==&ignored &&
      ((Object *)oldData)->ignoresList->items[1]==&boundary);
    CHECK(draft.Groups()[0].boundaryIgnores.size()==1 &&
      draft.Groups()[0].boundaryIgnores[0].Target()==&boundary);
    CHECK(((Object *)oldData)->clothType==2 && ((Object *)oldData)->connectionMode==0);
    for(size_t i=0;i<5;++i)CHECK(((Object *)oldData)->scalars[i]==params[i]);
    CHECK(((Object *)oldData)->radiusData->radiusValue==.025f);
    CHECK(((Object *)oldData)->radiusData->radiusUseCurve);
    CHECK(((Object *)oldData)->radiusData->radiusCurve->radiusKeys!=nullptr);
    CHECK(((Object *)oldData)->gravityDirection.x==.1f && ((Object *)oldData)->gravityDirection.y==-.9f && ((Object *)oldData)->gravityDirection.z==.2f);
    CHECK(((Object *)oldData)->angleData->angleEnabled && ((Object *)oldData)->angleData->angleStiffness==.75f);
    CHECK(((Object *)oldData)->angleData->angleCurveData->radiusValue==45 &&
      ((Object *)oldData)->angleData->angleCurveData->radiusUseCurve &&
      ((Object *)oldData)->angleData->angleCurveData->radiusCurve->radiusKeys!=nullptr);
    CHECK(draft.Groups()[0].authorNodes[0].role==1 && draft.Groups()[0].authorNodes[1].role==0);
    mode=requestMode;
    if(mode=="ok") {
      document.groups[0].parameters[0]=3;
      CHECK(draft.Prepare(api,document,bindings,error));
      CHECK(draft.Groups()[0].data.Target()!=oldData && draft.Groups()[0].data2.Target()!=oldData2);
      CHECK(((Object *)oldData)->scalars[0]==9.8f && ((Object *)draft.Groups()[0].data.Target())->scalars[0]==3);
      CHECK(roots.size()==rooted);
    } else if(mode=="collider") {
      EiemPhysicsAuthorCollider collider;collider.id=std::string(32,'c');collider.name="Sphere";collider.bone="Hair";collider.radius=.1f;collider.endRadius=.1f;
      document.colliders.push_back(collider);document.groups[0].colliders.push_back(collider.id);
      CHECK(draft.Prepare(api,document,bindings,error));
      CHECK(error.empty() && draft.Groups().size()==1);
      CHECK(draft.Groups()[0].data.Target()!=oldData && draft.Groups()[0].data2.Target()!=oldData2);
    } else {
      if(mode=="missing-node")bindings.erase("Hair/Tip");
      if(mode=="alias-node")bindings["Hair/Tip"]=&root;
      if(mode=="wrong-parent")child.parent=&foreign;
      if(mode=="wrong-node-type")child.klass=&dataClass;
      if(mode=="dead-node")child.alive=false;
      if(mode=="invalid-parameters")document.groups[0].parameters[3]=2;
      if(mode=="off-thread")onThread=false;
      if(mode=="v2")document.version=2;
      if(mode=="second-group-failure") {
        auto second=document.groups.front();second.id=std::string(32,'d');second.name="Second";
        document.groups.push_back(second);
      }
      CHECK(!draft.Prepare(api,document,bindings,error));
      CHECK(!error.empty() && draft.Groups()[0].data.Target()==oldData && draft.Groups()[0].data2.Target()==oldData2);
      CHECK(((Object *)oldData)->scalars[0]==9.8f && roots.size()==rooted);
      if(mode=="missing-node"||mode=="alias-node"||mode=="wrong-parent"||mode=="wrong-node-type"||
         mode=="dead-node"||mode=="invalid-parameters"||mode=="off-thread"||mode=="v2")CHECK(created==before);
    }
  }
  CHECK(roots.empty() && unexpectedCalls==0 && allocated==freed);
  CHECK(root.scalars[0]==91 && child.scalars[0]==91);
  return 0;
}
'''


class NativePhysicsConfigTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('cl'):
            raise unittest.SkipTest('Requires MSVC')
        cls.temp = tempfile.TemporaryDirectory(prefix='eiem-physics-config-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        source = cls.folder/'test.cpp';source.write_text(SOURCE,encoding='utf-8')
        cls.exe = cls.folder/'test.exe'
        result = subprocess.run(['cl','/nologo','/O2','/EHsc','/std:c++17','/utf-8',
                                 f'/I{ROOT/"src"}',f'/I{ROOT/"deps/minhook_lib/include"}',
                                 str(source),f'/Fe{cls.exe}'],cwd=cls.folder,
                                capture_output=True,text=True,errors='replace')
        if result.returncode:raise AssertionError(result.stdout+result.stderr)

    def run_case(self,*modes):
        for mode in modes:
            with self.subTest(mode=mode):
                result=subprocess.run([str(self.exe),mode],capture_output=True,text=True,errors='replace')
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def test_private_configs_and_fixed_root_are_independent_of_input_order(self):self.run_case('ok')
    def test_incomplete_or_ambiguous_contract_is_rejected_before_allocation(self):self.run_case('resolve-type','resolve-radius-type','resolve-duplicate','resolve-off-thread')
    def test_missing_or_aliased_bindings_preserve_existing_draft(self):self.run_case('missing-node','alias-node')
    def test_actual_parent_type_and_native_liveness_are_checked(self):self.run_case('wrong-parent','wrong-node-type','dead-node')
    def test_gravity_float3_layout_is_validated_before_write(self):self.run_case('gravity-layout-mismatch')
    def test_v2_is_not_coerced_but_author_colliders_reach_the_runtime_stage(self):self.run_case('v2','collider')
    def test_invalid_scalar_and_wrong_thread_do_not_allocate(self):self.run_case('invalid-parameters','off-thread')
    def test_failed_allocation_or_ctor_preserves_previous_draft(self):self.run_case('allocation-failure','ctor-failure')
    def test_scalar_write_or_readback_failure_is_transactional(self):self.run_case('scalar-mismatch','scalar-exception')
    def test_radius_curve_write_or_readback_failure_is_transactional(self):self.run_case('radius-readback-mismatch','radius-set-exception')
    def test_native_parameter_failure_is_transactional(self):self.run_case('native-object-missing','native-curve-readback-mismatch')
    def test_constructor_owned_lists_are_populated_and_checked(self):self.run_case('add-exception','list-count-mismatch','list-item-mismatch','default-list-missing','default-list-alias','default-list-wrong-type')
    def test_node_expiring_during_preparation_does_not_publish_draft(self):self.run_case('node-dies')
    def test_later_group_failure_does_not_replace_any_previous_config(self):self.run_case('second-group-failure')
