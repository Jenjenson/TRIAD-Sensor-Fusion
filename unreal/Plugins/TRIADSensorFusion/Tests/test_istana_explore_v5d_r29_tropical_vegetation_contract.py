from __future__ import annotations

import hashlib
import json
import math
import subprocess
import sys
import tempfile
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
    / "R29TropicalDetail"
)
GENERATED = SOURCE / "Generated"
CONTRACT = SOURCE / "r29_tropical_vegetation.contract.json"
MANIFEST = GENERATED / "IstanaPublicViewV5DR29TropicalVegetation.manifest.json"
GENERATOR = SOURCE / "build_r29_tropical_vegetation.py"
OFFLINE_RENDERER = SOURCE / "render_r29_offline_visual_audit.py"
OFFLINE_AUDIT_DIR = SOURCE / "OfflineVisualAudit"
OFFLINE_AUDIT = OFFLINE_AUDIT_DIR / "r29_offline_visual_audit.json"
OFFLINE_IMAGE = OFFLINE_AUDIT_DIR / "r29_normal_distance_source_preview.png"
OFFLINE_README = OFFLINE_AUDIT_DIR / "README.md"
ACTOR_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Public"
    / "TRIADIstanaExploreV5DR29VegetationActor.h"
)
ACTOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DR29VegetationActor.cpp"
)
FACTORY_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR29VegetationAssetFactory.cpp"
)
EDITOR_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaExploreV5DR29VegetationEditorLibrary.h"
)
EDITOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR29VegetationEditorLibrary.cpp"
)
NATIVE_TEST = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "Tests"
    / "TRIADIstanaExploreV5DR29VegetationActorTests.cpp"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def obj_counts(path: Path) -> dict[str, int | set[str]]:
    vertices = texture_coordinates = normals = triangles = 0
    materials: set[str] = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("v "):
            vertices += 1
        elif line.startswith("vt "):
            texture_coordinates += 1
        elif line.startswith("vn "):
            normals += 1
        elif line.startswith("f "):
            triangles += 1
        elif line.startswith("usemtl "):
            materials.add(line.split(maxsplit=1)[1])
    return {
        "vertices": vertices,
        "texture_coordinates": texture_coordinates,
        "normals": normals,
        "triangles": triangles,
        "materials": materials,
    }


def obj_normal_alignment(path: Path) -> tuple[float, float]:
    vertices: list[tuple[float, float, float]] = []
    normals: list[tuple[float, float, float]] = []
    faces: list[tuple[int, int, int]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("v "):
            vertices.append(tuple(map(float, line.split()[1:4])))
        elif line.startswith("vn "):
            normals.append(tuple(map(float, line.split()[1:4])))
        elif line.startswith("f "):
            faces.append(
                tuple(int(field.split("/")[0]) - 1 for field in line.split()[1:4])
            )
    if len(vertices) != len(normals):
        raise AssertionError(f"{path.name}: vertex/normal roster mismatch")
    dots: list[float] = []
    for face in faces:
        a, b, c = (vertices[index] for index in face)
        edge_ab = tuple(b[axis] - a[axis] for axis in range(3))
        edge_ac = tuple(c[axis] - a[axis] for axis in range(3))
        geometric = (
            edge_ab[1] * edge_ac[2] - edge_ab[2] * edge_ac[1],
            edge_ab[2] * edge_ac[0] - edge_ab[0] * edge_ac[2],
            edge_ab[0] * edge_ac[1] - edge_ab[1] * edge_ac[0],
        )
        geometric_length = math.sqrt(sum(value * value for value in geometric))
        if geometric_length <= 1.0e-12:
            raise AssertionError(f"{path.name}: degenerate triangle {face!r}")
        geometric = tuple(value / geometric_length for value in geometric)
        for index in face:
            authored = normals[index]
            authored_length = math.sqrt(sum(value * value for value in authored))
            authored = tuple(value / authored_length for value in authored)
            dots.append(sum(left * right for left, right in zip(geometric, authored)))
    return min(dots), sum(dots) / len(dots)


class R29TropicalVegetationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (
            CONTRACT,
            MANIFEST,
            GENERATOR,
            OFFLINE_RENDERER,
            OFFLINE_AUDIT,
            OFFLINE_IMAGE,
            OFFLINE_README,
            ACTOR_H,
            ACTOR_CPP,
            FACTORY_CPP,
            EDITOR_H,
            EDITOR_CPP,
            NATIVE_TEST,
        ):
            if not path.is_file():
                raise AssertionError(f"missing R29 input: {path}")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        cls.offline_audit = json.loads(OFFLINE_AUDIT.read_text(encoding="utf-8"))
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor = ACTOR_CPP.read_text(encoding="utf-8")
        cls.factory = FACTORY_CPP.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor = EDITOR_CPP.read_text(encoding="utf-8")
        cls.native_test = NATIVE_TEST.read_text(encoding="utf-8")

    def test_generated_modeled_blade_sources_match_exact_manifest(self) -> None:
        aggregate = self.manifest["aggregate"]
        self.assertEqual(3, aggregate["meshCount"])
        self.assertEqual(192, aggregate["modeledBladeCount"])
        self.assertEqual(3072, aggregate["triangleCount"])
        self.assertEqual(9216, aggregate["sourceCornerCount"])
        self.assertGreaterEqual(aggregate["minimumTriangleVertexNormalDot"], 0.98)
        self.assertEqual(sha256(GENERATOR), self.manifest["generatorSha256"])
        material_library = self.manifest["materialLibrary"]
        material_path = GENERATED / material_library["file"]
        self.assertEqual(material_library["bytes"], material_path.stat().st_size)
        self.assertEqual(material_library["sha256"], sha256(material_path))
        for row in self.manifest["meshes"]:
            path = GENERATED / row["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))
            counts = obj_counts(path)
            self.assertEqual(row["vertices"], counts["vertices"])
            self.assertEqual(row["sourceCorners"] // 3, counts["triangles"])
            self.assertEqual(row["vertices"], counts["texture_coordinates"])
            self.assertEqual(row["vertices"], counts["normals"])
            self.assertEqual({"R29Grass"}, counts["materials"])
            self.assertEqual(2, row["crossedRibbonsPerBlade"])
            self.assertEqual(4, row["segmentsPerRibbon"])
            self.assertEqual(0.0, row["bounds"]["minCm"][2])
            self.assertGreaterEqual(row["bounds"]["maxCm"][2], 13.8)
            self.assertLessEqual(row["bounds"]["maxCm"][2], 19.7)
            minimum_dot, mean_dot = obj_normal_alignment(path)
            self.assertGreaterEqual(minimum_dot, 0.98, path.name)
            self.assertGreaterEqual(mean_dot, 0.999, path.name)
            self.assertAlmostEqual(
                row["normalAlignment"]["minimum"], minimum_dot, places=6
            )
            self.assertAlmostEqual(
                row["normalAlignment"]["mean"], mean_dot, places=6
            )
        self.assertEqual(
            self.contract["sourceGeneration"]["meshSha256"],
            [row["sha256"] for row in self.manifest["meshes"]],
        )

    def test_generator_replays_bit_for_bit_without_network_or_random_state(self) -> None:
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            for destination in (Path(first), Path(second)):
                result = subprocess.run(
                    [sys.executable, str(GENERATOR), "--output", str(destination)],
                    check=True,
                    capture_output=True,
                    text=True,
                )
                self.assertIn("R29_TROPICAL_VEGETATION_SOURCE_BUILD_PASS", result.stdout)
            names = sorted(path.name for path in Path(first).iterdir())
            self.assertEqual(names, sorted(path.name for path in GENERATED.iterdir()))
            for name in names:
                self.assertEqual(
                    (Path(first) / name).read_bytes(),
                    (Path(second) / name).read_bytes(),
                    name,
                )
                self.assertEqual(
                    (Path(first) / name).read_bytes(),
                    (GENERATED / name).read_bytes(),
                    name,
                )

    def test_contract_is_honest_about_source_only_delivery_and_authority(self) -> None:
        delivery = self.contract["deliveryState"]
        self.assertTrue(delivery["sourcePackageComplete"])
        for key in (
            "nativeProjectApplied",
            "targetMapMutated",
            "unrealEditorLaunchedForThisDelivery",
            "visualCaptureAccepted",
        ):
            self.assertFalse(delivery[key], key)
        self.assertTrue(delivery["captureRevalidationRequired"])
        truth = self.contract["truthBoundary"]
        self.assertTrue(truth["appearanceOnly"])
        for key, value in truth.items():
            if key != "appearanceOnly":
                self.assertFalse(value, key)
        self.assertGreaterEqual(len(self.contract["knownLimitations"]), 3)

    def test_offline_visual_audit_is_hash_pinned_and_explicitly_non_native(self) -> None:
        contract = self.contract["offlineVisualAudit"]
        audit = self.offline_audit
        self.assertTrue(contract["sourceOnly"])
        self.assertTrue(contract["actualObjGeometryParsed"])
        for key in (
            "unrealRendererUsed",
            "pbrMaterialEvaluation",
            "treeMeshesRendered",
            "surroundingBuildingsRendered",
            "nativeVisualAcceptanceConferred",
        ):
            self.assertFalse(contract[key], key)
        self.assertEqual(45.0, contract["normalDistanceReferenceMeters"])
        self.assertEqual(sha256(OFFLINE_RENDERER), audit["rendererSha256"])
        self.assertEqual(sha256(OFFLINE_IMAGE), audit["image"]["sha256"])
        self.assertEqual(OFFLINE_IMAGE.stat().st_size, audit["image"]["bytes"])
        png = OFFLINE_IMAGE.read_bytes()
        self.assertEqual(b"\x89PNG\r\n\x1a\n", png[:8])
        self.assertEqual(1400, int.from_bytes(png[16:20], "big"))
        self.assertEqual(900, int.from_bytes(png[20:24], "big"))
        self.assertEqual(
            [row["sha256"] for row in self.manifest["meshes"]],
            [row["sha256"] for row in audit["sourceMeshes"]],
        )
        self.assertEqual([64, 56, 72], [row["modeledBladeCount"] for row in audit["sourceMeshes"]])
        self.assertTrue(audit["renderMethod"]["actualObjGeometryParsed"])
        self.assertTrue(audit["renderMethod"]["everyModeledBladeRepresented"])
        self.assertFalse(audit["renderMethod"]["unrealRendererUsed"])
        self.assertFalse(audit["renderMethod"]["pbrMaterialEvaluation"])
        self.assertFalse(audit["renderMethod"]["treeMeshesRendered"])
        self.assertFalse(audit["acceptance"]["nativeVisualCaptureAccepted"])
        self.assertEqual(720, audit["scene"]["carrierInstances"])
        self.assertEqual(45648, audit["scene"]["bladeInstances"])
        self.assertEqual(91296, audit["scene"]["projectedRibbonPolylines"])

    def test_offline_visual_audit_replays_exact_checked_in_png_and_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary)
            result = subprocess.run(
                [sys.executable, str(OFFLINE_RENDERER), "--output", str(destination)],
                check=True,
                capture_output=True,
                text=True,
            )
            self.assertIn("R29_OFFLINE_VISUAL_AUDIT_PASS", result.stdout)
            self.assertIn("nativeAcceptance=false", result.stdout)
            self.assertEqual(
                OFFLINE_IMAGE.read_bytes(),
                (destination / OFFLINE_IMAGE.name).read_bytes(),
            )
            self.assertEqual(
                OFFLINE_AUDIT.read_bytes(),
                (destination / OFFLINE_AUDIT.name).read_bytes(),
            )

    def test_actor_keeps_r28_positions_and_breaks_mesh_repetition(self) -> None:
        grass = self.contract["grassPresentation"]
        self.assertEqual(6144, grass["instanceCount"])
        self.assertEqual(3, grass["meshVariantCount"])
        self.assertEqual(12, grass["profileVariantBucketCount"])
        self.assertEqual(
            [50, 30, 20],
            grass["deterministicMeshVariantSelectionWeightsPercent"],
        )
        self.assertFalse(grass["exactRealizedVariantRatioClaimed"])
        for token in (
            "BuildDeterministicLayoutR28(Source, OutError)",
            "GrassVariantCount = 3",
            "GrassProfileCount = 4",
            "GrassBucketCount = GrassVariantCount * GrassProfileCount",
            "GrassInstanceCount = 6144",
            "Bucket < 5u ? 0 : (Bucket < 8u ? 1 : 2)",
            "Rotation.Pitch = FMath::Lerp(-1.8, 1.8",
            "Rotation.Roll = FMath::Lerp(-2.2, 2.2",
            "Source.GetTranslation()",
            "Scale.Z *= FMath::Lerp(0.50, 0.56",
            "R29 grass did not retain all twelve profile/silhouette combinations",
            "meshVariantSelectionWeightsPercent=50,30,20",
            "exactRealizedVariantRatioClaimed=false",
        ):
            self.assertIn(token, self.actor)

    def test_material_response_survives_normal_view_distance(self) -> None:
        material = self.contract["materialSuccessor"]
        self.assertEqual([18.0, 65.0], material["roughnessResponseMeters"])
        self.assertEqual([18.0, 60.0], material["normalResponseMeters"])
        self.assertEqual(0.10, material["retainedFarNormalResponse"])
        self.assertEqual(0, material["textureInputsAdded"])
        for token in (
            "smoothstep(1800.0,6500.0,distanceCm)",
            "floor(WorldPosition.xy/240.0)",
            "smoothstep(1800.0,6000.0,distanceCm)",
            "float retainedFarResponse=0.10",
            "authoredNormalsFaceAligned=true",
            "minimumTriangleVertexNormalDot=0.987537",
            "R29RoughnessDescription",
            "R29NormalAlphaDescription",
            "ValidateLandmarkGrassMaterialsR28(R28SourceReport)",
            "MaterialLibrarySource",
            "SourceColor->Code != TargetColor->Code",
            "sourceMaterialsModified=false",
        ):
            self.assertIn(token, self.factory)
        self.assertNotIn("EmissiveColor.Connect", self.factory)

    def test_tree_palette_preserves_anchors_and_uses_five_admitted_forms(self) -> None:
        tree = self.contract["treePresentation"]
        self.assertEqual(7, tree["treeCount"])
        self.assertEqual(5, tree["morphologyCueCount"])
        self.assertTrue(tree["r28AnchorTranslationsPreservedExactly"])
        for token in (
            "TreeMorphologyCount = 5",
            "TreeInstanceCount = 7",
            "SM_IPV5D_Tree_Umbrella_NearLOD0",
            "SM_IPV5D_Tree_Dome_NearLOD0",
            "SM_IPV5D_Tree_HighForkRounded_NearLOD0",
            "SM_IPV5D_Tree_ColumnarNarrow_NearLOD0",
            "SM_IPV5D_Tree_Palm_NearLOD0",
            "const int32 MorphologyByOrdinal[] = {0, 1, 2, 3, 4, 0, 1}",
            "R29 tree morphology variation moved an inherited R28 anchor",
            "Component->MinLOD = 0",
            "Component->ForcedLodModel = 0",
            "TreeLodDistanceScale = 1.80f",
        ):
            self.assertIn(token, self.actor + self.factory)

    def test_editor_endpoint_is_asset_only_and_runtime_actor_is_render_only(self) -> None:
        asset_only_editor = self.editor[
            : self.editor.index("ApplyR29VegetationSuccessorToLoadedHybridMap")
        ]
        combined_editor = self.editor_h + asset_only_editor + self.factory
        for token in (
            "BuildOrValidateR29VegetationAssets",
            "ValidateR29VegetationAssets",
            "ConfigureR29VegetationActor",
            "SaveLoadedAssets(OutAssets, false)",
            "bAutoGenerateCollision = false",
            "Section.bEnableCollision = false",
            "mapsSaved=0",
        ):
            self.assertIn(token, combined_editor)
        for forbidden in (
            "SpawnActor",
            "SaveMap(",
            "LoadMap(",
            "D:\\\\triad",
            "CAPSTONE",
        ):
            self.assertNotIn(forbidden, combined_editor)
        for token in (
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCanEverAffectNavigation(false)",
            "bSensorRfOrGeospatialAuthority = false",
            "bBotanicalSurveyAsBuiltOrCurrentSeasonClaimed = false",
            "mutuallyExclusiveWithR28RenderOwner=true",
            "visualCaptureAccepted=false",
        ):
            self.assertIn(token, self.actor)
        self.assertNotIn("ClaimLabel::GetData", self.actor)

    def test_guarded_map_swap_validates_everything_before_removing_r28(self) -> None:
        transaction = self.editor[
            self.editor.index("ApplyR29VegetationSuccessorToLoadedHybridMap") :
        ]
        for token in (
            "ValidateIstanaExploreV5DR28VisualSuccessorMap",
            "ValidateAssets",
            "BuildDeterministicLayout",
            "ExpectedGrassInstanceCount",
            "ExpectedTreeInstanceCount",
            "EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_FINAL_MUTATION_GATE",
            "V5DR29TropicalVegetationV1",
            "World->DestroyActor(Roster.R28, true, true)",
            "World->SpawnActor<ATRIADIstanaExploreV5DR29VegetationActor>",
            "coexistenceObserved=false",
            "assetsValidatedBeforeMutation=true",
            "geographyValidatedBeforeMutation=true",
            "grassGeographyExact=true",
            "treeGeographyExact=true",
            "sensorAuthority=false",
            "rfAuthority=false",
            "geospatialAuthority=false",
            "ValidateR29VegetationSuccessorMap",
        ):
            self.assertIn(token, self.editor_h + self.editor)
        validation_position = transaction.index("BuildDeterministicLayout")
        removal_position = transaction.index("World->DestroyActor(Roster.R28")
        spawn_position = transaction.index(
            "World->SpawnActor<ATRIADIstanaExploreV5DR29VegetationActor>"
        )
        save_position = transaction.index("SaveMap(World, TargetMapPackage)")
        self.assertLess(validation_position, removal_position)
        self.assertLess(removal_position, spawn_position)
        self.assertLess(spawn_position, save_position)
        self.assertIn(
            "Roster.R28ClassCount != 0 || Roster.R28TagCount != 0 ||\n"
            "        Roster.R29ClassCount != 1 || Roster.R29TagCount != 1",
            self.editor,
        )

    def test_successor_world_is_composable_with_exactly_one_environment_owner(self) -> None:
        validation = self.contract["successorWorldValidation"]
        self.assertEqual(
            [
                "ATRIADIstanaExploreV5DR28EnvironmentActor",
                "ATRIADIstanaExploreV5DR29FacadeEnvironmentActor",
            ],
            validation["acceptedEnvironmentOwnerExactlyOneOf"],
        )
        self.assertFalse(validation["environmentOwnerCoexistenceAccepted"])
        self.assertFalse(validation["environmentOwnerTagImpersonationAccepted"])
        self.assertFalse(validation["ambiguousEnvironmentRosterAccepted"])
        self.assertTrue(validation["r29FacadeVegetationCoexistenceSupported"])
        self.assertEqual(1, validation["exactPublicRealmActorCount"])
        self.assertEqual(1, validation["exactPublicRealmTagCount"])
        for token in (
            '#include "TRIADIstanaExploreV5DPublicRealmActor.h"',
            "R29FacadeEnvironmentClassPath",
            "ResolveEnvironmentOwnerRoster",
            "ResolvePublicRealmRoster",
            "ValidateCombinedWorldR29FacadeOwner",
            "ReadCombinedWorldR29FacadeProviderReady",
            "Actor->FindFunction(R29FacadeValidationFunction)",
            "FStructOnScope Parameters(Function)",
            "Actor->ProcessEvent(Function, ParameterMemory)",
            "bR28EnvironmentOwner == bR29FacadeEnvironmentOwner",
            "EnvironmentRoster.R28->ValidateR28Environment",
            "PublicRealm->ValidatePublicRealm",
            "PublicRealmClassCount != 1",
            "PublicRealmTagCount != 1",
            "impersonated or dual-owned environment-owner tag",
            "impersonated public-realm owner tag",
            "exactlyOneEnvironmentOwner=true",
            "publicRealmInvariantPreserved=true",
        ):
            self.assertIn(token, self.editor)
        self.assertNotIn(
            '#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"',
            self.editor,
        )

    def test_native_layout_automation_covers_replay_and_exact_censuses(self) -> None:
        for token in (
            "TRIAD.Istana.ExploreV5D.R29Vegetation.DeterministicLayout",
            "BuildDeterministicLayout(A, ErrorA)",
            "BuildDeterministicLayout(B, ErrorB)",
            "R29 layout is exactly replayable",
            "ExpectedGrassInstanceCount()",
            "ExpectedGrassBucketCount()",
            "ExpectedTreeInstanceCount()",
            "ExpectedTreeMorphologyCount()",
            "Grass bucket %d is populated",
            "Tree morphology %d is populated",
        ):
            self.assertIn(token, self.native_test)


if __name__ == "__main__":
    unittest.main()
