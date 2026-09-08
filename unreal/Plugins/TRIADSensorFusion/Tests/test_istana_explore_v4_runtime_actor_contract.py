from __future__ import annotations

import hashlib
import json
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
RUNTIME_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV4LandscapeActor.h"
)
RUNTIME_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV4LandscapeActor.cpp"
)
AUTOMATION_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV4LandscapeActorTests.cpp"
)
V4_SOURCE_ROOT = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV4"
V8_SOURCE_ROOT = REPO / "unreal/SourceAssets/IstanaPublicViewV8Portico"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class IstanaExploreV4RuntimeActorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = text(RUNTIME_H)
        cls.runtime_cpp = text(RUNTIME_CPP)
        cls.automation_cpp = text(AUTOMATION_CPP)
        cls.runtime_source = "\n".join((cls.runtime_h, cls.runtime_cpp))

    def test_frozen_read_only_inputs_are_bound_by_exact_hash(self) -> None:
        frozen_inputs = {
            V4_SOURCE_ROOT / "explore_v4_geospatial.contract.json": (
                16706,
                "CCD9B5200E03602EC6FA6986F4D3AB70920806C63D718B5B1A943CC29F30D8E7",
            ),
            V4_SOURCE_ROOT / "Prepared/v4_raster_sample_grid.json": (
                170691,
                "262C88795C3C5FB1BB92B2D1CD2D1886B25D5E3885E60F34E407082E4C81C447",
            ),
            V4_SOURCE_ROOT / "explore_v4_vegetation.contract.json": (
                117730,
                "ABDC65AA14DA9AE89736BBE2D75E6212FA38B76B44DBA491C9266DEBC247E104",
            ),
            V8_SOURCE_ROOT
            / "Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.obj": (
                1383806,
                "330B20E8F58289C84EF96CB031D374ADB3AAF52711E5247D1F167C7E94DC4526",
            ),
            V8_SOURCE_ROOT
            / "Generated/IstanaPublicViewV8CentralPorticoDepthOverlayV5Live.manifest.json": (
                6310,
                "3E88B8C4AEF5A499696436920972301EFBDA242BAAA3F64928D1A8B072557943",
            ),
        }
        for path, (byte_count, digest) in frozen_inputs.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, byte_count)
                self.assertEqual(sha256(path), digest)
                self.assertIn(digest, self.runtime_source)

    def test_exact_five_form_and_form_matched_heritage_architecture(self) -> None:
        main_components = (
            "UmbrellaTreeInstances",
            "DomeTreeInstances",
            "HighForkRoundedTreeInstances",
            "ColumnarNarrowTreeInstances",
            "PalmTreeInstances",
        )
        heritage_components = (
            "HeritageUmbrellaInstances",
            "HeritageDomeInstances",
            "HeritageHighForkRoundedInstances",
            "HeritageColumnarNarrowInstances",
            "HeritagePalmInstances",
        )
        for component in (*main_components, *heritage_components):
            self.assertIn(
                f"TObjectPtr<UHierarchicalInstancedStaticMeshComponent> {component};",
                self.runtime_h,
            )
            self.assertIn(
                f"{component} = CreateDefaultSubobject<"
                "UTRIADIstanaExploreV4SynchronousHismComponent>",
                self.runtime_cpp,
            )
        self.assertIn("DistinctTreeMeshes.Num() != 5", self.runtime_cpp)
        self.assertIn("MainTreeTotal != ExpectedInheritedTreeCount", self.runtime_cpp)
        self.assertNotIn("CrownWidthFactor", self.runtime_cpp)
        self.assertIn("TargetHeightMeters = FMath::Lerp(", self.runtime_cpp)
        self.assertIn("20.0, MaximumUmbrellaHeightMeters", self.runtime_cpp)
        self.assertIn(": 24.28810830713951", self.runtime_cpp)
        self.assertIn("TargetHeightMeters = 36.5", self.runtime_cpp)
        self.assertIn("double TargetHeightMeters = 28.3", self.runtime_cpp)
        self.assertIn("FVector TargetScale = Source.GetScale3D()", self.runtime_cpp)
        self.assertIn("FrozenV2ColumnarStartIndex = 272", self.runtime_cpp)
        self.assertIn("FrozenV2ColumnarCount = 232", self.runtime_cpp)
        self.assertIn("? Source", self.runtime_cpp)
        self.assertIn("Frozen V2 columnar census is exact", self.automation_cpp)
        self.assertIn("Palm runtime census is exactly zero", self.automation_cpp)
        self.assertIn(
            "const int32 ExpectedHeritageFormCounts[5] = {4, 4, 1, 0, 0};",
            self.runtime_cpp,
        )
        self.assertIn("HeritageTotal != ExpectedHeritageAnchorCount", self.runtime_cpp)
        self.assertIn("HeritageAnchorPawnBlockers", self.runtime_h)
        self.assertIn(
            "bFormComesFromPublishedQualitativeHint",
            self.runtime_source,
        )
        self.assertIn(
            "seven published qualitative hints, two explicitly generic dome fallbacks",
            self.runtime_cpp,
        )

    def test_v4_hism_postload_is_synchronous_density_closed_and_exactly_gated(
        self,
    ) -> None:
        self.assertIn(
            "class TRIADSENSORFUSION_API "
            "UTRIADIstanaExploreV4SynchronousHismComponent final",
            self.runtime_h,
        )
        self.assertIn(
            "virtual void OnPostLoadPerInstanceData() override;",
            self.runtime_h,
        )
        postload = self.runtime_cpp[
            self.runtime_cpp.index(
                "void UTRIADIstanaExploreV4SynchronousHismComponent::\n"
                "    OnPostLoadPerInstanceData()"
            ) : self.runtime_cpp.index("namespace\n{")
        ]
        for marker in (
            "bEnableDensityScaling = false;",
            "CurrentDensityScaling = 1.0f;",
            "bCanEnableDensityScaling = false;",
            "RF_ClassDefaultObject | RF_ArchetypeObject",
            "IsCompiling()",
            "Algo::AnyOf(",
            "PerInstanceSMData.Num() != InstanceReorderTable.Num()",
            "GetActiveLightingScenario() != OwnerLevel",
            "NumBuiltRenderInstances = NumBuiltInstances;",
            "InstanceCountToRender = NumBuiltInstances;",
            "BuildTreeIfOutdated(false, bForceTreeBuild);",
        ):
            self.assertIn(marker, postload)
        self.assertNotIn(
            "UInstancedStaticMeshComponent::OnPostLoadPerInstanceData", postload
        )
        self.assertNotIn(
            "UHierarchicalInstancedStaticMeshComponent::OnPostLoadPerInstanceData",
            postload,
        )
        self.assertNotIn("BuildTreeIfOutdated(true", postload)

        self.assertEqual(
            self.runtime_cpp.count(
                "CreateDefaultSubobject<"
                "UTRIADIstanaExploreV4SynchronousHismComponent>"
            ),
            16,
        )
        self.assertNotIn(
            "CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>",
            self.runtime_cpp,
        )
        for marker in (
            "struct FV4HismColdGateRow",
            "Rows.Num() != ExpectedHismComponentCount",
            "Component->GetClass() ==",
            "UTRIADIstanaExploreV4SynchronousHismComponent::StaticClass()",
            "RenderInstances == ActualInstances",
            "label=%s component=%s classOk=%s expectedInstances=%d "
            "actualInstances=%d renderInstances=%d actualMesh=%s "
            "expectedMesh=%s meshOk=%s "
            "relativeIdentity=%s collision=%d expectedCollision=%d "
            "overlap=%s channelsOk=%s autoRebuild=%s async=%s "
            "fullyBuilt=%s visible=%s expectedVisible=%s "
            "hiddenInGame=%s expectedHiddenInGame=%s densityScaling=%s",
            "ExpectedTotalHismInstanceCount == 21794",
            "ExpectedVisibleHismInstanceCount == 21785",
            "const int32 RequiredVisibleComponents = ExpectedVisibleHismComponentCount -",
            "bRenderVisualsHiddenByV5BSuccessor ? 5 : 0",
            "ExpectedUnderstoreyCount +\n                ExpectedGeometryGrassCount",
            "ISTANA_EXPLORE_V4_HISM_COLD_GATE_PASS components=%d visible=%d "
            "hiddenBlockers=%d nonEmpty=%d totalInstances=%d "
            "visibleInstances=%d autoRebuild=%d async=%d fullyBuilt=%d "
            "renderCountMatches=%d",
        ):
            self.assertIn(marker, self.runtime_cpp)
        self.assertNotIn(
            "a visible HISM lost identity, mesh, visibility",
            self.runtime_cpp,
        )
        for marker in (
            "Bulk-population HISM uses the exact synchronous V4 subclass",
            "Bulk-population HISM default disables density scaling",
            "Transient empty synchronous-HISM probe exists",
            "Transient empty probe retains zero render instances",
            "Transient empty probe restores unit density",
            "Transient populated synchronous-HISM probe exists",
            "Transient populated probe has one render instance",
            "Transient populated probe restores unit density",
        ):
            self.assertIn(marker, self.automation_cpp)
        self.assertEqual(
            self.automation_cpp.count("OnPostPopulatePerInstanceData();"),
            2,
        )

    def test_runtime_roster_matches_frozen_derivative_semantics(self) -> None:
        contract = json.loads(
            text(V4_SOURCE_ROOT / "explore_v4_vegetation.contract.json")
        )
        self.assertEqual(
            contract["schema"],
            "triad.istana_explore_v4_vegetation_contract.v2",
        )
        expected_slots = {
            "umbrella_tree": [
                "island_tree_02",
                "island_tree_02_leaves",
                "island_tree_02_branches",
            ],
            "dense_dome_tree": [
                "tree_small_02_branches",
                "tree_small_02_leaves",
                "tree_small_02_trunk",
            ],
            "high_fork_tree": [
                "island_tree_01",
                "island_tree_01_leaves",
                "island_tree_01_branches",
            ],
            "columnar_tree": [
                "jacaranda_tree_branches",
                "jacaranda_tree_trunk",
                "jacaranda_tree_leaves",
            ],
            "palm_accent": ["Atlas"],
            "shrub_04_a": ["shrub_04"],
            "periwinkle_06_f": ["periwinkle_plant"],
            "calathea_d": ["calathea_orbifolia_01"],
            "grass_medium_small_a": ["grass_medium_01"],
        }
        derivatives = {
            row["selectionId"]: row
            for row in contract["selectedMeshDerivatives"]
        }
        self.assertEqual(set(derivatives), set(expected_slots))
        for selection_id, slots in expected_slots.items():
            with self.subTest(selection_id=selection_id):
                row = derivatives[selection_id]
                self.assertEqual(row["materialSlotOrder"], slots)
                self.assertEqual(row["runtimeRequiredMinLOD"], 1)
                self.assertFalse(row["rawSourceRuntimeSelected"])
                self.assertIsNone(row["sourceNativeBiologicalGirthMeters"])
                for slot in slots:
                    self.assertIn(f'TEXT("{slot}")', self.runtime_cpp)
        self.assertEqual(
            contract["runtimeSelectionPolicy"][
                "allRuntimeMeshDerivativesRequireMinLODAtLeast"
            ],
            1,
        )
        self.assertFalse(
            contract["runtimeSelectionPolicy"][
                "rawLod0MayBeSelectedByMapOrRuntimeActor"
            ]
        )
        self.assertEqual(
            contract["runtimeSelectionPolicy"][
                "runtimeReferencesMustResolveOnlyUnder"
            ],
            "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation",
        )
        self.assertEqual(
            contract["runtimeSelectionPolicy"][
                "protectedExactRuntimeReferenceExceptions"
            ],
            [
                {
                    "selectionId": "high_fork_tree",
                    "objectPath": "/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_IslandTree01.SM_IPVExploreV3_IslandTree01",
                },
                {
                    "selectionId": "columnar_tree",
                    "objectPath": "/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A.SM_IstanaPublicViewExploreV1_Broadleaf_A",
                },
            ],
        )
        self.assertTrue(
            contract["runtimeSelectionPolicy"]["protectedReferencesAreReadOnly"]
        )
        self.assertTrue(
            contract["runtimeSelectionPolicy"][
                "protectedReferenceMaterialOverridesAreComponentLocal"
            ]
        )
        self.assertFalse(
            contract["runtimeSelectionPolicy"][
                "protectedReferencePackagesMayBeSavedByV4Workflow"
            ]
        )
        self.assertIn("V4VegetationRuntimeObjectRoot", self.runtime_cpp)
        self.assertIn("V4PorticoRuntimeObjectRoot", self.runtime_cpp)
        self.assertIn("ProtectedHighForkRuntimeObjectPath", self.runtime_cpp)
        self.assertIn("ProtectedColumnarRuntimeObjectPath", self.runtime_cpp)
        self.assertIn("IsAdmittedTreeMeshPath", self.runtime_cpp)

    def test_v2_main_and_heritage_placement_policies_are_enforced(self) -> None:
        contract = json.loads(
            text(V4_SOURCE_ROOT / "explore_v4_vegetation.contract.json")
        )
        policy = contract["runtimeSelectionPolicy"]
        main = policy["mainTreePlacement"]
        self.assertEqual(
            main["sourceUnion"],
            {
                "count": 720,
                "orderedV2ComponentCounts": [272, 232, 182, 34],
                "orderedV2ComponentNames": [
                    "UmbrellaBroadleafInstances",
                    "ColumnarBroadleafInstances",
                    "DomeBroadleafInstances",
                    "PalmInstances",
                ],
                "preserveTranslationAndRotation": True,
            },
        )
        self.assertEqual(
            main["columnarInheritance"],
            {
                "selectionId": "columnar_tree",
                "sourceIndexRangeInclusive": [272, 503],
                "instanceCount": 232,
                "transformMode": "EXACT_INHERITED_V2_WORLD_TRANSFORM",
                "rasterBandHeightFitClaimed": False,
                "crownReshapeClaimed": False,
            },
        )
        non_columnar = main["reclassifiedNonColumnar"]
        self.assertEqual(non_columnar["sourceInstanceCount"], 488)
        self.assertEqual(
            non_columnar["allowedSelectionIds"],
            ["umbrella_tree", "dense_dome_tree", "high_fork_tree"],
        )
        self.assertEqual(non_columnar["scaleMode"], "UNIFORM_HEIGHT_ONLY")
        self.assertFalse(non_columnar["nonUniformScaleAllowed"])
        self.assertEqual(
            non_columnar["rules"],
            {
                "umbrella_tree": {
                    "minimumHeightMeters": 20.0,
                    "maximumHeightMeters": 24.28810830713951,
                },
                "dense_dome_tree": {"heightMeters": 36.5},
                "high_fork_tree": {"heightMeters": 28.3},
            },
        )
        self.assertEqual(
            main["palm"],
            {
                "selectionId": "palm_accent",
                "runtimeAssetMayRemainImported": True,
                "freeRoamInstanceCount": 0,
                "closeHeroUseAllowed": False,
            },
        )
        self.assertFalse(main["individualTreeOrSpeciesClaimed"])

        heritage = policy["heritageTreePlacement"]
        self.assertEqual(heritage["anchorCount"], 9)
        self.assertEqual(
            heritage["anchorSource"],
            "explore_v4_geospatial.contract.json heritageTreeAnchors",
        )
        self.assertEqual(
            heritage["visualMeshPolicy"],
            "CLOSEST_AVAILABLE_BROAD_SILHOUETTE_PROXY",
        )
        self.assertEqual(
            heritage["visualScaleMode"], "UNIFORM_PUBLISHED_HEIGHT_ONLY"
        )
        self.assertFalse(heritage["publishedHeightMayBeClampedToMainTreeRange"])
        self.assertFalse(heritage["publishedGirthMayAffectVisualMeshTransform"])
        self.assertEqual(
            heritage["publishedGirthUse"],
            "SEPARATE_QUERY_ONLY_PAWN_BLOCKER_DIAMETER_EQUALS_GIRTH_DIVIDED_BY_PI",
        )
        self.assertFalse(heritage["speciesMeshClaimed"])
        self.assertFalse(heritage["crownTopologyClaimed"])
        self.assertFalse(heritage["meshNativeGirthClaimed"])
        self.assertEqual(
            heritage["formEvidenceCounts"],
            {"publishedQualitativeHint": 7, "explicitGenericFallback": 2},
        )

        for marker in (
            "FrozenV2ColumnarStartIndex = 272",
            "FrozenV2ColumnarCount = 232",
            "Row.WorldTransform.Equals(Source, 0.0f)",
            "FVector(UniformScale)",
            "Palm runtime census is exactly zero",
            "seven published qualitative hints, two explicitly generic dome fallbacks",
        ):
            self.assertIn(marker, self.runtime_source + self.automation_cpp)

    def test_exact_720_reclassification_preserves_translation_and_full_rotation(self) -> None:
        for marker in (
            "constexpr int32 ExpectedInheritedTreeCount = 720;",
            "InheritedV2TreeWorldTransforms",
            "TArray<FTRIADIstanaExploreV4RasterCue> RasterCues",
            "BuildDeterministicReclassifiedRows",
            "HashTreeRow(Index, Cue, Seed)",
            "Source.GetRotation()",
            "Source.GetTranslation()",
            "PreservesSourceTranslationAndRotation",
            "Row.WorldTransform.Equals(Source, 0.0f)",
            "PreservedTreeFormBySourceIndex",
            "MainTreeTotal != ExpectedInheritedTreeCount",
        ):
            self.assertIn(marker, self.runtime_source)
        self.assertIn("TestEqual(TEXT(\"Exactly 720 output rows\")", self.automation_cpp)
        self.assertIn("Translation remains exact", self.automation_cpp)
        self.assertIn("Full source rotation remains exact", self.automation_cpp)
        self.assertIn(
            "Pitch/roll drift with unchanged yaw is rejected", self.automation_cpp
        )
        self.assertIn(
            "Synthetic umbrella census is deterministic", self.automation_cpp
        )
        self.assertIn("FormCounts[0], 162", self.automation_cpp)
        self.assertIn("FormCounts[1], 174", self.automation_cpp)
        self.assertIn("FormCounts[2], 152", self.automation_cpp)
        self.assertIn("Repeat form is deterministic", self.automation_cpp)
        self.assertIn("Repeat transform is deterministic", self.automation_cpp)
        self.assertIn("719 transforms fail closed", self.automation_cpp)

    def test_heritage_visuals_use_height_only_and_girth_is_blocker_only(self) -> None:
        self.assertNotIn("NativeGirth", self.runtime_source)
        population_visual = self.runtime_cpp[
            self.runtime_cpp.index("const double NativeHeightCm = MeshHeightCm(") :
            self.runtime_cpp.index("const double PublishedTrunkDiameterCm =", self.runtime_cpp.index("const double NativeHeightCm = MeshHeightCm("))
        ]
        self.assertIn("PublishedHeightMeters", population_visual)
        self.assertIn("FVector(UniformScale)", population_visual)
        self.assertNotIn("PublishedGirthMeters", population_visual)
        self.assertIn(
            "Anchor.PublishedGirthMeters * CentimetersPerMeter / PI",
            self.runtime_cpp,
        )
        self.assertIn(
            "uniform published-height scaling",
            self.runtime_cpp,
        )
        self.assertIn(
            "ActualVisualTransform.GetScale3D().Equals(",
            self.runtime_cpp,
        )
        self.assertIn(
            "Pawn-only blockers sized from published girth",
            self.runtime_cpp,
        )

    def test_render_only_collision_boundary_and_exact_addition_census(self) -> None:
        expected_counts = {
            "ExpectedHeritageAnchorCount": 9,
            "ExpectedShrubCount": 512,
            "ExpectedFlowerCount": 192,
            "ExpectedUnderstoreyCount": 384,
            "ExpectedGeometryGrassCount": 1536,
            "ExpectedCloseTurfCount": 18432,
        }
        for name, count in expected_counts.items():
            self.assertIn(f"constexpr int32 {name} = {count};", self.runtime_cpp)
        for marker in (
            "Component->SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "Component->SetCollisionResponseToAllChannels(ECR_Ignore)",
            "HeritageAnchorPawnBlockers->SetCollisionEnabled(ECollisionEnabled::QueryOnly)",
            "HeritageAnchorPawnBlockers->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block)",
            "IsPawnOnlyQueryBlocker(HeritageAnchorPawnBlockers)",
            "bRecordedV3CloseTurfVisualsHidden",
            "PreservedCloseTurfWorldTransforms",
            "ActualWorldTransform.Equals(",
            "PreservedCloseTurfWorldTransforms[Index], 0.001f",
            "RecordedExternalV2PawnBlockerCount != ExpectedInheritedTreeCount",
        ):
            self.assertIn(marker, self.runtime_source)
        self.assertNotIn("SameTranslationAndYaw", self.runtime_source)
        self.assertIn(
            "Full close-turf replacement gate rejects scale drift",
            self.automation_cpp,
        )
        self.assertIn(
            "Full close-turf replacement gate rejects pitch/roll drift",
            self.automation_cpp,
        )

        population = self.runtime_cpp[
            self.runtime_cpp.index(
                "bool ATRIADIstanaExploreV4LandscapeActor::PopulateDeterministicLandscape("
            ) : self.runtime_cpp.index(
                "bool ATRIADIstanaExploreV4LandscapeActor::RecordExternalTargetMapState("
            )
        ]
        self.assertNotIn("->AddInstance(", population)
        self.assertEqual(population.count("->AddInstances("), 1)
        self.assertEqual(population.count("BuildTreeIfOutdated(false, true)"), 1)
        self.assertIn("!Entry.Component->IsTreeFullyBuilt()", population)
        self.assertIn("BulkPopulations.Num() != 16", population)
        self.assertIn("if (!Entry.WorldTransforms->IsEmpty())", population)
        self.assertIn(
            "*Entry.WorldTransforms, false, true, false", population
        )
        self.assertIn(
            "Entry.Component->GetInstanceCount() !=", population
        )
        self.assertIn("Entry.WorldTransforms->Num()", population)
        self.assertIn("return FailManualHismPopulation(", population)
        self.assertEqual(
            population.count("return FailManualHismPopulation("), 3
        )
        for zero_count_component in (
            "PalmTreeInstances, &MainTreeWorldTransformsByForm[4]",
            "HeritagePalmInstances, &HeritageWorldTransformsByForm[4]",
        ):
            self.assertIn(zero_count_component, population)

        disable_index = population.index(
            "bAutoRebuildTreeOnInstanceChanges = false;"
        )
        clear_index = population.index("ClearAllInstances();", disable_index)
        bulk_index = population.index("->AddInstances(", clear_index)
        count_index = population.index(
            "Entry.Component->GetInstanceCount() !=", bulk_index
        )
        build_index = population.index(
            "BuildTreeIfOutdated(false, true)", count_index
        )
        async_index = population.index("IsAsyncBuilding()", build_index)
        restore_index = population.index(
            "bAutoRebuildTreeOnInstanceChanges = true;", build_index
        )
        self.assertLess(disable_index, clear_index)
        self.assertLess(clear_index, bulk_index)
        self.assertLess(bulk_index, count_index)
        self.assertLess(count_index, build_index)
        self.assertLess(build_index, async_index)
        self.assertLess(async_index, restore_index)

        for marker in (
            "Exact bulk-population HISM roster count",
            "Bulk-population HISM default restores auto rebuild",
            "Bulk-population HISM default is not async building",
            "Bulk-population HISM default tree is fully built",
            "Outdated HISM tree fails persisted-readiness gate",
        ):
            self.assertIn(marker, self.automation_cpp)
        self.assertIn(
            "!bAsync && bFullyBuilt", self.runtime_cpp
        )
        self.assertIn(
            "!HeritageAnchorPawnBlockers->IsTreeFullyBuilt()",
            self.runtime_cpp,
        )

    def test_portico_and_min_lod_policies_are_fail_closed(self) -> None:
        manifest = json.loads(
            text(
                V8_SOURCE_ROOT
                / "Generated/IstanaPublicViewV8CentralPorticoDepthOverlayV5Live.manifest.json"
            )
        )
        self.assertEqual(manifest["mesh"]["triangleCount"], 6592)
        self.assertEqual(manifest["mesh"]["materialDefinitionCount"], 7)
        self.assertEqual(
            manifest["semanticSha256"].upper(),
            "3F0F48F1B8C07415D5539E91D1F006D3FD9EC3BD7E55A85B16F69883728465D8",
        )
        self.assertIn(manifest["semanticSha256"].upper(), self.runtime_source)
        for marker in (
            "constexpr int32 ExpectedPorticoTriangleCount = 6592;",
            "constexpr int32 ExpectedPorticoMaterialCount = 7;",
            "ExpectedPorticoTrianglesByMaterial[] = {",
            "3100, 1704, 48, 1092, 36, 144, 468",
            'V4VegetationRuntimeObjectRoot + TEXT("Materials/")',
            "ExpectedPorticoMaterialObjectPath",
            "BoundMaterial->GetPathName() !=",
            "PorticoV8RenderOnlyComponent->GetComponentTransform().Equals",
            "ECollisionEnabled::NoCollision",
            "Component->bOverrideMinLOD = true;",
            "Component->MinLOD = MinLod;",
            "ExpectedTreeMeshes[Index]->GetMinLODIdx() < 1",
            "ExpectedPlantMeshes[Index]->GetMinLODIdx() < 1",
        ):
            self.assertIn(marker, self.runtime_cpp)
        self.assertNotIn(
            "BoundMaterial->GetPathName().StartsWith(\n"
            "                V4PorticoRuntimeObjectRoot)",
            self.runtime_cpp,
        )
        self.assertGreaterEqual(
            len(re.findall(r"ConfigureVisualHism\([^;]+?, 1,", self.runtime_cpp)),
            15,
        )

    def test_instance_local_fixed_step_underdamped_wind_contract(self) -> None:
        for marker in (
            "ExpectedWindBindingCount == 18",
            "ExpectedRuntimeWindMidCount == 31",
            "ExpectedMaterialSlotForWindRole",
            "bUsesPerInstanceLocalPosition",
            "UPROPERTY(Transient)",
            "TArray<TObjectPtr<UMaterialInstanceDynamic>> WindMaterialInstances",
            "void TriggerWindGust(float PeakStrengthCm = -1.0f);",
            "while (WindStepAccumulatorSeconds",
            "Steps < 32",
            "RecoveryDampingRatio >= 1.0f",
            "RecoveryDampingRatio, 0.55f",
            "TRIAD_WindResponseScale",
            "HasCompleteRuntimeWindParameterRoster",
            "did not retain its exact TRIAD_WindResponseScale value",
            "animated_close_turf_geometry_cards=true",
            "external_static_lawn_surface_wpo=false",
        ):
            self.assertIn(marker, self.runtime_source)
        report = self.runtime_cpp[
            self.runtime_cpp.index("BuildWindRuntimeStateReport() const") :
            self.runtime_cpp.index("ValidateExploreV4Landscape(")
        ]
        for forbidden in ("Latitude", "Longitude", "SourceUrl", "LocalGroundLocationCm"):
            self.assertNotIn(forbidden, report)

    def test_truth_claims_and_attribution_gate_remain_closed(self) -> None:
        false_claims = (
            "bOneToOneOneKilometerClaimed",
            "bSurveyAccuracyClaimed",
            "bExactBotanicalInventoryClaimed",
            "bIndividualRasterTreeTruthClaimed",
            "bCollisionOrSensorTruthAuthority",
            "bGoogleOrOneMapContentUsed",
            "bPorticoCollisionAuthority",
            "bLowPolyPalmRuntimePlacementAllowed",
        )
        for claim in false_claims:
            self.assertIn(f"bool {claim} = false;", self.runtime_h)
            self.assertIn(claim, self.runtime_cpp)
        self.assertIn(
            "bool bRequiredAttributionPresentedInPublicSurface = false;",
            self.runtime_h,
        )
        self.assertIn("bool bPublicDistributionReady = false;", self.runtime_h)
        self.assertIn("RequiredDistributionAttribution", self.runtime_source)


if __name__ == "__main__":
    unittest.main()
