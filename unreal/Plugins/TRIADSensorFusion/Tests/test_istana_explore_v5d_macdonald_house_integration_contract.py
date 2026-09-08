from __future__ import annotations

import hashlib
import json
import math
import shutil
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
R24 = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings"
    / "R24MacDonaldHouse"
)
PLACEMENT = R24 / "macdonald_house_r24.unreal_placement.json"
MANIFEST = R24 / "Generated/IstanaPublicViewV5DR24MacDonaldHouse.manifest.json"
FEATURES = R24 / "Generated/IstanaPublicViewV5DR24MacDonaldHouse.features.json"
ACTOR_H = (
    PLUGIN
    / "Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DMacDonaldHouseActor.h"
)
ACTOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DMacDonaldHouseActor.cpp"
)
PROVENANCE_H = (
    PLUGIN
    / "Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DMacDonaldHouseProvenance.h"
)
PROVENANCE_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DMacDonaldHouseProvenance.cpp"
)
FACTORY_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DMacDonaldHouseAssetFactory.cpp"
)
EDITOR_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public"
    / "TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.h"
)
EDITOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.cpp"
)
POLICY_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
HYBRID_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public"
    / "TRIADIstanaExploreV5DHybridEditorLibrary.h"
)
HYBRID_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)
NATIVE_TEST = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private/Tests"
    / "TRIADIstanaExploreV5DMacDonaldHouseRuntimeTests.cpp"
)
WRAPPER = REPO / "scripts/Capture-IstanaExploreV5DMacDonaldHouseR24Evidence.ps1"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def function_body(source: str, marker: str) -> str:
    start = source.index(marker)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function after {marker!r}")


class IstanaExploreV5DMacDonaldHouseIntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.placement = json.loads(read(PLACEMENT))
        cls.manifest = json.loads(read(MANIFEST))
        cls.features = json.loads(read(FEATURES))
        cls.actor_h = read(ACTOR_H)
        cls.actor_cpp = read(ACTOR_CPP)
        cls.provenance = read(PROVENANCE_H) + read(PROVENANCE_CPP)
        cls.factory_cpp = read(FACTORY_CPP)
        cls.editor_cpp = read(EDITOR_CPP)
        cls.editor = read(EDITOR_H) + cls.editor_cpp
        cls.policy_cpp = read(POLICY_CPP)
        cls.hybrid_h = read(HYBRID_H)
        cls.hybrid_cpp = read(HYBRID_CPP)
        cls.native_test = read(NATIVE_TEST)
        cls.wrapper = read(WRAPPER)

    def test_capture_process_pins_provider_safe_http_concurrency(self) -> None:
        self.assertIn("HttpMaxConnectionsPerServer=12", self.wrapper)
        self.assertNotIn("HttpMaxConnectionsPerServer=16", self.wrapper)
        self.assertIn("MaxCacheItems=32768", self.wrapper)

    def test_all_source_admission_receipts_are_exact(self) -> None:
        expected = {
            "Generated/SM_IPV5D_R24_MacDonaldHouse_Render.obj": (
                293759,
                "BE049C7E1A8AB2D9DED98C027478EA5A5183F0C2CF2FFC206289D1FF47F955DE",
            ),
            "Generated/SM_IPV5D_R24_MacDonaldHouse_Render.mtl": (
                1487,
                "E35EE31D1B2ABB047FE6CA134510D32EDFC1E13FC98A0C338EACB5AEB7DE1784",
            ),
            "Generated/IstanaPublicViewV5DR24MacDonaldHouse.geometry.json": (
                1078589,
                "9F629115A98BE5A257D06ACD864CE40170C8EFCE57BF60D7D145B85D7682C69C",
            ),
            "Generated/IstanaPublicViewV5DR24MacDonaldHouse.features.json": (
                3800,
                "26EB68961BB66BFCC25B39AF86FFFFE5DCE9491F14AF5454316D88D5A7D24C78",
            ),
            "Generated/IstanaPublicViewV5DR24MacDonaldHouse.manifest.json": (
                4656,
                "E1949EFEBD51A18D9919BB4174A0C95038D7028ADD13D7BE46070E46C773F8C2",
            ),
            "macdonald_house_r24.contract.json": (
                6608,
                "30FDEDD2E7EBBB4E4E4133D0E2D383399FA5F17AA264B73AD422094F2D00DC77",
            ),
            "Sources/public_sources.json": (
                2728,
                "5292CAA6133BC0B2F69CCB52377DA62D778D4405F3213D585E6908184D24A570",
            ),
            "macdonald_house_r24.unreal_placement.json": (
                4192,
                "E5B40D02A3D712303A2D99734A42E9ADD68F4E528DED31DD763F37034961ED81",
            ),
        }
        for relative, (size, digest) in expected.items():
            path = R24 / relative
            self.assertEqual(size, path.stat().st_size, relative)
            self.assertEqual(digest, sha256(path), relative)
            self.assertIn(relative.replace("/", "/"), self.factory_cpp)
            self.assertIn(digest, self.factory_cpp)

    def test_source_package_stays_source_only_and_native_receipt_is_separate(self) -> None:
        self.assertFalse(self.manifest["authorityPolicy"]["liveUnrealIntegrated"])
        self.assertEqual(
            "LIVE_RUNTIME_PERSISTENT_RENDER_OVERLAY_PLACEMENT_CONTRACT",
            self.placement["status"],
        )
        self.assertEqual(576, self.manifest["counts"]["components"])
        self.assertEqual(4692, self.manifest["counts"]["vertices"])
        self.assertEqual(7080, self.manifest["counts"]["triangles"])
        self.assertEqual(9, self.manifest["counts"]["materials"])

    def test_volunteered_osm_receipt_and_nonuniform_fit_are_mathematically_bound(self) -> None:
        receipt = self.placement["sourceFeatureReceipt"]
        self.assertEqual("OSM:way:46521250", receipt["sourceKey"])
        self.assertEqual(
            "755536CDE8E666826FA6A325BCD707BDC7E0132C018BC87F0ABE8275F4557A39",
            receipt["featureGeometrySha256"],
        )
        self.assertEqual(
            "B7204683A26E7DA56E1A97E516E36220C1764341CA485B904D736F509B481A34",
            receipt["partGeometrySha256"],
        )
        points = receipt["footprintLocalMeters"]
        self.assertAlmostEqual(
            receipt["footprintShortEdgeMeters"],
            math.dist(points[0], points[1]),
            6,
        )
        self.assertAlmostEqual(
            receipt["footprintLongEdgeMeters"],
            math.dist(points[3], points[0]),
            6,
        )

        transform = self.placement["unrealTransform"]
        self.assertEqual(
            [36320.390052826, 87155.365772797, 0.0],
            transform["translationCentimeters"],
        )
        self.assertEqual(
            [0.0, -161.885822122792, 0.0],
            transform["rotationDegrees"],
        )
        self.assertEqual(
            [1.4799104705961, 1.98130612769146, 1.3],
            transform["scale3D"],
        )
        self.assertIn("NON_UNIFORM_SCALE_IS_A_VISUAL_FIT_ASSUMPTION", transform["limitations"])
        self.assertIn("NO_ONE_TO_ONE_DIMENSION_OR_FACADE_ALIGNMENT_CLAIM", transform["limitations"])

        # The actor receipt uses the building-mass centre, not the total-detail
        # bounds centre. After yaw and non-uniform scale it lands on the OSM
        # quadrilateral centroid to millimetric receipt precision.
        centre = [sum(p[axis] for p in points) / 4.0 for axis in (0, 1)]
        actor_xy_m = [value / 100.0 for value in transform["translationCentimeters"][:2]]
        local_mass_y = (-7.42 + 8.0) / 2.0
        scaled_y = local_mass_y * transform["scale3D"][1]
        yaw = math.radians(transform["rotationDegrees"][1])
        fitted_centre = [
            actor_xy_m[0] - math.sin(yaw) * scaled_y,
            actor_xy_m[1] + math.cos(yaw) * scaled_y,
        ]
        self.assertAlmostEqual(centre[0], fitted_centre[0], 4)
        self.assertAlmostEqual(centre[1], fitted_centre[1], 4)

    def test_public_exterior_and_negative_authority_remain_explicit(self) -> None:
        feature = self.features["features"][0]
        self.assertEqual(
            "PUBLICLY_VISIBLE_STREET_FACADE_HERITAGE_MOTIFS_AND_SIMPLIFIED_ENVELOPE_ONLY",
            feature["modeledScope"],
        )
        for forbidden_scope in (
            "INTERIORS",
            "SECURITY_SYSTEMS_OR_LAYOUT",
            "MEASURED_OR_AS_BUILT_DIMENSIONS",
            "SURVEY_GRADE_GEOREGISTRATION",
        ):
            self.assertIn(forbidden_scope, feature["excludedScope"])
        authority = self.placement["runtimeAuthorityPolicy"]
        self.assertTrue(authority["renderOnly"])
        for field in (
            "collisionAuthority",
            "navigationAuthority",
            "sensorOcclusionAuthority",
            "rfGeometryAuthority",
            "rfMaterialAuthority",
            "surveyAuthority",
            "asBuiltAuthority",
            "interiorAuthority",
            "securityDetailIncluded",
            "proceduralPbrMaterialsCalibrated",
        ):
            self.assertFalse(authority[field], field)

    def test_editor_factory_creates_bounded_texture_free_pbr_not_flat_mtl(self) -> None:
        for marker in (
            "BrickBaseCode",
            "BrickRoughnessCode",
            "BrickNormalCode",
            "PaintBaseCode",
            "GlassRoughnessCode",
            "RoofBaseCode",
            "MarbleBaseCode",
            "ConcreteBaseCode",
            "MetalBaseCode",
            "PlaqueBaseCode",
            "TextureSamples != 0",
            "Material->BlendMode = BLEND_Opaque",
            "Material->bTangentSpaceNormal = false",
            "Material->bUsedWithNanite = true",
            "MaxWorldPositionOffsetDisplacement = 0.0f",
            "Options->bImportMaterials = false",
            "Options->bImportTextures = false",
            "ExpectedTriangleCount = 7080",
            "ExpectedMaterialCount = 9",
            "FallbackPercentTriangles = 1.0f",
            "Mesh->bAllowCPUAccess = true",
        ):
            self.assertIn(marker, self.factory_cpp)
        self.assertNotIn("AssetImportTask* Texture", self.factory_cpp)

    def test_runtime_actor_uses_only_cooked_asset_and_has_no_truth_authority(self) -> None:
        combined = self.actor_h + self.actor_cpp + self.provenance
        for marker in (
            "SM_IPV5D_R24_MacDonaldHouse_Render",
            "FVector(36320.390052826, 87155.365772797, 0.0)",
            "FRotator(0.0, -161.885822122792, 0.0)",
            "FVector(1.4799104705961, 1.98130612769146, 1.3)",
            "SetActorEnableCollision(false)",
            "ECollisionEnabled::NoCollision",
            "SetCanEverAffectNavigation(false)",
            "bCollisionNavigationSensorOrRfAuthority = false",
            "bSurveyAsBuiltOrOneToOne = false",
            "bProviderReadyLiveSuccessor = false",
            "ValidateRuntimeMesh",
            "ComputeCookedRenderPayloadSha256",
        ):
            self.assertIn(marker, combined)
        for forbidden in (
            "FFileHelper",
            "SourceAssets/",
            "Generated/SM_IPV5D_R24",
            "CreatePhysicsState",
            "SetCollisionProfileName",
        ):
            self.assertNotIn(forbidden, self.actor_cpp)

    def test_provider_state_is_telemetry_only_and_does_not_expand_cesium_clip(self) -> None:
        observer = function_body(
            self.actor_cpp,
            "bool ATRIADIstanaExploreV5DMacDonaldHouseActor::\n"
            "    SynchronizeWithContextPolicy(",
        )
        for marker in (
            "TActorIterator<ATRIADIstanaExploreV5DContextPolicyActor>",
            "TActorIterator<ATRIADIstanaExploreV5DMacDonaldHouseActor>",
            "Policy->HasActorBegunPlay()",
            "Policy->ValidateHybridContext(PolicyReport)",
            "AddTickPrerequisiteActor(Policy)",
            "Policy->bLocalBuildingFallbackCurrentlyHidden",
            "RecordProviderReadyState(bReady",
            "LandmarkCount == 1",
            "bFullAuditDue = bRuntimeAssetContractFailClosed ||",
            "NowSeconds + 0.5",
            "fullContractAuditHz=2",
        ):
            self.assertIn(marker, observer)
        self.assertNotIn("SetProviderReady", self.actor_h + self.actor_cpp)
        self.assertNotIn("UCesiumPolygonRasterOverlay", observer)
        record = function_body(
            self.actor_cpp,
            "bool ATRIADIstanaExploreV5DMacDonaldHouseActor::\n"
            "    RecordProviderReadyState(",
        )
        self.assertIn("SetVisibility(true, true)", record)
        self.assertIn("SetHiddenInGame(false, true)", record)
        self.assertNotIn("!bInProviderReady", record)
        self.assertIn("PrimaryActorTick.TickInterval = 0.0f", self.actor_cpp)
        self.assertIn("RemoveTickPrerequisiteActor(Policy)", self.actor_cpp)
        self.assertNotIn("MacDonaldHouse", self.policy_cpp)
        composition = self.placement["compositionPolicy"]
        self.assertFalse(composition["insideExistingAuthoredCoreProviderClip"])
        self.assertFalse(composition["dedicatedProviderExclusionPolygonPresent"])
        self.assertFalse(composition["providerContextSilentlyHidden"])
        self.assertTrue(composition["localCoarseSurroundingsShellRetained"])
        self.assertTrue(composition["providerStateTelemetryOnly"])
        self.assertTrue(composition["visibilityInvariantAcrossProviderTransitions"])
        self.assertFalse(composition["providerReadyLiveSuccessorClaimed"])
        for marker in (
            "Provider false telemetry records",
            "Overlay remains visible at provider true",
            "Overlay remains visible after false-true-false",
        ):
            self.assertIn(marker, self.native_test)

    def test_hybrid_builder_and_validator_own_one_distinct_landmark(self) -> None:
        for marker in (
            '#include "TRIADIstanaExploreV5DMacDonaldHouseActor.h"',
            "LoadValidatedRuntimeMesh",
            "ExistingMacDonaldHouseCount != 0",
            "SpawnActor<ATRIADIstanaExploreV5DMacDonaldHouseActor>",
            "ExpectedPlacementTransform()",
            "ConfigureMacDonaldHouse(",
            "MacDonaldHouse->bProviderReady !=",
            "r24MacDonaldHouseCount=%d",
            "r24MacDonaldDedicatedOverlayVisible=%s",
            "r24VisibilityInvariantAcrossProviderTransitions=true",
            "r24ProviderStateTelemetryOnly=true",
            "r24CoarseLocalLandmarkShellsSuppressed=true",
            "r24SuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490",
            "outerGroundLoadingFallbackProviderCoupled=true",
            "r24DedicatedProviderExclusion=false",
            "r24ProviderReadyLiveSuccessor=false",
        ):
            self.assertIn(marker, self.hybrid_cpp)

    def test_map_migration_is_clean_semantic_backup_save_reload_transaction(self) -> None:
        migration = function_body(
            self.hybrid_cpp,
            "ApplyIstanaExploreV5DMacDonaldHouseR24ToLoadedHybridMap(",
        )
        for marker in (
            "ExistingMacDonaldHouseCount != 0",
            "MapPackage->IsDirty()",
            "ELandmarkPresencePolicy::Forbidden",
            "HashFileSha256",
            "CreateVerifiedPreMacDonaldHouseR24MapBackup",
            "RestoreVerifiedPredecessor",
            "RestoreVerifiedPreMacDonaldHouseR24MapViaSiblingTemp",
            "ELandmarkPresencePolicy::Required",
            "UEditorLoadingAndSavingUtils::SaveMap",
            "LoadMap(SourceFilename)",
            "LoadMap(DestinationFilename)",
            "ELandmarkPresencePolicy::Forbidden",
            "existingProviderClipUnchanged=true",
        ):
            self.assertIn(marker, migration)
        restore = function_body(
            self.hybrid_cpp,
            "RestoreVerifiedPreMacDonaldHouseR24MapViaSiblingTemp(",
        )
        self.assertIn("FPaths::CreateTempFilename", restore)
        self.assertIn("IFileManager::Get().Move", restore)
        self.assertIn("bBackupPreserved", restore)
        self.assertNotIn("*DestinationFilename,\n            *BackupFilename", restore)
        self.assertIn(
            "V5DMacDonaldHouseR24_20260903", self.hybrid_cpp
        )
        self.assertIn(
            "ApplyIstanaExploreV5DMacDonaldHouseR24ToLoadedHybridMap",
            self.hybrid_h,
        )

    def test_one_command_editor_entrypoint_and_native_filter_are_exposed(self) -> None:
        for marker in (
            "ImportApplyAndValidateIstanaExploreV5DMacDonaldHouseR24",
            "ImportIstanaExploreV5DMacDonaldHouseR24Assets",
            "BuildIstanaExploreV5DHybridMap",
            "ApplyIstanaExploreV5DMacDonaldHouseR24ToLoadedHybridMap",
            "ValidateIstanaExploreV5DMacDonaldHouseR24SuccessorMap",
        ):
            self.assertIn(marker, self.editor)
        for test_name in (
            "TRIAD.Istana.ExploreV5D.MacDonaldHouse.RuntimeContract",
            "TRIAD.Istana.ExploreV5D.MacDonaldHouse.AssetContract",
            "TRIAD.Istana.ExploreV5D.MacDonaldHouse.EndToEndContract",
        ):
            self.assertIn(test_name, self.native_test + self.editor)

    def test_fresh_asset_import_is_exact_empty_predecessor_transaction(self) -> None:
        create = function_body(self.factory_cpp, "bool CreateFreshAssets(")
        import_assets = function_body(
            self.editor_cpp,
            "bool UTRIADIstanaExploreV5DMacDonaldHouseEditorLibrary::\n"
            "    ImportIstanaExploreV5DMacDonaldHouseR24Assets(",
        )
        rollback = function_body(
            self.factory_cpp,
            "RollbackActiveFreshAssetTransactionInternal(",
        )
        artifact_guard = function_body(
            self.factory_cpp,
            "GetExactOwnedPackageArtifactPaths(",
        )
        for marker in (
            "ValidateExactEmptyAssetRoot",
            "bFreshAssetTransactionActive = true",
            "FreshAssetTransactionOwnedObjectPaths",
            "for (const FString& ReservedObjectPath : ExpectedObjectPaths())",
            "bFreshAssetTransactionReleasedToCaller = true",
        ):
            self.assertIn(marker, create)
        for marker in (
            "FMacDonaldFreshAssetTransactionGuard TransactionGuard",
            "SaveLoadedAssets",
            "ReloadPackages",
            "ValidateAssets",
            "CommitActiveFreshAssetTransaction",
            "TransactionGuard.Release()",
        ):
            self.assertIn(marker, import_assets)
        self.assertLess(
            import_assets.index("FMacDonaldFreshAssetTransactionGuard TransactionGuard"),
            import_assets.index("SaveLoadedAssets"),
        )
        for marker in (
            "DeleteObjectsUnchecked",
            "UPackageTools::UnloadPackages",
            "FUnloadPackageParams",
            "bResetTransBuffer = true",
            "GetExactOwnedPackageArtifactPaths",
            "IFileManager::Get().Delete",
            "ScanPathsSynchronous",
            "FindPackage(nullptr, *PackageName)",
            "ValidateExactEmptyAssetRoot",
            "rootEmpty=%s retryable=%s",
            "bRollbackComplete",
            "ownershipSubsetOfExactRoster=true",
            "exactResidualPackageArtifactsDeleted=%d",
        ):
            self.assertIn(marker, rollback)
        empty_root = function_body(
            self.factory_cpp,
            "ValidateExactEmptyAssetRoot(",
        )
        self.assertIn("FindFilesRecursive", empty_root)
        self.assertIn("registry=[%s] packageFiles=[%s]", empty_root)
        # A save/reload failure can leave an unloadable package artifact that
        # LoadObject cannot return.  Rollback therefore has a second, tightly
        # allowlisted disk-cleanup path: exact owned package names, exact root,
        # and only Unreal's known package sidecars.
        for marker in (
            "ExpectedPackageNames()",
            "ExactPackages.Contains(PackageName)",
            "FPaths::IsUnderDirectory(PackageStem, AssetDiskRoot)",
            'TEXT(".uasset")',
            'TEXT(".uexp")',
            'TEXT(".ubulk")',
            'TEXT(".uptnl")',
            "artifact containment proof failed",
        ):
            self.assertIn(marker, artifact_guard)

    def test_project_relative_source_and_cooked_payload_digest_are_recomputed(self) -> None:
        self.assertIn("CanonicalProjectRelativeSourcePath", self.factory_cpp)
        self.assertIn("FPaths::MakePathRelativeTo", self.factory_cpp)
        self.assertIn(
            "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/",
            self.provenance,
        )
        self.assertNotIn("FPaths::IsSamePath(ActualSource", self.factory_cpp)
        for marker in (
            "CookedRenderPayloadSha256",
            "cooked_render_payload.v1",
            "PositionVertexBuffer",
            "GetArrayView()",
            "Section.MaterialIndex",
            "Material.MaterialSlotName",
            "Material.MaterialInterface->GetPathName()",
            "RecomputedPayloadSha256 != Provenance->CookedRenderPayloadSha256",
            "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855",
        ):
            self.assertIn(marker, self.provenance + self.actor_cpp + self.factory_cpp)

    def test_imported_slot_metadata_is_editor_only_and_cook_digest_stable(self) -> None:
        runtime_validation = function_body(self.actor_cpp, "bool ValidateMesh(")
        payload = function_body(
            read(PROVENANCE_CPP), "ComputeCookedRenderPayloadSha256("
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
            self.factory_cpp,
        )

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
                "explore_v5d_r24_macdonald_",
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
            self.assertEqual(3, report["PoseCount"])
            self.assertTrue(
                all(
                    pose["OutputFileName"].startswith(filename_prefix)
                    for pose in report["Poses"]
                )
            )
            self.assertEqual(
                3, len(report["RuntimeContract"]["BoundaryAssetPaths"])
            )
            self.assertEqual(
                "SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY",
                report["ProtectedUE54Interaction"],
            )
            self.assertTrue(report["NativeUE55AndTRIADHelpersMustBeIdle"])


if __name__ == "__main__":
    unittest.main()
