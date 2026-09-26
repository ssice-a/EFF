"""Compile production trace wrappers; mocked hooks are not native/game acceptance."""
from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
import unittest

from test_native_physics_probe import SOURCE as PROBE_HOST

ROOT = Path(__file__).resolve().parents[1]
SOURCE = PROBE_HOST.replace(
    'struct Method { const char *name,*ret; uint32_t flags; std::vector<const char *> params; };',
    '''struct Method {
      void *mp=nullptr; const char *name,*ret; uint32_t flags; std::vector<const char *> params; uint32_t impl=0;
      Method(const char *n,const char *r,uint32_t f,std::vector<const char *> p):name(n),ret(r),flags(f),params(p) {}
    };''',
).replace('int main(int argc,char **argv)', 'static int unusedProbeMain(int argc,char **argv)') + r'''
#include <atomic>
#include <thread>
#include "eiem_native_physics_trace.h"
static int creates=0,enables=0,failCreate=0,failEnable=0;
extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID target,LPVOID,LPVOID *original) {
  ++creates; if(creates==failCreate)return MH_ERROR_MEMORY_ALLOC;
  *original=target;return MH_OK;
}
extern "C" MH_STATUS WINAPI MH_EnableHook(LPVOID) {
  ++enables;return enables==failEnable?MH_ERROR_MEMORY_PROTECT:MH_OK;
}
static constexpr size_t opCount=(size_t)EiemPhysicsOperation::Count;
static constexpr size_t hookCount=_countof(s_eiemPhysicsTraceHooks)+
  _countof(s_eiemPhysicsIcallTraceHooks)+_countof(s_eiemPhysicsJobTraceHooks);
static std::atomic<int> forwarded[opCount],badArguments{0};
static std::atomic<bool> blocked{false},entered{false},releaseCall{false};
static bool throwCall=false,nestedCall=false;
static void *const expectedObject=(void *)0x1234,*const expectedMethod=(void *)0x4567;
static void *const expectedRelated=(void *)0x8910;
static void *const expectedValues=(void *)0x7770;
static constexpr int32_t expectedTeamId=77;
static void Seen(EiemPhysicsOperation op,void *o,void *m) {
  ++forwarded[(size_t)op];
  if(o!=expectedObject||m!=expectedMethod)++badArguments;
}
static bool NativeBuild(void *o,void *m) {
  Seen(EiemPhysicsOperation::BuildAndRun,o,m);
  if(throwCall) RaiseException(0xe1234567,0,0,nullptr);
  if(nestedCall) EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(o,m);
  if(blocked) {entered=true;while(!releaseCall.load())std::this_thread::yield();}
  return true;
}
static bool NativeStart(void *o,void *m) {Seen(EiemPhysicsOperation::StartRuntimeBuild,o,m);return false;}
static void NativeInit(void *o,void *m) {Seen(EiemPhysicsOperation::Init,o,m);}
static void NativeDispose(void *o,void *m) {Seen(EiemPhysicsOperation::Dispose,o,m);}
static void NativeDisposeInternal(void *o,void *m) {Seen(EiemPhysicsOperation::DisposeInternal,o,m);}
static void NativeRemove(void *o,void *p,void *m) {
  Seen(EiemPhysicsOperation::RemoveMonitoringProcess,o,m);if(p!=expectedRelated)++badArguments;
}
static void NativeComplete(void *o,void *m) {Seen(EiemPhysicsOperation::CompleteMasterJob,o,m);}
static void NativeUpdateAnimator(void *o,void *ids,void *transforms,void *m) {
  Seen(EiemPhysicsOperation::TeamUpdateAnimatorData,o,m);
  if(ids!=expectedValues||transforms!=expectedRelated)++badArguments;
}
static void NativeClearAnimator(void *o,int32_t teamId,void *m) {
  Seen(EiemPhysicsOperation::TeamClearAnimatorData,o,m);if(teamId!=expectedTeamId)++badArguments;
}
template<EiemPhysicsOperation Op>
static void NativeTeamObject(void *o,int32_t teamId,void *related,void *m) {
  Seen(Op,o,m);
  if(teamId!=expectedTeamId||related!=expectedRelated)++badArguments;
}
static void *focusedTargets[]={ (void *)NativeComplete,
  (void *)NativeTeamObject<EiemPhysicsOperation::TeamAddAnimatorData>,
  (void *)NativeTeamObject<EiemPhysicsOperation::TeamAddAnimatorTransform> };
static Klass teamManager{"TeamManager","BeyondDynamicBone",{},{}},clothManager{"ClothManager","BeyondDynamicBone",{},{}},
  transformManager{"DynamicBoneTransformManager","BeyondDynamicBone",{},{} };
static Klass animator{"Animator","UnityEngine",{},{}};
struct BindingCounts {uint16_t count,invalid;};
static void SeenIcall(EiemPhysicsOperation op,void *o,void *related=nullptr) {
  ++forwarded[(size_t)op];
  if(o!=expectedObject || (related && related!=expectedRelated))++badArguments;
}
static void NativeCreateTransforms(void *o,void *values,void *out) {
  SeenIcall(EiemPhysicsOperation::AnimatorCreateClothBindings,o,values);
  *(BindingCounts *)out={4,1};
}
static void NativeCreateNames(void *o,void *values,void *out) {
  SeenIcall(EiemPhysicsOperation::AnimatorCreateClothBindingsByName,o,values);
  *(BindingCounts *)out={3,2};
}
static void NativeEnableBindings(void *o) {SeenIcall(EiemPhysicsOperation::AnimatorEnableClothBindings,o);}
static void NativeDisableBindings(void *o) {SeenIcall(EiemPhysicsOperation::AnimatorDisableClothBindings,o);}
static void NativeDestroyBindings(void *o) {SeenIcall(EiemPhysicsOperation::AnimatorDestroyClothBindings,o);}
static void *icallTargets[]={ (void *)NativeCreateTransforms,(void *)NativeCreateNames,
  (void *)NativeEnableBindings,(void *)NativeDisableBindings,(void *)NativeDestroyBindings };
template<EiemPhysicsOperation Op>
static EiemPhysicsJobHandle NativeJob4(void *o,EiemPhysicsJobHandle dependency,
                                      void *teamMap,void *animatorMap,void *transformMap,void *m) {
  Seen(Op,o,m);
  if(teamMap!=expectedValues||animatorMap!=expectedRelated||transformMap!=expectedValues)
    ++badArguments;
  return {dependency.handle+1,dependency.version+1};
}
template<EiemPhysicsOperation Op>
static EiemPhysicsJobHandle NativeJob1(void *o,EiemPhysicsJobHandle dependency,void *m) {
  Seen(Op,o,m);return {dependency.handle+1,dependency.version+1};
}
static void *jobTargets[]={
  (void *)NativeJob4<EiemPhysicsOperation::DynamicBoneWriteAnimatorBufferData>,
  (void *)NativeJob4<EiemPhysicsOperation::DynamicBoneReadAnimatorBufferData>,
  (void *)NativeJob1<EiemPhysicsOperation::DynamicBoneCopyDoubleBuffer>,
  (void *)NativeJob1<EiemPhysicsOperation::DynamicBoneWriteDoubleBufferTransform>};
static void SetupTrace() {
  Setup();cloth.methods.clear();processClass.methods.clear();
  nativeImage.classes.push_back(&teamManager);nativeImage.classes.push_back(&clothManager);
  nativeImage.classes.push_back(&transformManager);
  for(size_t i=0;i<_countof(s_eiemPhysicsTraceHooks);++i) {
    const auto &h=s_eiemPhysicsTraceHooks[i];
    auto *k=(Klass *)il2cpp_class_from_name(&nativeImage,"BeyondDynamicBone",h.klass);
    Method m(h.name,h.returns,0,h.parameter0?
      (h.parameter1?std::vector<const char *>{h.parameter0,h.parameter1}:
                    std::vector<const char *>{h.parameter0}):std::vector<const char *>{});
    m.mp=focusedTargets[i];k->methods.push_back(m);
  }
  for(size_t i=0;i<_countof(s_eiemPhysicsJobTraceHooks);++i) {
    const auto &h=s_eiemPhysicsJobTraceHooks[i];
    Method m(h.name,h.returns,0,h.hasMaps?
      std::vector<const char *>{h.parameter0,h.parameter1,h.parameter2,h.parameter3}:
      std::vector<const char *>{h.parameter0});
    m.mp=jobTargets[i];transformManager.methods.push_back(m);
  }
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::BuildAndRun]=(void *)NativeBuild;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::StartRuntimeBuild]=(void *)NativeStart;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::Init]=(void *)NativeInit;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::Dispose]=(void *)NativeDispose;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::DisposeInternal]=(void *)NativeDisposeInternal;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::RemoveMonitoringProcess]=(void *)NativeRemove;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::TeamUpdateAnimatorData]=(void *)NativeUpdateAnimator;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::TeamClearAnimatorData]=(void *)NativeClearAnimator;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::TeamMarkAnimatorTransformDirty]=
    (void *)NativeTeamObject<EiemPhysicsOperation::TeamMarkAnimatorTransformDirty>;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::AnimatorEnableClothBindings]=(void *)NativeEnableBindings;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::AnimatorDisableClothBindings]=(void *)NativeDisableBindings;
  s_eiemPhysicsTraceOriginal[(size_t)EiemPhysicsOperation::AnimatorDestroyClothBindings]=(void *)NativeDestroyBindings;
  unityImage.name=EiemPhysicsAnimatorImage;unityImage.classes.push_back(&animator);
  for(const auto &h:s_eiemPhysicsIcallTraceHooks) {
    Method m(h.name,"System.Void",0,h.parameter0?
      std::vector<const char *>{h.parameter0,h.parameter1}:std::vector<const char *>{});
    m.impl=0x1000;animator.methods.push_back(m);
  }
  il2cpp_method_get_flags=+[](void *m,uint32_t *impl)->uint32_t {
    *impl=((Method *)m)->impl;return ((Method *)m)->flags;
  };
  il2cpp_resolve_icall=+[](const char *name)->void * {
    for(size_t i=0;i<_countof(s_eiemPhysicsIcallTraceHooks);++i) {
      std::string expected=std::string("UnityEngine.Animator::")+s_eiemPhysicsIcallTraceHooks[i].name;
      if(expected==name)return icallTargets[i];
    }
    return nullptr;
  };
}
static DWORD PropagatedException() {
  __try { EiemPhysicsTraceBool<EiemPhysicsOperation::BuildAndRun>(expectedObject,expectedMethod); }
  __except(EXCEPTION_EXECUTE_HANDLER) { return GetExceptionCode(); }
  return 0;
}
static bool Export(const char *path,EiemPhysicsTraceReceipt *receipt=nullptr) {
  FILE *f=fopen(path,"wb");if(!f)return false;
  bool ok=EiemWritePhysicsCallTrace(f,receipt);if(fclose(f))ok=false;return ok;
}
int main(int argc,char **argv) {
  if(argc!=3)return 1;SetupTrace();std::string mode=argv[1],error;
  if(mode=="off-thread" || mode=="missing-api" || mode=="static" || mode=="wrong-return" ||
     mode=="wrong-parameter" || mode=="alias" || mode=="external-alias" || mode=="missing-address" ||
     mode=="ambiguous" || mode=="shared-target" || mode=="missing-icall" ||
     mode=="wrong-icall-signature" || mode=="non-icall") {
    if(mode=="off-thread")onThread=false;
    if(mode=="missing-api")il2cpp_image_get_class=nullptr;
    if(mode=="static")clothManager.methods[0].flags=0x10;
    if(mode=="wrong-return")clothManager.methods[0].ret="System.Int32";
    if(mode=="wrong-parameter")teamManager.methods[0].params[0]="System.Object";
    if(mode=="missing-address")clothManager.methods[0].mp=nullptr;
    if(mode=="shared-target")teamManager.methods[0].mp=clothManager.methods[0].mp;
    if(mode=="alias" || mode=="external-alias") {
      Method alias(nullptr,"System.Void",0,{});alias.mp=clothManager.methods[0].mp;
      (mode=="alias"?processClass:unityObject).methods.push_back(alias);
    }
    if(mode=="ambiguous")clothManager.methods.push_back(clothManager.methods[0]);
    if(mode=="missing-icall")il2cpp_resolve_icall=nullptr;
    if(mode=="wrong-icall-signature")animator.methods[0].params[0]="System.Object[]";
    if(mode=="non-icall")animator.methods[0].impl=0;
    CHECK(!EiemStartPhysicsTrace(error));CHECK(!error.empty());
    CHECK(!EiemPhysicsTraceActive());CHECK(creates==0&&enables==0);
    CHECK(invoked==0&&allocated==freed);CHECK(Export(argv[2]));return 0;
  }
  if(mode=="create-failure" || mode=="enable-failure") {
    if(mode=="create-failure")failCreate=3;else failEnable=3;
    CHECK(!EiemStartPhysicsTrace(error));CHECK(!EiemPhysicsTraceActive());
    CHECK(mode=="create-failure"?enables==0:enables==3);
    // Already-created trampolines remain usable, but passive, after a partial failure.
    CHECK(EiemPhysicsTraceBool<EiemPhysicsOperation::BuildAndRun>(expectedObject,expectedMethod));
    CHECK(s_eiemPhysicsTraceSequence==0);
    failCreate=failEnable=0;CHECK(EiemStartPhysicsTrace(error));
    CHECK(creates==(mode=="create-failure"?hookCount+1:hookCount));
    CHECK(enables==(mode=="enable-failure"?hookCount+1:hookCount));
    for(const auto &hook:s_eiemPhysicsTraceHooks)CHECK(hook.created&&hook.enabled);
    for(const auto &hook:s_eiemPhysicsIcallTraceHooks)CHECK(hook.created&&hook.enabled);
  } else {
    CHECK(EiemStartPhysicsTrace(error,mode=="deadline"?15:0));
    CHECK(creates==hookCount&&enables==hookCount);
  }
  CHECK(!EiemPhysicsBeginTrace(error));CHECK(!error.empty());
  if(mode=="stale-target") {
    EiemPhysicsStopTrace();clothManager.methods[0].mp=(void *)Seen;
    CHECK(!EiemStartPhysicsTrace(error));CHECK(creates==hookCount&&enables==hookCount);
  } else if(mode=="normal") {
    CHECK(EiemPhysicsTraceBool<EiemPhysicsOperation::BuildAndRun>(expectedObject,expectedMethod));
    CHECK(!EiemPhysicsTraceBool<EiemPhysicsOperation::StartRuntimeBuild>(expectedObject,expectedMethod));
    EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(expectedObject,expectedMethod);
    EiemPhysicsTraceVoid<EiemPhysicsOperation::Dispose>(expectedObject,expectedMethod);
    EiemPhysicsTraceVoid<EiemPhysicsOperation::DisposeInternal>(expectedObject,expectedMethod);
    EiemPhysicsTraceRemoveMonitoring(expectedObject,expectedRelated,expectedMethod);
    EiemPhysicsTraceVoid<EiemPhysicsOperation::CompleteMasterJob>(expectedObject,expectedMethod);
    EiemPhysicsTraceUpdateAnimatorData(expectedObject,expectedValues,expectedRelated,expectedMethod);
    EiemPhysicsTraceTeamId<EiemPhysicsOperation::TeamClearAnimatorData>(expectedObject,expectedTeamId,expectedMethod);
    EiemPhysicsTraceTeamObject<EiemPhysicsOperation::TeamAddAnimatorData>(expectedObject,expectedTeamId,expectedRelated,expectedMethod);
    EiemPhysicsTraceTeamObject<EiemPhysicsOperation::TeamAddAnimatorTransform>(expectedObject,expectedTeamId,expectedRelated,expectedMethod);
    EiemPhysicsTraceTeamObject<EiemPhysicsOperation::TeamMarkAnimatorTransformDirty>(expectedObject,expectedTeamId,expectedRelated,expectedMethod);
    BindingCounts a{},b{};
    EiemPhysicsTraceIcallCreate<EiemPhysicsOperation::AnimatorCreateClothBindings>(expectedObject,expectedRelated,&a);
    EiemPhysicsTraceIcallCreate<EiemPhysicsOperation::AnimatorCreateClothBindingsByName>(expectedObject,expectedRelated,&b);
    EiemPhysicsTraceIcallVoid<EiemPhysicsOperation::AnimatorEnableClothBindings>(expectedObject);
    EiemPhysicsTraceIcallVoid<EiemPhysicsOperation::AnimatorDisableClothBindings>(expectedObject);
    EiemPhysicsTraceIcallVoid<EiemPhysicsOperation::AnimatorDestroyClothBindings>(expectedObject);
    EiemPhysicsJobHandle dependency{42,7};
    const auto write=EiemPhysicsTraceJobBuffer4<EiemPhysicsOperation::DynamicBoneWriteAnimatorBufferData>(
      expectedObject,dependency,expectedValues,expectedRelated,expectedValues,expectedMethod);
    const auto read=EiemPhysicsTraceJobBuffer4<EiemPhysicsOperation::DynamicBoneReadAnimatorBufferData>(
      expectedObject,dependency,expectedValues,expectedRelated,expectedValues,expectedMethod);
    const auto copy=EiemPhysicsTraceJobBuffer1<EiemPhysicsOperation::DynamicBoneCopyDoubleBuffer>(
      expectedObject,dependency,expectedMethod);
    const auto writeback=EiemPhysicsTraceJobBuffer1<EiemPhysicsOperation::DynamicBoneWriteDoubleBufferTransform>(
      expectedObject,dependency,expectedMethod);
    CHECK(write.handle==43&&read.handle==43&&copy.handle==43&&writeback.handle==43);
    CHECK(a.count==4&&a.invalid==1&&b.count==3&&b.invalid==2);
    for(auto op:{EiemPhysicsOperation::BuildAndRun,EiemPhysicsOperation::StartRuntimeBuild,
      EiemPhysicsOperation::Init,EiemPhysicsOperation::Dispose,EiemPhysicsOperation::DisposeInternal,
      EiemPhysicsOperation::RemoveMonitoringProcess,EiemPhysicsOperation::CompleteMasterJob,
      EiemPhysicsOperation::TeamUpdateAnimatorData,EiemPhysicsOperation::TeamClearAnimatorData,
      EiemPhysicsOperation::TeamAddAnimatorData,EiemPhysicsOperation::TeamAddAnimatorTransform,
      EiemPhysicsOperation::TeamMarkAnimatorTransformDirty,
      EiemPhysicsOperation::AnimatorCreateClothBindings,EiemPhysicsOperation::AnimatorCreateClothBindingsByName,
      EiemPhysicsOperation::AnimatorEnableClothBindings,EiemPhysicsOperation::AnimatorDisableClothBindings,
      EiemPhysicsOperation::AnimatorDestroyClothBindings,
      EiemPhysicsOperation::DynamicBoneWriteAnimatorBufferData,
      EiemPhysicsOperation::DynamicBoneReadAnimatorBufferData,
      EiemPhysicsOperation::DynamicBoneCopyDoubleBuffer,
      EiemPhysicsOperation::DynamicBoneWriteDoubleBufferTransform})CHECK(forwarded[(size_t)op]==1);
  } else if(mode=="exception") {
    throwCall=true;CHECK(PropagatedException()==0xe1234567);CHECK(forwarded[0]==1);
  } else if(mode=="nested") {
    nestedCall=true;CHECK(EiemPhysicsTraceBool<EiemPhysicsOperation::BuildAndRun>(expectedObject,expectedMethod));
    CHECK(forwarded[0]==1&&forwarded[2]==1);
  } else if(mode=="stop-inflight") {
    blocked=true;
    std::thread worker([]{EiemPhysicsTraceBool<EiemPhysicsOperation::BuildAndRun>(expectedObject,expectedMethod);});
    while(!entered.load())std::this_thread::yield();
    EiemPhysicsStopTrace();EiemPhysicsTraceReceipt early;
    if(!Export((std::string(argv[2])+".early").c_str(),&early))abort();
    EiemPhysicsTraceExported(early);
    bool rejected=!EiemPhysicsBeginTrace(error);
    releaseCall=true;worker.join();CHECK(rejected);
    CHECK(!EiemPhysicsBeginTrace(error)); // Late return has not been exported yet.
    EiemPhysicsTraceExported(early);CHECK(!EiemPhysicsBeginTrace(error));
  } else if(mode=="concurrent") {
    std::vector<std::thread> workers;
    for(int t=0;t<4;++t)workers.emplace_back([]{
      for(int n=0;n<500;++n)EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(expectedObject,expectedMethod);
    });
    for(auto &worker:workers)worker.join();CHECK(forwarded[2]==2000);
  } else if(mode=="overflow") {
    for(size_t n=0;n<EiemPhysicsTraceCapacity/2+3;++n)
      EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(expectedObject,expectedMethod);
    CHECK(s_eiemPhysicsTraceDropped==6);
  } else if(mode=="deadline") {
    EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(expectedObject,expectedMethod);
    Sleep(25);
    EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(expectedObject,expectedMethod);
    CHECK(forwarded[(size_t)EiemPhysicsOperation::Init]==2);
    CHECK(!EiemPhysicsTraceActive()&&s_eiemPhysicsTraceSequence==2);
  } else if(mode=="unsaved") {
    EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(expectedObject,expectedMethod);
    alignas(8) unsigned char bindingHandle[0x80]={};
    EiemPhysicsTraceIcallCreate<EiemPhysicsOperation::AnimatorCreateClothBindings>(
      expectedObject,expectedRelated,bindingHandle);
    CHECK(s_eiemPhysicsBindingSnapshotCount==1);
    EiemPhysicsStopTrace();EiemPhysicsTraceReceipt receipt;
    CHECK(Export((std::string(argv[2])+".unacked").c_str(),&receipt));
    CHECK(!EiemPhysicsBeginTrace(error)); // Writing alone does not acknowledge a successful close.
    EiemPhysicsTraceExported({receipt.session+1,receipt.sequence});CHECK(!EiemPhysicsBeginTrace(error));
    EiemPhysicsTraceExported(receipt);CHECK(EiemPhysicsBeginTrace(error));
    CHECK(s_eiemPhysicsTraceSession==2&&s_eiemPhysicsTraceSequence==0);
    CHECK(s_eiemPhysicsBindingSnapshotCount==0);
    EiemPhysicsTraceExported(receipt);CHECK(s_eiemPhysicsTraceSaved==0);
    EiemPhysicsTraceVoid<EiemPhysicsOperation::Dispose>(expectedObject,expectedMethod);
  } else if(mode!="create-failure"&&mode!="enable-failure") { CHECK(false); }
  EiemPhysicsStopTrace();auto sequence=s_eiemPhysicsTraceSequence;
  EiemPhysicsTraceVoid<EiemPhysicsOperation::Init>(expectedObject,expectedMethod);
  CHECK(s_eiemPhysicsTraceSequence==sequence);CHECK(s_eiemPhysicsTraceInFlight==0);
  CHECK(invoked==0&&allocated==freed&&badArguments==0);
  EiemPhysicsTraceReceipt receipt;CHECK(Export(argv[2],&receipt));
  if(sequence)CHECK(!EiemPhysicsBeginTrace(error));
  EiemPhysicsTraceExported(receipt);CHECK(EiemPhysicsBeginTrace(error));
  return 0;
}
'''


class NativePhysicsTrace(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not shutil.which('cl'):
            raise unittest.SkipTest('Requires MSVC')
        cls.temp = tempfile.TemporaryDirectory(prefix='eiem-physics-trace-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.folder = Path(cls.temp.name)
        source = cls.folder / 'test.cpp'
        source.write_text(SOURCE, encoding='utf-8')
        cls.exe = cls.folder / 'test.exe'
        result = subprocess.run(
            ['cl', '/nologo', '/O2', '/EHsc', '/std:c++17', '/utf-8',
             f'/I{ROOT / "src"}', f'/I{ROOT / "deps/minhook_lib/include"}',
             str(source), f'/Fe{cls.exe}'], cwd=cls.folder,
            capture_output=True, text=True, errors='replace',
        )
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

    def trace(self, mode):
        path = self.folder / f'{mode}.json'
        result = subprocess.run([str(self.exe), mode, str(path)],
                                capture_output=True, text=True, timeout=20)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        data = json.loads(path.read_text(encoding='utf-8'))
        self.assertIn('not-async-task-or-job-quiescence', data['proof'])
        return data

    def assert_pairs(self, data):
        pairs = {}
        self.assertEqual([e['seq'] for e in data['events']], list(range(1, len(data['events']) + 1)))
        for event in data['events']:
            pairs.setdefault(event['call'], []).append(event)
        for events in pairs.values():
            self.assertEqual(len(events), 2)
            self.assertEqual(events[0]['phase'], 'enter')
            self.assertIn(events[1]['phase'], ('return', 'unwind'))
            for key in ('thread', 'operation', 'object', 'related'):
                self.assertEqual(events[0][key], events[1][key])
        self.assertEqual(data['inFlightCalls'], 0)

    def test_forwarding_preserves_all_arguments_and_boolean_results(self):
        data = self.trace('normal')
        self.assert_pairs(data)
        returns = [e for e in data['events'] if e['phase'] == 'return']
        self.assertEqual(len(returns), 21)
        self.assertIs(returns[0]['result'], True)
        self.assertIs(returns[1]['result'], False)
        self.assertTrue(all(e['result'] is None for e in returns[2:]))
        self.assertEqual(int(returns[5]['related'], 16), 0x8910)
        self.assertEqual((returns[12]['bindingCount'], returns[12]['invalidBindingCount']), (4, 1))
        self.assertEqual((returns[13]['bindingCount'], returns[13]['invalidBindingCount']), (3, 2))
        self.assertTrue(all(e['bindingCount'] is None for e in returns[:12] + returns[14:]))
        self.assertTrue(all(e['teamId'] is None for e in returns[:8] + returns[12:]))
        self.assertTrue(all(e['teamId'] == 77 for e in returns[8:12]))
        for event in returns[-4:]:
            self.assertEqual(int(event['jobInput'], 16), (7 << 32) | 42)
            self.assertEqual(int(event['jobOutput'], 16), (8 << 32) | 43)

    def test_native_exception_propagates_and_records_unwind(self):
        data = self.trace('exception')
        self.assert_pairs(data)
        self.assertEqual(data['events'][1]['phase'], 'unwind')
        self.assertIsNone(data['events'][1]['result'])

    def test_reentrant_call_keeps_nesting_and_pairs(self):
        data = self.trace('nested')
        self.assert_pairs(data)
        self.assertEqual([e['call'] for e in data['events']], [1, 2, 2, 1])

    def test_stop_retains_late_return_and_requires_another_export(self):
        data = self.trace('stop-inflight')
        early = json.loads((self.folder / 'stop-inflight.json.early').read_text())
        self.assertEqual(early['inFlightCalls'], 1)
        self.assertFalse(early['recording'])
        self.assertEqual(len(early['events']), 1)
        self.assert_pairs(data)
        self.assertEqual(len(data['events']), 2)

    def test_threaded_calls_keep_unique_pairs(self):
        data = self.trace('concurrent')
        self.assert_pairs(data)
        self.assertEqual(len(data['events']), 4000)
        self.assertEqual(len({e['thread'] for e in data['events']}), 4)

    def test_buffer_overflow_is_reported_without_overwriting_evidence(self):
        data = self.trace('overflow')
        self.assert_pairs(data)
        self.assertEqual(len(data['events']), data['capacity'])
        self.assertEqual(data['dropped'], 6)
        self.assertEqual(data['sequence'], data['capacity'] + 6)

    def test_recording_deadline_survives_a_stalled_window_timer(self):
        data = self.trace('deadline')
        self.assert_pairs(data)
        self.assertFalse(data['recording'])
        self.assertEqual(len(data['events']), 2)

    def test_only_current_successful_export_allows_replacing_session(self):
        data = self.trace('unsaved')
        self.assert_pairs(data)
        self.assertEqual(data['session'], 2)
        self.assertEqual(data['events'][0]['operation'], 'ClothProcess.Dispose')

    def test_contract_failures_make_no_hook_changes(self):
        for mode in ('off-thread', 'missing-api', 'static', 'wrong-return', 'wrong-parameter',
                     'alias', 'external-alias', 'missing-address', 'ambiguous', 'shared-target',
                     'missing-icall', 'wrong-icall-signature', 'non-icall'):
            with self.subTest(mode=mode):
                data = self.trace(mode)
                self.assertEqual(data['session'], 0)
                self.assertEqual(data['events'], [])

    def test_partial_hook_installation_remains_passive_and_can_resume(self):
        for mode in ('create-failure', 'enable-failure'):
            with self.subTest(mode=mode):
                self.assertEqual(self.trace(mode)['events'], [])

    def test_installed_target_cannot_silently_change(self):
        self.assertEqual(self.trace('stale-target')['events'], [])

    def test_private_window_messages_have_distinct_dispatch_ids(self):
        messages = {}
        pattern = r'#define\s+(WM_EIEM_\w+)\s+\(WM_APP\s*\+\s*(0x[0-9a-fA-F]+|\d+)\)'
        for path in (ROOT / 'src').glob('*.h'):
            for name, offset in re.findall(pattern, path.read_text(encoding='utf-8')):
                value = int(offset, 0)
                self.assertNotIn(value, messages, f'{name} conflicts with {messages.get(value)}')
                messages[value] = name
        self.assertIn('WM_EIEM_MOD_RECONCILE', messages.values())
        self.assertIn('WM_EIEM_MOD_KEY', messages.values())

    def test_physics_diagnostics_are_manual_bounded_and_absent_from_dump_ui(self):
        gui = (ROOT / 'src/gui.h').read_text(encoding='utf-8')
        scene = (ROOT / 'src/scene_dump.h').read_text(encoding='utf-8')
        diagnostic = (ROOT / 'src/eiem_native_physics_diagnostic.h').read_text(encoding='utf-8')
        api = (ROOT / 'src/eiem_native_physics_api.h').read_text(encoding='utf-8')
        probe = (ROOT / 'src/eiem_native_physics_probe.h').read_text(encoding='utf-8')
        trace = (ROOT / 'src/eiem_native_physics_trace.h').read_text(encoding='utf-8')
        trojan = (ROOT / 'src/trojan.h').read_text(encoding='utf-8')
        init = (ROOT / 'src/init.h').read_text(encoding='utf-8')
        skin_timing = (ROOT / 'src/il2cpp_trace.h').read_text(encoding='utf-8')
        for text in ('原生物理诊断', '开始原生物理跟踪', '停止并导出跟踪'):
            self.assertNotIn(text, gui)
        for name in ('WM_EIEM_PHYSICS_PROBE', 'WM_EIEM_PHYSICS_TRACE_START',
                     'WM_EIEM_PHYSICS_TRACE_STOP', 'EiemRequestPhysicsTrace'):
            self.assertNotIn(name, scene + diagnostic + trojan)
        self.assertNotIn('PhysicsAutoTrace', scene)
        self.assertIn('EiemPhysicsAnimatorImage="UnityEngine.AnimationModule.dll"', api)
        self.assertIn('EiemPhysicsAnimatorImage', probe)
        self.assertIn('EiemPhysicsAnimatorImage', trace)
        self.assertNotIn('UnityEngine.CoreModule.dll', probe + trace)
        self.assertIn('plugin\\\\physics_diagnostics', diagnostic)
        self.assertIn('EiemStartPhysicsManualCaptureOnUnityThread(hwnd);', trojan)
        self.assertIn('EiemFinishPhysicsManualCaptureOnUnityThread(hwnd,"bounded-window-complete");', trojan)
        self.assertIn('EiemStartPhysicsTrace(error,kEiemPhysicsManualCaptureMs)', diagnostic)
        timer = skin_timing[skin_timing.index('static void EiemRunSkinTimingProbe()'):]
        timer = timer[:timer.index('static bool EiemRendererEligibleForRule(')]
        self.assertNotIn('EiemPhysicsStopTrace()', timer)
        # The former post-FinalIK chest probe hooked every SolverManager
        # LateUpdate and synchronously traversed Transforms/wrote TSV data on
        # the game thread.  In-game bisection proved that hook alone caused
        # visible frame jumps.  Runtime physics diagnostics must not depend on
        # this unrelated high-frequency animation hook.
        self.assertNotIn('Hooked_SolverManager_LateUpdate', init + trojan)
        self.assertNotIn('SolverManager.LateUpdate', init + trojan)
        self.assertNotIn('EiemSampleChestMotionAfterLateUpdate', trojan)
        self.assertNotIn('CHEST-MOTION', gui + scene + diagnostic)
