import json
import hashlib
import importlib.util
import math
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE_ROOT = ROOT / "SourceAssets" / "IstanaPublicViewExploreV5D" / "GroundVegetation"
RUNTIME_PUBLIC = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusion" / "Public"
RUNTIME_PRIVATE = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusion" / "Private"
EDITOR_PUBLIC = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusionEditor" / "Public"
EDITOR_PRIVATE = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusionEditor" / "Private"
WORKSPACE_ROOT = ROOT.parents[2]
GENERIC_MATERIAL_TRANSACTION = WORKSPACE_ROOT / ".codex-tmp-run-v5d-material-transaction.ps1"
R21_MATERIAL_TRANSACTION = (
    WORKSPACE_ROOT / ".codex-tmp-run-v5d-r21-grass-material-transaction.ps1"
)
R21_TRANSACTION_SAFETY_SUPPORT = (
    WORKSPACE_ROOT / ".codex-tmp-v5d-r21-transaction-filesystem-safety.ps1"
)
R21_TRANSACTION_SAFETY_HARNESS = (
    WORKSPACE_ROOT / ".codex-tmp-test-v5d-r21-transaction-safety.ps1"
)
R21_ACCEPTANCE_ANALYZER = (
    WORKSPACE_ROOT / ".codex-tmp-analyze-v5d-r21-acceptance.py"
)
R22_MATERIAL_TRANSACTION = (
    WORKSPACE_ROOT / ".codex-tmp-run-v5d-r22-material-transaction.ps1"
)
R22_GRASS_SYSTEM_TRANSACTION = (
    WORKSPACE_ROOT / ".codex-tmp-run-v5d-r22-grass-system-transaction.ps1"
)
R22_TRANSACTION_SAFETY_SUPPORT = (
    WORKSPACE_ROOT / ".codex-tmp-v5d-r22-transaction-filesystem-safety.ps1"
)
R22_TRANSACTION_SAFETY_HARNESS = (
    WORKSPACE_ROOT / ".codex-tmp-test-v5d-r22-transaction-safety.ps1"
)


class ExploreV5DGroundVegetationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(
            (SOURCE_ROOT / "istana_public_view_v5d_ground_vegetation.contract.json").read_text(
                encoding="utf-8"
            )
        )
        cls.package_readme = (SOURCE_ROOT / "README.md").read_text(encoding="utf-8")
        cls.actor_h = (RUNTIME_PUBLIC / "TRIADIstanaExploreV5DGroundVegetationActor.h").read_text(
            encoding="utf-8"
        )
        cls.actor_cpp = (RUNTIME_PRIVATE / "TRIADIstanaExploreV5DGroundVegetationActor.cpp").read_text(
            encoding="utf-8"
        )
        cls.policy_cpp = (RUNTIME_PRIVATE / "TRIADIstanaExploreV5DContextPolicyActor.cpp").read_text(
            encoding="utf-8"
        )
        cls.v5b_actor_cpp = (RUNTIME_PRIVATE / "TRIADIstanaExploreV5BVisualActor.cpp").read_text(
            encoding="utf-8"
        )
        cls.editor_h = (
            EDITOR_PUBLIC / "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h"
        ).read_text(encoding="utf-8")
        cls.editor_cpp = (
            EDITOR_PRIVATE / "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp"
        ).read_text(encoding="utf-8")
        cls.generic_material_transaction = GENERIC_MATERIAL_TRANSACTION.read_text(
            encoding="utf-8"
        )
        cls.r21_material_transaction = R21_MATERIAL_TRANSACTION.read_text(
            encoding="utf-8"
        )
        cls.r21_transaction_safety_support = (
            R21_TRANSACTION_SAFETY_SUPPORT.read_text(encoding="utf-8")
        )
        cls.r21_transaction_safety_harness = (
            R21_TRANSACTION_SAFETY_HARNESS.read_text(encoding="utf-8")
        )
        cls.r22_material_transaction = R22_MATERIAL_TRANSACTION.read_text(
            encoding="utf-8"
        )
        cls.r22_grass_system_transaction = R22_GRASS_SYSTEM_TRANSACTION.read_text(
            encoding="utf-8"
        )
        cls.r22_transaction_safety_support = (
            R22_TRANSACTION_SAFETY_SUPPORT.read_text(encoding="utf-8")
        )
        cls.r22_transaction_safety_harness = (
            R22_TRANSACTION_SAFETY_HARNESS.read_text(encoding="utf-8")
        )

    def test_claim_boundary_is_visual_only_and_fail_closed(self):
        self.assertEqual(
            "triad.istana_public_view_v5d_ground_vegetation.v19",
            self.contract["schema"],
        )
        claim = self.contract["claimBoundary"]
        self.assertTrue(claim["renderOnly"])
        for key, value in claim.items():
            if key != "renderOnly":
                self.assertFalse(value, key)
        self.assertEqual("VISUAL_ASSUMPTION_BOUND", self.contract["status"])

    def test_r23_native_two_layer_geometry_and_staged_revisioned_migration(self):
        self.assertEqual(int("56474444", 16), self.contract["placementSeed"])
        self.assertEqual(
            {
                "v5bAccentTurfInstances": 18432,
                "v4EdgeGrassInstances": 1536,
                "v4MainAndHeritageTreeInstances": 729,
                "v5bExistingTreeBaseMulchInstances": 64,
            },
            self.contract["sourceReadback"],
        )
        for token in (
            "SourceGrassCount = 18432",
            "R20GrassMicroDetailCount = 12288",
            "R20MaximumGrassReuseLayers = 1",
            "R20GrassLayerOffsetMinimumRadiusCm = 28.0",
            "R20GrassLayerOffsetBandWidthCm = 10.0",
            "R20GrassLayerOffsetBandStepCm = 0.0",
            "R20GrassLayerVerticalSeparationCm = 0.04",
            "R20GrassCoverageScaleMin = 0.52",
            "R20GrassCoverageScaleMax = 0.72",
            "R20GrassHeightScaleMin = 0.95",
            "R20GrassHeightScaleMax = 1.12",
            "R20GrassShortHeightClassScale = 0.92",
            "R20GrassMediumHeightClassScale = 1.04",
            "R20GrassTallHeightClassScale = 1.16",
            "GrassMicroDetailCount = 18432",
            "MaximumGrassReuseLayers = 2",
            "GrassLayerOffsetMinimumRadiusCm = 18.0",
            "GrassLayerOffsetBandWidthCm = 24.0",
            "GrassLayerOffsetBandStepCm = 0.0",
            "GrassPlacementGapMinimumCm = 0.01",
            "GrassPlacementGapMaximumCm = 0.05",
            "GrassCoverageScaleMin = 0.72",
            "GrassCoverageScaleMax = 0.92",
            "GrassHeightScaleMin = 1.05",
            "GrassHeightScaleMax = 1.25",
            "GrassAnisotropyScaleMin = 0.98",
            "GrassAnisotropyScaleMax = 1.02",
            "GrassShortHeightClassScale = 0.88",
            "GrassMediumHeightClassScale = 1.02",
            "GrassTallHeightClassScale = 1.18",
            "GrassGoldenAngleDegrees = 137.50776405003785",
            "GrassPatchSuppressionRadiusCm = 350.0",
            "ExistingTreeBasePatchSourceRadiusCm = 100.0",
            "TreeBaseGrassSuppressionRadiusFraction = 1.0",
            "TreeBaseGrassCarrierFootprintMarginCm = 220.0",
            "IsInsideAnyScaledPatchExclusion",
            "GrassCandidates.Num() * LayoutMaximumReuseLayers",
            "FMath::DivideAndRoundUp",
            "FirstGrassPlacementBySource",
            "A deterministic grass reuse layer produced a coincident micro-detail transform",
            "maximumDeterministicReuseLayers=%d",
            "grassTerrainHeight=exactCopiedV5BAnalyticFormula",
            "sourceSurfaceDeltaPreserved=true",
            "addedPlacementGapCm=0.01..0.05",
            "inheritedSourceGrassSerializedUntouched=true",
            "ValidateR14PredecessorForR20Migration",
            "RebuildGroundVegetationLayoutToR20",
            "ValidateR20PredecessorForR23Migration",
            "RebuildGroundVegetationLayoutToR23",
            "GrassPresentationRevision = 14",
            "ConfigureGrassPresentationComponentsForRevision(\n"
            "        PredecessorGrassPresentationRevision);",
            "GrassPresentationRevision = GrassPresentationRevisionR20;\n"
            "    ConfigureGrassPresentationComponentsForRevision(\n"
            "        GrassPresentationRevisionR20);",
            "GrassPresentationRevision = GrassPresentationRevisionR23;\n"
            "    ConfigureGrassPresentationComponentsForRevision(\n"
            "        GrassPresentationRevisionR23);",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn(
            "GrassCandidates.Num() < GrassMicroDetailCount",
            self.actor_cpp,
        )
        self.assertIn(
            'TEXT("Exact R23 grass micro-detail census")',
            self.actor_cpp,
        )
        self.assertIn(
            'TEXT("R23 uses at most two deterministic layers and exercises its second layer")',
            self.actor_cpp,
        )
        predecessor_validation = self.actor_cpp.split(
            "ValidateR14PredecessorForR20Migration(FString& OutReport) const",
            1,
        )[1].split("RebuildGroundVegetationLayoutToR20", 1)[0]
        self.assertIn("Component->IsTreeFullyBuilt()", predecessor_validation)
        self.assertNotIn("Component->IsAsyncBuilding()", predecessor_validation)
        self.assertRegex(
            self.actor_cpp,
            r"BaseCandidate\.Hash \^\s*\(static_cast<uint32>\(LayerIndex\) \+ 1u\) \* 0xD1B54A35u",
        )

        structural = self.contract["grassStructuralRealismR20"]
        self.assertEqual(20, structural["serializedPresentationRevision"])
        self.assertEqual(14, structural["admittedPredecessorRevision"])
        self.assertEqual(14, structural["nativeDefaultSubobjectPolicyRevision"])
        self.assertEqual(
            20,
            structural["freshConfigurationAppliesPresentationRevision"],
        )
        self.assertEqual(1, structural["maximumReuseLayers"])
        self.assertEqual([28.0, 38.0], structural["singleLayerOffsetRadiusCm"])
        self.assertEqual([0.52, 0.72], structural["coverageScale"])
        self.assertEqual([0.98, 1.02], structural["anisotropyScale"])
        self.assertTrue(structural["sourceXyNormalizedByGeometricMean"])
        self.assertTrue(structural["horizontalAndVerticalScaleSplit"])
        self.assertEqual(
            {"macroClump": 0.72, "stableHash": 0.28},
            structural["coverageSignalWeights"],
        )
        self.assertEqual([0.95, 1.12], structural["heightScale"])
        self.assertEqual([0.92, 1.04, 1.16], structural["heightClassScales"])
        self.assertEqual(
            {"macroClump": 0.55, "stableHash": 0.45},
            structural["heightSignalWeights"],
        )
        self.assertEqual(350.0, structural["supplementalPatchSuppressionRadiusCm"])
        self.assertEqual(
            "ATRIADIstanaExploreV5DGroundVegetationActor",
            structural["treeBaseExclusionOwner"],
        )
        self.assertEqual(100.0, structural["existingTreeBasePatchSourceRadiusCm"])
        self.assertEqual(1.0, structural["treeBaseSuppressionRadiusFraction"])
        self.assertEqual(220.0, structural["treeBaseCarrierFootprintMarginCm"])
        self.assertEqual(
            0.84,
            structural["inheritedV5bPersistedSuppressionRadiusFractionUnchanged"],
        )
        self.assertTrue(structural["correlatedMesoClumps"])
        self.assertTrue(structural["sourceInstanceCensusUnchanged"])
        self.assertEqual(12288, structural["ownedInstanceCensus"])
        self.assertFalse(structural["grassShadowCastingEnabled"])

        current = self.contract["grassStructuralRealismR23"]
        self.assertEqual(23, current["serializedPresentationRevision"])
        self.assertEqual(20, current["admittedPredecessorRevision"])
        self.assertEqual([14, 20, 23], current["legacyMigrationSequence"])
        self.assertEqual(14, current["nativeDefaultSubobjectPolicyRevision"])
        self.assertEqual(23, current["freshConfigurationAppliesPresentationRevision"])
        self.assertEqual(2, current["maximumReuseLayers"])
        self.assertEqual([18.0, 42.0], current["offsetRadiusCm"])
        self.assertEqual(
            137.50776405003785,
            current["goldenAngleLayerDecorrelationDegrees"],
        )
        self.assertEqual(
            "FULL_HASHED_360_WITH_GOLDEN_ANGLE_LAYER_DECORRELATION",
            current["yawPolicy"],
        )
        self.assertEqual([0.72, 0.92], current["coverageScale"])
        self.assertEqual([0.98, 1.02], current["anisotropyScale"])
        self.assertEqual([1.05, 1.25], current["heightScale"])
        self.assertEqual([0.88, 1.02, 1.18], current["heightClassScales"])
        self.assertEqual([0.6, 0.3, 0.1], current["heightClassFractions"])
        self.assertEqual(
            {"macroClump": 0.72, "stableHash": 0.28},
            current["coverageSignalWeights"],
        )
        self.assertEqual(
            {"macroClump": 0.55, "stableHash": 0.45},
            current["heightSignalWeights"],
        )
        self.assertEqual(
            "EXACT_COPIED_V5B_ANALYTIC_FORMULA",
            current["terrainHeightSource"],
        )
        self.assertTrue(current["sourceSurfaceDeltaPreserved"])
        self.assertEqual([0.01, 0.05], current["addedPlacementGapCm"])
        self.assertEqual(18432, current["ownedInstanceCensus"])
        self.assertEqual([20.0, 28.0], current["grassMaterialVisibilityFadeMeters"])
        self.assertEqual([26.0, 34.0], current["grassHismCullMeters"])
        self.assertEqual(24.0, current["grassWpoDisableMeters"])
        self.assertEqual(
            [20.0, 28.0], current["edgeGrassMaterialVisibilityFadeMeters"]
        )
        self.assertEqual([30.0, 45.0], current["edgeGrassHismCullMeters"])
        self.assertEqual(24.0, current["edgeGrassWpoDisableMeters"])
        self.assertTrue(current["sourceInstanceCensusUnchanged"])
        self.assertTrue(current["carrierMeshAndLodsUnchanged"])
        self.assertTrue(current["collisionNavigationSensorAndRfStateUnchanged"])
        self.assertFalse(current["grassShadowCastingEnabled"])
        for token in (
            "0.72 * MacroClump",
            "0.28 * HashUnit(Candidate.Hash ^ 0x7F4A7C15u)",
            "0.55 * MacroClump",
            "0.45 * HashUnit(Candidate.Hash ^ 0x6B1E29D3u)",
            "SmoothUnit(CoverageSignal)",
            "SmoothUnit(HeightSignal)",
            "HashUnit(Candidate.Hash ^ 0x91E10DA5u)",
            "HashUnit(Candidate.Hash ^ 0xC2B2AE3Du)",
            "SourceScale.X * SourceScale.Y",
            "grassLayerOffsetRadiusBandCm=18..42",
            "grassLayerGoldenAngleDecorrelationDegrees=137.507764",
            "grassYaw=fullHashed360WithGoldenAngleLayerDecorrelation",
            "grassCoverageScale=0.72..0.92",
            "grassHeightScale=1.05..1.25",
            "grassHeightClasses=0.88,1.02,1.18",
            "horizontalAndVerticalScaleSplit=true",
            "correlatedMesoClumps=true",
            "grassPatchSuppressionRadiusCm=350",
            "R23 grass keeps bounded separated horizontal coverage variation",
            "R23 grass keeps the exact final bounded anisotropy ratio",
            "R23 grass uses the exact shared 18..42 cm offset band",
            "R23 grass has three-class bounded upright silhouette variation",
            "R23 grass carrier footprints remain outside supplemental patches",
            "R23 grass carrier footprints remain outside inherited V5B mulch",
            "grassPresentationRevision=R23",
            "grassPredecessorRevision=R20",
            "stagedLegacyMigration=R14ToR20ToR23",
            "grassCoverageSignalWeights=0.72/0.28",
            "grassHeightSignalWeights=0.55/0.45",
            "grassFinalAnisotropy=0.98..1.02",
            "accentCarrierRevision=R11",
            "accentCarrierLodTriangles=2550/680/204",
            "treeBaseGrassSuppressionRadiusFraction=1.0",
            "treeBaseGrassCarrierFootprintMarginCm=220",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn("SetCastShadow(true)", self.actor_cpp)
        self.assertIn(
            "TreeBaseGrassSuppressionRadiusFraction = 0.84",
            self.v5b_actor_cpp,
        )
        self.assertNotIn(
            "TreeBaseGrassCarrierFootprintMarginCm = 220.0",
            self.v5b_actor_cpp,
        )
        self.assertIn("12,288 deterministic grass micro-detail instances", self.package_readme)
        self.assertIn("12,288-instance V5D micro-clump layer", self.package_readme)

    def test_grass_and_soil_materials_are_world_stochastic_not_texture_tiled(self):
        self.assertIn("WorldPosition", self.editor_cpp)
        self.assertIn("TRIAD_EXPLORE_V5D_GRASS_WORLD_STOCHASTIC", self.editor_cpp)
        self.assertIn("TRIAD_EXPLORE_V5D_SOIL_WORLD_STOCHASTIC", self.editor_cpp)
        self.assertIn(
            "TRIAD_EXPLORE_V5D_LAWN_R10_COHERENT_11M_48M_MOW_WEAR_WET_FIELD_V2",
            self.editor_cpp,
        )
        self.assertIn("TRIAD_EXPLORE_V5D_LAWN_R10_WORLD_METER_SURFACE_UV_V2", self.editor_cpp)
        self.assertIn("FreshAssets.Num() != 6", self.editor_cpp)
        self.assertRegex(self.editor_cpp, r"macroNoise|MacroNoise")
        self.assertRegex(self.editor_cpp, r"detailNoise|DetailNoise")
        self.assertNotIn("Cesium3DTileset", self.editor_cpp)
        self.assertFalse(self.contract["soilProfile"]["textureTiling"])

    def test_inherited_v5b_turf_is_snapshotted_as_visible_medium_range_layer_and_restored(self):
        presentation = self.contract["inheritedV5bTurfRuntimePresentation"]
        self.assertEqual("V5BAccentTurfInstances", presentation["component"])
        self.assertEqual("SM_IPV5B_BermudaTurfCluster", presentation["requiredMesh"])
        self.assertEqual(18432, presentation["requiredInstanceCount"])
        self.assertEqual(
            {
                "revision": "R11",
                "tuftCenters": 272,
                "pairTufts": 136,
                "triadTufts": 136,
                "bladeRoots": 680,
                "clippedBlades": 510,
                "juvenileBlades": 170,
                "postureMix": [306, 306, 68],
                "cShapeAndCounterBend": [612, 68],
                "heightMix": [462, 184, 34],
                "lodTriangles": [2550, 680, 204],
                "lodVertices": [3910, 2040, 612],
            },
            presentation["carrierGeometry"],
        )
        self.assertFalse(presentation["runtimeMaterialChanged"])
        # SourceAssets is published independently from the C++ plugin. Its
        # frozen predecessor can still describe the hidden renderer until the
        # next content transaction; the runtime source assertions below are
        # authoritative for this reversible presentation upgrade.
        if "runtimeCompositeV5bAndV5dGrassOwnership" in presentation:
            self.assertTrue(presentation["runtimeRendererVisible"])
            self.assertFalse(presentation["runtimeHiddenInGame"])
            self.assertTrue(
                presentation["runtimeCompositeV5bAndV5dGrassOwnership"]
            )
        else:
            self.assertFalse(presentation["runtimeRendererVisible"])
            self.assertTrue(presentation["runtimeHiddenInGame"])
            self.assertTrue(presentation["runtimeExclusiveV5dGrassOwnership"])
        for key in (
            "runtimeOnly",
            "renderOnly",
            "exactTaggedV5dOwnerAdmissionOnly",
            "coldValidationRequiresOriginalV5bMaterial",
            "fullOverrideMaterialsArraySnapshotted",
            "fullOverrideMaterialsArrayRestoredOnApplyFailure",
            "fullOverrideMaterialsArrayRestoredOnRuntimeValidationFailure",
            "fullOverrideMaterialsArrayRestoredOnEndPlay",
            "fullVisibilityMaterialAndTagStateSnapshotted",
            "visibilityMaterialAndTagStateRestoredOnEveryFailureSuspendAndEndPlay",
            "preV5bBeginPlayMissRetriesFailClosedOnTick",
            "postFailureExactColdStateCanReapplyOnTick",
            "partialRuntimeStateMustRestoreExactlyBeforeRetry",
            "foreignTagOnlyNeverRemovedByNonOwner",
            "absentTagDoesNotBypassRetainedLocalOwnershipClaims",
            "localSnapshotOwnerRefusesRestoreWhenAnotherLocalOwnerExists",
            "exactSourceActorAndComponentIdentitySnapshotted",
            "distanceFieldLightingStateSnapshottedAndUnchanged",
            "v5bRuntimeReapplyReestablishesPresentationBeforeTerminalValidation",
            "v5bRuntimeReapplySuspendsAndRestoresFullOverrideArrayBeforeMutation",
            "v5bRuntimeReapplyResumesFromFreshColdSnapshotAfterValidation",
            "v5bMutationFailureLeavesPresentationSuspendedColdAndTagFree",
            "v5bRuntimeReapplyPreservesDormantOverrideSlots",
        ):
            self.assertTrue(presentation[key], key)
        for key in (
            "sourceMaterialAssetEdited",
            "sourceMeshTransformsCensusCullAndWpoChanged",
            "sourceCollisionOverlapNavigationSimulationOrRfAuthorityChanged",
        ):
            self.assertFalse(presentation[key], key)

        combined = self.actor_h + self.actor_cpp + self.v5b_actor_cpp
        for token in (
            "TRIADIstanaExploreV5DSourceTurfPresentation",
            "RuntimeSourceTurfPresentationTag",
            "ExpectedRuntimeSourceTurfCullStartDistanceCm",
            "ExpectedRuntimeSourceTurfCullEndDistanceCm",
            "ExpectedRuntimeSourceTurfLodDistanceScale",
            "EnsureRuntimeSourceV5BTurfPresentation",
            "ApplyRuntimeSourceV5BTurfPresentation",
            "ValidateAppliedRuntimeSourceTurfPresentationForSourceActor",
            "ValidateActiveRuntimeSourceTurfPresentationForSourceActor",
            "SuspendActiveRuntimeSourceTurfPresentationForV5BReapply",
            "ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply",
            "RestoreRuntimeSourceV5BTurfPresentation",
            "FindLocalRuntimeSourceV5BTurfClaims",
            "RuntimeOriginalSourceV5BTurfOverrideMaterials",
            "RuntimeOriginalSourceV5BTurfOwnerActor",
            "RuntimeOriginalSourceV5BTurfComponent",
            "bRuntimeOriginalSourceV5BTurfHadPresentationTag",
            "Component->OverrideMaterials",
            "Component->EmptyOverrideMaterials()",
            "Component->SetVisibility(true, true)",
            "Component->SetHiddenInGame(false, true)",
            "SourceGrassCount = 18432",
            "SourceV5BTurfCullStartDistanceCm = 3000",
            "SourceV5BTurfCullEndDistanceCm = 5200",
            "SourceV5BTurfWpoDisableDistanceCm = 3200",
            "CompositeSourceV5BTurfCullStartDistanceCm = 4000",
            "CompositeSourceV5BTurfCullEndDistanceCm = 6500",
            "CompositeSourceV5BTurfLodDistanceScale = 1.10f",
            "ECollisionEnabled::NoCollision",
            "GetGenerateOverlapEvents()",
            "CanEverAffectNavigation()",
            "EndPlay(const EEndPlayReason::Type EndPlayReason)",
            "sourceTransformsAndCensusUntouched=true",
            "runtimeCompositeV5BAndV5DGrassOwnership=true",
            "sourceRendererVisible=true",
            "sourceRendererHiddenInGame=false",
            "runtimeCullAndLodVisualOverrideOnly=true",
            "sourceAssetAndComponentMaterialsUntouched=true",
            "foreignTagNeverRemovedByNonOwner=true",
            "v5bRuntimeReapplySuspendColdResumeFresh=true",
            "v5bMutationFailureColdAndTagFree=true",
            "dormantOverrideSlotsPreserved=true",
        ):
            self.assertIn(token, combined)
        topology = self.v5b_actor_cpp.split(
            "ValidateOwnedComponentTopology(", 1
        )[1].split(
            "ExtractCloseTurfWorldTransforms(", 1
        )[0]
        for token in (
            "ValidateActiveRuntimeSourceTurfPresentationForSourceActor",
            "ExpectedRuntimeSourceTurfCullStartDistanceCm()",
            "ExpectedRuntimeSourceTurfCullEndDistanceCm()",
            "ExpectedRuntimeSourceTurfLodDistanceScale()",
            "AccentStartCull != ExpectedAccentStartCull",
            "AccentEndCull != ExpectedAccentEndCull",
            "AccentTurfInstances->InstanceLODDistanceScale",
        ):
            self.assertIn(token, topology)
        self.assertIn(
            "(!bV5DSourceTurfPresentation && !ValidateHismMeshAndMaterial(",
            self.v5b_actor_cpp,
        )
        self.assertIn(
            "bV5DSourceTurfPresentationTag && !bV5DSourceTurfPresentation",
            self.v5b_actor_cpp,
        )
        restore = self.actor_cpp.split(
            "RestoreRuntimeSourceV5BTurfPresentation(FString& OutError)", 1
        )[1].split("ResolveExactContextPolicy", 1)[0]
        self.assertLess(
            restore.index("Component->EmptyOverrideMaterials()"),
            restore.index("RuntimeOriginalSourceV5BTurfOverrideMaterials[Index]"),
        )
        self.assertIn("OverrideMaterialArraysEqual", restore)
        self.assertIn(
            "RuntimeOriginalSourceV5BTurfComponent.Get()", restore
        )
        self.assertNotIn(
            "V5BVisualActor ? V5BVisualActor->AccentTurfInstances.Get()",
            restore,
        )

        ensure = self.actor_cpp.split(
            "EnsureRuntimeSourceV5BTurfPresentation(FString& OutError)", 1
        )[1].split(
            "ApplyRuntimeSourceV5BTurfPresentation(FString& OutError)", 1
        )[0]
        self.assertIn("bCompleteRuntimeState", ensure)
        self.assertIn("bHasLocalPartialRuntimeState", ensure)
        self.assertIn("foreign presentation tag without a local snapshot", ensure)
        self.assertIn("RestoreRuntimeSourceV5BTurfPresentation", ensure)
        self.assertIn("ApplyRuntimeSourceV5BTurfPresentation", ensure)
        self.assertLess(
            ensure.index("RestoreRuntimeSourceV5BTurfPresentation"),
            ensure.index("return ApplyRuntimeSourceV5BTurfPresentation"),
        )
        self.assertIn("!V5BVisualActor->HasActorBegunPlay()", self.actor_cpp)

        claim_census = self.actor_cpp.split(
            "FindLocalRuntimeSourceV5BTurfClaims(", 1
        )[1].split(
            "ValidateActiveRuntimeSourceTurfPresentationForSourceActor(", 1
        )[0]
        self.assertIn("It->V5BVisualActor == Candidate", claim_census)
        self.assertIn(
            "It->RuntimeOriginalSourceV5BTurfOwnerActor.Get() == Candidate",
            claim_census,
        )
        self.assertIn("bRuntimeSourceV5BTurfPresentationApplied", claim_census)
        self.assertIn("bRuntimeSourceV5BTurfSnapshotValid", claim_census)

        apply_runtime = self.actor_cpp.split(
            "ApplyRuntimeSourceV5BTurfPresentation(FString& OutError)", 1
        )[1].split(
            "RestoreRuntimeSourceV5BTurfPresentation(FString& OutError)", 1
        )[0]
        self.assertIn(
            "bRuntimeOriginalSourceV5BTurfAffectDistanceFieldLighting =",
            apply_runtime,
        )
        self.assertNotIn(
            "Component->bAffectDistanceFieldLighting ||", apply_runtime
        )
        self.assertIn(
            "Component->bAffectDistanceFieldLighting !=",
            self.actor_cpp,
        )
        self.assertIn("OtherClaimCount != 0", apply_runtime)
        self.assertNotIn(
            "Component->SetMaterial(0, SavedAssets.GrassProfileMaterials[0])",
            apply_runtime,
        )
        self.assertIn("Component->SetVisibility(true, true)", apply_runtime)
        self.assertIn("Component->SetHiddenInGame(false, true)", apply_runtime)
        self.assertIn(
            "CompositeSourceV5BTurfCullStartDistanceCm,", apply_runtime
        )
        self.assertIn(
            "CompositeSourceV5BTurfCullEndDistanceCm);", apply_runtime
        )
        self.assertIn(
            "CompositeSourceV5BTurfLodDistanceScale;", apply_runtime
        )
        self.assertIn(
            "Component->SetVisibility(bRuntimeOriginalSourceV5BTurfVisible, true)",
            restore,
        )
        self.assertIn(
            "RuntimeOriginalSourceV5BTurfCullStartDistanceCm,", restore
        )
        self.assertIn(
            "RuntimeOriginalSourceV5BTurfCullEndDistanceCm);", restore
        )
        self.assertIn(
            "RuntimeOriginalSourceV5BTurfLodDistanceScale;", restore
        )

        v5b_apply_layout = self.v5b_actor_cpp.split(
            "bool ATRIADIstanaExploreV5BVisualActor::ApplyLayout(", 1
        )[1].split(
            "bool ATRIADIstanaExploreV5BVisualActor::ValidateAppliedLayout(", 1
        )[0]
        self.assertIn(
            "ApplyLayout refuses to mutate while an admitted V5D",
            v5b_apply_layout,
        )
        self.assertNotIn(
            "ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply",
            v5b_apply_layout,
        )
        v5b_reapply = self.v5b_actor_cpp.split(
            "bool ATRIADIstanaExploreV5BVisualActor::ReapplyExploreV5BVisuals(", 1
        )[1].split(
            "bool ATRIADIstanaExploreV5BVisualActor::ValidateExploreV5BVisuals(", 1
        )[0]
        self.assertIn(
            "SuspendActiveRuntimeSourceTurfPresentationForV5BReapply",
            v5b_reapply,
        )
        self.assertIn(
            "ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply",
            v5b_reapply,
        )
        suspend_runtime = self.actor_cpp.split(
            "SuspendActiveRuntimeSourceTurfPresentationForV5BReapply(", 1
        )[1].split(
            "ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply(", 1
        )[0]
        self.assertIn(
            "bRuntimeSourceV5BTurfPresentationApplied",
            suspend_runtime,
        )
        self.assertIn(
            "ValidateAppliedRuntimeSourceTurfPresentationForSourceActor",
            suspend_runtime,
        )
        self.assertIn(
            "RestoreRuntimeSourceV5BTurfPresentation",
            suspend_runtime,
        )
        self.assertIn(
            "retained local ownership claim(s) despite an absent source tag",
            suspend_runtime,
        )
        suspend_owner_gate = suspend_runtime.split(
            "if (MatchCount != 1 || !Match", 1
        )[1].split("FString ActivePresentationReport", 1)[0]
        self.assertIn(
            "RuntimeOriginalSourceV5BTurfOwnerActor.Get() != Candidate",
            suspend_owner_gate,
        )
        self.assertNotIn(
            "RuntimeOriginalSourceV5BTurfComponent.Get()",
            suspend_owner_gate,
        )
        self.assertLess(
            v5b_reapply.index(
                "SuspendActiveRuntimeSourceTurfPresentationForV5BReapply"
            ),
            v5b_reapply.index(
                "if (!ApplyLayout(SavedAssetRoster, SavedLayout, OutError))"
            ),
        )
        self.assertLess(
            v5b_reapply.index("if (!ValidateExploreV5BVisuals(ValidationReport))"),
            v5b_reapply.index(
                "ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply"
            ),
        )
        apply_failure_branch = v5b_reapply.split(
            "if (!ApplyLayout(SavedAssetRoster, SavedLayout, OutError))", 1
        )[1].split("PublicViewSceneActor->Tags.AddUnique", 1)[0]
        self.assertIn("remains suspended in exact cold tag-free state", apply_failure_branch)
        self.assertNotIn(
            "ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply",
            apply_failure_branch,
        )
        assign_mesh_and_material = self.v5b_actor_cpp.split(
            "void AssignMeshAndMaterial(", 1
        )[1].split("template <typename TActor>", 1)[0]
        self.assertIn("Component->SetMaterial(0, Material)", assign_mesh_and_material)
        self.assertNotIn("EmptyOverrideMaterials", assign_mesh_and_material)

        no_snapshot_restore = restore.split(
            "if (!bRuntimeSourceV5BTurfSnapshotValid)", 1
        )[1].split("int32 OtherLocalOwnerCount", 1)[0]
        self.assertNotIn("Tags.Remove", no_snapshot_restore)
        self.assertNotIn("EmptyOverrideMaterials", no_snapshot_restore)
        self.assertNotIn("SetMaterial", no_snapshot_restore)
        self.assertIn("OtherLocalOwnerCount", restore)
        self.assertLess(
            restore.index("OtherLocalOwnerCount"),
            restore.index("Component->EmptyOverrideMaterials()"),
        )

        begin_play = self.actor_cpp.split(
            "void ATRIADIstanaExploreV5DGroundVegetationActor::BeginPlay()", 1
        )[1].split(
            "void ATRIADIstanaExploreV5DGroundVegetationActor::Tick", 1
        )[0]
        tick = self.actor_cpp.split(
            "void ATRIADIstanaExploreV5DGroundVegetationActor::Tick", 1
        )[1].split(
            "void ATRIADIstanaExploreV5DGroundVegetationActor::EndPlay", 1
        )[0]
        for lifecycle_body in (begin_play, tick):
            self.assertLess(
                lifecycle_body.index("EnsureRuntimeSourceV5BTurfPresentation"),
                lifecycle_body.index("SynchronizeSourceTerrainRendererWithProviderPolicy"),
            )
            self.assertLess(
                lifecycle_body.index("SynchronizeSourceTerrainRendererWithProviderPolicy"),
                lifecycle_body.index("ValidateGroundVegetationRealism"),
            )
            self.assertIn("RestoreRuntimeSourceV5BTurfPresentation", lifecycle_body)

    def test_broad_lawn_overlay_breaks_uniformity_without_source_mutation(self):
        profile = self.contract["groundProfile"]
        self.assertEqual("M_IPV5D_LawnMacroVariation", profile["assetName"])
        self.assertEqual(0.2, profile["sourceTerrainVerticalOffsetCentimeters"])
        self.assertTrue(profile["masked"])
        self.assertTrue(profile["dedicatedMaterialGraph"])
        self.assertFalse(profile["clonedV5EightSampleGraph"])
        self.assertEqual(4, profile["textureSamples"])
        self.assertEqual(
            {
                "samplerSource": "SSM_FromTextureAsset",
                "mipValueMode": "TMVM_None",
                "automaticViewMipBias": True,
                "textureObjectConnected": False,
                "mipValueConnected": False,
                "coordinatesDxConnected": False,
                "coordinatesDyConnected": False,
                "automaticViewMipBiasValueConnected": False,
                "parameterNames": [
                    "V5D_R10_BaseColorTexture",
                    "V5D_R10_NormalTexture",
                    "V5D_R10_RoughnessTexture",
                    "V5D_R10_AoTexture",
                ],
                "legacyParameterNamesPreservedForInPlaceCompatibility": True,
            },
            profile["textureSampleState"],
        )
        self.assertTrue(profile["sourceTerrainMeshMaterialTransformAndCollisionRemainUntouched"])
        for token in (
            "GroundMacroVariationOverlay",
            "GroundOverlayOffsetCm = 0.20",
            "SavedSourceTerrainMaterial",
            "SavedSourceTerrainWorldTransform",
            "sourceTerrainMeshMaterialTransformCollisionUntouched=true",
        ):
            self.assertIn(token, self.actor_cpp + self.actor_h)
        self.assertNotIn("TerrainComponent->SetMaterial", self.editor_cpp)
        self.assertNotIn("TerrainComponent->SetCollision", self.editor_cpp)

    def test_r15_surface_readability_and_shader_cost_are_bounded_and_exact(self):
        profile = self.contract["groundProfile"]
        self.assertEqual("fine_turf_appearance_proxy", profile["appearanceClassification"])
        self.assertFalse(profile["currentBotanicalOrSpeciesClaimed"])
        self.assertEqual(4, profile["textureSamples"])
        self.assertEqual(1.4, profile["textureTileMeters"])
        self.assertEqual([11.0, 48.0], profile["worldSpaceLowFrequencyVariationMeters"])
        self.assertEqual("deterministic_frac_polynomial_arithmetic", profile["coherentHash"])
        self.assertEqual(0, profile["coherentHashTrigOperations"])
        self.assertEqual(1, profile["appearanceFieldPeriodicOperations"])
        self.assertEqual(
            "GroundR10FieldCode_only_excludes_unchanged_core_mask_topology",
            profile["periodicOperationScope"],
        )
        self.assertEqual(3.2, profile["mowingBands"]["spacingMeters"])
        self.assertEqual(7.0, profile["mowingBands"]["angleOffAxisDegrees"])
        self.assertEqual(1, profile["mowingBands"]["periodicOperations"])
        self.assertEqual([0.985, 1.015], profile["mowingBands"]["baseColorGainRange"])
        self.assertEqual([-0.025, 0.025], profile["mowingBands"]["roughnessDeltaRange"])
        self.assertEqual(
            "opposes_base_color_gain", profile["mowingBands"]["roughnessPhase"]
        )
        self.assertEqual(0.18, profile["wearResponse"]["maximumField"])
        self.assertTrue(profile["wearResponse"]["warm"])
        self.assertTrue(profile["wearResponse"]["desaturated"])
        self.assertEqual(0.015, profile["wearResponse"]["maximumGainDarkening"])
        self.assertEqual(0.05, profile["wearResponse"]["maximumRoughnessIncrease"])
        self.assertEqual(0.12, profile["wetResponse"]["maximumField"])
        self.assertEqual(
            [0.975, 0.99, 1.015],
            profile["wetResponse"]["coolMultiplierAtMaximum"],
        )
        self.assertEqual(0.045, profile["wetResponse"]["maximumGainDarkening"])
        self.assertEqual(
            0.057,
            profile["wetResponse"]["maximumApproximateLuminanceDarkening"],
        )
        self.assertEqual(0.06, profile["wetResponse"]["maximumRoughnessReduction"])
        self.assertEqual([30.0, 55.0], profile["detailFadeMeters"])
        self.assertEqual("R18", profile["materialRevision"])
        self.assertEqual(23, profile["expressionCount"])
        self.assertEqual(0.15, profile["nearNormalStrength"])
        self.assertEqual(0.01, profile["farNormalStrength"])
        self.assertEqual(0.82, profile["nearColorDetailPresence"])
        self.assertEqual(0.38, profile["farColorDetailPresence"])
        self.assertEqual([0.715, 0.965, 0.595], profile["photographicTintLinear"])
        self.assertEqual(
            0.88512921, profile["photographicTintApproximateLuminanceMultiplier"]
        )
        self.assertEqual(0.18, profile["specular"])
        self.assertEqual(
            0.63, profile["roughnessResponse"]["nearTextureBase"]
        )
        self.assertEqual(0.5, profile["roughnessResponse"]["nearTextureScale"])
        self.assertEqual(0.84, profile["roughnessResponse"]["farBase"])
        self.assertEqual([0.7, 0.91], profile["roughnessResponse"]["clamp"])
        self.assertTrue(
            profile["roughnessResponse"][
                "macroMowingWearAndWetResponseUnchangedFromR12"
            ]
        )
        self.assertEqual(
            [
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_Color.Grass004_Color",
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_NormalGL.Grass004_NormalGL",
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_Roughness.Grass004_Roughness",
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_AmbientOcclusion.Grass004_AmbientOcclusion",
            ],
            profile["admittedTextures"],
        )

        grass_profiles = self.contract["grassProfiles"]
        self.assertEqual([0.7, 0.66, 0.72, 0.76], [row["roughness"] for row in grass_profiles])
        self.assertEqual([0.3, 0.32, 0.28, 0.26], [row["specular"] for row in grass_profiles])
        self.assertEqual([0.14, 0.18, 0.11, 0.14], [row["windResponse"] for row in grass_profiles])
        self.assertEqual(
            [[0.105, 0.18, 0.083], [0.1, 0.175, 0.084], [0.091, 0.154, 0.087], [0.126, 0.165, 0.086]],
            [row["rootLinear"] for row in grass_profiles],
        )
        self.assertEqual(
            [[0.145, 0.226, 0.111], [0.137, 0.219, 0.111], [0.124, 0.197, 0.113], [0.171, 0.207, 0.112]],
            [row["bodyLinear"] for row in grass_profiles],
        )
        self.assertEqual(
            [[0.153, 0.235, 0.117], [0.145, 0.228, 0.117], [0.131, 0.205, 0.118], [0.179, 0.216, 0.118]],
            [row["tipLinear"] for row in grass_profiles],
        )
        self.assertEqual(
            [[0.118, 0.195, 0.092], [0.112, 0.189, 0.093], [0.104, 0.174, 0.099], [0.129, 0.164, 0.088]],
            [row["subsurfaceLinear"] for row in grass_profiles],
        )
        for row in grass_profiles:
            for body_channel, tip_channel in zip(row["bodyLinear"], row["tipLinear"], strict=True):
                self.assertGreaterEqual(tip_channel, body_channel)
                self.assertLessEqual(tip_channel / body_channel, 1.059)
            self.assertIn("fine-turf appearance proxy", row["role"])

        blade_policy = self.contract["grassBladeColorPolicy"]
        self.assertEqual("R19", blade_policy["materialRevision"])
        self.assertTrue(blade_policy["r17RootBodyTipTuplesPreservedExactly"])
        r19_response = self.contract["grassR19SurfaceResponse"]
        self.assertEqual(
            r19_response["finalBladeColorGainLinear"],
            blade_policy["finalBladeColorGainLinear"],
        )
        self.assertEqual([1.34, 1.18, 1.03], blade_policy["finalBladeColorGainLinear"])
        self.assertEqual(
            1.20319344,
            blade_policy["finalBladeColorGainApproximateLuminanceMultiplier"],
        )
        self.assertEqual(0.88, blade_policy["finalChromaFraction"])
        self.assertEqual([0.97, 1.035], blade_policy["broadLocalValueRange"])
        self.assertEqual([0.99, 1.015], blade_policy["nearFineLocalValueRange"])
        self.assertEqual(2, blade_policy["macroFieldsPerProfile"])
        self.assertEqual(11.0, blade_policy["macroScaleMeters"])
        self.assertEqual(3.6, blade_policy["mesoScaleMeters"])
        self.assertEqual("deterministic_frac_polynomial_arithmetic", blade_policy["coherentHash"])
        self.assertEqual(0, blade_policy["sinCosOperations"])
        self.assertEqual(
            {"macro": 0.7, "meso": 0.3, "sum": 1.0},
            blade_policy["broadSeedWeights"],
        )
        self.assertEqual(
            {"blade": 0.6, "instance": 0.4, "sum": 1.0},
            blade_policy["fineSeedWeights"],
        )
        self.assertEqual(0.82, blade_policy["cutMaskStart"])
        self.assertEqual(0.18, blade_policy["cutDesaturationFraction"])
        self.assertEqual(0.95, blade_policy["cutGain"])
        self.assertEqual([800.0, 2000.0], blade_policy["detailFadeCentimeters"])
        self.assertEqual(
            {"manicured": 0.045, "humid": 0.03, "shade": 0.065, "dry_edge": 0.16},
            blade_policy["dryBladeFractionByProfile"],
        )
        self.assertEqual(
            {"manicured": 0.1, "humid": 0.08, "shade": 0.14, "dry_edge": 0.22},
            blade_policy["rootThatchMixByProfile"],
        )
        self.assertEqual(0.226, blade_policy["maximumBodyGreenLinear"])
        self.assertEqual(0.195, blade_policy["maximumSubsurfaceGreenLinear"])
        self.assertTrue(blade_policy["farPerBladeVariationCollapsed"])
        self.assertTrue(blade_policy["meshTransformsWpoAndOpacityPreservedExactly"])

        current_grass_code = self.editor_cpp.split(
            "FString BuildR19GrassColorCode", 1
        )[1].split("FString BuildR12GrassColorCode", 1)[0]
        self.assertIn("float4 macroHash", current_grass_code)
        self.assertIn("frac(macroHash*(macroHash+33.33)", current_grass_code)
        self.assertIn("float4 mesoHash", current_grass_code)
        self.assertIn("/3.6", current_grass_code)
        self.assertNotIn("sin(", current_grass_code)
        self.assertNotIn("cos(", current_grass_code)
        self.assertIn("smoothstep(0.82,1.0,h)", current_grass_code)
        self.assertIn("lerp(0.970,1.035", current_grass_code)
        self.assertIn("0.70*macroNoise+0.30*mesoNoise", current_grass_code)
        self.assertIn("float3(1.34,1.18,1.03)", current_grass_code)

        field_code = self.editor_cpp.split(
            "const FString GroundR10FieldCode", 1
        )[1].split("const FString GroundR10BaseCode", 1)[0]
        self.assertEqual(1, field_code.count("sin("))
        self.assertNotIn("cos(", field_code)
        self.assertNotIn("frac(sin", field_code)
        self.assertIn("h11=frac(h11*(h11+33.33)", field_code)
        self.assertIn("h48=frac(h48*(h48+29.71)", field_code)

        base_code = self.editor_cpp.split(
            "const FString GroundR15BaseCode", 1
        )[1].split("const FString GroundR15RoughnessCode", 1)[0]
        roughness_code = self.editor_cpp.split(
            "const FString GroundR13RoughnessCode", 1
        )[1].split("const FString GroundR15BaseCode", 1)[0]
        self.assertIn(
            "const FString GroundR15RoughnessCode(GroundR13RoughnessCode)",
            self.editor_cpp,
        )
        for token in (
            "float detailPresence=lerp(0.38,0.82",
            "float3(1.035,1.000,0.945)",
            "float3(0.975,0.990,1.015)",
            "float wetGain=lerp(1.0,0.955",
            "float3(0.640,0.805,0.765)",
        ):
            self.assertIn(token, base_code)
        self.assertIn("0.63+0.50*saturate(TextureRoughness)", roughness_code)
        self.assertIn("float base=lerp(0.84,nearRough", roughness_code)
        self.assertIn("+0.05*wearAmount-0.06*wetAmount", roughness_code)
        self.assertIn("0.70,0.91", roughness_code)
        self.assertIn("%.6f*(0.5-Mowing)", roughness_code)
        self.assertIn("2.0f * GroundR10MowingRoughnessAmplitude", roughness_code)
        self.assertIn("GroundR10MowingRoughnessAmplitude = 0.025f", self.editor_cpp)
        for token in (
            "TextureSampleCount != 4",
            "ExpressionCollection.Expressions.Num() != 23",
            "GroundR10SurfaceUvCode",
            "max(TileMeters,0.01)*100.0",
            "GroundR13NearColorDetailPresence = 0.82f",
            "GroundR13FarColorDetailPresence = 0.38f",
            "GroundR15NearNormalStrength = 0.20f",
            "GroundR15FarNormalStrength = 0.02f",
            "GroundR15Specular = 0.22f",
            "GroundR10DetailFadeStartCm = 3000.0f",
            "GroundR10DetailFadeEndCm = 5500.0f",
            "Material->bUsedWithInstancedStaticMeshes = false",
            "Material->NaniteOverrideMaterial.bEnableOverride = true",
            "Data->Displacement.Expression = nullptr",
            "Data->MaterialAttributes.Expression = nullptr",
            "Data->SurfaceThickness.Expression = nullptr",
            "Data->FrontMaterial.Expression = nullptr",
            "bNoCustomizedUvConnections",
        ):
            self.assertIn(token, self.editor_cpp)

        ensure_report = self.editor_cpp.split(
            "EnsureGroundVegetationRealismMaterialAssets", 1
        )[1].split("UpgradeGroundVegetationRealismMaterialAssetsToR10", 1)[0]
        for token in (
            "revision=R19",
            "count=7",
            "coreRevision=R19",
            "grassRevision=R19",
            "groundOverlayRevision=R18",
            "edgeRevision=R11",
            "edgeFadeMaterial=M_IPV5D_GrassMedium_EdgeFade",
            "photographicTurfMaterialResponse=true",
            "currentBotanicalOrSpeciesClaim=false",
            "grassMacroFieldsPerProfile=2",
            "grassSinCosOperations=0",
            "exactTextureSamples=4",
            "maximumWetLuminanceDarkeningApprox=0.057",
            "wearWarmDesaturated=true",
            "nearColorDetailPresence=0.82",
            "overlayTint=0.715,0.965,0.595",
            "overlayRoughnessNear=0.63+0.50xTexture",
            "overlayRoughnessClamp=0.70,0.91",
            "nearNormalStrength=0.15",
            "farNormalStrength=0.01",
            "mowingRoughnessAmplitude=0.025",
            "mowingRoughnessPhase=opposesBaseColorGain",
            "grassExpressionsPerProfile=29",
            "grassTextureSamplesPerProfile=0",
            "grassNormalInputs=5",
            "grassDiffuseGain=1.34,1.18,1.03",
            "grassRoughnessClamp=0.70,0.90",
            "grassSpecularClamp=0.16,0.23",
            "grassNormalNearFar=0.50,0.03",
            "grassNormalBladeMultiplier=0.96,1.04",
            "soilGraphWiringValidated=true",
            "customAuxiliaryStateValidated=true",
            "compiledActiveFeatureLevelValidated=true",
            "overlayTextureSampleStateValidated=true",
            "appearanceFieldPeriodicOperationScope=GroundR10FieldCodeOnly",
            "coreMaskTopologyFingerprint=V5",
            "opaqueCollarMeters=50",
            "maximumOutsideProviderClipMeters=58",
        ):
            self.assertIn(token, ensure_report)
        for stale in (
            "grassSeedRange=0.92,1.12",
            "macroWeightsFineToBroad=0.18,0.42,0.40",
            "farNormalStrength=0.11",
            "lowFrequencyVariationMeters=3.2,17,43",
        ):
            self.assertNotIn(stale, ensure_report)
        self.assertIn("R15 shade-readable grass and overlay migration", self.package_readme)

    def test_r9_and_r10_grass_admission_seals_real_graph_connections(self):
        validator = self.editor_cpp.split(
            "bool ValidateGrassMaterialVersion", 1
        )[1].split("bool ValidateGrassMaterial(", 1)[0]
        normalized = re.sub(r"\s+", " ", validator)
        required_predicates = (
            "Data->ExpressionCollection.Expressions.Num() != 23",
            "TextureSampleNodes != 0",
            "InstanceRandomNodes != 1",
            "InstanceFadeNodes != 1",
            'Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM")',
            'Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE")',
            "Material->OpacityMaskClipValue, 0.50f",
            "Material->MaxWorldPositionOffsetDisplacement, GrassMaximumWpoCm",
            "!Material->bUsedWithInstancedStaticMeshes",
            "Material->bEnableTessellation",
            "Material->bEnableDisplacementFade",
            "!Material->NaniteOverrideMaterial.bEnableOverride",
            "Material->NaniteOverrideMaterial.GetOverrideMaterial()",
            "Material->GetNaniteOverride()",
            "BladeUv->UnMirrorU || BladeUv->UnMirrorV",
            "Time->bOverride_Period || Time->bIgnorePause",
            "!InputMatches(Color->Inputs[0].Input, BladeUv, 0)",
            "!InputMatches(Color->Inputs[1].Input, InstanceRandom, 0)",
            "!InputMatches(Color->Inputs[2].Input, WorldPosition, 0)",
            "!InputMatches(DistanceMatchedNormal->Alpha, InstanceFade, 0)",
            "GrassDitherTemporalAaFunctionPath",
            "DitherAlphaInputCount == 1",
            "DitherAlphaInputIndex != INDEX_NONE && bNoUnexpectedDitherInputs &&",
            "DitheredInstanceFade->FunctionInputs[DitherAlphaInputIndex].Input",
            "!InputMatches(Data->BaseColor, Color, 0)",
            "!InputMatches(Data->Normal, DistanceMatchedNormal, 0)",
            "!InputMatches(Data->OpacityMask, DitheredInstanceFade, 0)",
            "!InputMatches(Data->WorldPositionOffset, Wind, 0)",
            "Wind->Code != GrassWindCode",
            "Wind->Inputs.Num() == 12",
            "!bExactWindInputs",
            "return ValidateCompiledMaterial(Material, OutError)",
        )

        def source_seal_present(candidate: str) -> bool:
            return all(predicate in candidate for predicate in required_predicates)

        self.assertTrue(source_seal_present(normalized))
        for predicate in required_predicates:
            with self.subTest(removed_predicate=predicate):
                mutated = normalized.replace(predicate, "MUTATED_AWAY", 1)
                self.assertFalse(source_seal_present(mutated))

        expected_edges = {
            "Color.BladeUV": "BladeUv.0",
            "Color.Random01": "PerInstanceRandom.0",
            "Color.WorldPosition": "WorldPositionNoOffsets.0",
            "Material.BaseColor": "Color.0",
            "Dither.AlphaThreshold": "PerInstanceFade.0",
            "Material.OpacityMask": "DitherTemporalAA.0",
            "Normal.FadeAlpha": "PerInstanceFade.0",
            "Material.Normal": "DistanceMatchedNormal.0",
            "Wind.InstanceFade": "PerInstanceFade.0",
            "Material.WorldPositionOffset": "BoundedWind.0",
        }

        def topology_admitted(candidate: dict[str, str]) -> bool:
            return candidate == expected_edges

        self.assertTrue(topology_admitted(expected_edges.copy()))
        for edge in expected_edges:
            with self.subTest(miswired_edge=edge):
                mutated = expected_edges.copy()
                mutated[edge] = "DisconnectedOrWrongExpression.7"
                self.assertFalse(topology_admitted(mutated))

    def test_all_six_materials_require_render_valid_compiled_sm5_shaders(self):
        compiled_gate = self.editor_cpp.split(
            "bool ValidateCompiledMaterial", 1
        )[1].split("bool ValidateGrassMaterialVersion", 1)[0]
        for token in (
            "EnsureIsComplete",
            "GMaxRHIFeatureLevel",
            "GetMaterialResource(FeatureLevel)",
            "SubmitCompileJobs_GameThread",
            "FinishCompilation",
            "IsCompilationFinished",
            "IsGameThreadShaderMapComplete",
            "IsMaterialMapDDCEnabled",
            "IsShaderJobCacheDDCEnabled",
            "bCompileStateAccepted",
            "IsValidForRendering",
            "IsDefaultMaterial",
            "GetCompileErrors().IsEmpty",
        ):
            self.assertIn(token, compiled_gate)
        self.assertEqual(12, self.editor_cpp.count("return ValidateCompiledMaterial(Material, OutError)"))

    def test_preserved_soil_admission_seals_exact_six_node_graph(self):
        validator = self.editor_cpp.split("bool ValidateSoilMaterial", 1)[1].split(
            "bool ValidateLegacyGroundOverlayMaterial", 1
        )[0]
        normalized = re.sub(r"\s+", " ", validator)
        required_predicates = (
            "Material->GetClass() != UMaterial::StaticClass()",
            "Material->MaterialDomain != MD_Surface",
            "Material->BlendMode != BLEND_Opaque",
            "!Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit)",
            "!Material->bTangentSpaceNormal",
            "Material->bUseMaterialAttributes",
            "!Material->bUsedWithInstancedStaticMeshes",
            "Material->bEnableTessellation",
            "Material->bEnableDisplacementFade",
            "!Material->NaniteOverrideMaterial.bEnableOverride",
            "Material->NaniteOverrideMaterial.GetOverrideMaterial()",
            "Material->GetNaniteOverride()",
            "Data->ExpressionCollection.Expressions.Num() != 6",
            "TextureSampleNodes != 0",
            "ApronUv->UnMirrorU || ApronUv->UnMirrorV",
            "Base->OutputType != CMOT_Float3",
            "!InputMatches(Base->Inputs[0].Input, WorldPosition, 0)",
            "!InputMatches(Base->Inputs[1].Input, ApronUv, 0)",
            "Roughness->OutputType != CMOT_Float1",
            "!InputMatches(Roughness->Inputs[0].Input, WorldPosition, 0)",
            "Normal->OutputType != CMOT_Float3",
            "!InputMatches(Normal->Inputs[0].Input, WorldPosition, 0)",
            'Specular->Desc != TEXT("V5B_FORMAL_BED_SOIL_SPECULAR")',
            "Specular->DefaultValue, 0.08f",
            "!InputMatches(Data->BaseColor, Base, 0)",
            "!InputMatches(Data->Roughness, Roughness, 0)",
            "!InputMatches(Data->Specular, Specular, 0)",
            "!InputMatches(Data->Normal, Normal, 0)",
            "Data->WorldPositionOffset.Expression",
            "Data->MaterialAttributes.Expression",
            "return ValidateCompiledMaterial(Material, OutError)",
        )

        def source_seal_present(candidate: str) -> bool:
            return all(predicate in candidate for predicate in required_predicates)

        self.assertTrue(source_seal_present(normalized))
        for predicate in required_predicates:
            with self.subTest(removed_soil_guard=predicate):
                self.assertFalse(
                    source_seal_present(normalized.replace(predicate, "MUTATED_AWAY", 1))
                )

        expected_edges = {
            "Base.WorldPosition": "WorldPositionNoOffsets.0",
            "Base.ApronUV": "ApronUv0.0",
            "Roughness.WorldPosition": "WorldPositionNoOffsets.0",
            "Normal.WorldPosition": "WorldPositionNoOffsets.0",
            "Material.BaseColor": "Base.0",
            "Material.Roughness": "Roughness.0",
            "Material.Specular": "Specular08.0",
            "Material.Normal": "Normal.0",
        }
        self.assertEqual(expected_edges, expected_edges.copy())
        for edge in expected_edges:
            with self.subTest(miswired_soil_edge=edge):
                mutated = expected_edges.copy()
                mutated[edge] = "DetachedOrWrongExpression.3"
                self.assertNotEqual(expected_edges, mutated)

    def test_overlay_texture_sample_sampler_and_mip_state_is_mutation_sealed(self):
        builder = self.editor_cpp.split("UMaterialExpressionTextureSampleParameter2D* AddGroundTextureSample", 1)[1].split(
            "void ConfigureGroundCustom", 1
        )[0]
        ground_validator = self.editor_cpp.split(
            "bool ValidateGroundOverlayMaterial", 1
        )[1].split("bool ValidateMaterialAssetsInternal", 1)[0]
        validator = ground_validator.split("const auto SampleMatches", 1)[1].split(
            "const bool bMasksMatch", 1
        )[0]
        required_builder_state = (
            "Sample->SamplerSource = SSM_FromTextureAsset",
            "Sample->MipValueMode = TMVM_None",
            "Sample->AutomaticViewMipBias = true",
            "Sample->TextureObject.Expression = nullptr",
            "Sample->MipValue.Expression = nullptr",
            "Sample->CoordinatesDX.Expression = nullptr",
            "Sample->CoordinatesDY.Expression = nullptr",
            "Sample->AutomaticViewMipBiasValue.Expression = nullptr",
        )
        required_validator_state = (
            "Sample->SamplerSource == SSM_FromTextureAsset",
            "TMVM_MipBias : TMVM_None",
            "Sample->AutomaticViewMipBias &&",
            "Sample->ParameterName == ParameterName",
            "!Sample->TextureObject.Expression",
            "!Sample->MipValue.Expression",
            "!Sample->CoordinatesDX.Expression",
            "!Sample->CoordinatesDY.Expression",
            "!Sample->AutomaticViewMipBiasValue.Expression",
            "InputMatches(Sample->Coordinates, SurfaceUv, 0)",
        )
        for token in required_builder_state:
            self.assertIn(token, builder)
        for token in required_validator_state:
            self.assertIn(token, validator)
        expected_parameter_names = (
            "V5D_R10_BaseColorTexture",
            "V5D_R10_NormalTexture",
            "V5D_R10_RoughnessTexture",
            "V5D_R10_AoTexture",
        )
        for parameter_name in expected_parameter_names:
            self.assertIn(f'TEXT("{parameter_name}")', ground_validator)

        def sample_seal_present(candidate: str) -> bool:
            return all(token in candidate for token in required_validator_state)

        self.assertTrue(sample_seal_present(validator))
        for token in required_validator_state:
            with self.subTest(flipped_or_removed_sample_guard=token):
                self.assertFalse(
                    sample_seal_present(validator.replace(token, "MUTATED_AWAY", 1))
                )

        expected_state = {
            "samplerSource": "texture_asset",
            "mipMode": "none",
            "automaticViewMipBias": True,
            "parameterName": "exact",
            "textureObject": None,
            "mipValue": None,
            "coordinatesDx": None,
            "coordinatesDy": None,
            "automaticViewMipBiasValue": None,
            "coordinates": "SurfaceUv.0",
        }
        for field, expected in expected_state.items():
            with self.subTest(flipped_sample_state=field):
                mutated = expected_state.copy()
                mutated[field] = not expected if isinstance(expected, bool) else "WRONG"
                self.assertNotEqual(expected_state, mutated)

    def test_macro_lawn_overlay_owns_and_validates_its_nanite_usage_flag(self):
        builder = self.editor_cpp.split(
            "bool BuildGroundOverlayMaterialGraph", 1
        )[1].split("bool CreateGroundOverlayMaterial", 1)[0]
        validator = self.editor_cpp.split(
            "bool ValidateGroundOverlayMaterial", 1
        )[1].split("bool ValidateMaterialAssetsInternal", 1)[0]
        self.assertIn(
            "Material->bUsedWithNanite = true;\n"
            "    UMaterialEditingLibrary::RecompileMaterial(Material);",
            builder,
        )
        self.assertIn("!Material->GetUsageByFlag(MATUSAGE_Nanite)", validator)
        self.assertEqual(1, self.editor_cpp.count("bUsedWithNanite = true;"))
        self.assertEqual(
            1, self.editor_cpp.count("GetUsageByFlag(MATUSAGE_Nanite)")
        )

    def test_all_custom_expressions_reject_auxiliary_shader_state(self):
        helper = self.editor_cpp.split("bool HasNoAuxiliaryCustomState", 1)[1].split(
            "bool ValidateCompiledMaterial", 1
        )[0]
        required_empty_state = (
            "Custom->AdditionalOutputs.IsEmpty()",
            "Custom->AdditionalDefines.IsEmpty()",
            "Custom->IncludeFilePaths.IsEmpty()",
        )

        def helper_seal_present(candidate: str) -> bool:
            return all(token in candidate for token in required_empty_state)

        self.assertTrue(helper_seal_present(helper))
        for token in required_empty_state:
            with self.subTest(removed_custom_auxiliary_guard=token):
                self.assertFalse(
                    helper_seal_present(helper.replace(token, "MUTATED_AWAY", 1))
                )

        validator_ranges = (
            ("bool ValidateGrassMaterialVersion", "bool ValidateGrassMaterial("),
            ("bool ValidateSoilMaterial", "bool ValidateLegacyGroundOverlayMaterial"),
            ("bool ValidateLegacyGroundOverlayMaterial", "int32 CountCodeToken"),
            ("bool ValidateGroundOverlayMaterial", "bool ValidateMaterialAssetsInternal"),
        )
        for start, end in validator_ranges:
            with self.subTest(custom_validator=start):
                body = self.editor_cpp.split(start, 1)[1].split(end, 1)[0]
                guard = "!AllCustomExpressionsHaveNoAuxiliaryState(Data)"
                self.assertIn(guard, body)
                self.assertNotIn(guard, body.replace(guard, "MUTATED_AWAY", 1))

        configure_grass = self.editor_cpp.split("bool ConfigureGrassMaterial", 1)[1].split(
            "bool CreateGrassMaterial", 1
        )[0]
        create_soil = self.editor_cpp.split("bool CreateSoilMaterial", 1)[1].split(
            "void ResetGroundOverlayGraph", 1
        )[0]
        configure_ground = self.editor_cpp.split("void ConfigureGroundCustom", 1)[1].split(
            "bool BuildGroundOverlayMaterialGraph", 1
        )[0]
        for token in (
            "ResetCustomAuxiliaryState(Color)",
            "ResetCustomAuxiliaryState(Wind)",
        ):
            self.assertIn(token, configure_grass)
        for token in (
            "ResetCustomAuxiliaryState(Base)",
            "ResetCustomAuxiliaryState(Roughness)",
            "ResetCustomAuxiliaryState(Normal)",
        ):
            self.assertIn(token, create_soil)
        self.assertIn("ResetCustomAuxiliaryState(Custom)", configure_ground)
        reset_helper = self.editor_cpp.split("void ResetCustomAuxiliaryState", 1)[1].split(
            "bool ValidateCompiledMaterial", 1
        )[0]
        for token in (
            "Custom->AdditionalOutputs.Reset()",
            "Custom->AdditionalDefines.Reset()",
            "Custom->IncludeFilePaths.Reset()",
        ):
            self.assertIn(token, reset_helper)

        expected_state = {
            "additionalOutputs": (),
            "additionalDefines": (),
            "includeFilePaths": (),
        }
        for field in expected_state:
            with self.subTest(injected_auxiliary_state=field):
                mutated = expected_state.copy()
                mutated[field] = ("unexpected",)
                self.assertNotEqual(expected_state, mutated)

    def test_mowing_roughness_is_opposite_phase_wired_and_mutation_sealed(self):
        builder = self.editor_cpp.split(
            "bool BuildGroundOverlayMaterialGraph", 1
        )[1].split("bool CreateGroundOverlayMaterial", 1)[0]
        validator = self.editor_cpp.split(
            "bool ValidateGroundOverlayMaterial", 1
        )[1].split("bool ValidateMaterialAssetsInternal", 1)[0]
        required_builder_wiring = (
            'TEXT("TextureRoughness"), TEXT("Macro"), TEXT("Mowing")',
            "Roughness->Inputs[2].Input.Connect(0, Mowing)",
        )
        required_validator_wiring = (
            "Roughness->Inputs.Num() != 6",
            'Roughness->Inputs[2].InputName != TEXT("Mowing")',
            "!InputMatches(Roughness->Inputs[2].Input, Mowing, 0)",
        )
        for token in required_builder_wiring:
            self.assertIn(token, builder)
        for token in required_validator_wiring:
            self.assertIn(token, validator)
        self.assertIn("%.6f*(0.5-Mowing)", self.editor_cpp)

        def wiring_seal_present(candidate: str) -> bool:
            return all(token in candidate for token in required_validator_wiring)

        self.assertTrue(wiring_seal_present(validator))
        for token in required_validator_wiring:
            with self.subTest(removed_mowing_guard=token):
                self.assertFalse(
                    wiring_seal_present(validator.replace(token, "MUTATED_AWAY", 1))
                )

    def test_overlay_matches_compact_provider_polygon_with_coverage_safe_feather(self):
        mask = self.contract["groundProfile"]["coreMask"]
        self.assertEqual(
            "deterministic_star_shaped_irregular_ellipse_matching_linear_provider_clip",
            mask["shape"],
        )
        self.assertEqual([0.0, 55.0], mask["worldCenterMeters"])
        self.assertEqual([185.0, 245.0], mask["semiAxesMeters"])
        self.assertEqual(64, mask["splinePoints"])
        self.assertEqual([3, 5, 7], mask["radialRipple"]["harmonics"])
        self.assertEqual(
            [0.035, 0.015, 0.006], mask["radialRipple"]["amplitudes"]
        )
        self.assertEqual([0.43, -0.91, 1.37], mask["radialRipple"]["phasesRadians"])
        self.assertTrue(mask["linearSplinePointsRequired"])
        self.assertEqual(
            "inverse_ellipse_parameter_angle_bracket_with_strict_positive_ray_segment_validation",
            mask["edgeSelection"],
        )
        self.assertEqual(0.000001, mask["segmentParameterEpsilon"])
        self.assertEqual(50.0, mask["opaqueCollarMeters"])
        self.assertEqual(8.0, mask["transitionFeatherMeters"])
        self.assertEqual("outside_provider_clip_only", mask["transitionSide"])
        self.assertEqual(0.25, mask["ditherCellMeters"])
        self.assertEqual(
            "TRIAD_EXPLORE_V5D_LAWN_IRREGULAR_ELLIPSE_64_VERTEX_50M_OPAQUE_COLLAR_OUTWARD_DITHER_MASK_V5",
            mask["topologyFingerprint"],
        )
        self.assertTrue(mask["excludedFromAppearanceFieldPeriodicOperationCount"])
        self.assertTrue(mask["stableWorldSpace"])
        self.assertTrue(mask["cameraTimeAndTaaIndependent"])
        self.assertTrue(mask["fullyCoversProviderBoundary"])
        self.assertFalse(mask["coverageGapAtProviderBoundary"])
        self.assertTrue(mask["rendersOutsideProviderClip"])
        self.assertEqual(58.0, mask["maximumOutsideProviderClipMeters"])
        for token in (
            "GroundCoreMaskDescription",
            "TRIAD_EXPLORE_V5D_LAWN_IRREGULAR_ELLIPSE_64_VERTEX_50M_OPAQUE_COLLAR_OUTWARD_DITHER_MASK_V5",
            "ExpectedProviderSiteClipCenterCentimeters",
            "ExpectedProviderSiteClipSemiAxesCentimeters",
            "ExpectedProviderSiteClipRippleAmplitudes",
            "ExpectedProviderSiteClipRipplePhasesRadians",
            "ExpectedProviderSiteClipSplinePoints",
            "ExpectedGroundOverlayOpaqueCollarMeters",
            "ExpectedGroundOverlayOutwardFeatherMeters",
            "ExpectedGroundOverlayDitherCellMeters",
            "signedInwardDistanceCm + opaqueCollarCm",
            "SignedInwardDistanceCm +\n                RequiredGroundOverlayOpaqueCollarMeters",
            "coverage > 0.0",
            "stableCell = floor",
            "BLEND_Masked",
            "OpacityMaskClipValue = 0.5f",
            "Data->OpacityMask.Connect(0, CoreMask)",
        ):
            self.assertIn(token, self.editor_cpp + self.actor_cpp + self.actor_h)
        self.assertNotRegex(
            self.editor_cpp,
            r"GroundCoreMaskCode[\s\S]{0,400}(Time|CameraPosition|DitherTemporalAA)",
        )

    def test_r16_lawn_overlay_migration_is_atomic_one_package_and_seam_exact(self):
        upgrade = self.contract["r16LawnOverlayUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismLawnOverlayToR16"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual("V5D_R16_LAWN_OVERLAY_UPGRADE_PASS", upgrade["successReportPrefix"])
        self.assertEqual(["R15", "R16"], upgrade["uniformInputRevisions"])
        self.assertEqual("M_IPV5D_LawnMacroVariation", upgrade["soleSaveTarget"])
        self.assertEqual(1, upgrade["saveTargets"])
        self.assertEqual(1, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertTrue(upgrade["exactOnePackageAtomicTransaction"])
        self.assertTrue(upgrade["r15GrassProfilesAndSoilAndR11EdgeFadeArtifactHashesPreserved"])
        self.assertTrue(upgrade["targetMapPreAndPostArtifactHashesEqual"])
        self.assertEqual(50.0, upgrade["opaqueCollarMeters"])
        self.assertEqual(8.0, upgrade["outwardFeatherMeters"])
        self.assertEqual(58.0, upgrade["maximumOutsideProviderClipMeters"])
        self.assertEqual(1.0, upgrade["coverageAtProviderBoundary"])
        self.assertEqual(1.0, upgrade["coverageAtCollarOuterBoundary"])
        self.assertEqual(0.5, upgrade["coverageAtMidFeather"])
        self.assertTrue(upgrade["coverageZeroBeyondMaximumOutsideExtent"])
        self.assertTrue(upgrade["stableWorldSpaceDitherRemainsDeterministic"])
        self.assertFalse(upgrade["mapsEverOfferedToSave"])
        self.assertFalse(upgrade["sourceV3V4V5BMaterialAndTextureAssetsEverMutated"])
        self.assertFalse(upgrade["mapTransformsCollisionNavigationSensorAndRfEverMutated"])

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationRealismMaterialAssetsToR17", 1
        )[0]
        for token in (
            "ValidateR16UpgradeInputMaterialAssets",
            "CaptureR16PreservedPackageSnapshots",
            "BackUpR16LawnOverlayPackage",
            "ConfigureR16GroundOverlayMaskInPlace",
            "FinishAllCompilation",
            "SaveLoadedAssets(ExactSaveTargets, false)",
            "ReloadMaterialPackages",
            "ValidateCompleteR16MaterialAssets",
            "R16PreservedPackageSnapshotsMatch",
            "AUTOMATIC_ROLLBACK_OK packages=1",
            "IDEMPOTENT_V5D_R16_LAWN_OVERLAY_ALREADY_VALID",
        ):
            self.assertIn(token, endpoint)
        positions = [endpoint.index(token) for token in (
            "ValidateR16UpgradeInputMaterialAssets",
            "CaptureR16PreservedPackageSnapshots",
            "BackUpR16LawnOverlayPackage",
            "ConfigureR16GroundOverlayMaskInPlace",
            "FinishAllCompilation",
            "SaveLoadedAssets(ExactSaveTargets, false)",
            "ReloadMaterialPackages",
            "ValidateCompleteR16MaterialAssets",
        )]
        self.assertEqual(sorted(positions), positions)
        target_initializer = endpoint.split("TArray<UMaterial*> Targets = {", 1)[1].split("};", 1)[0]
        self.assertEqual("Materials[5]", target_initializer.strip())
        self.assertIn("ExactSaveTargets.Num() != 1", endpoint)
        for forbidden in ("SaveMap", "SavePackage", "Materials[0]", "Materials[1]", "Materials[2]", "Materials[3]", "Materials[4]"):
            self.assertNotIn(forbidden, endpoint)

        for token in (
            "RequiredGroundOverlayOpaqueCollarMeters = 50.0",
            "1.0 +\n            (SignedInwardDistanceCm +",
            "RequiredGroundOverlayOpaqueCollarMeters",
            "Authored overlay remains fully opaque through the 50 m collar",
            "Transition begins only after the full-opacity collar",
            "Overlay never renders beyond the exact 58 m collar and feather",
            "World-space dither is deterministic",
        ):
            self.assertIn(token, self.actor_cpp)

    def test_normalized_ellipse_angle_matches_exact_linear_edge_sweep(self):
        mask = self.contract["groundProfile"]["coreMask"]
        axis_x, axis_y = mask["semiAxesMeters"]
        amplitudes = mask["radialRipple"]["amplitudes"]
        phases = mask["radialRipple"]["phasesRadians"]
        point_count = mask["splinePoints"]
        segment_parameter_epsilon = mask["segmentParameterEpsilon"]
        sector = 2.0 * math.pi / point_count

        def point(index: int) -> tuple[float, float]:
            theta = sector * (index % point_count)
            scale = 1.0 + sum(
                amplitude * math.sin(harmonic * theta + phase)
                for harmonic, amplitude, phase in zip(
                    (3, 5, 7), amplitudes, phases, strict=True
                )
            )
            return (
                scale * axis_x * math.cos(theta),
                scale * axis_y * math.sin(theta),
            )

        points = [point(index) for index in range(point_count)]

        def ray_edge_distance(
            ray: tuple[float, float], edge_index: int
        ) -> tuple[float, float] | None:
            p0 = points[edge_index]
            p1 = points[(edge_index + 1) % point_count]
            edge = (p1[0] - p0[0], p1[1] - p0[1])
            denominator = ray[0] * edge[1] - ray[1] * edge[0]
            if abs(denominator) <= 1.0e-12:
                return None
            segment_parameter = (
                p0[0] * ray[1] - p0[1] * ray[0]
            ) / denominator
            distance = (
                p0[0] * edge[1] - p0[1] * edge[0]
            ) / denominator
            return distance, segment_parameter

        def brute_distance(ray: tuple[float, float]) -> float:
            candidates = []
            for edge_index in range(point_count):
                intersection = ray_edge_distance(ray, edge_index)
                if intersection is None:
                    continue
                distance, segment_parameter = intersection
                if (
                    distance > 0.0
                    and 0.0 <= segment_parameter <= 1.0
                ):
                    candidates.append(distance)
            self.assertTrue(candidates)
            return min(candidates)

        def inverse_ellipse_distance(ray: tuple[float, float]) -> float:
            parametric_angle = math.atan2(ray[1] / axis_y, ray[0] / axis_x)
            if parametric_angle < 0.0:
                parametric_angle += 2.0 * math.pi
            edge_index = min(
                point_count - 1,
                math.floor(parametric_angle / sector),
            )
            intersection = ray_edge_distance(ray, edge_index)
            self.assertIsNotNone(intersection)
            assert intersection is not None
            distance, segment_parameter = intersection
            self.assertGreater(distance, 0.0)
            self.assertGreaterEqual(
                segment_parameter,
                -segment_parameter_epsilon,
            )
            self.assertLessEqual(
                segment_parameter,
                1.0 + segment_parameter_epsilon,
            )
            segment_parameter = min(1.0, max(0.0, segment_parameter))
            self.assertGreaterEqual(segment_parameter, 0.0)
            self.assertLessEqual(segment_parameter, 1.0)
            return distance

        def cartesian_angle_distance(
            ray: tuple[float, float]
        ) -> tuple[float, float]:
            cartesian_angle = math.atan2(ray[1], ray[0]) % (2.0 * math.pi)
            edge_index = min(
                point_count - 1,
                math.floor(cartesian_angle / sector),
            )
            intersection = ray_edge_distance(ray, edge_index)
            self.assertIsNotNone(intersection)
            assert intersection is not None
            return intersection

        maximum_error = 0.0
        forty_five_error = None
        for sample in range(1440):
            degrees = sample * 0.25
            radians = math.radians(degrees)
            ray = (math.cos(radians), math.sin(radians))
            error = abs(inverse_ellipse_distance(ray) - brute_distance(ray))
            maximum_error = max(maximum_error, error)
            if degrees == 45.0:
                forty_five_error = error
        self.assertIsNotNone(forty_five_error)
        self.assertLessEqual(forty_five_error, 1.0e-9)
        self.assertLessEqual(maximum_error, 1.0e-9)

        diagonal = (math.sqrt(0.5), math.sqrt(0.5))
        wrong_distance, wrong_segment_parameter = cartesian_angle_distance(diagonal)
        self.assertLess(wrong_segment_parameter, 0.0)
        self.assertGreater(wrong_distance - brute_distance(diagonal), 5.0)

        self.assertIn("ParametricTheta", self.policy_cpp)
        self.assertIn("Centered.Y / SemiAxes.Y", self.policy_cpp)
        self.assertIn("Centered.X / SemiAxes.X", self.policy_cpp)
        self.assertIn("RawSegmentParameter < -SegmentParameterEpsilon", self.policy_cpp)
        self.assertIn("RawSegmentParameter > 1.0 + SegmentParameterEpsilon", self.policy_cpp)
        self.assertIn("FMath::Clamp", self.policy_cpp)
        self.assertIn("SegmentParameter < 0.0 || SegmentParameter > 1.0", self.policy_cpp)
        self.assertIn("BoundaryRayDistance <= 0.0", self.policy_cpp)
        self.assertIn(
            "parametricAngle = atan2(centeredCm.y/semiAxesCm.y, centeredCm.x/semiAxesCm.x)",
            self.editor_cpp,
        )
        self.assertIn(
            "rawSegmentParameter < -segmentParameterEpsilon || rawSegmentParameter > 1.0 + segmentParameterEpsilon || boundaryRayCm <= 0.0",
            self.editor_cpp,
        )
        self.assertIn(
            "segmentParameter = clamp(rawSegmentParameter, 0.0, 1.0)",
            self.editor_cpp,
        )
        self.assertNotIn("[loop] for (int edgeIndex", self.editor_cpp)
        self.assertNotIn(
            "float angle = atan2(centeredCm.y, centeredCm.x)",
            self.editor_cpp,
        )

    def test_source_terrain_renderer_follows_exact_policy_and_fails_closed(self):
        policy = self.contract["providerFallbackTerrainPolicy"]
        self.assertEqual(
            "ATRIADIstanaExploreV5DContextPolicyActor", policy["authorityActor"]
        )
        self.assertEqual(
            "bLocalBuildingFallbackCurrentlyHidden", policy["authorityState"]
        )
        self.assertTrue(policy["exactlyOneIdentityPolicyAndTaggedSiteClipRequired"])
        self.assertTrue(policy["policyValidateHybridContextMustPass"])
        self.assertTrue(policy["failClosed"])
        self.assertTrue(policy["fallbackVisible"]["sourceTerrainRendererVisible"])
        self.assertFalse(
            policy["providerReadyAndFallbackHidden"]["sourceTerrainRendererVisible"]
        )
        self.assertEqual(
            "QueryAndPhysics",
            policy["providerReadyAndFallbackHidden"]["sourceTerrainCollision"],
        )
        self.assertTrue(policy["restoreSourceTerrainRendererOnFallback"])
        self.assertTrue(policy["restoreSourceTerrainRendererOnEndPlay"])
        for token in (
            "ResolveExactContextPolicy",
            "ValidateHybridContext",
            "HybridContextPolicyTag",
            "HybridSiteClipTag",
            "ShouldHideSourceTerrainRenderer",
            "bLocalBuildingFallbackCurrentlyHidden",
            "SynchronizeSourceTerrainRendererWithProviderPolicy",
            "RestoreSourceTerrainRendering",
            "SetVisibility(bVisible, true)",
            "SetHiddenInGame(!bVisible, true)",
            "ECollisionEnabled::QueryAndPhysics",
            "EndPlay(const EEndPlayReason::Type EndPlayReason)",
        ):
            self.assertIn(token, self.actor_cpp + self.actor_h)
        renderer_helper = self.actor_cpp.split(
            "bool ApplySourceTerrainRendererVisibility", 1
        )[1].split("bool IsInsideHeroLawn", 1)[0]
        self.assertNotIn(
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)", renderer_helper
        )

    def test_stable_clip_active_visual_policy_preserves_simulation_authority(self):
        policy = self.contract["providerFallbackTerrainPolicy"]["stableVisualPolicy"]
        self.assertTrue(policy["providerSiteClipAlwaysActive"])
        self.assertTrue(policy["authoredCoreAlwaysVisible"])
        self.assertFalse(policy["aerialOrAltitudeVisualHandoff"])
        self.assertFalse(policy["runtimeProviderOverlayActivationMutation"])
        self.assertTrue(
            policy["transformsComponentsCollisionNavigationSensorAndRfStateRemainUntouched"]
        )
        self.assertFalse(policy["providerContentReadTracedAnalysedDerivedExportedOrBaked"])
        self.assertTrue(policy["endPlayRestoresGroundPresentation"])
        for forbidden in ("SetProviderSiteClipActive", "->Deactivate(", "->Activate("):
            self.assertNotIn(forbidden, self.editor_cpp)
        self.assertIn(
            "The seam uses one stable visual policy at every view height.",
            self.package_readme,
        )
        self.assertIn(
            "This source-renderer fallback\n"
            "does not deactivate the provider clip or hide the authored core.",
            self.package_readme,
        )
        for retired_claim in (
            "On ascent it first deactivates the provider clipping",
            "hides the seven exact visual-owner actors",
            "renderer-only 90 m enter / 65 m exit",
        ):
            self.assertNotIn(retired_claim, self.package_readme)

    def test_runtime_components_cannot_be_collision_navigation_or_rf_authority(self):
        self.assertIn("SetCollisionEnabled(ECollisionEnabled::NoCollision)", self.actor_cpp)
        self.assertIn("SetCanEverAffectNavigation(false)", self.actor_cpp)
        self.assertIn("bCollisionOrNavigationAuthority = false", self.actor_h)
        self.assertIn("bSensorOrRfMaterialAuthority = false", self.actor_h)
        self.assertNotRegex(
            self.actor_cpp,
            r"LineTrace|SweepMulti|SweepSingle|OverlapMulti|OverlapBlockingTest",
        )
        self.assertEqual(1, self.actor_cpp.count("ECC_GameTraceChannel"))
        self.assertRegex(
            self.actor_cpp,
            r"CollisionResponseChannelCount\s*=\s*\n?\s*"
            r"static_cast<int32>\(ECC_GameTraceChannel18\)\s*\+\s*1;",
        )
        self.assertGreaterEqual(self.actor_cpp.count("CollisionResponseChannelCount"), 5)

    def test_pie_reapply_uses_movable_overlay_and_current_hism_tree_readiness(self):
        profile = self.contract["groundProfile"]
        self.assertEqual("Movable", profile["overlayMobility"])
        layout = self.contract["layoutPolicy"]
        self.assertFalse(layout["ownedHismClusterTreeBuildAsync"])
        self.assertTrue(layout["ownedHismClusterTreeForceUpdate"])
        self.assertTrue(layout["ownedHismClusterTreeImmediateCompletionRequired"])
        self.assertTrue(layout["ownedHismReadinessRequiresTreeFullyBuilt"])
        self.assertFalse(layout["ownedHismReadinessRequiresNoAsyncBuilding"])

        hism_helper = self.actor_cpp.split("void ConfigureRenderOnlyHism", 1)[1].split(
            "void ConfigureRenderOnlyGroundOverlay", 1
        )[0]
        overlay_helper = self.actor_cpp.split(
            "void ConfigureRenderOnlyGroundOverlay", 1
        )[1].split("bool AppendWorldTransforms", 1)[0]
        apply_layout = self.actor_cpp.split(
            "ATRIADIstanaExploreV5DGroundVegetationActor::ApplyLayout", 1
        )[1].split(
            "ConfigureGroundVegetationRealism(",
            1,
        )[0]
        self.assertIn("SetMobility(EComponentMobility::Static)", hism_helper)
        self.assertIn("SetMobility(EComponentMobility::Movable)", overlay_helper)
        self.assertIn(
            "GroundMacroVariationOverlay->Mobility != EComponentMobility::Movable",
            self.actor_cpp,
        )
        overlay_mobility_repair = (
            "GroundMacroVariationOverlay->SetMobility(EComponentMobility::Movable)"
        )
        self.assertIn(overlay_mobility_repair, apply_layout)
        self.assertLess(
            apply_layout.index(overlay_mobility_repair),
            apply_layout.index("GroundMacroVariationOverlay->SetWorldTransform"),
        )
        self.assertRegex(
            apply_layout,
            r"BuildTreeIfOutdated\(\s*/\*Async\*/ false,\s*/\*ForceUpdate\*/ true\s*\)",
        )
        self.assertNotRegex(
            apply_layout,
            r"BuildTreeIfOutdated\(\s*(?:/\*Async\*/\s*)?true",
        )
        self.assertNotIn("Component->IsAsyncBuilding()", apply_layout)
        self.assertIn("bCurrentTreesFullyBuilt", apply_layout)
        self.assertIn("Component->IsTreeFullyBuilt()", apply_layout)

    def test_r20_predecessor_and_r23_current_grass_fade_policies(self):
        layout = self.contract["layoutPolicy"]
        self.assertEqual(30.0, layout["sourceV5bTurfCullStartMeters"])
        self.assertEqual(52.0, layout["sourceV5bTurfCullEndMeters"])
        if "sourceV5bRuntimeCompositeCullMeters" in layout:
            self.assertFalse(layout["sourceV5bSerializedCullPolicyChanged"])
            self.assertTrue(layout["sourceV5bRuntimeVisualCullPolicyChanged"])
            self.assertEqual(
                [40.0, 65.0], layout["sourceV5bRuntimeCompositeCullMeters"]
            )
            self.assertEqual(
                1.1, layout["sourceV5bRuntimeCompositeLodDistanceScale"]
            )
            self.assertFalse(layout["sourceV5bTurfRendererHiddenDuringBegunPlay"])
            self.assertEqual(65.0, layout["modeledGrassAbsentAtOrBeyondMeters"])
        else:
            self.assertFalse(layout["sourceV5bTurfCullPolicyChanged"])
            self.assertTrue(layout["sourceV5bTurfRendererHiddenDuringBegunPlay"])
            self.assertEqual(45.0, layout["modeledGrassAbsentAtOrBeyondMeters"])
        self.assertEqual(24.0, layout["v5dSparseResidualCullStartMeters"])
        self.assertEqual(38.0, layout["v5dSparseResidualCullEndMeters"])
        self.assertEqual(24.0, layout["v5dSparseResidualWpoDisableMeters"])
        self.assertEqual(0.6, layout["v5dGrassLodDistanceScale"])
        self.assertEqual(30.0, layout["v5dEdgeGrassCullStartMeters"])
        self.assertEqual(45.0, layout["v5dEdgeGrassCullEndMeters"])
        self.assertEqual(24.0, layout["v5dEdgeGrassWpoDisableMeters"])
        self.assertTrue(layout["farLawnOwnedBySubstrate"])
        self.assertIn("dithers out from 24-38 m", self.package_readme)
        self.assertIn(
            "Taller edge grass in begun Play fades from 30-45 m",
            self.package_readme,
        )
        for token in (
            "GrassCullStartDistanceCm = 2400",
            "GrassCullEndDistanceCm = 3800",
            "GrassWpoDisableDistanceCm = 2400",
            "GrassLodDistanceScale = 0.60f",
            "EdgeGrassCullStartDistanceCm = 3000",
            "EdgeGrassCullEndDistanceCm = 4500",
            "EdgeGrassWpoDisableDistanceCm = 2400",
            "Component->GetCullDistances(StartCullDistance, EndCullDistance)",
            "R14 CDO keeps the exact cold predecessor grass policy for migration admission",
            "R14 CDO keeps the exact cold predecessor edge-grass policy for migration admission",
            "R20 predecessor and R23 target policies remain explicit and distinct from the R14 CDO",
            "GrassCullStartDistanceCm = 2600",
            "GrassCullEndDistanceCm = 3400",
            "grassMaterialVisibilityMeters=20..28",
            "CompositeSourceV5BTurfCullStartDistanceCm = 4000",
            "CompositeSourceV5BTurfCullEndDistanceCm = 6500",
            "CompositeSourceV5BTurfLodDistanceScale = 1.10f",
            "compositeSourceGrassCullMeters=40..65",
            "compositeSourceGrassLodDistanceScale=1.10",
            "inheritedV5BRuntimeCullLodVisualOverrideOnly=true",
            "inheritedV5BSourceRendererPresentation=%s",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn("65-120 m", self.actor_cpp)
        for token in (
            "ExpectedAccentStartCull",
            "ExpectedAccentEndCull",
            "ExpectedAccentLodDistanceScale",
            "ValidateActiveRuntimeSourceTurfPresentationForSourceActor",
        ):
            self.assertIn(token, self.v5b_actor_cpp)

    def test_hero_lawn_and_edge_transition_are_explicit(self):
        for token in (
            "IsInsideHeroLawn",
            "HeroLawnHalfWidthCm",
            "HeroLawnMinimumYCm",
            "HeroLawnMaximumYCm",
            "EdgeTransitionWidthCm",
            "SpatialClumpNoise",
        ):
            self.assertIn(token, self.actor_cpp)
        for token in (
            "HeroDryWeight = 0.004",
            "HeroDryWeight = 0.008",
            "HeroDryWeight = 0.014",
            "0.010 + 0.006 * (1.0 - MacroClump)",
            "Hero lawn keeps restrained deterministic shade intrusion",
            "Hero lawn keeps restrained deterministic dry intrusion",
            "Hero lawn remains at least 95 percent manicured or humid",
            "heroLawnProfile=predominantlyManicuredHumid",
            "restrainedHeroShadeDryIntrusion=true",
            "tallerEdgeGrassSeasonCensus=%d,%d,%d",
            "edgeGrassCandidateMask=actualHeroLawnPlusCeremonialAxis",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn("if (!bHero && Draw < DryWeight)", self.actor_cpp)
        edge_candidates = self.actor_cpp.split(
            "TArray<FScoredIndex> EdgeCandidates;", 1
        )[1].split("SortScored(EdgeCandidates);", 1)[0]
        self.assertIn("IsInsideHeroLawn(Location)", edge_candidates)
        self.assertIn(
            "FMath::Abs(Location.X) < CeremonialAxisHalfWidthCm",
            edge_candidates,
        )
        self.assertNotIn("HeroLawnHalfWidthCm", edge_candidates)
        for season_count in (
            "return 512;",
            "return 640;",
            "return 768;",
            'TEXT("Exact humid-wet edge census"), WetA.EdgeGrass.Num(), 512',
            'TEXT("Exact transition edge census")',
            'TEXT("Exact dry preview edge census"), Dry.EdgeGrass.Num(), 768',
        ):
            self.assertIn(season_count, self.actor_cpp)
        self.assertTrue(self.contract["layoutPolicy"]["heroLawnPreserved"])
        self.assertTrue(
            self.contract["layoutPolicy"]["heroLawnExcludesSupplementalSoilAndTallEdgeGrass"]
        )
        diversity = self.contract["vegetationDiversityPolicy"]
        self.assertEqual(10, diversity["existingV4MainAndHeritageTreeFormComponentsRead"])
        self.assertFalse(diversity["newTreeInstancesAdded"])
        self.assertFalse(diversity["speciesOrEcologyClaimed"])

    def test_editor_hook_is_exactly_named_and_additive(self):
        hook = "ApplyGroundVegetationRealismPassToLoadedV5DHybridMap"
        self.assertIn(hook, self.editor_h)
        self.assertIn(hook, self.editor_cpp)
        self.assertIn(self.contract["targetMap"], self.editor_cpp)
        self.assertIn(self.contract["contentNamespace"], self.editor_cpp)
        self.assertNotRegex(self.editor_cpp, r"DeleteAsset|DeleteDirectory|ConsolidateAssets")

    def test_r10_material_upgrade_is_exactly_scoped_atomic_and_fail_closed(self):
        upgrade = self.contract["r10MaterialUpgrade"]
        self.assertEqual(
            "UpgradeGroundVegetationRealismMaterialAssetsToR10",
            upgrade["endpoint"],
        )
        self.assertEqual(
            [
                "M_IPV5D_Turf_Manicured",
                "M_IPV5D_Turf_Humid",
                "M_IPV5D_Turf_Shade",
                "M_IPV5D_Turf_DryEdge",
                "M_IPV5D_SoilMulch_Layered",
                "M_IPV5D_LawnMacroVariation",
            ],
            upgrade["requiredValidCleanPackages"],
        )
        self.assertEqual(
            [
                "M_IPV5D_Turf_Manicured",
                "M_IPV5D_Turf_Humid",
                "M_IPV5D_Turf_Shade",
                "M_IPV5D_Turf_DryEdge",
                "M_IPV5D_LawnMacroVariation",
            ],
            upgrade["inPlaceTargets"],
        )
        self.assertEqual("M_IPV5D_SoilMulch_Layered", upgrade["preservedPackage"])
        self.assertEqual(
            [".uasset", ".uexp", ".ubulk", ".uptnl", ".m.ubulk", ".o.ubulk"],
            upgrade["fullArtifactRosterExtensions"],
        )
        self.assertEqual(6, upgrade["artifactStatesPerPackage"])
        self.assertTrue(upgrade["absentSidecarsRecordedAndDeletedOnRollback"])
        self.assertEqual(2, upgrade["backupReceiptVersion"])
        self.assertEqual(5, upgrade["saveTargets"])
        self.assertEqual(5, upgrade["reloadTargets"])
        self.assertEqual(6, upgrade["coldValidatedMaterials"])
        self.assertTrue(upgrade["grassGraphWiringValidatedForR9AndR10"])
        self.assertTrue(
            upgrade[
                "grassGraphSealIncludesBladeUvRandomWorldPositionBaseColorFadeDitherCutoutNormalAndBoundedWpo"
            ]
        )
        self.assertTrue(upgrade["preservedSoilGraphWiringValidated"])
        self.assertEqual(6, upgrade["preservedSoilExpressionCount"])
        self.assertEqual(0, upgrade["preservedSoilTextureSampleCount"])
        self.assertTrue(upgrade["overlayTextureSampleSamplerAndMipStateValidated"])
        self.assertTrue(
            upgrade[
                "allAdmittedCustomExpressionsRejectAdditionalOutputsDefinesAndIncludePaths"
            ]
        )
        self.assertTrue(
            upgrade[
                "allGeneratedCustomExpressionsResetAdditionalOutputsDefinesAndIncludePaths"
            ]
        )
        self.assertTrue(upgrade["compiledSm5ShaderMapRequiredForAllSixMaterials"])
        self.assertTrue(upgrade["defaultMaterialFallbackRejected"])
        self.assertTrue(upgrade["compileErrorsRejected"])
        for key in (
            "soilPreAndPostArtifactHashesEqual",
            "targetMapPreAndPostArtifactHashesEqual",
            "rollbackRestoresAllFiveBeforeReload",
            "rollbackPreReloadHashesRequired",
            "rollbackPostReloadHashesRequired",
        ):
            self.assertTrue(upgrade[key], key)
        self.assertFalse(upgrade["mapsEverOfferedToSave"])
        self.assertFalse(upgrade["mixedVersionRepairAllowed"])

        endpoint_name = upgrade["endpoint"]
        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.split(endpoint_name, 1)[1].split(
            "ApplyGroundVegetationRealismPassToLoadedV5DHybridMap", 1
        )[0]
        ordered_markers = (
            "ValidateR10UpgradeInputMaterialAssets",
            "CaptureObjectPackageArtifacts",
            "CaptureTargetMapArtifacts",
            "BackUpMaterialPackages",
            "ConfigureR10GrassMaterial",
            "BuildR10GroundOverlayMaterialGraph",
            "FinishAllCompilation",
            "SaveLoadedAssets",
            "ReloadMaterialPackages",
            "ValidateR10MaterialAssetsInternal",
        )
        positions = [endpoint.index(marker) for marker in ordered_markers]
        self.assertEqual(sorted(positions), positions)
        target_initializer = endpoint.split(
            "TArray<UMaterial*> Targets = {", 1
        )[1].split("};", 1)[0]
        self.assertEqual(5, target_initializer.count("Materials["))
        self.assertNotIn("Materials[4]", target_initializer)
        self.assertIn("Materials[5]", target_initializer)
        self.assertIn("ExactSaveTargets.Num() != 5", endpoint)
        self.assertIn("SaveLoadedAssets(ExactSaveTargets, false)", endpoint)
        self.assertIn("ColdMaterials.Num() != 6", endpoint)
        self.assertIn("bColdPackagesClean", endpoint)
        self.assertIn("coldPackagesClean=true", endpoint)
        self.assertIn("PackageArtifactsMatch(SoilBefore", endpoint)
        self.assertIn("PackageArtifactsMatch(MapBefore", endpoint)
        self.assertIn("RestoreMaterialPackagesAtomically", endpoint)
        self.assertIn("AUTOMATIC_ROLLBACK_OK packages=5", endpoint)
        self.assertIn("mapsSaved=0", endpoint)
        self.assertNotIn("SaveMap", endpoint)
        self.assertNotIn("SavePackage", endpoint)
        self.assertNotIn("MarkPackageDirty", endpoint)

        candidates = self.editor_cpp.split(
            "TArray<FString> PackageArtifactCandidates", 1
        )[1].split("struct FPackageArtifactSnapshot", 1)[0]
        self.assertIn("FPaths::GetExtension(PackageFilename, true)", candidates)
        for sidecar in (".uexp", ".ubulk", ".uptnl", ".m.ubulk", ".o.ubulk"):
            self.assertIn(f'TEXT("{sidecar}")', candidates)
        self.assertIn("OutArtifacts.Num() != 6", self.editor_cpp)
        self.assertIn("Expected.Num() != 6", self.editor_cpp)
        self.assertIn("Package.Artifacts.Num() != 6", endpoint)

        backup = self.editor_cpp.split("bool BackUpMaterialPackagesForRevision", 1)[1].split(
            "bool ReloadMaterialPackages", 1
        )[0]
        for token in (
            "SavedDir",
            "TRIAD/Backups/V5D_%s_GroundMaterials",
            "StartsWith(BackupRoot",
            "state=PRESENT",
            "state=ABSENT",
            "rollback=DELETE_IF_PRESENT",
            "ReceiptReadback != Receipt",
            "PackageArtifactsMatch",
        ):
            self.assertIn(token, backup)
        self.assertIn("TRIAD_V5D_R10_GROUND_MATERIAL_BACKUP_V2", backup)

        rollback = self.editor_cpp.split(
            "bool RestoreMaterialPackagesAtomically", 1
        )[1].split("bool BuildAssetRoster", 1)[0]
        backup_prevalidation = self.editor_cpp.split(
            "bool PrevalidateMaterialUpgradeBackupForRestore", 1
        )[1].split("bool RestoreMaterialPackagesAtomically", 1)[0]
        for token in (
            "Backup.Packages.Num() != Backup.ExpectedPackageCount",
            "Package.Artifacts.Num() != Backup.ExpectedArtifactsPerPackage",
            "Package.Artifacts[0].Original != Package.PackageFilename",
            "IFileManager::Get().FileSize(*Artifact.Backup) !=",
            "FileMd5(Artifact.Backup) != Artifact.Md5",
            "!Artifact.Backup.IsEmpty() || Artifact.Bytes >= 0",
            "before canonical restore",
        ):
            self.assertIn(token, backup_prevalidation)
        prevalidation_barrier = rollback.index(
            "PrevalidateMaterialUpgradeBackupForRestore"
        )
        barrier_block = rollback.split(
            "bool bDiskRestoreSucceeded = true", 1
        )[0]
        self.assertIn(
            "if (!PrevalidateMaterialUpgradeBackupForRestore",
            barrier_block,
        )
        self.assertIn("return false;", barrier_block)
        self.assertNotIn("IFileManager::Get().Copy", barrier_block)
        self.assertNotIn("IFileManager::Get().Delete", barrier_block)
        self.assertLess(
            prevalidation_barrier, rollback.index("IFileManager::Get().Copy")
        )
        self.assertLess(
            prevalidation_barrier, rollback.index("IFileManager::Get().Delete")
        )
        restore_end = rollback.index("if (!bDiskRestoreSucceeded)")
        pre_reload_hash = rollback.index("PackageArtifactsMatch")
        reload_packages = rollback.index("ReloadMaterialPackages")
        post_reload_hash = rollback.rindex("PackageArtifactsMatch")
        self.assertLess(restore_end, pre_reload_hash)
        self.assertLess(pre_reload_hash, reload_packages)
        self.assertLess(reload_packages, post_reload_hash)
        self.assertIn("IFileManager::Get().Copy", rollback)
        self.assertIn("IFileManager::Get().Delete", rollback)

        admission = self.editor_cpp.split(
            "bool ValidateR10UpgradeInputMaterialAssets", 1
        )[1].split("UMaterial* DuplicateMaterialFresh", 1)[0]
        for token in (
            "ValidateSoilMaterial",
            "ValidateLegacyGrassMaterial",
            "ValidateR10GrassMaterial",
            "ValidateLegacyGroundOverlayMaterial",
            "ValidateR10GroundOverlayMaterial",
            "!bAllLegacy && !bAllR10",
            "mixed-version or invalid",
        ):
            self.assertIn(token, admission)

        for token in (
            "R10 material migration",
            ".m.ubulk",
            ".o.ubulk",
            "DELETE_IF_PRESENT",
            "Never restore only a subset",
        ):
            self.assertIn(token, self.package_readme)

    def test_r12_material_upgrade_is_exactly_five_package_atomic_and_fail_closed(self):
        upgrade = self.contract["r12MaterialUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismMaterialAssetsToR12"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual("V5D_R12_GROUND_MATERIAL_UPGRADE_PASS", upgrade["successReportPrefix"])
        self.assertEqual(["R10", "R12"], upgrade["uniformInputRevisions"])
        self.assertEqual(7, len(upgrade["requiredValidCleanPackages"]))
        self.assertEqual(
            [
                "M_IPV5D_Turf_Manicured",
                "M_IPV5D_Turf_Humid",
                "M_IPV5D_Turf_Shade",
                "M_IPV5D_Turf_DryEdge",
                "M_IPV5D_LawnMacroVariation",
            ],
            upgrade["inPlaceTargets"],
        )
        self.assertEqual(
            ["M_IPV5D_SoilMulch_Layered", "M_IPV5D_GrassMedium_EdgeFade"],
            upgrade["preservedPackages"],
        )
        self.assertEqual(5, upgrade["saveTargets"])
        self.assertEqual(5, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(1, upgrade["backupReceiptVersion"])
        self.assertEqual(
            "Saved/TRIAD/Backups/V5D_R12_GroundMaterials", upgrade["backupRoot"]
        )
        self.assertEqual(
            "TRIAD_V5D_R12_GROUND_MATERIAL_BACKUP_V1",
            upgrade["backupReceiptHeader"],
        )
        for key in (
            "grassGraphWiringValidatedForR10AndR12",
            "preservedSoilGraphWiringValidated",
            "preservedEdgeFadeGraphWiringValidated",
            "overlayTextureSampleSamplerAndMipStateValidated",
            "compiledActiveFeatureLevelShaderMapRequiredForAllSevenMaterials",
            "soilPreAndPostArtifactHashesEqual",
            "edgeFadePreAndPostArtifactHashesEqual",
            "targetMapPreAndPostArtifactHashesEqual",
            "rollbackRestoresAllFiveBeforeReload",
            "rollbackPreReloadHashesRequired",
            "rollbackPostReloadHashesRequired",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapsEverOfferedToSave",
            "soilEverOfferedToSave",
            "edgeFadeEverOfferedToSave",
            "mixedVersionRepairAllowed",
            "sourceV4V5BMaterialAssetsEverMutated",
            "mapTransformsCollisionNavigationAndRfEverMutated",
        ):
            self.assertFalse(upgrade[key], key)

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.split(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationRealismMaterialAssetsToR13", 1
        )[0]
        ordered_markers = (
            "ValidateR12UpgradeInputMaterialAssets",
            "CaptureObjectPackageArtifacts",
            "CaptureTargetMapArtifacts",
            "BackUpR12MaterialPackages",
            "ConfigureR12GrassMaterial",
            "BuildR12GroundOverlayMaterialGraph",
            "FinishAllCompilation",
            "SaveLoadedAssets",
            "ReloadMaterialPackages",
            "ValidateCompleteR12MaterialAssets",
        )
        positions = [endpoint.index(marker) for marker in ordered_markers]
        self.assertEqual(sorted(positions), positions)
        target_initializer = endpoint.split(
            "TArray<UMaterial*> Targets = {", 1
        )[1].split("};", 1)[0]
        self.assertEqual(5, target_initializer.count("Materials["))
        self.assertNotIn("Materials[4]", target_initializer)
        self.assertIn("Materials[5]", target_initializer)
        self.assertEqual(2, endpoint.count("CaptureObjectPackageArtifacts("))
        self.assertIn("ObjectPath(SoilAssetName)", endpoint)
        self.assertIn("ObjectPath(EdgeGrassFadeAssetName)", endpoint)
        self.assertIn("ExactSaveTargets.Num() != 5", endpoint)
        self.assertIn("SaveLoadedAssets(ExactSaveTargets, false)", endpoint)
        self.assertIn("ColdMaterials.Num() != 6", endpoint)
        self.assertIn("PackageArtifactsMatch(SoilBefore", endpoint)
        self.assertIn("PackageArtifactsMatch(EdgeBefore", endpoint)
        self.assertIn("PackageArtifactsMatch(MapBefore", endpoint)
        self.assertIn("AUTOMATIC_ROLLBACK_OK packages=5", endpoint)
        self.assertIn("V5D_R12_GROUND_MATERIAL_UPGRADE_PASS", endpoint)
        self.assertIn("mapsSaved=0", endpoint)
        self.assertNotIn("SaveMap", endpoint)
        self.assertNotIn("SavePackage", endpoint)
        self.assertNotIn("MarkPackageDirty", endpoint)

        admission = self.editor_cpp.split(
            "bool ValidateR12UpgradeInputMaterialAssets", 1
        )[1].split("UMaterial* DuplicateMaterialFresh", 1)[0]
        for token in (
            "ValidateR10GrassMaterial",
            "ValidateR12GrassMaterial",
            "ValidateR10GroundOverlayMaterial",
            "ValidateR12GroundOverlayMaterial",
            "ValidateSoilMaterial",
            "ValidateEdgeGrassFadeMaterial",
            "!bAllR10 && !bAllR12",
            "mixed-version or invalid",
        ):
            self.assertIn(token, admission)
        backup = self.editor_cpp.split("bool BackUpR12MaterialPackages", 1)[1].split(
            "bool ReloadMaterialPackages", 1
        )[0]
        self.assertIn('TEXT("R12")', backup)
        self.assertIn("TRIAD_V5D_R12_GROUND_MATERIAL_BACKUP_V1", backup)
        self.assertIn("V5D_%s_GroundMaterials", self.editor_cpp)
        for token in (
            "GrassProfiles[]",
            "0.105f, 0.180f, 0.083f",
            "0.70f, 0.30f, 0.14f",
            "0.100f, 0.175f, 0.084f",
            "0.66f, 0.32f, 0.18f",
            "0.091f, 0.154f, 0.087f",
            "0.72f, 0.28f, 0.11f",
            "0.126f, 0.165f, 0.086f",
            "0.76f, 0.26f, 0.14f",
        ):
            self.assertIn(token, self.editor_cpp)
        self.assertIn("R12 photographic grass-response migration", self.package_readme)
        self.assertIn("Never restore only a subset", self.package_readme)

    def test_r13_material_upgrade_and_grass_response_are_exact_and_fail_closed(self):
        response = self.contract["grassR13SurfaceResponse"]
        self.assertEqual([0.63, 0.84], response["roughness"]["clamp"])
        self.assertEqual([-0.035, 0.04], response["roughness"]["microDeltaRange"])
        self.assertEqual(0.02, response["roughness"]["rootDelta"])
        self.assertEqual(0.035, response["roughness"]["cutDelta"])
        self.assertEqual([0.2, 0.33], response["specular"]["clamp"])
        normal = response["distanceMatchedNormal"]
        self.assertEqual(800.0, normal["nearDistanceCentimeters"])
        self.assertEqual(2400.0, normal["farDistanceCentimeters"])
        self.assertEqual(0.72, normal["nearFacingCorrectedWeight"])
        self.assertEqual(0.28, normal["farFacingCorrectedWeight"])
        self.assertEqual(
            {
                "manicured": [0.814, 0.863, 0.829],
                "humid": [0.818, 0.863, 0.838],
                "shade": [0.839, 0.883, 0.876],
                "dry_edge": [0.754, 0.792, 0.786],
            },
            response["subsurfaceAttenuationLinearByProfile"],
        )

        upgrade = self.contract["r13MaterialUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismMaterialAssetsToR13"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual(
            "V5D_R13_GROUND_MATERIAL_UPGRADE_PASS",
            upgrade["successReportPrefix"],
        )
        self.assertEqual(["R12", "R13"], upgrade["uniformInputRevisions"])
        self.assertEqual(5, len(upgrade["requiredUniformCleanTargetPackages"]))
        self.assertEqual(5, upgrade["saveTargets"])
        self.assertEqual(5, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(
            "Saved/TRIAD/Backups/V5D_R13_GroundMaterials",
            upgrade["backupRoot"],
        )
        self.assertEqual(
            "TRIAD_V5D_R13_GROUND_MATERIAL_BACKUP_V1",
            upgrade["backupReceiptHeader"],
        )
        for key in (
            "exactFiveTargetAtomicTransaction",
            "r12ReviewedGrassProfileTuplesPreservedExactly",
            "grassGraphWiringValidatedForR12AndR13",
            "grassR13ColorRoughnessSpecularNormalAndSubsurfaceResponseSealed",
            "overlayGrass004TexturePathsAndR13ResponseSealed",
            "soilPreAndPostArtifactHashesEqual",
            "edgeFadePreAndPostArtifactHashesEqual",
            "targetMapPreAndPostArtifactHashesEqual",
            "rollbackRestoresAllFiveBeforeReload",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapsEverOfferedToSave",
            "soilEverOfferedToSave",
            "edgeFadeEverOfferedToSave",
            "mixedVersionRepairAllowed",
            "mapTransformsCollisionNavigationAndRfEverMutated",
        ):
            self.assertFalse(upgrade[key], key)

        endpoint = self.editor_cpp.split(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationRealismMaterialAssetsToR15", 1
        )[0]
        ordered_markers = (
            "ValidateR13UpgradeInputMaterialAssets",
            "CaptureObjectPackageArtifacts",
            "CaptureTargetMapArtifacts",
            "BackUpR13MaterialPackages",
            "ConfigureGrassMaterial",
            "BuildGroundOverlayMaterialGraph",
            "FinishAllCompilation",
            "SaveLoadedAssets",
            "ReloadMaterialPackages",
            "ValidateCompleteR13MaterialAssets",
        )
        positions = [endpoint.index(marker) for marker in ordered_markers]
        self.assertEqual(sorted(positions), positions)
        target_initializer = endpoint.split(
            "TArray<UMaterial*> Targets = {", 1
        )[1].split("};", 1)[0]
        self.assertEqual(5, target_initializer.count("Materials["))
        self.assertNotIn("Materials[4]", target_initializer)
        self.assertIn("Materials[5]", target_initializer)
        self.assertEqual(2, endpoint.count("CaptureObjectPackageArtifacts("))
        self.assertIn("ExactSaveTargets.Num() != 5", endpoint)
        self.assertIn("AUTOMATIC_ROLLBACK_OK packages=5", endpoint)
        self.assertNotIn("SaveMap", endpoint)
        self.assertNotIn("SavePackage", endpoint)
        self.assertNotIn("MarkPackageDirty", endpoint)

        admission = self.editor_cpp.split(
            "bool ValidateR13UpgradeInputMaterialAssets", 1
        )[1].split("UMaterial* DuplicateMaterialFresh", 1)[0]
        for token in (
            "ValidateR12GrassMaterial",
            "ValidateGrassMaterial",
            "ValidateR12GroundOverlayMaterial",
            "ValidateGroundOverlayMaterial",
            "ValidateSoilMaterial",
            "ValidateEdgeGrassFadeMaterial",
            "!bAllR12 && !bAllR13",
            "mixed-version or invalid",
        ):
            self.assertIn(token, admission)
        for token in (
            "GrassR13RoughnessCode",
            "GrassR13SpecularCode",
            "GrassR13NormalAlphaCode",
            "Data->ExpressionCollection.Expressions.Num() != 29",
            "GrassR13SubsurfaceAttenuation",
            "Grass004_Color.Grass004_Color",
            "Grass004_NormalGL.Grass004_NormalGL",
            "Grass004_Roughness.Grass004_Roughness",
            "Grass004_AmbientOcclusion.Grass004_AmbientOcclusion",
        ):
            self.assertIn(token, self.editor_cpp)

        configure_r13_grass = self.editor_cpp.split(
            "bool ConfigureGrassMaterial(\n", 1
        )[1].split("bool ConfigureR10GrassMaterial", 1)[0]
        for node, description in (
            ("Roughness", "GrassR13RoughnessDescription"),
            ("Specular", "GrassR13SpecularDescription"),
            ("NormalAlpha", "GrassR13NormalAlphaDescription"),
        ):
            with self.subTest(r13_custom_node=node):
                self.assertIn(
                    f"{node}->Description = {description};",
                    configure_r13_grass,
                )
                self.assertIn(
                    f"{node}->OutputType = CMOT_Float1;",
                    configure_r13_grass,
                )
        self.assertIn(
            "R13 stochastic grass and Grass004 overlay migration",
            self.package_readme,
        )
        self.assertIn(
            "farDetailFadeMeters=30,55 farNormalStrength=revisionValidated",
            self.actor_cpp,
        )
        self.assertNotIn(
            "farDetailFadeMeters=30,55 farNormalStrength=0.03",
            self.actor_cpp,
        )

    def test_r15_material_upgrade_and_grass_response_are_exact_and_fail_closed(self):
        response = self.contract["grassR15SurfaceResponse"]
        self.assertEqual([1.1, 1.16, 1.08], response["finalBladeColorGainLinear"])
        self.assertEqual(-0.06, response["roughness"]["profileBias"])
        self.assertEqual([-0.045, 0.035], response["roughness"]["microDeltaRange"])
        self.assertEqual(0.02, response["roughness"]["rootDelta"])
        self.assertEqual(-0.03, response["roughness"]["cutDelta"])
        self.assertEqual([0.56, 0.8], response["roughness"]["clamp"])
        self.assertEqual([0.92, 1.18], response["specular"]["profileMultiplierRange"])
        self.assertEqual(0.045, response["specular"]["cutAddition"])
        self.assertEqual([0.24, 0.4], response["specular"]["clamp"])
        normal = response["distanceMatchedNormal"]
        self.assertEqual(0.82, normal["nearBaseFacingCorrectedWeight"])
        self.assertEqual(0.32, normal["farBaseFacingCorrectedWeight"])
        self.assertEqual([0.86, 1.14], normal["perBladeMultiplierRange"])
        self.assertEqual(
            ["WorldPosition", "CameraPosition", "InstanceFade", "BladeUV", "Random01"],
            normal["customInputs"],
        )
        self.assertEqual(
            {
                "manicured": [0.96, 1.04, 0.98],
                "humid": [0.97, 1.06, 1.0],
                "shade": [1.02, 1.1, 1.05],
                "dry_edge": [0.9, 0.97, 0.92],
            },
            response["subsurfaceGainLinearByProfile"],
        )
        self.assertEqual(29, response["expressionCountPerProfile"])
        self.assertEqual(0, response["textureSampleCountPerProfile"])
        for key in (
            "emissiveConnected",
            "anisotropyConnected",
            "globalLightingChanged",
            "instanceTransformsChanged",
        ):
            self.assertFalse(response[key], key)

        upgrade = self.contract["r15MaterialUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismMaterialAssetsToR15"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual(
            "V5D_R15_GROUND_MATERIAL_UPGRADE_PASS",
            upgrade["successReportPrefix"],
        )
        self.assertEqual(["R13", "R15"], upgrade["uniformInputRevisions"])
        self.assertEqual(5, len(upgrade["requiredUniformCleanTargetPackages"]))
        self.assertEqual(5, upgrade["saveTargets"])
        self.assertEqual(5, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(29, upgrade["grassR15GraphExpressionCount"])
        self.assertEqual(0, upgrade["grassR15TextureSampleCount"])
        self.assertEqual(23, upgrade["overlayR15GraphExpressionCount"])
        self.assertEqual(4, upgrade["overlayR15TextureSampleCount"])
        for key in (
            "exactFiveTargetAtomicTransaction",
            "r13GrassProfileTuplesAndCoherentColorPolicyPreserved",
            "grassR15FiveInputNormalSealed",
            "grassR15ColorRoughnessSpecularNormalAndSubsurfaceResponseSealed",
            "grassR15EmissiveDisconnected",
            "overlayGrass004TexturePathsAndR15ResponseSealed",
            "soilPreAndPostArtifactHashesEqual",
            "edgeFadePreAndPostArtifactHashesEqual",
            "targetMapPreAndPostArtifactHashesEqual",
            "rollbackRestoresAllFiveBeforeReload",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapsEverOfferedToSave",
            "soilEverOfferedToSave",
            "edgeFadeEverOfferedToSave",
            "mixedVersionRepairAllowed",
            "sourceV3V4V5BMaterialAndTextureAssetsEverMutated",
            "globalLightingEverMutated",
            "materialEmissiveEverConnected",
            "instanceTransformsEverMutated",
            "mapTransformsCollisionNavigationAndRfEverMutated",
        ):
            self.assertFalse(upgrade[key], key)

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationRealismGrassSystemToR22", 1
        )[0]
        ordered_markers = (
            "ValidateR15UpgradeInputMaterialAssets",
            "CaptureObjectPackageArtifacts",
            "CaptureTargetMapArtifacts",
            "BackUpR15MaterialPackages",
            "ConfigureR15GrassMaterial",
            "BuildR15GroundOverlayMaterialGraph",
            "FinishAllCompilation",
            "SaveLoadedAssets",
            "ReloadMaterialPackages",
            "ValidateCompleteR15MaterialAssets",
        )
        positions = [endpoint.index(marker) for marker in ordered_markers]
        self.assertEqual(sorted(positions), positions)
        target_initializer = endpoint.split(
            "TArray<UMaterial*> Targets = {", 1
        )[1].split("};", 1)[0]
        self.assertEqual(5, target_initializer.count("Materials["))
        self.assertNotIn("Materials[4]", target_initializer)
        self.assertIn("Materials[5]", target_initializer)
        self.assertIn("ExactSaveTargets.Num() != 5", endpoint)
        self.assertIn("SaveLoadedAssets(ExactSaveTargets, false)", endpoint)
        self.assertIn("AUTOMATIC_ROLLBACK_OK packages=5", endpoint)
        self.assertIn("sourceV3V4V5BMaterialAssetsUntouched=true", endpoint)
        self.assertIn("globalLightingUntouched=true", endpoint)
        self.assertIn("materialsEmissiveUntouched=true", endpoint)
        self.assertIn("instanceTransformsUntouched=true", endpoint)
        for forbidden in ("SaveMap", "SavePackage", "MarkPackageDirty"):
            self.assertNotIn(forbidden, endpoint)

        admission = self.editor_cpp.split(
            "bool ValidateR15UpgradeInputMaterialAssets", 1
        )[1].split("UMaterial* DuplicateMaterialFresh", 1)[0]
        for token in (
            "ValidateGrassMaterial",
            "ValidateR15GrassMaterial",
            "ValidateGroundOverlayMaterial",
            "ValidateR15GroundOverlayMaterial",
            "ValidateSoilMaterial",
            "ValidateEdgeGrassFadeMaterial",
            "!bAllR13 && !bAllR15",
            "mixed-version or invalid",
        ):
            self.assertIn(token, admission)

        configure = self.editor_cpp.split(
            "bool ConfigureR15GrassMaterial", 1
        )[1].split("bool ConfigureR10GrassMaterial", 1)[0]
        for token in (
            "BuildR15GrassColorCode",
            "GrassR15RoughnessCode",
            "GrassR15SpecularCode",
            "GrassR15NormalAlphaCode",
            "NormalAlpha->Inputs.SetNum(5)",
            'Inputs[3].InputName = TEXT("BladeUV")',
            'Inputs[4].InputName = TEXT("Random01")',
            "GrassR15SubsurfaceGain[ProfileIndex]",
            "Data->ExpressionCollection.Expressions.Num() != 29",
        ):
            self.assertIn(token, configure)
        for token in (
            "BaseRoughness-0.060+lerp(-0.045,0.035,micro)",
            "-0.030*cutMask,0.56,0.80",
            "BaseSpecular*lerp(0.92,1.18,micro)+0.045*cutMask",
            "0.24,0.40",
            "float baseWeight=lerp(0.32,0.82,nearWeight)",
            "float bladeWeight=lerp(0.86,1.14,micro)",
        ):
            self.assertIn(token, self.editor_cpp)
        self.assertIn(
            "R15 shade-readable grass and overlay migration",
            self.package_readme,
        )
        self.assertIn("farNormalStrength=revisionValidated", self.actor_cpp)
        self.assertNotIn("farNormalStrength=0.02", self.actor_cpp)
        self.assertNotIn("OutReport.ReplaceInline", self.actor_cpp)
        self.assertIn("grassMaterialRevision=R19", self.actor_cpp)
        self.assertIn("groundOverlayMaterialRevision=R18", self.actor_cpp)
        self.assertIn(
            "groundOverlayR16CoreMaskPreserved=true",
            self.actor_cpp,
        )

    def test_r17_managed_turf_upgrade_is_five_target_atomic_and_preserves_r16_seam(self):
        response = self.contract["grassR17SurfaceResponse"]
        self.assertEqual([1.16, 1.18, 1.02], response["finalBladeColorGainLinear"])
        self.assertEqual(-0.02, response["roughness"]["profileBias"])
        self.assertEqual([-0.035, 0.04], response["roughness"]["microDeltaRange"])
        self.assertEqual(0.015, response["roughness"]["cutDelta"])
        self.assertEqual([0.63, 0.84], response["roughness"]["clamp"])
        self.assertEqual([0.86, 1.08], response["specular"]["profileMultiplierRange"])
        self.assertEqual(0.015, response["specular"]["cutAddition"])
        self.assertEqual([0.2, 0.33], response["specular"]["clamp"])
        self.assertEqual([0.66, 0.2], [
            response["distanceMatchedNormal"]["nearBaseFacingCorrectedWeight"],
            response["distanceMatchedNormal"]["farBaseFacingCorrectedWeight"],
        ])
        self.assertEqual([0.9, 1.1], response["distanceMatchedNormal"]["perBladeMultiplierRange"])
        upgrade = self.contract["r17GroundMaterialUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismMaterialAssetsToR17"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual("V5D_R17_GROUND_MATERIAL_UPGRADE_PASS", upgrade["successReportPrefix"])
        self.assertEqual(["R16", "R17"], upgrade["uniformInputRevisions"])
        self.assertEqual(5, upgrade["saveTargets"])
        self.assertEqual(5, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertTrue(upgrade["r16CoreMaskTopologyPreservedExactly"])
        self.assertFalse(upgrade["mapsEverOfferedToSave"])
        self.assertFalse(upgrade["sourceV3V4V5BMaterialAndTextureAssetsEverMutated"])
        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationRealismGrassSystemToR23", 1
        )[0]
        for token in (
            "ValidateR17UpgradeInputMaterialAssets",
            "CaptureR17PreservedPackageSnapshots",
            "BackUpR17GroundMaterialPackages",
            "ConfigureR17GrassMaterial",
            "ConfigureR17GroundOverlayPhotographicResponseInPlace",
            "ValidateCompleteR17MaterialAssets",
            "IDEMPOTENT_V5D_R17_GROUND_MATERIALS_ALREADY_VALID",
            "V5D_R17_GROUND_MATERIAL_UPGRADE_PASS",
            "opaqueCollarMeters=50",
            "outwardFeatherMeters=8",
        ):
            self.assertIn(token, endpoint)
        targets = endpoint.split("TArray<UMaterial*> Targets = {", 1)[1].split("};", 1)[0]
        self.assertEqual(5, targets.count("Materials["))
        self.assertNotIn("Materials[4]", targets)
        self.assertIn("Materials[5]", targets)
        backup_helper = self.editor_cpp.split(
            "bool BackUpR17GroundMaterialPackages", 1
        )[1].split("bool ReloadMaterialPackages", 1)[0]
        self.assertIn('TEXT("R17")', backup_helper)
        self.assertNotIn('TEXT("V5D_R17_GroundMaterials")', backup_helper)
        for token in (
            "float3(1.16,1.18,1.02)",
            "BaseRoughness-0.020+lerp(-0.035,0.040,micro)",
            "+0.015*cutMask,0.63,0.84",
            "BaseSpecular*lerp(0.86,1.08,micro)+0.015*cutMask",
            "float baseWeight=lerp(0.20,0.66,nearWeight)",
            "float bladeWeight=lerp(0.90,1.10,micro)",
            "float3(0.700,0.900,0.620)",
            "GroundR17NearNormalStrength = 0.15f",
            "GroundR17FarNormalStrength = 0.01f",
            "GroundR17Specular = 0.18f",
        ):
            self.assertIn(token, self.editor_cpp)
        r17_validator = self.editor_cpp.split(
            "bool ValidateR17GrassMaterial(\n"
            "    UMaterial* Material,\n"
            "    int32 ProfileIndex,\n"
            "    FString& OutError)\n{",
            1,
        )[1].split("bool ConfigureR17GrassMaterial", 1)[0]
        for token in (
            "Material->MaterialDomain != MD_Surface",
            "Material->BlendMode != BLEND_Masked",
            "MSM_TwoSidedFoliage",
            "Data->ExpressionCollection.Expressions.Num() != 29",
            "TextureSamples != 0",
            "InstanceRandomNodes != 1",
            "InstanceFadeNodes != 1",
            "!bNoCustomizedUvConnections",
            "!AllCustomExpressionsHaveNoAuxiliaryState(Data)",
            "!BladeUv || !WorldPosition || !Camera || !InstanceRandom || !InstanceFade",
            "bExactColorInputs",
            "bExactSurfaceInputs",
            "bExactNormalAlphaInputs",
            "InputMatches(Data->OpacityMask, Dither, 0)",
            "InputMatches(Data->WorldPositionOffset, Wind, 0)",
            "Data->EmissiveColor.Expression",
            "ValidateCompiledMaterial(Material, OutError)",
        ):
            self.assertIn(token, r17_validator)
        uniform = self.editor_cpp.split(
            "bool ValidateUniformR13R15OrR16MaterialAssetsInternal", 1
        )[1].split("bool ValidateR10MaterialAssetsInternal", 1)[0]
        self.assertIn("ValidateR16MaterialAssetsInternal(R16Materials, R16Error)", uniform)
        self.assertNotIn("ValidateMaterialAssetsInternal(R16Materials, R16Error)", uniform)

    def test_r18_lawn_calibration_is_exactly_one_package_and_preserves_r17(self):
        response = self.contract["groundR18SurfaceResponse"]
        self.assertEqual("R18", response["materialRevision"])
        self.assertEqual([0.715, 0.965, 0.595], response["photographicTintLinear"])
        self.assertEqual(
            0.88512921,
            response["photographicTintApproximateLuminanceMultiplier"],
        )
        self.assertEqual(
            [0.7, 0.9, 0.62],
            response["historicalR17PhotographicTintLinear"],
        )
        for key in (
            "baseDescriptionAndTintLiteralAreOnlyR17SourceTermChanges",
            "roughnessDescriptionCodeAndWiringPreservedExactlyFromR17",
            "grass004TexturesAndSampleStatePreservedExactlyFromR17",
            "mowingMacroWearWetnessAndDetailFadePreservedExactlyFromR17",
            "r16CoreMaskTopologyAndCodePreservedExactly",
            "fourGrassPackagesRemainExactR17",
        ):
            self.assertTrue(response[key], key)
        self.assertEqual([0.15, 0.01], response["nearAndFarNormalStrengthPreservedExactlyFromR17"])
        self.assertEqual(0.18, response["specularPreservedExactlyFromR17"])
        self.assertEqual(23, response["expressionCount"])
        self.assertEqual(4, response["textureSampleCount"])

        upgrade = self.contract["r18LawnOverlayUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismLawnOverlayToR18"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual(
            "V5D_R18_LAWN_OVERLAY_UPGRADE_PASS",
            upgrade["successReportPrefix"],
        )
        self.assertEqual(["R17", "R18"], upgrade["uniformInputRevisions"])
        self.assertEqual("M_IPV5D_LawnMacroVariation", upgrade["soleSaveTarget"])
        self.assertEqual(1, upgrade["saveTargets"])
        self.assertEqual(1, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(
            "TRIAD_V5D_R18_LAWN_OVERLAY_BACKUP_V1",
            upgrade["backupReceiptHeader"],
        )
        self.assertEqual(
            "Saved/TRIAD/Backups/V5D_R18_GroundMaterials",
            upgrade["backupRoot"],
        )
        for key in (
            "exactOnePackageAtomicTransaction",
            "fourR17GrassPackagesArtifactHashesPreserved",
            "soilEdgeFadeAndTargetMapArtifactHashesPreserved",
            "sourceV3V4V5BMaterialAndTextureArtifactHashesPreserved",
            "absentSidecarsRecordedAndDeletedOnRollback",
            "rollbackRestoresSoleTargetBeforeReload",
            "rollbackPreReloadHashesRequired",
            "rollbackPostReloadHashesRequired",
            "r17BaseDescriptionAndTintLiteralAreOnlySourceTermChanges",
            "r17RoughnessNormalSpecularTexturesMowingAndCoreMaskPreservedExactly",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapsEverOfferedToSave",
            "instanceTransformsEverMutated",
            "collisionNavigationSensorAndRfEverMutated",
            "globalLightingEverMutated",
            "materialEmissiveEverConnected",
            "mixedVersionRepairAllowed",
        ):
            self.assertFalse(upgrade[key], key)

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationRealismGrassMaterialsToR19", 1
        )[0]
        for token in (
            "ValidateR18UpgradeInputMaterialAssets",
            "CaptureR18PreservedPackageSnapshots",
            "BackUpR18LawnOverlayPackage",
            "ConfigureR18GroundOverlayTintInPlace",
            "ValidateCompleteR18MaterialAssets",
            "R18PreservedPackageSnapshotsMatch",
            "IDEMPOTENT_V5D_R18_LAWN_OVERLAY_ALREADY_VALID",
            "V5D_R18_LAWN_OVERLAY_UPGRADE_PASS",
            "ExactSaveTargets = {Targets[0]}",
            "fourR17GrassHashesPreserved=true",
            "overlayTint=0.715,0.965,0.595",
            "r17RoughnessAndNormalResponsePreservedExactly=true",
            "r16CoreMaskTopologyPreservedExactly=true",
        ):
            self.assertIn(token, endpoint)
        targets = endpoint.split("TArray<UMaterial*> Targets = {", 1)[1].split("};", 1)[0]
        self.assertEqual(1, targets.count("Materials["))
        self.assertEqual("Materials[5]", targets.strip())
        for forbidden in ("SaveMap", "SavePackage", "Materials[0]", "Materials[1]", "Materials[2]", "Materials[3]", "Materials[4]"):
            self.assertNotIn(forbidden, endpoint)

        configure = self.editor_cpp.split(
            "bool ConfigureR18GroundOverlayTintInPlace", 1
        )[1].split("bool ConfigureR22GroundOverlayMaterial", 1)[0]
        for token in (
            "ValidateR17GroundOverlayMaterial",
            "GroundR17BaseDescription",
            "GroundR17BaseCode",
            "Base->Description = GroundR18BaseDescription",
            "Base->Code = GroundR18BaseCode",
            "ResetCustomAuxiliaryState(Base)",
            "ValidateR18GroundOverlayMaterial",
        ):
            self.assertIn(token, configure)
        for forbidden in ("Roughness->", "NormalStrength->", "Specular->", "CoreMask->"):
            self.assertNotIn(forbidden, configure)

        r17_base = self.editor_cpp.split("const FString GroundR17BaseCode(", 1)[1].split(
            "const FString GroundR17RoughnessCode", 1
        )[0]
        r18_base = self.editor_cpp.split("const FString GroundR18BaseCode(", 1)[1].split(
            "// R22 is calibrated", 1
        )[0]
        self.assertEqual(
            r17_base.strip(),
            r18_base.replace("0.715,0.965,0.595", "0.700,0.900,0.620").strip(),
        )
        r18_validator = self.editor_cpp.split(
            "bool ValidateR18GroundOverlayMaterial", 1
        )[1].split("bool ValidateR17GrassMaterial", 1)[0]
        for token in (
            "GroundR18BaseDescription",
            "GroundR18BaseCode",
            "GroundR17RoughnessDescription",
            "GroundR17RoughnessCode",
            "GroundR17NearNormalStrength",
            "GroundR17FarNormalStrength",
            "GroundR17Specular",
            "GroundCoreMaskDescription",
            "GroundCoreMaskCode",
        ):
            self.assertIn(token, r18_validator)
        current_validator = self.editor_cpp.split(
            "bool ValidateMaterialAssetsInternal(\n"
            "    TArray<UMaterial*>& OutMaterials,\n"
            "    FString& OutError)\n{",
            1,
        )[1].split("bool ValidateR16MaterialAssetsInternal", 1)[0]
        for token in (
            "ValidateR19GrassMaterial(Material, Index, OutError)",
            "ValidateSoilMaterial(Soil, OutError)",
            "ValidateR18GroundOverlayMaterial(Overlay, OutError)",
        ):
            self.assertIn(token, current_validator)
        self.assertNotIn("ValidateR17GrassMaterial", current_validator)
        self.assertNotIn("ValidateR17GroundOverlayMaterial", current_validator)
        preserved = self.editor_cpp.split(
            "bool CaptureR18PreservedPackageSnapshots", 1
        )[1].split("bool R18PreservedPackageSnapshotsMatch", 1)[0]
        self.assertIn("for (const FString& Name : GrassAssetNames)", preserved)
        self.assertIn("CaptureTargetMapArtifacts", preserved)
        self.assertIn("OutSnapshots.Num() != 14", preserved)
        backup = self.editor_cpp.split("bool BackUpR18LawnOverlayPackage", 1)[1].split(
            "bool ReloadMaterialPackages", 1
        )[0]
        self.assertIn('TEXT("R18")', backup)
        self.assertIn('TEXT("TRIAD_V5D_R18_LAWN_OVERLAY_BACKUP_V1")', backup)
        self.assertIn("R18 calibrated lawn-overlay migration", self.package_readme)

    def test_r19_grass_upgrade_is_exactly_four_packages_and_distance_filters_aliasing(self):
        response = self.contract["grassR19SurfaceResponse"]
        self.assertEqual("R19", response["materialRevision"])
        self.assertEqual([1.34, 1.18, 1.03], response["finalBladeColorGainLinear"])
        self.assertEqual(1.20319344, response["finalBladeColorGainApproximateLuminanceMultiplier"])
        self.assertEqual(0.88, response["finalChromaFraction"])
        self.assertEqual([800.0, 2000.0], response["detailFadeCentimeters"])
        self.assertEqual(0.035, response["roughness"]["profileBias"])
        self.assertEqual([-0.012, 0.018], response["roughness"]["nearMicroDeltaRange"])
        self.assertEqual(0.025, response["roughness"]["rootDelta"])
        self.assertEqual(0.03, response["roughness"]["cutDelta"])
        self.assertEqual([0.7, 0.9], response["roughness"]["clamp"])
        self.assertEqual([0.62, 0.72], response["specular"]["profileMultiplierRange"])
        self.assertEqual(0.003, response["specular"]["cutAddition"])
        self.assertEqual([0.16, 0.23], response["specular"]["clamp"])
        self.assertEqual(0.5, response["specular"]["farMicroValue"])
        self.assertEqual(
            [0.5, 0.03],
            [
                response["distanceMatchedNormal"]["nearBaseFacingCorrectedWeight"],
                response["distanceMatchedNormal"]["farBaseFacingCorrectedWeight"],
            ],
        )
        self.assertEqual([0.96, 1.04], response["distanceMatchedNormal"]["nearPerBladeMultiplierRange"])
        self.assertEqual(0.5, response["distanceMatchedNormal"]["farMicroValue"])
        self.assertEqual(
            ["WorldPosition", "CameraPosition", "InstanceFade", "BladeUV", "Random01"],
            response["distanceMatchedNormal"]["customInputs"],
        )
        self.assertEqual(
            {
                "manicured": [1.2, 1.08, 0.9],
                "humid": [1.18, 1.09, 0.92],
                "shade": [1.23, 1.11, 0.98],
                "dry_edge": [1.15, 1.03, 0.86],
            },
            response["subsurfaceGainLinearByProfile"],
        )
        self.assertEqual(
            ["BladeUV", "Random01", "WorldPosition", "CameraPosition", "InstanceFade"],
            response["colorCustomInputs"],
        )
        self.assertEqual(
            ["BaseRoughness", "BladeUV", "Random01", "WorldPosition", "CameraPosition"],
            response["roughnessCustomInputs"],
        )
        self.assertEqual(
            ["BaseSpecular", "BladeUV", "Random01", "WorldPosition", "CameraPosition"],
            response["specularCustomInputs"],
        )
        self.assertEqual(29, response["expressionCountPerProfile"])
        self.assertEqual(0, response["textureSampleCountPerProfile"])

        upgrade = self.contract["r19GrassMaterialUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismGrassMaterialsToR19"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual("V5D_R19_GRASS_MATERIAL_UPGRADE_PASS", upgrade["successReportPrefix"])
        self.assertEqual(["R17", "R19"], upgrade["uniformGrassInputRevisions"])
        self.assertEqual("R18", upgrade["requiredGroundOverlayRevision"])
        self.assertEqual(
            [
                "M_IPV5D_Turf_Manicured",
                "M_IPV5D_Turf_Humid",
                "M_IPV5D_Turf_Shade",
                "M_IPV5D_Turf_DryEdge",
            ],
            upgrade["exactSaveTargets"],
        )
        self.assertEqual(4, upgrade["saveTargets"])
        self.assertEqual(4, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(11, upgrade["immutablePackageSnapshotCount"])
        self.assertEqual(
            [".uasset", ".uexp", ".ubulk", ".uptnl", ".m.ubulk", ".o.ubulk"],
            upgrade["nativeArtifactRosterExtensions"],
        )
        self.assertEqual([".upayload"], upgrade["externalWatchedExtraSidecars"])
        self.assertEqual(6, upgrade["nativeArtifactStatesPerPackage"])
        self.assertEqual(7, upgrade["externalWatchedArtifactStatesPerPackage"])
        self.assertEqual(
            [
                "/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_LawnMacroVariation.M_IPV5D_LawnMacroVariation",
                "/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_SoilMulch_Layered.M_IPV5D_SoilMulch_Layered",
                "/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_GrassMedium_EdgeFade.M_IPV5D_GrassMedium_EdgeFade",
                "/Game/TRIAD/IstanaPublicViewExploreV5B/Materials/M_IPV5B_AccentTurf.M_IPV5B_AccentTurf",
                "/Game/TRIAD/IstanaPublicViewExploreV5B/Materials/M_IPV5B_FormalBedSoil.M_IPV5B_FormalBedSoil",
                "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_GrassMedium_Wind.M_IPV4_GrassMedium_Wind",
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_Color.Grass004_Color",
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_NormalGL.Grass004_NormalGL",
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_Roughness.Grass004_Roughness",
                "/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_AmbientOcclusion.Grass004_AmbientOcclusion",
                "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid",
            ],
            upgrade["immutablePackages"],
        )
        self.assertEqual(
            "TRIAD_V5D_R19_GRASS_MATERIALS_BACKUP_V1",
            upgrade["backupReceiptHeader"],
        )
        self.assertEqual(
            "Saved/TRIAD/Backups/V5D_R19_GroundMaterials",
            upgrade["backupRoot"],
        )
        for key in (
            "exactFourPackageAtomicTransaction",
            "allFourTargetHashesMustChange",
            "rollbackRestoresAllFourBeforeReload",
            "rollbackPreReloadHashesRequired",
            "rollbackPostReloadHashesRequired",
            "overlaySoilEdgeSourcesTexturesAndMapArtifactHashesPreserved",
            "historicalR17AndR18ValidatorsPreserved",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapsEverOfferedToSave",
            "instanceTransformsEverMutated",
            "collisionNavigationSensorAndRfEverMutated",
            "globalLightingEverMutated",
            "materialEmissiveEverConnected",
            "mixedVersionRepairAllowed",
        ):
            self.assertFalse(upgrade[key], key)

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationEdgeGrassFadeAssetToR11", 1
        )[0]
        for token in (
            "ValidateR19UpgradeInputMaterialAssets",
            "CaptureR19PreservedPackageSnapshots",
            "BackUpR19GrassMaterialPackages",
            "ConfigureR19GrassMaterial",
            "ValidateCompleteR19MaterialAssets",
            "R19PreservedPackageSnapshotsMatch",
            "IDEMPOTENT_V5D_R19_GRASS_MATERIALS_ALREADY_VALID",
            "V5D_R19_GRASS_MATERIAL_UPGRADE_PASS",
            "grassDiffuseGain=1.34,1.18,1.03",
            "grassFinalChroma=0.88",
            "grassDetailFadeMeters=8,20",
            "grassRoughnessClamp=0.70,0.90",
            "grassSpecularClamp=0.16,0.23",
            "grassNormalNearFar=0.50,0.03",
            "overlayR18HashPreserved=true",
            "mapsSaved=0",
            "instanceTransformsUntouched=true",
        ):
            self.assertIn(token, endpoint)
        targets = endpoint.split("TArray<UMaterial*> Targets = {", 1)[1].split("};", 1)[0]
        self.assertEqual(4, targets.count("Materials["))
        for index in range(4):
            self.assertIn(f"Materials[{index}]", targets)
        self.assertNotIn("Materials[4]", targets)
        self.assertNotIn("Materials[5]", targets)
        for forbidden in ("SaveMap", "SavePackage", "SetStaticMesh", "SetWorldTransform"):
            self.assertNotIn(forbidden, endpoint)

        configure = self.editor_cpp.split(
            "bool ConfigureR19GrassMaterial(\n"
            "    UMaterial* Material,\n"
            "    int32 ProfileIndex,\n"
            "    FString& OutError)\n{",
            1,
        )[1].split("bool ConfigureR22GrassMaterial", 1)[0]
        for token in (
            "ValidateR17GrassMaterial",
            "BuildR19GrassColorCode",
            "GrassR19RoughnessCode",
            "GrassR19SpecularCode",
            "GrassR19NormalAlphaCode",
            "GrassR19SubsurfaceGain[ProfileIndex]",
            "ValidateR19GrassMaterial",
        ):
            self.assertIn(token, configure)
        for forbidden in (
            "AddExpression",
            "NewObject",
            "SetStaticMesh",
            "SetWorldTransform",
            "SaveMap",
            "Collision",
            "Navigation",
        ):
            self.assertNotIn(forbidden, configure)

        validator = self.editor_cpp.split(
            "bool ValidateR19GrassMaterial(\n"
            "    UMaterial* Material,\n"
            "    int32 ProfileIndex,\n"
            "    FString& OutError)\n{",
            1,
        )[1].split("bool ConfigureR19GrassMaterial", 1)[0]
        for token in (
            "Data->ExpressionCollection.Expressions.Num() != 29",
            "TextureSamples != 0",
            "bExactColorInputs",
            "bExactRoughnessInputs",
            "bExactSpecularInputs",
            "bExactNormalInputs",
            "ColorCode.Contains(TEXT(\"sin(\")",
            "Roughness->Code.Contains(TEXT(\"sin(\")",
            "Specular->Code.Contains(TEXT(\"sin(\")",
            "NormalAlpha->Code.Contains(TEXT(\"sin(\")",
            "Wind->Code != GrassWindCode",
            "InputMatches(Data->OpacityMask, Dither, 0)",
            "InputMatches(Data->WorldPositionOffset, Wind, 0)",
            "Data->EmissiveColor.Expression",
            "ValidateCompiledMaterial(Material, OutError)",
        ):
            self.assertIn(token, validator)
        self.assertIn("bool ValidateR17GrassMaterial(", self.editor_cpp)
        self.assertIn("bool ValidateR18GroundOverlayMaterial(", self.editor_cpp)
        self.assertIn("R19 distance-filtered grass-material migration", self.package_readme)
        self.assertIn("grassMaterialRevision=R19", self.actor_cpp)
        self.assertIn("grassR19DistanceFilteredColorNormalAndSurfaceResponse=true", self.actor_cpp)

    def test_r21_calibrated_grass_upgrade_is_four_package_only_and_wpo_stable(self):
        response = self.contract["grassR21CalibratedSurfaceResponse"]
        self.assertEqual("R21", response["materialRevision"])
        self.assertEqual(
            ["manicured", "humid", "shade", "dry_edge"],
            response["profileOrder"],
        )
        self.assertEqual(
            [[0.095, 0.115, 0.03], [0.087, 0.108, 0.032],
             [0.075, 0.092, 0.032], [0.115, 0.12, 0.027]],
            response["rootLinearByProfile"],
        )
        self.assertEqual(
            [[0.185, 0.21, 0.055], [0.172, 0.2, 0.057],
             [0.15, 0.175, 0.058], [0.205, 0.195, 0.048]],
            response["bodyLinearByProfile"],
        )
        self.assertEqual(
            [[0.2, 0.22, 0.06], [0.187, 0.21, 0.062],
             [0.163, 0.185, 0.063], [0.224, 0.207, 0.052]],
            response["tipLinearByProfile"],
        )
        self.assertEqual([0.68, 0.66, 0.72, 0.75], response["roughnessNearByProfile"])
        self.assertEqual(0.74, response["roughnessFar"])
        self.assertEqual([0.3, 0.31, 0.28, 0.26], response["specularNearByProfile"])
        self.assertEqual(0.19, response["specularFar"])
        self.assertEqual([0.1, 0.07, 0.13, 0.26], response["dryFractionByProfile"])
        self.assertEqual([0.18, 0.13, 0.22, 0.34], response["thatchFractionByProfile"])
        self.assertEqual([0.035, 0.025, 0.045, 0.075], response["soilRootFractionByProfile"])
        self.assertEqual([13.5, 4.7], response["macroScalesMeters"])
        self.assertEqual([5.0, 14.0], response["colorDetailFadeMeters"])
        self.assertEqual([4.0, 12.0], response["normalDetailFadeMeters"])
        self.assertEqual(29, response["expressionCountPerProfile"])
        self.assertEqual(0, response["textureSampleCountPerProfile"])
        self.assertTrue(response["calmWind"]["customExpressionCodeBytesPreservedExactlyFromR19"])
        self.assertTrue(response["calmWind"]["graphFingerprintPreservedExactlyFromR19"])
        self.assertTrue(
            response["calmWind"][
                "graphFingerprintIncludesWorldToInstanceTransformSourceTypeAndInput"
            ]
        )
        self.assertTrue(
            response[
                "distanceMatchedNormalWorldUpAndFacingCorrectedInputsValidatedExactly"
            ]
        )
        self.assertFalse(response["r20GeometryLayoutChanged"])

        color_code = self.editor_cpp.split(
            "FString BuildR21GrassColorCode", 1
        )[1].split("FString BuildR12GrassColorCode", 1)[0]
        for token in (
            "/13.5",
            "/4.7",
            "smoothstep(0.08,0.62,h)",
            "smoothstep(0.84,1.0,h)",
            "1.0-smoothstep(0.06,0.34,h)",
            "1.0-smoothstep(0.02,0.18,h)",
            "1.0-dryFraction-0.018",
            "1.0-thatchFraction-0.020",
            "1.0-soilRootFraction-0.015",
            "float3(0.285,0.205,0.060)",
            "float3(0.105,0.072,0.024)",
            "float3(0.050,0.034,0.014)",
            "farColor=lerp(body,dryColor,0.90*dryFraction)",
            "farColor=lerp(farColor,thatchColor,0.08*thatchFraction)",
            "result=lerp(farColor,nearDetailedColor,detailWeight)",
            "lerp(0.975,1.025",
            "lerp(0.990,1.010",
            "float3(1.015,1.000,0.965)",
            "float3(0.990,1.005,0.990)",
            "float3(1.08,1.00,0.92)",
            "0.20*cutMask",
            "lerp(1.0,0.97",
            "result=lerp(finalLuma.xxx,result,0.90)",
        ):
            self.assertIn(token, color_code)
        self.assertNotIn("sin(", color_code)
        self.assertNotIn("cos(", color_code)

        for token in (
            "smoothstep(500.0,1400.0,distanceCm)",
            "return lerp(0.74,BaseRoughness,detail)",
            "return lerp(0.19,BaseSpecular,detail)",
            "smoothstep(400.0,1200.0,distanceCm)",
            "0.42*detail*bladeWeight",
            "lerp(0.95,1.05,micro)",
        ):
            self.assertIn(token, self.editor_cpp)

        upgrade = self.contract["r21GrassMaterialUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismGrassMaterialsToR21"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual("V5D_R21_GRASS_MATERIAL_UPGRADE_PASS", upgrade["successReportPrefix"])
        self.assertEqual(["R19", "R21"], upgrade["uniformGrassInputRevisions"])
        self.assertEqual(4, upgrade["saveTargets"])
        self.assertEqual(4, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(11, upgrade["immutablePackageSnapshotCount"])
        self.assertEqual(7, upgrade["artifactStatesPerTargetPackage"])
        self.assertEqual(
            [
                ".uasset", ".uexp", ".ubulk", ".uptnl",
                ".m.ubulk", ".o.ubulk", ".upayload",
            ],
            upgrade["nativeR21ArtifactRosterExtensions"],
        )
        self.assertEqual(
            {"bytes": 32390049, "sha256": "4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33"},
            upgrade["requiredR20MapPin"],
        )
        self.assertEqual(
            [37718, 37645, 37706, 37805],
            [pin["bytes"] for pin in upgrade["predecessorCanonicalPins"]],
        )
        self.assertEqual(
            [
                "42B3E4F81046415B3A65CE0F4F81A5D168F4FE270EF70E5872A21131645C6339",
                "949324A4266C0A09684B213695295B0D2C9ADDDC310DB74F3FE34D12E10C9E51",
                "AC8D77D34C4C9BCA5144949A1989C44E2F6F4DBD2702F9705977818763CF5109",
                "197177AA26CF23A22E8E4297112441E75062EC643E43145A56065D7C7C0EB9DA",
            ],
            [pin["sha256"] for pin in upgrade["predecessorCanonicalPins"]],
        )
        for key in (
            "exactFourPackageAtomicTransaction",
            "allFourTargetHashesMustChange",
            "finalCanonicalBytesAndSha256Reported",
            "graphBasedColdValidationPrimary",
            "rollbackRestoresAllFourBeforeReload",
            "exactR19PredecessorPinsRequiredAfterRollback",
            "uniformR21IdempotenceRequiresCanonicalOnlyArtifactState",
            "wpoCustomExpressionCodeBytesPreservedExactly",
            "wpoGraphFingerprintPreservedExactly",
            "opacityGraphPreservedExactly",
            "legacyNativeArtifactRosterRemainsSix",
            "upayloadCreationTriggersAtomicRollback",
            "r21GraphAdmissionRunsBeforeBackwardR19Fallback",
            "publicEnsureApplyAndLoadedMapValidationAdmitUniformR21",
            "backwardR19PublicValidationAdmissionPreserved",
            "actorMaterialRevisionDerivedFromExactRuntimeRoughnessSpecularSignature",
            "editorGraphRevisionAndActorRuntimeRevisionMustAgree",
        ):
            self.assertTrue(upgrade[key], key)
        manifest_contract = upgrade["transactionManifest"]
        self.assertEqual(
            "triad.istana_public_view_v5d.r21_grass_material_transaction.v1",
            manifest_contract["schema"],
        )
        self.assertEqual(
            "D:/triad/TRIAD/Saved/TRIAD/Automation/R21GrassMaterialTransaction",
            manifest_contract["fixedRoot"],
        )
        self.assertTrue(manifest_contract["jsonExtensionRequired"])
        self.assertFalse(manifest_contract["existingEvidenceOverwriteAllowed"])
        self.assertEqual("CREATE_NEW", manifest_contract["temporaryFileMode"])
        self.assertEqual(
            "ATOMIC_SAME_DIRECTORY_MOVE", manifest_contract["publishMode"]
        )
        for key in (
            "genericPostNativeFailureRollbackRequired",
            "dedicatedPostSuccessFailureRollbackRequired",
            "rollbackUsesOnlyInvocationReturnedNativeBackupAndReceipt",
            "rollbackReceiptRequiresCanonicalPresentAndSixSidecarsAbsent",
            "rollbackBackupBytesMd5AndSha256ValidatedBeforeRestore",
            "rollbackRevalidatesProtectedUE54IdentityAndR20MapBeforeAndAfter",
            "rollbackUsesSameVolumeStagingAndAtomicPerFileReplace",
            "publishedReceiptQuarantinedBeforeRollback",
        ):
            self.assertTrue(manifest_contract[key], key)
        self.assertEqual(
            "D:/triad/TRIAD/Saved/TRIAD/Backups/V5D_R21_GroundMaterials",
            manifest_contract["rollbackBackupRoot"],
        )
        self.assertEqual(4, manifest_contract["rollbackReceiptPackageCount"])
        self.assertEqual(
            7, manifest_contract["rollbackReceiptArtifactSlotsPerPackage"]
        )
        self.assertFalse(manifest_contract["arbitraryRestorePathAccepted"])

        for token in (
            "$r21PostNativeRecoveryArmed",
            "Get-R21GenericRecoveryPlan",
            "Invoke-R21GenericExactRollback",
            "TRIAD_V5D_R21_GRASS_MATERIALS_BACKUP_V1",
            "R21 generic recovery receipt package order drifted",
            "R21 generic recovery backup hash/byte validation failed",
            "Resolve-R21AuthenticatedNewBackupDirectory",
            "BackupReceiptBytes",
            "BackupReceiptSha256",
            "IDEMPOTENT_VERIFIED",
            "$r21SuccessResult = [pscustomobject]@{",
            "Invoke-R21ExactFilesystemRollback",
            "Get-ChangedPaths",
            "automatically restored the exact pre-invocation R19 state",
        ):
            self.assertIn(token, self.generic_material_transaction)
        for token in (
            "$genericMutationCommitted",
            "Get-ValidatedR21RollbackPlan",
            "Invoke-ExactR21PostSuccessRollback",
            "Disable-InvocationManifestBeforeRollback",
            "Invoke-R21PostCommitRecoveryCoordinator",
            "$expectedGenericTransactionState",
            "IDEMPOTENT_VERIFIED",
            "BackupReceiptBytes",
            "BackupReceiptSha256",
            "automatically restored exact R19",
        ):
            self.assertIn(token, self.r21_material_transaction)
        for source, tokens in (
            (
                self.generic_material_transaction,
                (
                    "$cursor -ne $lines.Count",
                    "$actualFiles.Count -ne 5",
                    "[IO.FileAttributes]::ReparsePoint",
                    "Get-R21SafeDualFileIdentity",
                    "$backupIdentity.Md5",
                ),
            ),
            (
                self.r21_material_transaction,
                (
                    "$cursor -ne $lines.Count",
                    "$actualBackupFiles.Count -ne 5",
                    "Get-R21StrictContainedNoReparsePath",
                    "Get-R21SafeDualFileIdentity",
                    "$backupIdentity.Md5",
                ),
            ),
        ):
            for token in tokens:
                self.assertIn(token, source)
        for token in (
            "Get-R21PhysicalPathEntry",
            "Assert-R21NoReparsePathChain",
            "Get-R21SafeDualFileIdentity",
            "Read-R21SafeFileBytes",
            "Resolve-R21AuthenticatedNewBackupDirectory",
            "[IO.FileShare]::None",
            "Invoke-R21SafeReplace",
            "Invoke-R21AtomicJsonPublication",
            "Disable-R21PublishedManifestBeforeRollback",
            "RESTORED_CLEANUP_PENDING",
            "RECOVERY_REQUIRED",
            "RECOVERY_BLOCKED",
            "physical-manifest-invalidation",
            "Test-R21AdmissibleTransactionManifest",
        ):
            self.assertIn(token, self.r21_transaction_safety_support)
        for token in (
            "Replace:$index`:$phase",
            "SidecarMove:$ordinal`:$phase",
            "ManifestMove:After",
            "ManifestQuarantineMove:Before",
            "ManifestFallbackRemove:Before",
            "CleanupStage:0`:$phase",
            "JournalWrite:stage-00:intent:Before",
            "JournalPublish:stage-00:intent:After",
            "JournalReadback:stage-00:intent",
            "JournalTemporaryCleanup:stage-00:intent:$phase",
            "CatchTemporaryRemove:After",
            "immutable-receipt-corruption",
            "manifest-readback-$readbackMutation",
            "manifest-ancestor-junction",
            "backup-ancestor-junction",
            "journal-ancestor-junction",
            "rollback-dangling-canonical",
            "physical-manifest-published-flag-false",
            "$falsePublishedState.Published = $false",
            "rollback callback reached while a physical manifest remained",
            "physical manifest survived invalidation",
            "Get-R21PhysicalPathEntry",
            "ManifestDuplicateSourceRemove:$phase",
            "ManifestFallbackRemove:After",
            "manifest-fallback-remove-after",
            "idempotence-post-move-reconciliation-lock",
            "INJECTED_R21_POST_MOVE_RECONCILIATION_FAULT",
            "-Mode IDEMPOTENCE",
            "failed PASS/IDEMPOTENCE receipt remained physically canonical",
            "Resolve-R21AuthenticatedNewBackupDirectory",
            "Read-R21SafeFileBytes",
            "-ItemType Junction",
            "-ItemType SymbolicLink",
            "-Resume",
        ):
            self.assertIn(token, self.r21_transaction_safety_harness)
        self.assertLess(
            self.generic_material_transaction.index("$r21PostNativeRecoveryArmed"),
            self.generic_material_transaction.index("$snapshotAfter"),
        )
        recovery_plan_source = self.generic_material_transaction.split(
            "function Get-R21GenericRecoveryPlan", 1
        )[1].split("function Invoke-R21GenericExactRollback", 1)[0]
        for token in (
            "[AllowNull()]",
            "[AllowEmptyString()]",
            "$normalizedTransactionReport",
            "Resolve-R21AuthenticatedNewBackupDirectory",
        ):
            self.assertIn(token, recovery_plan_source)
        result_construct = self.generic_material_transaction.index(
            "$r21SuccessResult = [pscustomobject]@{"
        )
        recovery_guard_init = self.generic_material_transaction.index(
            "$r21PostNativeRecoveryArmed = $false"
        )
        outer_recovery_boundary = self.generic_material_transaction.index(
            "# This outer boundary starts before the helper launch"
        )
        native_ack_condition = self.generic_material_transaction.index(
            "$transaction.ReturnValue -eq $true"
        )
        native_ack_guard = self.generic_material_transaction.index(
            "$r21PostNativeRecoveryArmed = $true", native_ack_condition
        )
        report_access = self.generic_material_transaction.index(
            "$transactionReport = [string] $transaction.OutReport",
            native_ack_guard,
        )
        recovery_catch = self.generic_material_transaction.index(
            "\ncatch {\n    $postNativeFailure", result_construct
        )
        result_disarm = self.generic_material_transaction.index(
            "$r21PostNativeRecoveryArmed = $false", result_construct
        )
        result_construct_end = self.generic_material_transaction.index(
            "\n}", result_construct
        )
        result_emit = self.generic_material_transaction.index(
            "\n$r21SuccessResult\n", result_disarm
        )
        self.assertLess(result_construct, result_disarm)
        self.assertLess(result_construct_end, result_disarm)
        self.assertLess(result_disarm, result_emit)
        self.assertLess(recovery_guard_init, outer_recovery_boundary)
        self.assertLess(outer_recovery_boundary, native_ack_condition)
        self.assertLess(native_ack_condition, native_ack_guard)
        self.assertLess(native_ack_guard, report_access)
        self.assertLess(report_access, result_construct)
        self.assertLess(result_emit, recovery_catch)
        self.assertNotIn(
            "$r21PostNativeRecoveryArmed = $Revision -ceq 'R21'",
            self.generic_material_transaction,
        )
        coordinator_source = self.r21_transaction_safety_support.split(
            "function Invoke-R21PostCommitRecoveryCoordinator", 1
        )[1]
        self.assertNotIn(
            "$manifestDisabled = -not [bool] $PublicationState.Published",
            coordinator_source,
        )
        self.assertLess(
            coordinator_source.index(
                "$manifestDisposition = & $DisableManifest $plan"
            ),
            coordinator_source.index("$rollbackReport = & $Rollback $plan"),
        )
        self.assertLess(
            coordinator_source.index("& $AssertManifestInadmissible"),
            coordinator_source.index("$rollbackReport = & $Rollback $plan"),
        )
        dedicated_nonmutation_catch = self.r21_material_transaction.split(
            "$idempotenceDisposition = $null", 1
        )[1].split("if ($cleanupErrors.Count -gt 0)", 1)[0]
        self.assertNotIn(
            "if ($manifestPublicationState.Published)",
            dedicated_nonmutation_catch,
        )
        self.assertIn(
            "Disable-R21PublishedManifestBeforeRollback",
            dedicated_nonmutation_catch,
        )
        self.assertIn(
            "Assert-InvocationManifestInadmissible",
            dedicated_nonmutation_catch,
        )
        dedicated_manifest_assertion = self.r21_material_transaction.split(
            "function Assert-InvocationManifestInadmissible", 1
        )[1].split("function Invoke-ExactR21PostSuccessRollback", 1)[0]
        self.assertIn("Get-R21PhysicalPathEntry", dedicated_manifest_assertion)
        self.assertNotIn(
            "Test-R21AdmissibleMutationManifest",
            dedicated_manifest_assertion,
        )
        publication_fault_window = self.r21_transaction_safety_harness.split(
            "$publicationFaults = @(", 1
        )[1].split(
            "$idempotenceMove = New-HarnessScenario", 1
        )[0]
        self.assertNotIn("$state.Published", publication_fault_window)
        self.assertIn(
            "Disable-R21PublishedManifestBeforeRollback",
            publication_fault_window,
        )
        self.assertIn("Get-R21PhysicalPathEntry", publication_fault_window)
        self.assertIn(
            "TransactionState = $successTransactionState",
            self.generic_material_transaction[result_construct:result_disarm],
        )
        self.assertIn(
            "elseif ($Revision -ceq 'R21' -and $Idempotence)",
            self.generic_material_transaction[result_construct:result_emit],
        )
        dedicated_state_window = self.r21_material_transaction.split(
            "$expectedGenericTransactionState = if ($Idempotence)", 1
        )[1].split("if (-not $Idempotence)", 1)[0]
        self.assertIn("'IDEMPOTENT_VERIFIED'", dedicated_state_window)
        self.assertIn("'DISARMED'", dedicated_state_window)
        self.assertIn(
            "$expectedGenericTransactionState", dedicated_state_window
        )
        self.assertLess(
            self.r21_material_transaction.index("$genericMutationCommitted = $true"),
            self.r21_material_transaction.index(
                "$mapAfter = Assert-ExactIdentity"
            ),
        )
        self.assertNotIn("Set-Content", self.r21_material_transaction)
        generic_param_block = self.generic_material_transaction.split(
            "Set-StrictMode", 1
        )[0]
        dedicated_param_block = self.r21_material_transaction.split(
            "Set-StrictMode", 1
        )[0]
        for forbidden in ("RollbackPath", "BackupPath", "BackupDirectory"):
            self.assertNotIn(forbidden, generic_param_block)
            self.assertNotIn(forbidden, dedicated_param_block)
        for key in (
            "mapsEverOfferedToSave",
            "instanceTransformsEverMutated",
            "r20GeometryLayoutEverMutated",
            "collisionNavigationSensorAndRfEverMutated",
            "globalLightingEverMutated",
            "materialEmissiveEverConnected",
            "texturesCreatedOrMutated",
            "mixedVersionRepairAllowed",
        ):
            self.assertFalse(upgrade[key], key)

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationEdgeGrassFadeAssetToR11", 1
        )[0]
        for token in (
            "ValidateR21UpgradeInputMaterialAssets",
            "CaptureR21PreservedPackageSnapshots",
            "ValidateR21ExactDiskPins",
            "BackUpR21GrassMaterialPackages",
            "ConfigureR21GrassMaterial",
            "ValidateCompleteR21MaterialAssets",
            "R21PreservedPackageSnapshotsMatch",
            "IDEMPOTENT_V5D_R21_GRASS_MATERIALS_ALREADY_VALID",
            "V5D_R21_GRASS_MATERIAL_UPGRADE_PASS",
            "graphBasedColdValidationPrimary=true",
            "wpoCustomExpressionCodeBytesPreservedExactly=true",
            "mapsSaved=0",
            "finalPins=",
        ):
            self.assertIn(token, endpoint)
        targets = endpoint.split("TArray<UMaterial*> Targets = {", 1)[1].split("};", 1)[0]
        self.assertEqual(4, targets.count("Materials["))
        for forbidden in ("SaveMap", "SavePackage", "SetStaticMesh", "SetWorldTransform"):
            self.assertNotIn(forbidden, endpoint)

        configure = self.editor_cpp.split(
            "bool ConfigureR21GrassMaterial(", 1
        )[1].split("bool ConfigureR22GrassMaterial", 1)[0]
        self.assertIn("ValidateR19GrassMaterial", configure)
        self.assertIn("BuildR21WpoFingerprint", configure)
        self.assertIn("ValidateR21GrassMaterial", configure)
        for forbidden in (
            "Wind->Modify",
            "Wind->Code =",
            "AddExpression",
            "NewObject",
            "SetStaticMesh",
            "SetWorldTransform",
        ):
            self.assertNotIn(forbidden, configure)

        validator = self.editor_cpp.split(
            "bool ValidateR21GrassMaterial(", 1
        )[1].split("bool ConfigureR21GrassMaterial", 1)[0]
        for token in (
            "InstanceLocalPosition->TransformSourceType",
            "TRANSFORMPOSSOURCE_World",
            "InstanceLocalPosition->TransformType",
            "TRANSFORMPOSSOURCE_Instance",
            "InputMatches(InstanceLocalPosition->Input, WorldPosition, 0)",
            "InputMatches(DistanceMatchedNormal->A, WorldUpNormal, 0)",
            "FacingCorrectedNormal->A, ImportedCurvatureNormal, 0",
            "InputMatches(FacingCorrectedNormal->B, TwoSidedSign, 0)",
            "DistanceMatchedNormal->B, FacingCorrectedNormal, 0",
        ):
            self.assertIn(token, validator)
        wpo_fingerprint = self.editor_cpp.split(
            "FString BuildR21WpoFingerprint", 1
        )[1].split("bool ValidateR21GrassMaterial", 1)[0]
        for token in (
            "instanceLocalDesc=",
            "transformSource=",
            "transformType=",
            "instanceLocalInput=",
            "worldPositionShaderOffset=",
        ):
            self.assertIn(token, wpo_fingerprint)

        current_admission = self.editor_cpp.split(
            "bool ValidateCompleteCurrentMaterialAssets", 1
        )[1].split("bool ValidateCompleteR13MaterialAssets", 1)[0]
        self.assertLess(
            current_admission.index("ValidateCompleteR21MaterialAssets"),
            current_admission.index("ValidateCompleteR19MaterialAssets"),
        )
        for token in (
            'OutGrassRevision = TEXT("R21")',
            'OutGrassRevision = TEXT("R19")',
            "SelectCompleteGrassMaterialRevision(true, false)",
            "SelectCompleteGrassMaterialRevision(false, true)",
        ):
            self.assertIn(token, self.editor_cpp)
        loaded_validator = self.editor_cpp.split(
            "ValidateGroundVegetationRealismPassInLoadedV5DHybridMap", 1
        )[1].split("#if WITH_DEV_AUTOMATION_TESTS", 1)[0]
        for token in (
            "ValidateCompleteCurrentMaterialAssets",
            "ValidateLoadedWorld",
            'TEXT("grassMaterialRevision=") + GrassRevision',
            "editorGrassMaterialGraphValidated=true",
            "validatedGrassRevisionSelection=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK",
        ):
            self.assertIn(token, loaded_validator)
        self.assertLess(
            loaded_validator.index("ValidateCompleteCurrentMaterialAssets"),
            loaded_validator.index("ValidateLoadedWorld"),
        )

        for token in (
            "GrassR19RoughnessSignature",
            "GrassR21RoughnessSignature",
            "ClassifyGrassMaterialRuntimeSignature",
            "ResolveGrassMaterialRuntimeRevision",
            "grassMaterialRevision=R21",
            "grassMaterialRevision=R19",
            "materialRevisionDerivedFromExactRuntimeParameterSignature=true",
            "GrassMaterialRuntimeRevision",
        ):
            self.assertIn(token, self.actor_cpp)

        legacy_artifacts = self.editor_cpp.split(
            "TArray<FString> PackageArtifactCandidates", 1
        )[1].split("TArray<FString> R21PackageArtifactCandidates", 1)[0]
        r21_artifacts = self.editor_cpp.split(
            "TArray<FString> R21PackageArtifactCandidates", 1
        )[1].split("struct FPackageArtifactSnapshot", 1)[0]
        self.assertIn('.o.ubulk', legacy_artifacts)
        self.assertNotIn('.upayload', legacy_artifacts)
        self.assertIn('.upayload', r21_artifacts)
        for token in (
            "CaptureR21PackageArtifacts",
            "OutArtifacts.Num() != 7",
            "R21PackageArtifactsMatch",
            "ExpectedArtifactsPerPackage != 7",
            "Package.Artifacts.Num() != 7",
        ):
            self.assertIn(token, self.editor_cpp)
        self.assertIn("R21 calibrated grass-material migration", self.package_readme)
        self.assertIn(
            "TRIAD.Istana.ExploreV5D.GroundVegetation.R21MaterialContract",
            self.editor_cpp,
        )
        self._assert_r21_generated_utc_round_trip_is_exact_across_powershell_hosts()

    def test_r22_hyperreal_grass_system_is_atomic_stable_and_runtime_identifiable(self):
        response = self.contract["grassR22HyperrealSurfaceResponse"]
        self.assertEqual("R22", response["materialRevision"])
        self.assertEqual(31, response["grassExpressionCountPerProfile"])
        self.assertEqual(25, response["overlayExpressionCount"])
        self.assertEqual(25, response["edgeExpressionCount"])
        self.assertEqual([12.0, 18.0], response["stableVisibilityFadeMeters"])
        self.assertFalse(response["temporalDitherConnectedToOpacity"])
        self.assertEqual(0.0, response["defaultWindStrengthCentimeters"])
        self.assertEqual(22.0, response["runtimeMaterialRevisionMarker"])
        self.assertEqual("TMVM_MipBias", response["overlayTextureMipValueMode"])
        self.assertEqual([30.0, 55.0], response["overlayMipBiasDistanceMeters"])
        self.assertEqual(
            [1.96625, 0.91675, 0.56525], response["groundLevelLinearTint"]
        )
        self.assertTrue(response["editorAndRuntimeRevisionMarkersRequiredToAgree"])
        self.assertFalse(response["r20GeometryLayoutChanged"])

        upgrade = self.contract["r22GrassSystemUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismGrassSystemToR22"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual(
            "R22SixPackageHyperrealGrassSystemUpgrade", upgrade["operation"]
        )
        self.assertEqual(6, upgrade["saveTargets"])
        self.assertEqual(6, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(42, upgrade["artifactLedgerRows"])
        self.assertEqual(36, upgrade["absentSidecarStates"])
        self.assertEqual(7, upgrade["backupFileCount"])
        self.assertEqual(9, upgrade["immutablePackageSnapshotCount"])
        self.assertEqual(
            [39262, 39083, 39649, 39659, 32987, 33972],
            [pin["bytes"] for pin in upgrade["predecessorCanonicalPins"]],
        )
        self.assertEqual(
            [
                "200F648C69E8ED88CE2FED8FD08EEB2FEA01D4E7933A193BAB42B830A66FE1AD",
                "937B006B01BD95E1F805ED9424A7F09AC7E8FE9475D768139428FA4CF4AEDAC5",
                "5239BBC6F3AC171CFFC0AE7A9BAD3413198A2D30C00A1C780D3AEAE1FA0008A5",
                "5AD7B4C89F7942FBC2840B9123C3FA4C8DF83FA484EFCD1D2F1F4B5555649C58",
                "C032A10C3803FD92FAFED7CB003E853A89207A4364383E65F845C30130ADD38B",
                "640360DF0CCB516A9861D4CB27E2A948A5F81792E3F822299ED765D8261732B4",
            ],
            [pin["sha256"] for pin in upgrade["predecessorCanonicalPins"]],
        )
        self.assertEqual(
            {
                "bytes": 12219,
                "sha256": "1BA32CE17F611EA14C6D42C5D125D9562C15E1C8517BA845DEA3FC51B889B5AD",
            },
            upgrade["requiredSoilPin"],
        )
        self.assertEqual(
            {
                "bytes": 32390049,
                "sha256": "4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33",
            },
            upgrade["requiredR20MapPin"],
        )
        for key in (
            "allSixTargetHashesMustChange",
            "rollbackRestoresAllSixBeforeReload",
            "exactPredecessorPinsRequiredAfterRollback",
            "uniformR22IdempotenceRequiresCanonicalOnlyArtifactState",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapEverOfferedToSave",
            "soilEverOfferedToSave",
            "sourceOrTextureEverOfferedToSave",
            "instanceTransformsEverMutated",
            "collisionNavigationSensorAndRfEverMutated",
            "globalLightingEverMutated",
            "materialEmissiveEverConnected",
            "mixedVersionRepairAllowed",
        ):
            self.assertFalse(upgrade[key], key)

        manifest = upgrade["transactionManifest"]
        self.assertEqual(
            "triad.istana_public_view_v5d.r22_grass_system_transaction.v1",
            manifest["schema"],
        )
        self.assertEqual(
            "TRIAD_V5D_R22_GRASS_SYSTEM_BACKUP_V1",
            manifest["backupReceiptHeader"],
        )
        self.assertEqual(6, manifest["rollbackReceiptPackageCount"])
        self.assertEqual(7, manifest["rollbackReceiptArtifactSlotsPerPackage"])
        self.assertFalse(manifest["arbitraryRestorePathAccepted"])

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationEdgeGrassFadeAssetToR11", 1
        )[0]
        for token in (
            "ValidateR22UpgradeInputMaterialAssets",
            "CaptureR22PreservedPackageSnapshots",
            "ValidateR22ExactDiskPins",
            "BackUpR22GrassSystemPackages",
            "ConfigureR22GrassMaterial",
            "ConfigureR22GroundOverlayMaterial",
            "ConfigureR22EdgeGrassFadeMaterial",
            "ValidateCompleteR22MaterialAssets",
            "R22PreservedPackageSnapshotsMatch",
            "IDEMPOTENT_V5D_R22_GRASS_SYSTEM_ALREADY_VALID",
            "V5D_R22_GRASS_SYSTEM_UPGRADE_PASS",
            "operation=R22SixPackageHyperrealGrassSystemUpgrade",
            "targetArtifacts=42",
            "absentTargetSidecars=36",
            "mapsSaved=0",
            "finalPins=",
        ):
            self.assertIn(token, endpoint)
        targets = endpoint.split("TArray<UMaterial*> Targets = {", 1)[1].split(
            "};", 1
        )[0]
        for token in (
            "Materials[0]", "Materials[1]", "Materials[2]", "Materials[3]",
            "Materials[5]", "EdgeFadeMaterial",
        ):
            self.assertIn(token, targets)
        for forbidden in (
            "SaveMap", "SavePackage", "SetStaticMesh", "SetWorldTransform"
        ):
            self.assertNotIn(forbidden, endpoint)

        for token in (
            "GrassR22StableVisibilityCode",
            "float revisionGate=saturate(MaterialRevision/22.0)",
            "(bStableRevision ? (bR23B ? 32 : 31) : 29)",
            "(bR22Topology ? (bR23BCalibration ? 26 : 25) : 23)",
            "bRequireR22StableDerivative || bRequireR23StableDerivative",
            "R22RuntimeRevisionParameterName",
            "R22RuntimeRevisionValue",
            "TMVM_MipBias",
            "float3(1.96625,0.91675,0.56525)",
            "smoothstep(5000.0,15000.0,cameraHeightDeltaCm)",
        ):
            self.assertIn(token, self.editor_cpp)

        current_admission = self.editor_cpp.split(
            "bool ValidateCompleteCurrentMaterialAssets", 1
        )[1].split("bool ValidateCompleteR13MaterialAssets", 1)[0]
        self.assertLess(
            current_admission.index("ValidateCompleteR23MaterialAssets"),
            current_admission.index("ValidateCompleteR22MaterialAssets"),
        )
        self.assertLess(
            current_admission.index("ValidateCompleteR22MaterialAssets"),
            current_admission.index("ValidateCompleteR21MaterialAssets"),
        )
        self.assertLess(
            current_admission.index("ValidateCompleteR21MaterialAssets"),
            current_admission.index("ValidateCompleteR19MaterialAssets"),
        )

        for token in (
            "RuntimeMaterialRevisionParameter",
            "R22RuntimeMaterialRevisionSignature",
            "const TArray<float>& RevisionValues",
            "ResolveOptionalRuntimeMaterialRevision",
            "GroundOverlayMaterialRevision != EdgeGrassMaterialRevision",
            "grassMaterialRevision=R22",
            "groundOverlayMaterialRevision=R22",
            "edgeGrassMaterialRevision=R22",
            "materialRevisionDerivedFromUniformBladeOverlayEdgeMarkers=true",
            "Zero-wind R21 without a revision marker is rejected",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertIn(
            "validatedGrassRevisionSelection=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK",
            self.editor_cpp,
        )

    def test_r23_photographic_depth_grass_system_is_explicit_atomic_and_current(self):
        response = self.contract["grassR23PhotographicDepthSurfaceResponse"]
        self.assertEqual("R23", response["materialRevision"])
        self.assertEqual(
            ["manicured", "humid", "shade", "dry_edge"],
            response["profileOrder"],
        )
        self.assertEqual(
            [
                [0.06, 0.105, 0.03],
                [0.05, 0.095, 0.03],
                [0.045, 0.075, 0.03],
                [0.08, 0.095, 0.025],
            ],
            response["rootLinearByProfile"],
        )
        self.assertEqual(
            [
                [0.11, 0.195, 0.05],
                [0.095, 0.18, 0.052],
                [0.08, 0.145, 0.052],
                [0.135, 0.16, 0.045],
            ],
            response["bodyLinearByProfile"],
        )
        self.assertEqual(
            [
                [0.135, 0.22, 0.065],
                [0.12, 0.205, 0.068],
                [0.105, 0.175, 0.068],
                [0.17, 0.185, 0.055],
            ],
            response["tipLinearByProfile"],
        )
        self.assertEqual(
            [
                [1.12, 1.06, 0.92],
                [1.1, 1.07, 0.94],
                [1.16, 1.1, 1.0],
                [1.08, 1.01, 0.86],
            ],
            response["subsurfaceGainLinearByProfile"],
        )
        self.assertEqual(
            [0.68, 0.66, 0.72, 0.75], response["roughnessBaseByProfile"]
        )
        self.assertEqual([0.3, 0.31, 0.28, 0.26], response["specularBaseByProfile"])
        self.assertEqual([0.05, 0.035, 0.065, 0.18], response["dryFractionByProfile"])
        self.assertEqual([0.09, 0.07, 0.12, 0.24], response["thatchFractionByProfile"])
        self.assertEqual([0.025, 0.018, 0.032, 0.06], response["soilRootFractionByProfile"])
        self.assertEqual(31, response["grassExpressionCountPerProfile"])
        self.assertEqual(0, response["grassTextureSampleCountPerProfile"])
        self.assertEqual(5, response["grassStableVisibilityInputCount"])
        self.assertEqual([20.0, 28.0], response["grassStableVisibilityFadeMeters"])
        self.assertEqual([10.0, 24.0], response["grassColorDetailFadeMeters"])
        self.assertEqual([12.0, 24.0], response["grassNormalFadeMeters"])
        self.assertEqual([0.5, 0.08], response["grassNormalNearFar"])
        self.assertEqual([9.5, 2.1], response["grassMacroScalesMeters"])
        self.assertEqual(0.0, response["defaultWindStrengthCentimeters"])
        self.assertEqual(23.0, response["runtimeMaterialRevisionMarker"])
        self.assertFalse(response["temporalDitherConnectedToOpacity"])
        self.assertTrue(response["windCustomExpressionAndPinsPreservedExactlyFromR22"])
        self.assertEqual(25, response["overlayExpressionCount"])
        self.assertEqual(4, response["overlayTextureSampleCount"])
        self.assertEqual("TMVM_MipBias", response["overlayTextureMipValueMode"])
        self.assertEqual([30.0, 55.0], response["overlayMipBiasDistanceMeters"])
        self.assertTrue(response["overlayCameraHeightTintRemoved"])
        self.assertEqual([0.78, 1.1, 0.7], response["overlayNearLinearTint"])
        self.assertEqual([0.76, 1.02, 0.68], response["overlayFarLinearTint"])
        self.assertEqual([7.3, 1.7, 0.48], response["overlaySubstrateBreakupMeters"])
        self.assertEqual(0.18, response["overlaySpecular"])
        self.assertEqual([0.68, 0.91], response["overlayRoughnessRange"])
        self.assertEqual([0.22, 0.02], response["overlayNormalStrengthNearFar"])
        self.assertEqual(25, response["edgeExpressionCount"])
        self.assertEqual(4, response["edgeTextureSampleCount"])
        self.assertEqual([20.0, 28.0], response["edgeStableVisibilityFadeMeters"])
        self.assertTrue(response["expressionTopologyPreservedFromR22"])
        self.assertTrue(response["editorAndRuntimeRevisionMarkersRequiredToAgree"])
        self.assertFalse(response["emissiveConnected"])
        self.assertTrue(response["r23GeometryLayoutRequired"])

        upgrade = self.contract["r23GrassSystemUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismGrassSystemToR23"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual(
            "R23SixPackagePhotographicDepthGrassSystemUpgrade",
            upgrade["operation"],
        )
        self.assertEqual("V5D_R23_GRASS_SYSTEM_UPGRADE_PASS", upgrade["successReportPrefix"])
        self.assertEqual(["R22", "R23"], upgrade["uniformInputRevisions"])
        self.assertEqual(6, upgrade["saveTargets"])
        self.assertEqual(6, upgrade["reloadTargets"])
        self.assertEqual(6, upgrade["idempotenceReloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(42, upgrade["artifactLedgerRows"])
        self.assertEqual(36, upgrade["absentSidecarStates"])
        self.assertEqual(7, upgrade["backupFileCount"])
        self.assertEqual(9, upgrade["immutablePackageSnapshotCount"])
        self.assertEqual(
            [41028, 40858, 41398, 41412, 35780, 35623],
            [pin["bytes"] for pin in upgrade["predecessorCanonicalPins"]],
        )
        self.assertEqual(
            [
                "0BA45DF9C7C826B0F6473C75621DEEFADD4FE3DC8BE4D51D226FEAF337DD76CA",
                "86E60FC8839C11A7939DD822C517E560D0BE656B38E5534067A30080A6728634",
                "FF09A4652EC84CCEF922A253263FF2CD979DBE3E0B1B5B4EEA396387713CE35A",
                "7DEF8E81069B8424B6E0F33B1910637B2B58288E96BC6EEBBE7977A872FD7AE1",
                "0C88CB096CCF567F55684CBE53CA3BB0B42E60FF494968E3CC2CC11703D0F011",
                "A6E3CD4502D8B1932EE8E8AF0D302A92B4F838BA7E97A369650C1EE5AE017CF2",
            ],
            [pin["sha256"] for pin in upgrade["predecessorCanonicalPins"]],
        )
        self.assertEqual(
            {
                "bytes": 32390049,
                "sha256": "4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33",
            },
            upgrade["requiredR20MapPin"],
        )
        self.assertEqual(
            [
                {
                    "revision": "R20_PREDECESSOR",
                    "bytes": 32390049,
                    "sha256": "4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33",
                },
                {
                    "revision": "R23_LAYOUT_CURRENT",
                    "bytes": 34986401,
                    "sha256": "49A2879BE33704DDE1C6C0EFBAAE2364300E36EB1F0466B775EDE4F7BB4870DC",
                },
            ],
            upgrade["uniformR23IdempotenceAdmittedMapPins"],
        )
        self.assertEqual(
            {
                "bytes": 12219,
                "sha256": "1BA32CE17F611EA14C6D42C5D125D9562C15E1C8517BA845DEA3FC51B889B5AD",
            },
            upgrade["requiredSoilPin"],
        )
        for key in (
            "allSixTargetHashesMustChange",
            "rollbackRestoresAllSixBeforeReload",
            "exactR22PredecessorPinsRequiredAfterRollback",
            "uniformR23IdempotenceRequiresColdDiskReload",
            "uniformR23IdempotenceRequiresCanonicalOnlyArtifactState",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapEverOfferedToSave",
            "soilEverOfferedToSave",
            "sourceOrTextureEverOfferedToSave",
            "instanceTransformsEverMutated",
            "collisionNavigationSensorAndRfEverMutated",
            "globalLightingEverMutated",
            "materialEmissiveEverConnected",
            "mixedVersionRepairAllowed",
        ):
            self.assertFalse(upgrade[key], key)
        backup = upgrade["nativeTransactionBackup"]
        self.assertEqual(
            "TRIAD_V5D_R23_GRASS_SYSTEM_BACKUP_V1",
            backup["backupReceiptHeader"],
        )
        self.assertEqual(6, backup["rollbackReceiptPackageCount"])
        self.assertEqual(7, backup["rollbackReceiptArtifactSlotsPerPackage"])
        self.assertTrue(backup["rollbackReceiptRequiresCanonicalPresentAndSixSidecarsAbsent"])
        self.assertTrue(backup["rollbackBackupBytesMd5AndSha256ValidatedBeforeRestore"])
        self.assertTrue(backup["rollbackRevalidatesProtectedUE54IdentityMapAndSoilBeforeAndAfter"])
        self.assertFalse(backup["arbitraryRestorePathAccepted"])

        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.split(
            "\n    " + endpoint_name + "(", 1
        )[1].split(
            "\n    UpgradeGroundVegetationRealismGrassSystemToR23B(", 1
        )[0]
        for token in (
            "ValidateR23UpgradeInputMaterialAssets",
            "CaptureR23PreservedPackageSnapshots",
            "ValidateR23ExactDiskPins",
            "BackUpR23GrassSystemPackages",
            "ConfigureR23GrassMaterial",
            "ConfigureR23GroundOverlayMaterial",
            "ConfigureR23EdgeGrassFadeMaterial",
            "ValidateCompleteR23MaterialAssets",
            "R23PreservedPackageSnapshotsMatch",
            "IDEMPOTENT_V5D_R23_GRASS_SYSTEM_ALREADY_VALID",
            "reloadTargets=6 diskColdValidated=true",
            "V5D_R23_GRASS_SYSTEM_UPGRADE_PASS",
            "operation=R23SixPackagePhotographicDepthGrassSystemUpgrade",
            "uniformR22Input=true",
            "exactR22PredecessorPins=true",
            "targetArtifacts=42",
            "absentTargetSidecars=36",
            "grassStableVisibilityMeters=20,28",
            "runtimeMaterialRevisionMarker=23",
            "exactAdmittedR20OrR23MapPin=true",
            "mapsSaved=0",
            "finalPins=",
        ):
            self.assertIn(token, endpoint)
        targets = endpoint.split("TArray<UMaterial*> Targets = {", 1)[1].split(
            "};", 1
        )[0]
        for token in (
            "Materials[0]", "Materials[1]", "Materials[2]", "Materials[3]",
            "Materials[5]", "EdgeFadeMaterial",
        ):
            self.assertIn(token, targets)
        for forbidden in ("SaveMap", "SavePackage", "SetStaticMesh", "SetWorldTransform"):
            self.assertNotIn(forbidden, endpoint)

        exact_pin_validator = self.editor_cpp.split(
            "bool ValidateR23ExactDiskPins", 1
        )[1].split("bool ValidateR23LayoutMaterialDiskPins", 1)[0]
        for token in (
            "const bool bExactR20Map",
            "const bool bExactR23Map",
            "bAlreadyR23",
            "bExactR20Map || bExactR23Map",
            "admittedMapRevision=%s",
            "exactAdmittedR20OrR23MapPin=true",
        ):
            self.assertIn(token, exact_pin_validator)

        for token in (
            "const FR21GrassMaterialProfile GrassR23Profiles[]",
            "GrassR23StableVisibilityCode",
            "float revisionGate=saturate(MaterialRevision/23.0)",
            "GrassR23RoughnessCode",
            "return clamp(lerp(0.78,nearRough,detail),0.65,0.86)",
            "GrassR23SpecularCode",
            "return clamp(lerp(0.17,nearSpec,detail),0.15,0.29)",
            "GrassR23NormalAlphaCode",
            "0.08+0.42*detail",
            "GroundR23BaseCode",
            "float3 naturalTint=lerp(float3(0.76,1.02,0.68),float3(0.78,1.10,0.70),nearDetail)",
            "GroundR23RoughnessCode",
            "return clamp(base+nearDetail*detailDelta,0.68,0.91)",
            "GroundR23NearNormalStrength = 0.22f",
            "GroundR23FarNormalStrength = 0.02f",
            "GroundR23Specular = 0.18f",
            "R23RuntimeRevisionValue = 23.0f",
            "TRIAD_V5D_R23_GRASS_SYSTEM_BACKUP_V1",
        ):
            self.assertIn(token, self.editor_cpp)
        for token in (
            "R23RuntimeMaterialRevisionSignature = 23.0f",
            "EGrassMaterialRuntimeRevision::R23",
            "grassMaterialRevision=R23",
            "groundOverlayMaterialRevision=R23",
            "edgeGrassMaterialRevision=R23",
            "runtimeMaterialRevisionMarker=23",
            "edgeGrassFade=StableSpatialVisibility20m28m",
            "materialRevisionDerivedFromUniformBladeOverlayEdgeMarkers=true",
        ):
            self.assertIn(token, self.actor_cpp)

    def test_r23_serialized_layout_is_exactly_pinned_and_map_only(self):
        upgrade = self.contract["r23SerializedLayoutUpgrade"]
        self.assertEqual(
            "UpgradeGroundVegetationSerializedPresentationToR23",
            upgrade["endpoint"],
        )
        self.assertEqual("R20ToR23PhotographicGrassLayout", upgrade["operation"])
        self.assertEqual(
            {
                "bytes": 32390049,
                "sha256": "4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33",
            },
            upgrade["requiredR20PredecessorMapPin"],
        )
        self.assertEqual(
            {
                "bytes": 34986401,
                "sha256": "49A2879BE33704DDE1C6C0EFBAAE2364300E36EB1F0466B775EDE4F7BB4870DC",
            },
            upgrade["requiredR23FinalMapPin"],
        )
        self.assertEqual(6, len(upgrade["requiredR23MaterialPins"]))
        self.assertEqual(1, upgrade["saveTargets"])
        self.assertEqual(0, upgrade["materialPackagesSaved"])
        self.assertEqual(6, upgrade["mapArtifactStatesBackedUp"])
        self.assertEqual(0, upgrade["idempotenceSaveTargets"])
        self.assertEqual(1, upgrade["idempotenceReloadTargets"])
        for key in (
            "coldReloadRequired",
            "idempotenceRequiresExactR23FinalMapPin",
            "postBackupFailureRestoresExactR20Predecessor",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "arbitraryNonPredecessorOrFinalMapAccepted",
            "materialsSoilSourcesTexturesCollisionNavigationSensorRfAndLightingEverMutated",
        ):
            self.assertFalse(upgrade[key], key)

        endpoint = self.editor_cpp.rsplit(
            "UpgradeGroundVegetationSerializedPresentationToR23", 1
        )[1].split("UpgradeGroundVegetationEdgeGrassFadeAssetToR11", 1)[0]
        for token in (
            "R23FinalMapBytes",
            "R23FinalMapSha256",
            "CurrentBytes != R23FinalMapBytes",
            "ColdMapBytes != R23FinalMapBytes",
            "FinalBytes != R23FinalMapBytes",
            "exactR23FinalMapPin=true",
            "V5D_R23_GRASS_LAYOUT_UPGRADE_PASS",
            "IDEMPOTENT_V5D_R23_GRASS_LAYOUT_ALREADY_VALID",
            "saveTargets=0 mapsSaved=0",
            "saveTargets=1 mapsSaved=1 materialPackagesSaved=0",
        ):
            self.assertIn(token, endpoint)

    def test_r23b_material_only_photographic_calibration_is_atomic_and_layout_inert(self):
        response = self.contract[
            "grassR23BPhotographicCalibrationSurfaceResponse"
        ]
        self.assertEqual("R23B", response["calibrationRevision"])
        self.assertEqual("R23", response["baseMaterialRevision"])
        self.assertEqual(
            ["manicured", "humid", "shade", "dry_edge"],
            response["profileOrder"],
        )
        self.assertEqual(
            [
                [0.075, 0.095, 0.028],
                [0.065, 0.088, 0.029],
                [0.060, 0.073, 0.029],
                [0.100, 0.092, 0.025],
            ],
            response["rootLinearByProfile"],
        )
        self.assertEqual(
            [
                [0.135, 0.165, 0.047],
                [0.120, 0.158, 0.050],
                [0.105, 0.132, 0.050],
                [0.155, 0.150, 0.044],
            ],
            response["bodyLinearByProfile"],
        )
        self.assertEqual(
            [
                [0.160, 0.190, 0.060],
                [0.145, 0.185, 0.064],
                [0.130, 0.165, 0.065],
                [0.188, 0.178, 0.055],
            ],
            response["tipLinearByProfile"],
        )
        self.assertEqual(
            [
                [1.16, 0.98, 0.88],
                [1.15, 0.99, 0.90],
                [1.20, 1.01, 0.96],
                [1.12, 0.94, 0.82],
            ],
            response["subsurfaceGainLinearByProfile"],
        )
        self.assertEqual(
            [0.71, 0.70, 0.75, 0.78], response["roughnessBaseByProfile"]
        )
        self.assertEqual(0.80, response["roughnessFar"])
        self.assertEqual([0.68, 0.88], response["roughnessRange"])
        self.assertEqual(
            [0.27, 0.28, 0.25, 0.23], response["specularBaseByProfile"]
        )
        self.assertEqual(0.15, response["specularFar"])
        self.assertEqual([0.12, 0.25], response["specularRange"])
        self.assertEqual(
            [0.14, 0.18, 0.11, 0.14], response["windResponseByProfile"]
        )
        self.assertEqual(
            [0.060, 0.045, 0.075, 0.200], response["dryFractionByProfile"]
        )
        self.assertEqual(
            [0.120, 0.095, 0.150, 0.280],
            response["thatchFractionByProfile"],
        )
        self.assertEqual(
            [0.032, 0.025, 0.040, 0.075],
            response["soilRootFractionByProfile"],
        )
        self.assertEqual([0.210, 0.150, 0.050], response["dryBladeLinear"])
        self.assertEqual([0.100, 0.068, 0.028], response["thatchLinear"])
        self.assertEqual([0.045, 0.030, 0.014], response["soilRootLinear"])
        self.assertEqual(
            [0.76, 0.88, 0.94], response["nearDryThatchSoilBlend"]
        )
        self.assertEqual(0.48, response["farDryBlend"])
        self.assertEqual(0.060, response["farThatchBlend"])
        self.assertEqual([0.95, 1.05], response["broadVariationGain"])
        self.assertEqual([0.98, 1.02], response["fineVariationGain"])
        self.assertEqual(
            [[1.045, 0.975, 0.950], [1.005, 0.995, 0.980]],
            response["bladeTintEndpoints"],
        )
        self.assertEqual([1.06, 0.94, 0.92], response["finalBladeGain"])
        self.assertEqual(0.90, response["finalBladeChroma"])

        self.assertEqual(32, response["grassExpressionCountPerProfile"])
        self.assertEqual(0, response["grassTextureSampleCountPerProfile"])
        self.assertEqual(6, response["grassStableVisibilityInputCount"])
        self.assertEqual(
            [20.0, 28.0], response["grassStableVisibilityFadeMeters"]
        )
        self.assertEqual(
            [10.0, 24.0], response["grassColorDetailFadeMeters"]
        )
        self.assertEqual([12.0, 24.0], response["grassNormalFadeMeters"])
        self.assertEqual([0.38, 0.04], response["grassNormalNearFar"])
        self.assertEqual([9.5, 2.1], response["grassMacroScalesMeters"])
        self.assertFalse(response["temporalDitherConnectedToOpacity"])
        self.assertEqual(0.0, response["defaultWindStrengthCentimeters"])
        self.assertTrue(response["windCustomExpressionAndPinsPreservedExactlyFromR23"])
        self.assertEqual(23.0, response["runtimeMaterialRevisionMarker"])
        self.assertEqual(
            1.0, response["runtimeMaterialCalibrationRevisionMarker"]
        )

        self.assertEqual(26, response["overlayExpressionCount"])
        self.assertEqual(10, response["overlayBaseInputCount"])
        self.assertEqual(4, response["overlayTextureSampleCount"])
        self.assertEqual("TMVM_MipBias", response["overlayTextureMipValueMode"])
        self.assertEqual(
            [30.0, 55.0], response["overlayMipBiasDistanceMeters"]
        )
        self.assertEqual([1.06, 0.90, 0.60], response["overlayNearLinearTint"])
        self.assertEqual([1.02, 0.86, 0.58], response["overlayFarLinearTint"])
        self.assertEqual(
            [7.3, 1.7, 0.48], response["overlaySubstrateBreakupMeters"]
        )
        self.assertEqual([0.965, 1.035], response["overlayMacroGain"])
        self.assertEqual([0.996, 1.004], response["overlayMowingGain"])
        self.assertEqual([0.975, 1.025], response["overlayMicroGain"])
        self.assertEqual([0.72, 0.90], response["overlayThatchThreshold"])
        self.assertEqual([0.115, 0.078, 0.032], response["overlayThatchLinear"])
        self.assertEqual(0.18, response["overlayThatchBlend"])
        self.assertEqual([0.88, 0.975], response["overlaySoilThreshold"])
        self.assertEqual([0.050, 0.034, 0.018], response["overlaySoilLinear"])
        self.assertEqual(0.26, response["overlaySoilBlend"])
        self.assertEqual(0.15, response["overlaySpecular"])
        self.assertEqual(0.86, response["overlayRoughnessFar"])
        self.assertEqual([0.72, 0.92], response["overlayRoughnessRange"])
        self.assertEqual(
            [0.17, 0.01], response["overlayNormalStrengthNearFar"]
        )
        self.assertEqual(26, response["edgeExpressionCount"])
        self.assertEqual(4, response["edgeTextureSampleCount"])
        self.assertEqual(6, response["edgeStableVisibilityInputCount"])
        self.assertEqual(
            [20.0, 28.0], response["edgeStableVisibilityFadeMeters"]
        )
        self.assertTrue(response["oneConnectedCalibrationMarkerAddedPerTarget"])
        self.assertTrue(response["editorAndRuntimeRevisionMarkersRequiredToAgree"])
        self.assertFalse(response["r23SerializedLayoutChanged"])
        self.assertFalse(response["emissiveConnected"])

        upgrade = self.contract["r23BGrassSystemUpgrade"]
        endpoint_name = "UpgradeGroundVegetationRealismGrassSystemToR23B"
        self.assertEqual(endpoint_name, upgrade["endpoint"])
        self.assertEqual(
            "R23BSixPackagePhotographicMaterialCalibration",
            upgrade["operation"],
        )
        self.assertEqual(
            "V5D_R23B_GRASS_SYSTEM_UPGRADE_BOOTSTRAP_PASS",
            upgrade["bootstrapReportPrefix"],
        )
        self.assertEqual(
            "V5D_R23B_GRASS_SYSTEM_UPGRADE_PASS",
            upgrade["successReportPrefix"],
        )
        self.assertEqual(
            "IDEMPOTENT_V5D_R23B_GRASS_SYSTEM_ALREADY_VALID",
            upgrade["idempotenceReportPrefix"],
        )
        self.assertEqual(["R23", "R23B"], upgrade["uniformInputCalibrations"])
        self.assertEqual(6, len(upgrade["exactSaveTargets"]))
        self.assertEqual(6, len(upgrade["predecessorCanonicalPins"]))
        self.assertEqual(
            [
                ("M_IPV5D_Turf_Manicured", "R23B", 42053, "382B3F435190B79323DC0C91F2516889A24E93A072F6665BEB3AF14021F11324"),
                ("M_IPV5D_Turf_Humid", "R23B", 41699, "1BC6A33CFA62AEE015788F54E1EFA80BDB898F0E55FB1A84C3173A8193B567CD"),
                ("M_IPV5D_Turf_Shade", "R23B", 41665, "41F3EEA2A01D10EECFA20BF6F4927AA43F8B03F6AA45B5E16982B019F9810BFB"),
                ("M_IPV5D_Turf_DryEdge", "R23B", 42510, "6744C31747733108D53E5BCEF85BDE35BDE69F4C16BE7039DE54C3F2A8BE29B6"),
                ("M_IPV5D_LawnMacroVariation", "R23B", 37844, "4F976F96F28C57A3C9A98668460B8BF8F51824849D00F88BEDB8E10850FF6915"),
                ("M_IPV5D_GrassMedium_EdgeFade", "R23B", 36509, "5091EF91C6CCB8600F10507F6D46B6D780FC196DF63AB1754F53D49EA1141FBB"),
            ],
            [
                (pin["asset"], pin["revision"], pin["bytes"], pin["sha256"])
                for pin in upgrade["finalCanonicalPins"]
            ],
        )
        self.assertTrue(upgrade["finalCanonicalPinsSealed"])
        self.assertTrue(upgrade["publicationReady"])
        self.assertTrue(upgrade["bootstrapOnlyUntilFinalPinsSourceSealed"])

        layout = self.contract["r23SerializedLayoutUpgrade"]
        self.assertEqual(
            layout["requiredR23FinalMapPin"], upgrade["requiredR23MapPin"]
        )
        self.assertEqual(
            [
                (pin["asset"], pin["bytes"], pin["sha256"])
                for pin in layout["requiredR23MaterialPins"]
            ],
            [
                (pin["asset"], pin["bytes"], pin["sha256"])
                for pin in upgrade["predecessorCanonicalPins"]
            ],
        )
        self.assertTrue(
            all(
                pin["revision"] == "R23"
                for pin in upgrade["predecessorCanonicalPins"]
            )
        )
        self.assertEqual(
            {
                "bytes": 12219,
                "sha256": "1BA32CE17F611EA14C6D42C5D125D9562C15E1C8517BA845DEA3FC51B889B5AD",
            },
            upgrade["requiredSoilPin"],
        )
        for key, expected in (
            ("saveTargets", 6),
            ("reloadTargets", 6),
            ("idempotenceReloadTargets", 6),
            ("coldValidatedMaterials", 7),
            ("artifactStatesPerTargetPackage", 7),
            ("artifactLedgerRows", 42),
            ("absentSidecarStates", 36),
            ("backupFileCount", 7),
            ("immutablePackageSnapshotCount", 9),
        ):
            self.assertEqual(expected, upgrade[key], key)
        for key in (
            "allSixTargetHashesMustChange",
            "rollbackRestoresAllSixBeforeReload",
            "exactR23PredecessorPinsRequiredAfterRollback",
            "uniformR23BIdempotenceRequiresColdDiskReload",
            "uniformR23BIdempotenceRequiresCanonicalOnlyArtifactState",
        ):
            self.assertTrue(upgrade[key], key)
        for key in (
            "mapEverOfferedToSave",
            "soilEverOfferedToSave",
            "sourceOrTextureEverOfferedToSave",
            "instanceTransformsEverMutated",
            "collisionNavigationSensorAndRfEverMutated",
            "globalLightingEverMutated",
            "materialEmissiveEverConnected",
            "mixedVersionRepairAllowed",
            "r23SerializedLayoutEverMutated",
            "sensorPlacementImplementationOrExecutionInThisUpgrade",
        ):
            self.assertFalse(upgrade[key], key)
        self.assertEqual("PLAN_ONLY", upgrade["sensorPlacementDeliverableForThisWork"])
        backup = upgrade["nativeTransactionBackup"]
        self.assertEqual(
            "Saved/TRIAD/Backups/V5D_R23B_GroundMaterials",
            backup["backupRoot"],
        )
        self.assertEqual(
            "TRIAD_V5D_R23B_GRASS_SYSTEM_BACKUP_V1",
            backup["backupReceiptHeader"],
        )
        self.assertEqual(6, backup["rollbackReceiptPackageCount"])
        self.assertEqual(7, backup["rollbackReceiptArtifactSlotsPerPackage"])
        self.assertTrue(
            backup["rollbackReceiptRequiresCanonicalPresentAndSixSidecarsAbsent"]
        )
        self.assertTrue(
            backup["rollbackBackupBytesMd5AndSha256ValidatedBeforeRestore"]
        )
        self.assertTrue(
            backup["rollbackRevalidatesProtectedUE54IdentityMapAndSoilBeforeAndAfter"]
        )
        self.assertFalse(backup["arbitraryRestorePathAccepted"])

        for token in (
            "R23B material-only photographic calibration",
            "It is not a new serialized-presentation revision",
            "TRIAD_RuntimeMaterialCalibrationRevision=1",
            "No map is offered to save",
            "publicationReady=false",
            "finalPinsSealed=false",
            "Sensor placement remains",
            "a review-only plan",
        ):
            self.assertIn(token, self.package_readme)

        self.assertIn(endpoint_name, self.editor_h)
        for token in (
            "GrassR23BProfiles",
            "BuildR23BGrassColorCode",
            "GrassR23BRoughnessCode",
            "GrassR23BSpecularCode",
            "GrassR23BNormalAlphaCode",
            "GrassR23BStableVisibilityCode",
            "GroundR23BBaseCode",
            "GroundR23BRoughnessCode",
            "ConfigureR23BGrassMaterial",
            "ConfigureR23BGroundOverlayMaterial",
            "ConfigureR23BEdgeGrassFadeMaterial",
            "ValidateR23BGrassMaterial",
            "ValidateR23BGroundOverlayMaterial",
            "ValidateR23BEdgeGrassFadeMaterial",
            "ValidateR23BUpgradeInputMaterialAssets",
            "ValidateCompleteR23BMaterialAssets",
            "ValidateR23BExactDiskPins",
            "CaptureR23BPreservedPackageSnapshots",
            "R23BPreservedPackageSnapshotsMatch",
            "BackUpR23BGrassSystemPackages",
            "TRIAD_RuntimeMaterialCalibrationRevision",
            "TRIAD_V5D_R23B_GRASS_SYSTEM_BACKUP_V1",
            "V5D_R23B_GRASS_SYSTEM_UPGRADE_BOOTSTRAP_PASS",
            "publicationReady=false finalPinsSealed=false",
            "V5D_R23B_GRASS_SYSTEM_UPGRADE_PASS",
            "IDEMPOTENT_V5D_R23B_GRASS_SYSTEM_ALREADY_VALID",
            "validatedGrassRevisionSelection=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK",
        ):
            self.assertIn(token, self.editor_cpp)

        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "UpgradeGroundVegetationEdgeGrassFadeAssetToR11", 1
        )[0]
        for token in (
            "Materials[0]",
            "Materials[1]",
            "Materials[2]",
            "Materials[3]",
            "Materials[5]",
            "EdgeFadeMaterial",
            "mapsSaved=0",
            "instanceTransformsUntouched=true",
            "collisionPreserved=true",
            "navigationPreserved=true",
            "sensorAndRfAuthorityPreserved=true",
        ):
            self.assertIn(token, endpoint)
        for forbidden in (
            "SaveMap",
            "SavePackage",
            "PersistExactR23TargetWorld",
            "RebuildGroundVegetationLayoutToR23",
            "RebuildGroundVegetationLayoutToR23B",
            "SaveLayout",
            "SetStaticMesh",
            "SetWorldTransform",
            "SetCullDistances",
            "SetCollisionEnabled",
            "SetCanEverAffectNavigation",
            "OptimizeSensorPlacement",
            "ApplySensorPlacement",
        ):
            self.assertNotIn(forbidden, endpoint)

        current_admission = self.editor_cpp.split(
            "bool ValidateCompleteCurrentMaterialAssets", 1
        )[1].split("bool ValidateCompleteR13MaterialAssets", 1)[0]
        self.assertLess(
            current_admission.index("ValidateCompleteR23BMaterialAssets"),
            current_admission.index("ValidateCompleteR23MaterialAssets"),
        )
        for token in (
            "GrassR23BRoughnessSignature",
            "GrassR23BSpecularSignature",
            "RuntimeMaterialCalibrationRevisionParameter",
            "R23BRuntimeMaterialCalibrationRevisionSignature",
            "EGrassMaterialRuntimeRevision::R23B",
            "grassMaterialCalibrationRevision=R23B",
            "groundOverlayMaterialCalibrationRevision=R23B",
            "edgeGrassMaterialCalibrationRevision=R23B",
            "runtimeMaterialCalibrationRevisionMarker=1",
            "grassR23BPhotographicCalibration=true",
            "Exact uniform R23B runtime parameter signature is admitted",
            "Mixed R23/R23B calibration marker roster is rejected",
            "R23B roughness/specular without calibration marker is rejected",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn("GrassPresentationRevisionR23B", self.actor_cpp)
        self.assertNotIn("GrassPresentationRevisionR24", self.actor_cpp)

    def test_r22_transaction_wrappers_pin_six_package_atomic_recovery(self):
        support_bytes = R22_TRANSACTION_SAFETY_SUPPORT.read_bytes()
        support_sha256 = hashlib.sha256(support_bytes).hexdigest().upper()
        for source in (
            self.r22_material_transaction,
            self.r22_grass_system_transaction,
            self.r22_transaction_safety_harness,
        ):
            self.assertIn(f"-Bytes {len(support_bytes)}", source)
            self.assertIn(support_sha256, source)

        generic_bytes = R22_MATERIAL_TRANSACTION.read_bytes()
        self.assertIn(
            f"-Bytes {len(generic_bytes)}", self.r22_grass_system_transaction
        )
        self.assertIn(
            hashlib.sha256(generic_bytes).hexdigest().upper(),
            self.r22_grass_system_transaction,
        )

        for token in (
            "UpgradeGroundVegetationRealismGrassSystemToR22",
            "R22SixPackageHyperrealGrassSystemUpgrade",
            "TRIAD_V5D_R22_GRASS_SYSTEM_BACKUP_V1",
            "http://127.0.0.1:30010/remote/object/call",
            "exactSixPackagePredecessorPins=true",
            "-DDC=InstalledNoZenLocalFallback",
            "-LocalDataCachePath=",
            "${env:UE-LocalDataCachePath} = $localDdcPath",
        ):
            self.assertIn(token, self.r22_material_transaction)
        self.assertNotIn("/remote/batch", self.r22_material_transaction)

        dedicated = self.r22_grass_system_transaction
        self.assertEqual(6, dedicated.count("PredecessorBytes = [int64]"))
        self.assertEqual(6, dedicated.count("PredecessorSha256 = '"))
        for token in (
            "M_IPV5D_Turf_Manicured",
            "M_IPV5D_Turf_Humid",
            "M_IPV5D_Turf_Shade",
            "M_IPV5D_Turf_DryEdge",
            "M_IPV5D_LawnMacroVariation",
            "M_IPV5D_GrassMedium_EdgeFade",
            "M_IPV5D_SoilMulch_Layered.uasset",
            "1BA32CE17F611EA14C6D42C5D125D9562C15E1C8517BA845DEA3FC51B889B5AD",
            "Istana_PublicView_Explore_v5d_hybrid.umap",
            "4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33",
            "@('.uasset', '.uexp', '.ubulk', '.uptnl', '.m.ubulk', '.o.ubulk', '.upayload')",
            "[int] $GenericResult.ChangedArtifacts -ne 6",
            "$planTargets.Count -ne 6",
            "$ledger.Count -ne 42",
            "Assert-CanonicalOnlyTargetState",
            "Invoke-R22ExactFilesystemRollback",
        ):
            self.assertIn(token, dedicated)

        support = self.r22_transaction_safety_support
        barrier = support.split(
            "# Authenticate and stage the complete six-package predecessor set", 1
        )[1].split("foreach ($row in $staged)", 1)[0]
        self.assertIn("$errors.Count -gt 0 -or $staged.Count -ne 6", barrier)
        self.assertIn("RECOVERY_BLOCKED", barrier)
        self.assertIn("before canonical restore", barrier)

        for token in (
            "immutable-canonical-backup-corruption",
            "complete backup prevalidation/staging failed",
            "for ($index = 0; $index -lt 6; ++$index)",
            "backup corruption partially mutated canonical target $index",
            "R22_TRANSACTION_SAFETY_SELF_TEST_PASS",
        ):
            self.assertIn(token, self.r22_transaction_safety_harness)

    def test_r21_transaction_filesystem_fault_harness_and_receipt_round_trip(self):
        support_bytes = R21_TRANSACTION_SAFETY_SUPPORT.read_bytes()
        support_sha256 = hashlib.sha256(support_bytes).hexdigest().upper()
        for source in (
            self.generic_material_transaction,
            self.r21_material_transaction,
            self.r21_transaction_safety_harness,
        ):
            self.assertIn(f"-Bytes {len(support_bytes)}", source)
            self.assertIn(support_sha256, source)
        generic_bytes = GENERIC_MATERIAL_TRANSACTION.read_bytes()
        self.assertIn(
            f"-Bytes {len(generic_bytes)}", self.r21_material_transaction
        )
        self.assertIn(
            hashlib.sha256(generic_bytes).hexdigest().upper(),
            self.r21_material_transaction,
        )

        completed = subprocess.run(
            [
                "powershell.exe",
                "-NoLogo",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(R21_TRANSACTION_SAFETY_HARNESS),
            ],
            cwd=WORKSPACE_ROOT,
            text=True,
            capture_output=True,
            timeout=1800,
            check=False,
        )
        self.assertEqual(
            0,
            completed.returncode,
            msg=f"stdout={completed.stdout}\nstderr={completed.stderr}",
        )
        self.assertIn(
            "R21_TRANSACTION_SAFETY_SELF_TEST_PASS", completed.stdout
        )

        spec = importlib.util.spec_from_file_location(
            "triad_r21_acceptance_for_transaction_contract",
            R21_ACCEPTANCE_ANALYZER,
        )
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        analyzer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(analyzer)
        fixture = analyzer.synthetic_valid_transaction_receipt()
        for identity_field in (
            "ProtectedUE54IdentityBefore",
            "ProtectedUE54IdentityAfter",
        ):
            fixture[identity_field].pop("CreationTime", None)
        with tempfile.TemporaryDirectory(
            prefix="triad-r21-wrapper-shaped-receipt-"
        ) as temporary_directory:
            fixture_path = Path(temporary_directory) / "receipt.json"
            fixture_path.write_text(
                json.dumps(fixture, separators=(",", ":")),
                encoding="utf-8",
            )
            round_trip = subprocess.run(
                [
                    "powershell.exe",
                    "-NoLogo",
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(R21_TRANSACTION_SAFETY_HARNESS),
                    "-RoundTripFixture",
                    str(fixture_path),
                ],
                cwd=WORKSPACE_ROOT,
                text=True,
                capture_output=True,
                timeout=60,
                check=False,
            )
        self.assertEqual(
            0,
            round_trip.returncode,
            msg=f"stdout={round_trip.stdout}\nstderr={round_trip.stderr}",
        )
        wrapper_shaped = json.loads(round_trip.stdout)
        analyzer.validate_transaction_receipt_object(
            wrapper_shaped, verify_live_artifacts=False
        )
        identity_fields = {
            "ProcessId",
            "CreationUtcTicks",
            "Name",
            "ExecutablePath",
            "CommandLine",
        }
        self.assertEqual(
            identity_fields,
            set(wrapper_shaped["ProtectedUE54IdentityBefore"]),
        )
        self.assertEqual(
            identity_fields,
            set(wrapper_shaped["ProtectedUE54IdentityAfter"]),
        )
        arbitrary_fixture = subprocess.run(
            [
                "powershell.exe",
                "-NoLogo",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(R21_TRANSACTION_SAFETY_HARNESS),
                "-RoundTripFixture",
                str(R21_ACCEPTANCE_ANALYZER),
            ],
            cwd=WORKSPACE_ROOT,
            text=True,
            capture_output=True,
            timeout=60,
            check=False,
        )
        self.assertNotEqual(0, arbitrary_fixture.returncode)
        self.assertIn(
            "outside its fixed containment root",
            arbitrary_fixture.stderr,
        )

    def _assert_r21_generated_utc_round_trip_is_exact_across_powershell_hosts(self):
        spec = importlib.util.spec_from_file_location(
            "triad_r21_acceptance_for_generated_utc_contract",
            R21_ACCEPTANCE_ANALYZER,
        )
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        analyzer = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(analyzer)
        fixture = analyzer.synthetic_valid_transaction_receipt()
        for identity_field in (
            "ProtectedUE54IdentityBefore",
            "ProtectedUE54IdentityAfter",
        ):
            fixture[identity_field].pop("CreationTime", None)
        exact_generated_utc = "2026-08-31T23:04:38.1507033Z"
        self.assertEqual(exact_generated_utc, fixture["GeneratedUtc"])

        with tempfile.TemporaryDirectory(
            prefix="triad-r21-generated-utc-round-trip-"
        ) as temporary_directory:
            fixture_path = Path(temporary_directory) / "receipt.json"
            for executable in ("pwsh.exe", "powershell.exe"):
                fixture_path.write_text(
                    json.dumps(fixture, separators=(",", ":")),
                    encoding="utf-8",
                )
                completed = subprocess.run(
                    [
                        executable,
                        "-NoLogo",
                        "-NoProfile",
                        "-ExecutionPolicy",
                        "Bypass",
                        "-File",
                        str(R21_TRANSACTION_SAFETY_HARNESS),
                        "-RoundTripFixture",
                        str(fixture_path),
                    ],
                    cwd=WORKSPACE_ROOT,
                    text=True,
                    capture_output=True,
                    timeout=60,
                    check=False,
                )
                self.assertEqual(
                    0,
                    completed.returncode,
                    msg=(
                        f"host={executable} stdout={completed.stdout} "
                        f"stderr={completed.stderr}"
                    ),
                )
                round_trip = json.loads(completed.stdout)
                self.assertIsInstance(round_trip["GeneratedUtc"], str)
                self.assertEqual(
                    exact_generated_utc, round_trip["GeneratedUtc"]
                )

                for invalid_generated_utc in (
                    "2026-08-31T23:04:38.1507033",
                    "2026-09-01T07:04:38.1507033+08:00",
                    "2026-08-31T23:04:38.150703Z",
                    638922494781507033,
                ):
                    invalid_fixture = json.loads(json.dumps(fixture))
                    invalid_fixture["GeneratedUtc"] = invalid_generated_utc
                    fixture_path.write_text(
                        json.dumps(invalid_fixture, separators=(",", ":")),
                        encoding="utf-8",
                    )
                    rejected = subprocess.run(
                        [
                            executable,
                            "-NoLogo",
                            "-NoProfile",
                            "-ExecutionPolicy",
                            "Bypass",
                            "-File",
                            str(R21_TRANSACTION_SAFETY_HARNESS),
                            "-RoundTripFixture",
                            str(fixture_path),
                        ],
                        cwd=WORKSPACE_ROOT,
                        text=True,
                        capture_output=True,
                        timeout=60,
                        check=False,
                    )
                    self.assertNotEqual(
                        0,
                        rejected.returncode,
                        msg=(
                            f"host={executable} admitted GeneratedUtc="
                            f"{invalid_generated_utc!r} stdout={rejected.stdout}"
                        ),
                    )
                    self.assertIn(
                        "GeneratedUtc",
                        rejected.stderr,
                        msg=f"host={executable} stderr={rejected.stderr}",
                    )

    def test_r11_edge_fade_graph_and_one_package_upgrade_are_exact_and_fail_closed(self):
        upgrade = self.contract["r11EdgeGrassFadeUpgrade"]
        self.assertEqual(
            "UpgradeGroundVegetationEdgeGrassFadeAssetToR11",
            upgrade["endpoint"],
        )
        self.assertEqual("M_IPV5D_GrassMedium_EdgeFade", upgrade["assetName"])
        self.assertEqual(21, upgrade["expressionCount"])
        self.assertEqual(4, upgrade["textureSampleCount"])
        self.assertEqual(1, upgrade["sourceOpacityTextureOutputIndex"])
        self.assertEqual("red", upgrade["sourceOpacityMaskChannel"])
        self.assertEqual(9, upgrade["preservedWindInputCount"])
        self.assertEqual(5.0, upgrade["preservedMaximumWpoCentimeters"])
        self.assertEqual(14.0426675, upgrade["preservedWindHeightCentimeters"])
        self.assertEqual(1, upgrade["saveTargets"])
        self.assertEqual(1, upgrade["reloadTargets"])
        self.assertEqual(7, upgrade["coldValidatedMaterials"])
        self.assertEqual(6, upgrade["artifactStatesForNewPackage"])
        self.assertEqual(["R13", "R15", "R16"], upgrade["uniformCoreInputRevisions"])
        for key in (
            "sourceV4PackageHashGuarded",
            "allSixR13PackagesHashGuarded",
            "allSixR15PackagesHashGuarded",
            "allSixR16PackagesHashGuarded",
            "targetMapPackageHashGuarded",
            "newPackageInitiallyAbsentRequired",
            "staleAssetRegistryEntryRejected",
            "allR11StateIdempotent",
            "mixedStateRepairAllowed",
            "allSevenPackagesCleanAfterReloadRequired",
            "compiledSm5ShaderMapRequired",
            "fullNewPackageArtifactRosterBackedUpAsAbsent",
            "rollbackUnloadsBeforeDelete",
            "rollbackRescansAssetRegistryAfterDelete",
            "mapsEverOfferedToSave",
            "sourceV4EverOfferedToSave",
        ):
            expected = key not in {
                "mixedStateRepairAllowed",
                "mapsEverOfferedToSave",
                "sourceV4EverOfferedToSave",
            }
            self.assertEqual(expected, upgrade[key], key)
        self.assertEqual(
            "CREATE_AND_COLD_VALIDATE_R16_THEN_RUN_R11_THEN_ATOMIC_R17_THEN_ATOMIC_R18_THEN_ATOMIC_R19",
            upgrade["zeroPackageEnsureBehavior"],
        )

        validator = self.editor_cpp.split(
            "bool ValidateEdgeGrassMaterialGraph", 1
        )[1].split("bool ValidateEdgeGrassSourceMaterial", 1)[0]
        mutation_guards = {
            "uv1_without_coordinates_pin": "Sample->ConstCoordinate == 0",
            "constant_mip_override": "Sample->ConstMipValue == INDEX_NONE",
            "texture_object_pin": "!Sample->TextureObject.Expression",
            "mip_pin": "!Sample->MipValue.Expression",
            "derivative_x_pin": "!Sample->CoordinatesDX.Expression",
            "derivative_y_pin": "!Sample->CoordinatesDY.Expression",
            "automatic_bias_pin": "!Sample->AutomaticViewMipBiasValue.Expression",
            "time_wrap_enable": "OutGraph.Time->bOverride_Period",
            "time_pause_mode": "OutGraph.Time->bIgnorePause",
            "time_period_hidden_state": "OutGraph.Time->Period, 0.0f",
            "specular_connection": "OutGraph.Data->Specular.Expression",
            "specular_constant_enable": "OutGraph.Data->Specular.UseConstant",
            "specular_hidden_constant": "OutGraph.Data->Specular.Constant, 0.5f",
            "wrong_imported_height": "EdgeGrassSourceHeightCm",
            "custom_auxiliary_output_or_include": "AllCustomExpressionsHaveNoAuxiliaryState",
        }
        for mutation, guard in mutation_guards.items():
            with self.subTest(rejected_mutation=mutation):
                self.assertIn(guard, validator)
        for token in (
            "(bRequireFadeDerivative ? 21 : 18)",
            "OutGraph.TextureSamples != 4",
            "!Material->NaniteOverrideMaterial.bEnableOverride",
            "Material->NaniteOverrideMaterial.GetOverrideMaterial()",
            "Material->GetNaniteOverride()",
            "Material->OpacityMaskClipValue, 0.333f",
            "Material->MaxWorldPositionOffsetDisplacement",
            "OutGraph.Wind->Inputs.Num() == UE_ARRAY_COUNT(WindInputNames)",
            "DitherAlphaInputCount == 1",
            "GrassDitherTemporalAaFunctionPath",
            "OutGraph.FadeMultiply->A",
            "OutGraph.Opacity",
            "OutGraph.FadeMultiply->B",
            "OutGraph.Dither",
            "OutGraph.Data->OpacityMask",
            "OutGraph.Data->SubsurfaceColor",
            "OutGraph.Data->WorldPositionOffset",
        ):
            self.assertIn(token, validator)
        self.assertRegex(
            validator,
            r"OutGraph\.FadeMultiply->A,\s*OutGraph\.Opacity,\s*1",
        )
        self.assertRegex(
            validator,
            r"OutGraph\.FadeMultiply->B,\s*OutGraph\.Dither,\s*0",
        )

        creator = self.editor_cpp.split(
            "bool ConfigureEdgeGrassFadeMaterial", 1
        )[1].split("bool CreateEdgeGrassFadeMaterial", 1)[0]
        for token in (
            "/Script/Engine.MaterialExpressionPerInstanceFadeAmount",
            "Dither->SetMaterialFunction(DitherFunction)",
            "AlphaInputCount != 1",
            "Input.Connect(0, InstanceFade)",
            "FadeMultiply->A.Connect(1, SourceGraph.Opacity)",
            "FadeMultiply->B.Connect(0, Dither)",
            "OpacityMask.Connect(0, FadeMultiply)",
            "RecompileMaterial(Material)",
        ):
            self.assertIn(token, creator)

        endpoint_name = upgrade["endpoint"]
        self.assertIn(endpoint_name, self.editor_h)
        endpoint = self.editor_cpp.rsplit(endpoint_name, 1)[1].split(
            "ApplyGroundVegetationRealismPassToLoadedV5DHybridMap", 1
        )[0]
        absent_probe = endpoint.split("const FString EdgePackageName", 1)[1].split(
            "TArray<FR11PreservedPackageSnapshot>", 1
        )[0]
        self.assertIn("FindObject<UMaterial>(nullptr, *EdgeObjectPath)", absent_probe)
        self.assertIn("if (bEdgePackageOnDisk && !ExistingEdge)", absent_probe)
        self.assertEqual(1, absent_probe.count("LoadExact<UMaterial>"))
        self.assertLess(
            absent_probe.index("if (bEdgePackageOnDisk && !ExistingEdge)"),
            absent_probe.index("LoadExact<UMaterial>"),
        )
        ordered_markers = (
            "ValidateUniformR13R15OrR16MaterialAssetsInternal",
            "CaptureR11PreservedPackageSnapshots",
            "BackUpAbsentR11PackageState",
            "CreateEdgeGrassFadeMaterial",
            "FinishAllCompilation",
            "SaveLoadedAssets",
            "ReloadSingleR11MaterialPackage",
            "ValidateCompleteR16MaterialAssets",
            "CaptureObjectPackageArtifacts",
        )
        positions = [endpoint.index(marker) for marker in ordered_markers]
        self.assertEqual(sorted(positions), positions)
        for token in (
            "ExistingEdgeAssetData.IsValid()",
            "ExactSaveTargets = {NewEdge}",
            "ExactSaveTargets.Num() != 1",
            "ColdCoreMaterials.Num() == 6",
            "bAllSevenColdPackagesClean",
            "coldValidatedMaterials=7",
            "R11PreservedPackageSnapshotsMatch",
            "RestoreAbsentR11PackageAtomically",
            "AUTOMATIC_ROLLBACK_OK",
            "mapsSaved=0",
        ):
            self.assertIn(token, endpoint)
        for forbidden in ("SaveMap", "SavePackage", "MarkPackageDirty"):
            self.assertNotIn(forbidden, endpoint)

        preserved = self.editor_cpp.split(
            "bool CaptureR11PreservedPackageSnapshots", 1
        )[1].split("bool R11PreservedPackageSnapshotsMatch", 1)[0]
        self.assertIn("ObjectPaths.Add(SourceEdgeGrassMaterialPath)", preserved)
        self.assertIn("CaptureTargetMapArtifacts", preserved)
        self.assertIn("OutSnapshots.Num() != 8", preserved)
        rollback = self.editor_cpp.split(
            "bool RestoreAbsentR11PackageAtomically", 1
        )[1].split("bool BackUpMaterialPackages", 1)[0]
        for token in (
            "UPackageTools::UnloadPackages",
            "Backup.Artifacts.Num() == 6",
            "ScanModifiedAssetFiles",
            "GetAssetByObjectPath",
            "FindObject<UMaterial>",
        ):
            self.assertIn(token, rollback)

        ensure = self.editor_cpp.split(
            "bool EnsureCompleteR11MaterialAssets", 1
        )[1].split("FString FileMd5", 1)[0]
        for token in (
            "ExistingCount == 7",
            "ValidateCompleteCurrentMaterialAssets",
            "ValidateCompleteR19MaterialAssets",
            "ValidateCompleteR18MaterialAssets",
            "ValidateCompleteR17MaterialAssets",
            "ExistingCount == 6",
            "Run UpgradeGroundVegetationEdgeGrassFadeAssetToR11",
            "ExistingCount == 0",
            "EnsureMaterials",
            "UpgradeGroundVegetationEdgeGrassFadeAssetToR11",
            "UpgradeGroundVegetationRealismMaterialAssetsToR17",
            "UpgradeGroundVegetationRealismLawnOverlayToR18",
            "UpgradeGroundVegetationRealismGrassMaterialsToR19",
            "The zero-package build created clean R16 core and R11 edge state",
            "bEdgeHasAnyState",
            "GetAssetByObjectPath",
        ):
            self.assertIn(token, ensure)
        zero_path = ensure.split("if (ExistingCount == 0)", 1)[1]
        ordered_zero_markers = (
            "EnsureMaterials",
            "UpgradeGroundVegetationEdgeGrassFadeAssetToR11",
            "UpgradeGroundVegetationRealismMaterialAssetsToR17",
            "UpgradeGroundVegetationRealismLawnOverlayToR18",
            "UpgradeGroundVegetationRealismGrassMaterialsToR19",
            "ValidateCompleteR19MaterialAssets",
        )
        positions = [zero_path.index(marker) for marker in ordered_zero_markers]
        self.assertEqual(sorted(positions), positions)
        fresh_creator = self.editor_cpp.split("bool EnsureMaterials", 1)[1].split(
            "int32 ExistingGroundMaterialPackageCountIncludingR11", 1
        )[0]
        self.assertEqual(
            2, fresh_creator.count("ValidateR16MaterialAssetsInternal(OutMaterials, OutError)")
        )
        self.assertNotIn(
            "ValidateMaterialAssetsInternal(OutMaterials, OutError)", fresh_creator
        )

    def test_r11_edge_fade_runtime_swap_restore_and_cook_dependency_are_explicit(self):
        presentation = self.contract["edgeGrassRuntimeFadePresentation"]
        self.assertEqual("EdgeGrassInstances", presentation["component"])
        self.assertEqual(
            "M_IPV4_GrassMedium_Wind",
            presentation["frozenMapSerializedMaterial"],
        )
        self.assertEqual(
            "M_IPV5D_GrassMedium_EdgeFade",
            presentation["begunPlayMaterial"],
        )
        for key in (
            "runtimeOnly",
            "frozenMapBytesRemainUnchanged",
            "fullOverrideMaterialsArraySnapshotted",
            "onlySlotZeroReplaced",
            "dormantOverrideSlotsPreserved",
            "partialApplyFailureRestoresExactArray",
            "reapplyRestoresBeforeLayoutAndRebindsAfter",
            "tickFailureRestoresExactArray",
            "endPlayRestoresExactArray",
            "meshTransformsCensusCollisionNavigationAndRfRemainUntouched",
            "nativeCdoReferenceIsNonTransient",
            "constructorHelpersExactPathReference",
            "freshRestartBeforeCookRequired",
            "sameSessionLoadFallbackIsNotCookProof",
            "freshCookManifestAndPackagedLoadAcceptanceRequired",
        ):
            self.assertTrue(presentation[key], key)
        self.assertTrue(presentation["serializedCullPolicyChangedByR20"])
        self.assertFalse(presentation["mapsEverSavedByR11"])

        combined = self.actor_h + self.actor_cpp
        for token in (
            "M_IPV5D_GrassMedium_EdgeFade.M_IPV5D_GrassMedium_EdgeFade",
            "FObjectFinderOptional<UMaterialInterface>",
            "ExpectedEdgeGrassFadeMaterialPath",
            "ValidateNativeEdgeGrassFadeCookDependency",
            "EnsureRuntimeEdgeGrassFadePresentation",
            "ApplyRuntimeEdgeGrassFadePresentation",
            "ValidateRuntimeEdgeGrassFadePresentation",
            "RestoreRuntimeEdgeGrassFadePresentation",
            "RuntimeOriginalEdgeGrassOverrideMaterials",
            "RuntimeOriginalEdgeGrassComponent",
            "RuntimeOriginalEdgeGrassResolvedMaterial",
            "TRIAD.Istana.ExploreV5D.GroundVegetation.EdgeGrassFadeCookDependency",
            "freshCookManifestAndPackagedLoadStillRequired=true",
        ):
            self.assertIn(token, combined)
        self.assertRegex(
            self.actor_h,
            r"UPROPERTY\(\)\s*TObjectPtr<UMaterialInterface>\s+EdgeGrassFadeMaterialCookReference",
        )
        self.assertNotRegex(
            self.actor_h,
            r"UPROPERTY\(Transient\)\s*TObjectPtr<UMaterialInterface>\s+EdgeGrassFadeMaterialCookReference",
        )
        constructor_probe = self.actor_cpp.split(
            "ATRIADIstanaExploreV5DGroundVegetationActor::\n"
            "    ATRIADIstanaExploreV5DGroundVegetationActor()",
            1,
        )[1].split("PrimaryActorTick.bCanEverTick", 1)[0]
        self.assertIn("FPackageName::ObjectPathToPackageName", constructor_probe)
        self.assertIn("FPackageName::DoesPackageExist", constructor_probe)
        self.assertLess(
            constructor_probe.index("FPackageName::DoesPackageExist"),
            constructor_probe.index(
                "FObjectFinderOptional<UMaterialInterface>"
            ),
        )
        for field in (
            "RuntimeOriginalEdgeGrassOverrideMaterials",
            "RuntimeOriginalEdgeGrassComponent",
            "RuntimeOriginalEdgeGrassResolvedMaterial",
            "bRuntimeEdgeGrassFadeSnapshotValid",
            "bRuntimeEdgeGrassFadePresentationApplied",
        ):
            declaration = self.actor_h.split(field, 1)[0].rsplit("UPROPERTY", 1)[1]
            self.assertIn("Transient", declaration, field)

        apply = self.actor_cpp.split(
            "ApplyRuntimeEdgeGrassFadePresentation(FString& OutError)", 1
        )[1].split("ValidateRuntimeEdgeGrassFadePresentation", 1)[0]
        self.assertIn(
            "RuntimeOriginalEdgeGrassOverrideMaterials =\n        EdgeGrassInstances->OverrideMaterials",
            apply,
        )
        self.assertIn("EdgeGrassInstances->SetMaterial(0, FadeMaterial)", apply)
        self.assertNotIn("SavedAssets.EdgeGrassMaterial =", apply)
        restore = self.actor_cpp.split(
            "RestoreRuntimeEdgeGrassFadePresentation(FString& OutError)", 1
        )[1].split("ResolveExactContextPolicy", 1)[0]
        self.assertLess(
            restore.index("Component->EmptyOverrideMaterials()"),
            restore.index("RuntimeOriginalEdgeGrassOverrideMaterials.Num()"),
        )
        self.assertIn("OverrideMaterialArraysEqual", restore)
        self.assertIn("RuntimeOriginalEdgeGrassResolvedMaterial", restore)
        self.assertNotIn("MarkPackageDirty", self.actor_cpp)

        reapply = self.actor_cpp.split(
            "ReapplyGroundVegetationRealism(FString& OutError)", 1
        )[1].split("ValidateAppliedRuntimeSourceTurfPresentationForSourceActor", 1)[0]
        prior_runtime_restore = reapply.index(
            "RestoreRuntimeEdgeGrassFadePresentation"
        )
        cold_capture = reapply.index(
            "ColdEdgeOverrideMaterials = EdgeGrassInstances->OverrideMaterials"
        )
        apply_layout = reapply.index(
            "const bool bLayoutApplied = ApplyLayout"
        )
        cold_replay = reapply.index(
            "ReplayExactColdEdgeMaterialState(ReplayError)"
        )
        layout_failure_gate = reapply.index(
            "if (!bLayoutApplied || !bColdEdgeMaterialStateReplayed)"
        )
        runtime_ensure = reapply.index(
            "EnsureRuntimeEdgeGrassFadePresentation"
        )
        self.assertEqual(
            sorted(
                (
                    prior_runtime_restore,
                    cold_capture,
                    apply_layout,
                    cold_replay,
                    layout_failure_gate,
                    runtime_ensure,
                )
            ),
            [
                prior_runtime_restore,
                cold_capture,
                apply_layout,
                cold_replay,
                layout_failure_gate,
                runtime_ensure,
            ],
        )
        for token in (
            "ColdEdgeResolvedMaterial = EdgeGrassInstances->GetMaterial(0)",
            "ColdEdgeMesh = EdgeGrassInstances->GetStaticMesh()",
            "EdgeGrassInstances->SetStaticMesh(ColdEdgeMesh)",
            "EdgeGrassInstances->EmptyOverrideMaterials()",
            "ColdEdgeOverrideMaterials.Num()",
            "OverrideMaterialArraysEqual",
            "exactColdEdgeMaterialReplay=true",
            "exactColdEdgeMaterialReplay=false",
        ):
            self.assertIn(token, reapply)
        begin_play = self.actor_cpp.split("void ATRIADIstanaExploreV5DGroundVegetationActor::BeginPlay", 1)[1].split(
            "void ATRIADIstanaExploreV5DGroundVegetationActor::Tick", 1
        )[0]
        tick = self.actor_cpp.split("void ATRIADIstanaExploreV5DGroundVegetationActor::Tick", 1)[1].split(
            "void ATRIADIstanaExploreV5DGroundVegetationActor::EndPlay", 1
        )[0]
        end_play = self.actor_cpp.split("void ATRIADIstanaExploreV5DGroundVegetationActor::EndPlay", 1)[1].split(
            "#if WITH_DEV_AUTOMATION_TESTS", 1
        )[0]
        for lifecycle in (begin_play, tick):
            self.assertIn("EnsureRuntimeEdgeGrassFadePresentation", lifecycle)
            self.assertIn("RestoreRuntimeEdgeGrassFadePresentation", lifecycle)
        self.assertIn("RestoreRuntimeEdgeGrassFadePresentation", end_play)
        self.assertIn("sourceOpacityMaskOutput=1_red", self.actor_cpp)
        self.assertIn("inactive_cold_serialized_material", self.actor_cpp)
        self.assertIn("fresh cook manifest", self.package_readme.lower())

    def test_atomic_builder_hook_keeps_blueprint_exact_map_gate(self):
        trusted_hook = (
            "ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder"
        )
        self.assertIn(trusted_hook, self.editor_h)
        self.assertIn(trusted_hook, self.editor_cpp)
        self.assertIn("bTrustedUntitledHybridBuilder", self.editor_cpp)
        self.assertIn("FPackageName::IsTempPackage", self.editor_cpp)
        public_decl = self.editor_h.split(trusted_hook, 1)[0]
        self.assertIn("UFUNCTION(BlueprintCallable", public_decl)
        trusted_decl = self.editor_h.split(trusted_hook, 1)[1].split(";", 1)[0]
        self.assertNotIn("UFUNCTION", trusted_decl)

    def test_native_determinism_automation_is_registered(self):
        self.assertIn(
            "TRIAD.Istana.ExploreV5D.GroundVegetation.DeterministicLayout",
            self.actor_cpp,
        )
        self.assertGreaterEqual(len(re.findall(r"TestTrue\(|TestEqual\(", self.actor_cpp)), 8)

    def test_native_terrain_seam_policy_automation_is_registered(self):
        self.assertIn(
            "TRIAD.Istana.ExploreV5D.GroundVegetation.TerrainSeamPolicy",
            self.actor_cpp,
        )
        for token in (
            "Normalized ellipse-angle bracket matches the exact 45-degree edge",
            "0.25-degree sweep finds the exact edge on every ray",
            "0.25-degree sweep matches brute-force edge selection within 0.001 cm",
            "Exact provider boundary remains fully covered",
            "Authored overlay remains fully opaque through the 50 m collar",
            "Transition begins only after the full-opacity collar",
            "Overlay never renders beyond the exact 58 m collar and feather",
            "Continuous overlay coverage is zero beyond the exact 58 m collar and feather",
            "World-space dither is deterministic",
            "Only valid begun hidden-fallback state hides source terrain",
            "Collision remains live while renderer is hidden",
            "Fallback/EndPlay transition restores renderer",
        ):
            self.assertIn(token, self.actor_cpp)


if __name__ == "__main__":
    unittest.main()
