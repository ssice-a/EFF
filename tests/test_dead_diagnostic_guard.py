"""Retired diagnostic paths stay out of the production translation unit."""
from pathlib import Path
import unittest
from runtime_source import read_runtime_source

ROOT = Path(__file__).resolve().parents[1]
TRACE = read_runtime_source(ROOT)


class DeadDiagnosticGuardTests(unittest.TestCase):
    def test_retired_budget_stub_and_dead_debug_path_are_removed(self):
        self.assertNotIn('TraceTakeBudget(', TRACE)
        self.assertNotIn('[DEBUG-SKIN-BONES]', TRACE)
        self.assertNotIn('[DEBUG-matlifecycle]', TRACE)

    def test_hot_skin_setter_does_not_run_legacy_pose_measurement(self):
        start = TRACE.rindex('static void TraceSkinnedMeshSetBones(')
        end = TRACE.index('static bool EiemReadBoxedBool', start)
        body = TRACE[start:end]
        self.assertNotIn('EiemSkinProbe::Measure', body)
        self.assertNotIn('EiemSnapshotPartnerBones', body)
        self.assertNotIn('EiemArmSkinProbeSweep', body)

    def test_current_assembly_boundaries_do_not_recreate_partner_renderers(self):
        for name in ('TraceAssignSkinGo', 'TraceAssignSkinPost',
                     'TraceSetSmrRootBone'):
            start = TRACE.rindex(f'static void {name}')
            body = TRACE[start:start + 2400]
            self.assertNotIn('EiemApplyPartners', body)
            self.assertNotIn('EiemRegisterPartnersInSkinArrays', body)


if __name__ == '__main__':
    unittest.main()
