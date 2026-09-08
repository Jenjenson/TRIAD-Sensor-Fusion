from __future__ import annotations

import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
SOURCE = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Vegetation"
    / "R32MediumDistanceTurf"
)
CONTRACT = SOURCE / "r32_medium_distance_turf.contract.json"
README = SOURCE / "README.md"
ACTOR_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Public"
    / "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"
)
ACTOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.cpp"
)
GROUND_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Public"
    / "TRIADIstanaExploreV5DGroundVegetationActor.h"
)
GROUND_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DGroundVegetationActor.cpp"
)
R29_ASSET_FACTORY_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR29VegetationAssetFactory.cpp"
)
R32_ASSET_FACTORY_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.h"
)
R32_ASSET_FACTORY_CPP = R32_ASSET_FACTORY_H.with_suffix(".cpp")

EXPECTED_PROFILES = ("manicured", "humid", "shade", "dryEdge")
EXPECTED_SOURCE_CENSUS = {
    "manicured": 12_460,
    "humid": 3_976,
    "shade": 1_152,
    "dryEdge": 844,
    "total": 18_432,
}
EXPECTED_SELECTED_CENSUS = {
    "manicured": 3_115,
    "humid": 994,
    "shade": 288,
    "dryEdge": 211,
    "total": 4_608,
}
EXPECTED_MESH_PATHS = (
    "/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/"
    "SM_IPV5D_R29_GrassFineCluster_Render.SM_IPV5D_R29_GrassFineCluster_Render",
    "/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/"
    "SM_IPV5D_R29_GrassBroadCluster_Render.SM_IPV5D_R29_GrassBroadCluster_Render",
    "/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/"
    "SM_IPV5D_R29_GrassMixedCluster_Render.SM_IPV5D_R29_GrassMixedCluster_Render",
)
EXPECTED_SOURCE_MATERIAL_PATHS = tuple(
    "/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/"
    f"M_IPV5D_R29_Turf_{name}.M_IPV5D_R29_Turf_{name}"
    for name in ("Manicured", "Humid", "Shade", "DryEdge")
)
EXPECTED_R32_MATERIAL_PATHS = tuple(
    "/Game/TRIAD/IstanaPublicViewExploreV5D/MediumDistanceTurfR32/Materials/"
    f"M_IPV5D_R32_Turf_{name}.M_IPV5D_R32_Turf_{name}"
    for name in ("Manicured", "Humid", "Shade", "DryEdge")
)


def function_region(source: str, start_marker: str, end_marker: str) -> str:
    start = source.find(start_marker)
    if start < 0:
        raise AssertionError(f"missing function start marker: {start_marker}")
    end = source.find(end_marker, start)
    if end < 0:
        raise AssertionError(f"missing function end marker: {end_marker}")
    return source[start:end]


class R32MediumDistanceTurfContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (
            CONTRACT,
            README,
            ACTOR_H,
            ACTOR_CPP,
            GROUND_H,
            GROUND_CPP,
            R29_ASSET_FACTORY_CPP,
            R32_ASSET_FACTORY_H,
            R32_ASSET_FACTORY_CPP,
        ):
            if not path.is_file():
                raise AssertionError(f"missing R32 contract input: {path}")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.readme = README.read_text(encoding="utf-8")
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor_cpp = ACTOR_CPP.read_text(encoding="utf-8")
        cls.ground_h = GROUND_H.read_text(encoding="utf-8")
        cls.ground_cpp = GROUND_CPP.read_text(encoding="utf-8")
        cls.r29_asset_factory_cpp = R29_ASSET_FACTORY_CPP.read_text(
            encoding="utf-8"
        )
        cls.r32_asset_factory_h = R32_ASSET_FACTORY_H.read_text(
            encoding="utf-8"
        )
        cls.r32_asset_factory_cpp = R32_ASSET_FACTORY_CPP.read_text(
            encoding="utf-8"
        )

    def test_contract_selects_exactly_one_quarter_of_each_r23_profile(self) -> None:
        contract = self.contract
        self.assertEqual(
            "triad.istana_explore_v5d.r32_medium_distance_turf.v1",
            contract["schema"],
        )
        self.assertEqual("R32", contract["revision"])
        self.assertEqual(
            "VISUAL_ASSUMPTION_BOUND_R32_MEDIUM_DISTANCE_MODELED_TURF_BRIDGE",
            contract["claim"],
        )
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid",
            contract["targetMap"],
        )

        handoff = contract["sourceTransformHandoff"]
        self.assertEqual(
            "ATRIADIstanaExploreV5DGroundVegetationActor", handoff["ownerClass"]
        )
        self.assertEqual(
            "GetMediumDistanceTurfSourceProfilesR32", handoff["getter"]
        )
        self.assertEqual(23, handoff["serializedPresentationRevision"])
        self.assertEqual(list(EXPECTED_PROFILES), handoff["sourceProfileOrder"])
        self.assertEqual(EXPECTED_SOURCE_CENSUS, handoff["sourceProfileCensus"])
        self.assertTrue(handoff["validatesFullGroundVegetationRealismBeforeCopy"])
        self.assertTrue(handoff["validatesOwnedGrassInstanceTransformsBeforeCopy"])
        self.assertTrue(handoff["copyOnly"])
        self.assertFalse(handoff["sourceActorOrComponentsMutated"])

        selection = contract["deterministicSelection"]
        self.assertEqual(2, selection["distributionPolicyVersion"])
        self.assertEqual(4, selection["selectionDenominator"])
        self.assertEqual("A511E9B3", selection["selectionHashSaltHex"])
        self.assertFalse(selection["selectionUsesEveryNthOrdinal"])
        self.assertFalse(selection["selectionUsesOrdinalModulo"])
        self.assertFalse(selection["meshVariantUsesOrdinalModulo"])
        self.assertTrue(selection["regularEveryFourthPatternEliminated"])
        self.assertTrue(selection["regularTenSlotVariantPatternEliminated"])
        self.assertEqual(EXPECTED_SELECTED_CENSUS, selection["selectedProfileCensus"])
        self.assertEqual(0.25, selection["selectedFraction"])
        self.assertEqual(4, selection["profileCount"])
        self.assertEqual(3, selection["meshVariantCount"])
        self.assertEqual(12, selection["profileVariantBucketCount"])
        self.assertEqual([50, 30, 20], selection["meshVariantTargetWeights"])
        self.assertEqual(
            {
                "manicured": [1560, 933, 622],
                "humid": [499, 297, 198],
                "shade": [145, 87, 56],
                "dryEdge": [106, 63, 42],
            },
            selection["profileVariantCensus"],
        )
        self.assertEqual(
            EXPECTED_SELECTED_CENSUS["total"],
            sum(EXPECTED_SELECTED_CENSUS[name] for name in EXPECTED_PROFILES),
        )
        for name in EXPECTED_PROFILES:
            self.assertEqual(
                EXPECTED_SOURCE_CENSUS[name] // 4,
                EXPECTED_SELECTED_CENSUS[name],
            )
        self.assertTrue(selection["sourceTransformsOnly"])
        self.assertTrue(selection["worldTranslationsPreservedExactly"])
        self.assertTrue(selection["allBucketsRequiredNonEmpty"])
        self.assertEqual(
            {"fine": 2310, "broad": 1380, "mixed": 918, "total": 4608},
            selection["selectedMeshVariantCensus"],
        )
        self.assertTrue(selection["exactVariantQuotaClaimed"])
        self.assertFalse(selection["exactVariantPercentageClaimed"])

    def test_contract_reuses_exact_r29_meshes_and_isolates_r32_materials(self) -> None:
        assets = self.contract["reusedR29Assets"]
        self.assertEqual(0, assets["newMeshAssetPackagesCreated"])
        self.assertEqual(3, assets["meshCount"])
        self.assertEqual(4, assets["sourceMaterialCount"])
        self.assertEqual([0.0, 0.0, 0.0], assets["meshMinimumZCentimeters"])
        self.assertEqual(
            [13.844719, 18.459288, 19.605375],
            assets["meshMaximumZCentimeters"],
        )
        self.assertEqual([1, 1, 1], assets["meshLodCount"])
        self.assertEqual("R29Grass", assets["meshMaterialSlot"])
        self.assertEqual(EXPECTED_MESH_PATHS, tuple(assets["meshObjectPaths"]))
        self.assertEqual(
            EXPECTED_SOURCE_MATERIAL_PATHS,
            tuple(assets["sourceMaterialObjectPaths"]),
        )
        self.assertTrue(assets["sourceAssetRosterOrderExact"])
        self.assertFalse(assets["sourceAssetPackagesModified"])
        self.assertFalse(assets["sourceMaterialGraphsModified"])

        derivatives = self.contract["r32MaterialDerivatives"]
        self.assertEqual(4, derivatives["newAssetPackageCount"])
        self.assertEqual(
            EXPECTED_R32_MATERIAL_PATHS,
            tuple(derivatives["materialObjectPaths"]),
        )
        self.assertEqual(32, derivatives["expressionCountPerMaterial"])
        self.assertEqual(11.0, derivatives["broadScaleMeters"])
        self.assertEqual(3.4, derivatives["mesoScaleMeters"])
        self.assertEqual([20.0, 65.0], derivatives["mediumResponseFullReadMeters"])
        self.assertEqual([65.0, 76.0], derivatives["responseFadeOutMeters"])
        self.assertEqual([2.0, 5.5], derivatives["screenFootprintBandLimitMeters"])
        self.assertTrue(derivatives["usesScreenSpaceDerivatives"])
        self.assertFalse(derivatives["hardStripeOrCheckerSelectorUsed"])
        self.assertFalse(derivatives["sourceR29PackagesModified"])
        self.assertFalse(derivatives["geometryOrTransformsModified"])
        self.assertFalse(derivatives["physicalPbrTruthClaimed"])
        self.assertFalse(derivatives["siteMeasuredMaterialClaimed"])
        self.assertFalse(derivatives["nativeVisualAcceptance"])

    def test_contract_binds_tip_band_cull_band_and_render_only_policy(self) -> None:
        presentation = self.contract["presentation"]
        self.assertEqual(
            "closely mown formal lawn/fairway",
            presentation["referenceCharacter"],
        )
        self.assertTrue(presentation["closelyMown"])
        self.assertEqual(
            [12.0, 20.0, 50.0],
            presentation["readableMicrodetailReviewMeters"],
        )
        self.assertEqual(
            [20.0, 50.0, 65.0],
            presentation["mediumScaleMaterialReadabilityReviewMeters"],
        )
        self.assertEqual([5.0, 9.0], presentation["targetTipHeightCentimeters"])
        self.assertEqual(
            {
                "manicured": [5.0, 6.5],
                "humid": [6.2, 8.2],
                "shade": [6.5, 9.0],
                "dryEdge": [5.0, 7.0],
            },
            presentation["profileTipHeightCentimeters"],
        )
        self.assertEqual([65.0, 90.0], presentation["hismCullMeters"])
        self.assertEqual(60.0, presentation["wpoDisableMeters"])
        self.assertEqual(0.0, presentation["translationJitterCentimeters"])
        self.assertEqual(12, presentation["ownedHismCount"])
        self.assertTrue(presentation["perInstanceFadeUsesCullBand"])
        self.assertTrue(presentation["worldSpace"])
        self.assertTrue(presentation["identityActorTransformRequired"])
        self.assertFalse(presentation["sourceRotationsPreservedExactly"])
        self.assertTrue(presentation["sourceTiltRetainedUnderWorldZYawComposition"])
        self.assertTrue(presentation["deterministicYawAdded"])
        self.assertEqual([-180.0, 180.0], presentation["addedYawDegrees"])
        self.assertTrue(presentation["boundedXyScaleBreakup"])
        self.assertEqual([0.88, 1.12], presentation["xyScaleMultiplier"])
        self.assertFalse(presentation["castShadow"])
        self.assertFalse(presentation["castContactShadow"])
        self.assertTrue(presentation["renderInMainPass"])
        self.assertFalse(presentation["hiddenInSceneCapture"])
        self.assertTrue(presentation["visibleInSceneCapture"])
        self.assertTrue(presentation["rgbSimulationCameraParity"])
        self.assertTrue(presentation["rgbVisibilityIsPresentationOnly"])
        self.assertFalse(
            presentation["rgbVisibilityConfersSensorDetectionAuthority"]
        )
        for token in (
            "1.0-smoothstep(6500.0,9000.0,distanceCm)",
            "max(componentVisibility,calibratedVisibility)",
            "R29_MODELED_BLADE_STABLE_VISIBILITY_65M_90M",
        ):
            self.assertIn(token, self.r29_asset_factory_cpp)

        runtime = self.contract["runtimeActor"]
        self.assertEqual(
            "ATRIADIstanaExploreV5DR32MediumDistanceTurfActor", runtime["class"]
        )
        self.assertEqual("ConfigureR32MediumDistanceTurf", runtime["configureMethod"])
        self.assertEqual("ValidateR32MediumDistanceTurf", runtime["validationMethod"])
        self.assertTrue(runtime["deterministic"])
        self.assertTrue(runtime["coexistsWithSourceGroundVegetation"])
        self.assertFalse(runtime["sourceGroundVegetationHiddenOrMutated"])
        self.assertTrue(runtime["coexistsWithR29Vegetation"])
        self.assertFalse(runtime["silentlyHidesOrMutatesR29Vegetation"])

        performance = self.contract["performanceEnvelope"]
        self.assertTrue(performance["singleLodMeshes"])
        self.assertEqual([1024, 896, 1152], performance["sourceMeshTriangles"])
        self.assertTrue(performance["exactLod0TriangleCensusValidatedAtRuntime"])
        self.assertEqual(
            [64, 56, 72], performance["sourceManifestModeledBlades"]
        )
        self.assertEqual(4_659_456, performance["worstCaseVisibleTriangles"])
        self.assertEqual(
            291_216, performance["sourceManifestPresentedBladeBudget"]
        )
        self.assertFalse(performance["runtimeModeledBladeCensusClaimed"])
        self.assertFalse(performance["densityScalingEnabled"])
        self.assertTrue(performance["nativeTargetHardwareGateRequired"])
        self.assertFalse(performance["nativeGpuFrameTimeAndMoireAccepted"])

    def test_delivery_order_and_negative_authority_are_fail_closed(self) -> None:
        state = self.contract["deliveryState"]
        self.assertTrue(state["sourceImplementationComplete"])
        self.assertTrue(state["nativeExecutionWrapperImplemented"])
        for name in (
            "nativeProjectApplied",
            "targetMapMutated",
            "unrealEditorLaunchedForThisDelivery",
            "nativeCaptureProduced",
            "visualCaptureAccepted",
            "nativeExecutionEligibleNow",
        ):
            self.assertFalse(state[name], name)
        self.assertTrue(state["captureRevalidationRequired"])

        order = self.contract["nativeOrdering"]
        self.assertIn("R30 COMMITTED", order["promotionOrder"])
        self.assertIn("five-pose Player0", order["promotionOrder"])
        self.assertIn("then R31 COMMITTED", order["promotionOrder"])
        self.assertIn("then R32", order["promotionOrder"])
        for name in (
            "r30CommittedReceiptRequired",
            "r30AcceptedFivePosePlayer0CaptureRequired",
            "r31CommittedReceiptRequired",
            "r31AcceptedNativeCaptureRequired",
        ):
            self.assertTrue(order[name], name)
        for name in (
            "r32MayBypassR30OrR31",
            "prerequisitesSatisfiedForThisDelivery",
            "nativeTransactionExecuted",
        ):
            self.assertFalse(order[name], name)
        self.assertTrue(order["nativeTransactionImplemented"])
        self.assertTrue(order["nativeExecutionWrapperImplemented"])
        self.assertTrue(order["r31PreR33ContextPolicyWrapperSourceReclosed"])
        self.assertFalse(order["r31CommittedAndCapturedPredecessorAvailable"])

        preservation = self.contract["preservation"]
        self.assertTrue(preservation)
        self.assertTrue(all(value is False for value in preservation.values()))
        truth = self.contract["truthBoundary"]
        self.assertTrue(truth["appearanceOnly"])
        self.assertTrue(truth["visualAcceptanceOwnedByExternalReceiptOnly"])
        for name, value in truth.items():
            if name not in (
                "appearanceOnly",
                "visualAcceptanceOwnedByExternalReceiptOnly",
            ):
                self.assertFalse(value, name)

    def test_ground_actor_handoff_is_exact_validated_and_copy_only(self) -> None:
        signature_tokens = (
            "GetMediumDistanceTurfSourceProfilesR32(",
            "TArray<FTransform>& OutManicured",
            "TArray<FTransform>& OutHumid",
            "TArray<FTransform>& OutShade",
            "TArray<FTransform>& OutDryEdge",
            "FString& OutReport) const;",
        )
        for token in signature_tokens:
            self.assertIn(token, self.ground_h)

        region = function_region(
            self.ground_cpp,
            "GetMediumDistanceTurfSourceProfilesR32(",
            "void ATRIADIstanaExploreV5DGroundVegetationActor::BeginPlay()",
        )
        for token in (
            "ExpectedManicuredCount = 12460",
            "ExpectedHumidCount = 3976",
            "ExpectedShadeCount = 1152",
            "ExpectedDryEdgeCount = 844",
            "GrassPresentationRevision != GrassPresentationRevisionR23",
            "ValidateGroundVegetationRealism(FullGroundReport)",
            "ValidateOwnedGrassInstanceTransforms(ExactTransformReport)",
            "OutManicured = SavedGrassManicured",
            "OutHumid = SavedGrassHumid",
            "OutShade = SavedGrassShade",
            "OutDryEdge = SavedGrassDryEdge",
            "sourceReadOnly=true",
            "fullGroundVegetationValid=true",
        ):
            self.assertIn(token, region)
        for forbidden in (
            "ClearInstances(",
            "SetVisibility(",
            "SetHiddenInGame(",
            "RemoveAt(",
            "SavedGrassManicured =",
            "SavedGrassHumid =",
            "SavedGrassShade =",
            "SavedGrassDryEdge =",
        ):
            self.assertNotIn(forbidden, region)

    def test_r32_actor_public_api_and_fixed_constants_match_contract(self) -> None:
        for token in (
            "class TRIADSENSORFUSION_API ATRIADIstanaExploreV5DR32MediumDistanceTurfActor",
            "ConfigureR32MediumDistanceTurf(",
            "ValidateR32MediumDistanceTurf(",
            "ExpectedClaimLabel(",
            "ExpectedActorTag(",
            "ExpectedInstanceCount(",
            "ExpectedBucketCount(",
        ):
            self.assertIn(token, self.actor_h)

        for token in (
            "constexpr int32 SourceInstanceCount = 18432;",
            "constexpr int32 SelectionDenominator = 4;",
            "constexpr int32 ProfileCount = 4;",
            "constexpr int32 MeshVariantCount = 3;",
            "constexpr int32 BucketCount = ProfileCount * MeshVariantCount;",
            "SourceInstanceCount / SelectionDenominator;",
            "constexpr int32 CullStartDistanceCm = 6500;",
            "constexpr int32 CullEndDistanceCm = 9000;",
            "constexpr int32 WpoDisableDistanceCm = 6000;",
            "constexpr double MinimumTipHeightCm = 5.0;",
            "constexpr double MaximumTipHeightCm = 9.0;",
            "constexpr double MeshBoundsToleranceCm = 0.01;",
            "constexpr int32 SourceProfileCounts[] = {12460, 3976, 1152, 844};",
            "constexpr int32 ProfileQuotas[] = {3115, 994, 288, 211};",
            "constexpr int32 VariantInstanceCounts[] = {2310, 1380, 918};",
            "constexpr int32 DistributionPolicyVersion = 2;",
            "constexpr uint32 SelectionHashSalt = 0xA511E9B3u;",
            "constexpr uint32 VariantHashSalt = 0x63D83595u;",
            "constexpr double MinimumXyScaleMultiplier = 0.88;",
            "constexpr double MaximumXyScaleMultiplier = 1.12;",
            "constexpr double MaximumAddedYawDegrees = 180.0;",
            "constexpr int32 ProfileVariantQuotas[ProfileCount][MeshVariantCount]",
            "static_assert(WorstCaseVisibleTriangleCount == 4659456);",
            "static_assert(SelectedInstanceCount == 4608);",
            "static_assert(BucketCount == 12);",
        ):
            self.assertIn(token, self.actor_cpp)
        for path in (*EXPECTED_MESH_PATHS, *EXPECTED_R32_MATERIAL_PATHS):
            self.assertIn(path, self.actor_cpp)
        self.assertIn(self.contract["claim"], self.actor_cpp)

    def test_r32_material_factory_is_continuous_isolated_and_render_only(self) -> None:
        for token in (
            "CreateFreshAssets",
            "ValidateAssets",
            "LoadValidatedRuntimeContract",
            "R32_20M_65M_CONTINUOUS_TURF_ROUGHNESS",
            "smoothstep(1400.0,2000.0,distanceCm)",
            "smoothstep(6500.0,7600.0,distanceCm)",
            "r32BroadP",
            "r32MesoP",
            "broadF=broadF*broadF*(3.0-2.0*broadF)",
            "mesoF=mesoF*mesoF*(3.0-2.0*mesoF)",
            "max(length(ddx(worldM)),length(ddy(worldM)))",
            "smoothstep(2.0,5.5,footprintM)",
            "allowedGraphDeltas=baseColourBodyAndLabel,roughnessBodyAndLabel",
            "opacityModified=false",
            "wpoModified=false",
            "normalResponseModified=false",
            "pbrTruthClaimed=false",
            "siteMeasurementClaimed=false",
        ):
            self.assertIn(token, self.r32_asset_factory_cpp)
        for forbidden in (
            "SetWorldPositionOffset",
            "WorldPositionOffset.Expression =",
            "OpacityMask.Expression =",
            "CreateStaticMesh",
            "ImportAssetTasks",
        ):
            self.assertNotIn(forbidden, self.r32_asset_factory_cpp)
        self.assertEqual(
            4,
            self.r32_asset_factory_cpp.count("CONTINUOUS_MACRO_MESO_COLOUR"),
        )

    def test_r32_actor_selects_existing_translations_and_configures_render_only_hisms(self) -> None:
        for token in (
            "GetMediumDistanceTurfSourceProfilesR32(",
            "InSourceGroundVegetation->GetWorld() != World",
            "RankedSourceIndices.Sort(",
            "SelectionHashSalt",
            "VariantHashSalt",
            "ProfileVariantQuotas[Profile][Variant]",
            "Source.GetTranslation()",
            "PresentationRotation",
            "TipHeightCm < MinimumTipHeightCm",
            "TipHeightCm > MaximumTipHeightCm",
            "Mesh->GetBoundingBox()",
            "MeshMaximumHeightCm[Variant]",
            "Mesh->GetNumLODs() != 1",
            "RenderData->LODResources[0].GetNumTriangles() !=",
            "MeshTriangleCounts[Variant]",
            'MaterialSlotName != FName(TEXT("R29Grass"))',
            "VariantTotals[Variant] += Transforms.Num()",
            "VariantTotals[Variant] != VariantInstanceCounts[Variant]",
            "SetCullDistances(",
            "ECollisionEnabled::NoCollision",
            "SetCanEverAffectNavigation(false)",
            "SetCastShadow(false)",
            "SetCastContactShadow(false)",
            "bHiddenInSceneCapture = false",
            "SetVisibility(true, true)",
            "SetHiddenInGame(false, true)",
            "SetActorEnableCollision(false)",
            "GrassComponents",
            "ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_VALID",
            "translationsPreservedExactly=true",
            "sourceRotationsPreservedExactly=false",
            "visibleInSceneCapture=true",
            "rgbSimulationPresentationOnly=true",
            "visualCaptureAccepted=false",
            "targetHardwarePerformanceGateRequired=true",
            "exactLod0TriangleCensusValidated=true",
            "nativeMapApplicationClaimed=false",
            "collision=false",
            "navigation=false",
            "sensorAuthority=false",
            "rfAuthority=false",
            "geospatialAuthority=false",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertIn("Component->ComponentTags.Num() == 1", self.actor_cpp)
        self.assertIn("Tags.Num() != 1", self.actor_cpp)
        self.assertNotIn("SourceIndex += SelectionDenominator", self.actor_cpp)
        self.assertNotIn("SourceIndex %", self.actor_cpp)
        self.assertNotIn("Ordinal %", self.actor_cpp)
        self.assertGreaterEqual(
            self.actor_cpp.count("SourceGroundVegetation->GetWorld() != GetWorld()"),
            1,
        )
        self.assertNotIn("SetMobility(EComponentMobility::Movable)", self.actor_cpp)

    def test_readme_does_not_overstate_source_only_delivery(self) -> None:
        readme_flat = " ".join(self.readme.split())
        for text in (
            "appearance-only source implementation",
            "18,432 owned transforms",
            "4,608 existing world translations exactly",
            "3,115 manicured, 994 humid, 288",
            "12",
            "three existing R29 modeled-blade meshes",
            "5--9 cm",
            "65--90 m",
            "repository source only",
            "Native visual acceptance therefore remains false",
            "First commit R30",
            "Then commit R31",
            "no survey",
        ):
            self.assertIn(text, readme_flat)


if __name__ == "__main__":
    unittest.main()
