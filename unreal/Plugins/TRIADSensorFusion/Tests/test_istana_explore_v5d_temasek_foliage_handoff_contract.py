import importlib.util
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path


PLUGIN = Path(__file__).resolve().parents[1]
UNREAL = PLUGIN.parents[1]
R24 = (
    UNREAL
    / "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R24TemasekShophouse"
)
LEGACY_GENERATED = R24 / "Generated"
BUILDER_PATH = R24 / "build_temasek_shophouse_r24.py"
VALIDATOR_PATH = R24 / "validate_temasek_shophouse_r24.py"
RUNTIME = PLUGIN / "Source/TRIADSensorFusion"
EDITOR = PLUGIN / "Source/TRIADSensorFusionEditor"
LANDMARK_H = (
    RUNTIME / "Public/TRIADIstanaExploreV5DLandmarkVegetationActor.h"
)
LANDMARK_CPP = (
    RUNTIME / "Private/TRIADIstanaExploreV5DLandmarkVegetationActor.cpp"
)
PROVENANCE_H = (
    RUNTIME / "Public/TRIADIstanaExploreV5DTemasekShophouseProvenance.h"
)
PROVENANCE_CPP = (
    RUNTIME / "Private/TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp"
)
TEMASEK_ACTOR_CPP = (
    RUNTIME / "Private/TRIADIstanaExploreV5DTemasekShophouseActor.cpp"
)
FACTORY_CPP = (
    EDITOR / "Private/TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp"
)
EDITOR_CPP = (
    EDITOR / "Private/TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp"
)


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def normalized_component(geometry: dict, component: dict) -> dict:
    vertex_first = component["vertexFirst"]
    vertex_count = component["vertexCount"]
    triangle_first = component["triangleFirst"]
    triangle_count = component["triangleCount"]
    vertices = geometry["vertices"][vertex_first : vertex_first + vertex_count]
    triangles = []
    for a, b, c, _component_index, _material_index in geometry["triangles"][
        triangle_first : triangle_first + triangle_count
    ]:
        triangles.append([a - vertex_first, b - vertex_first, c - vertex_first])
    return {
        "id": component["id"],
        "category": component["category"],
        "materialSlot": component["materialSlot"],
        "semanticRoles": component["semanticRoles"],
        "boundsMeters": component["boundsMeters"],
        "closedTwoManifold": component["closedTwoManifold"],
        "edgeCount": component["edgeCount"],
        "signedVolumeCubicMeters": component["signedVolumeCubicMeters"],
        "vertices": vertices,
        "triangles": triangles,
    }


class TemasekFoliageHandoffContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.builder = load_module("temasek_r24_phase2_builder", BUILDER_PATH)
        sys.modules["build_temasek_shophouse_r24"] = cls.builder
        cls.validator = load_module("temasek_r24_phase2_validator", VALIDATOR_PATH)
        cls.temp = tempfile.TemporaryDirectory(prefix="triad_temasek_foliage_handoff_")
        cls.generated = Path(cls.temp.name)
        cls.manifest = cls.builder.build(cls.generated)
        cls.validation = cls.validator.validate(cls.generated, check_determinism=True)
        cls.contract = read_json(R24 / "temasek_shophouse_r24.contract.json")
        cls.geometry = read_json(
            cls.generated / "IstanaPublicViewV5DR24TemasekShophouse.geometry.json"
        )
        cls.legacy_geometry = read_json(
            LEGACY_GENERATED
            / "IstanaPublicViewV5DR24TemasekShophouse.geometry.json"
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def test_v2_foliage_layout_is_exact_and_owned_by_landmark_actor(self) -> None:
        layout = self.contract["foliageLayout"]
        self.assertEqual(
            "triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1",
            layout["schema"],
        )
        self.assertEqual(
            "ATRIADIstanaExploreV5DLandmarkVegetationActor",
            layout["renderOwnerClass"],
        )
        self.assertEqual(
            [
                {
                    "id": "mature_tree_01",
                    "localPositionMeters": [-18.5, -19.0, 0.0],
                    "legacyProxyHeightMeters": 6.8,
                    "landmarkMeshRole": "UMBRELLA",
                },
                {
                    "id": "mature_tree_02",
                    "localPositionMeters": [18.5, -18.8, 0.0],
                    "legacyProxyHeightMeters": 7.3,
                    "landmarkMeshRole": "DOME",
                },
                {
                    "id": "mature_tree_03",
                    "localPositionMeters": [-13.2, -16.2, 0.0],
                    "legacyProxyHeightMeters": 6.4,
                    "landmarkMeshRole": "HIGH_FORK",
                },
            ],
            layout["treeAnchors"],
        )
        self.assertEqual(
            {
                "translationCentimeters": [40411.657951, 88424.311139, 0.0],
                "rotationDegrees": [0.0, -162.5152283523459, 0.0],
                "scale3D": [1.053891, 1.221655, 1.0],
            },
            layout["sourcePlacementTransform"],
        )

    def test_baked_foliage_categories_are_explicit_zero(self) -> None:
        expected = {
            "MATURE_TREE_TRUNK": 0,
            "MATURE_TREE_BRANCH": 0,
            "MATURE_TREE_CANOPY": 0,
            "ROOF_TERRACE_FOLIAGE": 0,
            "NATIVE_SHRUB_CLUSTER": 0,
            "POLLINATOR_FLOWER_STEM": 0,
            "POLLINATOR_FLOWER_HEAD": 0,
        }
        self.assertEqual(expected, self.contract["foliageLayout"]["bakedRenderCategoryCounts"])
        self.assertEqual(expected, self.manifest["foliageLayout"]["bakedRenderCategoryCounts"])
        categories = self.geometry["counts"]["categories"]
        for category in expected:
            self.assertEqual(0, categories.get(category, 0), category)
        self.assertEqual(0, self.manifest["foliageLayout"]["bakedRenderComponentCount"])

    def test_architecture_hardscape_and_soils_are_byte_equivalent_to_r24(self) -> None:
        omitted = set(self.contract["foliageLayout"]["bakedRenderCategoryCounts"])
        before = {
            row["id"]: normalized_component(self.legacy_geometry, row)
            for row in self.legacy_geometry["components"]
            if row["category"] not in omitted
        }
        after = {
            row["id"]: normalized_component(self.geometry, row)
            for row in self.geometry["components"]
        }
        self.assertEqual(before, after)
        preserved = self.contract["foliageLayout"]["preservedRenderCategoryCounts"]
        self.assertEqual(
            {
                "BUILDING_MASS": 1,
                "ROOF_TERRACE_PLANTER": 4,
                "ROOF_TERRACE_SOIL": 4,
                "FRONTAGE_PAVER": 72,
                "RAIN_GARDEN_SOIL": 2,
                "RAIN_GARDEN_SWALE": 2,
            },
            preserved,
        )
        for category, count in preserved.items():
            self.assertEqual(count, self.geometry["counts"]["categories"][category])
        self.assertEqual(self.legacy_geometry["buildingMassBoundsMeters"], self.geometry["buildingMassBoundsMeters"])
        self.assertEqual(self.legacy_geometry["boundsMeters"], self.geometry["boundsMeters"])

    def test_manifest_and_validator_bind_the_layout_without_authority_escalation(self) -> None:
        self.assertEqual("PASS", self.validation["status"])
        self.assertEqual(self.contract["foliageLayout"], self.manifest["foliageLayout"])
        authority = self.contract["foliageLayout"]["authorityPolicy"]
        self.assertTrue(authority["appearanceOnly"])
        for key in (
            "collisionAuthority",
            "navigationAuthority",
            "sensorOcclusionAuthority",
            "rfGeometryAuthority",
            "rfMaterialAuthority",
            "surveyAuthority",
            "botanicalAuthority",
        ):
            self.assertFalse(authority[key], key)

    def test_cpp_admission_layers_pin_the_same_foliage_contract(self) -> None:
        combined = "\n".join(
            text(path)
            for path in (
                LANDMARK_H,
                LANDMARK_CPP,
                PROVENANCE_H,
                PROVENANCE_CPP,
                TEMASEK_ACTOR_CPP,
                FACTORY_CPP,
                EDITOR_CPP,
            )
        )
        for marker in (
            "ExpectedTemasekFoliageLayoutSchema",
            "ExpectedTemasekTreeAnchorsLocalMeters",
            "FoliageLayoutSchema",
            "FoliageRenderOwnerClass",
            "BakedFoliageRenderComponents = 0",
            "bFoliageRenderedByLandmarkVegetationActor",
            "bakedFoliageRenderComponents=0",
            "foliageTreeAnchors=3",
        ):
            self.assertIn(marker, combined)
        for value in (
            "-18.5",
            "-19.0",
            "18.5",
            "-18.8",
            "-13.2",
            "-16.2",
            "1.053891",
            "1.221655",
        ):
            self.assertIn(value, combined)

    def test_removed_foliage_materials_are_absent_from_future_mesh_contract(self) -> None:
        removed = {
            "M_TSH_Bark",
            "M_TSH_LeafDeep",
            "M_TSH_LeafLight",
            "M_TSH_PollinatorBloom",
        }
        slots = set(self.geometry["materialSlots"])
        self.assertTrue(removed.isdisjoint(slots))
        self.assertEqual(15, self.geometry["counts"]["materials"])
        factory = text(FACTORY_CPP)
        actor = text(TEMASEK_ACTOR_CPP)
        for slot in removed:
            self.assertNotIn(f'TEXT("{slot}")', factory)
            self.assertNotIn(f'TEXT("{slot}")', actor)

    def test_future_generated_receipts_and_census_are_pinned_fail_closed(self) -> None:
        factory = text(FACTORY_CPP)
        provenance = text(PROVENANCE_CPP)
        actor = text(TEMASEK_ACTOR_CPP)
        for path in sorted(self.generated.iterdir()):
            sha = hashlib.sha256(path.read_bytes()).hexdigest().upper()
            self.assertIn(path.name, factory)
            self.assertIn(str(path.stat().st_size), factory)
            self.assertIn(sha, factory)
        for marker in (
            "ExpectedAssetCount = 16",
            "ExpectedSourceVertexCount = 9536",
            "ExpectedTriangleCount = 15760",
            "ExpectedMaterialCount = 15",
            "ExpectedComponentCount = 828",
        ):
            self.assertIn(marker, factory)
        for marker in ("Components = 828", "SourceVertices = 9536", "Triangles = 15760", "MaterialSlots = 15"):
            self.assertIn(marker, provenance)
        self.assertIn("GetNumTriangles() != 15760", actor)
        self.assertIn("Sections.Num() != 15", actor)

    def test_json_schema_pins_the_v2_handoff_shape(self) -> None:
        schema = read_json(R24 / "temasek_shophouse_r24.contract.schema.json")
        self.assertEqual(
            "triad.istana_explore_v5d.r24_temasek_shophouse.contract.v2",
            schema["properties"]["schema"]["const"],
        )
        self.assertIn("foliageLayout", schema["required"])
        foliage = schema["properties"]["foliageLayout"]
        self.assertFalse(foliage["additionalProperties"])
        self.assertEqual(3, foliage["properties"]["treeAnchors"]["minItems"])
        self.assertEqual(3, foliage["properties"]["treeAnchors"]["maxItems"])
        self.assertEqual(15, schema["properties"]["materialSlots"]["minItems"])
        self.assertEqual(15, schema["properties"]["materialSlots"]["maxItems"])


if __name__ == "__main__":
    unittest.main()
