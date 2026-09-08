import hashlib
import json
from pathlib import Path
import re
import unittest


TEST_FILE = Path(__file__).resolve()
PLUGIN_ROOT = TEST_FILE.parents[1]
UNREAL_ROOT = TEST_FILE.parents[3]
REPO_ROOT = TEST_FILE.parents[4]
RUNTIME_PUBLIC = PLUGIN_ROOT / "Source" / "TRIADSensorFusion" / "Public"
RUNTIME_PRIVATE = PLUGIN_ROOT / "Source" / "TRIADSensorFusion" / "Private"
EDITOR_PUBLIC = PLUGIN_ROOT / "Source" / "TRIADSensorFusionEditor" / "Public"
EDITOR_PRIVATE = PLUGIN_ROOT / "Source" / "TRIADSensorFusionEditor" / "Private"
SCENE_HEADER = (RUNTIME_PUBLIC / "TRIADIstanaPublicViewSceneActor.h").read_text(encoding="utf-8")
SCENE_CPP = (RUNTIME_PRIVATE / "TRIADIstanaPublicViewSceneActor.cpp").read_text(encoding="utf-8")
POLICY_HEADER = (RUNTIME_PUBLIC / "TRIADIstanaPublicViewRuntimePolicyActor.h").read_text(encoding="utf-8")
POLICY_CPP = (RUNTIME_PRIVATE / "TRIADIstanaPublicViewRuntimePolicyActor.cpp").read_text(encoding="utf-8")
EDITOR_HEADER = (EDITOR_PUBLIC / "TRIADIstanaPublicViewEditorLibrary.h").read_text(encoding="utf-8")
EDITOR_CPP = (EDITOR_PRIVATE / "TRIADIstanaPublicViewEditorLibrary.cpp").read_text(encoding="utf-8")
IMPORT_SCRIPT = (REPO_ROOT / "scripts" / "Import-IstanaPublicViewAssets.ps1").read_text(encoding="utf-8")
BUILD_SCRIPT = (REPO_ROOT / "scripts" / "Build-IstanaPublicViewExteriorMap.ps1").read_text(encoding="utf-8")
INSTALL_SCRIPT = (REPO_ROOT / "scripts" / "Install-IstanaPublicViewDevelopmentAssets.ps1").read_text(encoding="utf-8")
SET_PIE_SCRIPT = (REPO_ROOT / "scripts" / "Set-IstanaPublicViewPlayInEditor.ps1").read_text(encoding="utf-8")
CAPTURE_PIE_SCRIPT = (REPO_ROOT / "scripts" / "Capture-IstanaPublicViewPlayPreviews.ps1").read_text(encoding="utf-8")
SETTINGS = json.loads((UNREAL_ROOT / "Config" / "IstanaPublicView.settings.json").read_text(encoding="utf-8"))
SOURCE_ROOT = UNREAL_ROOT / "SourceAssets" / "IstanaPublicView"
GENERATED_ROOT = SOURCE_ROOT / "Generated"
TEXTURE_ROOT = SOURCE_ROOT / "Textures" / "Generated"
OSM_CONTEXT_ROOT = SOURCE_ROOT / "GeneratedOptional" / "OSMContext"


CLAIM = "PUBLIC_REFERENCE_VISUAL_APPROXIMATION_NOT_SURVEY_CONTROLLED"
DESTINATION = "/Game/Maps/Istana_PublicView_Exterior_v1"
SOURCE_FILES = (
    "SM_IstanaPublicView_Building_Hero.obj",
    "SM_IstanaPublicView_Building_Collision.obj",
    "SM_IstanaPublicView_Terrain.obj",
    "SM_IstanaPublicView_TerrainSkirt.obj",
    "SM_IstanaPublicView_Hardscape.obj",
    "SM_IstanaPublicView_ContextBuildings.obj",
    "SM_IstanaPublicView_Tree_Rain.obj",
    "SM_IstanaPublicView_Tree_Palm.obj",
    "SM_IstanaPublicView_Tree_Framing.obj",
    "IstanaPublicView.instances.json",
    "IstanaPublicViewBuilding.manifest.json",
    "IstanaPublicViewContext.manifest.json",
)
MATERIALS = (
    "M_IPV_Render", "M_IPV_Trim", "M_IPV_Slate", "M_IPV_Shutter",
    "M_IPV_Glass", "M_IPV_Stone", "M_IPV_Metal", "M_IPV_Door",
    "M_IPV_DarkTimber", "M_IPV_Opaline", "M_IPV_Collision",
    "M_IPV_Lawn", "M_IPV_Water", "M_IPV_Planting",
    "M_IPV_ContextRender", "M_IPV_ContextGlass", "M_IPV_ContextRoof",
    "M_IPV_Bark", "M_IPV_LeafDark", "M_IPV_LeafMid", "M_IPV_LeafLight",
    "M_IPV_TerrainSkirt",
)
TEXTURE_SETS = (
    "Bark", "DarkTimber", "Foliage", "Lawn",
    "Plaster", "Shutter", "Slate", "Stone",
)
TEXTURES = tuple(
    f"T_IPV_{texture_set}_{suffix}"
    for texture_set in TEXTURE_SETS
    for suffix in ("BaseColor", "Normal", "ORM")
)
WEATHER_SCALARS = (
    "RAIN", "ROADWETNESS", "FOG", "DUST", "SNOW", "ROADSNOW",
    "MAPLELEAF", "ROADLEAF",
)


class IstanaPublicViewContractTests(unittest.TestCase):
    def test_isolated_types_and_entrypoints_exist(self) -> None:
        self.assertIn("ATRIADIstanaPublicViewSceneActor", SCENE_HEADER)
        self.assertIn("ATRIADIstanaPublicViewRuntimePolicyActor", POLICY_CPP)
        for function in (
            "ValidateIstanaPublicViewRemoteControlProject",
            "ImportIstanaPublicViewAssets",
            "ValidateIstanaPublicViewAssets",
            "BuildIstanaPublicViewExteriorMap",
            "ValidateIstanaPublicViewExteriorMap",
            "ValidateIstanaPublicViewPlayWorldReadiness",
            "CaptureIstanaPublicViewPlayCamera",
            "QuiesceIstanaPublicViewPlayWorldForStop",
        ):
            self.assertIn(function, EDITOR_HEADER)
            self.assertIn(f"::{function}", EDITOR_CPP)

    def test_source_contract_is_sha_bound_and_required_before_map_creation(self) -> None:
        for name in SOURCE_FILES:
            self.assertIn(name, EDITOR_CPP)
        for marker in (
            "FPublicViewSha256",
            "contractSha256",
            "surfaceContract",
            "UV0_CONTINUOUS_SOURCE_METRES",
            "ImportNormals",
            "MikkTSpace",
            "SM_IstanaPublicView_TerrainSkirt.obj",
            "TERRAIN_BOUNDARY_SKIRT_VISUAL_ONLY",
            "material_slot_mapping.json",
            "triad.istana_public_view_pbr_manifest.v1",
            "2b59b328a4e5f4963d4fb3b006eba7d12f42316b981e88b8aeebd0718643a87a",
            "triad.istana_public_view_building_manifest.v1",
            "triad.istana_public_view_context_manifest.v1",
            "triad.istana_public_view_instances.v1",
            "position metres multiplied by 100; yaw about +Z; uniform scale",
            "containsRouteGraph",
        ):
            self.assertIn(marker, EDITOR_CPP)
        self.assertNotIn("FPlatformMisc::GetSHA256Signature", EDITOR_CPP)

        build_start = EDITOR_CPP.index("::BuildIstanaPublicViewExteriorMap")
        build_body = EDITOR_CPP[build_start:]
        order = (
            build_body.index("DoesPackageExist(DestinationMapPackage)"),
            build_body.index("ValidatePublicViewSourceSet(&Placements"),
            build_body.index("InspectPublicViewAssetSet("),
            build_body.index("NewBlankMap(false)"),
            build_body.index("SaveMap(World, DestinationMapPackage)"),
        )
        self.assertEqual(order, tuple(sorted(order)))

    def test_editor_exactly_binds_current_frozen_source_and_texture_bytes(self) -> None:
        def assert_bound(path: Path) -> None:
            payload = path.read_bytes()
            self.assertIn(str(len(payload)), EDITOR_CPP, path.name)
            self.assertIn(hashlib.sha256(payload).hexdigest(), EDITOR_CPP, path.name)

        contract = SOURCE_ROOT / "istana_public_view.contract.json"
        building_manifest = GENERATED_ROOT / "IstanaPublicViewBuilding.manifest.json"
        context_manifest = GENERATED_ROOT / "IstanaPublicViewContext.manifest.json"
        for path in (contract, building_manifest, context_manifest):
            assert_bound(path)
        for manifest_path in (building_manifest, context_manifest):
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            for record in manifest["files"]:
                path = GENERATED_ROOT / record["path"]
                self.assertEqual(path.stat().st_size, record["bytes"])
                self.assertEqual(
                    hashlib.sha256(path.read_bytes()).hexdigest(),
                    record["sha256"],
                )
                assert_bound(path)

        osm_contract = SOURCE_ROOT / "istana_public_view_osm_context.contract.json"
        osm_manifest = OSM_CONTEXT_ROOT / "IstanaPublicViewOSMContext.manifest.json"
        osm_mesh = OSM_CONTEXT_ROOT / "SM_IstanaPublicView_OSMContextBuildings.obj"
        for path in (osm_contract, osm_manifest, osm_mesh):
            assert_bound(path)
        osm_record = json.loads(osm_manifest.read_text(encoding="utf-8"))["files"][0]
        self.assertEqual(osm_mesh.stat().st_size, osm_record["bytes"])
        self.assertEqual(hashlib.sha256(osm_mesh.read_bytes()).hexdigest(), osm_record["sha256"])

        texture_manifest_path = TEXTURE_ROOT / "manifest.json"
        assert_bound(texture_manifest_path)
        texture_manifest = json.loads(texture_manifest_path.read_text(encoding="utf-8"))
        texture_records = [
            output
            for record in texture_manifest["records"]
            for output in record["outputs"].values()
        ]
        self.assertEqual(len(texture_records), 24)
        for record in texture_records:
            path = TEXTURE_ROOT / record["file"]
            self.assertEqual(path.stat().st_size, record["bytes"])
            self.assertEqual(
                hashlib.sha256(path.read_bytes()).hexdigest(),
                record["sha256"],
            )
            assert_bound(path)

    def test_destination_and_asset_namespace_are_exact_and_non_overwriting(self) -> None:
        self.assertIn(DESTINATION, EDITOR_CPP)
        self.assertIn("/Game/TRIAD/IstanaPublicView", EDITOR_CPP)
        for forbidden in (
            "NewMapFromTemplate",
            "DeleteAsset",
            "DestroyActor",
            "bReplaceExisting = true",
            "bReplaceExistingSettings = true",
            "TRIADIstanaEditorLibrary",
            "/Game/SDTH",
            "/Game/Maps/Istana_1km",
            "/Game/TRIAD/Istana/",
            "/Game/TRIAD/IstanaDigitalTwin",
        ):
            self.assertNotIn(forbidden, EDITOR_CPP)
        save_calls = re.findall(r"SaveMap\s*\(([^)]*)\)", EDITOR_CPP)
        self.assertEqual(save_calls, ["World, DestinationMapPackage"])

    def test_odbl_context_is_exact_visible_visual_only_and_never_sensor_truth(self) -> None:
        osm = SETTINGS["osmOuterContext"]
        self.assertTrue(osm["integratedByIsolatedUnrealImport"])
        self.assertEqual(
            osm["asset"],
            "/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_OSMContextBuildings",
        )
        self.assertEqual(
            osm["sceneStatus"],
            "COORDINATE_CONTRACT_ALIGNED_MAPPING_GRADE_NOT_VISUALLY_OR_PHOTO_ACCEPTED",
        )
        self.assertEqual(osm["attribution"], "© OpenStreetMap contributors")
        self.assertEqual(osm["databaseLicence"], "ODbL-1.0")
        self.assertFalse(osm["sensorTruth"])
        self.assertFalse(osm["navigationTruth"])
        self.assertFalse(osm["containsRoadsOrRoutes"])
        self.assertFalse(osm["legacySyntheticFallback"]["visible"])
        self.assertFalse(osm["legacySyntheticFallback"]["active"])
        for marker in (
            "ValidateOsmContextSourceContract",
            "GeneratedOptional/OSMContext",
            "SM_IstanaPublicView_OSMContextBuildings",
            "4026a4a99170eade125156b7b52e5b92d3e61215dbf0df38011a5432f93f1d77",
            "11784053",
            "COORDINATE_CONTRACT_ALIGNED_NOT_VISUALLY_ACCEPTED",
            "LICENSED_ODBL_SNAPSHOT_DERIVED",
            "M_IPV_ContextRender",
            "M_IPV_ContextRoof",
            "BuildSettings.BuildScale3D = FVector(1.0, 1.0, 1.0)",
            "bUseFullPrecisionUVs = true",
            "FBXNIM_ImportNormals",
            "MikkTSpace",
        ):
            self.assertIn(marker, EDITOR_CPP)
        for marker in (
            "OSMContextBuildingsComponent",
            "ContextBuildingsComponent->SetVisibility(false, true)",
            "ContextBuildingsComponent->SetHiddenInGame(true, true)",
            "ContextBuildingsComponent->SetActive(false)",
            "OSMContextBuildingsComponent->SetVisibility(true, true)",
            "OSMContextBuildingsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "OSMContextBuildingsComponent->SetCollisionResponseToAllChannels(ECR_Ignore)",
            "OSMContextBuildingsComponent->SetGenerateOverlapEvents(false)",
            "OSMContextBuildingsComponent->SetAutoActivate(true)",
            "!OSMContextBuildingsComponent->bAutoActivate",
            "bOSMContextUsedForSensorTruth = false",
        ):
            self.assertIn(marker, SCENE_HEADER + SCENE_CPP)

    def test_full_volume_scene_and_hism_thresholds_are_hard_gates(self) -> None:
        self.assertIn("UHierarchicalInstancedStaticMeshComponent", SCENE_HEADER)
        self.assertNotIn("UInstancedStaticMeshComponent", SCENE_HEADER.replace("UHierarchicalInstancedStaticMeshComponent", ""))
        for marker in (
            "MinimumRequiredTreeInstances = 600",
            "MinimumRequiredHeroTreeInstances = 120",
            "HeroTreeRadiusMeters = 250.0f",
            "ContextRadiusMeters = 1000.0f",
            "bContainsWholeTreeBillboards = false",
            "TerrainSize.Z < 100.0",
            "TerrainSize.X < 195000.0",
            "TerrainSkirtComponent",
            "OSMContextBuildingsComponent",
            "SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION",
            "GetInstanceCount",
        ):
            self.assertIn(marker, SCENE_HEADER + SCENE_CPP)
        self.assertIn("RequiredTreeInstanceCount = 600", EDITOR_CPP)
        self.assertIn("RequiredHeroTreeInstanceCount = 120", EDITOR_CPP)
        self.assertIn("ContextBuildings->Num() < 160", EDITOR_CPP)
        self.assertIn("UMaterialBillboardComponent", EDITOR_CPP)
        self.assertIn(
            "SYNTHETIC_CONTEXT_AND_VEGETATION_VISUAL_ONLY_NO_COLLISION",
            SCENE_HEADER + SCENE_CPP + EDITOR_CPP,
        )
        self.assertNotIn("ECollisionEnabled::QueryOnly", SCENE_CPP)
        self.assertGreaterEqual(
            SCENE_CPP.count("SetCollisionEnabled(ECollisionEnabled::NoCollision)"),
            6,
        )
        self.assertGreaterEqual(
            SCENE_CPP.count("GetCollisionEnabled() != ECollisionEnabled::NoCollision"),
            2,
        )

    def test_every_slot_uses_exact_project_owned_pbr_or_purpose_built_material(self) -> None:
        for material in MATERIALS:
            self.assertIn(material, EDITOR_CPP)
        for texture in TEXTURES:
            self.assertIn(texture, EDITOR_CPP)
        for marker in (
            "bImportMaterials = false",
            "bImportTextures = false",
            "UTextureFactory",
            "ETextureSourceColorSpace::SRGB",
            "ETextureSourceColorSpace::Linear",
            "TC_Normalmap",
            "TC_Masks",
            "BaseColorTexture",
            "NormalTexture",
            "PackedORMTexture",
            "UMaterialExpressionTextureCoordinate",
            "UMaterialExpressionMultiply",
            "MP_AmbientOcclusion",
            "1.0f / Spec.UvMetersPerTile",
            "Mesh->SetMaterial(MaterialIndex, Material)",
            "Material->GetPathName() != ExpectedMaterial",
            "CreatePublicViewMaterial",
            "GetMaterialPropertyInputNode",
            "PrepareLegacyObjMaterialSlotAdapter",
            "LegacyObjMaterialSlots",
            "newmtl %s",
            "ImportTask->Filename = AdaptedObjFilename",
            "AssetImportData->Update(OriginalSourceFilename)",
        ):
            self.assertIn(marker, EDITOR_CPP)
        for forbidden in ("WorldGridMaterial", "DefaultMaterial", "/Engine/EngineMaterials"):
            self.assertNotIn(forbidden, EDITOR_CPP)
        self.assertIn("GetTextureSpecs().Num()", EDITOR_CPP)
        self.assertIn("Texture->Source.GetSizeX() != 2048", EDITOR_CPP)
        self.assertIn("Texture->bFlipGreenChannel", EDITOR_CPP)

    def test_only_live_nanite_public_view_materials_are_flagged_and_validated(self) -> None:
        selector = EDITOR_CPP[
            EDITOR_CPP.index("bool RequiresNaniteMaterialUsage(") :
            EDITOR_CPP.index("const TArray<FPublicViewTextureSpec>& GetTextureSpecs()")
        ]
        self.assertEqual(
            {
                "M_IPV_Bark",
                "M_IPV_LeafDark",
                "M_IPV_LeafMid",
                "M_IPV_LeafLight",
                "M_IPV_Lawn",
            },
            set(re.findall(r'TEXT\("(M_IPV_[^"]+)"\)', selector)),
        )
        creator = EDITOR_CPP[
            EDITOR_CPP.index("UMaterial* CreatePbrPublicViewMaterial(") :
            EDITOR_CPP.index("UMaterial* CreateConstantPublicViewMaterial(")
        ]
        validator = EDITOR_CPP[
            EDITOR_CPP.index("bool ValidatePublicViewMaterial(") :
            EDITOR_CPP.index("UMaterial* CreatePbrPublicViewMaterial(")
        ]
        constant_creator = EDITOR_CPP[
            EDITOR_CPP.index("UMaterial* CreateConstantPublicViewMaterial(") :
            EDITOR_CPP.index("UMaterial* CreatePublicViewMaterial(")
        ]
        self.assertIn("if (RequiresNaniteMaterialUsage(Spec))", creator)
        self.assertIn("Material->bUsedWithNanite = true;", creator)
        self.assertIn(
            "!Material->GetUsageByFlag(MATUSAGE_Nanite)",
            validator,
        )
        self.assertNotIn("bUsedWithNanite", constant_creator)

    def test_clear_play_profile_sets_and_reads_every_weather_scalar(self) -> None:
        for scalar in WEATHER_SCALARS:
            enum_name = f"WEATHER_PARAM_SCALAR_{scalar}"
            self.assertGreaterEqual(POLICY_CPP.count(enum_name), 2, enum_name)
        for marker in (
            "setWeatherEnabled(World, false)",
            "getIsWeatherEnabled(World)",
            "SetFogDensity(0.0f)",
            "SetSecondFogDensity(0.0f)",
            "SetFogMaxOpacity(0.0f)",
            "SetVolumetricFog(false)",
            "bOverride_AutoExposureApplyPhysicalCameraExposure = true",
            "AutoExposureApplyPhysicalCameraExposure = true",
            "bOverride_AutoExposureBias = true",
            "AutoExposureBias = 0.0f",
            "bOverride_LocalExposureHighlightContrastScale = true",
            "LocalExposureHighlightContrastScale = 1.0f",
            "bOverride_LocalExposureShadowContrastScale = true",
            "LocalExposureShadowContrastScale = 1.0f",
            "bOverride_LocalExposureDetailStrength = true",
            "LocalExposureDetailStrength = 1.0f",
            "bOverride_LocalExposureMiddleGreyBias = true",
            "LocalExposureMiddleGreyBias = 0.0f",
            "bOverride_LocalExposureHighlightContrastCurve = true",
            "LocalExposureHighlightContrastCurve = nullptr",
            "bOverride_LocalExposureShadowContrastCurve = true",
            "LocalExposureShadowContrastCurve = nullptr",
            "SetConstraintAspectRatio(true)",
            "bOverride_DepthOfFieldScale = true",
            "DepthOfFieldScale = 0.0f",
            "bRuntimePolicySettledAtRuntime",
            "const bool bFinalAttempt",
            "if (!bFinalAttempt)",
        ):
            self.assertIn(marker, POLICY_CPP)
        self.assertNotIn(
            "if ((!bFogSuppressionVerifiedAtRuntime || !bPrimaryCameraVerifiedAtRuntime)",
            POLICY_CPP,
        )

    def test_fixed_cameras_and_claim_are_consistent_with_config(self) -> None:
        self.assertEqual(SETTINGS["schema"], "triad.istana_public_view_unreal_settings.v1")
        self.assertEqual(SETTINGS["destinationMap"], DESTINATION)
        self.assertEqual(SETTINGS["assetRoot"], "/Game/TRIAD/IstanaPublicView")
        self.assertEqual(SETTINGS["claimStatus"], CLAIM)
        self.assertFalse(SETTINGS["surveyControlled"])
        self.assertFalse(SETTINGS["arbitraryViewIndistinguishabilityClaimed"])
        self.assertFalse(SETTINGS["photorealMaterialAcceptance"])
        self.assertTrue(SETTINGS["pbrTexturePackIntegrationIncluded"])
        self.assertEqual(
            SETTINGS["gameModeClass"],
            "/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode",
        )
        self.assertEqual(
            SETTINGS["materialImplementationTier"],
            "ORIGINAL_AI_ASSISTED_PBR_TEXTURES_INTEGRATED_PHOTO_QA_NOT_ACCEPTED",
        )
        self.assertFalse(SETTINGS["wholeTreeBillboardsAllowed"])
        self.assertEqual(SETTINGS["treeInstanceMinimum"], 600)
        self.assertEqual(SETTINGS["heroTreeInstanceMinimum"], 120)
        self.assertEqual(SETTINGS["heroTreeRadiusMeters"], 250)
        self.assertEqual(SETTINGS["contextRadiusMeters"], 1000)
        self.assertEqual(
            SETTINGS["runtimeStabilization"],
            {
                "attempts": 8,
                "retrySeconds": 0.75,
                "readinessRequiresFinalSettledPass": True,
                "reassertsGameModeWeatherFogAndCameraEveryPass": True,
            },
        )
        self.assertEqual(
            SETTINGS["sensorOcclusionPolicy"],
            {
                "marker": "SYNTHETIC_CONTEXT_AND_VEGETATION_VISUAL_ONLY_NO_COLLISION",
                "anonymousContextBuildingsCollision": "NoCollision",
                "osmContextBuildingsCollision": "NoCollision",
                "osmContextUsedForSensorTruth": False,
                "syntheticContextFallbackVisible": False,
                "vegetationHismCollision": "NoCollision",
                "visibilityResponse": "Ignore",
                "overlapEvents": False,
            },
        )
        self.assertEqual(len(SETTINGS["fixedCameras"]), 4)
        self.assertEqual(SETTINGS["cameraProfile"]["depthOfFieldScale"], 0)
        self.assertTrue(SETTINGS["cameraProfile"]["applyPhysicalCameraExposure"])
        self.assertEqual(SETTINGS["cameraProfile"]["exposureBias"], 0)
        self.assertGreaterEqual(
            EDITOR_CPP.count("AutoExposureApplyPhysicalCameraExposure"), 4
        )
        for game_mode_marker in (
            "SetPersistentPublicViewAirSimGameMode",
            "/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode",
            "DefaultGameMode",
            "bGameModeOverrideVerifiedAtRuntime",
            "GetAuthGameMode()",
        ):
            self.assertIn(game_mode_marker, EDITOR_CPP + POLICY_CPP)
        self.assertEqual(
            SETTINGS["sourceCoordinateImportPolicy"],
            {
                "objUnits": "centimetres",
                "convertScene": False,
                "convertSceneUnit": False,
                "forceFrontXAxis": False,
                "ceremonialApproachAxis": "+Y",
                "legacyObjExportContract": "UE55_LEGACY_OBJ_Y_MIRROR_WINDING_NORMAL_AND_UV_V_PRECONDITION",
                "encodedObjPosition": "logical (x,-y,z)",
                "encodedObjNormal": "logical (nx,-ny,nz)",
                "encodedObjFaceTokensReversed": True,
                "encodedObjUv0": "logical (u,1-v)",
                "expectedLegacyImporterReadback": "logical positions, explicit normals, front winding, and continuous-metre UV0",
                "buildScale3D": [1, 1, 1],
                "negativeBuildScaleForbidden": True,
                "exactManifestBoundsRequired": True,
                "uvSet": "UV0",
                "uvEncoding": "continuous source-metre projections; one UV unit equals one metre before material scaling",
                "normalImportMethod": "ImportNormals",
                "recomputeNormals": False,
                "recomputeTangents": True,
                "tangentSpace": "MikkTSpace",
                "removeDegenerates": True,
                "fullPrecisionUvs": True,
            },
        )
        for import_marker in (
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "bForceFrontXAxis = false",
            "UE55_LEGACY_OBJ_Y_MIRROR_WINDING_NORMAL_AND_UV_V_PRECONDITION",
            "ValidateLegacyObjImportContract",
            "reverse each complete v/vt/vn token sequence",
            "disk (u,v) = logical (u,1-v)",
            "negativeBuildScaleForbidden",
            "GetExpectedImportedBounds",
            "BoundsToleranceCentimeters = 2.0",
            "FVector(0.0, 9321.5, 715.219473)",
            "FVector(4800.0, 1528.5, 700.0)",
            "bRecomputeNormals = false",
            "bRecomputeTangents = true",
            "bUseMikkTSpace = true",
            "bUseFullPrecisionUVs = true",
            "BuildSettings.BuildScale3D = FVector(1.0, 1.0, 1.0)",
            "BuildSettings.BuildScale3D.Equals(FVector::OneVector",
            "GetNumTexCoords(0)",
        ):
            self.assertIn(import_marker, EDITOR_CPP)
        for camera in SETTINGS["fixedCameras"]:
            self.assertIn(camera["tag"], EDITOR_CPP)
            for value in camera["locationCentimeters"] + camera["lookAtCentimeters"]:
                self.assertTrue(isinstance(value, (int, float)))
        self.assertIn(CLAIM, SCENE_CPP)
        self.assertIn(CLAIM, POLICY_CPP)
        self.assertIn(CLAIM, EDITOR_CPP)
        self.assertIn("MATERIAL_ACCEPTANCE_BLOCKED", EDITOR_CPP)
        self.assertEqual(SETTINGS["pbrTextureContract"]["textureAssetCount"], 24)
        self.assertFalse(SETTINGS["pbrTextureContract"]["measuredMaterialScan"])
        self.assertEqual(SETTINGS["photoQaEvidence"]["status"], "NOT_READY")
        self.assertFalse(SETTINGS["photoQaEvidence"]["consumedByMapImport"])
        self.assertFalse(SETTINGS["displayColorPolicy"]["externalOcioDisplayValidated"])
        self.assertFalse(SETTINGS["displayColorPolicy"]["editorViewportOcioProgrammaticallyControlled"])
        self.assertEqual(
            SETTINGS["terrainBoundaryPolicy"]["marker"],
            "SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION",
        )

    def test_stale_constant_tier_and_eight_mesh_success_claims_are_rejected(self) -> None:
        combined = SCENE_HEADER + SCENE_CPP + EDITOR_CPP
        for stale in (
            "STRUCTURAL_CONSTANT_MATERIAL_PREVIEW_ONLY_NOT_PHOTOREAL_ACCEPTANCE",
            "eight exact meshes",
            "eight required source roles",
            "constant materials are structural placeholders",
            "PBR texture pack is not integrated",
            "separate PBR texture pack is not integrated",
        ):
            self.assertNotIn(stale, combined)
        self.assertIn("ten exact meshes", EDITOR_CPP)
        self.assertIn("ORIGINAL_AI_ASSISTED_PBR_TEXTURES_INTEGRATED_PHOTO_QA_NOT_ACCEPTED", combined)
        self.assertIn("NOT_READY_UNCALIBRATED_COMMONS_EVIDENCE_METADATA_ONLY", combined)

    def test_wrappers_run_source_validator_and_protect_existing_content(self) -> None:
        for script in (IMPORT_SCRIPT, BUILD_SCRIPT):
            self.assertIn("validate_public_view_assets.py", script)
            self.assertIn("validate_public_view_osm_context.py", script)
            self.assertIn("ValidateIstanaPublicViewRemoteControlProject", script)
            self.assertIn("Get-FileHash", script)
            self.assertIn("Content\\SDTH.umap", script)
            self.assertIn("Content\\Maps\\Istana_1km.umap", script)
            self.assertIn("Content\\Maps\\Istana_1km_Context_v2.umap", script)
            self.assertIn("Content\\TRIAD\\IstanaDigitalTwin", script)
            for destructive in ("Remove-Item", "Move-Item", "Copy-Item", "Set-Content"):
                self.assertNotIn(destructive, script)
        self.assertIn("ImportIstanaPublicViewAssets", IMPORT_SCRIPT)
        self.assertIn("ValidateIstanaPublicViewAssets", IMPORT_SCRIPT)
        self.assertIn("SM_IstanaPublicView_TerrainSkirt.uasset", IMPORT_SCRIPT)
        self.assertIn("SM_IstanaPublicView_OSMContextBuildings.uasset", IMPORT_SCRIPT)
        for texture in TEXTURES:
            self.assertIn(texture, IMPORT_SCRIPT)
        self.assertIn("BuildIstanaPublicViewExteriorMap", BUILD_SCRIPT)
        self.assertIn(DESTINATION, BUILD_SCRIPT)

    def test_editor_closed_installer_is_non_overwriting_and_content_safe(self) -> None:
        for marker in (
            "Get-Process UnrealEditor",
            "Install-IstanaDevelopmentAssets.ps1",
            "SourceAssets\\IstanaPublicView",
            "IstanaPublicView.settings.json",
            "validate_public_view_assets.py",
            "validate_public_view_osm_context.py",
            "Assert-NonOverwritingFileCopy",
            "Copy-NewOrEqualHashFile",
            "Get-FileHash",
            "Content\\SDTH.umap",
            "Content\\Maps\\Istana_1km.umap",
            "Content\\Maps\\Istana_1km_Context_v2.umap",
            "Content\\TRIAD\\Istana",
            "Content\\TRIAD\\IstanaDigitalTwin",
            "Assert-SameSnapshot",
        ):
            self.assertIn(marker, INSTALL_SCRIPT)
        for destructive in ("Remove-Item", "Move-Item"):
            self.assertNotIn(destructive, INSTALL_SCRIPT)
        self.assertNotRegex(
            INSTALL_SCRIPT,
            r"Copy-Item[^\r\n]*-Force",
        )

    def test_public_view_pie_capture_is_exact_four_camera_and_restores_primary(self) -> None:
        capture_start = EDITOR_CPP.index("::CaptureIstanaPublicViewPlayCamera")
        capture_body = EDITOR_CPP[capture_start:]
        for marker in (
            "ValidateIstanaPublicViewPlayWorldReadiness",
            "CEREMONIAL_FRONT",
            "FRONT_OBLIQUE",
            "ARCADE",
            "TOWER",
            "TRIADIstanaPublicViewCamera_FrontOblique",
            "TRIADIstanaPublicViewCamera_Arcade",
            "TRIADIstanaPublicViewCamera_Tower",
            "UWorld::RemovePIEPrefix",
            "FPaths::GetCleanFilename",
            "ipv_play_",
            "TRIAD/IstanaPreviews/PublicView",
            "FileExists",
            "FScreenshotRequest::IsScreenshotRequested",
            "SetFixedViewportSize(1920, 1080)",
            "GetCameraCacheView",
            "GetCameraView",
            "SetViewTargetWithBlend(RequestedCamera, 0.0f)",
            "RequestScreenshot(DestinationPath, false, false, false)",
            "GameViewport->Draw(false)",
            "RestorePrimaryCamera",
            "SetViewTargetWithBlend(PrimaryCamera, 0.0f)",
        ):
            self.assertIn(marker, capture_body)
        self.assertIn("RequiredTag = PrimaryCameraTag", capture_body)
        self.assertIn("TRIADIstanaPublicViewCamera_Primary", EDITOR_CPP)
        for forbidden in (
            "GEngine->Exec",
            "HighResShot",
            "ConsoleCommand",
            "DeleteFile",
        ):
            self.assertNotIn(forbidden, capture_body)

        for marker in (
            "CaptureIstanaPublicViewPlayCamera",
            "ValidateIstanaPublicViewRemoteControlProject",
            "ValidateIstanaPublicViewPlayWorldReadiness",
            "CEREMONIAL_FRONT",
            "FRONT_OBLIQUE",
            "ARCADE",
            "TOWER",
            "Test-Path -LiteralPath $outputPath",
            "Get-PngDimensions",
            "1920",
            "1080",
            "Get-FileHash",
            "SHA256",
        ):
            self.assertIn(marker, CAPTURE_PIE_SCRIPT)
        for forbidden in ("Remove-Item", "Move-Item", "Set-Content"):
            self.assertNotIn(forbidden, CAPTURE_PIE_SCRIPT)

    def test_public_view_pie_readiness_separates_editor_structure_from_runtime_state(self) -> None:
        readiness_start = EDITOR_CPP.index(
            "::ValidateIstanaPublicViewPlayWorldReadiness"
        )
        readiness_end = EDITOR_CPP.index(
            "::CaptureIstanaPublicViewPlayCamera",
            readiness_start,
        )
        readiness_body = EDITOR_CPP[readiness_start:readiness_end]
        for marker in (
            "UWorld* EditorWorld",
            "ValidatePublicViewWorld(EditorWorld, true",
            "Editor-map structural validation passed separately",
            "PlayWorld->WorldType != EWorldType::PIE",
            "PlayWorld->IsGameWorld()",
            "PlayWorld->HasBegunPlay()",
            "UWorld::RemovePIEPrefix",
            "SourcePackage != DestinationMapPackage",
            "GetCameraSpecs()",
            "It->ActorHasTag(Spec.RequiredTag)",
            "MatchingCameraCount != 1",
            "UniqueTaggedCameras.Num() != GetCameraSpecs().Num()",
            "Policy->IsFogSuppressionActive(FogComponentCount)",
            "FogComponentCount != Policy->FogComponentCountAtRuntime",
            "AirSim-added untagged cameras are allowed",
            "No PlayWorld absolute-transform or total camera/fog actor-count equality",
        ):
            self.assertIn(marker, readiness_body)
        self.assertNotIn("ValidatePublicViewWorld(PlayWorld", readiness_body)
        self.assertNotIn(
            "CameraActorCount != GetCameraSpecs().Num()",
            readiness_body,
        )

    def test_public_view_pie_start_and_stop_are_guarded_and_quiescent(self) -> None:
        for marker in (
            "RequestAirSimQuiescenceForTeardown",
            "TActorIterator<ASimModeBase>",
            "SimMode->pause(true)",
            "SimMode->isPaused()",
            "World->WorldType != EWorldType::PIE",
        ):
            self.assertIn(marker, POLICY_HEADER + POLICY_CPP)

        quiesce_start = EDITOR_CPP.index("::QuiesceIstanaPublicViewPlayWorldForStop")
        quiesce_body = EDITOR_CPP[quiesce_start:]
        for marker in (
            "UWorld::RemovePIEPrefix",
            "DestinationMapPackage",
            "RuntimePolicyTag",
            "SceneActorTag",
            "IstanaAirSimGameModeClassPath",
            "RequestAirSimQuiescenceForTeardown",
        ):
            self.assertIn(marker, quiesce_body)

        for marker in (
            "ValidateIstanaPublicViewRemoteControlProject",
            "ValidateIstanaPublicViewExteriorMap",
            "ValidateIstanaPublicViewPlayWorldReadiness",
            "QuiesceIstanaPublicViewPlayWorldForStop",
            "EditorRequestBeginPlay",
            "EditorRequestEndPlay",
            "Start-Sleep -Seconds $QuiesceDrainSeconds",
            "$requiredStableReadinessPolls = 3",
        ):
            self.assertIn(marker, SET_PIE_SCRIPT)
        stop_body = SET_PIE_SCRIPT[SET_PIE_SCRIPT.index("if ($Stop)"):]
        self.assertLess(
            stop_body.index("-FunctionName 'QuiesceIstanaPublicViewPlayWorldForStop'"),
            stop_body.index("-FunctionName 'EditorRequestEndPlay'"),
        )
        for forbidden in (
            "ValidateIstanaRuntimeMapV2",
            "TRIADIstanaEditorLibrary",
            "Stop-Process",
            "taskkill",
        ):
            self.assertNotIn(forbidden, SET_PIE_SCRIPT)


if __name__ == "__main__":
    unittest.main()
