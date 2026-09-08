"""Source contracts for the corrected destination-only Istana PIE map builder."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


PLUGIN = Path(__file__).resolve().parents[1]
RUNTIME = PLUGIN / "Source" / "TRIADSensorFusion"
EDITOR = PLUGIN / "Source" / "TRIADSensorFusionEditor"
EDITOR_H = (EDITOR / "Public" / "TRIADIstanaEditorLibrary.h").read_text(
    encoding="utf-8"
)
EDITOR_CPP = (EDITOR / "Private" / "TRIADIstanaEditorLibrary.cpp").read_text(
    encoding="utf-8"
)
POLICY_H = (RUNTIME / "Public" / "TRIADIstanaRuntimePolicyActor.h").read_text(
    encoding="utf-8"
)
POLICY_CPP = (RUNTIME / "Private" / "TRIADIstanaRuntimePolicyActor.cpp").read_text(
    encoding="utf-8"
)
ISTANA_GAMEMODE_H = (
    RUNTIME / "Private" / "TRIADIstanaAirSimGameMode.h"
).read_text(encoding="utf-8")
ISTANA_GAMEMODE_CPP = (
    RUNTIME / "Private" / "TRIADIstanaAirSimGameMode.cpp"
).read_text(encoding="utf-8")
EXTERIOR_H = (RUNTIME / "Public" / "TRIADIstanaExteriorMeshActor.h").read_text(
    encoding="utf-8"
)
EXTERIOR_CPP = (
    RUNTIME / "Private" / "TRIADIstanaExteriorMeshActor.cpp"
).read_text(encoding="utf-8")
MANAGER_CPP = (
    RUNTIME / "Private" / "TRIADSensorFusionScenarioManager.cpp"
).read_text(encoding="utf-8")
EDITOR_BUILD = (EDITOR / "TRIADSensorFusionEditor.Build.cs").read_text(
    encoding="utf-8"
)
SCRIPTS = PLUGIN.parents[2] / "scripts"
VISUAL_SETTINGS_PATH = (
    PLUGIN.parents[1] / "Config" / "IstanaVisualAcceptance.settings.json"
)
REFINED_MANIFEST_PATH = (
    PLUGIN.parents[1]
    / "SourceAssets"
    / "Istana"
    / "Generated"
    / "IstanaExterior.manifest.json"
)


class IstanaRuntimeMapV2ContractTests(unittest.TestCase):
    def test_remote_callable_v2_builder_and_validator_are_declared_and_defined(self) -> None:
        for name in (
            "BuildIstanaRuntimeMapV2",
            "ValidateIstanaRuntimeMapV2",
            "ValidateIstanaRuntimeMapV2Readiness",
            "ValidateIstanaPlayWorldReadiness",
            "ValidateIstanaVisualAcceptancePlayWorldReadiness",
            "QuiesceIstanaPlayWorldForStop",
            "PrepareIstanaRuntimeMapV2Streaming",
        ):
            signature = re.compile(rf"bool\s+.*{name}\(FString&\s+Out\w+\)")
            self.assertRegex(EDITOR_H, signature)
            self.assertRegex(EDITOR_CPP, signature)
        repair_signature = re.compile(
            r"bool\s+.*RepairIstanaRuntimeMapV2GameMode\(\s*"
            r"FString&\s+OutBackupFile,\s*FString&\s+OutMessage\)",
            re.S,
        )
        self.assertRegex(EDITOR_H, repair_signature)
        self.assertRegex(EDITOR_CPP, repair_signature)
        camera_repair_signature = re.compile(
            r"bool\s+.*RepairIstanaRuntimeMapV2Camera\(\s*"
            r"FString&\s+OutBackupFile,\s*FString&\s+OutMessage\)",
            re.S,
        )
        self.assertRegex(EDITOR_H, camera_repair_signature)
        self.assertRegex(EDITOR_CPP, camera_repair_signature)
        exterior_repair_signature = re.compile(
            r"bool\s+.*RepairIstanaRuntimeMapV2ExteriorAsset\(\s*"
            r"FString&\s+OutBackupFile,\s*FString&\s+OutMessage\)",
            re.S,
        )
        self.assertRegex(EDITOR_H, exterior_repair_signature)
        self.assertRegex(EDITOR_CPP, exterior_repair_signature)
        refined_import_signature = re.compile(
            r"bool\s+.*ImportIstanaExteriorRefinedLod0\(\s*"
            r"FString&\s+OutMessage\)",
            re.S,
        )
        self.assertRegex(EDITOR_H, refined_import_signature)
        self.assertRegex(EDITOR_CPP, refined_import_signature)

    def test_triad_owned_airsim_wrapper_is_behavior_preserving(self) -> None:
        self.assertIn('#include "AirSimGameMode.h"', ISTANA_GAMEMODE_H)
        self.assertRegex(
            ISTANA_GAMEMODE_H,
            r"class\s+TRIADSENSORFUSION_API\s+ATRIADIstanaAirSimGameMode\s*"
            r":\s*public\s+AAirSimGameMode",
        )
        self.assertIn("GENERATED_BODY()", ISTANA_GAMEMODE_H)
        self.assertIn(": Super(ObjectInitializer)", ISTANA_GAMEMODE_CPP)
        self.assertIn('"AirSimTriadRuntime"', (
            RUNTIME / "TRIADSensorFusion.Build.cs"
        ).read_text(encoding="utf-8"))

    def test_gamemode_wrapper_override_is_editor_notified_and_reload_validated(self) -> None:
        setter = EDITOR_CPP[
            EDITOR_CPP.index("SetPersistentAirSimGameModeOverride(") :
            EDITOR_CPP.index("ConfigureAirSimRuntimeStart(")
        ]
        for contract in (
            "FindFProperty<FClassProperty>",
            "GET_MEMBER_NAME_CHECKED(AWorldSettings, DefaultGameMode)",
            "WorldSettings->Modify()",
            "WorldSettings->PreEditChange(DefaultGameModeProperty)",
            "DefaultGameModeProperty->SetObjectPropertyValue_InContainer",
            "EPropertyChangeType::ValueSet",
            "WorldSettings->PostEditChangeProperty(ChangedEvent)",
            "WorldSettings->MarkPackageDirty()",
            "World->PersistentLevel->MarkPackageDirty()",
            "World->MarkPackageDirty()",
        ):
            self.assertIn(contract, setter)

        build = EDITOR_CPP[
            EDITOR_CPP.index("BuildIstanaRuntimeMapV2(") :
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2GameMode(")
        ]
        self.assertIn("SetPersistentAirSimGameModeOverride", EDITOR_CPP)
        self.assertIn("ReloadAndValidatePersistedRuntimeMapV2", build)
        self.assertIn("UEditorLoadingAndSavingUtils::LoadMap", EDITOR_CPP)
        self.assertIn("TRIAD AirSim wrapper loadable", EDITOR_CPP)
        self.assertIn(
            "/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode",
            EDITOR_CPP,
        )
        self.assertIn("IsChildOf(ExternalAirSimGameModeClass)", EDITOR_CPP)
        self.assertIn("World->GetOutermost()->IsDirty()", build)

    def test_exact_v2_gamemode_repair_is_backed_up_and_narrow(self) -> None:
        repair = EDITOR_CPP[
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2GameMode(") :
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2Camera(")
        ]
        for contract in (
            "IsEditorOperationSafe",
            "World->GetOutermost()->GetName() != DestinationMapV2Package",
            "ValidateRuntimeWorldV2(",
            "NonGameModeValidation",
            "false",
            "SavedDir",
            "IstanaMapBackups",
            "IFileManager::Get().Copy",
            "SetPersistentAirSimGameModeOverride",
            "SaveMap(World, DestinationMapV2Package)",
            "ReloadAndValidatePersistedRuntimeMapV2",
            "RestoreUnsavedGameModeEdit",
            "DestinationPackage->SetDirtyFlag(false)",
            "World->GetOutermost()->IsDirty()",
        ):
            self.assertIn(contract, repair)
        for forbidden in (
            "SaveMap(World, SourceMapPackage)",
            "SaveMap(World, DestinationMapPackage)",
            "Delete(",
            "NewMapFromTemplate",
        ):
            self.assertNotIn(forbidden, repair)
        operation_safety = EDITOR_CPP[
            EDITOR_CPP.index("bool IsEditorOperationSafe(") :
            EDITOR_CPP.index("template <typename TActor>")
        ]
        self.assertIn("GEditor->IsPlaySessionInProgress()", operation_safety)

    def test_exact_v2_camera_migration_is_backed_up_narrow_and_reload_proven(self) -> None:
        repair = EDITOR_CPP[
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2Camera(") :
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2ExteriorAsset(")
        ]
        for contract in (
            "IsEditorOperationSafe",
            "World->GetOutermost()->GetName() != DestinationMapV2Package",
            "NonMigrationValidation",
            "true,\n            false,\n            false",
            "RuntimePolicy->ExteriorGroundHeightMeters",
            "RuntimeCameras.Num() == 1",
            "ExteriorActors.Num() == 1",
            "FindComponentByClass<UCesiumGlobeAnchorComponent>",
            "RuntimeCamera->GetCameraComponent()",
            "RuntimeCamera->GetPackage() != DestinationPackage",
            "CameraAnchor->GetPackage() != DestinationPackage",
            "CameraComponent->GetPackage() != DestinationPackage",
            "Exterior->GetPackage() != DestinationPackage",
            "ExteriorAnchor->GetPackage() != DestinationPackage",
            "ExteriorMeshComponent->GetPackage() != DestinationPackage",
            "RuntimeCameraSouthOffsetMeters",
            "RuntimeCameraHeightAboveGroundMeters",
            "RuntimeCameraAimAboveGroundMeters",
            "RuntimeCameraFieldOfViewDegrees",
            "ImportedExteriorYawDegrees",
            "IsImportedExteriorCeremonialFrontFacingSouth",
            "pre_camera_repair",
            "IstanaMapBackups",
            "IFileManager::Get().Copy",
            "RestoreUnsavedCameraAndOrientationEdit",
            "DestinationPackage->Modify()",
            "RuntimeCamera->Modify()",
            "CameraAnchor->Modify()",
            "CameraComponent->Modify()",
            "Exterior->Modify()",
            "ExteriorAnchor->Modify()",
            "SetCameraAutoActivateForPlayer",
            "MoveToLongitudeLatitudeHeight(TargetLongitudeLatitudeHeight)",
            "SetFieldOfView(RuntimeCameraFieldOfViewDegrees)",
            "Exterior->ConfigureGeodeticPlacement",
            "SaveMap(World, DestinationMapV2Package)",
            "ReloadAndValidatePersistedRuntimeMapV2",
            "World->GetOutermost()->IsDirty()",
        ):
            self.assertIn(contract, repair)
        self.assertLess(
            repair.index("NonMigrationValidation"),
            repair.index("bCameraAlreadyCurrent"),
        )
        for forbidden in (
            "SaveMap(World, SourceMapPackage)",
            "SaveMap(World, DestinationMapPackage)",
            "Delete(",
            "DestroyActor",
            "NewMapFromTemplate",
        ):
            self.assertNotIn(forbidden, repair)

    def test_exact_v2_refined_exterior_migration_is_backed_up_narrow_and_reload_proven(self) -> None:
        repair = EDITOR_CPP[
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2ExteriorAsset(") :
            EDITOR_CPP.index("ValidateIstanaRuntimeMapV2(")
        ]
        for contract in (
            "IsEditorOperationSafe",
            "World->GetOutermost()->GetName() != DestinationMapV2Package",
            "NonExteriorAssetValidation",
            "true,\n            true,\n            true,\n            false",
            "ExteriorActors.Num() == 1",
            "Exterior->GetLevel() != World->PersistentLevel",
            "Exterior->GetPackage() != DestinationPackage",
            "ExteriorAnchor->GetPackage() != DestinationPackage",
            "ExteriorMeshComponent->GetPackage() != DestinationPackage",
            "LoadObject<UStaticMesh>",
            "IstanaExteriorMeshObjectPath",
            "ValidateIstanaExteriorMeshAsset",
            "LegacyIstanaExteriorMeshObjectPath",
            "PreviousRequiredAsset",
            "PreviousRequiredMeshLoaded",
            "PreviousRelativeTransform",
            "PreviousCollisionEnabled",
            "PreviousCollisionProfile",
            "PreviousCollisionObjectType",
            "PreviousCollisionResponses",
            "PreviousGenerateOverlapEvents",
            "PreviousVisible",
            "PreviousHiddenInGame",
            "PreviousActorTransform",
            "PreviousAnchorLongitudeLatitudeHeight",
            "PreviousAnchorEastSouthUpRotation",
            "PreviousYawDegrees",
            "ECollisionEnabled::QueryAndPhysics",
            "pre_refined_exterior_repair",
            "IstanaMapBackups",
            "IFileManager::Get().Copy",
            "RestoreUnsavedExteriorAssetEdit",
            "Exterior->SetExteriorMesh(RefinedMesh, AssignmentError)",
            "GetCollisionResponseToChannels()",
            "GetGenerateOverlapEvents()",
            "GetActorTransform().Equals(PreviousActorTransform",
            "GetEastSouthUpRotation().Equals",
            "FMath::IsNearlyEqual(Exterior->FootprintYawDegrees, PreviousYawDegrees",
            "ValidateRuntimeWorldV2(World, true, PreSaveValidation)",
            "SaveMap(World, DestinationMapV2Package)",
            "ReloadAndValidatePersistedRuntimeMapV2",
            "AUTHORED_REFINED_PRIMARY",
            "182.3-degree ceremonial-front heading",
            "left legacy SM_IstanaExterior, SDTH, and v1 untouched",
        ):
            self.assertIn(contract, repair)

        self.assertLess(
            repair.index("IFileManager::Get().Copy"),
            repair.index("DestinationPackage->Modify()"),
        )
        self.assertLess(
            repair.index("ValidateRuntimeWorldV2(World, true, PreSaveValidation)"),
            repair.index("SaveMap(World, DestinationMapV2Package)"),
        )
        for forbidden in (
            "SaveMap(World, SourceMapPackage)",
            "SaveMap(World, DestinationMapPackage)",
            "SaveLoadedAssets",
            "ConfigureGeodeticPlacement",
            "MoveToLongitudeLatitudeHeight",
            "SetEastSouthUpRotation",
            "Delete(",
            "DestroyActor",
            "NewMapFromTemplate",
        ):
            self.assertNotIn(forbidden, repair)

    def test_refined_exterior_migration_accepts_only_the_observed_class_default_transition(self) -> None:
        validator = EDITOR_CPP[
            EDITOR_CPP.index("bool ValidateRuntimeWorldV2(") :
            EDITOR_CPP.index("bool ValidateRuntimeWorldV2Readiness(")
        ]
        for contract in (
            "const bool bExactSupportedPair =",
            "const bool bLegacyHardWithRefinedSoftDefault =",
            "ExistingObjectPath == LegacyIstanaExteriorMeshObjectPath",
            "ExistingSoftObjectPath == IstanaExteriorMeshObjectPath",
            "!bExactSupportedPair &&\n                !bLegacyHardWithRefinedSoftDefault",
            "only an exact supported pair or the legacy-hard/refined-soft class-default transition is allowed",
            "normal validation still requires refined/refined",
            "bRequireRefinedExteriorAsset &&",
            "hard and soft asset references must both match the exact refined mesh",
        ):
            self.assertIn(contract, validator)

        repair = EDITOR_CPP[
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2ExteriorAsset(") :
            EDITOR_CPP.index("ValidateIstanaRuntimeMapV2(")
        ]
        for contract in (
            "PreviousHardObjectPath == LegacyIstanaExteriorMeshObjectPath",
            "PreviousSoftObjectPath == IstanaExteriorMeshObjectPath",
            "(!bExactSupportedPair && !bLegacyHardWithRefinedSoftDefault)",
        ):
            self.assertIn(contract, repair)
        self.assertNotIn(
            "PreviousHardObjectPath == IstanaExteriorMeshObjectPath &&\n"
            "        PreviousSoftObjectPath == LegacyIstanaExteriorMeshObjectPath",
            repair,
        )

    def test_remote_control_project_identity_and_context_readiness_fail_closed(self) -> None:
        identity = re.compile(
            r"ValidateIstanaRemoteControlProject\(\s*"
            r"const FString& ExpectedProjectPath,\s*FString& OutReport\)",
            re.S,
        )
        self.assertRegex(EDITOR_H, identity)
        self.assertRegex(EDITOR_CPP, identity)
        self.assertIn("FPaths::IsSamePath(ExpectedDirectory, ActualDirectory)", EDITOR_CPP)

        readiness = EDITOR_CPP[
            EDITOR_CPP.index("ValidateRuntimeWorldV2Readiness(") :
            EDITOR_CPP.index("struct FSurveyPoint")
        ]
        self.assertIn("ValidateRuntimeWorldV2", readiness)
        self.assertIn("LoadProgress < MinimumCalibrationTilesetLoadProgress", readiness)
        self.assertIn("!Tileset->GetCreatePhysicsMeshes()", readiness)
        self.assertIn("readiness FAILED", readiness)
        self.assertIn("readiness PASSED", readiness)

    def test_playworld_readiness_is_runtime_origin_safe_and_fail_closed(self) -> None:
        helper = EDITOR_CPP[
            EDITOR_CPP.index("bool ValidatePlayWorldV2Readiness(") :
            EDITOR_CPP.index("bool ReloadAndValidatePersistedRuntimeMapV2(")
        ]
        for contract in (
            "PlayWorld->WorldType != EWorldType::PIE",
            "PlayWorld->IsGameWorld()",
            "PlayWorld->HasBegunPlay()",
            "UWorld::RemovePIEPrefix",
            "PlayWorld->GetAuthGameMode()",
            "ActiveGameMode->GetClass() != ExpectedGameModeClass",
            "IsChildOf(ExternalAirSimGameModeClass)",
            "RuntimePolicy->HasActorBegunPlay()",
            "IsVisualWeatherSuppressionActive()",
            "IsExponentialHeightFogSuppressionActive",
            "bExponentialHeightFogSuppressionVerifiedAtRuntime",
            "ExponentialHeightFogComponentCountAtRuntime",
            "bGameModeOverrideVerifiedAtRuntime",
            "bRuntimeCameraEnforcementComplete",
            "UGameplayStatics::GetPlayerController(PlayWorld, 0)",
            "PlayerController->GetWorld() != PlayWorld",
            "!PlayerController->IsLocalController()",
            "PlayerController->GetViewTarget() != RuntimeCamera",
            "Exterior->GetActorUpVector()",
            "GetPlayerViewPoint",
            "GetCameraCacheTime()",
            "GetCameraCacheView()",
            "CachedPov.FOV",
            "RuntimeCameraFieldOfViewDegrees",
            "!FMath::IsFinite(LoadProgress)",
            "LoadProgress < MinimumCalibrationTilesetLoadProgress",
            "!Tileset->GetCreatePhysicsMeshes()",
        ):
            self.assertIn(contract, helper)
        for forbidden in (
            "ValidateRuntimeWorldV2(",
            "ValidateRuntimeWorldV2Readiness(",
            "GetLongitudeLatitudeHeight",
            "Wgs84DistanceMeters",
            "TransformUnrealPositionToLongitudeLatitudeHeight",
            "ACesiumCartographicPolygon",
            "UCesiumPolygonRasterOverlay",
        ):
            self.assertNotIn(forbidden, helper)

        public_validator = EDITOR_CPP[
            EDITOR_CPP.index(
                "UTRIADIstanaEditorLibrary::ValidateIstanaPlayWorldReadiness("
            ) :
            EDITOR_CPP.index(
                "UTRIADIstanaEditorLibrary::PrepareIstanaRuntimeMapV2Streaming("
            )
        ]
        self.assertIn(
            "ValidateRuntimeWorldV2(EditorWorld, true, EditorStructuralReport)",
            public_validator,
        )
        self.assertIn(
            "ValidatePlayWorldV2Readiness(PlayWorld, PlayReadinessReport)",
            public_validator,
        )
        self.assertNotIn("ValidateRuntimeWorldV2(PlayWorld", public_validator)
        self.assertNotIn("ValidateRuntimeWorldV2Readiness(PlayWorld", EDITOR_CPP)

    def test_visual_acceptance_readiness_is_explicit_transient_and_exact(self) -> None:
        visual_validator = EDITOR_CPP[
            EDITOR_CPP.index(
                "UTRIADIstanaEditorLibrary::ValidateIstanaVisualAcceptancePlayWorldReadiness("
            ) :
            EDITOR_CPP.index(
                "UTRIADIstanaEditorLibrary::QuiesceIstanaPlayWorldForStop("
            )
        ]
        for contract in (
            "ValidateIstanaPlayWorldReadiness(StandardReadiness)",
            "IsVisualAcceptanceAirSimProfileActive(ProfileReason)",
            "LoadProgress != 100.0f",
            "!Tileset->GetCreatePhysicsMeshes()",
            "!Tileset->EnableFrustumCulling",
            "Tileset->EnableFogCulling",
            "bAuthoredRefinedRendererActive",
            "ExteriorStaticMesh->GetPathName() == IstanaExteriorMeshObjectPath",
            "RequiredExteriorMeshAsset.ToSoftObjectPath().ToString() ==",
            "ExteriorMesh->IsVisible() && !ExteriorMesh->bHiddenInGame",
            "ExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision",
            "!RuntimePolicies[0]->bStreamedPrimaryVisualActiveAtRuntime",
            "RuntimePolicies[0]->bPreferStreamedIstanaVisualWhenReady",
            "IsVisualAcceptanceStudyOverlaySuppressionActive",
            "TRIADHumanOnlyOverlay",
            "AUTHORED_REFINED_PRIMARY",
            "Provider data is retained as surrounding context only",
            "is not claimed as the Istana building",
            "NON-PRODUCTION VISUAL ACCEPTANCE",
            "does not claim that the complete offscreen 1 km sensor-physics AOI is resident",
        ):
            self.assertIn(contract, visual_validator)
        self.assertNotIn("ValidateRuntimeWorldV2(PlayWorld", visual_validator)
        self.assertNotIn("IsStreamedPrimaryVisualActive", visual_validator)
        self.assertNotIn("STREAMED_PHOTOGRAMMETRY_PRIMARY", visual_validator)

        transient_policy = POLICY_CPP[
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::ApplyVisualAcceptanceStreamingPolicy()"
            ) :
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::EnforceRuntimeCameraForPlayer0()"
            )
        ]
        self.assertIn('TEXT("TRIADIstanaVisualAcceptance")', POLICY_CPP)
        for contract in (
            "World->WorldType != EWorldType::PIE",
            "Tileset->EnableFrustumCulling = true",
            "Tileset->EnableFogCulling = false",
            "Tileset->EnforceCulledScreenSpaceError = true",
            "Tileset->CulledScreenSpaceError = 32.0",
            "Tileset->RefreshTileset()",
            "bVisualAcceptanceStreamingPolicyAppliedAtRuntime",
            "TilesetRefreshAttempts = MaximumTilesetRefreshAttempts",
        ):
            self.assertIn(contract, transient_policy)
        for forbidden in ("->Modify()", "MarkPackageDirty", "SaveMap("):
            self.assertNotIn(forbidden, transient_policy)

    def test_streamed_primary_is_explicit_opt_in_reversible_and_collision_preserving(self) -> None:
        for contract in (
            "IsStreamedPrimaryVisualActive",
            "bPreferStreamedIstanaVisualWhenReady = false",
            "Explicit opt-in only",
            "StreamedPrimaryEvaluationIntervalSeconds = 1.0f",
            "StreamedPrimaryRestoreBelowLoadProgress = 99.0f",
            "bStreamedPrimaryVisualActiveAtRuntime",
            "bAuthoredExteriorFallbackVisibleAtRuntime",
            "bAuthoredExteriorCollisionPreservedAtRuntime",
        ):
            self.assertIn(contract, POLICY_H)

        evaluate = POLICY_CPP[
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::EvaluateStreamedPrimaryVisual()"
            ) :
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::RefreshUnreadyTilesets()"
            )
        ]
        for contract in (
            "World->WorldType != EWorldType::PIE",
            "FMath::IsFinite(LoadProgress)",
            "Tileset->GetCreatePhysicsMeshes()",
            "LoadProgress == 100.0f",
            "LoadProgress < RestoreThreshold",
            "ExteriorActors.Num() == 1",
            "ExteriorMesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision",
            "ExteriorMesh->SetVisibility(false, false)",
            "ExteriorMesh->SetHiddenInGame(true, false)",
            "Candidate->SetVisibility(true, false)",
            "Candidate->SetHiddenInGame(false, false)",
            "bAuthoredExteriorCollisionPreservedAtRuntime",
        ):
            self.assertIn(contract, evaluate)
        for forbidden in (
            "SetCollisionEnabled",
            "SetActorHiddenInGame",
            "->Modify()",
            "MarkPackageDirty",
            "SaveMap(",
            "DestroyActor",
        ):
            self.assertNotIn(forbidden, evaluate)

        begin_play = POLICY_CPP[
            POLICY_CPP.index("void ATRIADIstanaRuntimePolicyActor::BeginPlay()") :
            POLICY_CPP.index("void ATRIADIstanaRuntimePolicyActor::EndPlay(")
        ]
        self.assertIn("EvaluateStreamedPrimaryVisual()", begin_play)
        self.assertIn("if (bPreferStreamedIstanaVisualWhenReady)", begin_play)
        self.assertIn("StreamedPrimaryEvaluationTimerHandle", begin_play)
        self.assertIn("EvaluationInterval,\n            true", begin_play)
        self.assertLess(
            begin_play.index("if (bPreferStreamedIstanaVisualWhenReady)"),
            begin_play.index("StreamedPrimaryEvaluationTimerHandle"),
        )

        readback = POLICY_CPP[
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsStreamedPrimaryVisualActive("
            ) :
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsVisualAcceptanceStudyOverlaySuppressionActive("
            )
        ]
        for contract in (
            "!ExteriorMesh->IsVisible() && ExteriorMesh->bHiddenInGame",
            "GetCollisionEnabled() != ECollisionEnabled::NoCollision",
            "StreamedPrimaryRestoreBelowLoadProgress",
            "bEveryTilesetFiniteWithPhysics",
        ):
            self.assertIn(contract, readback)
        self.assertNotIn("== 100.0f", readback)

        normal_readiness = EDITOR_CPP[
            EDITOR_CPP.index("bool ValidatePlayWorldV2Readiness(") :
            EDITOR_CPP.index("bool ReloadAndValidatePersistedRuntimeMapV2(")
        ]
        self.assertIn(
            "STREAMED_PHOTOGRAMMETRY_PRIMARY_EXPLICIT_OPT_IN",
            normal_readiness,
        )
        self.assertIn(
            "AUTHORED_REFINED_FALLBACK_PENDING_STREAMED_OPT_IN",
            normal_readiness,
        )
        self.assertIn("AUTHORED_REFINED_PRIMARY", normal_readiness)
        self.assertIn(
            "streamed photogrammetry supplies surrounding context only",
            normal_readiness,
        )
        self.assertIn("IsStreamedPrimaryVisualActive", normal_readiness)

    def test_visual_acceptance_hides_only_tagged_study_overlays(self) -> None:
        transient_policy = POLICY_CPP[
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::ApplyVisualAcceptanceStreamingPolicy()"
            ) :
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::ApplyRuntimeCameraQualityProfile("
            )
        ]
        for contract in (
            "TActorIterator<ATRIADIstanaStudyAreaActor>",
            "ComponentHasTag(HumanOnlyOverlayComponentTag)",
            "Component->SetVisibility(false, false)",
            "Component->SetHiddenInGame(true, false)",
            "IsVisualAcceptanceStudyOverlaySuppressionActive",
            "bVisualAcceptanceStudyOverlaysHiddenAtRuntime",
        ):
            self.assertIn(contract, transient_policy)
        for forbidden in (
            "SetActorHiddenInGame",
            "SetCollisionEnabled",
            "DestroyActor",
            "InvertSelection",
            "ExcludeSelectedTiles",
            "->Modify()",
            "MarkPackageDirty",
            "SaveMap(",
        ):
            self.assertNotIn(forbidden, transient_policy)

        overlay_readback = POLICY_CPP[
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsVisualAcceptanceStudyOverlaySuppressionActive("
            ) :
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsVisualAcceptanceAirSimProfileActive("
            )
        ]
        self.assertIn("StudyAreaCount == 1", overlay_readback)
        self.assertIn("OutHiddenComponentCount > 0", overlay_readback)
        self.assertIn("!Component->IsVisible() && Component->bHiddenInGame", overlay_readback)

    def test_runtime_camera_quality_profile_is_transient_and_exact(self) -> None:
        for contract in (
            "RuntimeCameraAutoExposureBias = -0.25f",
            "RuntimeCameraLocalHighlightContrast = 0.85f",
            "RuntimeCameraLocalShadowContrast = 0.90f",
            "RuntimeCameraLocalDetailStrength = 1.15f",
            "RuntimeCameraGlobalContrast = 1.08",
            "RuntimeCameraSharpen = 0.35f",
        ):
            self.assertIn(contract, POLICY_CPP)
        apply_quality = POLICY_CPP[
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::ApplyRuntimeCameraQualityProfile("
            ) :
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::EnforceRuntimeCameraForPlayer0()"
            )
        ]
        for contract in (
            "SetConstraintAspectRatio(false)",
            "SetPostProcessBlendWeight(1.0f)",
            "ELocalExposureMethod::Bilateral",
            "bOverride_LocalExposureHighlightContrastScale = true",
            "bOverride_LocalExposureShadowContrastScale = true",
            "bOverride_LocalExposureDetailStrength = true",
            "bOverride_ColorContrast = true",
            "bOverride_Sharpen = true",
            "BloomIntensity = 0.0f",
            "VignetteIntensity = 0.0f",
            "MotionBlurAmount = 0.0f",
            "SceneFringeIntensity = 0.0f",
            "IsRuntimeCameraQualityProfileActive",
        ):
            self.assertIn(contract, apply_quality)
        for forbidden in ("->Modify()", "MarkPackageDirty", "SaveMap("):
            self.assertNotIn(forbidden, apply_quality)

        readback = POLICY_CPP[
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsRuntimeCameraQualityProfileActive("
            ) :
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsStreamedPrimaryVisualActive("
            )
        ]
        self.assertIn("!CameraComponent->bConstrainAspectRatio", readback)
        self.assertIn("Settings.LocalExposureMethod == ELocalExposureMethod::Bilateral", readback)
        self.assertIn("Settings.bOverride_SceneFringeIntensity", readback)

    def test_imported_exterior_ceremonial_front_is_geodetically_proven(self) -> None:
        self.assertIn("ImportedExteriorYawDegrees = 182.3", EDITOR_CPP)
        self.assertIn("FootprintYawDegrees = 182.3", EXTERIOR_H)
        self.assertIn("source ceremonial +Y to Unreal local -Y", EXTERIOR_H)
        orientation_helper = EDITOR_CPP[
            EDITOR_CPP.index("bool IsImportedExteriorCeremonialFrontFacingSouth(") :
            EDITOR_CPP.index("bool SetCameraAutoActivateForPlayer(")
        ]
        for contract in (
            "MakeSouthOffsetLongitudeLatitudeHeight(50.0",
            "InverseTransformVectorNoScale",
            "ImportedCeremonialFrontLocal(0.0, -1.0, 0.0)",
            "OutFrontAlignmentDot >= 0.999",
        ):
            self.assertIn(contract, orientation_helper)

        build = EDITOR_CPP[
            EDITOR_CPP.index("BuildIstanaRuntimeMapV2(") :
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2GameMode(")
        ]
        self.assertIn("ImportedExteriorYawDegrees", build)
        self.assertIn("IsImportedExteriorCeremonialFrontFacingSouth", EDITOR_CPP)
        repair = EDITOR_CPP[
            EDITOR_CPP.index("RepairIstanaRuntimeMapV2Camera(") :
            EDITOR_CPP.index("ValidateIstanaRuntimeMapV2(")
        ]
        self.assertIn("ImportedExteriorYawDegrees", repair)
        self.assertIn("PreviousExteriorYawDegrees", repair)
        self.assertIn("RestoreUnsavedCameraAndOrientationEdit", repair)

    def test_visual_profile_proof_does_not_use_cross_module_settings_singleton(self) -> None:
        verifier = POLICY_CPP[
            POLICY_CPP.index(
                "ATRIADIstanaRuntimePolicyActor::IsVisualAcceptanceAirSimProfileActive("
            ) :
            POLICY_CPP.index(
                "ATRIADIstanaRuntimePolicyActor::RequestAirSimQuiescenceForTeardown("
            )
        ]
        for contract in (
            'FParse::Value(',
            'TEXT("-settings=")',
            "FPaths::ProjectConfigDir()",
            'TEXT("IstanaVisualAcceptance.settings.json")',
            "FPaths::ConvertRelativePathToFull",
            "FPaths::IsSamePath",
            "FFileHelper::LoadFileToString",
            'TryGetStringField(TEXT("TRIADProfile")',
            'TryGetStringField(TEXT("SimMode")',
            'TryGetBoolField(TEXT("EnableRpc")',
            'HasField(TEXT("DefaultSensors"))',
            'Contains(TEXT("lidar"), ESearchCase::IgnoreCase)',
            'TryGetObjectField(TEXT("Vehicles")',
            'TryGetObjectField(TEXT("Sensors")',
            "TActorIterator<ASimModeBase>",
            "SimMode->IsA<ASimModeComputerVision>()",
            "SimModeCount != 1 || ComputerVisionSimModeCount != 1",
        ):
            self.assertIn(contract, verifier)
        for forbidden in (
            "AirSimSettings::singleton",
            "Settings.simmode_name",
            "Settings.enable_rpc",
            "Settings.settings_text_",
            "Settings.vehicles",
        ):
            self.assertNotIn(forbidden, verifier)
        self.assertIn(
            '#include "Vehicles/ComputerVision/SimModeComputerVision.h"',
            POLICY_CPP,
        )
        self.assertNotIn('#include "common/AirSimSettings.hpp"', POLICY_CPP)

    def test_streaming_preparation_drives_viewport_without_claiming_readiness(self) -> None:
        prepare = EDITOR_CPP[
            EDITOR_CPP.index("PrepareIstanaRuntimeMapV2Streaming(") :
            EDITOR_CPP.index("CalibrateIstanaRuntimeMapV2Ground(")
        ]
        for contract in (
            "GEditor->PlayWorld",
            "ValidateRuntimeWorldV2(World, true, StructuralReport)",
            "GCurrentLevelEditingViewportClient",
            "GEditor->GetActiveViewport()",
            "RuntimeCameraActorTag",
            "SetViewportType(LVT_Perspective)",
            "SetRealtime(true)",
            "SetViewLocation(RuntimeCamera->GetActorLocation())",
            "SetViewRotation(RuntimeCamera->GetActorRotation())",
            "Viewport->Draw(true)",
            "GetLoadProgress()",
            "LoadProgress <= KINDA_SMALL_NUMBER",
            "Tileset->RefreshTileset()",
            "No readiness claim, screenshot, package edit, or save",
        ):
            self.assertIn(contract, prepare)
        for forbidden in (
            "ValidateRuntimeWorldV2Readiness",
            "SaveMap(",
            "SaveLoadedAssets",
            "FScreenshotRequest",
            "->Modify()",
        ):
            self.assertNotIn(forbidden, prepare)
        self.assertEqual(prepare.count("Tileset->RefreshTileset()"), 1)

    def test_v2_is_new_destination_only_and_refuses_overwrite(self) -> None:
        self.assertIn(
            'TEXT("/Game/Maps/Istana_1km_Context_v2")',
            EDITOR_CPP,
        )
        v2 = EDITOR_CPP[
            EDITOR_CPP.index("BuildIstanaRuntimeMapV2(") :
            EDITOR_CPP.index("ValidateIstanaRuntimeMapV2(")
        ]
        self.assertIn("FPackageName::DoesPackageExist(DestinationMapV2Package)", v2)
        self.assertIn("NewMapFromTemplate(SourceFilename, false)", v2)
        self.assertIn("SaveMap(World, DestinationMapV2Package)", v2)
        self.assertNotIn("SaveMap(World, SourceMapPackage)", v2)
        self.assertNotIn("SaveMap(World, DestinationMapPackage)", v2)
        self.assertNotIn("RecenterGeoreferences", v2)
        self.assertIn("FindTemplateGeoreferenceWithoutModification", v2)

    def test_v2_requires_project_exterior_and_has_no_procedural_fallback(self) -> None:
        refined_path = (
            "/Game/TRIAD/Istana/Meshes/"
            "SM_IstanaExterior_Refined.SM_IstanaExterior_Refined"
        )
        self.assertIn(
            refined_path,
            EDITOR_CPP + EXTERIOR_CPP,
        )
        self.assertIn(
            "const FSoftObjectPath RequiredIstanaExteriorMeshPath",
            EXTERIOR_CPP,
        )
        self.assertNotIn(
            "/Game/TRIAD/Istana/Meshes/SM_IstanaExterior.SM_IstanaExterior",
            EXTERIOR_CPP,
        )
        v2 = EDITOR_CPP[
            EDITOR_CPP.index("BuildIstanaRuntimeMapV2(") :
            EDITOR_CPP.index("ValidateIstanaRuntimeMapV2(")
        ]
        self.assertIn("ATRIADIstanaExteriorMeshActor", v2)
        self.assertIn("ValidateIstanaExteriorMeshAsset", v2)
        self.assertIn("will not use the procedural fallback", v2)
        self.assertNotIn("ATRIADIstanaBuildingActor", v2)
        self.assertIn("There is intentionally no procedural geometry fallback", EXTERIOR_H)
        self.assertIn("SetEastSouthUpRotation", EXTERIOR_CPP)
        self.assertIn("ECollisionEnabled::QueryAndPhysics", EXTERIOR_CPP)

        validator = EDITOR_CPP[
            EDITOR_CPP.index("bool ValidateRuntimeWorldV2(") :
            EDITOR_CPP.index("bool ValidateRuntimeWorldV2Readiness(")
        ]
        for contract in (
            "bool bRequireRefinedExteriorAsset = true",
            "ExteriorMeshActors[0]->ExteriorMeshComponent->GetCollisionEnabled() !=",
            "ECollisionEnabled::QueryAndPhysics",
            "!ExteriorMeshActors[0]->ExteriorMeshComponent->IsVisible()",
            "ExteriorMeshActors[0]->ExteriorMeshComponent->bHiddenInGame",
            "ValidateIstanaExteriorMeshAsset",
            "ExistingSoftObjectPath",
            "bRequireRefinedExteriorAsset &&",
            "ExistingSoftObjectPath != IstanaExteriorMeshObjectPath",
            "RequiredExteriorMeshAsset",
            ".ToSoftObjectPath()",
            ".ToString()",
        ):
            self.assertIn(contract, validator)

    def test_pbr_import_is_exact_non_overwriting_and_materialized(self) -> None:
        for name in ("ImportIstanaPbrMaterials", "ValidateIstanaPbrMaterials"):
            self.assertIn(name, EDITOR_H)
            self.assertIn(name, EDITOR_CPP)
        self.assertIn('"MaterialEditor"', EDITOR_BUILD)
        self.assertIn("UTextureFactory", EDITOR_CPP)
        self.assertIn("UMaterialFactoryNew", EDITOR_CPP)
        self.assertIn("UMaterialEditingLibrary", EDITOR_CPP)
        self.assertNotIn("UMaterialInstanceConstantFactoryNew", EDITOR_CPP)
        self.assertIn("SourceAssets/Istana/Generated/Textures", EDITOR_CPP)
        self.assertIn("IstanaPbrTextures.manifest.json", EDITOR_CPP)
        self.assertIn("/Game/TRIAD/Istana/Textures", EDITOR_CPP)
        self.assertIn("/Game/TRIAD/Istana/Materials", EDITOR_CPP)
        self.assertIn("bReplaceExisting = false", EDITOR_CPP)
        self.assertIn("bReplaceExistingSettings = false", EDITOR_CPP)
        self.assertEqual(
            len(
                re.findall(
                    r'\{TEXT\("T_Istana_[^"]+"\), EIstanaTextureUsage::',
                    EDITOR_CPP,
                )
            ),
            22,
        )
        for setting in (
            "TC_Default",
            "TC_Normalmap",
            "TC_Masks",
            "TC_Grayscale",
            "SAMPLERTYPE_Normal",
            "SAMPLERTYPE_Masks",
            "SAMPLERTYPE_LinearGrayscale",
            "Texture->bFlipGreenChannel = false",
        ):
            self.assertIn(setting, EDITOR_CPP)
        for binding in (
            'TEXT("R"), MP_AmbientOcclusion',
            'TEXT("G"), MP_Roughness',
            'TEXT("B"), MP_Metallic',
            "UMaterialExpressionBumpOffset",
            "BLEND_Translucent",
            "RM_IndexOfRefraction",
        ):
            self.assertIn(binding, EDITOR_CPP)
        self.assertEqual(
            EDITOR_CPP.count(
                'ConnectMaterialExpression(SampleCoordinates, TEXT(""), '
            ),
            3,
        )
        self.assertGreaterEqual(EDITOR_CPP.count('TEXT("UVs")'), 4)
        self.assertIn(
            'HeightSample, TEXT("UVs"), TEXT("UV to slate height")',
            EDITOR_CPP,
        )
        self.assertNotIn(
            'BaseColorSample, TEXT("Coordinates")',
            EDITOR_CPP,
        )
        self.assertIn("GetMaterialExpressionInputNames(To)", EDITOR_CPP)

    def test_refined_lod0_import_is_manifest_exact_non_overwriting_and_materialized(self) -> None:
        self.assertIn("ImportIstanaExteriorLod0", EDITOR_H)
        self.assertIn("ImportIstanaExteriorRefinedLod0", EDITOR_H)

        legacy_importer = EDITOR_CPP[
            EDITOR_CPP.index("ImportIstanaExteriorLod0(") :
            EDITOR_CPP.index("ImportIstanaExteriorRefinedLod0(")
        ]
        self.assertIn("legacy SM_IstanaExterior import target is retired", legacy_importer)
        self.assertIn("return false", legacy_importer)
        self.assertNotIn("ImportAssetTasks", legacy_importer)

        manifest_validator = EDITOR_CPP[
            EDITOR_CPP.index("bool ValidateRefinedIstanaSourceManifest(") :
            EDITOR_CPP.index("bool ValidateIstanaExteriorMeshAsset(")
        ]
        for contract in (
            "IstanaExterior.manifest.json",
            "triad.istana_source_assets.v1",
            "right-handed Z-up; ceremonial approach +Y",
            'AuthoringUnits != TEXT("metres")',
            'EncodedObjUnits != TEXT("centimetres")',
            'UnrealUnits != TEXT("centimetres (import scale 1.0)")',
            "MaterialSlots->Num() != GetIstanaMaterialSpecs().Num()",
            "SlotName != GetIstanaMaterialSpecs()[Index].SlotName.ToString()",
            'Role == TEXT("visual_lod_0")',
            'Lod0Path != TEXT("SM_IstanaExterior_LOD0.obj")',
            "VertexCount != RefinedExteriorLod0VertexCount",
            "TriangleCount != RefinedExteriorLod0TriangleCount",
            "FVector(-62.5, -52.0, -0.85)",
            "FVector(62.5, 65.69444444444444, 36.0)",
            "DeclaredSha256.Equals(RefinedExteriorLod0Sha256",
            "EndPedimentCount != 2.0",
            "DeepEntranceArchCount != 3.0",
            "TowerLouvreBayCount != 4.0",
            "FrontDormerCount != 1.0",
            "bHasCupolaOrDome",
            "bHasExposedBrickFacade",
            "0.15 m maximum visible",
        ):
            self.assertIn(contract, manifest_validator)

        manifest = json.loads(REFINED_MANIFEST_PATH.read_text(encoding="utf-8"))
        self.assertEqual(manifest["schema"], "triad.istana_source_assets.v1")
        self.assertEqual(
            manifest["coordinateSystem"],
            "right-handed Z-up; ceremonial approach +Y",
        )
        self.assertEqual(manifest["authoringUnits"], "metres")
        self.assertEqual(manifest["encodedObjUnits"], "centimetres")
        self.assertEqual(
            manifest["unrealUnits"],
            "centimetres (import scale 1.0)",
        )
        expected_slots = [
            "M_Istana_Plaster",
            "M_Istana_PlasterTrim",
            "M_Istana_Slate",
            "M_Istana_Shutter",
            "M_Istana_Glass",
            "M_Istana_Stone",
            "M_Istana_Metal",
            "M_Istana_Door",
        ]
        self.assertEqual(manifest["materialSlots"], expected_slots)
        lod0 = next(
            entry for entry in manifest["files"] if entry["role"] == "visual_lod_0"
        )
        self.assertEqual(lod0["path"], "SM_IstanaExterior_LOD0.obj")
        self.assertEqual(lod0["vertices"], 175974)
        self.assertEqual(lod0["triangles"], 58658)
        self.assertEqual(lod0["boundsMinMeters"], [-62.5, -52.0, -0.85])
        self.assertEqual(lod0["boundsMaxMeters"], [62.5, 65.69444444444444, 36.0])
        self.assertEqual(
            lod0["sha256"],
            "4c822bb2c85451136c41fab0a362c7d2f0ca6663c0ae44cfecbe8f73e6a83b91",
        )

        importer = EDITOR_CPP[
            EDITOR_CPP.index("ImportIstanaExteriorRefinedLod0(") :
            EDITOR_CPP.index("GenerateIstanaPlacementSurvey(")
        ]
        for contract in (
            "IsEditorOperationSafe",
            "FPackageName::DoesPackageExist(IstanaExteriorMeshPackage)",
            "AssetSubsystem->DoesAssetExist(IstanaExteriorMeshObjectPath)",
            "Refusing to overwrite existing Istana exterior asset",
            "SourceAssets/Istana/Generated/SM_IstanaExterior_LOD0.obj",
            "ValidateRefinedIstanaSourceManifest",
            "PbrState != EIstanaPbrAssetState::CompleteValid",
            "complete existing eight-material PBR set",
            "bImportAsSkeletal = false",
            "MeshTypeToImport = FBXIT_StaticMesh",
            "bAutomatedImportShouldDetectType = false",
            "bImportMesh = true",
            "bImportMaterials = false",
            "bImportTextures = false",
            "ImportUniformScale = 1.0f",
            "bCombineMeshes = true",
            "bAutoGenerateCollision = true",
            "bGenerateLightmapUVs = true",
            "EFBXNormalImportMethod::FBXNIM_ImportNormals",
            "EFBXNormalGenerationMethod::MikkTSpace",
            "bBuildNanite = false",
            "bRemoveDegenerates = true",
            'DestinationPath = TEXT("/Game/TRIAD/Istana/Meshes")',
            'DestinationName = TEXT("SM_IstanaExterior_Refined")',
            "bReplaceExisting = false",
            "bReplaceExistingSettings = false",
            "bAutomated = true",
            "bSave = false",
            "bAsync = false",
            "ValidateIstanaExteriorMeshAsset(ImportedMesh, false",
            "ImportedMesh->GetMaterialIndex(Spec.SlotName)",
            "ImportedMesh->SetMaterial(MaterialIndex, Material)",
            "ValidateIstanaExteriorMeshAsset(ImportedMesh, true",
            "SaveLoadedAssets(AssetsToSave, false)",
            "simple collision",
            "eight exact existing project-owned PBR materials",
            "PowerShell caller separately proves the OBJ byte SHA-256",
        ):
            self.assertIn(contract, importer)
        self.assertNotIn("BuildIstanaPbrAssetSet", importer)
        self.assertNotIn("LegacyIstanaExteriorMeshObjectPath", importer)

        for contract in (
            "constexpr int32 RefinedExteriorLod0VertexCount = 175974",
            "constexpr int32 RefinedExteriorLod0TriangleCount = 58658",
            "4c822bb2c85451136c41fab0a362c7d2f0ca6663c0ae44cfecbe8f73e6a83b91",
            "FVector(12500.0, 11769.4444, 3685.0)",
            "Mesh->GetNumTriangles(0) != RefinedExteriorLod0TriangleCount",
            "StaticMaterials.Num() != GetIstanaMaterialSpecs().Num()",
            "BodySetup->AggGeom.GetElementCount() == 0",
        ):
            self.assertIn(contract, EDITOR_CPP)

    def test_air_sim_start_and_camera_are_globe_anchored(self) -> None:
        self.assertIn('/Script/AirSimTriadRuntime.AirSimGameMode', EDITOR_CPP)
        self.assertIn(
            '/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode',
            EDITOR_CPP,
        )
        self.assertIn("SetPersistentAirSimGameModeOverride", EDITOR_CPP)
        self.assertIn("TActorIterator<APlayerStart>", EDITOR_CPP)
        self.assertIn("SetCameraAutoActivateForPlayer", EDITOR_CPP)
        self.assertIn("EAutoReceiveInput::Player0", EDITOR_CPP)
        self.assertIn("GetAutoActivatePlayerIndex() != 0", EDITOR_CPP)
        self.assertNotIn("RuntimeCamera->AutoActivateForPlayer", EDITOR_CPP)
        self.assertIn("AddGlobeAnchor", EDITOR_CPP)
        self.assertIn("PlayerStartSouthOffsetMeters = 180.0", EDITOR_CPP)
        self.assertIn("RuntimeCameraSouthOffsetMeters = 210.0", EDITOR_CPP)
        self.assertIn("PlayerStartHeightAboveGroundMeters = 8.0", EDITOR_CPP)
        self.assertIn("RuntimeCameraHeightAboveGroundMeters = 24.0", EDITOR_CPP)
        self.assertIn("RuntimeCameraAimAboveGroundMeters = 13.0", EDITOR_CPP)
        self.assertIn("RuntimeCameraFieldOfViewDegrees = 52.0f", EDITOR_CPP)
        self.assertIn(
            "SetFieldOfView(\n            RuntimeCameraFieldOfViewDegrees)",
            EDITOR_CPP,
        )

    def test_ue55_compile_surface_uses_public_component_and_camera_access(self) -> None:
        self.assertIn('#include "MaterialDomain.h"', EDITOR_CPP)
        self.assertIn('#include "UObject/UnrealType.h"', EDITOR_CPP)
        self.assertIn("FindFProperty<FByteProperty>", EDITOR_CPP)
        self.assertIn("SetPropertyValue_InContainer", EDITOR_CPP)
        self.assertIn("Anchor->RegisterComponent()", EDITOR_CPP)
        self.assertNotIn("Anchor->OnComponentCreated()", EDITOR_CPP)
        self.assertNotIn("->AutoActivateForPlayer =", EDITOR_CPP)

    def test_ground_calibration_is_guarded_and_never_samples_the_roof(self) -> None:
        self.assertIn("CalibrateIstanaRuntimeMapV2Ground", EDITOR_H)
        calibration = EDITOR_CPP[
            EDITOR_CPP.index("SampleIstanaExteriorRingGroundHeight(") :
            EDITOR_CPP.index("bool ResolveNewSavedFile(")
        ]
        for contract in (
            "GetLoadProgress() < MinimumCalibrationTilesetLoadProgress",
            "!It->GetCreatePhysicsMeshes()",
            "static const double SampleRadiiMeters[] = {95.0, 125.0}",
            "BearingIndex < 8",
            "QueryParameters.AddIgnoredActor(ExteriorMeshActor)",
            "QueryParameters.AddIgnoredActor(StudyArea)",
            "Cast<ACesium3DTileset>(Hit.GetActor())",
            "RawHeights[End + 1] - RawHeights[Start] <= 4.0",
            "MinimumCalibrationAcceptedSamples",
            "MedianOfSortedHeights",
        ):
            self.assertIn(contract, calibration)
        self.assertNotIn("CenterLongitudeDegrees,\n                CenterLatitudeDegrees,\n                0.0", calibration)

        callable_body = EDITOR_CPP[
            EDITOR_CPP.index("CalibrateIstanaRuntimeMapV2Ground(") :
            EDITOR_CPP.index("ImportIstanaPbrMaterials(")
        ]
        for moved_actor in (
            "ExteriorMeshActor->ConfigureGeodeticPlacement",
            "StudyArea->ConfigureStudyArea",
            "PlayerStartAnchor->MoveToLongitudeLatitudeHeight",
            "RuntimeCameraAnchor->MoveToLongitudeLatitudeHeight",
            "CESIUM_COLLISION_EXTERIOR_RING_MEDIAN_WGS84_ELLIPSOID",
            "SaveMap(World, DestinationMapV2Package)",
        ):
            self.assertIn(moved_actor, callable_body)
        self.assertNotIn("SaveMap(World, SourceMapPackage)", callable_body)

    def test_refined_authored_primary_uses_overlap_without_context_deletion(self) -> None:
        self.assertIn("StreamedReplacementHorizontalScale = 1.004", EXTERIOR_H)
        self.assertIn("this authored\n     * primary is rendered", EXTERIOR_H)
        self.assertIn("Only an explicit streamed-primary opt-in may yield", EXTERIOR_H)
        self.assertIn(
            "Refined authored public-exterior renderer and collision primary",
            EXTERIOR_H,
        )
        self.assertIn("no Cesium footprint clip or surrounding-context deletion", EXTERIOR_H)
        self.assertIn("FMath::Clamp(StreamedReplacementHorizontalScale, 1.0, 1.01)", EXTERIOR_CPP)
        self.assertIn("FVector(1.004, 1.004, 1.0)", EDITOR_CPP)

    def test_v2_preview_and_pie_spawn_capture_are_distinct(self) -> None:
        for name in ("CaptureIstanaPreview", "CaptureIstanaPlaySpawnPreview"):
            self.assertIn(name, EDITOR_H)
            self.assertIn(name, EDITOR_CPP)
        preview = EDITOR_CPP[
            EDITOR_CPP.index("CaptureIstanaPreview(") :
            EDITOR_CPP.index("CaptureIstanaPlaySpawnPreview(")
        ]
        play_preview = EDITOR_CPP[EDITOR_CPP.index("CaptureIstanaPlaySpawnPreview(") :]
        self.assertIn("ValidateIstanaRuntimeMapV2", preview)
        self.assertIn("ValidateIstanaRuntimeMapV2Readiness", preview)
        self.assertIn("V2_IMPORTED_MESH", preview)
        self.assertIn('TEXT("IstanaPreviews/V2")', preview)
        self.assertIn("v2_editor_", preview)
        self.assertNotIn("V2_PIE_PLAYER0_SPAWN", preview)
        self.assertNotIn("v2_play_", preview)
        self.assertIn("V2_PIE_PLAYER0_SPAWN", play_preview)
        self.assertIn("ValidateIstanaPlayWorldReadiness", play_preview)
        self.assertNotIn("ValidateRuntimeWorldV2Readiness", play_preview)
        self.assertIn("GetPlayerViewPoint", play_preview)
        self.assertIn("PlayWorld->GetGameViewport()", play_preview)
        self.assertNotIn("GEngine->GameViewport", play_preview)
        self.assertIn("GetGameViewport", play_preview)
        self.assertIn("v2_play_", play_preview)

    def test_narrow_remote_control_callers_are_present(self) -> None:
        expected = {
            "Import-IstanaPbrMaterials.ps1": "ImportIstanaPbrMaterials",
            "Import-IstanaExteriorLod0.ps1": "ImportIstanaExteriorLod0",
            "Import-IstanaExteriorRefinedLod0.ps1": "ImportIstanaExteriorRefinedLod0",
            "Build-IstanaRuntimeMapV2.ps1": "BuildIstanaRuntimeMapV2",
            "Repair-IstanaRuntimeMapV2GameMode.ps1": "RepairIstanaRuntimeMapV2GameMode",
            "Repair-IstanaRuntimeMapV2Camera.ps1": "RepairIstanaRuntimeMapV2Camera",
            "Repair-IstanaRuntimeMapV2ExteriorAsset.ps1": "RepairIstanaRuntimeMapV2ExteriorAsset",
            "Calibrate-IstanaRuntimeMapV2Ground.ps1": "CalibrateIstanaRuntimeMapV2Ground",
            "Prepare-IstanaRuntimeMapV2Streaming.ps1": "PrepareIstanaRuntimeMapV2Streaming",
            "Capture-IstanaPreview.ps1": "CaptureIstanaPreview",
            "Capture-IstanaPlaySpawnPreview.ps1": "CaptureIstanaPlaySpawnPreview",
        }
        for script_name, function_name in expected.items():
            source = (SCRIPTS / script_name).read_text(encoding="utf-8")
            self.assertIn("/remote/object/call", source)
            self.assertIn(function_name, source)
            self.assertIn("ValidateIstanaRemoteControlProject", source)
            self.assertIn("-TimeoutSec", source)
            self.assertNotIn("DeleteAsset", source)

        build = (SCRIPTS / "Build-IstanaRuntimeMapV2.ps1").read_text(
            encoding="utf-8"
        )
        self.assertIn("Content\\SDTH.umap", build)
        self.assertIn("Get-FileHash -Algorithm SHA256", build)
        self.assertIn("ValidateIstanaRuntimeMapV2Readiness", build)
        self.assertIn("StructuralOnly", build)

        calibration = (
            SCRIPTS / "Calibrate-IstanaRuntimeMapV2Ground.ps1"
        ).read_text(encoding="utf-8")
        self.assertIn("ValidateIstanaRuntimeMapV2Readiness", calibration)
        self.assertIn("ReadinessWaitSeconds", calibration)
        self.assertIn("Start-Sleep -Seconds 5", calibration)

        prepare_script = (
            SCRIPTS / "Prepare-IstanaRuntimeMapV2Streaming.ps1"
        ).read_text(encoding="utf-8")
        self.assertIn("ValidateIstanaRemoteControlProject", prepare_script)
        self.assertIn("PrepareIstanaRuntimeMapV2Streaming", prepare_script)
        self.assertIn("PackageSaved = $false", prepare_script)
        self.assertIn("ReadinessClaimed = $false", prepare_script)
        self.assertNotIn("CaptureIstana", prepare_script)
        self.assertNotIn("ValidateIstanaRuntimeMapV2Readiness", prepare_script)

        repair_script = (
            SCRIPTS / "Repair-IstanaRuntimeMapV2GameMode.ps1"
        ).read_text(encoding="utf-8")
        self.assertIn("Content\\SDTH.umap", repair_script)
        self.assertIn("Content\\Maps\\Istana_1km.umap", repair_script)
        self.assertIn("Content\\Maps\\Istana_1km_Context_v2.umap", repair_script)
        self.assertIn("Get-FileHash -Algorithm SHA256", repair_script)
        self.assertIn("IstanaMapBackups", repair_script)
        self.assertIn("ReloadPersistenceValidated = $true", repair_script)
        self.assertNotIn("Remove-Item", repair_script)

        camera_repair_script = (
            SCRIPTS / "Repair-IstanaRuntimeMapV2Camera.ps1"
        ).read_text(encoding="utf-8")
        for contract in (
            "ValidateIstanaRemoteControlProject",
            "RepairIstanaRuntimeMapV2Camera",
            "Content\\SDTH.umap",
            "Content\\Maps\\Istana_1km.umap",
            "Content\\Maps\\Istana_1km_Context_v2.umap",
            "Get-FileHash -Algorithm SHA256",
            "IstanaMapBackups",
            "backupHash -ne $v2HashBefore",
            "v2HashOnFailure -ne $v2HashBefore",
            "camera migration created a backup but exact v2 did not change",
            "[string]::IsNullOrWhiteSpace($backupFile)",
            "ReloadPersistenceValidated = $true",
            "-TimeoutSec",
        ):
            self.assertIn(contract, camera_repair_script)
        for forbidden in ("Remove-Item", "Set-Content", "Copy-Item"):
            self.assertNotIn(forbidden, camera_repair_script)

        refined_import_script = (
            SCRIPTS / "Import-IstanaExteriorRefinedLod0.ps1"
        ).read_text(encoding="utf-8")
        for contract in (
            "ValidateIstanaRemoteControlProject",
            "ImportIstanaExteriorRefinedLod0",
            "SM_IstanaExterior_LOD0.obj",
            "IstanaExterior.manifest.json",
            "SM_IstanaExterior_Refined.uasset",
            "Refusing to overwrite existing refined exterior asset",
            "triad.istana_source_assets.v1",
            "right-handed Z-up; ceremonial approach +Y",
            "authoringUnits -ne 'metres'",
            "encodedObjUnits -ne 'centimetres'",
            "unrealUnits -ne 'centimetres (import scale 1.0)'",
            "[int] $lod0[0].vertices -ne 175974",
            "[int] $lod0[0].triangles -ne 58658",
            "4C822BB2C85451136C41FAB0A362C7D2F0CA6663C0AE44CFECBE8F73E6A83B91",
            "Get-FileHash -LiteralPath $sourceObj -Algorithm SHA256",
            "materialHashesBefore",
            "sourceMapHashAfter -ne $sourceMapHashBefore",
            "legacyHashAfter -ne $legacyHashBefore",
            "materialHashAfter -ne $materialHashesBefore[$materialFile]",
            "-TimeoutSec 600",
            "SourceObjSha256 = $actualDigest",
            "OverwriteAllowed = $false",
        ):
            self.assertIn(contract, refined_import_script)
        for forbidden in ("Remove-Item", "Set-Content", "Copy-Item"):
            self.assertNotIn(forbidden, refined_import_script)

        exterior_repair_script = (
            SCRIPTS / "Repair-IstanaRuntimeMapV2ExteriorAsset.ps1"
        ).read_text(encoding="utf-8")
        for contract in (
            "ValidateIstanaRemoteControlProject",
            "RepairIstanaRuntimeMapV2ExteriorAsset",
            "Content\\SDTH.umap",
            "Content\\Maps\\Istana_1km.umap",
            "Content\\Maps\\Istana_1km_Context_v2.umap",
            "SM_IstanaExterior.uasset",
            "SM_IstanaExterior_Refined.uasset",
            "Get-OptionalSha256",
            "Get-FileHash -Algorithm SHA256",
            "sourceHashOnFailure -ne $sourceHashBefore",
            "v1HashOnFailure -ne $v1HashBefore",
            "legacyHashOnFailure -ne $legacyHashBefore",
            "refinedHashOnFailure -ne $refinedHashBefore",
            "v2HashOnFailure -ne $v2HashBefore",
            "Saved\\TRIAD\\IstanaMapBackups",
            "StartsWith",
            "backupHash -ne $v2HashBefore",
            "exact v2 changed without a reported, verified pre-migration backup",
            "migration created a backup but exact v2 did not change",
            "IdempotentNoChange",
            "ReloadPersistenceValidated = $true",
            "-TimeoutSec 600",
        ):
            self.assertIn(contract, exterior_repair_script)
        for forbidden in ("Remove-Item", "Set-Content", "Copy-Item"):
            self.assertNotIn(forbidden, exterior_repair_script)

        pie = (SCRIPTS / "Set-IstanaPlayInEditor.ps1").read_text(encoding="utf-8")
        self.assertIn("ValidateIstanaRemoteControlProject", pie)
        self.assertIn("ValidateIstanaRuntimeMapV2", pie)
        self.assertIn("ValidateIstanaPlayWorldReadiness", pie)
        self.assertIn("ValidateIstanaVisualAcceptancePlayWorldReadiness", pie)
        self.assertIn("[switch] $VisualAcceptanceOnly", pie)
        self.assertIn("$requiredStableReadinessPolls = if ($VisualAcceptanceOnly) { 3 } else { 1 }", pie)
        self.assertIn("$stableReadinessPolls -lt $requiredStableReadinessPolls", pie)
        self.assertIn("-TRIADIstanaVisualAcceptance", pie)
        self.assertIn(r"IstanaVisualAcceptance\.settings\.json", pie)
        self.assertIn("[int] $ReadinessWaitSeconds = 600", pie)
        self.assertIn("[int] $QuiesceDrainSeconds = 15", pie)
        self.assertIn("$readinessDeadline", pie)
        self.assertIn("Last report:", pie)
        self.assertIn("SessionWasAlreadyRunning", pie)
        self.assertIn("ValidateExistingPlaySession", pie)
        self.assertIn("PIE was intentionally left running", pie)
        self.assertIn("-TimeoutSec", pie)

        stop_branch = pie[pie.index("if ($Stop) {") : pie.index("# Starting uses")]
        self.assertIn("EditorRequestEndPlay", stop_branch)
        self.assertIn("QuiesceIstanaPlayWorldForStop", stop_branch)
        self.assertIn("if (-not $quiesceVerified)", stop_branch)
        self.assertIn("Refusing unsafe EditorRequestEndPlay", stop_branch)
        self.assertLess(
            stop_branch.index("if (-not $quiesceVerified)"),
            stop_branch.index("functionName = 'EditorRequestEndPlay'"),
        )
        self.assertIn("Start-Sleep -Seconds $QuiesceDrainSeconds", stop_branch)
        self.assertIn("IsInPlayInEditor", pie)
        self.assertNotIn("ValidateIstanaRuntimeMapV2", stop_branch)

        timeout_branch = pie[
            pie.index("if (-not $playReadiness -or") : pie.index("[PSCustomObject]@{", pie.index("if (-not $playReadiness -or"))
        ]
        self.assertNotIn("EditorRequestEndPlay", timeout_branch)

        play_capture = (
            SCRIPTS / "Capture-IstanaPlaySpawnPreview.ps1"
        ).read_text(encoding="utf-8")
        self.assertIn("ValidateIstanaPlayWorldReadiness", play_capture)
        self.assertIn("ValidateIstanaVisualAcceptancePlayWorldReadiness", play_capture)
        self.assertIn("$requiredStablePolls = if ($VisualAcceptanceOnly) { 3 } else { 1 }", play_capture)
        self.assertIn("v2_play_visual_acceptance_", play_capture)
        self.assertIn("PlayWorldReadiness", play_capture)

    def test_visual_acceptance_profile_and_launcher_are_isolated(self) -> None:
        settings = json.loads(VISUAL_SETTINGS_PATH.read_text(encoding="utf-8"))
        self.assertEqual(settings["TRIADProfile"], "VISUAL_ACCEPTANCE_ONLY")
        self.assertFalse(settings["TRIADProductionSensorProfile"])
        self.assertEqual(settings["SimMode"], "ComputerVision")
        self.assertFalse(settings["EnableRpc"])
        self.assertNotIn("DefaultSensors", settings)
        self.assertEqual(len(settings["Vehicles"]), 1)
        vehicle = next(iter(settings["Vehicles"].values()))
        self.assertEqual(vehicle["VehicleType"], "ComputerVision")
        self.assertTrue(vehicle["AutoCreate"])
        self.assertEqual(vehicle["Sensors"], {})
        self.assertNotRegex(
            VISUAL_SETTINGS_PATH.read_text(encoding="utf-8"),
            r'(?i)"SensorType"\s*:\s*6|"[^"]*lidar[^"]*"\s*:',
        )

        launcher = (
            SCRIPTS / "Start-IstanaVisualAcceptanceEditor.ps1"
        ).read_text(encoding="utf-8")
        for contract in (
            "Istana_1km_Context_v2.umap",
            "IstanaVisualAcceptance.settings.json",
            "TRIAD.uproject",
            "Get-FileHash -Algorithm SHA256",
            "Get-CimInstance Win32_Process",
            "-settings=",
            "-TRIADIstanaVisualAcceptance",
            "Start-Process",
            "$ValidateOnly",
            "EnableRpc",
            "SensorType",
        ):
            self.assertIn(contract, launcher)
        for forbidden in ("Stop-Process", "Remove-Item", "Set-Content"):
            self.assertNotIn(forbidden, launcher)

        installer = (
            SCRIPTS / "Install-IstanaDevelopmentAssets.ps1"
        ).read_text(encoding="utf-8")
        for contract in (
            "unreal\\Config\\IstanaVisualAcceptance.settings.json",
            "Config\\IstanaVisualAcceptance.settings.json",
            "sourceVisualSettingsHash",
            "installedVisualSettingsHash",
            "Refusing to overwrite a different project visual-acceptance settings file",
            "Content\\SDTH.umap",
            "SdthSha256Unchanged",
        ):
            self.assertIn(contract, installer)
        self.assertIn("-WindowStyle Normal", launcher)

    def test_complete_interior_clip_contract_is_retained(self) -> None:
        self.assertIn("Overlay->InvertSelection = true", EDITOR_CPP)
        self.assertIn("Overlay->ExcludeSelectedTiles = true", EDITOR_CPP)
        self.assertIn("Tileset->EnableFrustumCulling = false", EDITOR_CPP)
        self.assertIn("Tileset->EnableFogCulling = false", EDITOR_CPP)
        self.assertIn("Tileset->SetCreatePhysicsMeshes(true)", EDITOR_CPP)
        self.assertIn("Wgs84DestinationDegrees", EDITOR_CPP)
        self.assertIn("PolygonPointCount = 64", EDITOR_CPP)
        self.assertIn("StudyRadiusMeters = 1000.0", EDITOR_CPP)

    def test_map_policy_suppresses_only_visual_weather(self) -> None:
        self.assertIn("bSuppressAirSimVisualWeather = true", POLICY_H)
        self.assertIn("UWeatherLib::setWeatherEnabled(World, false)", POLICY_CPP)
        self.assertIn("WEATHER_PARAM_SCALAR_FOG", POLICY_CPP)
        self.assertIn(
            "ATRIADIstanaRuntimePolicyActor::ShouldSuppressAirSimVisualWeather(World)",
            MANAGER_CPP,
        )
        self.assertIn("visual weather suppressed by map runtime policy", MANAGER_CPP)
        self.assertIn("ResolveSingaporeWeather", MANAGER_CPP)

    def test_map_policy_clears_and_reads_back_every_inherited_height_fog(self) -> None:
        for contract in (
            "bSuppressInheritedExponentialHeightFog = true",
            "MaximumExponentialFogSuppressionAttempts = 8",
            "bExponentialHeightFogSuppressionVerifiedAtRuntime",
            "ExponentialHeightFogComponentCountAtRuntime",
            "IsExponentialHeightFogSuppressionActive",
        ):
            self.assertIn(contract, POLICY_H)

        apply_fog = POLICY_CPP[
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::ApplyClearExponentialHeightFogPolicy()"
            ) :
            POLICY_CPP.index(
                "void ATRIADIstanaRuntimePolicyActor::EnforceClearExponentialHeightFog()"
            )
        ]
        for contract in (
            "TActorIterator<AExponentialHeightFog>",
            "FogComponent->SetFogDensity(0.0f)",
            "FogComponent->SetSecondFogDensity(0.0f)",
            "FogComponent->SetFogMaxOpacity(0.0f)",
            "FogComponent->SetVolumetricFog(false)",
        ):
            self.assertIn(contract, apply_fog)
        for forbidden in (
            "Destroy(",
            "SetActorHiddenInGame",
            "SetVisibility(false)",
        ):
            self.assertNotIn(forbidden, apply_fog)

        readback = POLICY_CPP[
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsExponentialHeightFogSuppressionActive("
            ) :
            POLICY_CPP.index(
                "bool ATRIADIstanaRuntimePolicyActor::IsVisualAcceptanceAirSimProfileActive("
            )
        ]
        for contract in (
            "FogComponent->FogDensity == 0.0f",
            "FogComponent->SecondFogData.FogDensity == 0.0f",
            "FogComponent->FogMaxOpacity == 0.0f",
            "!FogComponent->bEnableVolumetricFog",
            "++OutComponentCount",
        ):
            self.assertIn(contract, readback)

        begin_play = POLICY_CPP[
            POLICY_CPP.index("void ATRIADIstanaRuntimePolicyActor::BeginPlay()") :
            POLICY_CPP.index("void ATRIADIstanaRuntimePolicyActor::EndPlay(")
        ]
        self.assertIn("ApplyClearExponentialHeightFogPolicy()", begin_play)
        self.assertIn("ExponentialFogSuppressionTimerHandle", begin_play)
        play_readiness = EDITOR_CPP[
            EDITOR_CPP.index("bool ValidatePlayWorldV2Readiness(") :
            EDITOR_CPP.index("bool ReloadAndValidatePersistedRuntimeMapV2(")
        ]
        self.assertIn("IsExponentialHeightFogSuppressionActive", play_readiness)
        self.assertIn("LiveFogComponentCount", play_readiness)
        self.assertIn("exact zero primary/secondary density", play_readiness)

    def test_runtime_policy_boundedly_reclaims_player0_camera(self) -> None:
        for contract in (
            "bEnforceTaggedRuntimeCameraForPlayer0 = true",
            'RequiredRuntimeCameraTag = TEXT("TRIADIstanaRuntimeV2Camera")',
            "MaximumRuntimeCameraEnforcementAttempts = 8",
            "bRuntimeCameraEnforcementComplete",
            "bRuntimeCameraViewTargetVerifiedAtRuntime",
        ):
            self.assertIn(contract, POLICY_H)
        for contract in (
            "UGameplayStatics::GetPlayerController(World, 0)",
            "PlayerController->GetWorld() != World",
            "!PlayerController->IsLocalController()",
            "PlayerController->SetViewTargetWithBlend(RuntimeCamera, 0.0f)",
            "PlayerController->GetViewTarget() == RuntimeCamera",
            "RuntimeCameraEnforcementAttempts >= MaximumRuntimeCameraEnforcementAttempts",
            "RuntimeCameraEnforcementTimerHandle",
        ):
            self.assertIn(contract, POLICY_CPP)

    def test_explicit_pie_stop_quiesces_airsim_without_stopping_itself(self) -> None:
        self.assertIn("RequestAirSimQuiescenceForTeardown", POLICY_H)
        quiesce_runtime = POLICY_CPP[
            POLICY_CPP.index("RequestAirSimQuiescenceForTeardown(") :
            POLICY_CPP.index("ConfigureMapMetadata(")
        ]
        for contract in (
            "TActorIterator<ASimModeBase>",
            "SimMode->pause(true)",
            "SimMode->isPaused()",
            "SimMode->GetWorld() != World",
            "bAirSimQuiescenceRequestedForTeardown",
        ):
            self.assertIn(contract, quiesce_runtime)

        quiesce_editor = EDITOR_CPP[
            EDITOR_CPP.index(
                "UTRIADIstanaEditorLibrary::QuiesceIstanaPlayWorldForStop("
            ) :
            EDITOR_CPP.index(
                "UTRIADIstanaEditorLibrary::PrepareIstanaRuntimeMapV2Streaming("
            )
        ]
        self.assertIn("RequestAirSimQuiescenceForTeardown", quiesce_editor)
        for forbidden in (
            "ValidateRuntimeWorldV2(",
            "EditorRequestEndPlay",
            "SaveMap(",
            "->Modify()",
        ):
            self.assertNotIn(forbidden, quiesce_editor)

    def test_runtime_weather_suppression_is_read_back_live(self) -> None:
        self.assertIn("IsVisualWeatherSuppressionActive() const", POLICY_H)
        verifier = POLICY_CPP[
            POLICY_CPP.index(
                "ATRIADIstanaRuntimePolicyActor::IsVisualWeatherSuppressionActive() const"
            ) :
            POLICY_CPP.index(
                "ATRIADIstanaRuntimePolicyActor::ConfigureMapMetadata("
            )
        ]
        self.assertIn("!UWeatherLib::getIsWeatherEnabled(World)", verifier)
        self.assertEqual(verifier.count("UWeatherLib::getWeatherParamScalar"), 8)
        for effect in (
            "WEATHER_PARAM_SCALAR_RAIN",
            "WEATHER_PARAM_SCALAR_ROADWETNESS",
            "WEATHER_PARAM_SCALAR_FOG",
            "WEATHER_PARAM_SCALAR_DUST",
            "WEATHER_PARAM_SCALAR_SNOW",
            "WEATHER_PARAM_SCALAR_ROADSNOW",
            "WEATHER_PARAM_SCALAR_MAPLELEAF",
            "WEATHER_PARAM_SCALAR_ROADLEAF",
        ):
            self.assertIn(effect, verifier)

    def test_map_policy_requires_and_runtime_verifies_persisted_wrapper(self) -> None:
        self.assertIn("bRequireIstanaAirSimGameMode = true", POLICY_H)
        self.assertIn(
            'RequiredGameModeClassPath = TEXT("/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode")',
            POLICY_H,
        )
        self.assertIn("GetAuthGameMode()", POLICY_CPP)
        self.assertIn("bGameModeOverrideVerifiedAtRuntime", POLICY_CPP)
        self.assertIn("required persisted GameMode", POLICY_CPP)
        self.assertNotIn("request-scoped", POLICY_H + POLICY_CPP + EDITOR_H)

    def test_zero_progress_recovery_is_bounded_and_secret_free(self) -> None:
        self.assertIn("MaximumTilesetRefreshAttempts = 1", POLICY_H)
        self.assertIn("GetLoadProgress() <= KINDA_SMALL_NUMBER", POLICY_CPP)
        self.assertIn("Tileset->RefreshTileset()", POLICY_CPP)
        self.assertIn("Never log its URL, token, or request details", POLICY_CPP)
        lowered = POLICY_CPP.lower()
        for forbidden in ("geturl(", "getionaccesstoken", "authorization:", "?key="):
            self.assertNotIn(forbidden, lowered)


if __name__ == "__main__":
    unittest.main()
