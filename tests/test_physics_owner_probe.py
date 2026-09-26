from runtime_source import read_runtime_source
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PhysicsOwnerContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.owner = (ROOT / "src" / "eiem_npc_model_owner.h").read_text(
            encoding="utf-8"
        )
        cls.trace = read_runtime_source(ROOT)
        cls.registration = (
            ROOT / "src" / "eiem_registration_trace.h"
        ).read_text(encoding="utf-8")

    def test_npc_adapter_contains_only_real_owner_boundaries(self):
        for method in ("StartNPC", "ReleaseAvatar", "OnRelease"):
            self.assertIn(f'"{method}"', self.owner)
        self.assertNotIn("_BuildBeyondCloth", self.owner)
        self.assertNotIn("EiemPhysicsOwner", self.owner)
        self.assertIn("EiemRegisterAndApplyModelInstance", self.owner)

    def test_start_registers_after_game_owned_start(self):
        start = self.owner[
            self.owner.index("static void EiemNpcTraceStartNpc") :
            self.owner.index("static void EiemNpcTraceReleaseAvatar")
        ]
        self.assertIn("original(avatar, component, methodInfo)", start)
        self.assertLess(
            start.index("original(avatar, component, methodInfo)"),
            start.index("EiemRegisterAndApplyModelInstance"),
        )
        self.assertIn("EiemRegistrationTraceOwnerState", start)
        self.assertIn("EiemModelOwnerKind::NpcAvatar", start)

    def test_release_forgets_owner_before_game_clears_it(self):
        for signature, original in (
            ("static void EiemNpcTraceReleaseAvatar", "original(manager, component, methodInfo)"),
            ("static void EiemNpcTraceOnRelease", "original(component, methodInfo)"),
        ):
            start = self.owner.index(signature)
            end = self.owner.index("\n}", start)
            body = self.owner[start:end]
            self.assertIn("EiemForgetModelOwner(EiemModelOwnerKind::NpcAvatar", body)
            self.assertLess(body.index("EiemForgetModelOwner"), body.index(original))

    def test_install_has_no_research_cloth_hook(self):
        install = self.owner[self.owner.index("static void EiemInstallNpcModelOwner") :]
        self.assertEqual(install.count("\n  EiemNpcHookExact("), 3)
        self.assertNotIn("BuildBeyondCloth", install)

    def test_unified_trace_is_low_volume_and_observation_only(self):
        self.assertIn("EiemRegistrationTraceFirst", self.registration)
        self.assertIn("[INSTANCE-REG-v3]", self.registration)
        self.assertIn("event=partner", self.registration)
        self.assertIn("event=lod", self.registration)
        self.assertIn("skeletonAnchor", self.registration)
        self.assertIn("sourceLevels", self.registration)
        self.assertIn("bindposeHash", self.registration)
        self.assertIn("event=partner-pose", self.registration)
        self.assertIn("event=renderer-owner", self.registration)
        self.assertIn("event=lod-group-member", self.registration)
        self.assertIn("event=release", self.registration)
        self.assertNotIn("AddComponent", self.registration)
        self.assertNotIn("g_object_destroy", self.registration)

    def test_production_trace_uses_unified_events(self):
        self.assertIn("EiemRegistrationTraceModel", self.trace)
        self.assertIn("EiemRegistrationTraceRelease", self.trace)
        self.assertIn("EiemRegistrationTraceRenderer", self.trace)
        self.assertNotIn("EiemTraceRendererOwnerCorrelation", self.trace)
        self.assertIn("ownerPrefabInstance", self.trace)
        self.assertNotIn("EiemRegistrationTraceLod", self.trace)
        self.assertNotIn("TraceLodGroupSetLODs", self.trace)
        self.assertNotIn("EiemRegistrationTraceLodGroupSet", self.trace)
        self.assertNotIn("EiemTraceLodGroupMembers", self.trace)
        self.assertIn("EiemReadLodRendererMesh", self.trace)
        self.assertNotIn("EiemReconcilePartnerLodGroup", self.trace)
        self.assertIn("EiemRegistrationTraceEligibility", self.trace)
        self.assertIn('event=eligibility', self.registration)
        self.assertNotIn("EiemPhysicsOwnerProbeObserve", self.trace)
        self.assertNotIn("[LIFECYCLE-PROBE]", self.trace)


if __name__ == "__main__":
    unittest.main()
