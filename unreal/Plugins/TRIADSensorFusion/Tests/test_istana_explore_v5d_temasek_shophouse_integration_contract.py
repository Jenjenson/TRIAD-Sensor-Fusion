import hashlib
import json
import math
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PLUGIN = Path(__file__).resolve().parents[1]
UNREAL = PLUGIN.parents[1]
R24 = UNREAL / "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R24TemasekShophouse"
GENERATED = R24 / "Generated"
RUNTIME = PLUGIN / "Source/TRIADSensorFusion"
EDITOR = PLUGIN / "Source/TRIADSensorFusionEditor"
ACTOR_H = RUNTIME / "Public/TRIADIstanaExploreV5DTemasekShophouseActor.h"
ACTOR_CPP = RUNTIME / "Private/TRIADIstanaExploreV5DTemasekShophouseActor.cpp"
PROVENANCE = RUNTIME / "Private/TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp"
FACTORY = EDITOR / "Private/TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp"
EDITOR_CPP = EDITOR / "Private/TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp"
EDITOR_H = EDITOR / "Public/TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h"
HYBRID_H = EDITOR / "Public/TRIADIstanaExploreV5DHybridEditorLibrary.h"
HYBRID_CPP = EDITOR / "Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
NATIVE_TEST = RUNTIME / "Private/Tests/TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp"
TRANSACTION = UNREAL.parent / "scripts/Invoke-R24NativeTransaction.ps1"
WRAPPER = UNREAL.parent / "scripts/Capture-IstanaExploreV5DTemasekShophouseR24Evidence.ps1"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def digest(path: Path) -> str:
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
                return source[brace:index + 1]
    raise AssertionError(f"unterminated function: {signature}")


class TemasekShophouseNativeIntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._generated_temp = tempfile.TemporaryDirectory(
            prefix="triad_temasek_r24_integration_"
        )
        cls.generated = Path(cls._generated_temp.name)
        subprocess.run(
            [
                sys.executable,
                str(R24 / "build_temasek_shophouse_r24.py"),
                "--output-dir",
                str(cls.generated),
            ],
            check=True,
            capture_output=True,
            text=True,
        )
        cls.placement = json.loads(text(cls.generated / "temasek_shophouse_r24.unreal_placement.json"))
        cls.manifest = json.loads(text(cls.generated / "IstanaPublicViewV5DR24TemasekShophouse.manifest.json"))
        cls.materials = json.loads(text(cls.generated / "IstanaPublicViewV5DR24TemasekShophouse.materials.json"))
        cls.actor = text(ACTOR_H) + text(ACTOR_CPP)
        cls.provenance = text(PROVENANCE)
        cls.factory = text(FACTORY)
        cls.editor = text(EDITOR_H) + text(EDITOR_CPP)
        cls.hybrid = text(HYBRID_H) + text(HYBRID_CPP)
        cls.hybrid_cpp = text(HYBRID_CPP)
        cls.native = text(NATIVE_TEST)
        cls.transaction = text(TRANSACTION)
        cls.wrapper = text(WRAPPER)

    @classmethod
    def tearDownClass(cls) -> None:
        cls._generated_temp.cleanup()

    def test_capture_process_pins_provider_safe_http_concurrency(self) -> None:
        self.assertIn("HttpMaxConnectionsPerServer=12", self.wrapper)
        self.assertNotIn("HttpMaxConnectionsPerServer=16", self.wrapper)
        self.assertIn("MaxCacheItems=32768", self.wrapper)

    def test_source_receipts_are_exact_and_factory_pinned(self) -> None:
        expected = {
            "Generated/SM_IPV5D_R24_TemasekShophouse_Render.obj": (610314, "F5ED94C312A38A542D9C078F19F16FAF04CEA6ECAE5B4C2CF0AA7F3D651B9007"),
            "Generated/SM_IPV5D_R24_TemasekShophouse_Render.mtl": (2355, "41D50D10924A2AA0FECAB2ADC48E7BC071A20ED8A9F2D109BE81E0D2EF2CE4CB"),
            "Generated/IstanaPublicViewV5DR24TemasekShophouse.geometry.json": (2149277, "CC40E8C2C975D2A19124D74EAAC80B6C14C7121AD6BDDD616A740620737B05A7"),
            "Generated/IstanaPublicViewV5DR24TemasekShophouse.features.json": (7376, "900BED02B0C5CBFD8D2668C017B33F34B800B354F251B57767DE243F8126E4E0"),
            "Generated/IstanaPublicViewV5DR24TemasekShophouse.materials.json": (6610, "945404D4E6EC0E873FCDCBDA13977A67747961A3ADAAD413D4C1D20690EBE3FF"),
            "Generated/IstanaPublicViewV5DR24TemasekShophouse.manifest.json": (9131, "ED46D1B6367859A42BF1404FE1C0F5E457C16276777818B510FD8A8CA3BE8D87"),
            "Generated/temasek_shophouse_r24.unreal_placement.json": (4517, "6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5"),
            "temasek_shophouse_r24.contract.json": (10720, "F304EE5D090D41F2331F293E6CD797A9B27D5B3B74DE44DCAA6A3CC2A66FE082"),
            "Sources/public_sources.json": (5870, "E4D1B0F3648B87E577A004F158821EBD716E0288511C51A468E272D475DE009E"),
        }
        for relative, (size, sha) in expected.items():
            path = (
                self.generated / relative.removeprefix("Generated/")
                if relative.startswith("Generated/")
                else R24 / relative
            )
            self.assertEqual(size, path.stat().st_size, relative)
            self.assertEqual(sha, digest(path), relative)
            self.assertIn(relative, self.factory)
            self.assertIn(sha, self.factory)

    def test_census_and_all_fifteen_non_foliage_sections_are_bound(self) -> None:
        self.assertEqual(828, self.manifest["counts"]["components"])
        self.assertEqual(9536, self.manifest["counts"]["vertices"])
        self.assertEqual(15760, self.manifest["counts"]["triangles"])
        self.assertEqual(15, self.manifest["counts"]["materials"])
        expected_slots = sorted(row["slot"] for row in self.materials["materials"])
        self.assertEqual(15, len(expected_slots))
        for slot in expected_slots:
            self.assertIn(f'TEXT("{slot}")', self.factory)
            self.assertIn(f'TEXT("{slot}")', self.actor)
        self.assertIn("ExpectedMaterialCount = 15", self.factory)
        self.assertIn("ExpectedTriangleCount = 15760", self.factory)
        self.assertIn("ExpectedComponentCount = 828", self.factory)

    def test_placement_is_exact_but_explicitly_non_survey(self) -> None:
        transform = self.placement["unrealTransform"]
        self.assertEqual([40411.657951, 88424.311139, 0.0], transform["translationCentimeters"])
        self.assertEqual([0.0, -162.5152283523459, 0.0], transform["rotationDegrees"])
        self.assertEqual([1.053891, 1.221655, 1.0], transform["scale3D"])
        self.assertIn("IGNORE_VOLUNTEERED_OSM_40_M_HEIGHT", transform["heightPolicy"])
        self.assertIn("NO_ONE_TO_ONE_FACADE_OR_LANDSCAPE_ALIGNMENT_CLAIM", transform["limitations"])
        for value in ("40411.657951", "88424.311139", "-162.5152283523459", "1.053891", "1.221655"):
            self.assertIn(value, self.actor + self.native)
        receipt = self.placement["sourceFeatureReceipt"]
        self.assertEqual("OSM:way:1551538490", receipt["sourceKey"])
        self.assertEqual("A273E09B21C71AA7436C5B156E643AF4176F3B90A215DD0FEA56928FA5BDDEF7", receipt["featureGeometrySha256"])

    def test_provenance_recomputes_cooked_render_payload(self) -> None:
        combined = self.provenance + self.factory + self.actor
        for marker in (
            "0ACA9941B8C60766EA4647A991DF80944B4A746E06CC2A5680C2D167C0FA4A4D",
            "PositionVertexBuffer", "GetArrayView()", "Section.MaterialIndex",
            "Material.MaterialSlotName", "CookedRenderPayloadSha256",
            "RecomputedPayloadSha256 != Provenance->CookedRenderPayloadSha256",
            "SourceObjProjectRelativePath", "FPaths::MakePathRelativeTo",
        ):
            self.assertIn(marker, combined)

    def test_imported_slot_metadata_is_editor_only_and_cook_digest_stable(self) -> None:
        runtime_validation = function_body(text(ACTOR_CPP), "bool ValidateMesh(")
        payload = function_body(
            self.provenance, "ComputeCookedRenderPayloadSha256("
        )
        runtime_compact = "".join(runtime_validation.split())
        payload_compact = "".join(payload.split())

        self.assertIn(
            "boolbImportedSlotNameMatches=true;"
            "#ifWITH_EDITORONLY_DATA"
            "bImportedSlotNameMatches="
            "Binding.ImportedMaterialSlotName==FName(MaterialSpecs[Index].Slot);"
            "#endif",
            runtime_compact,
        )
        self.assertIn(
            "Binding.MaterialSlotName!=FName(MaterialSpecs[Index].Slot)||"
            "!bImportedSlotNameMatches||",
            runtime_compact,
        )
        self.assertEqual(1, runtime_validation.count("Binding.ImportedMaterialSlotName"))
        self.assertIn(
            "AppendUtf8(Payload,Material.MaterialSlotName.ToString());"
            "#ifWITH_EDITORONLY_DATA"
            "AppendUtf8(Payload,Material.ImportedMaterialSlotName.ToString());"
            "#else"
            "AppendUtf8(Payload,Material.MaterialSlotName.ToString());"
            "#endif",
            payload_compact,
        )
        self.assertEqual(1, payload.count("Material.ImportedMaterialSlotName"))
        self.assertEqual(2, payload.count("Material.MaterialSlotName.ToString()"))
        self.assertIn(
            "Source.MaterialSlotName != Source.ImportedMaterialSlotName",
            self.factory,
        )

    def test_materials_are_texture_free_deterministic_pbr_priors(self) -> None:
        for marker in (
            "TShWhiteBase", "TShCharcoalBase", "TShGlassBase", "TShTimberBase",
            "TShBrassBase", "TShMosaicBase", "TShConcreteBase", "TShTerracottaBase",
            "TShSolarBase", "TShPaverLightBase", "TShSoilBase", "TShWaterBase",
            "TextureSamples != 0",
            "Options->bImportMaterials = false", "Options->bImportTextures = false",
            "Material->bUsedWithNanite = true", "FallbackPercentTriangles = 1.0f",
        ):
            self.assertIn(marker, self.factory)
        self.assertTrue(all(not row["textureAssets"] for row in self.materials["materials"]))
        self.assertTrue(all(not row["calibrated"] for row in self.materials["materials"]))

    def test_runtime_layer_is_visible_render_only_and_provider_telemetry_only(self) -> None:
        combined = self.actor + self.native
        for marker in (
            "SetActorEnableCollision(false)", "ECollisionEnabled::NoCollision",
            "SetCanEverAffectNavigation(false)", "bCoarseShellOverlapResolved = false",
            "bProviderOverlapResolved = false", "bProviderReadyLiveSuccessorClaimed = false",
            "SetVisibility(true, true)", "SetHiddenInGame(false, true)",
            "Provider false telemetry records", "Overlay remains visible at provider true",
            "Overlay remains visible after false-true-false",
        ):
            self.assertIn(marker, combined)
        self.assertNotIn("UCesiumPolygonRasterOverlay", self.actor)

    def test_asset_import_is_exact_empty_predecessor_transaction(self) -> None:
        for marker in (
            "ExpectedAssetCount = 16", "ValidateExactEmptyAssetRoot",
            "FreshAssetTransactionOwnedObjectPaths", "FUnloadPackageParams",
            "bResetTransBuffer = true", "GetExactOwnedPackageArtifactPaths",
            "DeleteObjectsUnchecked", "IFileManager::Get().Delete",
            "ScanPathsSynchronous", "ValidateExactEmptyAssetRoot",
            "SaveLoadedAssets", "ReloadPackages", "CommitActiveFreshAssetTransaction",
        ):
            self.assertIn(marker, self.factory + self.editor)
        self.assertIn("assets=16", self.factory)
        self.assertIn("ImportIstanaExploreV5DTemasekShophouseR24Assets", self.editor)
        self.assertIn("ValidateIstanaExploreV5DTemasekShophouseR24Assets", self.editor)
        self.assertNotIn("R24Bssets", self.editor)

    def test_map_migration_is_backup_save_reload_and_idempotence_guarded(self) -> None:
        for marker in (
            "ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap",
            "CreateVerifiedPreTemasekShophouseR24MapBackup",
            "RestoreVerifiedPreTemasekShophouseR24MapViaSiblingTemp",
            "V5DTemasekShophouseR24_20260903", "FPaths::CreateTempFilename",
            "IFileManager::Get().Move", "backupPreserved=true",
            "SpawnActor<ATRIADIstanaExploreV5DTemasekShophouseActor>",
            "ValidateTemasekShophouseOverlay", "IDEMPOTENT_EXPLORE_V5D_R24_TEMASEK_ALREADY_VALID",
            "providerAndCoarseShellOverlapUnresolved=true",
        ):
            self.assertIn(marker, self.hybrid)
        self.assertIn("ImportApplyAndValidateIstanaExploreV5DTemasekShophouseR24", self.editor)
        self.assertIn("TRIAD.Istana.ExploreV5D.TemasekShophouse.EndToEndContract", self.editor)

    def test_canonical_gate_rejects_missing_duplicate_and_provider_drift(self) -> None:
        body = function_body(self.hybrid, "bool ValidateHybridWorld(")
        for marker in (
            "enum class ELandmarkPresencePolicy",
            "Forbidden",
            "Optional",
            "Required",
        ):
            self.assertIn(marker, self.hybrid)
        for marker in (
            "MatchesLandmarkPresence(TemasekShophouseCount, TemasekShophousePolicy)",
            "TemasekShophouse->bProviderReady !=",
            "r24TemasekShophouseCount=%d",
        ):
            self.assertIn(marker, body)
        self.assertIn("ELandmarkPresencePolicy::Required", self.hybrid)
        self.assertIn("ELandmarkPresencePolicy::Forbidden", self.hybrid)
        self.assertIn("exactly-one-MacDonald/no-Temasek", self.hybrid)

    def test_fresh_builder_creates_the_strict_final_two_landmark_world(self) -> None:
        builder = function_body(self.hybrid_cpp, "BuildIstanaExploreV5DHybridMap(")
        for marker in (
            "TemasekShophouseAssetFactory::",
            "LoadValidatedRuntimeMesh(",
            "ExistingTemasekShophouseCount != 0",
            "SpawnActor<ATRIADIstanaExploreV5DTemasekShophouseActor>",
            "ConfigureTemasekShophouse(",
            "ExpectedPlacementTransform()",
            "ELandmarkPresencePolicy::Required",
        ):
            self.assertIn(marker, builder)

    def test_authority_remains_fail_closed(self) -> None:
        authority = self.placement["runtimeAuthorityPolicy"]
        self.assertTrue(authority["renderOnly"])
        for key in (
            "collisionAuthority", "navigationAuthority", "sensorOcclusionAuthority",
            "rfGeometryAuthority", "rfMaterialAuthority", "surveyAuthority",
            "asBuiltAuthority", "interiorAuthority", "securityDetailIncluded",
            "proceduralPbrMaterialsCalibrated",
        ):
            self.assertFalse(authority[key], key)
        for marker in (
            "bCollisionNavigationSensorOrRfAuthority = false",
            "bSurveyAsBuiltOrOneToOne = false",
            "bProviderReadyLiveSuccessor = false",
        ):
            self.assertIn(marker, self.provenance)

    def test_native_transaction_has_external_migration_rollback_and_exact_gates(self) -> None:
        for marker in (
            "New-MigrationRollbackJournal",
            "migration_before",
            "MapBefore = $MapSnapshot",
            "MacDonaldBefore = $MacDonaldInventory",
            "TemasekBefore = $TemasekInventory",
            "Restore-MigrationRollbackJournal",
            "Remove-Item -LiteralPath $path -Force",
            "Assert-MigrationInventoryRestored",
            "externalRollback={$rollbackStatus}",
            "$TestFilter.Count -ne $allowedTestFilters.Count",
            "$filter -cne $allowedTestFilters[$filterIndex]",
            "exact ordered five-filter gate roster",
        ):
            self.assertIn(marker, self.transaction)
        self.assertNotIn("Remove-Item -LiteralPath $path -Recurse", self.transaction)

    def test_capture_wrapper_has_strict_additive_provider_fallback_contract(self) -> None:
        self.assertIn(
            "[ValidateSet('TelemetryOnly', 'ProviderFallback', 'ProviderReady')]",
            self.wrapper,
        )
        for classification in (
            "EXPLICIT_NON_PROOF_TELEMETRY_DIAGNOSTIC",
            "STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF",
            "PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED",
        ):
            self.assertIn(classification, self.wrapper)

        state = function_body(self.wrapper, "function Test-ExactPoseState")
        self.assertIn("$EvidenceMode -ceq 'ProviderFallback'", state)
        self.assertIn("'providerReadyForProof=false'", state)
        self.assertIn("'localFallbackHidden=false'", state)
        wait = function_body(self.wrapper, "function Wait-StableExactPoseState")
        self.assertIn(
            "$requireFallback = $ProviderEvidenceMode -ceq 'ProviderFallback'",
            wait,
        )
        self.assertIn("-EvidenceMode $ProviderEvidenceMode", wait)
        self.assertIn("ProviderFallbackRequired = $requireFallback", wait)
        self.assertIn(
            "providerReadyForProof=false and localFallbackHidden=false readbacks",
            wait,
        )

        post = function_body(self.wrapper, "function Assert-PostCaptureWorld")
        self.assertIn("-EvidenceMode $ProviderEvidenceMode", post)
        self.assertIn("ProviderTelemetry = $providerTelemetry", post)

        capture_start = self.wrapper.index("$stateWindow = Wait-StableExactPoseState")
        immediate = self.wrapper.index("$preCaptureState = Invoke-OwnedRcCall", capture_start)
        immediate_gate = self.wrapper.index(
            "-Pose $pose -EvidenceMode $ProviderEvidenceMode", immediate
        )
        capture_time = self.wrapper.index(
            "$captureRequestedUtc = [DateTime]::UtcNow", immediate_gate
        )
        capture_call = self.wrapper.index(
            "$capture = Invoke-RequiredOwnedRcCall", capture_time
        )
        self.assertLess(capture_start, immediate)
        self.assertLess(immediate, immediate_gate)
        self.assertLess(immediate_gate, capture_time)
        self.assertLess(capture_time, capture_call)
        capture = self.wrapper[capture_time : self.wrapper.index("$png = Wait-StablePng", capture_time)]
        self.assertIn("$ProviderEvidenceMode -cne 'ProviderReady'", capture)
        self.assertIn("'CaptureIstanaExploreV5DHybridDiagnosticPlayView'", capture)
        self.assertIn("'CaptureIstanaExploreV5DHybridPlayView'", capture)
        self.assertIn("'NON-PROOF DIAGNOSTIC EVIDENCE:'", capture)
        self.assertIn("$strictFallbackCaptureAcknowledgementMarker", capture)
        self.assertIn(
            "'localFallbackVisible=true localFallbackHidden=false'", self.wrapper
        )

    def test_capture_wrapper_preserves_exact_multi_session_capstone_snapshot(self) -> None:
        self.assertIn(
            r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe",
            self.wrapper,
        )
        self.assertIn(
            r"C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject",
            self.wrapper,
        )
        snapshot = function_body(self.wrapper, "function Get-ProtectedUE54Identity")
        for marker in (
            "CreationUtcTicks",
            "Sort-Object ProcessId",
            "SessionCount = $identities.Count",
            "Sessions = @($identities)",
        ):
            self.assertIn(marker, snapshot)
        self.assertNotIn("ExpectedState", snapshot)
        unchanged = function_body(
            self.wrapper, "function Assert-ProtectedUE54Unchanged"
        )
        self.assertIn("ConvertTo-Json -Compress -Depth 8", unchanged)
        self.assertIn(
            "$script:protectedUE54Before = Get-ProtectedUE54Identity",
            self.wrapper,
        )
        self.assertIn("ProtectedUE54Before", self.wrapper)
        self.assertIn("ProtectedUE54After", self.wrapper)
        self.assertNotIn("Get-VerifiedProtectedUE54Identity", self.wrapper)
        self.assertNotIn("Get-UnprotectedUnrealEditors", self.wrapper)
        self.assertNotIn("protectedUE54ExpectedState", self.wrapper)
        self.assertNotIn("protectedUE54ProcessId", self.wrapper)

    def test_capture_wrapper_rejects_only_native_ue55_or_exact_triad_helpers(self) -> None:
        scoped = function_body(
            self.wrapper, "function Get-NativeTRIADUnrealProcesses"
        )
        self.assertIn("$nativeUE55EngineRoot", scoped)
        self.assertIn("-Token $projectFile", scoped)
        self.assertIn("$isUE55 -or $isNativeTRIAD", scoped)
        self.assertNotIn("CAPSTONE", scoped)
        self.assertIn(
            "the protected UE5.4/CAPSTONE set may remain open", self.wrapper
        )

    def test_capture_wrapper_keeps_caller_selected_map_and_dll_pins(self) -> None:
        for parameter in (
            "$ExpectedMapBytes",
            "$ExpectedMapSha256",
            "$ExpectedRuntimeDllBytes",
            "$ExpectedRuntimeDllSha256",
            "$ExpectedEditorDllBytes",
            "$ExpectedEditorDllSha256",
        ):
            self.assertIn(parameter, self.wrapper)
        for pin in ("$mapPin", "$runtimeDllPin", "$editorDllPin"):
            self.assertIn(f"Assert-ExactPin -Pin {pin}", self.wrapper)
            self.assertIn(pin, self.wrapper[
                self.wrapper.index("$script:boundaryPins = @(") :
                self.wrapper.index(
                    "Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'"
                )
            ])

        for marker in (
            "phase2_r26_native_retry2_20260906T0608SGT",
            "F1006E1338DC74311749AA0E4C95940795EA807DFFF8E813E3C1D2593801E58A",
            "36335002L",
            "0CDA45D7390A92885A911C1CD3404B0B59E889CE54A5879B8F1234197F73D498",
            "4672000L",
            "0B90C971AA385244FD20D2916BA87B7AE013C0C40F5B1153FB237E4CCF156191",
            "7725056L",
            "7F5D8596FD032DFAC335D7DC9AD2EC9DC7B72029144D36A40292CB8C19510F5A",
            "Caller-selected map/DLL pins do not exactly match the sealed Phase-2 commit receipt",
        ):
            self.assertIn(marker, self.wrapper)

    def test_capture_wrapper_seals_phase2_roster_and_foliage_handoff(self) -> None:
        for marker in (
            "$expectedTriangleCount = 15760",
            "$expectedSourceVertexCount = 9536",
            "$expectedComponentCount = 828",
            "$expectedMaterialCount = 15",
            "$expectedPackageCount = 16",
            "$expectedBakedFoliageCount = 0",
            "$expectedFoliageTreeAnchorCount = 3",
            "ATRIADIstanaExploreV5DLandmarkVegetationActor",
            "function Get-ValidatedPhase2CommitReceipt",
            "function Assert-Phase2FoliageLayout",
            "RemovedLegacyFoliagePackageStems",
        ):
            self.assertIn(marker, self.wrapper)
        for removed in (
            "M_TSH_Bark_PBR_R24",
            "M_TSH_LeafDeep_PBR_R24",
            "M_TSH_LeafLight_PBR_R24",
            "M_TSH_PollinatorBloom_PBR_R24",
        ):
            self.assertIn(removed, self.wrapper)
        roster = self.wrapper[
            self.wrapper.index("$temasekAssetRelativeStems = @(") :
            self.wrapper.index("$removedLegacyFoliageRelativeStems = @(")
        ]
        for removed in ("Bark", "LeafDeep", "LeafLight", "PollinatorBloom"):
            self.assertNotIn(removed, roster)

    def test_capture_wrapper_pins_successor_assets_and_map_markers(self) -> None:
        for marker in (
            "applyDpiScaling=false",
            "forbidHoles=true",
            "loadingDescendantLimit=20",
            "r24CoarseLocalLandmarkShellsSuppressed=true",
            "currentContextSuppressionContract=local_fallback_suppression_v2",
            "currentContextTriangles=43448",
            "currentContextSuppressedTriangles=96",
            "contextFacadeR25=true",
            "outerGroundLoadingFallbackTriangles=1280",
            "outerGroundLoadingFallbackSourceCorners=3840",
            "outerGroundLoadingFallbackRenderVertices=768",
            "outerGroundLoadingFallbackRenderOnly=true",
            "outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false",
            "inheritedV5CPlanningGroundHidden=true",
            "assets=16",
            "meshTriangles=15760",
            "sourceVertices=9536",
            "proceduralComponents=828",
            "materialSlots=15",
            "bakedFoliageRenderComponents=0",
            "foliageTreeAnchors=3",
        ):
            self.assertIn(f"'{marker}'", self.wrapper)
        for path in (
            "LocalFallbackSuppressionV2\\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset",
            "OuterGroundLoadingFallback\\SM_IPV5D_OuterGroundLoadingFallback_Render.uasset",
            "OuterGroundLoadingFallback\\Materials\\M_IPV5D_OuterGroundLoadingFallback.uasset",
        ):
            self.assertIn(path, self.wrapper)
        self.assertNotIn("LocalFallbackSuppressionV1", self.wrapper)
        self.assertNotIn("LocalFallbackSuppressed_v1.uasset", self.wrapper)
        self.assertNotIn("local_fallback_suppression_v1", self.wrapper)
        boundary = self.wrapper[
            self.wrapper.index("$script:boundaryPins = @(") :
            self.wrapper.index("Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'")
        ]
        manifest = self.wrapper[self.wrapper.index("$result =") :]
        for variable in (
            "$suppressedFallbackMeshPin",
            "$outerGroundMeshPin",
            "$outerGroundMaterialPin",
        ):
            self.assertIn(f"{variable} =", self.wrapper)
            self.assertIn(variable, boundary)
            self.assertIn(variable, manifest)
        for marker in (
            "StrictLocalFallbackGateRequired",
            "RequiredProviderReadyForProof",
            "RequiredLocalFallbackHidden",
            "StrictDiagnosticFallbackAcknowledgementRequired",
            "StrictDiagnosticFallbackAcknowledgementMarker",
            "ProviderReadyProofClaimed",
        ):
            self.assertIn(marker, manifest)

    def test_capture_wrapper_static_self_check_exercises_all_three_modes(self) -> None:
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        expected = {
            "TelemetryOnly": (
                "EXPLICIT_NON_PROOF_TELEMETRY_DIAGNOSTIC",
                "explore_v5d_diagnostic_",
            ),
            "ProviderFallback": (
                "STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF",
                "explore_v5d_diagnostic_",
            ),
            "ProviderReady": (
                "PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED",
                "explore_v5d_r24_temasek_shophouse_",
            ),
        }
        for mode, (classification, filename_prefix) in expected.items():
            completed = subprocess.run(
                [
                    pwsh,
                    "-NoProfile",
                    "-NonInteractive",
                    "-File",
                    str(WRAPPER),
                    "-RunToken",
                    f"contract_{mode}",
                    "-ProviderEvidenceMode",
                    mode,
                    "-StaticSelfCheck",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, completed.returncode, completed.stderr)
            report = json.loads(completed.stdout)
            self.assertEqual("STATIC_SELF_CHECK_PASS", report["Status"])
            self.assertEqual(mode, report["ProviderEvidenceMode"])
            self.assertEqual(classification, report["EvidenceClassification"])
            self.assertEqual(4, report["PoseCount"])
            self.assertEqual(
                [
                    "facade_close",
                    "front_corner_oblique",
                    "streetscape_context",
                    "tree_acceptance_19m",
                ],
                [pose["Label"] for pose in report["Poses"]],
            )
            self.assertTrue(
                all(
                    pose["OutputFileName"].startswith(filename_prefix)
                    for pose in report["Poses"]
                )
            )
            self.assertEqual(
                3, len(report["LandmarkContract"]["BoundaryAssetPaths"])
            )
            landmark = report["LandmarkContract"]
            self.assertEqual(15760, landmark["MeshTriangles"])
            self.assertEqual(15, landmark["MaterialCount"])
            self.assertEqual(16, landmark["PackageCount"])
            self.assertEqual(0, landmark["BakedFoliageRenderComponents"])
            self.assertEqual(3, landmark["FoliageTreeAnchors"])
            self.assertEqual(4, len(landmark["RemovedLegacyFoliagePackageStems"]))
            tree_pose = report["Poses"][3]
            self.assertGreaterEqual(tree_pose["TargetDistanceMeters"], 18.5)
            self.assertLessEqual(tree_pose["TargetDistanceMeters"], 19.5)
            acceptance = tree_pose["VisualAcceptance"]
            self.assertEqual("TEMASEK_LANDMARK_TREE_HANDOFF", acceptance["Subject"])
            self.assertEqual("mature_tree_02", acceptance["FoliageAnchorId"])
            self.assertEqual("DOME", acceptance["FoliageMeshRole"])
            self.assertFalse(acceptance["AutomatedPixelAcceptanceClaimed"])
            self.assertEqual(
                "SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY",
                report["ProtectedUE54Interaction"],
            )
            self.assertTrue(report["NativeUE55AndTRIADHelpersMustBeIdle"])


if __name__ == "__main__":
    unittest.main()
