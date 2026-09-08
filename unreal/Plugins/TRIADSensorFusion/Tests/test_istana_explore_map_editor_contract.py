from __future__ import annotations

import json
import math
import re
import struct
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
RUNTIME = PLUGIN / "Source" / "TRIADSensorFusion"
EDITOR = PLUGIN / "Source" / "TRIADSensorFusionEditor"
SOURCE_PACK = REPO / "unreal" / "SourceAssets" / "IstanaPublicViewExploreV1"

EDITOR_HEADER = EDITOR / "Public" / "TRIADIstanaExploreEditorLibrary.h"
EDITOR_SOURCE = EDITOR / "Private" / "TRIADIstanaExploreEditorLibrary.cpp"
EDITOR_BUILD_RULES = EDITOR / "TRIADSensorFusionEditor.Build.cs"
LANDSCAPE_HEADER = RUNTIME / "Public" / "TRIADIstanaExploreLandscapeActor.h"
LANDSCAPE_SOURCE = RUNTIME / "Private" / "TRIADIstanaExploreLandscapeActor.cpp"
GAME_MODE_HEADER = RUNTIME / "Private" / "TRIADIstanaExploreGameMode.h"
GAME_MODE_SOURCE = RUNTIME / "Private" / "TRIADIstanaExploreGameMode.cpp"
CONTRACT = SOURCE_PACK / "explore_v1.contract.json"

SOURCE_MAP = "/Game/Maps/Istana_PublicView_Exterior_v5"
DESTINATION_MAP = "/Game/Maps/Istana_PublicView_Explore_v1"
EXPLORE_GAME_MODE = "/Script/TRIADSensorFusion.TRIADIstanaExploreGameMode"
LANDSCAPE_CLAIM = (
    "ISTANA_LIKE_PUBLIC_REFERENCE_LANDSCAPE_APPROXIMATION_"
    "NOT_BOTANICAL_INVENTORY_NOT_SURVEY_CONTROLLED"
)


def function_body(source: str, signature: str) -> str:
    """Return one C++ function body, retaining nested initializer braces."""
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated C++ function: {signature}")


def f32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def deterministic_near_grass_census() -> tuple[int, int]:
    """Mirror UE 5.5 FRandomStream float behavior and authored draw order."""
    seed = 0x1757A6A5

    def fraction() -> float:
        nonlocal seed
        seed = (seed * 196314165 + 907633515) & 0xFFFFFFFF
        bits = 0x3F800000 | (seed & 0x007FFFFF)
        return f32(struct.unpack("<f", struct.pack("<I", bits))[0] - f32(1.0))

    def random_range(low: float, high: float) -> float:
        return f32(
            f32(low)
            + f32(f32(f32(high) - f32(low)) * fraction())
        )

    # Mature trees: scale and yaw.
    for _ in range(36):
        random_range(0.82, 1.13)
        random_range(0.0, 360.0)

    # Paired shrub terraces: X/Y jitter, scale and yaw.
    for _side in range(2):
        for _row in range(16):
            for _column in range(12):
                random_range(-0.45, 0.45)
                random_range(-0.50, 0.50)
                random_range(0.72, 1.18)
                random_range(0.0, 360.0)

    # Foundation strip: Y jitter, yaw and scale.
    for _side in range(2):
        for _index in range(24):
            random_range(-0.35, 0.35)
            random_range(0.0, 360.0)
            random_range(0.42, 0.68)

    # Deeper groundcover: X/Y jitter, yaw and scale.
    for _side in range(2):
        for _row in range(5):
            for _column in range(18):
                random_range(-0.28, 0.28)
                random_range(-0.35, 0.35)
                random_range(0.0, 360.0)
                random_range(0.35, 0.58)

    accepted = 0
    excluded = 0
    for x_index in range(-28, 29):
        for y_index in range(49):
            x = f32(f32(f32(x_index) * f32(2.45)) + random_range(-0.34, 0.34))
            y = f32(
                f32(29.0)
                + f32(f32(y_index) * f32(2.65))
                + random_range(-0.34, 0.34)
            )
            inside_axis = abs(x) < f32(16.0) and y < f32(122.0)
            fountain_y = f32(y - f32(95.0))
            distance_squared = f32(f32(x * x) + f32(fountain_y * fountain_y))
            inside_fountain = distance_squared < f32(f32(17.0) * f32(17.0))
            if inside_axis or inside_fountain:
                excluded += 1
                continue
            accepted += 1
            random_range(0.0, 360.0)
            random_range(0.82, 1.18)
    return accepted, excluded


class IstanaExploreMapEditorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (
            EDITOR_HEADER,
            EDITOR_SOURCE,
            EDITOR_BUILD_RULES,
            LANDSCAPE_HEADER,
            LANDSCAPE_SOURCE,
            GAME_MODE_HEADER,
            GAME_MODE_SOURCE,
            CONTRACT,
        )
        missing = [path for path in required if not path.is_file()]
        if missing:
            raise AssertionError(
                "Istana Explore map/editor source set is incomplete: "
                + ", ".join(str(path) for path in missing)
            )
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")
        cls.editor_build_rules = EDITOR_BUILD_RULES.read_text(encoding="utf-8")
        cls.landscape_header = LANDSCAPE_HEADER.read_text(encoding="utf-8")
        cls.landscape_source = LANDSCAPE_SOURCE.read_text(encoding="utf-8")
        cls.game_mode_header = GAME_MODE_HEADER.read_text(encoding="utf-8")
        cls.game_mode_source = GAME_MODE_SOURCE.read_text(encoding="utf-8")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))

    def assert_ordered(self, source: str, *needles: str) -> None:
        cursor = -1
        for needle in needles:
            position = source.find(needle, cursor + 1)
            self.assertGreater(
                position,
                cursor,
                f"missing or out-of-order source contract token: {needle}",
            )
            cursor = position

    def test_01_compile_clean_editor_boundary_and_entrypoints(self) -> None:
        for include in (
            '#include "Components/HierarchicalInstancedStaticMeshComponent.h"',
            '#include "Components/StaticMeshComponent.h"',
            '#include "Factories/FbxStaticMeshImportData.h"',
            '#include "GameFramework/WorldSettings.h"',
            '#include "StaticMeshCompiler.h"',
        ):
            self.assertIn(include, self.editor_source)
        for obsolete in (
            '#include "Engine/WorldSettings.h"',
            '#include "StaticMeshCompilingManager.h"',
        ):
            self.assertNotIn(obsolete, self.editor_source)
        for dependency in (
            '"TRIADSensorFusion"',
            '"UnrealEd"',
            '"AssetTools"',
        ):
            self.assertIn(dependency, self.editor_build_rules)
        identity = function_body(
            self.editor_source,
            "ValidateIstanaExploreRemoteControlProject(",
        )
        self.assertEqual(identity.count("FPaths::NormalizeDirectoryName("), 3)
        self.assertIn('EndsWith(TEXT(".uproject")', identity)
        self.assertIn("FPaths::IsSamePath(Actual, Expected)", identity)
        for entrypoint in (
            "BuildIstanaExploreV1Map",
            "ValidateIstanaExploreV1Map",
            "ValidateIstanaExploreV1PlayWorld",
            "QuiesceIstanaExploreV1PlayWorldForStop",
        ):
            self.assertIn(entrypoint, self.editor_header)

    def test_02_contract_and_map_paths_are_exactly_additive(self) -> None:
        self.assertEqual(
            self.contract["schema"],
            "triad.istana_public_view_explore_v1.contract.v1",
        )
        self.assertEqual(self.contract["revision"], "EXPLORE_V1_ADDITIVE")
        self.assertEqual(self.contract["sourceMap"], SOURCE_MAP)
        self.assertEqual(self.contract["destinationMap"], DESTINATION_MAP)
        self.assertIn(
            f'const FString SourceMapPackage(TEXT("{SOURCE_MAP}"));',
            self.editor_source,
        )
        self.assertIn(
            f'const FString DestinationMapPackage(TEXT("{DESTINATION_MAP}"));',
            self.editor_source,
        )
        self.assertIn(
            'TEXT("/Game/Maps/Istana_PublicView_Explore_v1.'
            'Istana_PublicView_Explore_v1")',
            self.editor_source,
        )

        build = function_body(self.editor_source, "BuildIstanaExploreV1Map(")
        self.assert_ordered(
            build,
            "HasDirtyPackages(Error)",
            "FPackageName::DoesPackageExist(DestinationMapPackage)",
            "SourceWorld->GetOutermost()->GetName() != SourceMapPackage",
            "ValidateIstanaPublicViewHeroV5Map(SourceV5Validation)",
            "CaptureProtectedMapBytes(ProtectedMapBytes, Error)",
            "DuplicateLoadedAsset(SourceWorld, DestinationMapPackage)",
            "PreserveOnlyOuterLegacyTrees(Scene)",
            "ValidateWorld(TargetWorld, BeforeSave)",
            "SaveLoadedAsset(TargetWorld, false)",
            "LoadMap(Filename)",
            "ValidateWorld(Reloaded, Persisted)",
            "ValidateProtectedMapBytesUnchanged(ProtectedMapBytes, Error)",
        )
        self.assertIn("IDEMPOTENT_EXPLORE_V1_MAP_ALREADY_VALID", build)
        self.assertIn(
            "destination exists and is not the open exact validated map",
            build,
        )
        for forbidden in (
            "SaveLoadedAsset(SourceWorld",
            "SourceWorld->Modify",
            "SourceWorld->MarkPackageDirty",
            "DuplicateLoadedAsset(TargetWorld",
        ):
            self.assertNotIn(forbidden, build)

    def test_03_v1_through_v5_are_snapshotted_and_byte_preserved(self) -> None:
        protected = self.contract["protected"]
        self.assertEqual(
            protected,
            {
                "v1ThroughV5MapsUnchanged": True,
                "distantOsmBuildingsUnchanged": True,
                "heroBuildingUnchanged": True,
                "fixedQaCamerasRetainedButNotAutoActivatedInExploreMap": True,
            },
        )
        roster = function_body(self.editor_source, "ProtectedMapPackages()")
        for version in range(1, 5):
            self.assertEqual(
                roster.count(
                    f'TEXT("/Game/Maps/Istana_PublicView_Exterior_v{version}")'
                ),
                1,
            )
        self.assertEqual(roster.count("SourceMapPackage"), 1)
        self.assertNotIn("Explore", roster)
        self.assertNotIn("Exterior_v6", roster)

        capture = function_body(self.editor_source, "bool CaptureProtectedMapBytes(")
        for token in (
            "FPackageName::DoesPackageExist(PackageName, &Record.Filename)",
            "FFileHelper::LoadFileToArray(Record.Bytes, *Record.Filename)",
            "Record.Bytes.IsEmpty()",
            "OutRecords.Add(MoveTemp(Record))",
        ):
            self.assertIn(token, capture)
        readback = function_body(
            self.editor_source, "bool ValidateProtectedMapBytesUnchanged("
        )
        for token in (
            "Records.Num() != ProtectedMapPackages().Num()",
            "ProtectedMapPackages().Contains(Record.PackageName)",
            "FPaths::IsSamePath(Filename, Record.Filename)",
            "FFileHelper::LoadFileToArray(CurrentBytes, *Filename)",
            "CurrentBytes != Record.Bytes",
        ):
            self.assertIn(token, readback)

    def test_04_osm_mesh_material_transform_and_state_are_preserved(self) -> None:
        snapshot_start = self.editor_source.index("struct FDistantContextSnapshot")
        snapshot_end = self.editor_source.index(
            "const TArray<FString>& ProtectedMapPackages", snapshot_start
        )
        snapshot = self.editor_source[snapshot_start:snapshot_end]
        for field in (
            "MeshPath",
            "MaterialPaths",
            "RelativeTransform",
            "WorldTransform",
            "CollisionEnabled",
            "CollisionResponses",
            "bVisible",
            "bHiddenInGame",
            "bActive",
            "bAutoActivate",
            "bGenerateOverlapEvents",
        ):
            self.assertIn(field, snapshot)

        capture = function_body(self.editor_source, "bool CaptureDistantContext(")
        compare = function_body(self.editor_source, "bool AreDistantContextsIdentical(")
        for token in (
            "GetStaticMesh()->GetPathName()",
            "GetNumMaterials()",
            "GetMaterial(Index)",
            "GetRelativeTransform()",
            "GetComponentTransform()",
            "GetCollisionEnabled()",
            "GetCollisionResponseToChannel",
            "IsVisible()",
            "bHiddenInGame",
            "IsActive()",
            "bAutoActivate",
            "GetGenerateOverlapEvents()",
        ):
            self.assertIn(token, capture)
        for field in (
            "MeshPath",
            "MaterialPaths",
            "RelativeTransform",
            "WorldTransform",
            "CollisionEnabled",
            "CollisionResponses",
            "bVisible",
            "bHiddenInGame",
            "bActive",
            "bAutoActivate",
            "bGenerateOverlapEvents",
        ):
            self.assertIn(f"Expected.{field}", compare)
            self.assertIn(f"Actual.{field}", compare)

        build = function_body(self.editor_source, "BuildIstanaExploreV1Map(")
        self.assert_ordered(
            build,
            "CaptureDistantContext(\n            SourceScene->OSMContextBuildingsComponent.Get()",
            "DuplicateLoadedAsset(SourceWorld, DestinationMapPackage)",
            "CaptureDistantContext(\n            Scene ? Scene->OSMContextBuildingsComponent.Get()",
            "AreDistantContextsIdentical(",
            "RecordPreservedDistantContext(",
            "ValidateWorld(TargetWorld, BeforeSave)",
            "ValidateWorld(Reloaded, Persisted)",
        )
        osm_lines = [
            line
            for line in self.editor_source.splitlines()
            if "OSMContextBuildingsComponent" in line
        ]
        self.assertEqual(len(osm_lines), 5)
        self.assertTrue(all(".Get()" in line for line in osm_lines))

        record = function_body(
            self.landscape_source, "RecordPreservedDistantContext("
        )
        validate = function_body(
            self.landscape_source, "ValidatePreservedDistantContext("
        )
        self.assertIn(
            "/Game/TRIAD/IstanaPublicView/Context/"
            "SM_IstanaPublicView_OSMContextBuildings."
            "SM_IstanaPublicView_OSMContextBuildings",
            record,
        )
        for token in (
            "PreservedDistantContextMeshPath",
            "PreservedDistantContextMaterialPaths",
            "PreservedDistantContextRelativeTransform",
            "PreservedDistantContextWorldTransform",
        ):
            self.assertIn(token, record)
            self.assertIn(token, validate)
        for token in (
            "!Component->IsVisible()",
            "Component->bHiddenInGame",
            "!Component->IsActive()",
            "!Component->bAutoActivate",
            "ECollisionEnabled::NoCollision",
            "Component->GetGenerateOverlapEvents()",
            "!IgnoresAllChannels(Component)",
        ):
            self.assertIn(token, validate)

    def test_05_legacy_tree_partition_is_exact_and_transform_preserving(self) -> None:
        landscape = self.contract["landscape"]
        self.assertEqual(landscape["innerLegacyTreeRemovalRadiusMeters"], 250.0)
        self.assertEqual(landscape["outerLegacyTreeInstancesPreserved"], 560)
        self.assertRegex(
            self.editor_source,
            r"InnerLegacyTreeRadiusCentimeters\s*=\s*25000\.0",
        )
        self.assertRegex(
            self.editor_source,
            r"ExpectedPreservedOuterLegacyTrees\s*=\s*560",
        )
        count = function_body(self.editor_source, "int32 CountTreeInstances(")
        for component in (
            "RainTreeInstances",
            "PalmTreeInstances",
            "FramingTreeInstances",
        ):
            self.assertEqual(count.count(component), 1)

        prune = function_body(self.editor_source, "int32 PreserveOnlyOuterLegacyTrees(")
        for component in (
            "RainTreeInstances.Get()",
            "PalmTreeInstances.Get()",
            "FramingTreeInstances.Get()",
        ):
            self.assertEqual(prune.count(component), 1)
        for token in (
            "GetInstanceTransform(Index, Transform, false)",
            "Transform.GetTranslation().Size2D() >= InnerLegacyTreeRadiusCentimeters",
            "Preserved.Add(Transform)",
            "Component->ClearInstances()",
            "Component->AddInstance(Transform, false)",
            "PreservedCount += Preserved.Num()",
        ):
            self.assertIn(token, prune)
        self.assertNotIn("OSMContextBuildingsComponent", prune)

        build = function_body(self.editor_source, "BuildIstanaExploreV1Map(")
        validate_world = function_body(self.editor_source, "bool ValidateWorld(")
        self.assertIn("CountTreeInstances(SourceScene) != 720", build)
        self.assertIn(
            "PreserveOnlyOuterLegacyTrees(Scene) != ExpectedPreservedOuterLegacyTrees",
            build,
        )
        self.assertIn(
            "CountTreeInstances(Scene) != ExpectedPreservedOuterLegacyTrees",
            validate_world,
        )

    def test_06_deterministic_greenery_census_and_truth_claims_are_exact(self) -> None:
        expected = {
            "broadleafFrameTreeExact": 36,
            "layeredShrubAExact": 304,
            "layeredShrubBExact": 152,
            "clippedHedgeExact": 124,
            "foundationGroundcoverExact": 228,
            "nearGrassExact": 2325,
            "deterministicPlacementSeed": 0x1757A6A5,
        }
        landscape_contract = self.contract["landscape"]
        self.assertEqual(
            {name: landscape_contract[name] for name in expected}, expected
        )
        constants = {
            "ExpectedTreeCount": 36,
            "ExpectedShrubACount": 304,
            "ExpectedShrubBCount": 152,
            "ExpectedHedgeCount": 124,
            "ExpectedGroundcoverCount": 228,
            "ExpectedGrassCount": 2325,
        }
        for name, value in constants.items():
            self.assertRegex(
                self.landscape_source,
                rf"constexpr int32 {name}\s*=\s*{value};",
            )

        positions = function_body(
            self.landscape_source, "BroadleafFramePositionsMeters()"
        )
        pairs = [
            (float(x), float(y))
            for x, y in re.findall(
                r"\{\s*(-?\d+\.\d+)\s*,\s*(-?\d+\.\d+)\s*\}", positions
            )
        ]
        self.assertEqual(len(pairs), 36)
        self.assertEqual(len(set(pairs)), 36)
        self.assertGreaterEqual(
            min(math.hypot(x, y) for x, y in pairs),
            90.0,
        )

        populate = function_body(self.landscape_source, "PopulateDeterministicLandscape(")
        self.assert_ordered(
            populate,
            "ClearAllInstances()",
            "FRandomStream Random(DeterministicPlacementSeed)",
            "BroadleafFramePositionsMeters()",
            "ShrubInstancesB",
            "HedgeInstances->AddInstance",
            "GroundcoverInstances->AddInstance",
            "NearGrassInstances->AddInstance",
            "ValidateExploreLandscape(Report)",
        )
        self.assertEqual(populate.count("FormalBedRandom.FRandRange("), 4)
        self.assertEqual(
            populate.count("Random.FRandRange(")
            - populate.count("FormalBedRandom.FRandRange("),
            17,
        )
        for pattern in (
            r"SideIndex\s*=\s*0;\s*SideIndex\s*<\s*2",
            r"Row\s*=\s*0;\s*Row\s*<\s*16",
            r"Column\s*=\s*0;\s*Column\s*<\s*12",
            r"\(Row \+ Column \+ SideIndex\) % 3 == 0",
            r"FRandomStream FormalBedRandom\(0x0B3D2024\)",
            r"Row\s*=\s*0;\s*Row\s*<\s*3",
            r"Column\s*=\s*0;\s*Column\s*<\s*12",
            r"83\.0f \+ Row \* 5\.0f",
            r"Index\s*=\s*0;\s*Index\s*<\s*31",
            r"Index\s*=\s*0;\s*Index\s*<\s*24",
            r"Row\s*=\s*0;\s*Row\s*<\s*5",
            r"Column\s*=\s*0;\s*Column\s*<\s*18",
            r"XIndex\s*=\s*-28;\s*XIndex\s*<=\s*28",
            r"YIndex\s*=\s*0;\s*YIndex\s*<=\s*48",
        ):
            self.assertRegex(populate, pattern)
        for nondeterministic in (
            "FMath::Rand",
            "FPlatformTime",
            "FDateTime",
            "FGuid",
        ):
            self.assertNotIn(nondeterministic, populate)
        self.assertEqual(deterministic_near_grass_census(), (2325, 468))

        validate = function_body(
            self.landscape_source,
            "bool ATRIADIstanaExploreLandscapeActor::ValidateExploreLandscape(",
        )
        exact_component_bindings = {
            "ShrubInstancesA": "ExpectedShrubACount",
            "ShrubInstancesB": "ExpectedShrubBCount",
            "HedgeInstances": "ExpectedHedgeCount",
            "GroundcoverInstances": "ExpectedGroundcoverCount",
            "NearGrassInstances": "ExpectedGrassCount",
            "BroadleafTreeInstances": "ExpectedTreeCount",
        }
        for component, census in exact_component_bindings.items():
            self.assertIn(
                f"ValidateVisualComponent({component}, {census}", validate
            )
        visual = function_body(self.landscape_source, "ValidateVisualComponent(")
        self.assertIn("GetInstanceCount() != ExpectedInstances", visual)
        self.assertNotIn("GetInstanceCount() < ExpectedInstances", visual)

        self.assertEqual(self.contract["claim"], LANDSCAPE_CLAIM)
        self.assertEqual(
            self.contract["referenceEpoch"],
            "2009_FITTING_COMPOSITION_WITH_2024_TARGET_EPOCH_MISMATCH_DISCLOSED",
        )
        self.assertEqual(
            self.contract["unresolved"],
            [
                "TARGET_EPOCH_PLANTING_INVENTORY",
                "EXACT_TREE_SPECIES_AND_AGE",
                "EXACT_SHRUB_AND_TURF_CULTIVARS",
                "SIDE_AND_REAR_LANDSCAPE_SURVEY",
            ],
        )
        self.assertIn(f'TEXT("{LANDSCAPE_CLAIM}")', self.landscape_header)
        for false_claim in (
            "bExactSpeciesOrCultivarsClaimed = false",
            "bExactIndividualTreePlacementClaimed = false",
            "bVegetationUsedForSensorTruth = false",
        ):
            self.assertIn(false_claim, self.landscape_header)
        for validation_gate in (
            "bExactSpeciesOrCultivarsClaimed",
            "bExactIndividualTreePlacementClaimed",
            "bVegetationUsedForSensorTruth",
            "DeterministicPlacementSeed != 0x1757A6A5",
        ):
            self.assertIn(validation_gate, validate)

    def test_07_game_mode_policy_cameras_and_start_are_exact(self) -> None:
        self.assertRegex(
            self.game_mode_header,
            r"ATRIADIstanaExploreGameMode\s*\n?\s*:\s*public\s+"
            r"ATRIADIstanaAirSimGameMode",
        )
        self.assertIn(
            "DefaultPawnClass = ATRIADIstanaFreeRoamPawn::StaticClass()",
            self.game_mode_source,
        )
        self.assertIn("HUDClass = nullptr", self.game_mode_source)
        self.assertIn("ChoosePlayerStart_Implementation", self.game_mode_header)
        choose_start = function_body(
            self.game_mode_source, "ChoosePlayerStart_Implementation("
        )
        for token in (
            'TEXT("TRIADIstanaExplorePlayerStartV1")',
            "TActorIterator<APlayerStart>",
            "It->Tags.Contains(ExploreStartTag)",
            "if (ExactStart)",
            "Super::ChoosePlayerStart_Implementation(Player)",
        ):
            self.assertIn(token, choose_start)

        self.assertIn(
            f'TEXT("{EXPLORE_GAME_MODE}")', self.editor_source
        )
        build = function_body(self.editor_source, "BuildIstanaExploreV1Map(")
        for token in (
            "Policy->bEnforceFixedPrimaryCamera = false",
            "Policy->bRequireIstanaAirSimGameMode = true",
            "Policy->RequiredGameModeClassPath = ExploreGameModeClassPath",
            "SetCameraAutoActivationDisabled(*It, Error)",
            "DefaultGameMode = ExploreGameMode",
            "Parameters.OverrideLevel = TargetWorld->PersistentLevel",
            "StartParameters.OverrideLevel = TargetWorld->PersistentLevel",
            'StartParameters.Name = TEXT("TRIADIstanaExplorePlayerStartV1")',
            "FVector(0.0, 21000.0, 2400.0)",
            "FRotator StartRotation(-5.0f, -90.0f, 0.0f)",
            "Start->Tags.AddUnique(ExplorePlayerStartTag)",
        ):
            self.assertIn(token, build)
        self.assertEqual(build.count("OverrideLevel = TargetWorld->PersistentLevel"), 2)
        self.assertNotIn("Destroy()", build)

        camera_set = function_body(
            self.editor_source, "bool SetCameraAutoActivationDisabled("
        )
        camera_get = function_body(
            self.editor_source, "bool IsCameraAutoActivationDisabled("
        )
        for helper in (camera_set, camera_get):
            self.assertIn("FindFProperty<FByteProperty>", helper)
            self.assertIn('TEXT("AutoActivateForPlayer")', helper)
            self.assertIn("EAutoReceiveInput::Disabled", helper)
        self.assertIn("SetPropertyValue_InContainer", camera_set)
        self.assertIn("GetPropertyValue_InContainer", camera_get)
        self.assertNotIn("AutoActivateForPlayer =", self.editor_source)

        validate_world = function_body(self.editor_source, "bool ValidateWorld(")
        for token in (
            "ExploreGameModeDefaults->DefaultPawnClass !=",
            "ATRIADIstanaFreeRoamPawn::StaticClass()",
            "ExploreGameModeDefaults->HUDClass != nullptr",
            "DefaultGameMode.Get() != ExploreGameMode",
            "Policy->bEnforceFixedPrimaryCamera",
            "!Policy->bRequireIstanaAirSimGameMode",
            "Policy->RequiredGameModeClassPath != ExploreGameModeClassPath",
            "IsCameraAutoActivationDisabled(*It)",
            "StartCount != 1 || CameraCount < 1",
        ):
            self.assertIn(token, validate_world)
        exploration = self.contract["exploration"]
        self.assertTrue(exploration["defaultEnabledOnPlayInExploreMap"])
        self.assertTrue(exploration["visualNavigationOnly"])
        self.assertTrue(exploration["airSimHudAndVehicleSpawningDisabled"])

    def test_08_pie_readback_is_identity_and_possession_gated(self) -> None:
        play = function_body(
            self.editor_source, "ValidateIstanaExploreV1PlayWorld("
        )
        self.assert_ordered(
            play,
            "ValidateWorld(EditorWorld, EditorReport)",
            "GEditor->PlayWorld",
            "PlayWorld->WorldType != EWorldType::PIE",
            "GEditor->IsSimulatingInEditor()",
            "UWorld::RemovePIEPrefix",
            "PlayWorld->GetAuthGameMode()",
            "UGameplayStatics::GetPlayerController(PlayWorld, 0)",
            "Cast<ATRIADIstanaFreeRoamPawn>(PlayerController->GetPawn())",
            "PlayerController->GetViewTarget() != ExplorePawn",
            "Landscape->ValidateExploreLandscape(LandscapeReport)",
            "Landscape->ValidatePreservedDistantContext(",
        )
        for token in (
            "ActiveGameMode->GetClass() != ExploreGameMode",
            "ActiveGameMode->HUDClass != nullptr",
            "HudCount != 0",
            "!ExplorePawn->HasActorBegunPlay()",
            "!Policy->HasActorBegunPlay()",
            "Policy->bEnforceFixedPrimaryCamera",
            "Policy->RequiredGameModeClassPath != ExploreGameModeClassPath",
            "!Landscape->HasActorBegunPlay()",
        ):
            self.assertIn(token, play)

        quiesce = function_body(
            self.editor_source, "QuiesceIstanaExploreV1PlayWorldForStop("
        )
        self.assertIn("ValidateIstanaExploreV1PlayWorld(Readiness)", quiesce)
        self.assertIn("no AirSim HUD/SimMode exists", quiesce)
        self.assertNotRegex(quiesce, r"\bEditorRequestEndPlay\s*\(")

    def test_09_broadleaf_pbr_roster_is_explicit_exact_and_fail_closed(self) -> None:
        for include in (
            '#include "EditorFramework/AssetImportData.h"',
            '#include "Engine/Texture2D.h"',
            '#include "Factories/MaterialFactoryNew.h"',
            '#include "Factories/TextureFactory.h"',
            '#include "MaterialEditingLibrary.h"',
            '#include "MaterialDomain.h"',
            '#include "Materials/MaterialExpressionTextureSample.h"',
            '#include "Misc/SecureHash.h"',
        ):
            self.assertIn(include, self.editor_source)

        lock = json.loads(
            (SOURCE_PACK / "explore_v1_sources.lock.json").read_text(
                encoding="utf-8"
            )
        )
        pngs = [row for row in lock["files"] if row["path"].endswith(".png")]
        self.assertEqual(len(pngs), 10)
        texture_specs = function_body(
            self.editor_source, "BroadleafTextureSpecs()"
        )
        for row in pngs:
            asset_name = Path(row["path"]).stem
            self.assertEqual(texture_specs.count(f'TEXT("{asset_name}")'), 1)
            self.assertIn(str(row["bytes"]), texture_specs)
            self.assertIn(row["md5"], texture_specs)
        self.assertIn(
            'TEXT("Sources/PolyHaven/jacaranda_tree_1k/textures")',
            self.editor_source,
        )

        source_gate = function_body(
            self.editor_source, "bool ValidateBroadleafTextureSourceRoster("
        )
        for token in (
            "BroadleafTextureSpecs().Num() != 10",
            "IFileManager::Get().FileSize(*Source) != Spec.ExpectedBytes",
            "FMD5Hash::HashFile(*Source)",
            "LexToString(Hash).Equals(Spec.ExpectedMd5",
            "UniqueNames.Contains(Spec.AssetName)",
        ):
            self.assertIn(token, source_gate)

        texture_import = function_body(
            self.editor_source, "bool ImportBroadleafTextures("
        )
        for token in (
            "UTextureFactory",
            "Factory->bCreateMaterial = false",
            "Factory->CompressionSettings = BroadleafTextureCompression",
            "Factory->bFlipNormalMapGreenChannel =",
            "Task->Filename = BroadleafTextureSourcePath(Spec)",
            "Task->bReplaceExisting = false",
            "Tasks.Num() != 10 || TaskByName.Num() != 10",
            "AssetTools.ImportAssetTasks(Tasks)",
            "Texture->bFlipGreenChannel =",
            "Texture->VirtualTextureStreaming = false",
            "ValidateBroadleafTexture(Texture, Spec, OutError)",
            "OutTextures.Num() != 10",
        ):
            self.assertIn(token, texture_import)
        texture_validate = function_body(
            self.editor_source, "bool ValidateBroadleafTexture("
        )
        for token in (
            "Texture->Source.GetSizeX() != 1024",
            "Texture->Source.GetSizeY() != 1024",
            "Texture->AssetImportData->ExtractFilenames()",
            "SourceFiles.Num() == 1",
            "CurrentHash != *ImportedHash",
            "BroadleafTextureCompression(Spec.Usage)",
            "Spec.Usage == EBroadleafTextureUsage::NormalOpenGl",
        ):
            self.assertIn(token, texture_validate)

        material_specs = function_body(
            self.editor_source, "BroadleafMaterialSpecs()"
        )
        for material_name in (
            "jacaranda_tree_trunk",
            "jacaranda_tree_branches",
            "jacaranda_tree_leaves",
        ):
            self.assertIn(f'TEXT("{material_name}")', material_specs)
        create = function_body(
            self.editor_source, "UMaterial* CreateBroadleafMaterial("
        )
        self.assert_ordered(
            create,
            "AssetTools.CreateAsset(",
            "AddBroadleafTextureSample(Material, Diffuse",
            "AddBroadleafTextureSample(Material, Normal",
            "AddBroadleafTextureSample(Material, Roughness",
            "AddBroadleafTextureSample(Material, Alpha",
            "EditorOnly->BaseColor.Connect(0, DiffuseSample)",
            "EditorOnly->Normal.Connect(0, NormalSample)",
            "EditorOnly->Roughness.Connect(1, RoughnessSample)",
            "EditorOnly->OpacityMask.Connect(1, AlphaSample)",
            "EditorOnly->SubsurfaceColor.Connect(0, DiffuseSample)",
            "UMaterialEditingLibrary::RecompileMaterial(Material)",
            "ValidateBroadleafMaterial(Material, Spec, OutError)",
        )
        for token in (
            "Material->BlendMode = Spec.bLeaves ? BLEND_Masked : BLEND_Opaque",
            "Material->TwoSided = Spec.bLeaves",
            "Material->bUsedWithInstancedStaticMeshes = true",
            "Spec.bLeaves ? MSM_TwoSidedFoliage : MSM_DefaultLit",
        ):
            self.assertIn(token, create)

        validate_material = function_body(
            self.editor_source, "bool ValidateBroadleafMaterial("
        )
        for token in (
            "EditorOnly->BaseColor",
            "EditorOnly->Normal",
            "EditorOnly->Roughness",
            "EditorOnly->OpacityMask",
            "EditorOnly->SubsurfaceColor.Expression != DiffuseSample",
            "HasOnlyShadingModel(\n                MSM_TwoSidedFoliage)",
            "HasOnlyShadingModel(MSM_DefaultLit)",
            "ExpressionCollection.Expressions.Num() !=",
            "ExpectedExpressions.Contains(Expression)",
            "!Material->bUsedWithInstancedStaticMeshes",
        ):
            self.assertIn(token, validate_material)

        create_lawn = function_body(
            self.editor_source, "UMaterial* CreateExploreLawnMaterial("
        )
        self.assert_ordered(
            create_lawn,
            "LoadExact<UMaterial>(SharedLawnMaterialObjectPath)",
            "SharedLawn->GetOutermost()->IsDirty()",
            "AssetTools.DuplicateAsset(",
            "ExploreLawn->bUsedWithInstancedStaticMeshes = true",
            "UMaterialEditingLibrary::RecompileMaterial(ExploreLawn)",
            "ValidateExploreLawnMaterial(ExploreLawn, OutError)",
            "OutAssetsToSave.Add(ExploreLawn)",
        )
        validate_lawn = function_body(
            self.editor_source, "bool ValidateExploreLawnMaterial("
        )
        for token in (
            "ExploreLawnMaterialObjectPath",
            "!ExploreLawn->bUsedWithInstancedStaticMeshes",
            "SharedLawn->GetOutermost()->IsDirty()",
        ):
            self.assertIn(token, validate_lawn)

        resolve = function_body(
            self.editor_source, "bool ResolveBroadleafMaterialRoster("
        )
        for token in (
            "Broadleaf->GetStaticMaterials().Num() != 3",
            "Slot.MaterialSlotName != Slot.ImportedMaterialSlotName",
            "BroadleafTrunkSlotName",
            "BroadleafBranchSlotName",
            "BroadleafLeafSlotName",
            "BroadleafTrunkMaterialObjectPath",
            "BroadleafBranchMaterialObjectPath",
            "BroadleafLeafMaterialObjectPath",
            "!OutRoster.Trunk || !OutRoster.Branches || !OutRoster.Leaves",
        ):
            self.assertIn(token, resolve)

        bind = function_body(
            self.editor_source, "bool BindBroadleafMaterials("
        )
        for token in (
            "Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(",
            "BoundIndices.Contains(Index)",
            "Slot.MaterialSlotName = Spec.SlotName",
            "Slot.ImportedMaterialSlotName = Spec.SlotName",
            "Slot.MaterialInterface = Material",
            "ValidateBroadleafMaterialRoster(Broadleaf, OutError)",
        ):
            self.assertIn(token, bind)

        validate_assets = function_body(
            self.editor_source, "bool ValidateImportedAssets("
        )
        self.assertIn(
            "ValidateExactExploreAssetOutputRoster(OutError)", validate_assets
        )
        self.assertIn(
            "bAllowMissingPersistedUvMetadataMarker", validate_assets
        )
        self.assertEqual(
            validate_assets.count("ValidateGeneratedMeshUvMetadataState("),
            2,
        )
        self.assertIn("ValidateBroadleafTextureRoster(OutError)", validate_assets)
        self.assertIn(
            "ValidateBroadleafMaterialRoster(Broadleaf, OutError)", validate_assets
        )
        self.assertIn("ValidateExploreLawnMaterial(Lawn, OutError)", validate_assets)
        self.assertIn(
            "ValidateBroadleafRuntimeLods(Broadleaf, OutError)", validate_assets
        )

        validate_uv_metadata = function_body(
            self.editor_source,
            "bool ValidateGeneratedMeshMaterialUvChannelData(",
        )
        for token in (
            "!Mesh->GetRenderData()",
            "Mesh->GetStaticMaterials().IsEmpty()",
            "MaterialIndex < Mesh->GetStaticMaterials().Num()",
            "!Slot.UVChannelData.bInitialized",
            "Slot.UVChannelData.bOverrideDensities",
            "!FMath::IsFinite(Density)",
            "Density < 0.0f",
        ):
            self.assertIn(token, validate_uv_metadata)

        rebuild_uv_metadata = function_body(
            self.editor_source,
            "bool RebuildGeneratedMeshMaterialUvChannelData(",
        )
        self.assert_ordered(
            rebuild_uv_metadata,
            "Slot.UVChannelData.bInitialized = false",
            "Slot.UVChannelData.bOverrideDensities = false",
            "Density = 0.0f",
            "Mesh->UpdateUVChannelData(true)",
            "ValidateGeneratedMeshMaterialUvChannelData(",
        )

        for token in (
            'TEXT("TRIAD.IstanaExploreV1.GeneratedMeshUvMetadataSchema")',
            "schema=1;asset=SM_IstanaPublicViewExploreV1_Broadleaf_A;slots=3;source_lods=4;uv_density=render_derived",
            "schema=1;asset=SM_IstanaPublicViewExploreV1_GrassClump;slots=1;source_lods=1;uv_density=render_derived",
        ):
            self.assertIn(token, self.editor_source)
        marker_status = function_body(
            self.editor_source, "GetGeneratedMeshUvMetadataMarkerStatus("
        )
        for token in (
            "Metadata->FindValue(",
            "EGeneratedMeshUvMetadataMarkerStatus::Missing",
            "EGeneratedMeshUvMetadataMarkerStatus::Exact",
            "EGeneratedMeshUvMetadataMarkerStatus::Foreign",
        ):
            self.assertIn(token, marker_status)
        validate_marker = function_body(
            self.editor_source, "bool ValidateGeneratedMeshUvMetadataState("
        )
        negative_marker_gates = {
            "absent marker": (
                "MarkerStatus == EGeneratedMeshUvMetadataMarkerStatus::Missing",
                "bAllowMissingMarker",
                "is missing its persisted UV-metadata schema marker",
            ),
            "foreign marker": (
                "MarkerStatus == EGeneratedMeshUvMetadataMarkerStatus::Foreign",
                "has a foreign UV-metadata schema marker",
            ),
        }
        for case, tokens in negative_marker_gates.items():
            with self.subTest(case=case):
                for token in tokens:
                    self.assertIn(token, validate_marker)
        self.assertIn(
            "ValidateGeneratedMeshMaterialUvChannelData(", validate_marker
        )
        stamp_marker = function_body(
            self.editor_source, "bool StampGeneratedMeshUvMetadataSchema("
        )
        self.assert_ordered(
            stamp_marker,
            "Metadata->SetValue(",
            "Metadata->GetValue(",
        )

        exact_roster = function_body(
            self.editor_source, "bool ValidateExactExploreAssetOutputRoster("
        )
        for token in (
            "ObjectPaths.Num() != 16",
            "FPackageName::DoesPackageExist(PackageName, &Filename)",
            "IFileManager::Get().FindFilesRecursive(",
            "ActualFiles.Num() != ExpectedFilenames.Num()",
            "!ExpectedFilenames.Contains(ActualFile)",
        ):
            self.assertIn(token, exact_roster)
        with self.subTest(case="partial roster"):
            self.assertIn(
                "ActualFiles.Num() != ExpectedFilenames.Num()", exact_roster
            )

        repair_uv_metadata = function_body(
            self.editor_source, "TryRepairGeneratedMeshUvMetadata("
        )
        self.assert_ordered(
            repair_uv_metadata,
            "ValidateImportedAssets(PreflightError, true)",
            "GetGeneratedMeshUvMetadataMarkerStatus(Mesh)",
            "MarkerStatus != EGeneratedMeshUvMetadataMarkerStatus::Missing",
            "RepairableObjectPaths.Contains(Mesh->GetPathName())",
            "RebuildGeneratedMeshMaterialUvChannelData(",
            "StampGeneratedMeshUvMetadataSchema(Mesh, OutError)",
            "ChangedMeshes.Add(Mesh)",
            "Assets->SaveLoadedAsset(Mesh, false)",
            "ValidateImportedAssets(OutError)",
        )
        self.assertIn("BroadleafObjectPath, GrassObjectPath", repair_uv_metadata)
        validate_or_repair = function_body(
            self.editor_source, "bool ValidateOrRepairImportedAssets("
        )
        self.assert_ordered(
            validate_or_repair,
            "ValidateImportedAssets(OutError)",
            "TryRepairGeneratedMeshUvMetadata(OutError)",
        )

        configure_lods = function_body(
            self.editor_source, "bool ConfigureBroadleafRuntimeLods("
        )
        for token in (
            "Broadleaf->SetNumSourceModels(BroadleafSourceLodCount)",
            "Broadleaf->bAutoComputeLODScreenSize = false",
            "EStaticMeshReductionTerimationCriterion::Triangles",
            "Reduction.MaxNumOfTriangles = Spec.MaximumTriangles",
            "Reduction.TextureImportance = EMeshFeatureImportance::Highest",
            "Reduction.ShadingImportance = EMeshFeatureImportance::Highest",
            "Reduction.bRecalculateNormals = false",
            "Broadleaf->SetMinLODIdx(BroadleafRuntimeMinLod)",
            "ValidateBroadleafRuntimeLods(Broadleaf, OutError)",
        ):
            self.assertIn(token, configure_lods)
        validate_lods = function_body(
            self.editor_source, "bool ValidateBroadleafRuntimeLods("
        )
        for token in (
            "Broadleaf->GetDefaultMinLOD() != BroadleafRuntimeMinLod",
            "Broadleaf->GetMinLODIdx() != BroadleafRuntimeMinLod",
            "RenderData->LODResources.Num() != BroadleafSourceLodCount",
            "Triangles > static_cast<int32>(Spec.MaximumTriangles)",
            "Lod.GetNumTexCoords() < 1",
            "MaterialIndices.Num() != 3",
            ".GetVertexUV(SampleVertex, 0).ContainsNaN()",
            ".VertexTangentZ(SampleVertex).ContainsNaN()",
        ):
            self.assertIn(token, validate_lods)
        for token in (
            "BroadleafTreeInstances->ForcedLodModel = 0",
            "BroadleafTreeInstances->bOverrideMinLOD = true",
            "BroadleafTreeInstances->MinLOD = 1",
            "Component->GetStaticMesh()->GetMinLODIdx() != 1",
        ):
            self.assertIn(token, self.landscape_source)
        mesh_import = function_body(self.editor_source, "MakeMeshImportTask(")
        self.assertIn("Options->bImportMaterials = false", mesh_import)
        self.assertIn("Options->bImportTextures = false", mesh_import)
        importer = function_body(
            self.editor_source, "ImportIstanaExploreV1Assets("
        )
        self.assert_ordered(
            importer,
            "AnyExploreAssetOutputExists()",
            "ValidateBroadleafTextureSourceRoster(Error)",
            "ImportBroadleafTextures(",
            "CreateBroadleafMaterials(",
            "CreateExploreLawnMaterial(",
            "MakeMeshImportTask(",
            "AssetTools.ImportAssetTasks(Tasks)",
            "BindBroadleafMaterials(Broadleaf, Materials, Error)",
            "ConfigureBroadleafRuntimeLods(Broadleaf, Error)",
            "Grass->GetStaticMaterials().Add(FStaticMaterial(",
            "RebuildGeneratedMeshMaterialUvChannelData(\n            Broadleaf",
            "RebuildGeneratedMeshMaterialUvChannelData(\n            Grass",
            "StampGeneratedMeshUvMetadataSchema(Broadleaf, Error)",
            "StampGeneratedMeshUvMetadataSchema(Grass, Error)",
            "AssetsToSave.Num() != 16",
            "Assets->SaveLoadedAsset(Object, false)",
            "ValidateImportedAssets(Error)",
        )
        self.assertIn(
            "ValidateOrRepairImportedAssets(Error, bRepairedUvMetadata)",
            importer,
        )
        build = function_body(self.editor_source, "BuildIstanaExploreV1Map(")
        self.assertIn(
            "ValidateOrRepairImportedAssets(Error, bRepairedUvMetadata)", build
        )
        self.assertIn(
            "partial or invalid 16-asset destination roster already exists",
            importer,
        )


if __name__ == "__main__":
    unittest.main()
