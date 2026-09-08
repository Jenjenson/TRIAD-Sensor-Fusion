import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE_ROOT = ROOT / "SourceAssets" / "IstanaPublicViewExploreV5D" / "TreeRealism"
RUNTIME_PUBLIC = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusion" / "Public"
RUNTIME_PRIVATE = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusion" / "Private"
EDITOR_PUBLIC = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusionEditor" / "Public"
EDITOR_PRIVATE = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusionEditor" / "Private"


class ExploreV5DTreeRealismContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(
            (SOURCE_ROOT / "istana_public_view_v5d_tree_realism.contract.json").read_text(
                encoding="utf-8"
            )
        )
        cls.lod_amendment = json.loads(
            (
                SOURCE_ROOT
                / "istana_public_view_v5d_tree_realism.runtime_lod_amendment.v2.json"
            ).read_text(encoding="utf-8")
        )
        cls.readme = (SOURCE_ROOT / "README.md").read_text(encoding="utf-8")
        cls.actor_h = (RUNTIME_PUBLIC / "TRIADIstanaExploreV5DTreeRealismActor.h").read_text(
            encoding="utf-8"
        )
        cls.actor_cpp = (RUNTIME_PRIVATE / "TRIADIstanaExploreV5DTreeRealismActor.cpp").read_text(
            encoding="utf-8"
        )
        cls.v4_h = (RUNTIME_PUBLIC / "TRIADIstanaExploreV4LandscapeActor.h").read_text(
            encoding="utf-8"
        )
        cls.v4_cpp = (RUNTIME_PRIVATE / "TRIADIstanaExploreV4LandscapeActor.cpp").read_text(
            encoding="utf-8"
        )
        cls.v5b_cpp = (RUNTIME_PRIVATE / "TRIADIstanaExploreV5BVisualActor.cpp").read_text(
            encoding="utf-8"
        )
        cls.v5_h = (RUNTIME_PUBLIC / "TRIADIstanaExploreV5AppearanceActor.h").read_text(
            encoding="utf-8"
        )
        cls.v5_cpp = (RUNTIME_PRIVATE / "TRIADIstanaExploreV5AppearanceActor.cpp").read_text(
            encoding="utf-8"
        )
        cls.v5_native_test = (
            RUNTIME_PRIVATE / "Tests/TRIADIstanaExploreV5RuntimeTests.cpp"
        ).read_text(encoding="utf-8")
        cls.editor_h = (
            EDITOR_PUBLIC / "TRIADIstanaExploreV5DTreeRealismEditorLibrary.h"
        ).read_text(encoding="utf-8")
        cls.editor_cpp = (
            EDITOR_PRIVATE / "TRIADIstanaExploreV5DTreeRealismEditorLibrary.cpp"
        ).read_text(encoding="utf-8")

    def test_exact_tree_census_and_truth_boundary_are_explicit(self):
        self.assertEqual(729, self.contract["sourceCensus"]["v4MainAndHeritageTreeInstances"])
        self.assertEqual(10, self.contract["sourceCensus"]["v4TreeFormComponents"])
        self.assertEqual(5, self.contract["sourceCensus"]["v4BroadFormMeshes"])
        truth = self.contract["truthBoundary"]
        self.assertTrue(truth["appearanceOnly"])
        for key, value in truth.items():
            if key != "appearanceOnly":
                self.assertFalse(value, key)

    def test_v5d_runtime_exposes_source_lod0_and_holds_medium_range_detail(self):
        policy = self.contract["runtimeLodPolicy"]
        self.assertEqual(1, policy["sourceMeshMinimumLod"])
        self.assertEqual(0, policy["derivativeAssetMinimumLodForProvenanceOnly"])
        # The separately published SourceAssets bundle can remain at the exact
        # LOD1 predecessor until its native/content transaction is run. Accept
        # that frozen record or the post-publication policy, but always require
        # the checked-in runtime source below to carry the realism upgrade.
        if "sourceLod0AvailableNearCamera" in policy:
            self.assertEqual(0, policy["runtimeComponentMinimumLod"])
            self.assertTrue(policy["sourceLod0AvailableNearCamera"])
            self.assertEqual(0, policy["componentForcedLodModel"])
            self.assertEqual(1.8, policy["runtimeInstanceLodDistanceScale"])
            self.assertTrue(policy["automaticScreenSizeLodSelection"])
            self.assertTrue(policy["mediumRangeCrownDetailRetained"])
            self.assertFalse(policy["forceAll729ToOneLodAtDistance"])
            self.assertFalse(policy["forceAll729ToLod0"])
        else:
            self.assertEqual(1, policy["runtimeComponentMinimumLod"])
            self.assertTrue(policy["cleanSourceLod1Runtime"])
            self.assertEqual(1, policy["componentForcedLodModel"])
            self.assertEqual(1.0, policy["runtimeInstanceLodDistanceScale"])
            self.assertFalse(policy["automaticScreenSizeLodSelection"])
            self.assertTrue(policy["forceAll729ToOneLodAtDistance"])
        self.assertTrue(policy["hismForcedLodModelUsesDirectZeroBasedIndex"])
        for token in (
            '#include "MeshDescription.h"',
            "RehomeExactSourceMeshDescriptions",
            "Source->CloneMeshDescription(Lod, SourceDescription)",
            "Duplicate->CreateMeshDescription(",
            "Duplicate->CommitMeshDescription(Lod, CommitParams)",
            "Duplicate->SetStaticMaterials(Source->GetStaticMaterials())",
            "Duplicate->GetSectionInfoMap().CopyFrom",
            "Duplicate->GetOriginalSectionInfoMap().CopyFrom",
            "RestoreExactSourceExtendedBounds",
            "Candidates[Index]->SetExtendedBounds(",
            "Sources[Index]->GetExtendedBounds()",
            "Duplicate->SetMinLODIdx(0)",
            "RuntimeTreeMinimumLod = 0",
            "Component->MinLOD = RuntimeTreeMinimumLod",
            "Component->MinLOD != RuntimeTreeMinimumLod",
            "ExpectedRuntimeTreeMinimumLod()",
            "RuntimeTreeForcedLodModel = 0",
            "ExpectedRuntimeTreeForcedLodModel()",
            "Component->ForcedLodModel = RuntimeTreeForcedLodModel",
            "RuntimeTreeLodDistanceScale = 1.8f",
            "InstanceLODDistanceScale = RuntimeTreeLodDistanceScale",
        ):
            self.assertIn(token, self.editor_cpp + self.actor_cpp)
        for token in (
            "exact source LOD0 near the camera",
            "irregular tropical layering",
            "sourceLOD0AvailableNearCamera=true",
            "automaticScreenSizeLod=true",
            "mediumRangeCrownDetailRetained=true",
            "allTreesForcedToLod0=false",
        ):
            self.assertIn(token, self.actor_cpp)
        for token in (
            "spreading/high-fork tropical crown cues",
            "branch breaks through the medium field",
            "never species identifications",
        ):
            self.assertIn(token, self.actor_h)
        for token in (
            "runtimeMinimumLod=0",
            "runtimeForcedLodModel=0",
            "automaticScreenSizeLod=true",
            "runtimeLodDistanceScale=1.8",
            "sourceLOD0AvailableNearCamera=true",
            "mediumRangeCrownDetailRetained=true",
        ):
            self.assertIn(token, self.editor_cpp)
        rehome_start = self.editor_cpp.index(
            "bool RehomeExactSourceMeshDescriptions("
        )
        rehome_end = self.editor_cpp.index("bool EnsureMeshes(", rehome_start)
        rehome = self.editor_cpp[rehome_start:rehome_end]
        self.assertNotIn("Source->CommitMeshDescription", rehome)
        self.assertNotIn("Source->PostEditChange", rehome)
        self.assertNotIn("Source->MarkPackageDirty", rehome)

    def test_runtime_lod_amendment_reconciles_the_frozen_v1_record(self):
        amendment = self.lod_amendment
        self.assertEqual(
            "triad.istana_public_view_explore_v5d_tree_realism.runtime_lod_amendment.v2",
            amendment["schema"],
        )
        self.assertEqual(self.contract["schema"], amendment["amendsSchema"])
        self.assertEqual("runtimeLodPolicy only", amendment["scope"])
        self.assertTrue(amendment["historicalV1PolicySuperseded"])

        policy = amendment["effectiveRuntimeLodPolicy"]
        self.assertEqual(0, policy["runtimeComponentMinimumLod"])
        self.assertTrue(policy["sourceLod0AvailableNearCamera"])
        self.assertEqual(0, policy["componentForcedLodModel"])
        self.assertEqual(1.8, policy["runtimeInstanceLodDistanceScale"])
        self.assertTrue(policy["automaticScreenSizeLodSelection"])
        self.assertTrue(policy["mediumRangeCrownDetailRetained"])
        self.assertFalse(policy["forceAll729ToOneLodAtDistance"])
        self.assertFalse(policy["forceAll729ToLod0"])
        self.assertFalse(policy["hismForcedLodModelUsesDirectZeroBasedIndex"])
        self.assertTrue(policy["hismForcedLodModelZeroIsAutomaticSentinel"])
        self.assertTrue(policy["hismPositiveForcedLodModelUsesOneBasedEncoding"])

        preservation = amendment["preservation"]
        self.assertEqual(729, preservation["treeCount"])
        for key, value in preservation.items():
            if key != "treeCount":
                self.assertFalse(value, key)
        self.assertTrue(amendment["truthBoundary"]["appearanceOnly"])
        self.assertFalse(
            amendment["truthBoundary"]["nativeVisualAcceptanceProvidedByThisAmendment"]
        )

        for text in (
            "frozen v1 publication record",
            "runtime_lod_amendment.v2.json",
            "runtime component minimum LOD is 0",
            "`ForcedLodModel` is 0",
            "automatic-selection sentinel",
            "positive forced-LOD values",
            "instance LOD distance scale of 1.8",
            "without forcing every tree",
        ):
            self.assertIn(text, self.readme)

    def test_managed_derivative_bounds_repair_is_narrow_and_pre_save(self):
        self.assertRegex(
            self.editor_h,
            r"static\s+bool\s+EnsureTreeCanopyRealismMeshAsset\s*\(",
        )
        self.assertRegex(
            self.editor_cpp,
            r"UTRIADIstanaExploreV5DTreeRealismEditorLibrary::\s*"
            r"EnsureTreeCanopyRealismMeshAsset\s*\(",
        )
        self.assertRegex(
            self.editor_cpp,
            r"bool\s+EnsureExistingMeshForForm\s*\(",
        )

        one_form_helper = self.editor_cpp[
            self.editor_cpp.index("bool EnsureExistingMeshForForm(") :
            self.editor_cpp.index("bool RestoreExactSourceExtendedBounds(")
        ]
        one_form_helper_compact = re.sub(r"\s+", "", one_form_helper)
        for token in (
            "if(!V4||FormIndex<0||"
            "FormIndex>=UE_ARRAY_COUNT(MeshAssetNames))",
            "Sources.Num()!=UE_ARRAY_COUNT(MeshAssetNames)||"
            "Sources.Contains(nullptr)",
            "for(constFString&AssetName:MeshAssetNames)",
            "FPackageName::DoesPackageExist(PackagePath(AssetName))",
            "FindPackage(nullptr,*PackagePath(AssetName))",
            "ExistingCount!=UE_ARRAY_COUNT(MeshAssetNames)",
        ):
            self.assertIn(token, one_form_helper_compact)
        for token in (
            "requested V5D tree form index is outside the exact five-form roster",
            "complete preexisting five-mesh namespace",
            "EnsureExistingMeshForForm(V4, FormIndex, Mesh, Error)",
            "V5D_TREE_REALISM_MESH_ASSET_VALID formIndex=%d",
            "exactFiveMeshNamespaceRequired=true",
            "oneFormPerColdProcess=true",
        ):
            self.assertIn(token, self.editor_cpp)

        match_start = self.editor_cpp.index(
            "bool MatchesExactManagedDerivativeStateExceptBounds("
        )
        match_end = self.editor_cpp.index(
            "bool RestoreExactSourceExtendedBounds(", match_start
        )
        match = self.editor_cpp[match_start:match_end]
        for token in (
            "Candidate->GetPathName() != ObjectPath(MeshAssetNames[FormIndex])",
            "Candidate->GetMinLODIdx() != 0",
            "Source->GetMinLODIdx() != 1",
            "CandidateRender->LODResources.Num() != SourceRender->LODResources.Num()",
            "CandidateLod.GetNumTriangles() != SourceLod.GetNumTriangles()",
            "CandidateLod.Sections.Num() != SourceLod.Sections.Num()",
            "CandidateSection.MaterialIndex != SourceSection.MaterialIndex",
            "CandidateSection.NumTriangles != SourceSection.NumTriangles",
            "CandidateMaterial.MaterialSlotName != SourceMaterial.MaterialSlotName",
            "CandidateInterface == SourceInterface",
            "MaterialObjectPath(TreeResponseSpecs[MaterialIndex].AssetName)",
            "(!bExactSourceBinding && !bExactResponseBinding)",
        ):
            self.assertIn(token, match)

        ensure_start = self.editor_cpp.index("bool EnsureMeshes(")
        ensure_end = self.editor_cpp.index("bool BuildAssetRoster(", ensure_start)
        ensure = self.editor_cpp[ensure_start:ensure_end]
        self.assertIn("ExistingCount == UE_ARRAY_COUNT(MeshAssetNames)", ensure)
        self.assertIn("bRequireAtLeastOneMismatch", self.editor_cpp)
        self.assertIn("bounded source-material-to-response/bounds repair", ensure)
        fresh_start = ensure.index("TArray<UObject*> FreshAssets;")
        finish_index = ensure.index(
            "FAssetCompilingManager::Get().FinishAllCompilation();", fresh_start
        )
        restore_index = ensure.index(
            "RestoreExactSourceExtendedBounds(", finish_index
        )
        save_index = ensure.index("Assets->SaveLoadedAsset(Mesh", restore_index)
        self.assertLess(finish_index, restore_index)
        self.assertLess(restore_index, save_index)
        self.assertNotIn("SaveLoadedAssets(FreshAssets", ensure)
        self.assertIn("FScopedTreeAssetSaveValidationSuppression", self.editor_cpp)
        self.assertIn('TEXT("bValidateOnSave")', self.editor_cpp)
        self.assertIn("CollectGarbage(RF_NoFlags);", self.editor_cpp)
        release_start = self.editor_cpp.index(
            "bool ReleaseStaticMeshDescriptionCacheMemoryBounded("
        )
        release_end = self.editor_cpp.index(
            "const FString MeshAssetNames[]", release_start
        )
        release = self.editor_cpp[release_start:release_end]
        compile_index = release.index(
            "FAssetCompilingManager::Get().FinishAllCompilation();"
        )
        dirty_snapshot_index = release.index(
            "const bool bWasDirty = Mesh->GetOutermost()->IsDirty();"
        )
        clear_index = release.index("Mesh->ClearMeshDescriptions();")
        clear_hi_res_index = release.index("Mesh->ClearHiResMeshDescription();")
        dirty_validation_index = release.index(
            "Mesh->GetOutermost()->IsDirty() != bWasDirty"
        )
        trim_index = release.index("FMemory::Trim(true);")
        self.assertEqual(
            sorted(
                (
                    compile_index,
                    dirty_snapshot_index,
                    clear_index,
                    clear_hi_res_index,
                    dirty_validation_index,
                    trim_index,
                )
            ),
            [
                compile_index,
                dirty_snapshot_index,
                clear_index,
                clear_hi_res_index,
                dirty_validation_index,
                trim_index,
            ],
        )
        self.assertIn("changed package dirtiness while releasing", release)
        self.assertNotIn("MarkPackageDirty", release)
        self.assertGreaterEqual(
            self.editor_cpp.count("ReleaseStaticMeshDescriptionCacheMemoryBounded("),
            7,
        )
        self.assertIn(
            "}\n    FAssetCompilingManager::Get().FinishAllCompilation();\n"
            "    ResponseMaterials.Reset();\n"
            "    CollectGarbage(RF_NoFlags);",
            self.editor_cpp,
        )
        self.assertIn(
            "}\n    FAssetCompilingManager::Get().FinishAllCompilation();\n"
            "    Meshes.Reset();\n"
            "    CollectGarbage(RF_NoFlags);",
            self.editor_cpp,
        )
        self.assertIn("EnsureTreeMaterialResponseAssets", self.editor_h)
        self.assertIn("materialAndMeshCreationProcessesSeparated=true", self.editor_cpp)
        self.assertIn(
            "duplicateGenericSaveValidationSuppressed=true",
            self.editor_cpp,
        )
        self.assertIn(
            "Candidate->GetBounds().Origin.Equals(Source->GetBounds().Origin, 0.001)",
            self.actor_cpp,
        )

    def test_source_packages_and_non_v5d_maps_are_never_mutated(self):
        preservation = self.contract["preservation"]
        for key, value in preservation.items():
            self.assertFalse(value, key)
        self.assertNotRegex(
            self.editor_cpp,
            r"DeleteAsset|DeleteDirectory|ConsolidateAssets|SaveLoadedAsset\(Sources",
        )
        self.assertIn(self.contract["targetMap"], self.editor_cpp)
        self.assertIn("FPackageName::IsTempPackage", self.editor_cpp)

    def test_runtime_override_snapshots_restores_and_fails_closed(self):
        roster_start = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateAssetRoster("
        )
        roster_end = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    BuildDeterministicTreeBaseLayout(",
            roster_start,
        )
        roster = self.actor_cpp[roster_start:roster_end]
        mesh_gate_start = self.actor_cpp.index(
            "bool MeshesShareGeometryAndSlots("
        )
        mesh_gate_end = self.actor_cpp.index(
            "bool SourceComponentsUseExactV4Materials(", mesh_gate_start
        )
        mesh_gate = self.actor_cpp[mesh_gate_start:mesh_gate_end]
        for token in (
            "Candidate->GetMaterial(Slot)",
            "TreeResponseMaterialPaths[MaterialIndex]",
        ):
            self.assertIn(token, mesh_gate)
        self.assertNotIn("Source->GetMaterial(Slot)", mesh_gate)
        source_material_gate_start = mesh_gate_end
        source_material_gate_end = self.actor_cpp.index(
            "void ConfigureRenderOnlyTransition(", source_material_gate_start
        )
        source_material_gate = self.actor_cpp[
            source_material_gate_start:source_material_gate_end
        ]
        for token in (
            "MainSourceComponent->GetStaticMesh() != Source",
            "HeritageSourceComponent->GetStaticMesh() != Source",
            "MainSourceComponent->GetMaterial(Slot)",
            "HeritageSourceComponent->GetMaterial(Slot)",
            "SourceTreeMaterialPaths[MaterialIndex]",
        ):
            self.assertIn(token, source_material_gate)
        self.assertIn("SourceComponentsUseExactV4Materials(", roster)
        self.assertIn("MeshesShareGeometryAndSlots(", roster)

        validation_start = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateTreeRealism("
        )
        validation_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::BeginPlay()",
            validation_start,
        )
        validation = self.actor_cpp[validation_start:validation_end]
        cold_validation = validation[
            validation.index("else", validation.index("HasActorBegunPlay()")) :
        ]
        for token in (
            "RuntimeOriginalComponentMeshes",
            "RuntimeOriginalMinLods",
            "RuntimeOriginalMaterials",
            "RestoreRuntimeTreeVisualOverride",
            "ValidateAppliedRuntimeOverrideForSourceActor",
            "V4LandscapeActor->Tags.Remove(RuntimeOverrideTag())",
        ):
            self.assertIn(token, self.actor_h + self.actor_cpp)
        self.assertIn("RestoreRuntimeTreeVisualOverride();", self.actor_cpp)
        self.assertIn("failed closed", self.actor_cpp)
        self.assertIn("!Tags.Contains(ExpectedActorTag())", validation)
        self.assertIn(
            "ValidateAssetRoster(SavedAssets, V4LandscapeActor, Error)",
            cold_validation,
        )
        self.assertIn("cold V5D tree derivative asset roster failed closed", cold_validation)

        apply_start = self.actor_cpp.index(
            "ApplyRuntimeTreeVisualOverride(FString& OutError)"
        )
        apply_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    RestoreRuntimeTreeVisualOverride()",
            apply_start,
        )
        apply = self.actor_cpp[apply_start:apply_end]
        synchronous_build = apply.index(
            "Component->BuildTreeIfOutdated(false, true)"
        )
        immediate_validation = apply.index(
            "ValidateAppliedRuntimeOverrideForSourceActor(", synchronous_build
        )
        self.assertLess(synchronous_build, immediate_validation)
        self.assertNotIn("Component->BuildTreeIfOutdated(true, true)", apply)

        runtime_validation_start = self.actor_cpp.index(
            "ValidateAppliedRuntimeOverrideForSourceActor("
        )
        runtime_validation_end = self.actor_cpp.index(
            "ValidateActiveRuntimeOverrideForSourceActor(",
            runtime_validation_start,
        )
        runtime_validation = self.actor_cpp[
            runtime_validation_start:runtime_validation_end
        ]
        self.assertIn("!Component->IsTreeFullyBuilt()", runtime_validation)
        self.assertNotIn("Component->IsAsyncBuilding()", runtime_validation)
        self.assertIn(
            "Component->MinLOD != RuntimeTreeMinimumLod", runtime_validation
        )
        self.assertIn(
            "Component->ForcedLodModel != RuntimeTreeForcedLodModel",
            runtime_validation,
        )
        self.assertIn(
            "Component->ForcedLodModel = RuntimeTreeForcedLodModel", apply
        )

        restore_start = apply_end
        restore_end = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateTreeRealism(",
            restore_start,
        )
        restore = self.actor_cpp[restore_start:restore_end]
        self.assertIn(
            "Component->MinLOD = RuntimeOriginalMinLods[ComponentIndex]", restore
        )
        self.assertIn(
            "Component->MinLOD != RuntimeOriginalMinLods[ComponentIndex]", restore
        )
        self.assertIn("Component->ForcedLodModel = 0", restore)
        self.assertIn("Component->ForcedLodModel != 0", restore)
        self.assertIn("!Component->bOverrideMinLOD", restore)
        self.assertIn("bMaterialsRestored", restore)
        self.assertIn("failed its exact mesh/material/MinLOD", restore)

    def test_runtime_tree_override_uses_the_pre_world_begin_play_window(self):
        lifecycle_sources = self.actor_h + self.actor_cpp
        for forbidden in (
            "TimerManager",
            "SetTimerForNextTick",
            "ApplyDeferredRuntimeTreeVisualOverride",
        ):
            self.assertNotIn(forbidden, lifecycle_sources)

        for token in (
            "DECLARE_MULTICAST_DELEGATE(FTRIADIstanaExploreV4WindRuntimeReady)",
            "FTRIADIstanaExploreV4WindRuntimeReady& OnWindRuntimeReady()",
            "return WindRuntimeReadyEvent;",
            "FTRIADIstanaExploreV4WindRuntimeReady WindRuntimeReadyEvent;",
        ):
            self.assertIn(token, self.v4_h)

        v4_begin_start = self.v4_cpp.index(
            "void ATRIADIstanaExploreV4LandscapeActor::BeginPlay()"
        )
        v4_begin_end = self.v4_cpp.index(
            "void ATRIADIstanaExploreV4LandscapeActor::TriggerWindGust(",
            v4_begin_start,
        )
        v4_begin_play = self.v4_cpp[v4_begin_start:v4_begin_end]
        apply_wind = v4_begin_play.index("ApplyWindParameters();")
        readiness_broadcast = v4_begin_play.index(
            "WindRuntimeReadyEvent.Broadcast();", apply_wind
        )
        one_shot_clear = v4_begin_play.index(
            "WindRuntimeReadyEvent.Clear();", readiness_broadcast
        )
        self.assertLess(apply_wind, readiness_broadcast)
        self.assertLess(readiness_broadcast, one_shot_clear)
        self.assertEqual(1, self.v4_cpp.count("WindRuntimeReadyEvent.Broadcast();"))
        self.assertEqual(1, self.v4_cpp.count("WindRuntimeReadyEvent.Clear();"))

        begin_play_start = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::BeginPlay()"
        )
        begin_play_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    HandleV4WindRuntimeReady()",
            begin_play_start,
        )
        begin_play = self.actor_cpp[begin_play_start:begin_play_end]
        lifecycle_subscription = begin_play.index(
            "WorldLifecycleHandle = World->GetOnBeginPlayEvent().AddUObject("
        )
        active_order = begin_play.index(
            "if (V4LandscapeActor->IsWindRuntimeActive())",
            lifecycle_subscription,
        )
        immediate_apply = begin_play.index(
            "ApplyRuntimeTreeVisualOverrideAtStartup();", active_order
        )
        readiness_subscription = begin_play.index(
            "V4LandscapeActor->OnWindRuntimeReady().AddUObject(", immediate_apply
        )
        self.assertLess(lifecycle_subscription, active_order)
        self.assertLess(active_order, immediate_apply)
        self.assertLess(immediate_apply, readiness_subscription)
        self.assertIn("V4WindRuntimeReadyHandle =", begin_play)
        self.assertIn("HandleWorldBegunPlayStateChanged", begin_play)
        self.assertIn("HandleV4WindRuntimeReady", begin_play)

        readiness_handler_start = begin_play_end
        readiness_handler_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    ApplyRuntimeTreeVisualOverrideAtStartup()",
            readiness_handler_start,
        )
        readiness_handler = self.actor_cpp[
            readiness_handler_start:readiness_handler_end
        ]
        self.assertIn("ApplyRuntimeTreeVisualOverrideAtStartup();", readiness_handler)

        world_handler_start = self.actor_cpp.index(
            "HandleWorldBegunPlayStateChanged(bool bHasBegunPlay)"
        )
        world_handler_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    RemoveV4WindRuntimeReadyBinding()",
            world_handler_start,
        )
        world_handler = self.actor_cpp[world_handler_start:world_handler_end]
        true_edge = world_handler.index("if (bHasBegunPlay)")
        deadline_gate = world_handler.index(
            "if (!bRuntimeOverrideApplied && V4WindRuntimeReadyHandle.IsValid())",
            true_edge,
        )
        true_edge_return = world_handler.index("return;", deadline_gate)
        false_edge_restore = world_handler.index(
            "RestoreRuntimeTreeVisualOverride();", true_edge_return
        )
        self.assertLess(true_edge, deadline_gate)
        self.assertLess(deadline_gate, true_edge_return)
        self.assertLess(true_edge_return, false_edge_restore)
        self.assertIn(
            "V4 wind readiness did not arrive before UWorld entered begun play.",
            world_handler,
        )
        for cleanup in (
            "RemoveV4WindRuntimeReadyBinding();",
            "RemoveWorldLifecycleBinding();",
        ):
            self.assertIn(cleanup, world_handler[deadline_gate:true_edge_return])
            self.assertIn(cleanup, world_handler[false_edge_restore:])

        apply_start = self.actor_cpp.index(
            "ApplyRuntimeTreeVisualOverride(FString& OutError)"
        )
        apply_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    RestoreRuntimeTreeVisualOverride()",
            apply_start,
        )
        apply = self.actor_cpp[apply_start:apply_end]
        for pre_play_gate in (
            "!HasActorBegunPlay()",
            "!World || World->HasBegunPlay()",
            "!V4LandscapeActor->HasActorBegunPlay()",
            "!V4LandscapeActor->IsWindRuntimeActive()",
            "before UWorld enters begun play",
        ):
            self.assertIn(pre_play_gate, apply)
        self.assertIn("if (!Component->SetStaticMesh(ExpectedMesh))", apply)
        for diagnostic_field in (
            "actual=%s",
            "expected=%s",
            "worldBegun=%s",
            "mobility=%d",
        ):
            self.assertIn(diagnostic_field, apply)

        restore_start = apply_end
        restore_end = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateTreeRealism(",
            restore_start,
        )
        restore = self.actor_cpp[restore_start:restore_end]
        self.assertIn(
            "Component->GetStaticMesh() != OriginalMesh &&\n"
            "                !Component->SetStaticMesh(OriginalMesh)",
            restore,
        )
        self.assertIn("bRestoreSucceeded = false;", restore)
        self.assertIn("if (!bRestoreSucceeded)", restore)
        for diagnostic_field in (
            "actual=%s",
            "expected=%s",
            "worldBegun=%s",
            "mobility=%d",
        ):
            self.assertIn(diagnostic_field, restore)

        remove_ready_start = world_handler_end
        remove_ready_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    RemoveWorldLifecycleBinding()",
            remove_ready_start,
        )
        remove_ready = self.actor_cpp[remove_ready_start:remove_ready_end]
        self.assertIn("OnWindRuntimeReady().Remove(", remove_ready)
        self.assertIn("V4WindRuntimeReadyHandle.Reset();", remove_ready)

        remove_world_start = remove_ready_end
        remove_world_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::EndPlay(",
            remove_world_start,
        )
        remove_world = self.actor_cpp[remove_world_start:remove_world_end]
        self.assertIn("GetOnBeginPlayEvent().Remove(WorldLifecycleHandle)", remove_world)
        self.assertIn("WorldLifecycleHandle.Reset();", remove_world)

        end_play_start = remove_world_end
        end_play_end = self.actor_cpp.index(
            "#if WITH_DEV_AUTOMATION_TESTS", end_play_start
        )
        end_play = self.actor_cpp[end_play_start:end_play_end]
        self.assertIn("RemoveV4WindRuntimeReadyBinding();", end_play)
        self.assertIn("bRestoreAtWorldFalseEdge", end_play)
        self.assertIn("World->bIsTearingDown", end_play)
        self.assertIn("WorldLifecycleHandle.IsValid()", end_play)
        self.assertIn("RestoreRuntimeTreeVisualOverride();", end_play)
        self.assertIn("RemoveWorldLifecycleBinding();", end_play)

    def test_authored_actor_tag_has_one_runtime_contract(self):
        self.assertIn("static const FName& ExpectedActorTag();", self.actor_h)
        self.assertIn(
            "ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag()",
            self.actor_cpp,
        )
        self.assertNotIn("const FName TreeRealismActorTag", self.editor_cpp)
        self.assertGreaterEqual(self.editor_cpp.count("ExpectedActorTag()"), 3)

    def test_v4_and_v5b_only_admit_the_exact_tagged_runtime_override(self):
        for source in (self.v4_cpp, self.v5b_cpp):
            self.assertIn("TRIADIstanaExploreV5DTreeRealismActor.h", source)
            self.assertIn("RuntimeOverrideTag()", source)
        self.assertIn("ValidateActiveRuntimeOverrideForSourceActor", self.v4_cpp)
        for source, variable in (
            (self.v4_cpp, "ExpectedMinLod"),
            (self.v5b_cpp, "ExpectedTreeMinLod"),
        ):
            self.assertRegex(
                source,
                rf"{variable}\s*=\s*bV5DTreeRuntimeOverride[\s\S]*?"
                r"ExpectedRuntimeTreeMinimumLod\(\)[\s\S]*?: 1;",
            )
        for source, variable in (
            (self.v4_cpp, "ExpectedForcedLodModel"),
            (self.v5b_cpp, "ExpectedTreeForcedLodModel"),
        ):
            self.assertRegex(
                source,
                rf"{variable}\s*=\s*bV5DTreeRuntimeOverride[\s\S]*?"
                r"ExpectedRuntimeTreeForcedLodModel\(\)[\s\S]*?: 0;",
            )

    def test_v4_delegates_only_tree_materials_to_the_exact_v5d_override(self):
        validator_start = self.v4_cpp.index(
            "bool ATRIADIstanaExploreV4LandscapeActor::"
            "ValidateAssetAndWindConfiguration("
        )
        validator_end = self.v4_cpp.index(
            "void ATRIADIstanaExploreV4LandscapeActor::PostLoad(",
            validator_start,
        )
        validator = self.v4_cpp[validator_start:validator_end]

        exact_override_gate = validator.index(
            "ValidateActiveRuntimeOverrideForSourceActor("
        )
        delegation = validator.index(
            "bTreeMaterialBindingDelegatedToExactV5DOverride"
        )
        self.assertLess(exact_override_gate, delegation)
        self.assertRegex(
            validator,
            r"bTreeMaterialBindingDelegatedToExactV5DOverride\s*=\s*"
            r"bV5DTreeRuntimeOverride\s*&&\s*"
            r"TreeForm\s*!=\s*ETRIADIstanaExploreV4TreeForm::Count;",
        )
        self.assertEqual(
            2,
            validator.count(
                "!bTreeMaterialBindingDelegatedToExactV5DOverride &&"
            ),
        )
        self.assertGreaterEqual(validator.count("HasBaseOrRuntimeMidParent("), 2)
        self.assertIn(
            "Binding.BaseMaterial->GetPathName().StartsWith(", validator
        )
        self.assertIn("!Binding.bUsesPerInstanceLocalPosition", validator)
        self.assertIn("ExpectedMaterialSlotForWindRole(Binding.Role)", validator)

    def test_contact_shadow_relief_keeps_direct_shadows(self):
        lighting = self.contract["canopyLighting"]
        self.assertTrue(lighting["directTreeShadowsPreserved"])
        self.assertTrue(lighting["contactShadowSuppressedOnV5DVisualTreeComponents"])
        self.assertIn("Component->SetCastShadow(true)", self.actor_cpp)
        self.assertIn("Component->SetCastContactShadow(false)", self.actor_cpp)
        self.assertNotIn("DirectionalLight", self.actor_cpp + self.editor_cpp)

    def test_v5_admits_only_the_exact_active_v5d_tree_shadow_override(self):
        resolver_start = self.v5_cpp.index(
            "bool ResolveAppliedContactShadowRosterState("
        )
        resolver_end = self.v5_cpp.index(
            "bool ValidateContactShadowRowIdentity(", resolver_start
        )
        resolver = self.v5_cpp[resolver_start:resolver_end]
        self.assertIn(
            "ATRIADIstanaExploreV5DTreeRealismActor::RuntimeOverrideTag()",
            resolver,
        )
        self.assertIn(
            "ValidateActiveRuntimeOverrideForSourceActor(",
            resolver,
        )
        self.assertIn(
            "AppliedWithExactV5DTreeRuntimeOverride",
            resolver,
        )
        self.assertIn(
            "A tagged V5D tree contact-shadow suppression failed its exact",
            resolver,
        )
        self.assertLess(
            resolver.index("OutState = EContactShadowRosterState::Applied;"),
            resolver.index("RuntimeOverrideTag()"),
        )
        self.assertLess(
            resolver.index("ValidateActiveRuntimeOverrideForSourceActor("),
            resolver.rindex("AppliedWithExactV5DTreeRuntimeOverride"),
        )

        expected_start = self.v5_cpp.index("bool ExpectedContactShadowForState(")
        expected_end = self.v5_cpp.index(
            "int32 ExpectedEnabledContactShadowCount(", expected_start
        )
        expected = self.v5_cpp[expected_start:expected_end]
        self.assertIn("bV5DTreeRuntimeSuppressionAdmitted", expected)
        self.assertIn("return false;", expected)
        self.assertEqual(
            10,
            self.v5_cpp.count(
                "EComponentMobility::Static, true, true, true, true, true}"
            ),
        )

        reapply_start = self.v5_cpp.index(
            "ReapplyExploreV5AppearanceWithExpectedContactShadowPreState("
        )
        reapply_end = self.v5_cpp.index(
            "bool ATRIADIstanaExploreV5AppearanceActor::ValidateExploreV5Appearance(",
            reapply_start,
        )
        reapply = self.v5_cpp[reapply_start:reapply_end]
        self.assertIn("ResolveAppliedContactShadowRosterState(", reapply)
        self.assertIn("ExpectedContactShadowForState(*Row.Contract, TargetState)", self.v5_cpp)
        self.assertIn("ValidateAppliedContactShadowRows(", self.v5_cpp)
        self.assertIn("effective runtime %d enabled/%d disabled", self.v5_cpp)
        self.assertIn("exactV5DTreeRuntimeSuppression=%s", self.v5_cpp)
        self.assertIn(
            "ValidateV5DTreeRuntimeContactShadowRosterForAutomation",
            self.v5_h + self.v5_cpp + self.v5_native_test,
        )
        self.assertIn("V5DTreeRuntimeDrift[2] = true", self.v5_native_test)
        self.assertIn("V5DTreeRuntimeDrift[12] = false", self.v5_native_test)

        apply_start = self.actor_cpp.index(
            "ApplyRuntimeTreeVisualOverride(FString& OutError)"
        )
        apply_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::\n"
            "    RestoreRuntimeTreeVisualOverride()",
            apply_start,
        )
        apply = self.actor_cpp[apply_start:apply_end]
        self.assertLess(
            apply.index("!Component->bCastContactShadow"),
            apply.index("Component->SetCastContactShadow(false)"),
        )
        self.assertIn("exact applied-V5 contact-shadow prestate", apply)

        applied_validator_start = self.actor_cpp.index(
            "ValidateAppliedRuntimeOverrideForSourceActor("
        )
        applied_validator_end = self.actor_cpp.index(
            "ValidateActiveRuntimeOverrideForSourceActor(",
            applied_validator_start,
        )
        applied_validator = self.actor_cpp[
            applied_validator_start:applied_validator_end
        ]
        self.assertIn(
            "RuntimeOriginalCastContactShadow[ComponentIndex] != 1u",
            applied_validator,
        )

    def test_tree_base_and_leaf_litter_are_deterministic_render_only(self):
        transitions = self.contract["treeBaseTransitions"]
        self.assertEqual(128, transitions["nearVisibleTreesSelected"])
        self.assertEqual(3, transitions["rootFragmentsPerSelectedTree"])
        self.assertEqual(384, transitions["rootApronInstances"])
        self.assertEqual(8, transitions["leafLitterFragmentsPerSelectedTree"])
        self.assertEqual(1024, transitions["leafLitterInstances"])
        self.assertTrue(transitions["fragmentsPairwiseNonCoincidentPerTree"])
        self.assertTrue(transitions["perFragmentAngleRadiusAndScaleVariation"])
        self.assertEqual(430, transitions["existingV5BTreeBasesExcludedWithinCentimeters"])
        self.assertEqual(492, transitions["sourceTreePrefilterClearanceCentimeters"])
        self.assertEqual([18, 62], transitions["rootFragmentOffsetCentimeters"])
        self.assertTrue(transitions["edgeBlendedAndFragmented"])
        self.assertFalse(transitions["continuousOpaqueDiscAllowed"])
        self.assertEqual([0.38, 0.58], transitions["rootFragmentScaleRange"])
        self.assertEqual(
            [0.10, 0.22], transitions["rootFragmentMinorAxisFractionRange"]
        )
        self.assertEqual(0.10, transitions["rootFragmentZFraction"])
        self.assertEqual(-0.20, transitions["rootFragmentZOffsetCentimeters"])
        self.assertEqual([0.035, 0.070], transitions["leafLitterScaleRange"])
        self.assertEqual(
            [0.22, 0.52], transitions["leafLitterMinorAxisFractionRange"]
        )
        self.assertEqual(0.24, transitions["leafLitterZFraction"])
        self.assertTrue(transitions["transitionMeshZUsesFragmentScale"])
        self.assertEqual(64, transitions["legacyV5BFullApronInstances"])
        self.assertEqual(
            [220, 360],
            transitions["legacyV5BNominalFullApronDiameterCentimeters"],
        )
        self.assertTrue(transitions["legacyV5BFullApronsSuppressedAtRuntime"])
        self.assertTrue(
            transitions["legacyV5BVisibilitySnapshotAndRestoreOnEndPlay"]
        )
        self.assertFalse(transitions["legacyV5BMaterialModified"])
        self.assertFalse(transitions["legacyV5BTransformsAndCensusModified"])
        self.assertIn("M_IPV5D_SoilMulch_Layered", transitions["material"])
        for token in (
            "BuildDeterministicTreeBaseLayout",
            "RootApronInstances",
            "LeafLitterInstances",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCanEverAffectNavigation(false)",
        ):
            self.assertIn(token, self.actor_h + self.actor_cpp)

        layout_start = self.actor_cpp.index("BuildDeterministicTreeBaseLayout(")
        layout_end = self.actor_cpp.index("ExtractSourceTreeTransforms(", layout_start)
        layout = self.actor_cpp[layout_start:layout_end]
        for exact_constant in (
            "SelectedNearTreeCount = 128",
            "RootFragmentsPerTree = 3",
            "LeafLitterPerTree = 8",
            "RootFragmentCount == 384",
            "LeafLitterCount == 1024",
            "RootFragmentOffsetMinCm = 18.0",
            "RootFragmentOffsetMaxCm = 62.0",
            "RootApronZOffsetCm = -0.20",
            "RootApronScaleMin = 0.38",
            "RootApronScaleMax = 0.58",
            "LeafLitterScaleMin = 0.035",
            "LeafLitterScaleMax = 0.070",
        ):
            self.assertIn(exact_constant, self.actor_cpp)
        for token in (
            "ExistingBaseClearanceCm + RootFragmentOffsetMaxCm",
            "RootScale * 0.10",
            "LitterScale * 0.24",
            "RootFragmentIndex < RootFragmentsPerTree",
            "LitterIndex < LeafLitterPerTree",
            "RootFragmentHash = MixBits(",
            "RootAngleSectorDegrees = 360.0 / RootFragmentsPerTree",
            "LitterAngleSectorDegrees = 360.0 / LeafLitterPerTree",
            "RootRadiusBandWidth",
            "RootScaleBandWidth",
            "LitterRadiusBandWidth",
            "LitterScaleBandWidth",
        ):
            self.assertIn(token, layout)
        self.assertNotIn("HashUnit(Hash ^ 0x165667B1u)),\n                1.0", layout)
        self.assertNotIn("HashUnit(LitterHash ^ 0xE6546B64u)),\n                    1.0", layout)
        self.assertIn(
            "OutLayout.RootApronWorldTransforms.Num() != RootFragmentCount",
            layout,
        )
        self.assertIn(
            "OutLayout.LeafLitterWorldTransforms.Num() != LeafLitterCount",
            layout,
        )

        automation_start = self.actor_cpp.index(
            "FTRIADIstanaExploreV5DTreeRealismDeterministicTransitionsTest::RunTest("
        )
        automation = self.actor_cpp[automation_start:]
        for native_assertion in (
            "A.SelectedSourceTreeIndices.Num(),\n        SelectedNearTreeCount",
            "A.RootApronWorldTransforms.Num(),\n        RootFragmentCount",
            "A.LeafLitterWorldTransforms.Num(),\n        LeafLitterCount",
            "Ordinal * RootFragmentsPerTree + RootFragmentIndex",
            "DistanceSquared2D(\n                        Root.GetTranslation(),\n                        Previous.GetTranslation()) > 1.0",
            "FMath::Abs(RootRadius - PreviousRadius) > 1.0",
            "FMath::Abs(RootScale.X - PreviousScale.X) > 0.001",
            "FMath::FindDeltaAngleDegrees(",
            "RootApronZOffsetCm",
            "Ordinal * LeafLitterPerTree + PreviousLitterIndex",
            "TransformArraysEqual(Trees, SourceTreesBeforeBuild)",
            "TransformArraysEqual(ExistingBases, ExistingBasesBeforeBuild)",
        ):
            self.assertIn(native_assertion, automation)
        self.assertIn(
            "selectedNearTrees=%d rootFragmentsPerTree=%d "
            "deterministicRootFragments=%d rootFragmentsPairwiseNonCoincident=true "
            "rootFragmentOffsetCm=18..62 rootFragmentMajorScale=0.38..0.58 "
            "rootFragmentMinorFraction=0.10..0.22 rootFragmentZFraction=0.10 "
            "rootFragmentZOffsetCm=-0.20 leafLitterFragmentsPerTree=%d "
            "deterministicFragmentedLeafLitter=%d",
            self.actor_cpp,
        )
        for report_token in (
            "isolatedTreeDerivatives=%d",
            "runtimeMinimumLod=%d",
            "forcedLodModel=%d",
            "sourceLOD0AvailableNearCamera=true",
            "automaticScreenSizeLod=true",
            "runtimeLodDistanceScale=%.1f",
            "mediumRangeCrownDetailRetained=true",
            "allTreesForcedToLod0=false",
            "rootFragmentOffsetCm=18..62",
            "rootFragmentMajorScale=0.38..0.58",
            "rootFragmentMinorFraction=0.10..0.22",
            "rootFragmentZFraction=0.10",
            "rootFragmentZOffsetCm=-0.20",
        ):
            self.assertIn(report_token, self.actor_cpp)

        for token in (
            "ApplyRuntimeLegacyTreeBaseApronSuppression",
            "ValidateRuntimeLegacyTreeBaseApronSuppression",
            "RestoreRuntimeLegacyTreeBaseApronSuppression",
            "LegacyAprons->SetVisibility(false, false)",
            "SavedExistingTreeBaseWorldTransforms",
            "TransformArraysEqual(",
            "LegacyAprons->GetCollisionEnabled() != ECollisionEnabled::NoCollision",
            "LegacyAprons->CanEverAffectNavigation()",
            "V5BVisualActor->SavedAssetRoster.FormalBedVeneerMaterial",
        ):
            self.assertIn(token, self.actor_h + self.actor_cpp)

        apply_start = self.actor_cpp.index(
            "ApplyRuntimeLegacyTreeBaseApronSuppression(FString& OutError)"
        )
        apply_end = self.actor_cpp.index(
            "ValidateRuntimeLegacyTreeBaseApronSuppression(FString& OutError) const",
            apply_start,
        )
        apply_legacy = self.actor_cpp[apply_start:apply_end]
        self.assertNotIn("UpdateInstanceTransform", apply_legacy)
        self.assertNotIn("SetMaterial", apply_legacy)
        self.assertIn("!LegacyAprons->IsVisible()", self.actor_cpp)
        self.assertIn(
            "RestoreRuntimeLegacyTreeBaseApronSuppression();", self.actor_cpp
        )
        restore_start = self.actor_cpp.index(
            "RestoreRuntimeLegacyTreeBaseApronSuppression()"
        )
        restore_end = self.actor_cpp.index(
            "ValidateAppliedRuntimeOverrideForSourceActor(", restore_start
        )
        restore_legacy = self.actor_cpp[restore_start:restore_end]
        self.assertIn("bRuntimeLegacyTreeBaseApronWasVisible", restore_legacy)
        self.assertIn(
            "bRuntimeLegacyTreeBaseApronWasHiddenInGame", restore_legacy
        )
        self.assertIn("LegacyAprons->SetVisibility(", restore_legacy)
        self.assertIn("LegacyAprons->SetHiddenInGame(", restore_legacy)

        validate_start = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::ValidateOwnedTransitions("
        )
        validate_end = self.actor_cpp.index(
            "ValidateAppliedRuntimeOverrideForSourceActor(", validate_start
        )
        validate = self.actor_cpp[validate_start:validate_end]
        readiness = validate.index("!Row.Component->IsTreeFullyBuilt()")
        begun_play_gate = validate.rindex("HasActorBegunPlay()", 0, readiness)
        self.assertLess(begun_play_gate, readiness)
        self.assertNotIn("Row.Component->IsAsyncBuilding()", validate)
        self.assertIn(
            "A begun-play V5D tree-base transition cluster tree is not render-ready.",
            validate,
        )

        begin_play_start = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::BeginPlay()"
        )
        begin_play_end = self.actor_cpp.index(
            "void ATRIADIstanaExploreV5DTreeRealismActor::EndPlay(",
            begin_play_start,
        )
        begin_play = self.actor_cpp[begin_play_start:begin_play_end]
        self.assertIn("RootApronInstances.Get()", begin_play)
        self.assertIn("LeafLitterInstances.Get()", begin_play)
        self.assertIn("Component->BuildTreeIfOutdated(false, true)", begin_play)

        populate_start = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::PopulateOwnedTransitions("
        )
        populate_end = self.actor_cpp.index(
            "bool ATRIADIstanaExploreV5DTreeRealismActor::ConfigureTreeRealism(",
            populate_start,
        )
        populate = self.actor_cpp[populate_start:populate_end]
        self.assertIn("Population.Component->IsTreeFullyBuilt()", populate)
        self.assertNotIn("Population.Component->IsAsyncBuilding()", populate)

    def test_frozen_v1_unsupported_material_record_is_superseded_only_in_isolated_v3(self):
        material = self.contract["materialVariation"]
        self.assertFalse(material["supportedByExistingAdmittedTreeMaterialGraphs"])
        self.assertFalse(material["sourceOrRuntimeWindMaterialMutationAllowed"])
        self.assertFalse(material["unsupportedColorOrRoughnessOverrideInvented"])
        self.assertIn("sourceWindMidsSnapshottedAndRestored=true", self.actor_cpp)
        self.assertIn("sixWindParametersCachedAndPropagatedAfterV4Tick=true", self.actor_cpp)
        self.assertIn("duplicates the exact thirteen", self.readme)
        self.assertIn("original opacity input", self.readme)

    def test_builder_hook_and_native_automation_are_exact(self):
        hook = "ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder"
        self.assertIn(hook, self.editor_h)
        self.assertIn(hook, self.editor_cpp)
        trusted_decl = self.editor_h.split(hook, 1)[1].split(";", 1)[0]
        self.assertNotIn("UFUNCTION", trusted_decl)
        self.assertIn(
            "TRIAD.Istana.ExploreV5D.TreeRealism.DeterministicTransitions",
            self.actor_cpp,
        )
        self.assertGreaterEqual(len(re.findall(r"TestTrue\(|TestEqual\(", self.actor_cpp)), 7)


if __name__ == "__main__":
    unittest.main()
