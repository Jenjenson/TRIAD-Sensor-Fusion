from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import shutil
import subprocess
import unittest


REPO = Path(__file__).resolve().parents[4]
PACKAGE = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Terrain"
    / "CopernicusDEM2021"
)
CONTRACT = PACKAGE / "copernicus_dem_2021.native_fallback.contract.json"
SOURCE_MANIFEST = (
    PACKAGE / "Generated" / "IstanaCopernicusDEM2021Terrain.manifest.json"
)
OBJ = PACKAGE / "Generated" / "SM_IPV5D_CopernicusDEM2021_TerrainProxy_Visual.obj"
RUNTIME_HEADER = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
)
RUNTIME_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp"
)
HISTORICAL_RUNTIME_REPOSITORY_RELATIVE = (
    r"SourceAssets\IstanaPublicViewExploreV5D\Terrain\CopernicusDEM2021"
    r"\NativeSourceClosure"
    r"\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp"
)
HISTORICAL_RUNTIME_NATIVE_RELATIVE = (
    r"Plugins\TRIADSensorFusion\Source\TRIADSensorFusion\Private"
    r"\TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp"
)
HISTORICAL_RUNTIME_CPP = (
    REPO / "unreal" / Path(HISTORICAL_RUNTIME_REPOSITORY_RELATIVE)
)
CONTEXT_POLICY_HEADER = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DContextPolicyActor.h"
)
CONTEXT_POLICY_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
FACTORY_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory.cpp"
)
HYBRID_EDITOR_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)
PROVIDER_DOC = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Docs"
    / "IstanaExploreV5DProviderQuality.md"
)
EDITOR_CPP = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary.cpp"
)
NATIVE_TEST = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests"
    / "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActorTests.cpp"
)
WRAPPER = (
    REPO
    / "scripts"
    / "Invoke-IstanaExploreV5DR29CopernicusTerrainFallbackNativeTransactionV1.ps1"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class R29CopernicusTerrainNativeFallbackContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.source_manifest = json.loads(
            SOURCE_MANIFEST.read_text(encoding="utf-8")
        )

    def test_exact_source_receipts_are_hash_pinned(self) -> None:
        pins = self.contract["sourcePins"]
        self.assertEqual(len(pins), 4)
        for pin in pins:
            path = PACKAGE / pin["path"]
            self.assertTrue(path.is_file(), pin["path"])
            self.assertEqual(path.stat().st_size, pin["bytes"], pin["path"])
            self.assertEqual(sha256(path), pin["sha256"], pin["path"])

    def test_obj_is_exact_129_square_in_metres(self) -> None:
        vertices: list[tuple[float, float, float]] = []
        texcoords = normals = triangles = 0
        materials: set[str] = set()
        for line in OBJ.read_text(encoding="utf-8").splitlines():
            fields = line.split()
            if not fields:
                continue
            if fields[0] == "v":
                vertices.append(tuple(map(float, fields[1:4])))
            elif fields[0] == "vt":
                texcoords += 1
            elif fields[0] == "vn":
                normals += 1
                normal = tuple(map(float, fields[1:4]))
                self.assertAlmostEqual(math.sqrt(sum(x * x for x in normal)), 1.0, places=5)
            elif fields[0] == "f":
                triangles += 1
                self.assertEqual(len(fields), 4)
            elif fields[0] == "usemtl":
                materials.add(fields[1])
        self.assertEqual(len(vertices), 129 * 129)
        self.assertEqual(texcoords, 129 * 129)
        self.assertEqual(normals, 129 * 129)
        self.assertEqual(triangles, 32768)
        self.assertEqual(materials, {"MI_IPV5D_R29_CopernicusTerrainProxy"})
        self.assertEqual(min(v[0] for v in vertices), -1000.0)
        self.assertEqual(max(v[0] for v in vertices), 1000.0)
        self.assertEqual(min(v[1] for v in vertices), -1000.0)
        self.assertEqual(max(v[1] for v in vertices), 1000.0)
        self.assertEqual(min(v[2] for v in vertices), -30.303865)
        self.assertEqual(max(v[2] for v in vertices), 1.193868)

    def test_native_import_scale_and_bounds_are_fail_closed(self) -> None:
        import_contract = self.contract["assetImport"]
        self.assertEqual(import_contract["requiredUniformScale"], 100.0)
        self.assertEqual(import_contract["expectedBoundsCentimetres"]["x"], [-100000.0, 100000.0])
        self.assertEqual(import_contract["expectedBoundsCentimetres"]["y"], [-100000.0, 100000.0])
        factory = FACTORY_CPP.read_text(encoding="utf-8")
        runtime = RUNTIME_CPP.read_text(encoding="utf-8")
        for token in (
            "ImportUniformScale = RequiredImportScale",
            "ExpectedBoundsMin(-100000.0, -100000.0, -3030.3865)",
            "ExpectedBoundsMax(100000.0, 100000.0, 119.3868)",
            "bAutoGenerateCollision = false",
            "MarkAsNotHavingNavigationData",
        ):
            self.assertIn(token, factory)
        self.assertIn("QueryAndPhysics", runtime)
        self.assertIn("SetTerrainRendererVisiblePreservingCollision", runtime)

    def test_single_render_section_covers_the_complete_mesh(self) -> None:
        factory = FACTORY_CPP.read_text(encoding="utf-8")
        for token in (
            "RenderData->LODResources[0].Sections[0]",
            "RenderSection.FirstIndex != 0",
            "RenderSection.NumTriangles != ExpectedTriangleCount",
            "RenderSection.MaterialIndex != 0",
            "Section.MaterialIndex != 0",
            "OriginalSection.MaterialIndex != Section.MaterialIndex",
            "OriginalSection.bEnableCollision != Section.bEnableCollision",
            "OriginalSection.bCastShadow != Section.bCastShadow",
            "OriginalSection.bAffectDistanceFieldLighting !=",
        ):
            self.assertIn(token, factory)

    def test_missing_asset_paths_are_never_loaded_before_roster_admission(self) -> None:
        factory = FACTORY_CPP.read_text(encoding="utf-8")
        validator = factory[factory.index("bool ValidateInternal(") :]
        self.assertLess(
            validator.index("ValidateRootRoster(bRequireSaved, OutReport)"),
            validator.index("LoadExact<UMaterial>(MaterialObjectPath)"),
        )

    def test_mask_is_exact_complement_of_existing_core_oracle(self) -> None:
        mask = self.contract["authoredCoreProtection"]
        self.assertEqual(mask["strategy"], "EXACT_BINARY_COMPLEMENT_OF_EXISTING_GROUND_MACRO_OVERLAY_MASK")
        self.assertEqual(mask["providerClipShape"], "64_EDGE_IRREGULAR_ELLIPSE")
        self.assertEqual(mask["opaqueCollarMeters"], 50.0)
        self.assertEqual(mask["outwardFeatherMeters"], 8.0)
        self.assertEqual(mask["stableDitherCellMeters"], 0.25)
        factory = FACTORY_CPP.read_text(encoding="utf-8")
        runtime = RUNTIME_CPP.read_text(encoding="utf-8")
        native_test = NATIVE_TEST.read_text(encoding="utf-8")
        for token in (
            "ExpectedProviderSiteClipRippleAmplitudes",
            "ExpectedProviderSiteClipRipplePhasesRadians",
            "ExpectedProviderSiteClipSegmentParameterEpsilon",
            "ExpectedGroundOverlayOpaqueCollarMeters",
            "ExpectedGroundOverlayOutwardFeatherMeters",
            "ExpectedGroundOverlayDitherCellMeters",
            "return 1.0-coreMask;",
        ):
            self.assertIn(token, factory)
        self.assertIn("!ATRIADIstanaExploreV5DGroundVegetationActor::", runtime)
        self.assertIn("Every sample has exactly one visual owner", native_test)
        self.assertIn("625", native_test)

    def test_cesium_preferred_and_all_simulation_authority_denied(self) -> None:
        runtime_selection = self.contract["runtimeSelection"]
        self.assertIn("CESIUM_WORLD_TERRAIN", runtime_selection["preferred"])
        self.assertEqual(
            runtime_selection["failClosedState"],
            "LOCAL_DEM_HIDDEN_AND_SOURCE_TERRAIN_RENDERER_VISIBLE",
        )
        authority = self.contract["authority"]
        self.assertTrue(authority["renderOnly"])
        self.assertTrue(
            all(value is False for key, value in authority.items() if key != "renderOnly")
        )
        combined = "\n".join(
            path.read_text(encoding="utf-8")
            for path in (RUNTIME_HEADER, RUNTIME_CPP, FACTORY_CPP, EDITOR_CPP)
        )
        for token in (
            "bCesiumWorldTerrainPreferred = true",
            "bCollisionNavigationSensorRfAuthority = false",
            "bAbsoluteHeightGeospatialOrSurveyAuthority = false",
            "bBareEarthDtmClaimed = false",
            "bLocalBuildingFallbackCurrentlyHidden",
            "FailClosedToSourceTerrain",
        ):
            self.assertIn(token, combined)

    def test_world_terrain_label_is_not_a_dedicated_terrain_provider_claim(self) -> None:
        """Cross-check the R29 label against the provider actually authored."""
        runtime_selection = self.contract["runtimeSelection"]
        self.assertIn("CESIUM_WORLD_TERRAIN", runtime_selection["preferred"])
        self.assertEqual(
            runtime_selection["providerReadySignal"],
            "ATRIADIstanaExploreV5DContextPolicyActor."
            "bLocalBuildingFallbackCurrentlyHidden",
        )

        context_header = CONTEXT_POLICY_HEADER.read_text(encoding="utf-8")
        context_cpp = CONTEXT_POLICY_CPP.read_text(encoding="utf-8")
        hybrid_editor = HYBRID_EDITOR_CPP.read_text(encoding="utf-8")
        runtime = RUNTIME_CPP.read_text(encoding="utf-8")
        provider_doc = PROVIDER_DOC.read_text(encoding="utf-8")
        for token in (
            "ExpectedIonAssetId = 2275207",
            "TotalTilesetCount == 1",
            "TaggedVisualTilesetCount == 1",
            "const float LoadProgress = Tileset->GetLoadProgress();",
            "HideLocalBuildingFallbackAtLoadProgress",
            "RestoreLocalBuildingFallbackBelowLoadProgress",
        ):
            self.assertIn(token, context_header + context_cpp)
        for token in (
            "GooglePhotorealistic3DTilesIonAssetId = 2275207",
            "Cesium Photorealistic 3D Tiles - V5D Visual Context Only",
            "SetIonAssetID(GooglePhotorealistic3DTilesIonAssetId)",
        ):
            self.assertIn(token, hybrid_editor)
        self.assertIn(
            "Policy->bLocalBuildingFallbackCurrentlyHidden", runtime
        )
        self.assertIn(
            "The Google visual tileset must bind",
            " ".join(provider_doc.split()),
        )
        self.assertFalse(
            self.source_manifest["authorityFlags"][
                "isCesiumProviderReadinessProof"
            ]
        )

    def test_real_world_terrain_accuracy_admission_remains_fail_closed(self) -> None:
        manifest = self.source_manifest
        source = manifest["sourceDataset"]
        quality = manifest["qualityAdmission"]
        heights = manifest["heightSemantics"]
        self.assertEqual(
            source["surfaceType"],
            "DIGITAL_SURFACE_MODEL_INCLUDES_BUILDINGS_INFRASTRUCTURE_AND_VEGETATION",
        )
        self.assertAlmostEqual(source["nativeNominalResolutionMetres"], 30.9)
        self.assertFalse(quality["authoritativeTerrainAdmitted"])
        self.assertEqual(
            set(quality["authoritativeTerrainRejectionReasons"]),
            {
                "SOURCE_IS_A_30M_DSM_NOT_A_BARE_EARTH_DTM",
                "HEIGHT_ERROR_MASK_DOES_NOT_COVER_THE_COMPLETE_AOI",
                "NO_EGM2008_TO_WGS84_ELLIPSOID_CONVERSION_WAS_ADMITTED",
                "NO_SURVEY_CONTROL_OR_HELD_OUT_CHECKPOINT_RESIDUALS_ARE_PRESENT",
            },
        )
        self.assertFalse(quality["heightErrorMaskAoiComplete"])
        self.assertLess(
            manifest["sourceAoiMetrics"]["heightErrorSigmaMeters"][
                "validSampleFraction"
            ],
            1.0,
        )
        self.assertFalse(heights["absoluteCesiumPlacementApproved"])
        self.assertFalse(heights["ellipsoidConversionApplied"])
        self.assertIn("No admitted EGM2008 geoid conversion grid", heights["reason"])
        self.assertFalse(
            self.contract["sourceDataset"]["absolutePlacementApproved"]
        )
        self.assertFalse(self.contract["authority"]["geospatialPlacement"])
        self.assertFalse(self.contract["authority"]["absoluteHeight"])

    def test_map_apply_is_additive_hash_and_backup_guarded(self) -> None:
        editor = EDITOR_CPP.read_text(encoding="utf-8")
        for token in (
            "ExpectedPredecessorBytes",
            "ExpectedPredecessorSha256",
            "V5DR29CopernicusTerrainFallbackV1",
            "FPaths::IsUnderDirectory",
            "APPLY_REFUSED_FINAL_MUTATION_GATE",
            "SpawnActor<",
            "SaveMap",
            "LoadMap",
            "backupPreserved=true",
            "oneSave=true",
            'TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.h',
            "ValidateR29FacadeReplacementInLoadedV5DHybridMap(FacadeReport)",
        ):
            self.assertIn(token, editor)
        self.assertNotIn("ValidatePublicViewScene(SceneReport, false)", editor)
        self.assertNotIn("SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics)", editor)

    def test_wrapper_is_source_only_by_default_and_hash_pins_every_input(self) -> None:
        wrapper = WRAPPER.read_text(encoding="utf-8")
        for token in (
            "HashPinnedCodeInputs=7",
            "HashPinnedSourceInputs=5",
            "ExactOutputAssetCount=2",
            "ExactImportUniformScale=100.0",
            "NativeTreeReadOrWritten=$false",
            "UnrealBuildOrEditorLaunched=$false",
            "ExpectedMapSha256",
            "ExpectedRuntimeDllSha256",
            "ExpectedEditorDllSha256",
            "predecessor.umap",
            "UnrealEditor-Cmd.exe",
            "'-Module=TRIADSensorFusion'",
            "'-Module=TRIADSensorFusionEditor'",
            "'-ForceHeaderGeneration'",
            "'-NoUBTMakefiles'",
            "'-MaxParallelActions=1'",
            "ForcedObjectInvalidationCount=$forcedObjects.Count",
            "Remove-Item -LiteralPath $objectPath -Force",
            "raw if isinstance(raw, tuple) else (raw,)",
            "function Invoke-TerrainRemoteStage(",
            "-RemoteControlHttpServer -RCWebControlEnable",
            "Get-OwnedHelperIdentity",
            "-PassThru -WindowStyle Hidden",
            "ApplyCopernicusTerrainFallbackToLoadedHybridMap",
            "ValidateCopernicusTerrainFallbackSuccessorMap",
            "$response.ReturnValue -ne $true",
            "$response.OutReport",
            "R29_COPERNICUS_TERRAIN_SUCCESSOR_VALID",
            "Apply process did not produce a changed successor with an immutable backup.",
            "SuccessorRuntimeDll",
            "SuccessorEditorDll",
            "commit.json",
            "Set-Content -LiteralPath $commitPath -Encoding utf8NoBOM -NoNewline",
            "NativeVisualAcceptance=$false",
        ):
            self.assertIn(token, wrapper)
        self.assertNotIn("D:\\triad", wrapper)

    def test_historical_actor_source_closure_maps_to_canonical_native_path(self) -> None:
        self.assertTrue(HISTORICAL_RUNTIME_CPP.is_file())
        self.assertEqual(HISTORICAL_RUNTIME_CPP.stat().st_size, 19064)
        self.assertEqual(
            sha256(HISTORICAL_RUNTIME_CPP),
            "F57C4C51F666290EF3C74CB03E4A536C476250CCC3AC3BD008D92456C824CA36",
        )

        historical_runtime = HISTORICAL_RUNTIME_CPP.read_text(encoding="utf-8")
        self.assertIn("const bool bProviderReady =", historical_runtime)
        self.assertIn(
            "Policy->bLocalBuildingFallbackCurrentlyHidden;", historical_runtime
        )
        self.assertNotIn("bR33DualCesiumContextConfigured", historical_runtime)
        self.assertNotIn("ShouldPresentR29CopernicusFallback", historical_runtime)

        live_runtime = RUNTIME_CPP.read_text(encoding="utf-8")
        self.assertIn("bR33DualCesiumContextConfigured", live_runtime)
        self.assertIn("ShouldPresentR29CopernicusFallback", live_runtime)

        wrapper = WRAPPER.read_text(encoding="utf-8")
        actor_pin = next(
            line
            for line in wrapper.splitlines()
            if "F57C4C51F666290EF3C74CB03E4A536C476250CCC3AC3BD008D92456C824CA36"
            in line
        )
        self.assertIn(
            f"RepositoryRelativePath='{HISTORICAL_RUNTIME_REPOSITORY_RELATIVE}'",
            actor_pin,
        )
        self.assertIn(
            f"NativeRelativePath='{HISTORICAL_RUNTIME_NATIVE_RELATIVE}'",
            actor_pin,
        )
        self.assertNotIn("[pscustomobject]@{RelativePath=", actor_pin)
        self.assertIn(
            "$source = Join-Path $repoUnrealRoot "
            "(Get-PinRelativePath $pin 'Repository')",
            wrapper,
        )
        self.assertIn(
            "$destination = Join-Path $nativeRoot "
            "(Get-PinRelativePath $pin 'Native')",
            wrapper,
        )

    @unittest.skipUnless(shutil.which("pwsh"), "PowerShell 7 is unavailable")
    def test_static_self_check_never_reads_or_writes_native_tree(self) -> None:
        completed = subprocess.run(
            [
                shutil.which("pwsh") or "pwsh",
                "-NoProfile",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "python_test",
                "-StaticSelfCheck",
            ],
            cwd=REPO,
            check=True,
            capture_output=True,
            text=True,
        )
        receipt = json.loads(completed.stdout)
        self.assertEqual(receipt["Status"], "STATIC_SELF_CHECK_PASS")
        self.assertEqual(receipt["HashPinnedCodeInputs"], 7)
        self.assertEqual(
            receipt["HistoricalActorRepositorySource"],
            HISTORICAL_RUNTIME_REPOSITORY_RELATIVE,
        )
        self.assertEqual(
            receipt["HistoricalActorNativeDestination"],
            HISTORICAL_RUNTIME_NATIVE_RELATIVE,
        )
        self.assertEqual(receipt["ExactImportUniformScale"], 100.0)
        self.assertEqual(receipt["RequiredPostImportBoundsXCentimeters"], [-100000.0, 100000.0])
        self.assertTrue(receipt["CesiumWorldTerrainPreferred"])
        self.assertTrue(receipt["SourceTerrainQueryAndPhysicsPreserved"])
        self.assertFalse(receipt["CollisionNavigationSensorRfGeospatialAuthority"])
        self.assertFalse(receipt["UnrealBuildOrEditorLaunched"])
        self.assertFalse(receipt["NativeTreeReadOrWritten"])
        self.assertFalse(receipt["TargetMapMutated"])


if __name__ == "__main__":
    unittest.main()
