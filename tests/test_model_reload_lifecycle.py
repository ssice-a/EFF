"""Actual registration + complete F10 reconcile, with a simulated Unity sink."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from test_mod_controls import PREFIX, function

ROOT=Path(__file__).resolve().parents[1]
EXTRA=r'''
#include "eiem_mod_update.h"
#include "eiem_render_replay.h"
static thread_local EiemRenderReplayLedger *s_eiemActiveRenderReplay=nullptr;
static EiemModUpdateQueue s_eiemModUpdates;
static SRWLOCK s_eiemNativeSkinRefreshLock=SRWLOCK_INIT;
static std::vector<uintptr_t> s_eiemNativeSkinRefreshModels;
static bool g_shutdownRequested=false;
static DWORD s_eiemUnityThreadId=0;
static HWND g_gameHwnd=nullptr;
static constexpr UINT_PTR kEiemModRetryTimer=0xE13A;
static constexpr UINT_PTR kEiemModReplayTimer=0xE13B;
static volatile LONG s_eiemSkinTimingProbeSequence=0;
static void EiemLogReplacementNativeMeshSnapshot(const char *) {}
static void EiemLogSkinTimingProbe(const char *,LONG) {}
static void EiemArmSkinTimingProbe(LONG) {}
static void *g_gameObject_GetComponentsInChildren=(void *)1;
static SRWLOCK s_eiemInputLock=SRWLOCK_INIT;
static std::vector<EiemModInputEvent> s_eiemPendingInputs;
static void EiemProbeCheckpoint(const char *,bool=false) {}
static void EiemReportCameraFade() {}
static void EiemReapplyShapeControls(const std::vector<std::string> &) {}
static void EiemRefreshShapeTransitionTimer() {}
static void EiemCollectSkeletonInstances() {}
static void EiemPhysicsRuntimeBoundary(const char *) {}
static size_t EiemPhysicsRuntimeRetireChangedAssets(const char *) { return 0; }
static thread_local bool s_eiemPhysicsLifecycleTransaction = false;
static void EiemReconcileModelPhysics(
    void *, const std::vector<EiemPhysicsIntent> &, bool, const char *) {}
static bool failRestoreBatch=false;
static bool EiemRestoreRenderOverrides(
    const std::vector<std::string> * = nullptr) {
  if(failRestoreBatch) appliedGenerations.erase((void *)11);
  else appliedGenerations.clear();
  appliedModels.clear();
  for (const auto &entry : appliedGenerations) appliedModels.push_back(entry.first);
  return !failRestoreBatch;
}
static uint32_t EiemReapplySubmeshVisibility(
    const std::vector<EiemSubmeshVisibilityChange> &) { return 0; }
'''
SINK=r'''
static std::vector<void *> appliedModels;
static std::map<void *, LONG> appliedGenerations;
static bool EiemApplyStandaloneRenderRules(void *model,const char *,bool *matched=nullptr,
                                         const std::vector<std::string> * = nullptr,
                                         std::vector<EiemPhysicsIntent> * = nullptr,
                                         bool = true) {
  std::vector<EiemModRule> rules; EiemFindStandaloneRenderRules(&rules);
  const bool hit=simulateMatch && !rules.empty();
  if(matched) *matched=hit;
  if(hit && rules[0].hasMesh) {
    appliedGenerations[model] = s_eiemModGeneration;
    appliedModels.clear();
    for (const auto &entry : appliedGenerations) appliedModels.push_back(entry.first);
    return true;
  }
  return false;
}
'''
MAIN=r'''
static void WriteMod(bool enabled) {
  CreateDirectoryA("plugin",nullptr); CreateDirectoryA("plugin/mods",nullptr); CreateDirectoryA("plugin/mods/test",nullptr);
  std::ofstream file("plugin/mods/test/mod.ini");
  if(enabled) file << "[MeshBody]\npath=meshes/body.mesh\n[RenderBody]\nasset=Body\nmesh=MeshBody\n";
  else file << "; disabled\n";
}
static void WriteInvalidMod() {
  std::ofstream file("plugin/mods/test/mod.ini");
  file << "[RenderBody]\nasset=Body\nmesh=MissingMesh\n";
}
static void Reload() {
  s_eiemModUpdates.Request(EiemModUpdate::Reload);
  EiemRunModReconcile(); // one ordered restore, publish and replay transaction
}
int main() {
  WriteMod(false); EiemReloadMods();
  EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart,(void *)1,(void *)11,nullptr,1,"first-arrival",true);
  EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart,(void *)2,(void *)12,nullptr,2,"second-arrival",true);
  CHECK(s_eiemModelInstances.size()==2 && appliedModels.empty());
  for(int i=0;i<3;++i) {
    WriteMod(true); Reload();
    CHECK(appliedModels==std::vector<void *>({(void *)11,(void *)12}));
    WriteMod(false); Reload();
    CHECK(appliedModels.empty() && s_eiemModelInstances.size()==2);
  }
  WriteMod(true); Reload();
  const LONG beforeFailure=s_eiemModGeneration;
  WriteMod(false); failRestoreBatch=true; Reload();
  CHECK(s_eiemModGeneration==beforeFailure);
  CHECK(appliedModels==std::vector<void *>({(void *)11,(void *)12}));
  CHECK(appliedGenerations[(void *)11]==beforeFailure && appliedGenerations[(void *)12]==beforeFailure);
  failRestoreBatch=false; Reload();
  CHECK(s_eiemModGeneration==beforeFailure+1 && appliedModels.empty());
  // Recycled owner, while rules are completely absent: release previous model too.
  EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart,(void *)1,(void *)13,nullptr,3,"owner-reuse",true);
  CHECK(s_eiemModelInstances.size()==2);
  WriteMod(true); Reload();
  CHECK(appliedModels==std::vector<void *>({(void *)12,(void *)13}));
  const LONG validGeneration=s_eiemModGeneration;
  const auto validApplied=appliedModels;
  WriteInvalidMod(); Reload();
  CHECK(s_eiemModGeneration==validGeneration && appliedModels==validApplied);
  WriteMod(true);
  // A disappeared native model must be pruned even if no release hook arrived.
  objectStatus[(void *)12]=0;
  Reload(); CHECK(appliedModels==std::vector<void *>({(void *)13}));
  CHECK(s_eiemModelInstances.size()==1);
  // Unknown validity is not treated as alive or silently discarded.
  objectStatus[(void *)13]=-1;
  Reload(); CHECK(appliedModels.empty() && s_eiemModelInstances.size()==1);
  objectStatus[(void *)13]=1;
  Reload(); CHECK(appliedModels==std::vector<void *>({(void *)13}));
  // A new wrapper at the same address must not inherit the former owner.
  ++objectSerials[(void *)13];
  EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart,(void *)3,(void *)13,nullptr,99,"address-reuse",true);
  CHECK(s_eiemModelInstances.size()==1 && s_eiemModelInstances[0].owners.size()==1);
  CHECK(s_eiemModelInstances[0].owners[0].owner==(void *)3);
  EiemForgetModelOwner(EiemModelOwnerKind::BaseModelPart,(void *)1,"owner-release");
  CHECK(s_eiemModelInstances.size()==1);
  EiemForgetModelOwner(EiemModelOwnerKind::BaseModelPart,(void *)3,"owner-release");
  CHECK(s_eiemModelInstances.empty());
  // Several world/UI/NPC adapters may observe one concrete model. Keep every
  // owner so retiring the first four cannot release a still-live instance.
  for (uintptr_t owner=101; owner<=106; ++owner)
    EiemRegisterAndApplyModelInstance(EiemModelOwnerKind::BaseModelPart,
        (void *)owner,(void *)20,nullptr,0,"shared-model",false);
  CHECK(s_eiemModelInstances.size()==1 && s_eiemModelInstances[0].owners.size()==6);
  // A stale control event must not swallow a native skin refresh coalesced
  // into the same wake-up message.
  WriteMod(true); Reload();
  s_eiemNativeSkinRefreshModels.push_back((uintptr_t)20);
  s_eiemPendingInputs.push_back({{},s_eiemModGeneration-1,"stale/mod.ini"});
  s_eiemModUpdates.Request(EiemModUpdate::Reapply);
  s_eiemModUpdates.Request(EiemModUpdate::SkinRefresh);
  EiemRunModReconcile();
  CHECK(appliedModels==std::vector<void *>({(void *)20}));
  CHECK(s_eiemNativeSkinRefreshModels.empty());
  for (uintptr_t owner=101; owner<=105; ++owner)
    EiemForgetModelOwner(EiemModelOwnerKind::BaseModelPart,(void *)owner,"owner-release");
  CHECK(s_eiemModelInstances.size()==1 && s_eiemModelInstances[0].owners.size()==1);
  EiemForgetModelOwner(EiemModelOwnerKind::BaseModelPart,(void *)106,"owner-release");
  CHECK(s_eiemModelInstances.empty());
  return 0;
}
'''

class ModelReloadLifecycle(unittest.TestCase):
    def test_empty_start_multimodel_reload_reuse_expiry_and_release(self):
        if not shutil.which('cl'): self.skipTest('Requires MSVC')
        from runtime_source import read_runtime_source
        trace=read_runtime_source(ROOT)
        start=PREFIX.index('static bool EiemApplyStandaloneRenderRules(')
        end=PREFIX.index('static void EiemStoreModelPhysicsIntents(',start)
        prefix=PREFIX[:start]+SINK+PREFIX[end:]
        prefix=prefix.replace('struct EiemUnityRef {', '#include <map>\nstatic std::map<void *,int> objectStatus,objectSerials;\nstruct EiemUnityRef {')
        prefix=prefix.replace('void *p=nullptr;', 'void *p=nullptr; int serial=0;')
        prefix=prefix.replace('return {p};', 'return {p,objectSerials[p]};')
        prefix=prefix.replace('return p;', 'return p && serial==objectSerials[p]?p:nullptr;')
        prefix=prefix.replace('return p?1:0;', 'return !Target()?0:objectStatus.count(p)?objectStatus[p]:1;')
        registration='static bool EiemRegisterAndApplyModelInstance('
        code=function(trace[trace.rindex(registration):],registration)
        for sig in ('static void EiemForgetModelOwner(', 'static void EiemForgetModelInstance(',
                    'static void EiemPruneModelInstances(', 'static std::vector<EiemModelInstanceState> EiemSnapshotModelInstances()', 'static void EiemRunModReconcile()'):
            code+='\n'+function(trace[trace.rindex(sig):],sig)
        with tempfile.TemporaryDirectory(prefix='eiem-model-reload-') as tmp:
            folder=Path(tmp); source=folder/'test.cpp'; exe=folder/'test.exe'
            source.write_text(prefix+EXTRA+code+MAIN,encoding='utf-8')
            build=subprocess.run(['cl','/nologo','/EHsc','/std:c++17','/utf-8',f'/I{ROOT/"src"}',str(source),f'/Fe{exe}','user32.lib'],cwd=folder,capture_output=True,text=True,encoding='utf-8',errors='replace')
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            result=subprocess.run([str(exe)],cwd=folder,capture_output=True,text=True,errors='replace')
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
