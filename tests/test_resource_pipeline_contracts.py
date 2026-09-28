from pathlib import Path
import unittest
from runtime_source import read_runtime_source


ROOT = Path(__file__).resolve().parents[1]


class RuntimeHookContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.trace = read_runtime_source(ROOT)
        cls.features = (ROOT / "src" / "eiem_runtime_features.h").read_text(encoding="utf-8")
        cls.mods = (ROOT / "src" / "eiem_mod_document.h").read_text(encoding="utf-8") + (ROOT / "src" / "eiem_mods.h").read_text(encoding="utf-8")
        cls.api = (ROOT / "src" / "il2cpp_api.h").read_text(encoding="utf-8")
        cls.trojan = (ROOT / "src" / "trojan.h").read_text(encoding="utf-8")
        cls.backend = (ROOT / "src" / "eiem_resource_backend.h").read_text(
            encoding="utf-8"
        )
        cls.init = (ROOT / "src" / "init.h").read_text(encoding="utf-8")
        cls.dispatcher = (ROOT / "src" / "eiem_mod_dispatcher.h").read_text(
            encoding="utf-8"
        )

    def test_model_instances_have_prefab_and_ui_lifecycle_adapters(self):
        self.assertIn("TracePrefabInstantiateCompleted", self.trace)
        self.assertIn('"PrefabInstantiateProxy.OnCompleted"', self.trace)
        self.assertIn("TraceUIModelLoaderLoadModel", self.trace)
        self.assertIn("TraceUIModelLoaderLoadModelAsync", self.trace)
        self.assertNotIn("EiemUIModelLoadedCallback", self.trace)
        self.assertIn("EiemRegisterAndApplyModelInstance", self.trace)
        self.assertIn("EiemApplyStandaloneRenderRules", self.trace)
        self.assertNotIn("UnityEngine.Object.Instantiate", self.trace)
        self.assertNotIn("TraceApplyLoadedModelRenderers", self.trace)
        self.assertNotIn("EiemResolveRenderRuleForAsset", self.trace)

    def test_model_scan_replaces_inactive_authored_lod_siblings(self):
        start = self.trace.index("static bool EiemApplyRenderRuleSetToRenderer")
        end = self.trace.index("static bool EiemApplyRenderRuleSet(", start)
        matcher = self.trace[start:end]
        self.assertIn("EiemRendererEligibleForRule(meshOwner, drawRenderer,", matcher)
        self.assertIn("includeGameHidden", matcher)

        scan_start = self.trace.index("static bool EiemApplyRenderRuleSet(")
        scan_end = self.trace.index("static bool EiemApplyStandaloneRenderRules(", scan_start)
        scan = self.trace[scan_start:scan_end]
        self.assertIn("bool includeInactive = true", scan)
        self.assertIn(
            "snapshotType(g_skinnedMeshRendererClass, &skinnedRenderers)", scan
        )
        self.assertIn("snapshotType(g_meshFilterClass, &meshFilters)", scan)
        self.assertIn('visitType(skinnedRenderers, "SkinnedMeshRenderer")', scan)
        self.assertIn('visitType(meshFilters, "MeshFilter")', scan)
        self.assertNotIn("allowPartnerCreation", scan)
        self.assertIn('"inactive-authored-lod"', self.trace)
        self.assertIn('"force-off-authored-lod"', self.trace)
        self.assertIn('"disabled-authored-lod"', self.trace)
        self.assertIn("EiemRegistrationTraceEligibility", self.trace)
        self.assertIn('"force-off"', self.trace)
        self.assertIn('"disabled-unowned"', self.trace)

    def test_failed_mesh_does_not_commit_dependent_material_state(self):
        start = self.trace.index("static bool EiemApplyResolvedRenderRule")
        end = self.trace.index("static void *EiemFindMeshFilterDrawRenderer", start)
        body = self.trace[start:end]
        self.assertIn(
            "bool resourceCommitted = resourcesReady && (!applyMesh || meshApplied)",
            body,
        )
        self.assertIn("resource dependent edits skipped after mesh failure", body)
        self.assertLess(body.index("if (!resourceCommitted) {\n    const bool restored"),
                        body.index("EiemRememberRuleBinding(renderer, rule)"))
        self.assertLess(
            body.index("EiemBuildRendererMaterialsForSource"),
            body.index("EiemBeginMeshWrite(renderer)"),
        )
        self.assertIn("if (resourceCommitted) {", body)

    def test_prefab_lifecycle_releases_instance_state(self):
        for method in ("Unload", "Clear", "Dispose"):
            self.assertIn("TracePrefabInstantiate" + method, self.trace)
        self.assertIn("EiemForgetRenderOverrides(modelOwner)", self.trace)

    def test_ui_async_leaves_managed_callback_and_completion_to_game(self):
        start = self.trace.rindex("static int32_t TraceUIModelLoaderLoadModelAsync")
        end = self.trace.index("static void TraceUIModelLoaderUnloadModel", start)
        body = self.trace[start:end]
        self.assertNotIn("EiemCreateUIModelCallback", self.trace)
        self.assertNotIn("TraceActionGameObjectCtorFn", self.trace)
        self.assertIn("original(self, path, parent, callback, methodInfo)", body)
        self.assertNotIn("EiemRegisterAndApplyModelInstance", body)

    def test_ui_lifecycle_releases_instances_without_native_pending_callbacks(self):
        self.assertIn("TraceUIModelLoaderUnloadModel", self.trace)
        self.assertNotIn("TraceUIModelLoaderCancel", self.trace)
        self.assertIn("TraceUIModelLoaderClear", self.trace)
        self.assertIn("TraceUIModelLoaderDispose", self.trace)
        self.assertIn("EiemForgetModelInstance(model", self.trace)
        self.assertNotIn("EiemPendingUIModelCallback", self.trace)
        self.assertNotIn("EiemCancelPendingUIModelCallbacks", self.trace)

    def test_cached_model_allocation_does_not_commit_inside_native_allocation(self):
        start = self.trace.rindex("static void TraceModelManagerGameObjectAllocate")
        body = self.trace[start : start + 700]
        self.assertIn("original(self, model, methodInfo)", body)
        self.assertNotIn("EiemReapplyRegisteredModelInstance", body)
        self.assertNotIn("EiemRegisterAndApplyModelInstance", body)

    def test_f10_reconcile_only_walks_registered_model_instances(self):
        start = self.trace.index("static void EiemRunModReconcile")
        end = self.trace.index("\n}\n", start)
        body = self.trace[start:end]
        self.assertIn("EiemSnapshotModelInstances()", body)
        self.assertIn("EiemApplyStandaloneRenderRules", body)
        self.assertNotIn("find_objects_of_type", body.lower())

    def test_historical_submesh_and_hg_observation_hooks_are_removed(self):
        for obsolete in (
            "TraceSubMeshInfoSetMesh",
            "TraceSubMeshInfoGetMesh",
            "TraceLodGetSubMeshInfo",
            "TraceGetPartCpuMesh",
            "TraceHgRendererSetData",
            "TraceHgRendererGetData",
            "TraceHgDataGetMeshes",
            "TraceHgDataSetMaterials",
            "TraceHgStateInit",
            "TraceHgStateInvalidate",
            "TraceHgStateSetVisible",
            "[TRACE-HG-FLOW]",
        ):
            self.assertNotIn(obsolete, self.trace)

    def test_vfs_observation_cannot_bypass_render_rules_with_loose_bundles(self):
        for obsolete in ("TraceFindVfsOverride", "TraceTryBundleOverride",
                         "TraceLoadPlaintextBundle", "TraceCreateManagedPath",
                         "s_assetBundleLoadFromFile", "s_vfsThreadAttached"):
            self.assertNotIn(obsolete, self.trace)
        for method in ("TraceVfsLoadBundleFromFile", "TraceVfsLoadBundleFromFileAsync",
                       "TraceVfsLoadBundleFromFilePos", "TraceVfsLoadBundleFromFileAsyncPos"):
            start = self.trace.index("static void *" + method + "(")
            end = self.trace.index("\n}\n", start)
            body = self.trace[start:end]
            self.assertIn("original ? original(self, path,", body)
            self.assertIn("TraceRememberBundlePath(path)", body)
            self.assertNotIn("GetFileAttributes", body)

    def test_base_model_view_part_completion_registers_exact_model_path(self):
        self.assertIn("EiemModelOwnerKind::BaseModelPart", self.trace)
        self.assertIn("EiemRegisterBaseModelViewPartInstance", self.trace)

        finish = self.trace[
            self.trace.rindex("static void TraceBasePartFinish") :
        ][:1200]
        self.assertIn("EiemRegisterBaseModelViewPartInstance", finish)

        callback = self.trace[
            self.trace.rindex("static void TraceBasePartLoadUseHandleFinishCallback") :
        ][:1200]
        self.assertIn("EiemRegisterBaseModelViewPartInstance", callback)

        result = self.trace[
            self.trace.rindex("static bool TraceBasePartLoadUseHandleFinish") :
        ][:1200]
        self.assertIn("EiemRegisterBaseModelViewPartInstance", result)
        self.assertIn("TraceBasePartReleaseModel", self.trace)
        self.assertIn("TraceBasePartOnRelease", self.trace)
        self.assertIn(
            "EiemForgetModelOwner(EiemModelOwnerKind::BaseModelPart",
            self.trace,
        )

    def test_character_ui_owner_can_apply_mesh_rules_without_prefab_identity(self):
        self.assertIn("EiemModelOwnerKind::CharUIModel", self.trace)
        self.assertIn("TraceCharUIModelOnAwake", self.trace)
        self.assertIn("TraceCharUIModelSetVisible", self.trace)
        self.assertIn("TraceCharUIModelOnRelease", self.trace)
        start = self.trace.rindex("static bool EiemRegisterCharUIModelInstance")
        body = self.trace[start : start + 3200]
        self.assertIn("g_component_get_gameObject", body)
        self.assertIn("EiemRegisterAndApplyModelInstance", body)
        self.assertIn("nullptr", body)
        self.assertNotIn("FindObjectsOfType", body)
        self.assertIn(
            "EiemForgetModelOwner(EiemModelOwnerKind::CharUIModel",
            self.trace,
        )
        self.assertNotIn("EiemFindModPrefabPathByModelName", body)

    def test_npc_owner_registers_completed_model_and_releases_by_component(self):
        owner = (ROOT / "src" / "eiem_npc_model_owner.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("EiemModelOwnerKind::NpcAvatar", owner)
        start = owner[
            owner.index("static void EiemNpcTraceStartNpc") :
            owner.index("static void EiemNpcTraceReleaseAvatar")
        ]
        self.assertIn("EiemRegisterAndApplyModelInstance", start)
        release = owner[
            owner.index("static void EiemNpcTraceReleaseAvatar") :
            owner.index("static void EiemInstallNpcModelOwner")
        ]
        self.assertEqual(
            release.count("EiemForgetModelOwner(EiemModelOwnerKind::NpcAvatar"),
            2,
        )
        self.assertNotIn("_BuildBeyondCloth", owner)

    def test_physics_intents_follow_render_hits_but_are_owned_by_the_model(self):
        match_start = self.trace.index("static bool EiemApplyRenderRuleSetToRenderer")
        match_body = self.trace[match_start : match_start + 5200]
        self.assertIn("EiemCollectPhysicsIntent(rule, physicsIntents, drawRenderer)", match_body)
        self.assertLess(
            match_body.index("EiemCollectPhysicsIntent(rule, physicsIntents, drawRenderer)"),
            match_body.index("EiemModAffected(rule.modPath, affected)"),
        )
        setter_start = self.trace.rindex("static bool EiemApplyStandaloneRenderRulesToRenderer")
        setter_body = self.trace[setter_start : setter_start + 800]
        self.assertNotIn("EiemCollectPhysicsIntent", setter_body)
        self.assertIn("std::vector<EiemPhysicsIntent> physicsIntents", self.trace)
        self.assertIn("EiemStoreModelPhysicsIntents", self.trace)

    def test_character_ui_visibility_keeps_plan_and_tracks_activity(self):
        start = self.trace.rindex("static void TraceCharUIModelSetVisible")
        end = self.trace.rindex("static void TraceCharUIModelOnRelease")
        body = self.trace[start:end]
        self.assertIn("EiemRegisterCharUIModelInstance", body)
        self.assertIn("EiemSetModelOwnerActive", body)
        self.assertNotIn("EiemForgetModelOwner", body)
        release_start = self.trace.rindex("static void TraceCharUIModelOnRelease")
        release_body = self.trace[release_start : release_start + 500]
        self.assertIn("EiemForgetModelOwner", release_body)

    def test_npc_final_bone_boundary_remains_observation_only(self):
        start = self.trace.rindex("static void TraceSetSmrRootBone")
        end = self.trace.index(
            "static size_t EiemApplyStandaloneRenderRulesToSkinArray", start
        )
        body = self.trace[start:end]
        self.assertIn(
            "original(animator, renderers, rootBoneInfos, methodInfo)", body
        )
        self.assertNotIn("EiemApplyStandaloneRenderRules", body)
        self.assertNotIn("EiemFindPrefabRenderRuleByAsset", self.mods)

    def test_prefab_parser_supports_multiple_mods_for_one_path(self):
        self.assertIn("struct EiemModPrefab", self.mods)
        self.assertIn("EiemFindModPrefabs", self.mods)
        body = self.mods[self.mods.index("static void EiemFindModPrefabs") :]
        self.assertIn("out->push_back(prefab)", body[:800])

    def test_asset_only_render_selector_does_not_require_transform_path(self):
        start = self.trace.index("static bool EiemApplyRenderRuleSetToRenderer")
        body = self.trace[start : start + 4200]
        self.assertNotIn(
            "!model || !EiemOnUnityThread() || !g_gameObject_get_transform",
            body,
        )
        self.assertIn("if (rule.path[0])", body)
        self.assertIn("relativePathAttempted", body)

    def test_game_material_controller_commits_reapply_bound_rules(self):
        self.assertIn("TraceRendererInfoTrySetSharedMaterial", self.trace)
        self.assertIn("TraceRendererInfoTrySetSharedMaterials", self.trace)
        self.assertIn("TraceRendererInfoTryReplaceSharedMaterials", self.trace)
        self.assertIn("EiemFindBoundRenderRule", self.trace)

    def test_renderer_info_init_reapplies_materials_after_game_commit(self):
        start = self.trace.index("static void TraceMaterialInfoInit")
        end = self.trace.index("static bool TraceRendererInfoTrySetSharedMaterial", start)
        body = self.trace[start:end]
        self.assertLess(
            body.index("original(self, renderer, configs, methodInfo)"),
            body.index("EiemApplyStandaloneRenderRulesToRenderer"),
        )
        self.assertLess(
            body.index("if (s_eiemEntityRenderHelperInitGuard) {"),
            body.index("EiemApplyStandaloneRenderRulesToRenderer"),
        )
        self.assertIn("s_eiemMaterialsToReapplyAfterHelper.push_back(renderer)", body)
        material_start = self.trace.rindex(
            "static void TraceEntityRenderHelperMaterialControllerInit"
        )
        material_end = self.trace.index(
            "static void TraceEntityRenderHelperInitRenderAndMaterial",
            material_start,
        )
        helper = self.trace[material_start:material_end]
        self.assertNotIn('EiemReapplyRendererMaterialsAfterCommit(', helper)
        outer_start = self.trace.rindex(
            "static void TraceEntityRenderHelperInitRenderAndMaterial"
        )
        outer_end = self.trace.index(
            "static bool EiemApplyStandaloneRenderRulesToRenderer", outer_start
        )
        outer = self.trace[outer_start:outer_end]
        self.assertIn('EiemReapplyRendererMaterialsAfterCommit(', outer)
        self.assertLess(
            outer.index("if (original) original(self, methodInfo);", outer.index("const bool outermost")),
            outer.index('EiemReapplyRendererMaterialsAfterCommit('),
        )
        self.assertIn('"RendererInfo._Init"', body)
        self.assertNotIn("allowPartnerCreation", body)
        self.assertIn("EiemReapplyRendererMaterialsAfterCommit", body)
        self.assertNotIn("EiemApplyPrefabRules", body)

    def test_reload_replays_registered_instances_without_reentering_creation_helper(self):
        start = self.trace.index("static void EiemRunModReconcile() {")
        end = self.trace.index("static void *s_origAssetBundleLoadAsset1", start)
        body = self.trace[start:end]
        self.assertIn("EiemApplyStandaloneRenderRules(", body)
        self.assertNotIn("EiemRebuildNativeRendererRegistration(", body)

    def test_resources_apply_at_enclosing_model_assembly_boundaries(self):
        material_start = self.trace.rindex(
            "static void TraceEntityRenderHelperMaterialControllerInit"
        )
        material_end = self.trace.index(
            "static void TraceEntityRenderHelperInitRenderAndMaterial",
            material_start,
        )
        material = self.trace[material_start:material_end]
        self.assertIn("EiemLogMaterialControllerRegistry(self, renderers, \"before-original\")", material)
        self.assertLess(
            material.index("EiemApplyStandaloneRenderRules("),
            material.rindex(
                "original(self, renderers, rendererTypeConfigs,"
            ),
        )
        self.assertIn("s_eiemEntityRenderHelperActiveModel", material)

        helper_start = self.trace.rindex(
            "static void TraceEntityRenderHelperInitRenderAndMaterial"
        )
        helper_end = self.trace.index(
            "static bool EiemApplyStandaloneRenderRulesToRenderer", helper_start
        )
        helper = self.trace[helper_start:helper_end]
        self.assertNotIn(
            'EiemApplyStandaloneRenderRules(\n        model, "EntityRenderHelper._InitRenderAndMaterial-before"',
            helper,
        )
        self.assertIn('EntityRenderHelper.MaterialController.Init-before', material)
        self.assertIn("s_eiemEntityRenderHelperMaterialApplied", helper)
        self.assertNotIn("s_eiemEnclosingModelAssemblyDepth == 0", helper)

        base_start = self.trace.rindex("static void TraceBasePartPostDeal")
        base_end = self.trace.index("static void TraceComplexPartPostDeal", base_start)
        base = self.trace[base_start:base_end]
        self.assertLess(
            base.index("original(self, methodInfo)"),
            base.index("EiemRegisterBaseModelViewPartInstance"),
        )
        self.assertIn("PostDealLoadedModel\", true", base)
        self.assertNotIn("PostDealLoadedModel-before", base)

        for name in ("TraceCreateSmsGo", "TraceCreateSmsPost"):
            start = self.trace.index(f"static void {name}")
            body = self.trace[start : start + 2400]
            self.assertNotIn("EiemApplyStandaloneRenderRulesToSkinArray", body)
            self.assertIn("AssignSkin", body)

        for name in ("TraceAssignSkinGo", "TraceAssignSkinPost"):
            start = self.trace.index(f"static void {name}")
            body = self.trace[start : self.trace.index(
                "static void TraceSetSmrRootBone", start)]
            self.assertIn("original(lod, renderers", body)
            self.assertNotIn("EiemRememberAssemblyBoneSnapshot", body)
            self.assertNotIn("EiemApplyStandaloneRenderRulesToSkinArray", body)

    def test_mesh_commit_hides_every_changed_skin_transaction(self):
        start = self.trace.index("static bool EiemApplyResolvedRenderRule")
        end = self.trace.index("static bool EiemApplyRenderRuleSetToRenderer", start)
        body = self.trace[start:end]
        self.assertIn("if ((meshWillChange || bonesWillChange) &&", body)

    def test_per_renderer_mesh_setters_defer_resource_replay(self):
        for name in ("TraceSkinnedMeshSetSharedMesh", "TraceMeshFilterSetSharedMesh"):
            start = self.trace.rindex(f"static void {name}")
            body = self.trace[start : start + 1800]
            self.assertNotIn("EiemApplyStandaloneRenderRulesToRenderer", body)
            self.assertIn("completed assembly boundaries", body)

        start = self.trace.rindex("static void TraceSkinnedMeshSetBones")
        end = self.trace.index("static bool EiemReadBoxedBool", start)
        bones = self.trace[start:end]
        self.assertIn("EiemRememberGameSourceBones", bones)
        self.assertIn("EiemQueueNativeSkinRefresh", bones)
        self.assertNotIn("EiemPreserveSourceSkinning", bones)

    def test_creation_boundary_owns_cold_and_replay_registration(self):
        self.assertNotIn("EiemRunDelayedNativeRendererReplay", self.trace)
        self.assertNotIn("EiemRebuildNativeRendererRegistration", self.trace)
        self.assertNotIn("kEiemDeferInitialSkinCommit", self.features)

        reconcile = self.trace[self.trace.index("static void EiemRunModReconcile") :]
        self.assertIn("restoreComplete = EiemRestoreRenderOverrides(affected)", reconcile)
        self.assertIn("if (restoreComplete && affected) EiemPublishModState", reconcile)
        self.assertNotIn("EiemRebuildNativeRendererRegistration(\n             instance.model", reconcile)
        self.assertIn("applied = EiemApplyStandaloneRenderRules(\n", reconcile)
        self.assertIn("EiemRenderReplayLedger replayLedger", reconcile)

    def test_value_type_asset_handle_is_unboxed_before_instance_methods(self):
        self.assertIn("il2cpp_object_unbox", self.api)
        start = self.backend.index("static void *EiemLoadOriginalAsset")
        body = self.backend[start : start + 1800]
        self.assertIn("il2cpp_object_unbox(handle)", body)
        self.assertIn("Invoke(s_eiemProxyLoadImmediate, unboxedHandle)", body)
        self.assertIn("Invoke(s_eiemProxyGet, unboxedHandle)", body)

    def test_configured_target_diagnostics_are_not_character_hardcoded(self):
        self.assertNotIn('strstr(pathText, "wulfa")', self.trace)
        self.assertNotIn('_strnicmp(assetName, "S_actor_wulfa"', self.trace)
        self.assertNotIn("typhoea", self.trace.lower())
        self.assertIn("TraceIdentityTextMatchesConfiguredRule", self.trace)

    def test_asset_completion_is_observation_only(self):
        start = self.trace.index("static void TraceAssetFinishWithAsset")
        end = self.trace.index("static void TraceAssetOnComplete", start)
        body = self.trace[start:end]
        self.assertNotIn("TraceTryGlobalResourceRedirect", body)
        self.assertIn("original(self, asset, methodInfo)", body)

    def test_standalone_render_rules_are_mesh_identity_actions(self):
        self.assertIn("EiemFindStandaloneRenderRules", self.mods)
        self.assertIn("EiemApplyStandaloneRenderRules", self.trace)
        start = self.trace.rindex("static bool EiemApplyStandaloneRenderRules")
        body = self.trace[start : start + 1000]
        self.assertIn("EiemFindStandaloneRenderRules", body)
        self.assertIn("EiemApplyRenderRuleSet", body)

    def test_prefab_completion_always_offers_completed_model_to_mesh_rules(self):
        start = self.trace.rindex("static void TracePrefabInstantiateCompleted")
        body = self.trace[start : start + 2200]
        self.assertIn("EiemRegisterAndApplyModelInstance", body)
        self.assertNotIn("configured &&\n      EiemRegisterAndApplyModelInstance", body)

    def test_f10_replays_standalone_mesh_rules_for_registered_instances(self):
        start = self.trace.index("static void EiemRunModReconcile")
        end = self.trace.index("static void *s_origAssetBundleLoadAsset1", start)
        body = self.trace[start:end]
        self.assertIn("EiemApplyStandaloneRenderRules", body)

    def test_mesh_setter_matches_even_when_resource_origin_was_not_observed(self):
        start = self.trace.rindex("static void TraceSkinnedMeshSetSharedMesh")
        body = self.trace[start : start + 1800]
        self.assertNotIn("EiemApplyStandaloneRenderRulesToRenderer", body)
        self.assertNotIn("TraceTryGlobalResourceRedirect", body)

    def test_material_clones_its_declared_source_asset(self):
        start = self.backend.index("static bool EiemBuildMaterialResource")
        end = self.backend.index("static void EiemApplySubmeshMaterialMap", start)
        material_body = self.backend[start:end]
        self.assertNotIn("templateMaterial", material_body)
        self.assertIn("void *sourceMaterial = EiemLoadOriginalAsset", material_body)
        self.assertNotIn("EiemBuildGlobalResource", self.backend)

    def test_material_loader_resolves_the_game_resource_manager_without_probes(self):
        resolve_start = self.backend.index("static void EiemResolveResourceBackend")
        resolve_end = self.backend.index("template <typename T>", resolve_start)
        resolve = self.backend[resolve_start:resolve_end]
        load_start = self.backend.index("static void *EiemLoadOriginalAsset")
        load_end = self.backend.index("static bool EiemParseColor", load_start)
        load = self.backend[load_start:load_end]
        self.assertIn('FindClass("Beyond.Resource", "ResourceManager"', resolve)
        self.assertIn('FindMethod(', resolve)
        self.assertIn('"get_instance"', resolve)
        self.assertIn("EiemResolveResourceManagerInstance()", load)
        self.assertNotIn("kEiemValidationIdentityProbe", load)

    def test_mesh_and_declared_materials_are_prepared_before_renderer_mutation(self):
        start = self.trace.index("static bool EiemApplyResolvedRenderRule")
        end = self.trace.index("static void *EiemFindMeshFilterDrawRenderer", start)
        body = self.trace[start:end]
        prepare = body.index("EiemBuildRendererMaterialsForSource")
        mesh_write = body.index("EiemBeginMeshWrite(renderer)")
        material_write = body.index("EiemAssignRendererMaterials")
        self.assertLess(prepare, mesh_write)
        self.assertLess(mesh_write, material_write)
        self.assertIn("resource preparation failed before mutation", body)

    def test_skinned_mesh_commit_is_hidden_until_mesh_bones_and_materials_are_ready(self):
        start = self.trace.index("static bool EiemApplyResolvedRenderRule")
        end = self.trace.index("static void *EiemFindMeshFilterDrawRenderer", start)
        body = self.trace[start:end]
        disable = body.index("[DEBUG-HR-ATOMIC-v1]")
        mesh = body.index("EiemSetSharedMesh(renderer, assignedMesh")
        material = body.index("EiemAssignRendererMaterials")
        restore = body.index("restored renderer=%p section=%s")
        self.assertLess(disable, mesh)
        self.assertLess(mesh, material)
        self.assertLess(material, restore)
        self.assertIn("rendererEnabledBeforeCommit", body)
        self.assertIn("EiemReadRendererEnabled(drawRenderer, &actual)", body)

    def test_cross_prefab_bone_rename_uses_instance_source_palettes(self):
        resolver_start = self.trace.index(
            "static bool EiemResolveMeshBonesFromNativeInstance")
        resolver_end = self.trace.index(
            "static bool EiemPreserveSourceSkinning", resolver_start)
        resolver = self.trace[resolver_start:resolver_end]
        self.assertIn("EiemResolveMeshBonesFromSourcePalettes", resolver)
        self.assertIn("sourceCandidates", self.trace)
        self.assertNotIn("EiemResolveMeshBonesFromIndexTable", resolver)
        self.assertNotIn("EiemSkinAssetFamily", resolver)
        for character_specific in (
                "typhoe", "cloth_", "body_", "skirt_", "actor_"):
            self.assertNotIn(character_specific, resolver.lower())

        model_start = self.trace.index("static bool EiemApplyRenderRuleSet(")
        model_end = self.trace.index(
            "static bool EiemApplyStandaloneRenderRules(", model_start)
        model_pass = self.trace[model_start:model_end]
        self.assertNotIn("EiemLiveSkinCaptureContext", model_pass)
        self.assertNotIn("s_eiemLiveSkinSources", self.trace)
        self.assertIn("originalBonesHandle", self.trace)

    def test_initial_mod_rules_load_before_resource_hooks_are_enabled(self):
        start = self.init.index("static DWORD WINAPI InitThread")
        body = self.init[start:]
        self.assertLess(body.index("EiemReloadMods();"),
                        body.index("InitIl2CppResourceTrace(asms, ac);"))

        hotkey_start = self.dispatcher.index("static DWORD WINAPI HotkeyThread")
        hotkey_body = self.dispatcher[hotkey_start:]
        hotkey_setup = hotkey_body[:hotkey_body.index("enum class KeyAction")]
        self.assertNotIn("EiemReloadMods();", hotkey_setup)
        self.assertNotIn("EiemReloadMods();", hotkey_body)
        self.assertIn("EiemRequestModUpdate(EiemModUpdate::Reload", hotkey_body)

    def test_prefab_reference_does_not_scope_a_mesh_identity_rule(self):
        start = self.mods.index("static void EiemCompileModProgram")
        end = self.mods.index("static bool EiemValidShapeExpression", start)
        body = self.mods[start:end]
        self.assertNotIn("for (const auto &prefab", body)
        self.assertNotIn('statement.key.compare(0, 8, "partner.")', body)
        self.assertIn("program.standaloneRules.push_back(i)", body)
        self.assertNotIn("EiemApplyPrefabRules", self.trace)

    def test_reload_is_dispatched_only_on_unity_thread(self):
        start = self.trace.index("static void EiemRunModReconcile()")
        end = self.trace.index("static void *s_origAssetBundleLoadAsset1", start)
        body = self.trace[start:end]
        self.assertIn("EiemDispatchModUpdate(requests", body)
        self.assertIn("Physics remains owned by model generation", body)
        self.assertIn("EiemPhysicsRuntimeRetireChangedAssets", body)
        self.assertNotIn("mod reload before renderer restore", body)
        self.assertIn("EiemRestoreRenderOverrides(affected)", body)
        self.assertIn("EiemPrepareModReload(&reloadProgram", body)
        self.assertIn("rejected before mutation", body)
        self.assertIn("EiemPublishPreparedModReload(std::move(reloadProgram))", body)
        self.assertLess(body.index("EiemPrepareModReload(&reloadProgram"),
                        body.index('EiemPhysicsRuntimeBoundary("mod reconcile begin")'))
        self.assertLess(body.index("EiemRestoreRenderOverrides(affected)"),
                        body.index("EiemPublishPreparedModReload(std::move(reloadProgram))"))
        visibility = body.index("if (submeshVisibilityOnly")
        boundary = body.index('EiemPhysicsRuntimeBoundary("mod reconcile begin")')
        self.assertLess(visibility, boundary)
        visibility_end = body.index("      affected = &affectedMods;", visibility)
        visibility_body = body[visibility:visibility_end]
        self.assertIn("EiemReapplySubmeshVisibility", visibility_body)
        self.assertNotIn("EiemApplyStandaloneRenderRules(", visibility_body)
        entry = (ROOT / "src" / "eiem.cpp").read_text(encoding="utf-8")
        self.assertNotIn("EiemReloadMods();", entry)

    def test_submesh_key_mutates_only_the_bound_mesh_indices(self):
        start = self.trace.index("static uint32_t EiemReapplySubmeshVisibility")
        end = self.trace.index("static void EiemRunModReconcile()", start)
        body = self.trace[start:end]
        self.assertIn("EiemBuildMeshResource", body)
        self.assertNotIn("EiemSetSharedMesh", body)
        self.assertNotIn("EiemRememberReplacement", body)
        self.assertNotIn("EiemApplyStandaloneRenderRules", body)
        self.assertNotIn("EiemPreserveSourceSkinning", body)
        self.assertNotIn("EiemAssignRendererMaterials", body)
        self.assertNotIn("EiemPhysics", body)

    def test_mesh_cache_keeps_one_identity_across_submesh_visibility(self):
        self.assertIn("uint64_t variant = 0;", self.backend)
        build_start = self.backend.index("static bool EiemBuildMeshResource")
        build_end = self.backend.index("static bool EiemResolveResourceDiskPath", build_start)
        build = self.backend[build_start:build_end]
        self.assertIn("rule.hiddenSubmeshMask", build)
        self.assertIn("EiemApplyMeshSubmeshVisibility", build)
        self.assertNotIn("fileStamp ^= (uint64_t)rule.hiddenSubmeshMask", build)
        self.assertNotIn("rule.hiddenSubmeshMask)) return false", build)

    def test_mesh_cache_identity_includes_the_full_reload_generation(self):
        build_start = self.backend.index("static bool EiemBuildMeshResource")
        build_end = self.backend.index("static bool EiemResolveResourceDiskPath", build_start)
        build = self.backend[build_start:build_end]
        # Every F10 salts the source stamp with a fresh generation, even
        # when the author did not change files. No role-specific bypass.
        self.assertIn("const uint64_t sourceStamp = EiemMeshResourceFileStamp(fullPath);", build)
        self.assertIn("EiemMeshCacheStamp(sourceStamp)", build)
        self.assertNotIn("kEiemTestReuseUnchangedCloth01Mesh", build)
        self.assertIn("static uint64_t EiemMeshCacheStamp", self.backend)

    def test_static_baseline_uses_minimal_mod_window_dispatcher(self):
        start = self.dispatcher.index("static LRESULT CALLBACK EiemModWndProc")
        end = self.dispatcher.index("static DWORD WINAPI HotkeyThread", start)
        body = self.dispatcher[start:end]
        self.assertIn("WM_EIEM_MOD_RECONCILE", body)
        self.assertIn("WM_EIEM_MOD_KEY", body)
        self.assertIn("EiemRunModReconcile", body)
        self.assertNotIn("PhysicsAutoTrace", body)
        self.assertNotIn("ApplyMmdPose", body)
        init = self.init[self.init.index("static DWORD WINAPI InitThread") :]
        self.assertLess(init.index("CreateThread(NULL, 0, HotkeyThread"),
                        init.index("if (kEiemEnableLegacyWorkers)"))

    def test_reconcile_wakeup_is_coalesced_and_physics_is_boundary_driven(self):
        queue = self.trace[self.trace.index("static bool EiemPostPendingModUpdate") :
                           self.trace.rindex("static void EiemQueueModReconcile")]
        self.assertIn("static volatile LONG s_eiemModUpdateMessagePosted", self.trace)
        self.assertIn("s_eiemModUpdateMessagePosted", queue)
        self.assertIn("InterlockedCompareExchange(&s_eiemModUpdateMessagePosted, 1, 0)", queue)
        self.assertIn("InterlockedExchange(&s_eiemModUpdateMessagePosted, 0)", queue)
        trojan = (ROOT / "src" / "trojan.h").read_text(encoding="utf-8")
        reconcile_handler = trojan[trojan.index("if (msg == WM_EIEM_MOD_RECONCILE)") :]
        self.assertIn("InterlockedExchange(&s_eiemModUpdateMessagePosted, 0)", reconcile_handler)
        physics = (ROOT / "src" / "eiem_native_physics_runtime.h").read_text(encoding="utf-8")
        self.assertIn("static void EiemPhysicsRuntimeBoundary", physics)
        self.assertNotIn("EiemPhysicsRuntimePeriodic", physics)
        self.assertNotIn("PhysicsCandidate", physics)
        self.assertIn("EiemPhysicsRuntimeCheckReady();", physics)
        self.assertNotIn("EiemPhysicsRuntimePeriodic", trojan)

    def test_lod_suppression_uses_game_force_rendering_off_state(self):
        self.assertIn("g_renderer_get_forceRenderingOff", self.trace)
        self.assertIn("forceRenderingOffRead && forceRenderingOff", self.trace)
        self.assertIn('"force-off"', self.trace)
        self.assertIn('"enabled-or-unread"', self.trace)

    def test_legacy_per_frame_ik_detours_are_removed(self):
        for source in (self.init, self.trace, self.trojan):
            for obsolete in (
                "Hooked_MovementComponent_Tick",
                "s_origMoveTick",
                "MovementComponent.Tick",
                "Hooked_IK_UpdateSolver",
                "s_origUpdateSolver",
                "BipedIK.UpdateSolver",
                "Hooked_OnUpdate",
                "s_origOnUpdate",
                "IKSolverTrigonometric.OnUpdate",
            ):
                self.assertNotIn(obsolete, source)

    def test_key_dispatch_is_recorded_at_generation_boundary(self):
        self.assertIn("EiemRegistrationTraceInput", self.trace)
        start = self.trace.index("static void EiemQueueModKey")
        body = self.trace[start : start + 900]
        self.assertIn("EiemRegistrationTraceInput", body)
        self.assertIn("chord.vk", body)
        self.assertIn("chord.modifiers", body)

    def test_reconcile_begin_and_end_share_the_generation_trace(self):
        self.assertIn("EiemRegistrationTraceReconcile", self.trace)
        body = self.trace[self.trace.index("static void EiemRunModReconcile") :]
        self.assertIn("if (!requests) return;", body[:1000])
        self.assertNotIn("s_eiemDeferredModReplayPending", body)
        self.assertNotIn("kEiemDeferredReplayRequest", body)
        self.assertIn('EiemRegistrationTraceReconcile("begin"', body)
        self.assertIn('EiemRegistrationTraceReconcile(\n      "end"', body)
        self.assertIn("inputs.size()", body)
        self.assertIn("instances.size()", body)


class BlenderExportContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.addon = (ROOT / "tools" / "Blender" / "eiem_blender_addon.py").read_text(
            encoding="utf-8"
        )
        cls.controls = (ROOT / "tools" / "Blender" / "eiem_blender_controls.py").read_text(
            encoding="utf-8"
        )
        writer_path = (
            ROOT / "tools" / "AnimeStudio" / "AnimeStudio.GUI" /
            "EiemPackageWriter.cs"
        )
        cls.writer = writer_path.read_text(encoding="utf-8") if writer_path.exists() else None
        physics_writer_path = writer_path.with_name("EiemPhysicsSourceWriter.cs")
        cls.physics_writer = (physics_writer_path.read_text(encoding="utf-8")
                              if physics_writer_path.exists() else None)

    def test_export_is_rooted_at_selected_eiem_meshes(self):
        self.assertIn("selected_objects", self.addon)
        self.assertIn("No EIEM mesh objects selected", self.controls)

    def test_export_materials_and_textures_are_dependency_closure(self):
        self.assertIn("referenced_materials", self.addon)
        self.assertIn("referenced_images", self.addon)

    def test_blender_emits_standalone_render_identity_without_prefab_scope(self):
        import ast
        tree = ast.parse(self.addon)
        body = "\n".join(ast.get_source_segment(self.addon, node) for node in tree.body
                         if isinstance(node, ast.FunctionDef) and
                         node.name in ("export_package", "write_export_package"))
        self.assertIn('asset = str(first.get("eiem_render_asset"', body)
        self.assertIn('"eiem_target_asset", first.data.get("eiem_asset"', body)
        self.assertNotIn("has no Prefab/Render identity", body)
        self.assertNotIn('"render.%d=%s"', body)

    def test_existing_resources_default_to_their_offline_identity(self):
        self.assertIn('mesh["eiem_target_path"] = values.get(', self.addon)
        self.assertIn('"target.path", payload.get("source", ""))', self.addon)
        self.assertIn('material_values["target.path"] = values.get(', self.addon)
        self.assertIn(
            '"target.path", material_values.get("source", ""))', self.addon
        )

    def test_animestudio_emits_one_render_action_per_mesh_resource(self):
        if self.writer is None:
            self.skipTest("AnimeStudio is a separately managed checkout")
        self.assertIn("renderedMeshes", self.writer)
        self.assertIn("if (!renderedMeshes.Add(mesh))", self.writer)
        self.assertIn('ini.Add($"render.{index}={renderSections[index]}")', self.writer)

    def test_animestudio_normal_package_embeds_optional_native_source(self):
        if self.writer is None or self.physics_writer is None:
            self.skipTest("AnimeStudio is a separately managed checkout")
        self.assertIn("EiemPhysicsSourceWriter.Write", self.writer)
        self.assertIn('Path.Combine(rootDirectory, "physics")', self.writer)
        for component in ("BeyondBoneCloth", "BeyondBoneSphereCollider",
                          "BeyondBoneCapsuleCollider", "BeyondBonePlaneCollider"):
            self.assertIn(component, self.physics_writer)
        self.assertIn('purpose = "raw-component-evidence-not-a-physics-package"',
                      self.physics_writer)

    def test_blender_has_ordered_left_aligned_eiem_property_panels(self):
        self.assertIn("class EIEM_PT_material_properties", self.addon)
        self.assertIn("class EIEM_PT_mesh_properties", self.addon)
        self.assertIn("class EIEM_PT_image_properties", self.addon)
        self.assertIn('label_column.alignment = "LEFT"', self.addon)
        start = self.addon.index("def material_property_groups")
        body = self.addon[start : start + 1800]
        self.assertLess(body.index("texture_paths"), body.index("parameters"))
        self.assertIn("EIEM_INTERNAL_MATERIAL_PROPERTIES", body)


if __name__ == "__main__":
    unittest.main()
