from pathlib import Path
import unittest
from runtime_source import read_runtime_source


ROOT = Path(__file__).resolve().parents[1]


class NativePhysicsRuntimeContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.runtime = (ROOT / "src" / "eiem_native_physics_runtime.h").read_text(
            encoding="utf-8"
        )
        cls.skeleton_runtime = (
            ROOT / "src" / "eiem_skeleton_runtime.h"
        ).read_text(encoding="utf-8")
        cls.trace = read_runtime_source(ROOT)
        cls.registration = (
            ROOT / "src" / "eiem_registration_trace.h"
        ).read_text(encoding="utf-8")
        cls.mods = (ROOT / "src" / "eiem_mods.h").read_text(encoding="utf-8")
        cls.features = (ROOT / "src" / "eiem_runtime_features.h").read_text(encoding="utf-8")
        cls.diagnostic = (
            ROOT / "src" / "eiem_native_physics_diagnostic.h"
        ).read_text(encoding="utf-8")
        cls.trojan = (ROOT / "src" / "trojan.h").read_text(encoding="utf-8")

    def test_resource_runtime_is_automatic_and_separate_from_dump_ui(self):
        self.assertIn("kEiemEnableExperimentalPhysicsRuntime = false", self.features)
        self.assertIn("kEiemEnableNativePhysicsObservation = false", self.features)
        self.assertIn('#include "eiem_native_physics_runtime.h"', self.trace)
        self.assertNotIn('eiem_native_physics_factory_probe.h', self.trace)
        self.assertNotIn("EiemPhysicsRuntimePeriodic", self.trojan)
        self.assertIn('EiemPhysicsRuntimeBoundary("mod reconcile begin")', self.trace)
        self.assertIn("s_eiemPhysicsRuntimePendingReady", self.runtime)
        self.assertIn("EiemPhysicsRuntimeRetireChangedAssets", self.runtime)
        self.assertNotIn("ImGui", self.runtime)
        self.assertNotIn("scene_dump", self.runtime)

    def test_production_physics_is_frozen_with_two_independent_gates(self):
        self.assertIn("if (!kEiemEnableExperimentalPhysicsRuntime) return true;", self.mods)
        build = self.runtime[
            self.runtime.index("static bool EiemPhysicsRuntimeBuild") :
            self.runtime.index("static void EiemReconcileModelPhysics")
        ]
        self.assertIn("if (!kEiemEnableExperimentalPhysicsRuntime) return false;", build)
        reconcile = self.runtime[
            self.runtime.index("static void EiemReconcileModelPhysics") :
            self.runtime.index("static size_t EiemPhysicsRuntimeRetireChangedAssets")
        ]
        self.assertIn("if (!kEiemEnableExperimentalPhysicsRuntime) return;", reconcile)
        self.assertIn(
            "if (!kEiemEnableNativePhysicsObservation || !hwnd) return;",
            self.diagnostic,
        )

    def test_physics_mode_is_reported_once_at_resource_trace_start(self):
        self.assertIn("[PHYSICS-MODE] experimentalRuntime=%s nativeObservation=%s", self.trace)

    def test_runtime_consumes_renderer_hits_and_selects_one_animator(self):
        self.assertIn("intent.matchedRenderers", self.runtime)
        self.assertIn("EiemPhysicsRuntimeRendererUnderModel", self.runtime)
        self.assertIn("EiemPhysicsRuntimeRendererDepth", self.runtime)
        self.assertIn("ancestors != 1", self.runtime)
        self.assertIn("Physics Render matches span different Animators", self.runtime)

    def test_configuration_precedes_build_on_an_inactive_host(self):
        body = self.runtime[
            self.runtime.index("static bool EiemPhysicsRuntimeBuild") :
            self.runtime.index("static void EiemReconcileModelPhysics")
        ]
        for earlier, later in (
            ("EiemPhysicsRuntimeNewHost", "addComponent"),
            ("addComponent", "disableAutoBuild"),
            ("disableAutoBuild", "setSerializeData"),
            ("setSerializeData", "bool active = true"),
            ("bool active = true", "buildAndRun"),
        ):
            self.assertLess(body.index(earlier), body.rindex(later))
        host = self.runtime[
            self.runtime.index("static bool EiemPhysicsRuntimeNewHost") :
            self.runtime.index("static void EiemPhysicsRuntimeBeginRetire")
        ]
        self.assertIn("bool active = false", host)

    def test_component_keeps_constructor_owned_serialize_data2(self):
        self.assertNotIn("serializeData2Field", self.runtime)
        self.assertIn("componentData2Ref", self.runtime)
        self.assertIn("getSerializeData2", self.runtime)
        self.assertIn("actualData2", self.runtime)
        self.assertIn("EiemUnityRef::Capture(actualData2, false)", self.runtime)

    def test_ready_requires_live_process_and_expected_animator(self):
        poll = self.runtime[
            self.runtime.index("static void EiemPhysicsRuntimeCheckReady") :
            self.runtime.index("static void EiemPhysicsRuntimeBoundary")
        ]
        for required in ("processValid", "processRunning", "processTeamId"):
            self.assertIn(required, poll)
        self.assertIn("animator != instance->animator", poll)
        self.assertIn("instance->ready = true", poll)

    def test_retirement_destroys_owned_host_and_collider_objects_then_waits_for_native_death(self):
        self.assertNotIn("DisposeInternal", self.runtime)
        self.assertNotIn("DestroyClothBindings", self.runtime)
        self.assertIn("g_object_destroy", self.runtime)
        self.assertIn("destroyNeeded", self.runtime)
        self.assertIn("EiemPhysicsRuntimeDead", self.runtime)
        dead = self.runtime[
            self.runtime.index("static bool EiemPhysicsRuntimeDead") :
            self.runtime.index("static size_t EiemPhysicsRuntimeCollect")
        ]
        self.assertIn("hostRef.Status()", dead)
        self.assertIn("componentRef.Status()", dead)
        self.assertIn("collider.gameObjectRef.Status()", dead)
        self.assertIn("collider.transformRef.Status()", dead)
        release = self.runtime[
            self.runtime.index("static void EiemReleaseModelPhysics") :
            self.runtime.index("static void EiemPhysicsRuntimeCheckReady")
        ]
        self.assertIn("s_eiemPhysicsPendingReleases.push_back", release)
        self.assertIn("EiemPhysicsRuntimeDrainReleases", self.runtime)
        self.assertIn("EiemPhysicsRuntimeReleaseOnUnityThread", self.runtime)

    def test_author_colliders_are_shared_by_group_and_bound_before_build(self):
        build = self.runtime[
            self.runtime.index("static bool EiemPhysicsRuntimeBuild") :
            self.runtime.index("static void EiemReconcileModelPhysics")
        ]
        for earlier, later in (
            ("EiemPhysicsRuntimeCreateColliders", "EiemPhysicsRuntimeBindColliders"),
            ("EiemPhysicsRuntimeBindColliders", "EiemPhysicsRuntimeActivateColliders"),
            ("EiemPhysicsRuntimeActivateColliders", "buildAndRun"),
        ):
            self.assertLess(build.index(earlier), build.rindex(later))
        self.assertIn("source.span + source.radius + endRadius", self.runtime)
        self.assertIn("void *parentArgs[] = {binding->second, &keepWorld}", self.runtime)
        self.assertIn("components.emplace(collider.id, collider.component)", self.runtime)
        self.assertIn("instance.asset->physics.groups[groupIndex].colliders", self.runtime)

    def test_missing_ui_collider_owner_is_an_explicit_physics_anchor(self):
        self.assertIn("virtualSkeletonPaths", self.runtime)
        self.assertIn("&virtualSkeletonPaths", self.runtime)
        self.assertIn("!node.source || !source[i]", self.skeleton_runtime)
        self.assertIn("virtualPaths && virtualPaths->count(node.path)", self.skeleton_runtime)

    def test_inactive_model_retires_native_physics(self):
        reconcile = self.runtime[
            self.runtime.index("static void EiemReconcileModelPhysics") :
            self.runtime.index("static void EiemPhysicsRuntimeReleaseOnUnityThread")
        ]
        self.assertIn("if (!active)", reconcile)
        self.assertIn("EiemPhysicsRuntimeBeginRetire(instance, true, stage)", reconcile)
        self.assertNotIn("(void)active", reconcile)

    def test_loader_publishes_physics_mods_for_the_runtime_adapter(self):
        self.assertNotIn("if (requestsPhysics)", self.mods)
        self.assertNotIn("native Physics execution is not connected (file skipped)", self.mods)
        self.assertIn("EiemAppendModDocument(next,std::move(document))", self.mods)
        self.assertIn("EiemReconcileModelPhysics", self.trace)

    def test_binding_log_reports_mesh_palette_membership(self):
        self.assertIn("g_smr_get_bones", self.runtime)
        self.assertIn("paletteHits", self.runtime)
        self.assertIn("palette=%zu selected=%zu paletteHits=%zu boundaryIgnores=%zu", self.runtime)
        self.assertIn("boundaryIgnores += group.boundaryIgnores.size()", self.runtime)

    def test_runtime_observes_visible_palette_and_move_node_writeback(self):
        self.assertNotIn("EiemPhysicsRuntimeLogPartnerBinding", self.runtime)
        self.assertNotIn("EiemPhysicsRuntimeObserveMotion", self.runtime)
        self.assertNotIn("maxLocalPositionDeltaSq", self.runtime)
        self.assertNotIn("instance.motionSamples", self.runtime)

    def test_deterministic_failure_is_not_rebuilt_until_a_new_boundary(self):
        self.assertIn("struct EiemPhysicsRuntimeFailure", self.runtime)
        self.assertIn("uint64_t assetStamp", self.runtime)
        self.assertIn("EiemPhysicsRuntimeAssetStamp", self.runtime)
        self.assertIn("EiemPhysicsRuntimeFailureMatches", self.runtime)
        self.assertIn("EiemPhysicsRuntimeRememberFailure", self.runtime)
        reconcile = self.runtime[
            self.runtime.index("static void EiemReconcileModelPhysics") :
            self.runtime.index("static void EiemPhysicsRuntimeReleaseOnUnityThread")
        ]
        self.assertIn("if (!failed && !EiemPhysicsRuntimeBuild", reconcile)
        self.assertIn("&&\n        !retryable", reconcile)
        self.assertIn("retry-suppressed", reconcile)
        self.assertIn("failure.assetStamp ==", reconcile)

    def test_manual_capture_finishes_trace_before_export(self):
        finish = self.diagnostic[
            self.diagnostic.index("static void EiemFinishPhysicsManualCaptureOnUnityThread") :
            self.diagnostic.index("static void EiemStartPhysicsManualCaptureOnUnityThread")
        ]
        self.assertNotIn("EiemPhysicsFactoryProbe", finish)
        self.assertLess(finish.index("EiemPhysicsStopTrace"),
                        finish.index("EiemWriteNativePhysicsDiagnostic"))


if __name__ == "__main__":
    unittest.main()
