from __future__ import annotations

import hashlib
import json
import unittest
from collections import Counter
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
SOURCE = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV5C/Surroundings"
GENERATED = SOURCE / "Generated"
OBJ = GENERATED / "SM_IPV5C_OfficialPreferredSurroundingContext.obj"
MTL = GENERATED / "SM_IPV5C_OfficialPreferredSurroundingContext.mtl"
FEATURES = GENERATED / "IstanaPublicViewV5CSurroundings.features.json"
MANIFEST = GENERATED / "IstanaPublicViewV5CSurroundings.manifest.json"
CONTRACT = SOURCE / "istana_public_view_v5c_surroundings.contract.json"
RUNTIME_H = PLUGIN / "Source/TRIADSensorFusion/Public/TRIADIstanaPublicViewSceneActor.h"
RUNTIME_CPP = PLUGIN / "Source/TRIADSensorFusion/Private/TRIADIstanaPublicViewSceneActor.cpp"
FACTORY_H = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CSurroundingsAssetFactory.h"
FACTORY_CPP = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CSurroundingsAssetFactory.cpp"
EDITOR_H = PLUGIN / "Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5CSurroundingsEditorLibrary.h"
EDITOR_CPP = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CSurroundingsEditorLibrary.cpp"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function: {signature}")


class IstanaExploreV5CSurroundingsIntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = read(RUNTIME_H)
        cls.runtime_cpp = read(RUNTIME_CPP)
        cls.factory_h = read(FACTORY_H)
        cls.factory_cpp = read(FACTORY_CPP)
        cls.editor_h = read(EDITOR_H)
        cls.editor_cpp = read(EDITOR_CPP)
        cls.manifest = json.loads(read(MANIFEST))
        cls.contract = json.loads(read(CONTRACT))

    def test_exact_source_hashes_sizes_and_manifest_census(self) -> None:
        expected = {
            OBJ: (13977769, "774F7E30456B989D0BF9EEB013C10D2688A2B3DE87936529BBB154E83D344C74"),
            MTL: (778, "6BBDA30E125F92EEF36D060404CA6EBB3F7DFC9DF95D7895766A57A99A737CA7"),
            FEATURES: (1960959, "8825DDC93E7C6465B01AD5A5AD23B2E8368F2C825B6F8F8EB1FBBD2DA8138793"),
            MANIFEST: (19852, "CEBFA56EC84E697305A35DA9CCD06B29619AB4500701B4A806B958B415F04F20"),
            CONTRACT: (11730, "52587014FC80459732B1C943056D1724684287B67CA0DE8556AF7E1E2F8C7FBD"),
        }
        combined = self.factory_cpp + self.runtime_h + self.runtime_cpp
        for path, (size, digest) in expected.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, size)
                self.assertEqual(sha256(path), digest)
                self.assertIn(digest, combined)

        selection = self.manifest["selectionMetrics"]
        topology = self.manifest["topologyMetrics"]
        self.assertEqual(selection["selectedTotalFeatureCount"], 1338)
        self.assertEqual(selection["selectedOfficialFeatureCount"], 449)
        self.assertEqual(selection["retainedOsmFallbackFeatureCount"], 889)
        self.assertEqual(selection["officialReplacedOsmFeatureCount"], 416)
        self.assertEqual(topology["outputHoleCount"], 16)
        self.assertEqual(topology["triangleCount"], 35424)
        self.assertAlmostEqual(topology["minimumOutputRadiusMeters"], 304.025284)
        self.assertAlmostEqual(topology["maximumOutputRadiusMeters"], 999.767652)
        self.assertFalse(self.contract["scope"]["containsVegetation"])
        self.assertFalse(self.contract["scope"]["containsTerrain"])
        self.assertEqual(
            self.manifest["groundingMetrics"],
            {
                "foundationSkirtDepthMeters": 1.0,
                "gradeSampleMethod": "POLYGON_PART_REPRESENTATIVE_POINT_BARYCENTRIC_ON_FROZEN_TERRAIN",
                "maximumRoofElevationMeters": 116.041346,
                "maximumSyntheticGradeElevationMeters": 1.774387,
                "minimumMeshWallBaseElevationMeters": -2.332826,
                "minimumSyntheticGradeElevationMeters": -1.332826,
                "physicalGradeOrFoundationClaimed": False,
                "polygonPartCount": 1339,
                "selectedTerrainSubstrateTriangleCount": 7168,
                "terrainGeometryEmitted": False,
            },
        )
        self.assertEqual(
            self.contract["terrainSubstrate"]["sha256"],
            "78AF53572EF53BACB684B5B2103D7BA427999C8223BDDD5C0DE76DEB8B16DD23",
        )
        for key in (
            "physicalGradeClaimed",
            "physicalFoundationClaimed",
            "facadeOrApertureAuthorityClaimed",
            "terrainOrSurveyAuthorityClaimed",
        ):
            self.assertFalse(self.contract["presentationPolicy"][key])
        presentation = self.contract["presentationPolicy"]
        self.assertEqual(
            presentation["surfaceResponseContract"],
            "V3_SCREEN_ADAPTIVE_DUAL_SCALE_BASECOLOR_ROUGHNESS",
        )
        self.assertEqual(presentation["microCellFwidthFade"], [0.3, 0.75])
        self.assertEqual(presentation["apertureFrameFraction"], 0.1)
        self.assertEqual(presentation["apertureFadeCentimetres"], [35000.0, 70000.0])
        self.assertEqual(presentation["macroBayGroupCount"], 4)
        self.assertEqual(presentation["macroStoreyGroupCount"], 4)
        self.assertEqual(presentation["macroToneRange"], [0.9, 1.06])
        self.assertEqual(presentation["glassRoughness"], 0.28)
        self.assertTrue(presentation["materialOnlyNoGeometryChange"])

    def test_obj_has_exact_four_semantic_triangle_census_despite_transitions(self) -> None:
        triangles: Counter[str] = Counter()
        current_material: str | None = None
        transition_count = 0
        for line in OBJ.read_text(encoding="utf-8").splitlines():
            if line.startswith("usemtl "):
                current_material = line.split(maxsplit=1)[1]
                transition_count += 1
            elif line.startswith("f "):
                self.assertIsNotNone(current_material)
                triangles[current_material] += len(line.split()) - 3
        self.assertEqual(
            triangles,
            Counter(
                {
                    "M_IPV5C_OfficialContextRender": 7690,
                    "M_IPV5C_OfficialContextRoof": 2945,
                    "M_IPV5C_OsmFallbackContextRender": 17690,
                    "M_IPV5C_OsmFallbackContextRoof": 7099,
                }
            ),
        )
        self.assertEqual(sum(triangles.values()), 35424)
        self.assertEqual(transition_count, 2678)

    def test_factory_is_hash_gated_isolated_and_render_only(self) -> None:
        combined = self.factory_h + self.factory_cpp + self.editor_cpp
        for marker in (
            "/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings",
            "ExpectedAssetCount = 6",
            "ExpectedTriangleCount = 35424",
            "ExpectedMaterialCount = 4",
            "FreshAssets.Num() != 6",
            "SaveLoadedAssets(FreshAssets, false)",
            "ValidateAllSourceHashes",
            "Actual != Expected",
        ):
            self.assertIn(marker, combined)

        task = function_body(self.factory_cpp, "UAssetImportTask* MakeImportTask()")
        for marker in (
            "bImportMaterials = false",
            "bImportTextures = false",
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "ImportUniformScale = 1.0f",
            "bCombineMeshes = true",
            "bReorderMaterialToFbxOrder = true",
            "bTransformVertexToAbsolute = true",
            "bAutoGenerateCollision = false",
            "bGenerateLightmapUVs = true",
            "FBXNIM_ImportNormals",
            "bBuildNanite = true",
            "bReplaceExisting = false",
            "bSave = false",
        ):
            self.assertIn(marker, task)

        mesh = function_body(self.factory_cpp, "bool ValidateMesh(")
        for marker in (
            "BuildScale3D !=",
            "FVector::OneVector",
            "AggGeom.GetElementCount() != 0",
            "CTF_UseComplexAsSimple",
            "FVector(-98438.3058, -99831.9761, -233.2826)",
            "FVector(99880.8394, 99339.8856, 11604.1346)",
            "Mesh->NaniteSettings.bEnabled",
        ):
            self.assertIn(marker, mesh)

    def test_isolated_texture_free_master_and_exact_non_authoritative_instances(self) -> None:
        ordered = (
            ("M_IPV5C_OfficialContextRender", 7690),
            ("M_IPV5C_OfficialContextRoof", 2945),
            ("M_IPV5C_OsmFallbackContextRender", 17690),
            ("M_IPV5C_OsmFallbackContextRoof", 7099),
        )
        positions = []
        for name, triangle_count in ordered:
            positions.append(self.factory_cpp.index(f'TEXT("{name}")'))
            self.assertIn(str(triangle_count), self.factory_cpp)
        self.assertEqual(positions, sorted(positions))
        self.assertIn("M_IPV5C_ContextMassing_Master", self.factory_cpp)
        self.assertNotIn("MI_IPV5_ContextRender", self.factory_cpp)
        self.assertNotIn("MI_IPV5_ContextRoof", self.factory_cpp)
        creator = function_body(self.factory_cpp, "UMaterial* CreateMassingMaster(")
        master = function_body(self.factory_cpp, "bool ValidateMassingMaster(")
        self.assertIn(
            "Material->bUsedWithNanite = true;\n"
            "    UMaterialEditingLibrary::RecompileMaterial(Material);",
            creator,
        )
        self.assertIn("!Material->GetUsageByFlag(MATUSAGE_Nanite)", master)
        self.assertEqual(1, self.factory_cpp.count("bUsedWithNanite = true;"))
        self.assertEqual(
            1, self.factory_cpp.count("GetUsageByFlag(MATUSAGE_Nanite)")
        )
        for marker in (
            "TRIAD_IPV5C_NONAUTHORITATIVE_CONTEXT_MASSING_V3_SCREEN_ADAPTIVE_DUAL_SCALE_FACADE",
            "ExpressionCollection.Expressions.Num() != 23",
            "MasterScalarNodeSpecs",
            "MasterVectorNodeSpecs",
            "MasterCustomInputSpecs",
            'TEXT("V5C.BayMeters"), TEXT("BayMeters"), 3.0f',
            'TEXT("V5C.StoreyMeters"), TEXT("StoreyMeters"), 3.2f',
            "Parameter->ParameterName != FName(Spec.ParameterName)",
            "Parameter->DefaultValue",
            "Response->Inputs[Index].InputName != FName(Spec.InputName)",
            "WPT_ExcludeAllShaderOffsets",
            "Response->AdditionalDefines.IsEmpty()",
            "Response->OutputType != CMOT_Float4",
            "ComponentMaskCount != 1",
            'RoughnessOutput->Desc != TEXT("V5C.ContextMassingRoughnessA")',
            "EditorOnly->Roughness.Expression != RoughnessOutput",
            "MaterialDomain != MD_Surface",
            "BlendMode != BLEND_Opaque",
            "HasOnlyShadingModel(MSM_DefaultLit)",
            "Material->TwoSided",
            "Material->bEnableTessellation",
            "MaxWorldPositionOffsetDisplacement",
            "EditorOnly->EmissiveColor.Expression",
            "EditorOnly->Opacity.Expression",
            "EditorOnly->WorldPositionOffset.Expression",
            "EditorOnly->PixelDepthOffset.Expression",
            "V5C.UV0_SourceMetres",
            "outerAperture",
            "microReadability",
            "insetGlass",
            "frameMask",
            "cellHash",
            "cellOccupancy",
            "macroQ",
            "macroHash",
            "macroTone",
            "glassFresnel",
            "plinthMask",
            "responseRoughness",
            "finalRoughness",
        ):
            self.assertIn(marker, self.factory_cpp)
        self.assertNotIn("UMaterialExpressionTextureSample", self.factory_cpp)
        validate = function_body(
            self.factory_cpp, "bool ValidateMaterialInstance("
        )
        for marker in (
            "TextureParameterValues.IsEmpty()",
            "StaticSwitchParameters.IsEmpty()",
            "BaseOverrides.IsEmpty()",
            "SameNames(ExpectedScalarNames, ActualScalarNames)",
            "SameNames(ExpectedVectorNames, ActualVectorNames)",
            "ApertureHintStrength",
        ):
            self.assertIn(marker, validate)
        for parameter in (
            "SurfaceTintLow",
            "SurfaceTintHigh",
            "ApertureHintTint",
            "AtmosphereTint",
            "VariationCellMeters",
            "BayMeters",
            "StoreyMeters",
            "ApertureWidthFraction",
            "ApertureHeightFraction",
            "ApertureSillFraction",
            "ApertureHintStrength",
            "ApertureFadeStartCm",
            "ApertureFadeEndCm",
            "AtmosphereStartCm",
            "AtmosphereEndCm",
            "AtmosphereStrength",
            "SurfaceRoughness",
            "SurfaceSpecular",
        ):
            self.assertIn(parameter, self.factory_cpp)
        self.assertIn("terrainGroundedPolygonParts=1339", self.factory_cpp)
        self.assertIn("foundationSkirtMeters=1.0", self.factory_cpp)
        self.assertIn("localFacadeUvMetres=true", self.factory_cpp)
        self.assertIn("nonAuthoritativeFramedInsetGlazingCue=true", self.factory_cpp)
        self.assertIn("screenAdaptiveMicroFacadeFwidth=0.30..0.75", self.factory_cpp)
        self.assertIn("macroFacadeGroups=4x4", self.factory_cpp)
        self.assertIn("macroTone=0.90..1.06", self.factory_cpp)
        self.assertIn("proceduralGlassRoughness=0.28", self.factory_cpp)
        self.assertIn("deterministicCellVariation=true", self.factory_cpp)
        self.assertIn("plinthCue=true", self.factory_cpp)
        self.assertIn("apertureFadeCm=35000..70000", self.factory_cpp)
        self.assertIn("materialOnlyNoGeometryChange=true", self.factory_cpp)
        self.assertIn("noSurveyGradeFacadeOrRfAuthority=true", self.factory_cpp)

    def test_importer_permutation_is_normalized_by_exact_names_and_census(self) -> None:
        normalize = function_body(
            self.factory_cpp, "bool NormalizeAndBindMaterials("
        )
        for marker in (
            "CanonicalOrder.Find(Imported.MaterialSlotName)",
            "Imported.MaterialSlotName != Imported.ImportedMaterialSlotName",
            "ImportedToCanonical[ImportedIndex]",
            "RenderSection.NumTriangles !=",
            "GetSectionInfoMap().Set",
            "GetOriginalSectionInfoMap().Set",
            "SetStaticMaterials(OrderedMaterials)",
        ):
            self.assertIn(marker, normalize)
        for marker in (
            "FallbackTarget =\n        ENaniteFallbackTarget::PercentTriangles",
            "FallbackPercentTriangles = 1.0f",
            "FallbackRelativeError = 0.0f",
            "FinishCompilation({Mesh})",
        ):
            self.assertIn(marker, self.factory_cpp)
        self.assertNotIn("ExpectedImportedToCanonical", self.factory_cpp)

    def test_runtime_switch_restores_legacy_before_validation_or_hide(self) -> None:
        configure = function_body(
            self.runtime_cpp,
            "ConfigureV5CSurroundingsPresentation(\n        UStaticMesh* InSurroundingsV5C,\n        UStaticMesh* InGroundContextV5C",
        )
        restore_at = configure.index(
            "RestoreLegacyOsmSurroundingsPresentationFailSafe();"
        )
        validate_buildings_at = configure.index(
            "ValidateExactV5CSurroundingsMesh"
        )
        validate_ground_at = configure.index(
            "ValidateExactV5CGroundContextMesh"
        )
        hide_at = configure.index(
            "OSMContextBuildingsComponent->SetVisibility(false"
        )
        self.assertLess(restore_at, validate_buildings_at)
        self.assertLess(restore_at, validate_ground_at)
        self.assertLess(validate_buildings_at, hide_at)
        self.assertLess(validate_ground_at, hide_at)
        self.assertGreaterEqual(
            configure.count(
                "RestoreLegacyOsmSurroundingsPresentationFailSafe();"
            ),
            2,
        )
        self.assertIn(
            "OSMContextBuildingsComponent->bAutoActivate = false", configure
        )
        self.assertIn(
            "OSMContextBuildingsComponent->SetActive(false)", configure
        )
        self.assertIn(
            "V5CSurroundingsRenderOnlyComponent->bAutoActivate = true",
            configure,
        )
        self.assertIn(
            "V5CGroundContextRenderOnlyComponent->bAutoActivate = true",
            configure,
        )
        self.assertIn(
            "Mutate neither V5C sibling until both independent candidates pass",
            configure,
        )
        restore = function_body(
            self.runtime_cpp,
            "RestoreLegacyOsmSurroundingsPresentationFailSafe()",
        )
        self.assertIn(
            "OSMContextBuildingsComponent->bAutoActivate = true", restore
        )
        self.assertIn(
            "V5CSurroundingsRenderOnlyComponent->bAutoActivate = false",
            restore,
        )
        self.assertIn(
            "V5CGroundContextRenderOnlyComponent->bAutoActivate = false",
            restore,
        )
        validate = function_body(
            self.runtime_cpp,
            "ValidateV5CSurroundingsPresentation(FString& OutReport) const",
        )
        for marker in (
            "if (!bV5CSurroundingsPresentationActive)",
            "failSafeLegacyOsmVisible=true",
            "Active V5C surroundings state requires exact visible building and ground-context siblings",
            "ValidateExactV5CGroundContextMesh",
            "CanEverAffectNavigation()",
            "bHiddenInSceneCapture",
            "bCastContactShadow",
            "HumanOnlyOverlayComponentTag",
            "IgnoresEveryCollisionChannel(Component)",
            "bV5CSurroundingsTerrainGradeOrFoundationClaimed",
            "physical terrain-grade/foundation/facade",
        ):
            self.assertIn(marker, validate)

    def test_runtime_ground_context_is_exact_atomic_and_non_authoritative(self) -> None:
        combined = self.runtime_h + self.runtime_cpp
        for marker in (
            "V5CGroundContextRenderOnlyComponent",
            "V5COfficialPlanningGroundContextRenderOnlySuccessor",
            "/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/",
            "SM_IPV5C_OfficialPlanningGroundContext",
            "ExpectedV5CGroundContextImportedTriangleCount = 30403",
            "ExpectedV5CGroundContextMaterialCount = 2",
            "23336, 7067",
            "MI_IPV5C_OfficialPlanningRoadZone",
            "MI_IPV5C_OfficialPlanningRoadGraphic",
            "bV5CGroundContextRenderOnly = true",
            "bV5CGroundContextRoadWidthOrMaterialClaimed = false",
            "bV5CGroundContextElevationOrSurveyClaimed = false",
            "bV5CGroundContextCollisionNavigationSensorOrRfAuthority = false",
        ):
            self.assertIn(marker, combined)

        ground_mesh = function_body(
            self.runtime_cpp, "bool ValidateExactV5CGroundContextMesh("
        )
        for marker in (
            "ExpectedV5CGroundContextImportedTriangleCount",
            "ExpectedV5CGroundContextMaterialCount",
            "HasValidNaniteData",
            "AggGeom.GetElementCount() != 0",
            "CTF_UseComplexAsSimple",
            "ExpectedV5CGroundContextMaterialObjectPaths",
            "ExpectedV5CGroundContextImportedTrianglesByMaterial",
            "FVector(-99838.5847, -99957.6768, -128.0038645876)",
            "FVector(99892.9611, 99966.3390, 191.8825290834)",
            "StaticMaterial.MaterialSlotName",
            "StaticMaterial.ImportedMaterialSlotName",
        ):
            self.assertIn(marker, ground_mesh)
        self.assertIn("30,403 imported-triangle", ground_mesh)
        self.assertNotIn("30,408-triangle", ground_mesh)

    def test_editor_apply_is_current_v5b_only_unsaved_and_recoverable(self) -> None:
        apply = function_body(
            self.editor_cpp,
            "ApplyIstanaExploreV5CSurroundingsToCurrentWorld(FString& OutMessage)",
        )
        for marker in (
            "AssetReport",
            "ValidateAtomicV5CAssetRoots",
            "LoadExactV5CGroundContextMesh",
            "ResolveCurrentPublicViewScene",
            "ConfigureV5CSurroundingsPresentation",
            "ValidateV5CSurroundingsPresentation",
            "ValidatePublicViewScene",
            "RestoreLegacyOsmSurroundingsPresentationFailSafe",
            "MarkPackageDirty",
        ):
            self.assertIn(marker, apply)
        restore_at = apply.index(
            "Actor->RestoreLegacyOsmSurroundingsPresentationFailSafe();"
        )
        validate_roots_at = apply.index("ValidateAtomicV5CAssetRoots")
        configure_at = apply.index("ConfigureV5CSurroundingsPresentation")
        self.assertLess(restore_at, validate_roots_at)
        self.assertLess(validate_roots_at, configure_at)
        self.assertIn(
            "APPLY_REFUSED_ASSETS_SAFE_LEGACY_OSM", apply
        )
        self.assertIn(
            "APPLY_REFUSED_LOAD_SAFE_LEGACY_OSM", apply
        )
        asset_roots = function_body(
            self.editor_cpp, "bool ValidateAtomicV5CAssetRoots("
        )
        for marker in (
            "TRIADIstanaExploreV5CSurroundingsAssetFactory::ValidateAssets",
            "TRIADIstanaExploreV5CGroundContextAssetFactory::ValidateAssets",
            "exact three-asset ground-context root",
        ):
            self.assertIn(marker, asset_roots.replace("\n", ""))
        self.assertIn(
            "Actor->V5CGroundContextRenderOnlyComponent->Modify()",
            self.editor_cpp,
        )
        for marker in (
            "road-width/material",
            "elevation/Z",
            "survey",
            "sensor or RF authority",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertIn("/Game/Maps/Istana_PublicView_Explore_v5b", self.editor_cpp)
        self.assertNotIn("SaveMap", self.editor_cpp)
        self.assertNotIn("SaveCurrentLevel", self.editor_cpp)
        self.assertIn(
            "RestoreIstanaPublicViewOsmContextInCurrentWorld", self.editor_h
        )


if __name__ == "__main__":
    unittest.main()
