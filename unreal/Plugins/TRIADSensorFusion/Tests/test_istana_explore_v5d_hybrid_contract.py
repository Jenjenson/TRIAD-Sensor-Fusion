from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
RUNTIME_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h"
RUNTIME_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp"
PUBLIC_REALM_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DPublicRealmActor.cpp"
V5_APPEARANCE_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5AppearanceActor.cpp"
GROUND_VEGETATION_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DGroundVegetationActor.h"
GROUND_VEGETATION_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DGroundVegetationActor.cpp"
RUNTIME_BUILD_CS = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/TRIADSensorFusion.Build.cs"
EDITOR_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DHybridEditorLibrary.h"
EDITOR_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
PROVIDER_QUALITY_DOC = REPO / "unreal/Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md"


def extract_braced_block(source: str, marker: str) -> tuple[str, int]:
    marker_index = source.index(marker)
    open_brace = source.index("{", marker_index)
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace + 1 : index], index + 1
    raise AssertionError(f"Unterminated C++ block after {marker!r}")


class IstanaExploreV5DHybridContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = RUNTIME_H.read_text(encoding="utf-8")
        cls.runtime_cpp = RUNTIME_CPP.read_text(encoding="utf-8")
        cls.public_realm_cpp = PUBLIC_REALM_CPP.read_text(encoding="utf-8")
        cls.v5_appearance_cpp = V5_APPEARANCE_CPP.read_text(encoding="utf-8")
        cls.ground_vegetation_h = GROUND_VEGETATION_H.read_text(encoding="utf-8")
        cls.ground_vegetation_cpp = GROUND_VEGETATION_CPP.read_text(encoding="utf-8")
        cls.runtime_build_cs = RUNTIME_BUILD_CS.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor_cpp = EDITOR_CPP.read_text(encoding="utf-8")
        cls.provider_quality_doc = PROVIDER_QUALITY_DOC.read_text(encoding="utf-8")

    def test_hybrid_map_is_additive_and_never_overwrites_v5b(self) -> None:
        self.assertIn('/Game/Maps/Istana_PublicView_Explore_v5b', self.editor_cpp)
        self.assertIn('/Game/Maps/Istana_PublicView_Explore_v5d_hybrid', self.editor_cpp)
        self.assertIn('FEditorFileUtils::LoadMap(SourceFilename, true, false)', self.editor_cpp)
        self.assertIn('overwrite is refused', self.editor_cpp)
        self.assertIn('SourceBytesBefore', self.editor_cpp)

    def test_builder_quiesces_inherited_assets_before_strict_validation(self) -> None:
        self.assertIn('#include "AssetCompilingManager.h"', self.editor_cpp)
        builder_start = self.editor_cpp.index(
            'BuildIstanaExploreV5DHybridMap(FString& OutMessage)'
        )
        builder_end = self.editor_cpp.index(
            'ValidateIstanaExploreV5DHybridMap(', builder_start
        )
        builder = self.editor_cpp[builder_start:builder_end]
        source_load = builder.index(
            'UWorld* SourceWorld = UEditorLoadingAndSavingUtils::LoadMap('
        )
        source_finish = builder.index(
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            source_load,
        )
        source_validate = builder.index(
            'UTRIADIstanaExploreV5BEditorLibrary::ValidateIstanaExploreV5BMap(',
            source_finish,
        )
        self.assertLess(source_load, source_finish)
        self.assertLess(source_finish, source_validate)

        reload_target = builder.index('UWorld* ReloadedTarget =')
        cold_finish = builder.index(
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            reload_target,
        )
        cold_validate = builder.index(
            'ELandmarkPresencePolicy::Required',
            cold_finish,
        )
        self.assertLess(reload_target, cold_finish)
        self.assertLess(cold_finish, cold_validate)

    def test_public_realm_build_preflights_then_adds_one_identity_actor(self) -> None:
        builder, _ = extract_braced_block(
            self.editor_cpp,
            'BuildIstanaExploreV5DHybridMap(FString& OutMessage)',
        )
        factory_preflight = builder.index('LoadValidatedRuntimeContract(')
        source_load = builder.index(
            'UWorld* SourceWorld = UEditorLoadingAndSavingUtils::LoadMap('
        )
        duplicate = builder.index(
            'FEditorFileUtils::LoadMap(SourceFilename, true, false)'
        )
        self.assertLess(factory_preflight, source_load)
        self.assertLess(factory_preflight, duplicate)

        roster_start = builder.index('int32 ExistingPublicRealmCount = 0;')
        spawn = builder.index(
            'Target->SpawnActor<ATRIADIstanaExploreV5DPublicRealmActor>('
        )
        template_roster = builder[roster_start:spawn]
        for fragment in (
            'FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>',
            'ExistingPublicRealmCount != 0',
            'publicRealm=%d',
        ):
            self.assertIn(fragment, template_roster)

        identity = builder.index('FTransform::Identity', spawn)
        configure = builder.index('PublicRealm->ConfigurePublicRealm(', identity)
        self.assertLess(spawn, identity)
        self.assertLess(identity, configure)
        for downstream_operation in (
            'EnsureFountainRealismMaterialAssets(',
            'ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder(',
            'ConfigureCurrentSurroundingsAndOuterGroundPresentation(',
            'ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder(',
            'ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder(',
            'UEditorLoadingAndSavingUtils::SaveMap(',
            'FString ColdReport;',
        ):
            self.assertLess(configure, builder.index(downstream_operation, configure))

    def test_cesium_is_live_visual_only_and_high_detail(self) -> None:
        required = (
            'GooglePhotorealistic3DTilesIonAssetId = 2275207',
            'Tileset->SetMaximumScreenSpaceError(1.0)',
            'HybridCacheBytes = 2LL * 1024LL * 1024LL * 1024LL',
            'HybridSimultaneousLoads = 12',
            'HybridLoadingDescendantLimit = 20',
            'Tileset->ApplyDpiScaling = EApplyDpiScaling::No',
            'Tileset->ForbidHoles = true',
            'Tileset->LoadingDescendantLimit = HybridLoadingDescendantLimit',
            'Tileset->EnforceCulledScreenSpaceError = true',
            'HybridCulledScreenSpaceError = 8.0',
            'Tileset->SetUseLodTransitions(false)',
            'Tileset->ShowCreditsOnScreen = true',
            'Tileset->PreloadAncestors = true',
            'Tileset->PreloadSiblings = false',
            'Tileset->SetCreatePhysicsMeshes(false)',
            'Tileset->SetCreateNavCollision(false)',
            'Tileset->SetIgnoreKhrMaterialsUnlit(false)',
            'Tileset->SetGenerateSmoothNormals(false)',
            'cesiumStandardPersistentHttpRequestCacheAcknowledged=true',
            'triadProviderTokenReadSerializedOrLogged=false',
            'triadProviderContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false',
        )
        for fragment in required:
            self.assertIn(fragment, self.editor_cpp)
        self.assertNotIn('GetIonAccessToken', self.editor_cpp)
        self.assertNotIn('SetIonAccessToken', self.editor_cpp)

    def test_provider_quality_preset_is_exact_and_preserves_truth_boundaries(self) -> None:
        for fragment in (
            'double RequiredMaximumScreenSpaceError = 1.0;',
            'constexpr int64 RequiredProviderCacheBytes =',
            '2LL * 1024LL * 1024LL * 1024LL;',
            'constexpr int32 RequiredProviderSimultaneousLoads = 12;',
            'constexpr int32 RequiredProviderLoadingDescendantLimit = 20;',
            'Tileset->ApplyDpiScaling != EApplyDpiScaling::No',
            'constexpr double RequiredProviderCulledScreenSpaceError = 8.0;',
            'Tileset->ForbidHoles',
            'Tileset->LoadingDescendantLimit !=',
            'Tileset->EnforceCulledScreenSpaceError',
            'Tileset->GetUseLodTransitions()',
            'Tileset->MaximumCachedBytes != RequiredProviderCacheBytes',
            'RequiredProviderSimultaneousLoads',
            '!Tileset->PreloadAncestors || Tileset->PreloadSiblings',
            'preloadAncestors=true preloadSiblings=false',
            '!bCesiumLayerIsVisualOnly',
            'bCesiumCollisionNavigationSensorOrRfAuthority',
            '!bProviderContentClippedFromAuthoredCore',
            'Tileset->GetCreatePhysicsMeshes()',
            'Tileset->GetCreateNavCollision()',
        ):
            self.assertIn(fragment, self.runtime_h + self.runtime_cpp)
        self.assertNotIn('RequiredMinimumCacheBytes', self.runtime_cpp)
        self.assertNotIn('RequiredMinimumSimultaneousLoads', self.runtime_cpp)

    def test_provider_readiness_is_stable_before_fallback_removal(self) -> None:
        tick, _ = extract_braced_block(
            self.runtime_cpp,
            'void ATRIADIstanaExploreV5DContextPolicyActor::Tick(',
        )
        next_sample, _ = extract_braced_block(
            self.runtime_cpp,
            'int32 NextProviderReadyConsecutiveSamples(',
        )
        should_restore, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ShouldRestoreLocalBuildingFallback(',
        )
        for fragment in (
            'int32 RequiredProviderReadyConsecutiveSamples = 3;',
            'int32 ProviderReadyConsecutiveSamples = 0;',
        ):
            self.assertIn(fragment, self.runtime_h)
        for fragment in (
            'constexpr int32 RequiredProviderReadySamples = 3;',
            'RequiredProviderPolicyTickIntervalSeconds = 0.5f;',
            'PrimaryActorTick.TickInterval = RequiredProviderPolicyTickIntervalSeconds;',
            'PrimaryActorTick.TickInterval,',
            'NextProviderReadyConsecutiveSamples(',
            'RequiredProviderReadyConsecutiveSamples);',
            'ProviderReadyConsecutiveSamples >=',
            'SetProviderReadyPresentation(Scene, true, Error)',
            'ProviderReadyConsecutiveSamples = 0;',
            'ShouldRestoreLocalBuildingFallback(',
        ):
            self.assertIn(fragment, self.runtime_cpp)
        for fragment in (
            '!FMath::IsFinite(LoadProgress)',
            '!FMath::IsFinite(ReadyThreshold)',
            'LoadProgress < ReadyThreshold',
            'return 0;',
            'FMath::Max(CurrentSamples, 0) + 1',
            'RequiredSamples);',
        ):
            self.assertIn(fragment, next_sample)
        for fragment in (
            'bFallbackCurrentlyHidden',
            'FMath::IsFinite(LoadProgress)',
            'FMath::IsFinite(RestoreThreshold)',
            'LoadProgress < RestoreThreshold',
        ):
            self.assertIn(fragment, should_restore)
        ready_sample = tick.index('NextProviderReadyConsecutiveSamples(')
        hide_fallback = tick.index(
            'SetProviderReadyPresentation(Scene, true, Error)'
        )
        self.assertLess(ready_sample, hide_fallback)
        self.assertNotIn('above 90 m', self.editor_cpp)
        self.assertNotIn('separate aerial handoff', self.editor_cpp)
        self.assertIn('with no aerial handoff', self.editor_cpp)

    def test_provider_readiness_hysteresis_has_a_native_boundary_test(self) -> None:
        for fragment in (
            'TRIAD.Istana.ExploreV5D.Hybrid.ProviderReadinessHysteresis',
            'NextProviderReadyConsecutiveSamples(97.99f, 98.0f, Samples, 3)',
            'One interrupted observation resets the dwell',
            'Exactly three consecutive observations reach readiness',
            'ShouldRestoreLocalBuildingFallback(true, 89.99f, 90.0f)',
            'ShouldRestoreLocalBuildingFallback(true, 90.0f, 90.0f)',
            'ShouldRestoreLocalBuildingFallback(true, 95.0f, 90.0f)',
            'ShouldRestoreLocalBuildingFallback(false, 0.0f, 90.0f)',
        ):
            self.assertIn(fragment, self.runtime_cpp)

        restore, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ATRIADIstanaExploreV5DContextPolicyActor::\n'
            '    RestoreGroundLevelPresentation(',
        )
        end_play, _ = extract_braced_block(
            self.runtime_cpp,
            'void ATRIADIstanaExploreV5DContextPolicyActor::EndPlay(',
        )
        self.assertGreaterEqual(
            restore.count('ProviderReadyConsecutiveSamples = 0;'), 3
        )
        self.assertIn('ProviderReadyConsecutiveSamples = 0;', end_play)

    def test_provider_cache_telemetry_and_docs_describe_a_target_not_a_ceiling(self) -> None:
        telemetry_sources = self.runtime_cpp + self.editor_cpp
        self.assertNotIn('loadedTileMemory' + 'CacheBytes', telemetry_sources)
        self.assertIn('maximumCachedBytesSetting=%lld', self.runtime_cpp)
        self.assertIn('maximumCachedBytesSetting=%lld', self.editor_cpp)
        for fragment in (
            'exact 2 GiB `MaximumCachedBytes` setting',
            'target for evicting non-required tiles',
            'not a process-memory',
            'tiles required to render the current view may keep memory above it',
        ):
            self.assertIn(fragment, self.provider_quality_doc)

    def test_provider_quality_existing_map_migration_is_hash_pinned_scoped_and_rollback_safe(self) -> None:
        self.assertIn(
            'ApplyIstanaExploreV5DProviderQualityPassToLoadedHybridMap(',
            self.editor_h,
        )
        for fragment in (
            'constexpr int64 PreProviderQualityPassBytes = 34991378;',
            '57CA4A4C2440454E1F903ECCA166A6BBB566A2CE0E535AB26FFA62818F1D17C2',
            'ValidateProviderQualityMigrationPredecessorWorld(',
            'CreateVerifiedPreProviderQualityMapBackup(',
            'RestoreVerifiedPreProviderQualityMapViaSiblingTemp(',
        ):
            self.assertIn(fragment, self.editor_cpp)

        predecessor, _ = extract_braced_block(
            self.editor_cpp,
            'bool ValidateProviderQualityMigrationPredecessorWorld(',
        )
        for fragment in (
            'LogicalPackage != DestinationMapPackage || MapPackage->IsDirty()',
            'Tileset->ApplyDpiScaling != EApplyDpiScaling::UseProjectDefault',
            'Tileset->ForbidHoles',
            'Tileset->LoadingDescendantLimit != HybridLoadingDescendantLimit',
            'Tileset->CulledScreenSpaceError,',
            'HybridCulledScreenSpaceError,',
            'Tileset->GetGeoreference().Get() != Georeference',
            'SiteClipOverlays.Num() != 1',
            'MacDonaldHouseCount != 1',
            'TemasekShophouseCount != 1',
            'bCesiumCollisionNavigationSensorOrRfAuthority',
        ):
            self.assertIn(fragment, predecessor)

        backup, _ = extract_braced_block(
            self.editor_cpp,
            'bool CreateVerifiedPreProviderQualityMapBackup(',
        )
        self.assertIn('V5DProviderQualityPass_20260905', backup)
        self.assertIn('!IFileManager::Get().FileExists(*OutBackupFilename)', backup)
        self.assertIn('PreProviderQualityPassBytes', backup)
        self.assertIn('PreProviderQualityPassSha256', backup)

        restore, _ = extract_braced_block(
            self.editor_cpp,
            'bool RestoreVerifiedPreProviderQualityMapViaSiblingTemp(',
        )
        for fragment in (
            'FPaths::CreateTempFilename(',
            'FPaths::IsSamePath(',
            'ON_SCOPE_EXIT',
            'IFileManager::Get().Move(',
            'bDestinationRestored',
            'bBackupPreserved',
        ):
            self.assertIn(fragment, restore)

        endpoint, _ = extract_braced_block(
            self.editor_cpp,
            'ApplyIstanaExploreV5DProviderQualityPassToLoadedHybridMap(',
        )
        ordered = (
            'ValidateLocalFallbackSuppressedAsset(',
            'GetLocalFallbackSuppressedMeshObjectPath()',
            'TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::',
            'ValidateAssets(OuterGroundAssetReport)',
            'GetMeshObjectPath()',
            'ValidateProviderQualityMigrationPredecessorWorld(',
            'HashFileSha256(',
            'CreateVerifiedPreProviderQualityMapBackup(',
            'ReverifiedPredecessorReport',
            'const EApplyDpiScaling OriginalApplyDpiScaling',
            'OriginalCurrentSurroundingsMesh',
            'OriginalOuterGroundMesh',
            'Tileset->Modify();',
            'Tileset->ApplyDpiScaling = EApplyDpiScaling::No;',
            'Tileset->ForbidHoles = true;',
            'ConfigureCurrentSurroundingsAndOuterGroundPresentation(',
            'HasOnlyExpectedVisualQualitySuccessorDelta()',
            'ValidateHybridWorld(',
            'World->MarkPackageDirty();',
            'UEditorLoadingAndSavingUtils::SaveMap(',
            'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)',
            'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)',
            'EXPLORE_V5D_PROVIDER_QUALITY_APPLY_PASS',
        )
        cursor = -1
        for fragment in ordered:
            next_cursor = endpoint.index(fragment, cursor + 1)
            self.assertGreater(next_cursor, cursor)
            cursor = next_cursor
        self.assertEqual(
            endpoint.count('UEditorLoadingAndSavingUtils::SaveMap('), 1
        )
        self.assertEqual(
            endpoint.count(
                'Tileset->ApplyDpiScaling = EApplyDpiScaling::No;'
            ),
            1,
        )
        self.assertEqual(endpoint.count('Tileset->ForbidHoles = true;'), 1)
        self.assertEqual(
            endpoint.count(
                'ConfigureCurrentSurroundingsAndOuterGroundPresentation('
            ),
            1,
        )
        self.assertNotIn(
            'ConfigureCurrentSurroundingsPresentation(', endpoint
        )
        self.assertNotIn('HasOnlyExpectedProviderQualityDelta', endpoint)

        expected_delta, _ = extract_braced_block(
            endpoint,
            'const auto HasOnlyExpectedVisualQualitySuccessorDelta =',
        )
        for fragment in (
            'OriginalCurrentSurroundingsMesh->GetPathName()',
            'GetSurroundingsMeshObjectPath()',
            'OriginalOuterGroundMesh == nullptr',
            'GetStaticMesh() ==\n                SuppressedSurroundingsMesh',
            'GetStaticMesh() == OuterGroundMesh',
            'ValidateCurrentSurroundingsSuccessorForInheritedScene(',
        ):
            self.assertIn(fragment, expected_delta)

        mutation = endpoint[
            endpoint.index('Tileset->Modify();') : endpoint.index(
                'if (!HasOnlyExpectedVisualQualitySuccessorDelta())'
            )
        ]
        for forbidden in (
            'LoadingDescendantLimit =',
            'CulledScreenSpaceError =',
            'MaximumCachedBytes =',
            'MaximumSimultaneousTileLoads =',
            'SetGeoreference(',
            'SetActorTransform(',
            'Tags =',
            'SetStaticMesh(',
            'SetRelativeTransform(',
            'SetVisibility(',
            'SetHiddenInGame(',
        ):
            self.assertNotIn(forbidden, mutation)

        post_mutation = endpoint[endpoint.index('Tileset->Modify();') :]
        self.assertEqual(post_mutation.count('return false;'), 7)
        self.assertEqual(
            post_mutation.count(
                'RestoreVerifiedPredecessor(RollbackReport)'
            ),
            7,
        )
        self.assertNotIn('bool bRestoreDisk', endpoint)
        self.assertNotIn('inMemoryPredecessorRestore=', endpoint)
        for failure_marker in (
            'if (!Policy->ConfigureCurrentSurroundingsAndOuterGroundPresentation(',
            'if (!HasOnlyExpectedVisualQualitySuccessorDelta())',
            'if (!ValidateHybridWorld(\n            World,\n            PreSaveReport,',
            'if (!UEditorLoadingAndSavingUtils::SaveMap(',
            'if (!ReloadedSource || !ReloadedSourcePackage ||',
            'if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||',
            'if (!bSuccessorHasReceipt || !bBackupStillExact)',
        ):
            failure, _ = extract_braced_block(endpoint, failure_marker)
            self.assertEqual(
                failure.count(
                    'RestoreVerifiedPredecessor(RollbackReport)'
                ),
                1,
                failure_marker,
            )

        rollback, _ = extract_braced_block(
            endpoint, 'auto RestoreVerifiedPredecessor ='
        )
        disk_rollback_order = (
            rollback.index('CurrentPackage->SetDirtyFlag(false);'),
            rollback.index(
                'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)'
            ),
            rollback.index('UEditorLoadingAndSavingUtils::NewBlankMap(false)'),
            rollback.index(
                'RestoreVerifiedPreProviderQualityMapViaSiblingTemp('
            ),
            rollback.index(
                'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)'
            ),
            rollback.index('FAssetCompilingManager::Get().FinishAllCompilation();'),
            rollback.index(
                'ValidateProviderQualityMigrationPredecessorWorld(\n'
                '                RestoredTarget,'
            ),
            rollback.index('HashFileSha256(\n                DestinationFilename,'),
            rollback.index('RestoredBytes == PreProviderQualityPassBytes'),
            rollback.index('RestoredSha256 == PreProviderQualityPassSha256'),
        )
        self.assertEqual(tuple(sorted(disk_rollback_order)), disk_rollback_order)
        self.assertGreaterEqual(
            endpoint.count('ELandmarkPresencePolicy::Required'), 4
        )
        for fragment in (
            'UEditorLoadingAndSavingUtils::NewBlankMap(false)',
            'blankUnloadFallback=%s',
            'coldUnloadUsedBlankWorld=%s',
            'OriginalLoadingDescendantLimit == HybridLoadingDescendantLimit',
            'HybridCulledScreenSpaceError',
            'successorReceiptRequiresPinning=true',
            'geospatialAnchorUnchanged=true',
            'collisionNavigationSensorRfTerrainAuthority=false',
        ):
            self.assertIn(fragment, endpoint)

    def test_provider_throttle_successor_is_two_field_hash_pinned_and_rollback_safe(self) -> None:
        self.assertIn(
            'ApplyIstanaExploreV5DProviderThrottleSuccessorToLoadedHybridMap(',
            self.editor_h,
        )
        for fragment in (
            'constexpr int64 PreProviderThrottleSuccessorBytes = 34992305;',
            '053DE9DE459EAACB8B60BA9DCCEC356D40F1114CE39D0A098A04306327EE40ED',
            'constexpr int32 PreProviderThrottleSimultaneousLoads = 64;',
            'ValidateProviderThrottleMigrationPredecessorWorld(',
            'CreateVerifiedPreProviderThrottleSuccessorMapBackup(',
            'RestoreVerifiedPreProviderThrottleSuccessorMapViaSiblingTemp(',
        ):
            self.assertIn(fragment, self.editor_cpp)

        predecessor, _ = extract_braced_block(
            self.editor_cpp,
            'bool ValidateProviderThrottleMigrationPredecessorWorld(',
        )
        for fragment in (
            'LogicalPackage != DestinationMapPackage || MapPackage->IsDirty()',
            'DiskBytes != PreProviderThrottleSuccessorBytes',
            'DiskSha256 != PreProviderThrottleSuccessorSha256',
            'Tileset->MaximumSimultaneousTileLoads !=\n            PreProviderThrottleSimultaneousLoads',
            '!Tileset->PreloadSiblings',
            'Tileset->ApplyDpiScaling != EApplyDpiScaling::No',
            '!Tileset->ForbidHoles',
            'Tileset->GetGeoreference().Get() != Georeference',
            'ValidateCurrentSurroundingsSuccessorForInheritedScene(',
            'bTriadReadSerializedOrLoggedProviderToken',
            'bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent',
        ):
            self.assertIn(fragment, predecessor)

        backup, _ = extract_braced_block(
            self.editor_cpp,
            'bool CreateVerifiedPreProviderThrottleSuccessorMapBackup(',
        )
        for fragment in (
            '!IFileManager::Get().FileExists(*OutBackupFilename)',
            'PreProviderThrottleSuccessorBytes',
            'PreProviderThrottleSuccessorSha256',
        ):
            self.assertIn(fragment, backup)
        backup_name, _ = extract_braced_block(
            self.editor_cpp,
            'FString GetPreProviderThrottleSuccessorBackupFilename()',
        )
        self.assertIn('V5DProviderThrottleSuccessor_20260905', backup_name)

        restore, _ = extract_braced_block(
            self.editor_cpp,
            'bool RestoreVerifiedPreProviderThrottleSuccessorMapViaSiblingTemp(',
        )
        for fragment in (
            'FPaths::CreateTempFilename(',
            'FPaths::IsSamePath(',
            'ON_SCOPE_EXIT',
            'IFileManager::Get().Move(',
            'bDestinationRestored',
            'bBackupPreserved',
        ):
            self.assertIn(fragment, restore)

        endpoint, _ = extract_braced_block(
            self.editor_cpp,
            'ApplyIstanaExploreV5DProviderThrottleSuccessorToLoadedHybridMap(',
        )
        ordered = (
            'HashFileSha256(',
            'GetPreProviderThrottleSuccessorBackupFilename()',
            'EXPLORE_V5D_PROVIDER_THROTTLE_ALREADY_APPLIED',
            'ValidateProviderThrottleMigrationPredecessorWorld(',
            'CreateVerifiedPreProviderThrottleSuccessorMapBackup(',
            'ReverifiedPredecessorReport',
            'OriginalMaximumSimultaneousTileLoads',
            'bOriginalPreloadSiblings',
            'auto RestoreVerifiedPredecessor =',
            'const auto HasOnlyExpectedProviderThrottleSuccessorDelta =',
            'Tileset->Modify();',
            'Tileset->MaximumSimultaneousTileLoads = HybridSimultaneousLoads;',
            'Tileset->PreloadSiblings = false;',
            'HasOnlyExpectedProviderThrottleSuccessorDelta()',
            'ValidateHybridWorld(',
            'World->MarkPackageDirty();',
            'UEditorLoadingAndSavingUtils::SaveMap(',
            'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)',
            'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)',
            'EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_PASS',
        )
        cursor = -1
        for fragment in ordered:
            next_cursor = endpoint.index(fragment, cursor + 1)
            self.assertGreater(next_cursor, cursor)
            cursor = next_cursor

        self.assertEqual(
            endpoint.count('UEditorLoadingAndSavingUtils::SaveMap('), 1
        )
        mutation = endpoint[
            endpoint.index('Tileset->Modify();') : endpoint.index(
                'if (!HasOnlyExpectedProviderThrottleSuccessorDelta())'
            )
        ]
        assignments = [
            line.strip()
            for line in mutation.splitlines()
            if line.strip().startswith('Tileset->') and '=' in line
        ]
        self.assertEqual(
            assignments,
            [
                'Tileset->MaximumSimultaneousTileLoads = HybridSimultaneousLoads;',
                'Tileset->PreloadSiblings = false;',
            ],
        )
        for forbidden in (
            'RefreshTileset',
            'SetMaximumScreenSpaceError(',
            'ApplyDpiScaling =',
            'ForbidHoles =',
            'MaximumCachedBytes =',
            'LoadingDescendantLimit =',
            'SetGeoreference(',
            'SetActorTransform(',
            'Tags =',
        ):
            self.assertNotIn(forbidden, mutation)

        expected_delta, _ = extract_braced_block(
            endpoint,
            'const auto HasOnlyExpectedProviderThrottleSuccessorDelta =',
        )
        for fragment in (
            'OriginalMaximumSimultaneousTileLoads ==\n                PreProviderThrottleSimultaneousLoads',
            'Tileset->MaximumSimultaneousTileLoads ==\n                HybridSimultaneousLoads',
            'bOriginalPreloadSiblings && !Tileset->PreloadSiblings',
            'Tileset->MaximumCachedBytes == OriginalMaximumCachedBytes',
            'Tileset->PreloadAncestors == bOriginalPreloadAncestors',
            'Tileset->GetActorTransform().Equals(OriginalTransform, 0.001)',
            'Tileset->Tags == OriginalTags',
        ):
            self.assertIn(fragment, expected_delta)

        post_mutation = endpoint[endpoint.index('Tileset->Modify();') :]
        self.assertEqual(post_mutation.count('return false;'), 6)
        self.assertEqual(
            post_mutation.count('RestoreVerifiedPredecessor(RollbackReport)'),
            6,
        )
        rollback, _ = extract_braced_block(
            endpoint,
            'auto RestoreVerifiedPredecessor =',
        )
        for fragment in (
            'CurrentPackage->SetDirtyFlag(false);',
            'UEditorLoadingAndSavingUtils::NewBlankMap(false)',
            'RestoreVerifiedPreProviderThrottleSuccessorMapViaSiblingTemp(',
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            'ValidateProviderThrottleMigrationPredecessorWorld(',
            'RestoredBytes == PreProviderThrottleSuccessorBytes',
            'RestoredSha256 == PreProviderThrottleSuccessorSha256',
        ):
            self.assertIn(fragment, rollback)
        for fragment in (
            'idempotent=true',
            'exactPredecessorBackup=true',
            'mutatedProperties=MaximumSimultaneousTileLoads,PreloadSiblings',
            'simultaneousLoads=64->12',
            'preloadSiblings=true->false',
            'providerReadyGateUnchanged=true',
            'geometryExport=false',
            'successorReceiptRequiresPinning=true',
        ):
            self.assertIn(fragment, endpoint)

    def test_georeference_is_exact_and_explicit(self) -> None:
        for fragment in (
            'IstanaLongitudeDegrees = 103.84288055',
            'IstanaLatitudeDegrees = 1.30709615',
            'IstanaFallbackEllipsoidHeightMeters = 47.0',
            'SetOriginPlacement(EOriginPlacement::CartographicOrigin)',
            'Tileset->SetGeoreference',
        ):
            self.assertIn(fragment, self.editor_cpp)

    def test_provider_geometry_is_clipped_out_of_authored_core(self) -> None:
        for fragment in (
            'ExpectedProviderSiteClipSplinePoints()',
            'ExpectedProviderSiteClipPointCentimeters(Index)',
            'ACesiumCartographicPolygon',
            'UCesiumPolygonRasterOverlay',
            'SiteClipOverlay->MaterialLayerKey = TEXT("Clipping")',
            'SiteClipOverlay->InvertSelection = false',
            'SiteClipOverlay->ExcludeSelectedTiles = true',
            'CesiumDefaultGeoreferenceTag(TEXT("DEFAULT_GEOREFERENCE"))',
            'Georeference->Tags.AddUnique(CesiumDefaultGeoreferenceTag)',
            'ACesiumGeoreference::GetDefaultGeoreference(Target)',
            'SiteClip->SetActorTransform(',
            'ETeleportType::TeleportPhysics',
            'SiteClip->GlobeAnchor->Sync()',
            'SiteClip->GlobeAnchor->GetResolvedGeoreference()',
            'SiteClip->Polygon->SetSplinePointType(',
            'ESplinePointType::Linear',
            'MI_CesiumThreeOverlaysAndClipping',
            'MI_CesiumThreeOverlaysAndClippingTranslucent',
            'compact deterministic 64-vertex irregular-ellipse visual core',
        ):
            self.assertIn(fragment, self.editor_cpp)
        for fragment in (
            'bProviderContentClippedFromAuthoredCore = true',
            'ProviderSiteClipCenterMeters = FVector2D(0.0, 55.0)',
            'ProviderSiteClipSemiAxesMeters = FVector2D(185.0, 245.0)',
            'ProviderSiteClipSplinePoints = 64',
            'ExpectedProviderSiteClipRippleAmplitudes',
            'EvaluateProviderSiteClipSignedInwardDistanceCentimeters',
            'ValidateProviderSiteClip',
        ):
            self.assertIn(fragment, self.runtime_h + self.runtime_cpp)

    def test_suppressed_current_surroundings_and_outer_ground_are_exact_render_only_context(self) -> None:
        for fragment in (
            '/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV1/',
            'SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1',
            'RenderData->LODResources.Num() != 1',
            'GetNumTriangles() != 43492',
            'GetNumVertices() == 0',
            'Sections.Num() != 17',
            'GetNumTexCoords() != 1',
            'Mesh->HasValidNaniteData()',
            'Mesh->NaniteSettings.bEnabled',
            'Mesh->NaniteSettings.KeepPercentTriangles',
            'Mesh->NaniteSettings.TrimRelativeError',
            'Body->AggGeom.GetElementCount() != 0',
            'UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance',
            'LegacyProvenanceCount != 0',
            'Provenance->IsCanonicalContract()',
            'ProvenanceCount != 1',
            'bCurrentSurroundingsRenderOnly = true',
            'bCurrentSurroundingsCollisionNavigationSensorOrRfAuthority = false',
            'bCurrentSurroundingsMeasuredSurveyAsBuiltOrHyperreal = false',
            'TRIADV5DCurrentPublicContextRenderOnly',
            'Component->ComponentTags.Contains(TEXT("TRIADHumanOnlyOverlay"))',
            'RootComponent->SetMobility(EComponentMobility::Static)',
            'RootComponent->Mobility != EComponentMobility::Static',
            'GetActorTransform().Equals(FTransform::Identity',
            'GetComponentTransform().Equals(FTransform::Identity',
            'Component->bRenderInMainPass',
            'Component->GetNumOverrideMaterials() != 0',
            'Component->GetOverlayMaterial() != nullptr',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/',
            'SM_IPV5D_OuterGroundLoadingFallback_Render',
            'M_IPV5D_OuterGroundLoadingFallback',
            'GetNumTriangles() != 1280',
            'OuterGroundLoadingFallbackSourceCornerCount = 3840',
            'OuterGroundLoadingFallbackRenderVertexCount = 768',
            'GetNumVertices() !=\n            OuterGroundLoadingFallbackRenderVertexCount',
            'UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance',
            'OuterGroundLoadingFallbackRenderOnlyComponent',
            'TRIADV5DOuterGroundLoadingFallbackRenderOnly',
            'bOuterGroundLoadingFallbackRenderOnly = true',
            'bOuterGroundLoadingFallbackCollisionNavigationSensorRfTerrainAuthority = false',
            'bOuterGroundLoadingFallbackSurveyAsBuilt = false',
            'OuterComponent->GetOverlayMaterial() != nullptr',
            'const bool bExpectedVisible = !bLocalBuildingFallbackCurrentlyHidden',
        ):
            self.assertIn(fragment, self.runtime_h + self.runtime_cpp)
        self.assertNotIn('GetNumTriangles() != 43544', self.runtime_cpp)

    def test_outer_ground_imported_slot_is_editor_only_but_runtime_contract_stays_strict(self) -> None:
        validator, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ValidateOuterGroundLoadingFallbackMesh(',
        )
        declaration = validator.index('bool bImportedSlotNameMatches = true;')
        editor_guard = validator.index('#if WITH_EDITORONLY_DATA', declaration)
        imported_check = validator.index(
            'Material.ImportedMaterialSlotName ==', editor_guard
        )
        guard_end = validator.index('#endif', imported_check)
        runtime_check = validator.index('!bImportedSlotNameMatches', guard_end)
        self.assertLess(declaration, editor_guard)
        self.assertLess(editor_guard, imported_check)
        self.assertLess(imported_check, guard_end)
        self.assertLess(guard_end, runtime_check)
        self.assertEqual(validator.count('ImportedMaterialSlotName'), 1)
        for retained_runtime_authority in (
            'Material.MaterialSlotName !=',
            'Interface->GetPathName() !=',
            'Section.MaterialIndex != 0',
            'Section.NumTriangles != 1280',
            'Section.bEnableCollision',
            'Section.bCastShadow',
            'Section.bAffectDistanceFieldLighting',
        ):
            self.assertIn(retained_runtime_authority, validator)

        successor_materials = (
            ('MAT_BOTTOM_HIDDEN', 9446),
            ('MAT_COMMERCIAL_HINT', 2166),
            ('MAT_GENERIC_BUILDING_HINT', 14696),
            ('MAT_HEALTHCARE_HINT', 278),
            ('MAT_HOTEL_HINT', 414),
            ('MAT_INDUSTRIAL_HINT', 8),
            ('MAT_RELIGIOUS_HINT', 150),
            ('MAT_RESIDENTIAL_HINT', 6340),
            ('MAT_ROOF_COMMERCIAL_HINT', 1029),
            ('MAT_ROOF_GENERIC_BUILDING_HINT', 5452),
            ('MAT_ROOF_HEALTHCARE_HINT', 131),
            ('MAT_ROOF_HOTEL_HINT', 169),
            ('MAT_ROOF_INDUSTRIAL_HINT', 2),
            ('MAT_ROOF_RELIGIOUS_HINT', 53),
            ('MAT_ROOF_RESIDENTIAL_HINT', 2694),
            ('MAT_ROOF_TRANSPORT_HINT', 132),
            ('MAT_TRANSPORT_HINT', 332),
        )
        self.assertEqual(sum(count for _, count in successor_materials), 43492)
        for slot_name, triangle_count in successor_materials:
            self.assertIn(
                f'{{TEXT("{slot_name}"), {triangle_count},',
                self.runtime_cpp,
            )

        builder_start = self.editor_cpp.index(
            'BuildIstanaExploreV5DHybridMap(FString& OutMessage)'
        )
        builder_end = self.editor_cpp.index(
            'ValidateIstanaExploreV5DHybridMap(', builder_start
        )
        builder = self.editor_cpp[builder_start:builder_end]
        self.assertIn(
            'TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::\n'
            '            ValidateLocalFallbackSuppressedAsset(',
            builder,
        )
        self.assertIn(
            'TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::\n'
            '            ValidateAssets(',
            builder,
        )
        self.assertGreaterEqual(builder.count('LoadObject<UStaticMesh>'), 2)
        self.assertIn(
            'ConfigureCurrentSurroundingsAndOuterGroundPresentation(', builder
        )
        self.assertNotIn('ConfigureCurrentSurroundingsPresentation(', builder)
        self.assertNotIn(
            'TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::ValidateAsset(',
            builder,
        )
        suppressed_preflight = builder.index(
            'ValidateLocalFallbackSuppressedAsset('
        )
        outer_preflight = builder.index('ValidateAssets(OuterGroundAssetReport)')
        source_duplicate = builder.index(
            'FEditorFileUtils::LoadMap(SourceFilename, true, false)'
        )
        self.assertLess(suppressed_preflight, outer_preflight)
        self.assertLess(outer_preflight, source_duplicate)
        self.assertLess(
            suppressed_preflight,
            source_duplicate,
        )
        self.assertLess(
            builder.index('ConfigureFountainRealism('),
            builder.index(
                'ConfigureCurrentSurroundingsAndOuterGroundPresentation('
            ),
        )
        self.assertLess(
            builder.index(
                'ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder('
            ),
            builder.index(
                'ConfigureCurrentSurroundingsAndOuterGroundPresentation('
            ),
        )
        self.assertLess(
            builder.index(
                'ConfigureCurrentSurroundingsAndOuterGroundPresentation('
            ),
            builder.index('UEditorLoadingAndSavingUtils::SaveMap('),
        )
        self.assertIn(
            '#include "TRIADIstanaExploreV5DContextPolicyActor.h"',
            self.v5_appearance_cpp,
        )
        self.assertIn(
            'ValidateInheritedSceneOrExactV5DCurrentSuccessor(',
            self.v5_appearance_cpp,
        )
        self.assertIn(
            'Policy->ValidateCurrentSurroundingsSuccessorForInheritedScene(',
            self.v5_appearance_cpp,
        )
        self.assertIn(
            'Policy->ValidateCurrentSurroundingsV2SuccessorForInheritedScene(',
            self.v5_appearance_cpp,
        )
        self.assertIn(
            'ValidateCurrentSurroundingsSuccessorForInheritedScene(',
            self.runtime_h,
        )
        self.assertIn(
            'ISTANA_EXPLORE_V5D_CURRENT_SURROUNDINGS_SUCCESSOR_VALID',
            self.runtime_cpp,
        )

    def test_runtime_fallback_preserves_buildings_and_suppresses_v5c_planning_ground(self) -> None:
        fallback_start = self.runtime_cpp.index(
            'void ATRIADIstanaExploreV5DContextPolicyActor::\n'
            '    SetLocalBuildingFallbackVisible('
        )
        fallback_end = self.runtime_cpp.index(
            'bool ATRIADIstanaExploreV5DContextPolicyActor::ValidateHybridContext(',
            fallback_start,
        )
        fallback = self.runtime_cpp[fallback_start:fallback_end]
        self.assertIn(
            'Scene->IsV5CSurroundingsPresentationActive()',
            fallback,
        )
        self.assertIn(
            'SetRendererVisible(Scene->ContextBuildingsComponent, false)',
            fallback,
        )
        self.assertIn(
            'bVisible && !bUseCurrentSurroundings && !bUseV5CSurroundings',
            fallback,
        )
        self.assertIn(
            'bVisible && bUseV5CSurroundings',
            fallback,
        )
        self.assertIn(
            'bVisible && bUseCurrentSurroundings',
            fallback,
        )
        self.assertIn(
            'SuppressInheritedPlanningGroundPresentation(\n'
            '        Scene,\n'
            '        PlanningGroundError)',
            fallback,
        )
        self.assertIn(
            '!IsComponentHidden(Scene->V5CGroundContextRenderOnlyComponent)',
            fallback,
        )
        hidden_validator, _ = extract_braced_block(
            fallback,
            'const auto IsComponentHidden =',
        )
        self.assertRegex(
            hidden_validator,
            r'return\s+Candidate\s*&&\s*!Candidate->IsVisible\(\)\s*&&\s*'
            r'Candidate->bHiddenInGame\s*;',
        )
        self.assertNotIn('!Candidate ||', hidden_validator)
        suppression, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ATRIADIstanaExploreV5DContextPolicyActor::\n'
            '    SuppressInheritedPlanningGroundPresentation(',
        )
        for fragment in (
            'Scene->V5CGroundContextRenderOnlyComponent',
            'PlanningGround->SetVisibility(false, true)',
            'PlanningGround->SetHiddenInGame(true, true)',
            'PlanningGround->IsVisible() || !PlanningGround->bHiddenInGame',
        ):
            self.assertIn(fragment, suppression)
        for forbidden_mutation in (
            'SetActive(',
            'bAutoActivate',
            'SetStaticMesh(',
            'SetRelativeTransform(',
            'SetCollision',
            'SetCanEverAffectNavigation(',
            'SetMaterial(',
        ):
            self.assertNotIn(forbidden_mutation, suppression)
        self.assertIn('inheritedV5CPlanningGroundHidden=true', self.runtime_cpp)
        for forbidden_mutation in (
            'SetActive(',
            'Activate(',
            'Deactivate(',
            'bAutoActivate',
            'SetStaticMesh(',
            'SetRelativeTransform(',
            'SetCollision',
            'SetCanEverAffectNavigation(',
        ):
            self.assertNotIn(forbidden_mutation, fallback)
        self.assertIn('LoadProgress >= HideLocalBuildingFallbackAtLoadProgress', self.runtime_cpp)
        self.assertIn('ShouldRestoreLocalBuildingFallback(', self.runtime_cpp)
        self.assertIn(
            'RestoreLocalBuildingFallbackBelowLoadProgress) &&',
            self.runtime_cpp,
        )
        self.assertIn('SetLocalBuildingFallbackVisible(CachedScene.Get(), true)', self.runtime_cpp)

    def test_provider_readiness_atomically_couples_both_fallbacks(self) -> None:
        transition, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ATRIADIstanaExploreV5DContextPolicyActor::\n'
            '    SetProviderReadyPresentation(',
        )
        for fragment in (
            'const bool bPreviousProviderReady = PublicRealm->bProviderReady;',
            'const bool bPreviousLocalFallbackHidden =',
            'PublicRealm->SetProviderReady(bProviderReady, PublicRealmError)',
            'SetLocalBuildingFallbackVisible(Scene, !bProviderReady);',
            'bLocalBuildingFallbackCurrentlyHidden = bProviderReady;',
            'ValidateCurrentSurroundingsPresentation(',
            'PublicRealm->ValidatePublicRealm(PublicRealmReport)',
            'PublicRealm->bProviderReady !=',
            'bLocalBuildingFallbackCurrentlyHidden',
            'bPreviousProviderReady',
            'bPreviousLocalFallbackHidden',
            'lost atomic current-context/public-realm coherence and was restored',
        ):
            self.assertIn(fragment, transition)

        mutate_public = transition.index(
            'PublicRealm->SetProviderReady(bProviderReady, PublicRealmError)'
        )
        mutate_local = transition.index(
            'SetLocalBuildingFallbackVisible(Scene, !bProviderReady);'
        )
        validate = transition.index(
            'ValidateCurrentSurroundingsPresentation(', mutate_local
        )
        rollback_public = transition.index(
            'PublicRealm->SetProviderReady(\n'
            '            bPreviousProviderReady',
            validate,
        )
        rollback_local = transition.index(
            'SetLocalBuildingFallbackVisible(\n'
            '            Scene,\n'
            '            !bPreviousLocalFallbackHidden)',
            rollback_public,
        )
        self.assertLess(mutate_public, mutate_local)
        self.assertLess(mutate_local, validate)
        self.assertLess(validate, rollback_public)
        self.assertLess(rollback_public, rollback_local)

        tick, _ = extract_braced_block(
            self.runtime_cpp,
            'void ATRIADIstanaExploreV5DContextPolicyActor::Tick(',
        )
        ready_threshold = tick.index(
            'LoadProgress >= HideLocalBuildingFallbackAtLoadProgress'
        )
        ready_transition = tick.index(
            'SetProviderReadyPresentation(Scene, true, Error)',
            ready_threshold,
        )
        restore_threshold = tick.index(
            'ShouldRestoreLocalBuildingFallback('
        )
        restore_transition = tick.index(
            'SetProviderReadyPresentation(Scene, false, Error)',
            restore_threshold,
        )
        self.assertLess(ready_threshold, ready_transition)
        self.assertLess(ready_transition, restore_threshold)
        self.assertLess(restore_threshold, restore_transition)

        restore, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ATRIADIstanaExploreV5DContextPolicyActor::\n'
            '    RestoreGroundLevelPresentation(',
        )
        self.assertIn(
            'SetProviderReadyPresentation(\n'
            '            Scene,\n'
            '            false,',
            restore,
        )
        self.assertIn('SetLocalBuildingFallbackVisible(Scene, true);', restore)

    def test_public_realm_is_in_the_whole_actor_stable_visual_roster(self) -> None:
        roster, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ATRIADIstanaExploreV5DContextPolicyActor::\n'
            '    ResolveAuthoredCoreVisualActors(',
        )
        for fragment in (
            'int32 PublicRealmCount = 0;',
            'FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>',
            'TreeCount != 1 || PublicRealmCount != 1',
            'publicRealm=%d',
            'Tree,\n        PublicRealm};',
        ):
            self.assertIn(fragment, roster)

        validation, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ATRIADIstanaExploreV5DContextPolicyActor::ValidateHybridContext(',
        )
        self.assertIn(
            'ResolveAuthoredCoreVisualActors(VisualActors, Error)', validation
        )
        self.assertIn('Actor->IsHidden()', validation)
        self.assertIn('bAuthoredCoreVisualsCurrentlyVisible', validation)

    def test_stable_visual_policy_has_no_authored_core_visibility_transaction(self) -> None:
        validation, _ = extract_braced_block(
            self.runtime_cpp,
            'bool ATRIADIstanaExploreV5DContextPolicyActor::ValidateHybridContext(',
        )
        for stable_tuple_fragment in (
            'bProviderSiteClipCurrentlyActive',
            'bAuthoredCoreVisualsCurrentlyVisible',
            '!bAerialProviderHandoffRequested',
            '!bAerialProviderHandoffCurrentlyActive',
            'AerialProviderReadySamples == 0',
            'Overlay->IsActive()',
            'Actor->IsHidden()',
        ):
            self.assertIn(stable_tuple_fragment, validation)

        for runtime_toggle in (
            'SetAuthoredCoreVisualsVisible(',
            'SetProviderSiteClipActive(',
            '->Activate(',
            '->Deactivate(',
        ):
            self.assertNotIn(runtime_toggle, self.runtime_h + self.runtime_cpp)

    def test_public_realm_is_scene_capture_excluded_but_not_a_human_overlay(self) -> None:
        component_config, _ = extract_braced_block(
            self.public_realm_cpp,
            'void ConfigureRenderOnlyComponent(',
        )
        component_validation, _ = extract_braced_block(
            self.public_realm_cpp,
            'bool ValidateRenderOnlyComponent(',
        )
        constructor, _ = extract_braced_block(
            self.public_realm_cpp,
            'ATRIADIstanaExploreV5DPublicRealmActor()',
        )
        configure, _ = extract_braced_block(
            self.public_realm_cpp,
            'bool ATRIADIstanaExploreV5DPublicRealmActor::ConfigurePublicRealm(',
        )
        provider_transition, _ = extract_braced_block(
            self.public_realm_cpp,
            'bool ATRIADIstanaExploreV5DPublicRealmActor::SetProviderReady(',
        )
        validation, _ = extract_braced_block(
            self.public_realm_cpp,
            'bool ATRIADIstanaExploreV5DPublicRealmActor::ValidatePublicRealm(',
        )

        self.assertIn('Component->bHiddenInSceneCapture = true;', component_config)
        self.assertIn(
            'Component->ComponentTags.Remove(HumanOnlyOverlayTag);',
            component_config,
        )
        self.assertIn('!Component->bHiddenInSceneCapture', component_validation)
        self.assertIn(
            'Component->ComponentTags.Contains(HumanOnlyOverlayTag)',
            component_validation,
        )
        self.assertIn('Tags.Remove(HumanOnlyOverlayTag);', constructor)
        self.assertIn('Tags.Remove(HumanOnlyOverlayTag);', configure)
        self.assertIn('Tags.Contains(HumanOnlyOverlayTag)', validation)
        self.assertNotIn('Tags.AddUnique(HumanOnlyOverlayTag)', self.public_realm_cpp)

        for fragment in (
            'CorePublicRealmRenderOnly->SetVisibility(true, true);',
            'CorePublicRealmRenderOnly->SetHiddenInGame(false, true);',
            'FallbackPublicRealmRenderOnly->SetVisibility(true, true);',
            'FallbackPublicRealmRenderOnly->SetHiddenInGame(false, true);',
            'bProviderReady = false;',
        ):
            self.assertIn(fragment, configure)
        self.assertIn(
            'FallbackPublicRealmRenderOnly->SetVisibility(!bInProviderReady, true);',
            provider_transition,
        )
        self.assertNotIn(
            'CorePublicRealmRenderOnly->SetVisibility(', provider_transition
        )
        self.assertIn(
            'CoreRenderOnlyTag,\n            true,',
            validation,
        )
        self.assertIn(
            'FallbackRenderOnlyTag,\n            !bProviderReady,',
            validation,
        )
        for no_authority_fragment in (
            'SetActorEnableCollision(false);',
            'GetActorEnableCollision()',
            '!bRenderOnly',
            'bCollisionOrNavigationAuthority',
            'bSensorOrRfMaterialAuthority',
        ):
            self.assertIn(no_authority_fragment, self.public_realm_cpp)

    def test_stable_visual_policy_has_no_altitude_handoff_or_toggle(self) -> None:
        for fragment in (
            'bProviderSiteClipCurrentlyActive = true',
            'bAuthoredCoreVisualsCurrentlyVisible = true',
            'bAerialProviderHandoffRequested = false',
            'bAerialProviderHandoffCurrentlyActive = false',
            'AerialProviderReadySamples = 0',
            'bStableVisualPolicyTuple',
            'FindExactlyOne<ATRIADIstanaPublicViewSceneActor>',
            'FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>',
            'FindExactlyOne<ATRIADIstanaExploreV5DTreeRealismActor>',
        ):
            self.assertIn(fragment, self.runtime_h + self.runtime_cpp)

        overlay_start = self.editor_cpp.index(
            'NewObject<UCesiumPolygonRasterOverlay>('
        )
        overlay_end = self.editor_cpp.index(
            'SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DContextPolicy")',
            overlay_start,
        )
        overlay_setup = self.editor_cpp[overlay_start:overlay_end]
        self.assertLess(
            overlay_setup.index('SiteClipOverlay->SetAutoActivate(true)'),
            overlay_setup.index('SiteClipOverlay->RegisterComponent()'),
        )
        self.assertNotIn('SiteClipOverlay->Activate(', overlay_setup)

        tick_start = self.runtime_cpp.index(
            'void ATRIADIstanaExploreV5DContextPolicyActor::Tick('
        )
        tick_end = self.runtime_cpp.index(
            'void ATRIADIstanaExploreV5DContextPolicyActor::EndPlay(', tick_start
        )
        tick = self.runtime_cpp[tick_start:tick_end]
        self.assertIn('SetProviderReadyPresentation(', tick)
        for forbidden_toggle_or_altitude_gate in (
            'GetPlayerViewPoint(',
            'ShouldRequestAerialProviderHandoff(',
            'SetActorHiddenInGame(',
            '->Activate(',
            '->Deactivate(',
        ):
            self.assertNotIn(forbidden_toggle_or_altitude_gate, tick)
        self.assertNotIn('SetActorTransform', tick)
        self.assertNotIn('SetActorEnableCollision', tick)

    def test_quit_only_cesium_shutdown_drain_is_ordered_and_non_destructive(self) -> None:
        public_dependencies, _ = extract_braced_block(
            self.runtime_build_cs,
            'PublicDependencyModuleNames.AddRange(new[]',
        )
        private_dependencies, _ = extract_braced_block(
            self.runtime_build_cs,
            'PrivateDependencyModuleNames.AddRange(new[]',
        )
        self.assertNotIn('"HTTP"', public_dependencies)
        self.assertEqual(private_dependencies.count('"HTTP"'), 1)

        end_play, _ = extract_braced_block(
            self.runtime_cpp,
            'void ATRIADIstanaExploreV5DContextPolicyActor::EndPlay(',
        )
        quit_condition = 'if (EndPlayReason == EEndPlayReason::Quit)'
        quit_condition_index = end_play.index(quit_condition)
        quit_drain, quit_block_end = extract_braced_block(end_play, quit_condition)

        native_capture_index = quit_drain.index(
            'NativeTileset->getExternals().pLogger'
        )
        cesium_scope_index = quit_drain.index(
            'FScopedCesiumQuitLoggerLevel CesiumLogGuard('
        )
        http_scope_index = quit_drain.index(
            'FLogScopedVerbosityOverride HttpLogGuard('
        )
        suspend_index = quit_drain.index('Tileset->SuspendUpdate = true;')
        refresh_index = quit_drain.index('Tileset->RefreshTileset();')
        flush_index = quit_drain.index(
            'FHttpModule::Get().GetHttpManager().Flush('
        )
        flush_reason_index = quit_drain.index(
            'EHttpFlushReason::Default', flush_index
        )
        ready_index = quit_drain.index(
            'bQuitDrainReady = Tileset->IsReadyForFinishDestroy();'
        )
        self.assertLess(native_capture_index, cesium_scope_index)
        self.assertLess(cesium_scope_index, http_scope_index)
        self.assertLess(http_scope_index, suspend_index)
        self.assertLess(suspend_index, refresh_index)
        self.assertLess(refresh_index, flush_index)
        self.assertLess(flush_index, flush_reason_index)
        self.assertLess(flush_reason_index, ready_index)

        for quit_only_operation in (
            'Tileset->SuspendUpdate = true;',
            'Tileset->RefreshTileset();',
            'FHttpModule::Get().GetHttpManager().Flush(',
            'bQuitDrainReady = Tileset->IsReadyForFinishDestroy();',
        ):
            self.assertNotIn(
                quit_only_operation,
                end_play[:quit_condition_index],
            )
        self.assertEqual(
            end_play.count('Tileset->IsReadyForFinishDestroy();'), 1
        )
        self.assertEqual(end_play.count('CachedTileset.Reset();'), 1)
        self.assertGreater(
            end_play.index('CachedTileset.Reset();'),
            quit_block_end,
        )

        for bounded_drain_contract in (
            'constexpr double QuitDrainTimeoutSeconds = 5.0;',
            'FPlatformTime::Seconds()',
            'FPlatformProcess::SleepNoStats(0.001f);',
            'spdlog::level::critical',
            'Logger->set_level(OriginalLevel);',
            'ISTANA_EXPLORE_V5D_CESIUM_QUIT_DRAIN_PASS',
            'ready=true nativeTilesetPresent=true elapsedMs=%.3f',
            'ISTANA_EXPLORE_V5D_CESIUM_QUIT_DRAIN_FAIL',
        ):
            self.assertIn(bounded_drain_contract, self.runtime_cpp)

        self.assertEqual(
            self.runtime_cpp.count(
                'ISTANA_EXPLORE_V5D_CESIUM_QUIT_DRAIN_PASS'
            ),
            1,
        )
        self.assertIn('&LogHttp,', quit_drain)
        self.assertIn('ELogVerbosity::Error', quit_drain)
        self.assertNotIn('EHttpFlushReason::FullFlush', self.runtime_cpp)
        self.assertNotIn('FHttpModule::Get().ShutdownModule()', self.runtime_cpp)

        for fidelity_reducing_shutdown_shortcut in (
            'Tileset->Destroy()',
            'Tileset->SetActorHiddenInGame(true)',
            'Tileset->SetActorTickEnabled(false)',
            'Tileset->SetMaximumScreenSpaceError(',
        ):
            self.assertNotIn(fidelity_reducing_shutdown_shortcut, end_play)

    def test_high_view_qa_requires_stable_visual_policy_without_altitude_gate(self) -> None:
        for fragment in (
            'bool HasStableHybridVisualPolicy(',
            'Policy->bProviderSiteClipCurrentlyActive',
            'Policy->bAuthoredCoreVisualsCurrentlyVisible',
            '!Policy->bAerialProviderHandoffRequested',
            '!Policy->bAerialProviderHandoffCurrentlyActive',
            'Policy->AerialProviderReadySamples == 0',
            'bStableVisualPolicy',
            'providerReadyForProof=%s',
            'stableVisualPolicy=%s',
            'providerSiteClipActive=%s',
            'authoredCoreVisualsVisible=%s',
        ):
            self.assertIn(fragment, self.editor_cpp)

        capture_start = self.editor_cpp.index(
            'CaptureIstanaExploreV5DHybridPlayView('
        )
        capture = self.editor_cpp[capture_start:]
        policy_guard = capture.index('!bStableVisualPolicy')
        screenshot_request = capture.index('Viewport->TakeHighResScreenShot()')
        self.assertLess(policy_guard, screenshot_request)
        self.assertNotIn('ShouldRequestAerialProviderHandoff(', capture)
        self.assertNotIn('ViewLocation.Z', capture)

    def test_truth_contract_is_fail_closed(self) -> None:
        for fragment in (
            'bCesiumLayerIsVisualOnly = true',
            'bCesiumCollisionNavigationSensorOrRfAuthority = false',
            'bCesiumStandardPersistentHttpRequestCacheAcknowledged = true',
            'bTriadReadSerializedOrLoggedProviderToken = false',
            'bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent = false',
            'bProviderContentClippedFromAuthoredCore = true',
            'ValidateHybridContext',
        ):
            self.assertIn(fragment, self.runtime_h)
        self.assertIn('bCesiumCollisionNavigationSensorOrRfAuthority ||', self.runtime_cpp)
        self.assertIn(
            '!bCesiumStandardPersistentHttpRequestCacheAcknowledged ||',
            self.runtime_cpp,
        )
        self.assertIn('bTriadReadSerializedOrLoggedProviderToken ||', self.runtime_cpp)
        self.assertIn(
            'bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent ||',
            self.runtime_cpp,
        )
        self.assertIn('!bProviderContentClippedFromAuthoredCore ||', self.runtime_cpp)

    def test_public_realm_has_exact_cold_and_pie_roster_validation(self) -> None:
        cold_validation, _ = extract_braced_block(
            self.editor_cpp,
            'bool ValidateHybridWorld(',
        )
        pie_validation, _ = extract_braced_block(
            self.editor_cpp,
            'bool GetValidatedHybridPlayState(',
        )

        for validation in (cold_validation, pie_validation):
            self.assertIn('int32 PublicRealmCount = 0;', validation)
            self.assertEqual(
                validation.count(
                    'FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>'
                ),
                1,
            )
            self.assertIn('PublicRealmCount != 1', validation)
            self.assertIn('!PublicRealm', validation)
            self.assertIn(
                'ATRIADIstanaExploreV5DPublicRealmActor::ExpectedActorTag()',
                validation,
            )
            self.assertIn(
                'PublicRealm->ValidatePublicRealm(PublicRealmReport)',
                validation,
            )
            self.assertIn('publicRealm=%d', validation)
            self.assertIn('publicRealm={%s}', validation)
            self.assertIn('*PublicRealmReport', validation)

        self.assertIn('PublicRealm->HasActorBegunPlay()', pie_validation)
        self.assertNotIn('PublicRealm->HasActorBegunPlay()', cold_validation)
        self.assertIn(
            'ISTANA_EXPLORE_V5D_HYBRID_MAP_VALID', cold_validation
        )
        self.assertIn(
            'runtime actors, finite stream progress', pie_validation
        )

    def test_editor_api_exposes_bounded_build_validation_and_safe_visual_qa(self) -> None:
        for api in (
            'BuildIstanaExploreV5DHybridMap',
            'ApplyIstanaExploreV5DPublicRealmPassToLoadedHybridMap',
            'ApplyIstanaExploreV5DPublicRealmMaterialCorrectionToLoadedHybridMap',
            'ApplyIstanaExploreV5DInheritedPlanningGroundSuppressionToLoadedHybridMap',
            'ValidateIstanaExploreV5DHybridMap',
            'ValidateIstanaExploreV5DHybridPlayWorld',
            'EnsureIstanaExploreV5DFountainRealismMaterialAssets',
            'UpgradeIstanaExploreV5DRequiredNaniteMaterialUsage',
            'ValidateIstanaExploreV5DRequiredNaniteMaterialUsage',
            'GetIstanaExploreV5DHybridPlayStateReport',
            'TeleportIstanaExploreV5DHybridPlayPawnForQa',
            'SetIstanaExploreV5DHybridPlayViewPoseForQa',
            'CaptureIstanaExploreV5DHybridPlayView',
            'CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView',
            'CaptureIstanaExploreV5DHybridPublicRealmCoreHiddenDiagnosticPlayView',
            'CaptureIstanaExploreV5DHybridGroundMacroOverlayHiddenDiagnosticPlayView',
            'CaptureIstanaExploreV5DHybridDiagnosticPlayView',
        ):
            self.assertIn(api, self.editor_h)
        self.assertNotIn(
            'CaptureIstanaExploreV5DHybridProviderSiteClipInactiveDiagnosticPlayView',
            self.editor_h + self.editor_cpp,
        )
        self.assertNotIn('Import', self.editor_h)
        self.assertNotIn('Export', self.editor_h)

    def test_public_realm_existing_map_migration_is_hash_pinned_and_cold_validated(self) -> None:
        for pinned_contract in (
            'constexpr int64 PrePublicRealmDestinationBytes = 37409759;',
            '56A2A3B32457C3E34D74CA13C131C5420C0582BBC183BF0B891D0F760E85B842',
        ):
            self.assertIn(pinned_contract, self.editor_cpp)

        backup, _ = extract_braced_block(
            self.editor_cpp,
            'bool CreateVerifiedPrePublicRealmMapBackup(',
        )
        for fragment in (
            'TRIAD/MapBackups/V5DPublicRealmMigration_20260831',
            'Istana_PublicView_Explore_v5d_hybrid_56A2A3B3.umap',
            'IFileManager::Get().MakeDirectory(*BackupDirectory, true)',
            'IFileManager::Get().FileExists(*OutBackupFilename)',
            'IFileManager::Get().Copy(',
            'false,\n            true) != COPY_OK',
            'HashFileSha256(',
            'BackupBytes != PrePublicRealmDestinationBytes',
            'BackupSha256 != PrePublicRealmDestinationSha256',
            'non-overwriting V5D public-realm map backup',
        ):
            self.assertIn(fragment, backup)

        migration, _ = extract_braced_block(
            self.editor_cpp,
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::\n'
            '    ApplyIstanaExploreV5DPublicRealmPassToLoadedHybridMap(',
        )
        for fragment in (
            'LogicalPackage != DestinationMapPackage',
            'ExistingPublicRealmCount == 1 && !MapPackage->IsDirty()',
            'IDEMPOTENT_EXPLORE_V5D_PUBLIC_REALM_PASS_ALREADY_VALID',
            'ExistingPublicRealmCount != 0 || MapPackage->IsDirty()',
            'DestinationBytes != PrePublicRealmDestinationBytes',
            'DestinationSha256 != PrePublicRealmDestinationSha256',
            'LoadValidatedRuntimeContract(Assets, Provenance, Error)',
            'CreateVerifiedPrePublicRealmMapBackup(',
            'World->SpawnActor<ATRIADIstanaExploreV5DPublicRealmActor>(',
            'FTransform::Identity',
            'PublicRealm->ConfigurePublicRealm(',
            'ValidateHybridWorld(World, PreSaveReport,',
            'World->MarkPackageDirty();',
            'UEditorLoadingAndSavingUtils::SaveMap(',
            'UWorld* ReloadedSource =',
            'UWorld* ReloadedTarget = UEditorLoadingAndSavingUtils::LoadMap(',
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            'ValidateHybridWorld(ReloadedTarget, ColdReport,',
            'EXPLORE_V5D_PUBLIC_REALM_APPLY_PASS',
        ):
            self.assertIn(fragment, migration)
        self.assertEqual(
            migration.count(
                'SpawnActor<ATRIADIstanaExploreV5DPublicRealmActor>'
            ),
            1,
        )
        for unrelated_rebuild_pass in (
            'ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder(',
            'ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder(',
            'ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder(',
        ):
            self.assertNotIn(unrelated_rebuild_pass, migration)

        roster_guard = migration.index(
            'ExistingPublicRealmCount != 0 || MapPackage->IsDirty()'
        )
        hash_check = migration.index(
            'DestinationBytes != PrePublicRealmDestinationBytes'
        )
        asset_preflight = migration.index('LoadValidatedRuntimeContract(')
        backup_call = migration.index('CreateVerifiedPrePublicRealmMapBackup(')
        spawn = migration.index(
            'World->SpawnActor<ATRIADIstanaExploreV5DPublicRealmActor>('
        )
        identity = migration.index('FTransform::Identity', spawn)
        configure = migration.index('PublicRealm->ConfigurePublicRealm(', identity)
        pre_save_validation = migration.index(
            'ValidateHybridWorld(World, PreSaveReport,', configure
        )
        save = migration.index(
            'UEditorLoadingAndSavingUtils::SaveMap(', pre_save_validation
        )
        reload_source = migration.index('UWorld* ReloadedSource =', save)
        reload_target = migration.index('UWorld* ReloadedTarget =', reload_source)
        finish_compilation = migration.index(
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            reload_target,
        )
        cold_validation = migration.index(
            'ValidateHybridWorld(ReloadedTarget, ColdReport,',
            finish_compilation,
        )
        ordered_operations = (
            roster_guard,
            hash_check,
            asset_preflight,
            backup_call,
            spawn,
            identity,
            configure,
            pre_save_validation,
            save,
            reload_source,
            reload_target,
            finish_compilation,
            cold_validation,
        )
        self.assertEqual(tuple(sorted(ordered_operations)), ordered_operations)

    def test_public_realm_pre_save_failures_cold_restore_the_verified_predecessor(self) -> None:
        migration, _ = extract_braced_block(
            self.editor_cpp,
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::\n'
            '    ApplyIstanaExploreV5DPublicRealmPassToLoadedHybridMap(',
        )
        rollback, _ = extract_braced_block(
            migration,
            'auto ColdRestoreVerifiedPredecessor =',
        )
        for fragment in (
            'SpawnedActor->Destroy();',
            'MapPackage->SetDirtyFlag(false);',
            'FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)',
            'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)',
            'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)',
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            'int32 RestoredPublicRealmCount = 0;',
            'FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(',
            'HashFileSha256(',
            'RestoredBytes == PrePublicRealmDestinationBytes',
            'RestoredSha256 == PrePublicRealmDestinationSha256',
            '!RestoredPackage->IsDirty()',
            'RestoredPublicRealmCount == 0',
            'MapPackage->SetDirtyFlag(true);',
            'cold predecessor restore=%s',
        ):
            self.assertIn(fragment, rollback)

        destroy = rollback.index('SpawnedActor->Destroy();')
        transient_clean = rollback.index('MapPackage->SetDirtyFlag(false);')
        source_package = rollback.index('SourceMapPackage', transient_clean)
        unload_through_source = rollback.index(
            'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)',
            source_package,
        )
        reload_destination = rollback.index(
            'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)',
            unload_through_source,
        )
        zero_actor_check = rollback.index(
            'FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(',
            reload_destination,
        )
        disk_hash = rollback.index('HashFileSha256(', zero_actor_check)
        exact_bytes = rollback.index(
            'RestoredBytes == PrePublicRealmDestinationBytes', disk_hash
        )
        exact_sha = rollback.index(
            'RestoredSha256 == PrePublicRealmDestinationSha256', exact_bytes
        )
        clean_package = rollback.index('!RestoredPackage->IsDirty()', exact_sha)
        zero_actor_result = rollback.index(
            'RestoredPublicRealmCount == 0', clean_package
        )
        ordered_rollback = (
            destroy,
            transient_clean,
            source_package,
            unload_through_source,
            reload_destination,
            zero_actor_check,
            disk_hash,
            exact_bytes,
            exact_sha,
            clean_package,
            zero_actor_result,
        )
        self.assertEqual(tuple(sorted(ordered_rollback)), ordered_rollback)

        configure_failure, _ = extract_braced_block(
            migration,
            'if (!PublicRealm ||\n'
            '        !PublicRealm->ConfigurePublicRealm(',
        )
        pre_save_failure, _ = extract_braced_block(
            migration,
            'if (!ValidateHybridWorld(World, PreSaveReport,',
        )
        for failure in (configure_failure, pre_save_failure):
            self.assertIn('FString RollbackReport;', failure)
            self.assertEqual(
                failure.count('ColdRestoreVerifiedPredecessor('),
                1,
            )
            self.assertIn('bRollbackSucceeded', failure)
            self.assertIn('rollbackSucceeded=%s', failure)
            self.assertIn('*RollbackReport', failure)
            self.assertNotIn('PublicRealm->Destroy();', failure)
            self.assertNotIn('MapPackage->SetDirtyFlag(false);', failure)
            self.assertLess(
                failure.index('ColdRestoreVerifiedPredecessor('),
                failure.index('return false;'),
            )
        self.assertEqual(migration.count('ColdRestoreVerifiedPredecessor'), 3)

    def test_public_realm_material_correction_is_hash_gated_and_map_only(self) -> None:
        for pinned_contract in (
            'constexpr int64 PrePublicRealmMaterialCorrectionBytes = 37414472;',
            'CDBEDE32ACBCD18D1DFCE03D01F4BA75CFE57063EE34AA2EAA89629B0E9B6080',
            'constexpr int64 CorrectedPublicRealmMaterialCorrectionBytes = 37414654;',
            '154732F334F1CFBB45B61F8777EBD8D788599B814EFCB265950D24ADA0BA3A90',
        ):
            self.assertIn(pinned_contract, self.editor_cpp)

        backup, _ = extract_braced_block(
            self.editor_cpp,
            'bool CreateVerifiedPrePublicRealmMaterialCorrectionMapBackup(',
        )
        for fragment in (
            'TRIAD/MapBackups/',
            'V5DPublicRealmMaterialCorrection_20260831',
            'Istana_PublicView_Explore_v5d_hybrid_CDBEDE32.umap',
            'IFileManager::Get().MakeDirectory(*BackupDirectory, true)',
            'IFileManager::Get().FileExists(*OutBackupFilename)',
            'IFileManager::Get().Copy(',
            'false,\n            true) != COPY_OK',
            'HashFileSha256(',
            'BackupBytes != PrePublicRealmMaterialCorrectionBytes',
            'BackupSha256 != PrePublicRealmMaterialCorrectionSha256',
            'non-overwriting V5D public-realm material-correction map backup',
        ):
            self.assertIn(fragment, backup)

        correction, _ = extract_braced_block(
            self.editor_cpp,
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::\n'
            '    ApplyIstanaExploreV5DPublicRealmMaterialCorrectionToLoadedHybridMap(',
        )
        for fragment in (
            'LogicalPackage != DestinationMapPackage',
            'PublicRealmCount == 1 && PublicRealm && !MapPackage->IsDirty()',
            'IDEMPOTENT_EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_ALREADY_VALID',
            'CorrectedPublicRealmMaterialCorrectionBytes > 0',
            '!CorrectedPublicRealmMaterialCorrectionSha256.IsEmpty()',
            'ExistingBytes ==',
            'CorrectedPublicRealmMaterialCorrectionBytes',
            'ExistingSha256 ==',
            'CorrectedPublicRealmMaterialCorrectionSha256',
            'REFUSED_CORRECTED_HASH_UNPINNED_OR_DRIFTED',
            'PublicRealmCount != 1 || !PublicRealm || MapPackage->IsDirty()',
            'PublicRealm->ValidateLegacyVisualMaterialContract(LegacyReport)',
            'HashFileSha256(',
            'DestinationBytes != PrePublicRealmMaterialCorrectionBytes',
            'DestinationSha256 != PrePublicRealmMaterialCorrectionSha256',
            'LoadValidatedRuntimeContract(Assets, Provenance, Error)',
            'PublicRealm->SavedProvenance.CoreSourceSha256 !=',
            'PublicRealm->SavedProvenance.FallbackSourceSha256 !=',
            'CreateVerifiedPrePublicRealmMaterialCorrectionMapBackup(',
            'PublicRealm->UpgradeVisualMaterials(Assets, UpgradeReport)',
            'ValidateHybridWorld(World, PreSaveReport,',
            'World->MarkPackageDirty();',
            'UEditorLoadingAndSavingUtils::SaveMap(',
            'UWorld* ReloadedSource =',
            'UWorld* ReloadedTarget =',
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            'ValidateHybridWorld(ReloadedTarget, ColdReport,',
            'ReloadedPackage->IsDirty()',
            'correctedBytes=%lld correctedSha256=%s',
            'EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_PASS',
            'only the existing actor\'s appearance revision, road material references, and exact component override slots were changed',
        ):
            self.assertIn(fragment, correction)

        for prohibited_mutation in (
            'SpawnActor<',
            'Destroy();',
            'DestroyActor(',
            'SetStaticMesh(',
            'SetStaticMaterials(',
            'ImportAssetTasks(',
            'SaveLoadedAsset(',
            'SaveLoadedAssets(',
            'CreateFreshAssets(',
            'CreateFreshVisualMaterialUpgrade(',
        ):
            self.assertNotIn(prohibited_mutation, correction)
        self.assertEqual(
            correction.count('UEditorLoadingAndSavingUtils::SaveMap('),
            1,
        )

        legacy_guard = correction.index(
            'PublicRealm->ValidateLegacyVisualMaterialContract(LegacyReport)'
        )
        hash_gate = correction.index(
            'DestinationBytes != PrePublicRealmMaterialCorrectionBytes'
        )
        asset_preflight = correction.index('LoadValidatedRuntimeContract(')
        provenance_gate = correction.index(
            'PublicRealm->SavedProvenance.SchemaRevision !='
        )
        backup_call = correction.index(
            'CreateVerifiedPrePublicRealmMaterialCorrectionMapBackup('
        )
        upgrade = correction.index('PublicRealm->UpgradeVisualMaterials(')
        pre_save_validation = correction.index(
            'ValidateHybridWorld(World, PreSaveReport,', upgrade
        )
        save = correction.index(
            'UEditorLoadingAndSavingUtils::SaveMap(', pre_save_validation
        )
        reload_target = correction.index('UWorld* ReloadedTarget =', save)
        cold_validation = correction.index(
            'ValidateHybridWorld(ReloadedTarget, ColdReport,', reload_target
        )
        ordered_operations = (
            legacy_guard,
            hash_gate,
            asset_preflight,
            provenance_gate,
            backup_call,
            upgrade,
            pre_save_validation,
            save,
            reload_target,
            cold_validation,
        )
        self.assertEqual(tuple(sorted(ordered_operations)), ordered_operations)

        rollback, _ = extract_braced_block(
            correction,
            'auto ColdRestoreVerifiedMaterialPredecessor =',
        )
        for fragment in (
            'GEditor->GetEditorWorldContext().World()',
            'CurrentWorld->GetOutermost()',
            'CurrentPackage->SetDirtyFlag(false)',
            'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)',
            'HashFileSha256(\n            BackupFilename',
            'BackupBytes == PrePublicRealmMaterialCorrectionBytes',
            'BackupSha256 == PrePublicRealmMaterialCorrectionSha256',
            'IFileManager::Get().Copy(',
            '*DestinationFilename',
            '*BackupFilename',
            'true,\n                true) != COPY_OK',
            'HashFileSha256(\n            DestinationFilename',
            'CopiedBytes == PrePublicRealmMaterialCorrectionBytes',
            'CopiedSha256 == PrePublicRealmMaterialCorrectionSha256',
            'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)',
            'ValidateLegacyVisualMaterialContract(',
            'RestoredPublicRealmCount == 1',
            '!RestoredPackage->IsDirty()',
        ):
            self.assertIn(fragment, rollback)
        self.assertNotIn('Destroy();', rollback)
        unload = rollback.index(
            'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)'
        )
        backup_hash = rollback.index(
            'HashFileSha256(\n            BackupFilename', unload
        )
        restore_copy = rollback.index('IFileManager::Get().Copy(', backup_hash)
        copied_hash = rollback.index(
            'HashFileSha256(\n            DestinationFilename', restore_copy
        )
        reload = rollback.index(
            'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)',
            copied_hash,
        )
        self.assertEqual(
            (unload, backup_hash, restore_copy, copied_hash, reload),
            tuple(sorted((unload, backup_hash, restore_copy, copied_hash, reload))),
        )

        for failure_marker in (
            'if (!UEditorLoadingAndSavingUtils::SaveMap(',
            'if (!ReloadedSource)',
            'if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||',
            'if (!HashFileSha256(\n            DestinationFilename,\n            CorrectedSha256',
        ):
            failure, _ = extract_braced_block(correction, failure_marker)
            self.assertIn(
                'ColdRestoreVerifiedMaterialPredecessor(RollbackReport)',
                failure,
            )
            self.assertIn('bRollbackSucceeded', failure)
            self.assertIn('return false;', failure)

    def test_inherited_planning_ground_suppression_migration_is_hash_pinned_map_only_and_cold_validated(self) -> None:
        for pinned_contract in (
            'constexpr int64 PreInheritedPlanningGroundSuppressionBytes = 37414654;',
            '154732F334F1CFBB45B61F8777EBD8D788599B814EFCB265950D24ADA0BA3A90',
            'constexpr int64 CorrectedInheritedPlanningGroundSuppressionBytes = 37414604;',
            'E01C5ECB476E9723F3FB85AFFE101EE4CEBC719FD493E40C26E3BC85584FFAAB',
        ):
            self.assertIn(pinned_contract, self.editor_cpp)

        api_name = (
            'ApplyIstanaExploreV5DInheritedPlanningGroundSuppressionToLoadedHybridMap'
        )
        self.assertIn(api_name, self.editor_h)
        migration_marker = (
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::\n'
            '    ApplyIstanaExploreV5DInheritedPlanningGroundSuppressionToLoadedHybridMap('
        )
        self.assertIn(migration_marker, self.editor_cpp)

        backup, _ = extract_braced_block(
            self.editor_cpp,
            'bool CreateVerifiedPreInheritedPlanningGroundSuppressionMapBackup(',
        )
        for fragment in (
            'TRIAD/MapBackups/',
            'V5DInheritedPlanningGroundSuppression_20260831',
            'Istana_PublicView_Explore_v5d_hybrid_154732F3.umap',
            'IFileManager::Get().MakeDirectory(*BackupDirectory, true)',
            'IFileManager::Get().FileExists(*OutBackupFilename)',
            'IFileManager::Get().Copy(',
            'false,\n            true) != COPY_OK',
            'HashFileSha256(',
            'BackupBytes != PreInheritedPlanningGroundSuppressionBytes',
            'BackupSha256 != PreInheritedPlanningGroundSuppressionSha256',
            'non-overwriting V5D inherited planning-ground suppression map backup',
        ):
            self.assertIn(fragment, backup)

        migration, _ = extract_braced_block(
            self.editor_cpp,
            migration_marker,
        )
        for fragment in (
            '!GIsEditor || !GEditor || GEditor->PlayWorld ||',
            'GEditor->IsPlaySessionInProgress()',
            'LogicalPackage != DestinationMapPackage',
            'MapPackage->IsDirty()',
            'SceneCount != 1 || PolicyCount != 1',
            'DestinationBytes != PreInheritedPlanningGroundSuppressionBytes',
            'DestinationSha256 != PreInheritedPlanningGroundSuppressionSha256',
            'CorrectedPublicRealmMaterialCorrectionBytes',
            'CorrectedPublicRealmMaterialCorrectionSha256',
            'CreateVerifiedPreInheritedPlanningGroundSuppressionMapBackup(',
            'Scene->V5CGroundContextRenderOnlyComponent',
            'V5COfficialPlanningGroundContextRenderOnlySuccessor',
            '!PlanningGround->IsVisible() || PlanningGround->bHiddenInGame',
            'Policy->SuppressInheritedPlanningGroundPresentation(',
            'PlanningGround->IsVisible() || !PlanningGround->bHiddenInGame',
            'ValidateHybridWorld(World, PreSaveReport,',
            'World->MarkPackageDirty();',
            'UEditorLoadingAndSavingUtils::SaveMap(',
            'UWorld* ReloadedSource =',
            'UWorld* ReloadedTarget =',
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            'ValidateHybridWorld(ReloadedTarget, ColdReport,',
            'CorrectedInheritedPlanningGroundSuppressionBytes > 0',
            '!CorrectedInheritedPlanningGroundSuppressionSha256.IsEmpty()',
            'ExistingBytes == CorrectedInheritedPlanningGroundSuppressionBytes',
            'ExistingSha256 == CorrectedInheritedPlanningGroundSuppressionSha256',
            'FinalBytes != CorrectedInheritedPlanningGroundSuppressionBytes',
            'FinalSha256 != CorrectedInheritedPlanningGroundSuppressionSha256',
            'IDEMPOTENT_EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_ALREADY_VALID',
            'EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_PASS',
        ):
            self.assertIn(fragment, migration)

        for prohibited_mutation in (
            'SpawnActor<',
            'Destroy();',
            'DestroyActor(',
            'SetStaticMesh(',
            'SetStaticMaterials(',
            'SetVisibility(',
            'SetHiddenInGame(',
            'ImportAssetTasks(',
            'SaveLoadedAsset(',
            'SaveLoadedAssets(',
        ):
            self.assertNotIn(prohibited_mutation, migration)
        self.assertEqual(
            migration.count('UEditorLoadingAndSavingUtils::SaveMap('),
            1,
        )

        predecessor_hash_gate = migration.index(
            'DestinationBytes != PreInheritedPlanningGroundSuppressionBytes'
        )
        backup_call = migration.index(
            'CreateVerifiedPreInheritedPlanningGroundSuppressionMapBackup(',
            predecessor_hash_gate,
        )
        predecessor_state = migration.index(
            '!PlanningGround->IsVisible() || PlanningGround->bHiddenInGame',
            backup_call,
        )
        configure = migration.index(
            'Policy->SuppressInheritedPlanningGroundPresentation(',
            predecessor_state,
        )
        successor_state = migration.index(
            'PlanningGround->IsVisible() || !PlanningGround->bHiddenInGame',
            configure,
        )
        pre_save_validation = migration.index(
            'ValidateHybridWorld(World, PreSaveReport,',
            successor_state,
        )
        save = migration.index(
            'UEditorLoadingAndSavingUtils::SaveMap(',
            pre_save_validation,
        )
        reload_target = migration.index('UWorld* ReloadedTarget =', save)
        cold_validation = migration.index(
            'ValidateHybridWorld(ReloadedTarget, ColdReport,',
            reload_target,
        )
        final_hash = migration.index(
            'FinalBytes != CorrectedInheritedPlanningGroundSuppressionBytes',
            cold_validation,
        )
        ordered_operations = (
            predecessor_hash_gate,
            backup_call,
            predecessor_state,
            configure,
            successor_state,
            pre_save_validation,
            save,
            reload_target,
            cold_validation,
            final_hash,
        )
        self.assertEqual(tuple(sorted(ordered_operations)), ordered_operations)

        rollback, _ = extract_braced_block(
            migration,
            'auto ColdRestoreVerifiedPlanningGroundPredecessor =',
        )
        for fragment in (
            'CurrentPackage->SetDirtyFlag(false)',
            'UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)',
            'BackupBytes == PreInheritedPlanningGroundSuppressionBytes',
            'BackupSha256 == PreInheritedPlanningGroundSuppressionSha256',
            'IFileManager::Get().Copy(',
            '*DestinationFilename',
            '*BackupFilename',
            'CopiedBytes == PreInheritedPlanningGroundSuppressionBytes',
            'CopiedSha256 == PreInheritedPlanningGroundSuppressionSha256',
            'UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)',
            'RestoredPlanningGround->IsVisible()',
            '!RestoredPlanningGround->bHiddenInGame',
            '!RestoredPackage->IsDirty()',
        ):
            self.assertIn(fragment, rollback)

        for failure_marker in (
            'if (!ValidateHybridWorld(World, PreSaveReport,',
            'if (!UEditorLoadingAndSavingUtils::SaveMap(',
            'if (!ReloadedSource)',
            'if (!ReloadedTarget || !ReloadedPackage ||',
            'if (!HashFileSha256(\n            DestinationFilename,\n            FinalSha256',
        ):
            failure, _ = extract_braced_block(migration, failure_marker)
            self.assertIn(
                'ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport)',
                failure,
            )
            self.assertIn('bRollbackSucceeded', failure)
            self.assertIn('return false;', failure)

    def test_public_realm_material_correction_preserves_actor_meshes_and_source_bindings(self) -> None:
        presentation, _ = extract_braced_block(
            self.public_realm_cpp,
            'bool ValidatePresentationAssets(',
        )
        for fragment in (
            '&LegacyRoadBaseMaterialObjectPath',
            '&LegacyRoadGraphicMaterialObjectPath',
            '&ConcreteMaterialObjectPath',
            'Assets.RoadBaseMaterial->GetPathName() !=',
            'Assets.RoadGraphicMaterial->GetPathName() !=',
            'RoadAsphaltMaterialObjectPath',
            'RoadGraphicSuppressionMaterialObjectPath',
            'ValidateExactMeshSourceBindings(',
            'exact dry-asphalt road presentation and a distinct fully clipped planning-graphic presentation while retaining immutable source bindings',
        ):
            self.assertIn(fragment, presentation)
        self.assertEqual(
            presentation.count('ValidateExactMeshSourceBindings('),
            2,
        )

        source_binding_validation, _ = extract_braced_block(
            self.public_realm_cpp,
            'bool ValidateExactMeshSourceBindings(',
        )
        for fragment in (
            'Mesh->GetStaticMaterials().Num() != ExpectedMaterialPaths.Num()',
            'Mesh->GetStaticMaterials()[Slot].MaterialInterface',
            'BoundMaterial->GetPathName() != *ExpectedPath',
            'exact immutable planning/concrete binding',
        ):
            self.assertIn(fragment, source_binding_validation)

        upgrade, _ = extract_braced_block(
            self.public_realm_cpp,
            'bool ATRIADIstanaExploreV5DPublicRealmActor::UpgradeVisualMaterials(',
        )
        for fragment in (
            'ValidateLegacyVisualMaterialContract(LegacyReport)',
            'ValidatePresentationAssets(InAssets, OutError)',
            'InAssets.CoreMesh != SavedAssets.CoreMesh',
            'InAssets.FallbackMesh != SavedAssets.FallbackMesh',
            'InAssets.ConcreteMaterial != SavedAssets.ConcreteMaterial',
            'const FTRIADIstanaExploreV5DPublicRealmAssets PreviousAssets =',
            'CorePublicRealmRenderOnly->SetMaterial(0, SavedAssets.RoadBaseMaterial)',
            'FallbackPublicRealmRenderOnly->SetMaterial(',
            'AppearanceRevision = PbrAppearanceRevision',
            'ValidatePublicRealm(CorrectedReport)',
            'CorePublicRealmRenderOnly->EmptyOverrideMaterials()',
            'FallbackPublicRealmRenderOnly->EmptyOverrideMaterials()',
            'SavedAssets = PreviousAssets',
            'AppearanceRevision = LegacyAppearanceRevision',
            'ValidateLegacyVisualMaterialContract(RestoredReport)',
        ):
            self.assertIn(fragment, upgrade)
        self.assertEqual(upgrade.count('SetMaterial('), 3)
        for prohibited_mutation in (
            'SetStaticMesh(',
            'SetStaticMaterials(',
            'SpawnActor<',
            'Destroy();',
            'SetActorTransform(',
            'SetRelativeTransform(',
            'SetVisibility(',
            'SetHiddenInGame(',
            'SetCollisionEnabled(',
        ):
            self.assertNotIn(prohibited_mutation, upgrade)

        correction, _ = extract_braced_block(
            self.editor_cpp,
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::\n'
            '    ApplyIstanaExploreV5DPublicRealmMaterialCorrectionToLoadedHybridMap(',
        )
        rollback, _ = extract_braced_block(
            correction,
            'auto ColdRestoreVerifiedMaterialPredecessor =',
        )
        for fragment in (
            'RestoredPublicRealmCount == 1',
            'ValidateLegacyVisualMaterialContract(',
            'RestoredBytes == PrePublicRealmMaterialCorrectionBytes',
            'RestoredSha256 == PrePublicRealmMaterialCorrectionSha256',
            '!RestoredPackage->IsDirty()',
        ):
            self.assertIn(fragment, rollback)
        self.assertNotIn('Destroy', rollback)
        self.assertEqual(
            correction.count('ColdRestoreVerifiedMaterialPredecessor'),
            8,
        )

    def test_required_nanite_material_migration_is_exact_clean_and_idempotent(self) -> None:
        expected_packages = (
            '/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafDark',
            '/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafMid',
            '/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafLight',
            '/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Bark',
            '/Game/TRIAD/IstanaPublicView/HeroMaterialsV5/M_IPV_HeroSurface_V5',
            '/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/M_IPV5_Grass001_Lawn_Base',
            '/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/M_IPV5C_ContextMassing_Master',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_LawnMacroVariation',
            '/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Lawn',
        )
        roster_start = self.editor_cpp.index(
            'RequiredNaniteMaterialPackagePaths['
        )
        roster_end = self.editor_cpp.index(
            'struct FHybridProofViewSuppressionState', roster_start
        )
        roster = self.editor_cpp[roster_start:roster_end]
        self.assertIn('RequiredNaniteMaterialCount = 9', self.editor_cpp)
        self.assertEqual(roster.count('TEXT("/Game/TRIAD/'), 9)
        for package in expected_packages:
            self.assertEqual(roster.count(f'TEXT("{package}")'), 1)

        for fragment in (
            'GEditor->PlayWorld || GEditor->IsPlaySessionInProgress()',
            'LOAD_NoRedirects',
            'Cast<UMaterial>(Object)',
            'Material->GetPathName() != ObjectPath',
            'Package->GetName() != PackagePath',
            'Package->IsDirty()',
            'Material->SetMaterialUsage(',
            'MATUSAGE_Nanite',
            'Material->GetUsageByFlag(MATUSAGE_Nanite)',
            'FAssetCompilingManager::Get().FinishAllCompilation();',
            'AssetsToSave.Reserve(RequiredNaniteMaterialCount)',
            'SaveLoadedAssets(AssetsToSave, true)',
            'packagesClean=true exactPaths=true',
        ):
            self.assertIn(fragment, self.editor_cpp)
        self.assertNotIn('SetUsageByFlag(', self.editor_cpp)

        upgrade, _ = extract_braced_block(
            self.editor_cpp,
            'UpgradeIstanaExploreV5DRequiredNaniteMaterialUsage(',
        )
        idempotent_guard = upgrade.index(
            'if (Material->bUsedWithNanite &&'
        )
        self.assertLess(idempotent_guard, upgrade.index('Material->Modify();'))
        self.assertLess(
            upgrade.index('Material->Modify();'),
            upgrade.index('Material->SetMaterialUsage('),
        )
        self.assertLess(
            upgrade.rindex(
                'FAssetCompilingManager::Get().FinishAllCompilation();'
            ),
            upgrade.index('SaveLoadedAssets(AssetsToSave, true)'),
        )
        self.assertLess(
            upgrade.index('SaveLoadedAssets(AssetsToSave, true)'),
            upgrade.index('ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_UPGRADE_PASS'),
        )

    def test_visual_proof_capture_waits_for_stream_and_never_overwrites(self) -> None:
        for fragment in (
            'Tileset->GetLoadProgress() < 98.0f',
            '!Policy->bLocalBuildingFallbackCurrentlyHidden',
            'explore_v5d_',
            'TRIAD/IstanaPreviews/ExploreV5D',
            'IFileManager::Get().FileSize(*Destination) >= 0',
            'Config.SetResolution(2560, 1440, 1.0f)',
        ):
            self.assertIn(fragment, self.editor_cpp)

    def test_grass_semantic_matte_api_and_preconditions_are_exact(self) -> None:
        self.assertIn('#include "HAL/IConsoleManager.h"', self.editor_cpp)
        self.assertIn('#include "UnrealEngine.h"', self.editor_cpp)
        self.assertIn(
            'static bool CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(\n'
            '        const FString& OutputFileName,\n'
            '        FString& OutMessage);',
            self.editor_h,
        )
        self.assertIn(
            'R23 masked materials and never changes a material, component, map,',
            self.editor_h,
        )
        matte, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(',
        )

        for fragment in (
            'TEXT("explore_v5d_r23_grass_semantic_matte_")',
            'constexpr int32 CaptureWidth = 2560;',
            'constexpr int32 CaptureHeight = 1440;',
            'constexpr float RequiredHorizontalFovDegrees = 80.0f;',
            'constexpr float RequiredAspectRatio = 16.0f / 9.0f;',
            'constexpr float MaximumGrassDepthCentimeters = 5000.0f;',
            'constexpr float DepthEqualityEpsilonCentimeters = 0.25f;',
            'constexpr int32 ExpectedTotalInstances = 18944;',
            'OutputFileName.StartsWith(',
            'ESearchCase::CaseSensitive',
            'OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase)',
            'FPaths::GetCleanFilename(OutputFileName) != OutputFileName',
            'FPaths::MakeValidFileName(OutputFileName) != OutputFileName',
            'FPaths::ValidatePath(OutputFileName, &FilenameReason)',
            'Character < static_cast<TCHAR>(0x20)',
            'Character == static_cast<TCHAR>(0x7f)',
            'static bool bGrassMatteCaptureActive = false;',
            'FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot',
            'GetValidatedHybridPlayState(',
            'Tileset->GetLoadProgress()',
            '!FMath::IsFinite(LoadProgress)',
            'LoadProgress < 98.0f',
            'LoadProgress > 100.0f',
            '!Policy->bLocalBuildingFallbackCurrentlyHidden',
            'HasStableHybridVisualPolicy(Policy)',
            'FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>',
            'GroundVegetationCount != 1',
            'GroundVegetation->GetWorld() != World',
            'GroundVegetation->ValidateGroundVegetationRealism(',
            'ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID',
            'grassPresentationRevision=R23',
            'stagedLegacyMigration=R14ToR20ToR23',
            'grassMaterialRevision=R23',
            'groundOverlayMaterialRevision=R23',
            'edgeGrassMaterialRevision=R23',
            'runtimeMaterialRevisionMarker=23',
            'struct FExpectedIntegerConsoleVariable',
            'constexpr FExpectedIntegerConsoleVariable '
            'ExpectedIntegerConsoleVariables[]',
            '{TEXT("foliage.SplitFactor"), 16}',
            '{TEXT("foliage.ForceLOD"), -1}',
            '{TEXT("foliage.OnlyLOD"), -1}',
            '{TEXT("foliage.DisableCull"), 0}',
            '{TEXT("foliage.CullAll"), 0}',
            '{TEXT("foliage.DitheredLOD"), 1}',
            '{TEXT("foliage.OverestimateLOD"), 0}',
            '{TEXT("foliage.MaxTrianglesToRender"), 100000000}',
            '{TEXT("foliage.MinVertsToSplitNode"), 8192}',
            '{TEXT("foliage.MaxEndCullDistance"), 0}',
            '{TEXT("foliage.MinLOD"), -1}',
            '{TEXT("foliage.CullAllInVertexShader"), 0}',
            '{TEXT("r.MeshStreaming"), 0}',
            '{TEXT("r.SceneCapture.DepthPrepassOptimization"), 0}',
            'IConsoleManager::Get().FindConsoleVariable(Expected.Name)',
            'ConsoleVariable->GetInt() != Expected.Value',
            'struct FExpectedFloatConsoleVariable',
            'constexpr FExpectedFloatConsoleVariable '
            'ExpectedFloatConsoleVariables[]',
            '{TEXT("foliage.MinimumScreenSize"), 0.000005f, 0.00000001f}',
            '{TEXT("foliage.LODDistanceScale"), 1.0f, 0.000001f}',
            '{TEXT("foliage.RandomLODRange"), 0.0f, 0.000001f}',
            'ConsoleVariable->GetFloat()',
            'GetCachedScalabilityCVars().ViewDistanceScale',
            'FMath::IsNearlyEqual(CachedViewDistanceScale, 1.0f, 0.000001f)',
            'ValidatedFoliageMinimumScreenSize',
            'ValidatedFoliageLodDistanceScale',
            'ValidatedFoliageRandomLodRange',
        ):
            self.assertIn(fragment, matte)

        validated_state = matte.index('GetValidatedHybridPlayState(')
        validated_ground = matte.index(
            'GroundVegetation->ValidateGroundVegetationRealism('
        )
        validated_cvars = matte.index(
            'constexpr FExpectedIntegerConsoleVariable '
            'ExpectedIntegerConsoleVariables[]'
        )
        allocate_target = matte.index('NewObject<UTextureRenderTarget2D>(')
        self.assertLess(validated_state, validated_ground)
        self.assertLess(validated_ground, validated_cvars)
        self.assertLess(validated_cvars, allocate_target)
        self.assertEqual(
            matte.count(
                'IConsoleManager::Get().FindConsoleVariable(Expected.Name)'
            ),
            2,
        )

    def test_grass_semantic_matte_binds_world_transforms_and_rebuilds_exact_trees(self) -> None:
        self.assertIn(
            'bool ValidateOwnedGrassInstanceTransforms(FString& OutReport) const;',
            self.ground_vegetation_h,
        )
        transform_validation, _ = extract_braced_block(
            self.ground_vegetation_cpp,
            'ValidateOwnedGrassInstanceTransforms(FString& OutReport) const',
        )
        for fragment in (
            'GrassManicuredInstances',
            'GrassHumidInstances',
            'GrassShadeInstances',
            'GrassDryEdgeInstances',
            'EdgeGrassInstances',
            '&SavedGrassManicured',
            '&SavedGrassHumid',
            '&SavedGrassShade',
            '&SavedGrassDryEdge',
            '&SavedEdgeGrass',
            'UE_ARRAY_COUNT(Components) == 5',
            'UE_ARRAY_COUNT(ExpectedTransforms) == 5',
            'Component->GetInstanceCount() != Expected.Num()',
            'Component->GetInstanceTransform(\n'
            '                    InstanceIndex,\n'
            '                    Actual,\n'
            '                    true)',
            "UE 5.5's CPU GetInstanceTransform path is double-backed FMatrix",
            "the proof's observable HISM render tree converts instances to",
            'renderer-representability floor',
            'constexpr double SerializedHismTransformToleranceCm = 0.001;',
            '!Actual.Equals(\n'
            '                    Expected[InstanceIndex],\n'
            '                    SerializedHismTransformToleranceCm)',
            'VerifiedTransforms != GrassMicroDetailCount + SavedEdgeGrass.Num()',
            'exactDeterministicSavedGrassTransforms=true verifiedGrassTransforms=%d',
        ):
            self.assertIn(fragment, transform_validation)

        matte, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(',
        )
        validate_roster = matte.index('VerifiedTotalInstances != ExpectedTotalInstances')
        validate_transforms = matte.index(
            'GroundVegetation->ValidateOwnedGrassInstanceTransforms('
        )
        rebuild_trees = matte.index('Component->BuildTreeIfOutdated(', validate_transforms)
        allocate_target = matte.index('NewObject<UTextureRenderTarget2D>(', rebuild_trees)
        self.assertLess(validate_roster, validate_transforms)
        self.assertLess(validate_transforms, rebuild_trees)
        self.assertLess(rebuild_trees, allocate_target)
        for fragment in (
            'Component->BuildTreeIfOutdated(\n'
            '                /*Async*/ false,\n'
            '                /*ForceUpdate*/ true)',
            '!Component->IsTreeFullyBuilt()',
            'Component->GetNumRenderInstances() !=\n'
            '                ExpectedInstanceCounts[Index]',
            'SynchronouslyRebuiltClusterTrees != 5',
        ):
            self.assertIn(fragment, matte)

    def test_grass_semantic_matte_roster_material_and_render_state_are_exact(self) -> None:
        matte, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(',
        )

        component_members = (
            'GroundVegetation->GrassManicuredInstances',
            'GroundVegetation->GrassHumidInstances',
            'GroundVegetation->GrassShadeInstances',
            'GroundVegetation->GrassDryEdgeInstances',
            'GroundVegetation->EdgeGrassInstances',
        )
        component_names = (
            'V5DGrassManicuredMicroClumps',
            'V5DGrassHumidMicroClumps',
            'V5DGrassShadeMicroClumps',
            'V5DGrassDryEdgeMicroClumps',
            'V5DEdgeGrassSeasonalAccents',
        )
        material_paths = (
            '/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/'
            'M_IPV5D_Turf_Manicured.M_IPV5D_Turf_Manicured',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/'
            'M_IPV5D_Turf_Humid.M_IPV5D_Turf_Humid',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/'
            'M_IPV5D_Turf_Shade.M_IPV5D_Turf_Shade',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/'
            'M_IPV5D_Turf_DryEdge.M_IPV5D_Turf_DryEdge',
            '/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/'
            'M_IPV5D_GrassMedium_EdgeFade.M_IPV5D_GrassMedium_EdgeFade',
        )
        for exact_ordered_roster in (
            component_members,
            component_names,
            material_paths,
        ):
            positions = [matte.index(value) for value in exact_ordered_roster]
            self.assertEqual(positions, sorted(positions))

        for exact_array in (
            'constexpr int32 ExpectedInstanceCounts[] = {\n'
            '        12461, 3975, 1150, 846, 512};',
            'constexpr int32 ExpectedCullStarts[] = {\n'
            '        2600, 2600, 2600, 2600, 3000};',
            'constexpr int32 ExpectedCullEnds[] = {\n'
            '        3400, 3400, 3400, 3400, 4500};',
            'constexpr int32 ExpectedWpoDisableDistances[] = {\n'
            '        2400, 2400, 2400, 2400, 2400};',
        ):
            self.assertIn(exact_array, matte)

        for fragment in (
            'UE_ARRAY_COUNT(GrassComponents) == 5',
            'UE_ARRAY_COUNT(ExpectedComponentNames) == 5',
            'UE_ARRAY_COUNT(ExpectedMaterialPaths) == 5',
            'UE_ARRAY_COUNT(ExpectedInstanceCounts) == 5',
            'UE_ARRAY_COUNT(ExpectedCullStarts) == 5',
            'UE_ARRAY_COUNT(ExpectedCullEnds) == 5',
            'UE_ARRAY_COUNT(ExpectedWpoDisableDistances) == 5',
            'Component->GetCullDistances(CullStart, CullEnd);',
            'Component->GetMaterial(0)',
            'Component->GetOwner() != GroundVegetation',
            'Component->GetWorld() != World',
            '!Component->IsRegistered()',
            'Component->GetFName() != FName(ExpectedComponentNames[Index])',
            '!Component->GetStaticMesh()',
            'Component->GetStaticMesh()->IsCompiling()',
            '!Component->GetStaticMesh()->HasValidRenderData(false)',
            'Material->GetPathName() != ExpectedMaterialPaths[Index]',
            'Component->GetInstanceCount() != ExpectedInstanceCounts[Index]',
            'CullStart != ExpectedCullStarts[Index]',
            'CullEnd != ExpectedCullEnds[Index]',
            'Component->WorldPositionOffsetDisableDistance !=',
            'ExpectedWpoDisableDistances[Index]',
            'Component->InstanceLODDistanceScale,\n                0.60f,\n'
            '                0.0001f',
            'Component->bEnableDensityScaling',
            'Component->CurrentDensityScaling,\n'
            '                1.0f,\n'
            '                0.0001f',
            'Component->InstancingRandomSeed == 0',
            'Component->ForcedLodModel != 0',
            'Component->bOverrideMinLOD',
            'Component->MinLOD != 0',
            '!Component->IsTreeFullyBuilt()',
            '!Component->IsVisible() || Component->bHiddenInGame',
            '!Component->bRenderInMainPass',
            'Component->bHiddenInSceneCapture',
            'ECollisionEnabled::NoCollision',
            'Component->CanEverAffectNavigation()',
            'VerifiedTotalInstances != ExpectedTotalInstances',
        ):
            self.assertIn(fragment, matte)

    def test_grass_semantic_matte_copies_projection_and_runs_two_depth_passes(self) -> None:
        matte, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(',
        )

        for fragment in (
            'UGameViewportClient* ViewportClient = World->GetGameViewport();',
            'ULocalPlayer* LocalPlayer = Player ? Player->GetLocalPlayer()',
            'Player->PlayerCameraManager',
            'FSceneViewProjectionData ProjectionData;',
            'Player->GetPlayerViewPoint(PlayerViewLocation, PlayerViewRotation);',
            'CameraManager->GetCameraCacheView()',
            'LocalPlayer->GetProjectionData(',
            'INDEX_NONE',
            'ProjectionData.IsValidViewRectangle()',
            'ProjectionData.IsPerspectiveProjection()',
            'ProjectionData.ProjectionMatrix.ContainsNaN()',
            'CameraView->ProjectionMode != ECameraProjectionMode::Perspective',
            'CameraView->FOV,\n            RequiredHorizontalFovDegrees,\n'
            '            0.001f',
            'ProjectionData.GetConstrainedViewRect()',
            'ProjectionAspectRatio,\n            RequiredAspectRatio,\n'
            '            0.001f',
            'ProjectionData.ViewOrigin - PlayerViewLocation',
            'NewObject<UTextureRenderTarget2D>',
            'NewObject<USceneCaptureComponent2D>',
            'RF_Transient',
            'RenderTarget->InitCustomFormat(\n'
            '        CaptureWidth,\n'
            '        CaptureHeight,\n'
            '        PF_R32_FLOAT,\n'
            '        true);',
            'CaptureComponent->SetWorldLocationAndRotation(\n'
            '        PlayerViewLocation,\n'
            '        PlayerViewRotation);',
            'CaptureComponent->FOVAngle = CameraView->FOV;',
            'CaptureComponent->bUseCustomProjectionMatrix = true;',
            'CaptureComponent->CustomProjectionMatrix = ProjectionData.ProjectionMatrix;',
            'CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;',
            'CaptureComponent->bCaptureEveryFrame = false;',
            'CaptureComponent->bCaptureOnMovement = false;',
            'CaptureComponent->bRenderInMainRenderer = false;',
            'CaptureComponent->bIgnoreScreenPercentage = true;',
            'CaptureComponent->LODDistanceFactor = 1.0f;',
            'CaptureComponent->MaxViewDistanceOverride = 0.0f;',
            'CaptureComponent->ShowFlags.SetAntiAliasing(false);',
            'CaptureComponent->ShowFlags.SetTemporalAA(false);',
            'CaptureComponent->ShowFlags.SetMotionBlur(false);',
            'CaptureComponent->ShowFlags.SetMaterials(true);',
            'CaptureComponent->RegisterComponentWithWorld(World);',
            'RegisteredCaptureComponentsBefore + 1',
            'ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList',
            'CaptureComponent->ShowOnlyComponent(Component);',
            'CaptureComponent->ShowOnlyComponents.Num() != 5',
            'CaptureComponent->bCameraCutThisFrame = true;',
            'CaptureComponent->CaptureScene();',
            'RenderTarget->GameThread_GetRenderTargetResource()',
            'Resource->ReadLinearColorPixels(',
            'FReadSurfaceDataFlags(RCM_MinMax)',
            'FIntRect(0, 0, CaptureWidth, CaptureHeight)',
            'OutDepth.Num() == CaptureWidth * CaptureHeight',
        ):
            self.assertIn(fragment, matte)

        show_only_mode = matte.index(
            'ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList'
        )
        add_show_only = matte.index(
            'CaptureComponent->ShowOnlyComponent(Component);',
            show_only_mode,
        )
        self.assertEqual(
            matte.count('if (!CaptureLinearDepth(GrassOnlyDepth))'), 1
        )
        self.assertEqual(
            matte.count('if (!CaptureLinearDepth(FullSceneDepth))'), 1
        )
        grass_depth = matte.index(
            'if (!CaptureLinearDepth(GrassOnlyDepth))', add_show_only
        )
        clear_show_only = matte.index(
            'CaptureComponent->ClearShowOnlyComponents();', grass_depth
        )
        full_scene_mode = matte.index(
            'ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives',
            clear_show_only,
        )
        full_scene_depth = matte.index(
            'if (!CaptureLinearDepth(FullSceneDepth))', full_scene_mode
        )
        self.assertLess(show_only_mode, add_show_only)
        self.assertLess(add_show_only, grass_depth)
        self.assertLess(grass_depth, clear_show_only)
        self.assertLess(clear_show_only, full_scene_mode)
        self.assertLess(full_scene_mode, full_scene_depth)

    def test_grass_semantic_matte_binary_cleanup_and_atomic_publish_are_exact(self) -> None:
        matte, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(',
        )

        for fragment in (
            'FMath::IsFinite(GrassDepth)',
            'GrassDepth > 0.0f',
            'GrassDepth < MaximumGrassDepthCentimeters',
            'FMath::IsFinite(SceneDepth) && SceneDepth > 0.0f',
            'FMath::Abs(GrassDepth - SceneDepth) <=',
            'DepthEqualityEpsilonCentimeters',
            'const uint8 Value = bVisibleGrass ? 255 : 0;',
            'MattePixels[PixelIndex] = FColor(Value, Value, Value, 255);',
            'ForegroundPixelCount += bVisibleGrass ? 1 : 0;',
            'static_cast<int64>(MattePixels.Num()) - ForegroundPixelCount',
            'FImageUtils::PNGCompressImageArray(',
            'TArrayView64<const FColor>',
            'CompressedPng.Num() <= 0',
            'CaptureComponent->ClearShowOnlyComponents();',
            'CaptureComponent->ClearHiddenComponents();',
            'CaptureComponent->HiddenActors.Reset();',
            'CaptureComponent->ShowOnlyActors.Reset();',
            'CaptureComponent->TextureTarget = nullptr;',
            'CaptureComponent->UnregisterComponent();',
            'CaptureComponent->DestroyComponent();',
            'RenderTarget->ReleaseResource();',
            'CleanupTransientCapture();',
            'GetValidatedHybridPlayState(',
            'WorldAfter != World || PlayerAfter != Player',
            'PawnAfter != Pawn || PolicyAfter != Policy || TilesetAfter != Tileset',
            'RegisteredCaptureComponentsAfter != RegisteredCaptureComponentsBefore',
            'GroundVegetation->ValidateGroundVegetationRealism(',
            'GroundReportAfter != GroundReportBefore',
            'PlayerViewLocationAfter - PlayerViewLocation',
            'PlayerViewRotationAfter.Equals(',
            'FString TemporaryDestination = Destination + TEXT(".tmp.")',
            'FGuid::NewGuid().ToString(EGuidFormats::Digits)',
            'FFileHelper::SaveArrayToFile(',
            'IFileManager::Get().FileSize(*TemporaryDestination) <= 0',
            'IFileManager::Get().Move(',
            'could not atomically publish its no-overwrite PNG',
            'TemporaryDestination.Reset();',
        ):
            self.assertIn(fragment, matte)

        self.assertEqual(
            matte.count('IFileManager::Get().FileSize(*Destination) >= 0'),
            2,
        )
        self.assertIn(
            'IFileManager::Get().Delete(\n'
            '                *TemporaryDestination,\n'
            '                false,\n'
            '                true,\n'
            '                true);',
            matte,
        )
        self.assertIn(
            'IFileManager::Get().Move(\n'
            '            *Destination,\n'
            '            *TemporaryDestination,\n'
            '            false,\n'
            '            false,\n'
            '            false,\n'
            '            true)',
            matte,
        )
        self.assertIn(
            'ON_SCOPE_EXIT\n'
            '    {\n'
            '        bGrassMatteCaptureActive = false;\n'
            '    };',
            matte,
        )
        self.assertIn(
            'ON_SCOPE_EXIT\n'
            '    {\n'
            '        CleanupTransientCapture();\n'
            '    };',
            matte,
        )
        self.assertIn(
            'ON_SCOPE_EXIT\n'
            '    {\n'
            '        if (!TemporaryDestination.IsEmpty())\n'
            '        {\n'
            '            IFileManager::Get().Delete(',
            matte,
        )

        encode = matte.index('FImageUtils::PNGCompressImageArray(')
        cleanup = matte.index('CleanupTransientCapture();', encode)
        post_validate = matte.index('const bool bPostStateValid', cleanup)
        write_temporary = matte.index('FFileHelper::SaveArrayToFile(', post_validate)
        publish = matte.index('IFileManager::Get().Move(', write_temporary)
        success = matte.index('ISTANA_EXPLORE_V5D_GRASS_MATTE_CAPTURED', publish)
        self.assertLess(encode, cleanup)
        self.assertLess(cleanup, post_validate)
        self.assertLess(post_validate, write_temporary)
        self.assertLess(write_temporary, publish)
        self.assertLess(publish, success)

    def test_grass_semantic_matte_manifest_and_read_only_boundary_are_exact(self) -> None:
        matte, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(',
        )
        success_marker = 'TEXT("ISTANA_EXPLORE_V5D_GRASS_MATTE_CAPTURED '
        self.assertEqual(matte.count(success_marker), 1)
        success_start = matte.index(success_marker) + len('TEXT("')
        success_end = matte.index('"),', success_start)
        success = matte[success_start:success_end]
        self.assertTrue(success.endswith('destination=%s'))
        ordered_tokens = (
            'ISTANA_EXPLORE_V5D_GRASS_MATTE_CAPTURED',
            'method=TransientSceneCapture2DShowOnlyLinearDepthVisibleMask',
            'width=2560',
            'height=1440',
            'encoding=PNG_RGBA8_BINARY',
            'foreground=255',
            'background=0',
            'alpha=255',
            'captureSource=SCS_SceneDepth',
            'primitiveRenderMode=PRM_UseShowOnlyList',
            'occlusion=GrassDepthEqualsSceneCaptureRenderableDepth',
            'occlusionScope=SceneCaptureDepthWritingPrimitivesAfterDeclaredSuppressions',
            'playerVisibleOcclusionComplete=false',
            'depthEqualityEpsilonCm=0.25',
            'maxGrassDepthCm=5000',
            'rowOrigin=TopLeftReadSurfaceOrder',
            'componentCount=5',
            'instanceCount=18944',
            'showOnlyComponents=5',
            'grassPresentationRevision=R23',
            'originalR23OpacityMaterials=true',
            'materialOverridesApplied=0',
            'customDepthStencilApplied=0',
            'taa=false',
            'projectionMatrixCopied=true',
            'cullingFovDegrees=%.6f',
            'projectionAspectRatio=%.9f',
            'cvarFoliageSplitFactor=16',
            'cvarFoliageForceLod=-1',
            'cvarFoliageOnlyLod=-1',
            'cvarFoliageDisableCull=0',
            'cvarFoliageCullAll=0',
            'cvarFoliageDitheredLod=1',
            'cvarFoliageOverestimateLod=0',
            'cvarFoliageMaxTrianglesToRender=100000000',
            'cvarFoliageMinVertsToSplitNode=8192',
            'cvarFoliageMaxEndCullDistance=0',
            'cvarFoliageMinLod=-1',
            'cvarFoliageCullAllInVertexShader=0',
            'cvarMeshStreaming=0',
            'cvarSceneCaptureDepthPrepassOptimization=0',
            'cvarFoliageMinimumScreenSize=%.9f',
            'cvarFoliageLodDistanceScale=%.6f',
            'cvarFoliageRandomLodRange=%.6f',
            'cachedViewDistanceScale=%.6f',
            'clusterTreesSynchronouslyRebuiltUnderPinnedCvars=true',
            'componentTransformsEqualDeterministicSavedLayout=true',
            'verifiedInstanceTransforms=18944',
            'registeredSceneCaptureComponentsDeltaAfter=0',
            'playerViewChanged=false',
            'strictZeroAtOrBeyondEnd=false',
            'foregroundPixelCount=%lld',
            'backgroundPixelCount=%lld',
            'hiddenLineBatchers=%d',
            'hiddenHumanOverlays=%d',
            'hiddenDemoTargets=%d',
            'component0=V5DGrassManicuredMicroClumps',
            'material0=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/'
            'Materials/M_IPV5D_Turf_Manicured.M_IPV5D_Turf_Manicured',
            'instances0=12461 cullCm0=2600,3400 wpoDisableCm0=2400 lodScale0=0.60',
            'component1=V5DGrassHumidMicroClumps',
            'material1=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/'
            'Materials/M_IPV5D_Turf_Humid.M_IPV5D_Turf_Humid',
            'instances1=3975 cullCm1=2600,3400 wpoDisableCm1=2400 lodScale1=0.60',
            'component2=V5DGrassShadeMicroClumps',
            'material2=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/'
            'Materials/M_IPV5D_Turf_Shade.M_IPV5D_Turf_Shade',
            'instances2=1150 cullCm2=2600,3400 wpoDisableCm2=2400 lodScale2=0.60',
            'component3=V5DGrassDryEdgeMicroClumps',
            'material3=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/'
            'Materials/M_IPV5D_Turf_DryEdge.M_IPV5D_Turf_DryEdge',
            'instances3=846 cullCm3=2600,3400 wpoDisableCm3=2400 lodScale3=0.60',
            'component4=V5DEdgeGrassSeasonalAccents',
            'material4=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/'
            'Materials/M_IPV5D_GrassMedium_EdgeFade.M_IPV5D_GrassMedium_EdgeFade',
            'instances4=512 cullCm4=3000,4500 wpoDisableCm4=2400 lodScale4=0.60',
            'destination=%s',
        )
        cursor = 0
        for token in ordered_tokens:
            position = success.find(token, cursor)
            self.assertGreaterEqual(position, 0, token)
            self.assertEqual(success.count(token), 1, token)
            cursor = position + len(token)
        self.assertIn(
            '        CameraView->FOV,\n'
            '        ProjectionAspectRatio,\n'
            '        ValidatedFoliageMinimumScreenSize,\n'
            '        ValidatedFoliageLodDistanceScale,\n'
            '        ValidatedFoliageRandomLodRange,\n'
            '        CachedViewDistanceScale,\n'
            '        ForegroundPixelCount,\n'
            '        BackgroundPixelCount,\n'
            '        HiddenLineBatcherCount,\n'
            '        HiddenHumanOverlayCount,\n'
            '        HiddenDemoTargetCount,\n'
            '        *Destination);',
            matte,
        )

        for forbidden_world_mutation_or_cvar in (
            'SetMaterial(',
            'SetVisibility(',
            'SetHiddenInGame(',
            'SetCullDistances(',
            'SetStaticMesh(',
            'SetCollisionEnabled(',
            'SetCollisionResponse',
            'SetCanEverAffectNavigation(',
            'SetActorHiddenInGame(',
            'SetActorTickEnabled(',
            'SetRenderCustomDepth(',
            'SetCustomDepthStencilValue(',
            'bRenderCustomDepth =',
            'CustomDepthStencilValue =',
            'WorldPositionOffsetDisableDistance =',
            'InstanceLODDistanceScale =',
            'BeginHybridProofViewSuppression(',
            'AddHybridProofHiddenComponent(',
            'Player->HiddenPrimitiveComponents',
            'Player->HiddenActors',
            'Pawn->Set',
            'Player->Set',
            'CameraManager->UpdateCamera(',
            'Modify()',
            'MarkPackageDirty(',
            'SaveMap(',
            'SavePackage(',
            'ConsoleVariable->Set(',
            'RegisterConsoleVariable(',
            'UnregisterConsoleObject(',
            'CallAllConsoleVariableSinks(',
            'ProcessUserConsoleInput(',
            'ExecuteConsoleCommand(',
            'ConsoleCommand(',
            'GetWorld()->Exec(',
        ):
            self.assertNotIn(forbidden_world_mutation_or_cvar, matte)

    def test_public_realm_core_hidden_diagnostic_is_exact_player0_only_and_reversible(self) -> None:
        diagnostic, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridPublicRealmCoreHiddenDiagnosticPlayView(',
        )

        for fragment in (
            'TEXT("explore_v5d_diagnostic_public_realm_core_hidden_")',
            'explore_v5d_diagnostic_public_realm_core_hidden_*.png',
            'GetValidatedHybridPlayState(',
            '!FMath::IsFinite(LoadProgress)',
            'LoadProgress < 98.0f',
            '!Policy->bLocalBuildingFallbackCurrentlyHidden',
            'bStableVisualPolicy',
            'FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>',
            'PublicRealmCount != 1',
            'TEXT("CorePublicRealmRenderOnly")',
            'TEXT("TRIADV5DCorePublicRealmRenderOnly")',
            'TEXT("FallbackPublicRealmRenderOnly")',
            'TEXT("TRIADV5DFallbackPublicRealmRenderOnly")',
            'Component->IsRegistered()',
            'Component->GetCollisionEnabled()',
            'ECollisionEnabled::NoCollision',
            'FCollisionResponseContainer(ECR_Ignore)',
            '!Component->CanEverAffectNavigation()',
            'PublicRealm->ValidatePublicRealm(PublicRealmReport)',
            'PublicRealm->bProviderReady',
            'Viewport->GetSceneHDREnabled()',
            'IFileManager::Get().FileSize(*Destination) >= 0',
            'FScreenshotRequest::IsScreenshotRequested()',
            'Config.SetHDRCapture(false)',
            'Config.SetResolution(2560, 1440, 1.0f)',
            'BeginHybridProofViewSuppression(',
            'AddHybridProofHiddenComponent(',
            'SuppressedCoreCount != 1',
            'Viewport->TakeHighResScreenShot()',
            'NON-PROOF PUBLIC-REALM CORE-HIDDEN DIAGNOSTIC:',
            'GetIstanaExploreV5DHybridPlayStateReport',
            'ValidateIstanaExploreV5DHybridPlayWorld',
            'without changing component visibility, tick, collision, navigation, map, sensor, RF, or the normal proof path',
        ):
            self.assertIn(fragment, diagnostic)

        begin_index = diagnostic.index('BeginHybridProofViewSuppression(')
        add_index = diagnostic.index('AddHybridProofHiddenComponent(')
        request_index = diagnostic.index('Viewport->TakeHighResScreenShot()')
        self.assertLess(begin_index, add_index)
        self.assertLess(add_index, request_index)

        for forbidden_mutation in (
            'CorePublicRealm->SetVisibility(',
            'CorePublicRealm->SetHiddenInGame(',
            'FallbackPublicRealm->SetVisibility(',
            'FallbackPublicRealm->SetHiddenInGame(',
            'SetCollisionEnabled(',
            'SetCanEverAffectNavigation(',
            'SetActorHiddenInGame(',
            'SetActorTickEnabled(',
            'Modify()',
        ):
            self.assertNotIn(forbidden_mutation, diagnostic)

    def test_ground_macro_overlay_hidden_diagnostic_is_exact_player0_only_and_reversible(self) -> None:
        diagnostic, _ = extract_braced_block(
            self.editor_cpp,
            'CaptureIstanaExploreV5DHybridGroundMacroOverlayHiddenDiagnosticPlayView(',
        )

        for fragment in (
            'TEXT("explore_v5d_diagnostic_ground_macro_overlay_hidden_")',
            'explore_v5d_diagnostic_ground_macro_overlay_hidden_*.png',
            'GetValidatedHybridPlayState(',
            '!FMath::IsFinite(LoadProgress)',
            'LoadProgress < 98.0f',
            '!Policy->bLocalBuildingFallbackCurrentlyHidden',
            'bStableVisualPolicy',
            'FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>',
            'GroundVegetationCount != 1',
            'GroundVegetation->ValidateGroundVegetationRealism(',
            'TEXT("V5DGroundMacroVariationOverlay")',
            'GroundMacroOverlay->IsRegistered()',
            'GroundMacroOverlay->GetStaticMesh() &&',
            'GroundVegetation->SavedAssets.GroundOverlayMesh',
            'GroundMacroOverlay->GetMaterial(0) &&',
            'GroundVegetation->SavedAssets.GroundOverlayMaterial',
            'EComponentMobility::Movable',
            'ECollisionEnabled::NoCollision',
            'FCollisionResponseContainer(ECR_Ignore)',
            '!GroundMacroOverlay->CanEverAffectNavigation()',
            'GroundMacroOverlay->IsVisible()',
            '!GroundMacroOverlay->bHiddenInGame',
            '!GroundMacroOverlay->CastShadow',
            '!GroundMacroOverlay->bCastContactShadow',
            '!GroundMacroOverlay->bAffectDistanceFieldLighting',
            'GroundVegetation->bSourceTerrainRendererHiddenForReadyProvider',
            'PublicViewScene->TerrainComponent.Get()',
            'TEXT("TerrainVisualCollision")',
            'SourceTerrain->IsRegistered()',
            '!SourceTerrain->IsVisible()',
            'SourceTerrain->bHiddenInGame',
            'ECollisionEnabled::QueryAndPhysics',
            'Viewport->GetSceneHDREnabled()',
            'IFileManager::Get().FileSize(*Destination) >= 0',
            'FScreenshotRequest::IsScreenshotRequested()',
            'Config.SetHDRCapture(false)',
            'Config.SetForce128BitRendering(false)',
            'Config.SetResolution(2560, 1440, 1.0f)',
            'BeginHybridProofViewSuppression(',
            'AddHybridProofHiddenComponent(',
            'SuppressedGroundMacroOverlayCount != 1',
            'Viewport->TakeHighResScreenShot()',
            'NON-PROOF GROUND-MACRO-OVERLAY-HIDDEN DIAGNOSTIC:',
            'screenshot-restoration completion',
            'GetIstanaExploreV5DHybridPlayStateReport',
            'ValidateIstanaExploreV5DHybridPlayWorld',
            'without changing component visibility, tick, collision, navigation, map, simulation, sensor, or RF state or the normal proof path',
        ):
            self.assertIn(fragment, diagnostic)

        begin_index = diagnostic.index('BeginHybridProofViewSuppression(')
        add_index = diagnostic.index('AddHybridProofHiddenComponent(')
        request_index = diagnostic.index('Viewport->TakeHighResScreenShot()')
        self.assertLess(begin_index, add_index)
        self.assertLess(add_index, request_index)
        self.assertEqual(
            diagnostic.count('AddHybridProofHiddenComponent('),
            1,
        )

        for forbidden_mutation in (
            'GroundMacroOverlay->SetVisibility(',
            'GroundMacroOverlay->SetHiddenInGame(',
            'SourceTerrain->SetVisibility(',
            'SourceTerrain->SetHiddenInGame(',
            'SetCollisionEnabled(',
            'SetCanEverAffectNavigation(',
            'SetActorHiddenInGame(',
            'SetActorTickEnabled(',
            'Modify()',
        ):
            self.assertNotIn(forbidden_mutation, diagnostic)

    def test_unsafe_provider_site_clip_inactive_diagnostic_is_removed(self) -> None:
        combined_editor = self.editor_h + self.editor_cpp
        for removed_path in (
            'CaptureIstanaExploreV5DHybridProviderSiteClipInactiveDiagnosticPlayView',
            'ValidateProviderSiteClipInactiveDiagnosticTransientState',
            'PollProviderSiteClipInactiveDiagnosticWarmup',
            'FailProviderSiteClipInactiveDiagnosticWarmup',
            'ProviderSiteClipWarmupTickerHandle',
            'ProviderSiteClipDiagnosticPollIntervalSeconds',
            'bProviderSiteClipDiagnosticStateSaved',
            'Overlay->Deactivate(',
            'Overlay->Activate(',
        ):
            self.assertNotIn(removed_path, combined_editor)

        for safe_diagnostic in (
            'CaptureIstanaExploreV5DHybridPublicRealmCoreHiddenDiagnosticPlayView',
            'CaptureIstanaExploreV5DHybridGroundMacroOverlayHiddenDiagnosticPlayView',
            'CaptureIstanaExploreV5DHybridDiagnosticPlayView',
            'BeginHybridProofViewSuppression(',
            'RestoreHybridProofViewSuppression()',
        ):
            self.assertIn(safe_diagnostic, combined_editor)

    def test_diagnostic_capture_requires_stable_policy_and_is_stream_tolerant(self) -> None:
        diagnostic_start = self.editor_cpp.index(
            'CaptureIstanaExploreV5DHybridDiagnosticPlayView('
        )
        diagnostic_end = self.editor_cpp.index(
            '#if WITH_DEV_AUTOMATION_TESTS', diagnostic_start
        )
        diagnostic = self.editor_cpp[diagnostic_start:diagnostic_end]

        for fragment in (
            'GetValidatedHybridPlayState(',
            'TEXT("explore_v5d_diagnostic_")',
            'explore_v5d_diagnostic_*.png',
            '!FMath::IsFinite(LoadProgress)',
            'LoadProgress < 0.0f',
            'LoadProgress > 100.0f',
            'HasStableHybridVisualPolicy(Policy)',
            'providerSiteClipActive=%s',
            'authoredCoreVisualsVisible=%s',
            'stableVisualPolicy=%s',
            'Viewport->GetSceneHDREnabled()',
            'IFileManager::Get().FileSize(*Destination) >= 0',
            'Config.SetResolution(2560, 1440, 1.0f)',
            'BeginHybridProofViewSuppression(',
            'RestoreHybridProofViewSuppression();',
            'NON-PROOF DIAGNOSTIC EVIDENCE',
            'cesiumLoadProgress=%.3f',
            'localFallbackVisible=%s',
            'localFallbackHidden=%s',
        ):
            self.assertIn(fragment, diagnostic)

        self.assertNotIn('LoadProgress < 98.0f', diagnostic)
        self.assertNotIn('ShouldRequestAerialProviderHandoff(', diagnostic)
        self.assertNotIn('ViewLocation.Z', diagnostic)
        suppress_index = diagnostic.index('BeginHybridProofViewSuppression(')
        request_index = diagnostic.index('Viewport->TakeHighResScreenShot()')
        self.assertLess(suppress_index, request_index)

    def test_visual_proof_capture_reversibly_filters_operator_debug_geometry(self) -> None:
        for fragment in (
            '#include "Components/LineBatchComponent.h"',
            '#include "TRIADDemoDroneActor.h"',
            'HumanOnlyOverlayComponentTag(TEXT("TRIADHumanOnlyOverlay"))',
            'Player->HiddenPrimitiveComponents.Add(Component)',
            'Player->HiddenActors.Add(Actor)',
            'Actor->IsA<ATRIADDemoDroneActor>()',
            'FScreenshotRequest::OnScreenshotRequestProcessed().AddStatic(',
            'FEditorDelegates::PrePIEEnded.AddStatic(',
            'RestoreHybridProofViewSuppression();',
            'without changing RF/simulation',
        ):
            self.assertIn(fragment, self.editor_cpp)

        capture_start = self.editor_cpp.index(
            'CaptureIstanaExploreV5DHybridPlayView('
        )
        capture = self.editor_cpp[capture_start:]
        suppress_index = capture.index('BeginHybridProofViewSuppression(')
        request_index = capture.index('Viewport->TakeHighResScreenShot()')
        self.assertLess(suppress_index, request_index)

        teleport_start = self.editor_cpp.index(
            'TeleportIstanaExploreV5DHybridPlayPawnForQa('
        )
        teleport_end = self.editor_cpp.index(
            'CaptureIstanaExploreV5DHybridPlayView(', teleport_start
        )
        teleport = self.editor_cpp[teleport_start:teleport_end]
        self.assertIn('EXPLORE_V5D_HYBRID_QA_TELEPORT_REFUSED_CAPTURE_ACTIVE', teleport)
        self.assertIn('RestoreHybridProofViewSuppression();', teleport)

    def test_exact_qa_view_pose_is_transient_stable_and_readback_gated(self) -> None:
        endpoint, _ = extract_braced_block(
            self.editor_cpp,
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::\n'
            '    SetIstanaExploreV5DHybridPlayViewPoseForQa(',
        )
        for fragment in (
            'FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot',
            'EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_REFUSED_CAPTURE_ACTIVE',
            'RestoreHybridProofViewSuppression();',
            'ClearHybridExactQaViewPose();',
            'GetValidatedHybridPlayState(',
            'HasStableHybridVisualPolicy(Policy)',
            'Pawn->SetActorTickEnabled(false);',
            'WorldViewLocationCentimeters - ExactQaViewCameraRelativeLocation',
            'FRotator::ZeroRotator',
            'Camera->SetRelativeLocation(ExactQaViewCameraRelativeLocation);',
            'Camera->SetRelativeRotation(NormalizedViewRotation);',
            'CameraManager->UpdateCamera(0.0f);',
            'Player->GetPlayerViewPoint(ActualViewLocation, ActualViewRotation);',
            'ExactQaViewLocationToleranceCentimeters',
            'ExactQaViewRotationToleranceDegrees',
            'GHybridExactQaViewPose.Pawn = Pawn;',
            'EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_PASS',
            'exactQaViewPose=true',
        ):
            self.assertIn(fragment, endpoint)

        self.assertIn(
            'const FVector ExactQaViewCameraRelativeLocation(0.0, 0.0, 64.0);',
            self.editor_cpp,
        )
        self.assertIn(
            'constexpr float ExactQaViewLocationToleranceCentimeters = 0.1f;',
            self.editor_cpp,
        )
        self.assertIn(
            'constexpr float ExactQaViewRotationToleranceDegrees = 0.05f;',
            self.editor_cpp,
        )
        for forbidden_persistent_mutation in (
            'Modify(',
            'MarkPackageDirty',
            'SaveMap(',
            'SavePackage(',
        ):
            self.assertNotIn(forbidden_persistent_mutation, endpoint)

        state_report, _ = extract_braced_block(
            self.editor_cpp,
            'GetIstanaExploreV5DHybridPlayStateReport(FString& OutReport)',
        )
        for report_fragment in (
            'viewRotationDeg=%s',
            'exactQaViewPose=%s',
            'GHybridExactQaViewPose.Pawn.Get() == Pawn',
            'GHybridExactQaViewPose.WorldViewLocationCentimeters -',
            'bExactQaViewPose ? TEXT("true") : TEXT("false")',
        ):
            self.assertIn(report_fragment, state_report)

        historical_teleport, _ = extract_braced_block(
            self.editor_cpp,
            'TeleportIstanaExploreV5DHybridPlayPawnForQa(',
        )
        self.assertIn('ClearHybridExactQaViewPose();', historical_teleport)
        self.assertNotIn('GHybridExactQaViewPose.Pawn = Pawn;', historical_teleport)

        for legacy_camera_fragment in (
            'const FVector LegacyQaCameraRelativeLocation(0.0, 0.0, 64.0);',
            'const FRotator LegacyQaCameraRelativeRotation(-5.0, 0.0, 0.0);',
            'UCameraComponent* Camera = Pawn->GetExploreCameraComponent();',
            'EXPLORE_V5D_HYBRID_QA_TELEPORT_REFUSED_CAMERA',
            'Camera->SetRelativeLocation(LegacyQaCameraRelativeLocation);',
            'Camera->SetRelativeRotation(LegacyQaCameraRelativeRotation);',
            'Camera->GetRelativeLocation().Equals(',
            'Camera->GetRelativeRotation().Equals(',
            'exact legacy camera-relative pose',
        ):
            self.assertIn(
                legacy_camera_fragment,
                self.editor_cpp
                if legacy_camera_fragment.startswith('const ')
                else historical_teleport,
            )

        camera_lookup = historical_teleport.index(
            'UCameraComponent* Camera = Pawn->GetExploreCameraComponent();'
        )
        camera_null_gate = historical_teleport.index(
            'if (!Camera)', camera_lookup
        )
        restore_location = historical_teleport.index(
            'Camera->SetRelativeLocation(LegacyQaCameraRelativeLocation);',
            camera_null_gate,
        )
        restore_rotation = historical_teleport.index(
            'Camera->SetRelativeRotation(LegacyQaCameraRelativeRotation);',
            restore_location,
        )
        pawn_move = historical_teleport.index(
            'Pawn->SetActorLocationAndRotation(', restore_rotation
        )
        camera_location_readback = historical_teleport.index(
            'Camera->GetRelativeLocation().Equals(', pawn_move
        )
        camera_rotation_readback = historical_teleport.index(
            'Camera->GetRelativeRotation().Equals(', camera_location_readback
        )
        self.assertLess(camera_lookup, camera_null_gate)
        self.assertLess(camera_null_gate, restore_location)
        self.assertLess(restore_location, restore_rotation)
        self.assertLess(restore_rotation, pawn_move)
        self.assertLess(pawn_move, camera_location_readback)
        self.assertLess(camera_location_readback, camera_rotation_readback)

    def test_ground_realism_is_atomic_exact_and_runtime_validated(self) -> None:
        hook = (
            'ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder('
        )
        self.assertIn('TRIADIstanaExploreV5DGroundVegetationActor.h', self.editor_cpp)
        self.assertIn('TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h', self.editor_cpp)
        self.assertIn(hook, self.editor_cpp)
        self.assertIn('GroundVegetationCount != 1', self.editor_cpp)
        self.assertIn('GroundVegetation->HasActorBegunPlay()', self.editor_cpp)
        self.assertIn(
            'GroundVegetation->ValidateGroundVegetationRealism(',
            self.editor_cpp,
        )

        hook_index = self.editor_cpp.index(hook)
        late_destination_index = self.editor_cpp.index(
            'if (DestinationExists())',
            hook_index,
        )
        save_index = self.editor_cpp.index(
            'UEditorLoadingAndSavingUtils::SaveMap(',
            hook_index,
        )
        self.assertLess(hook_index, late_destination_index)
        self.assertLess(hook_index, save_index)

    def test_inherited_v5_appearance_is_in_the_cold_and_runtime_aggregate(self) -> None:
        cold_start = self.editor_cpp.index('bool ValidateHybridWorld(')
        cold_end = self.editor_cpp.index(
            'bool GetValidatedHybridPlayState(', cold_start
        )
        cold_validation = self.editor_cpp[cold_start:cold_end]
        pie_start = cold_end
        pie_end = self.editor_cpp.index(
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::', pie_start
        )
        pie_validation = self.editor_cpp[pie_start:pie_end]

        self.assertIn('TRIADIstanaExploreV5AppearanceActor.h', self.editor_cpp)
        for validation in (cold_validation, pie_validation):
            self.assertIn('int32 V5Count = 0;', validation)
            self.assertIn(
                'FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>',
                validation,
            )
            self.assertIn('V5Count != 1', validation)
            self.assertIn('V5->ExploreV4LandscapeActor != V4', validation)
            self.assertIn('V5->ValidateExploreV5Appearance(', validation)

        self.assertIn('World->IsGameWorld())', cold_validation)
        self.assertIn('V5->HasActorBegunPlay()', pie_validation)
        self.assertIn(
            'V5->ValidateExploreV5Appearance(V5AppearanceReport, true)',
            pie_validation,
        )

    def test_tree_realism_is_atomic_exact_and_runtime_validated(self) -> None:
        hook = 'ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder('
        cold_start = self.editor_cpp.index('bool ValidateHybridWorld(')
        cold_end = self.editor_cpp.index(
            'bool GetValidatedHybridPlayState(', cold_start
        )
        cold_validation = self.editor_cpp[cold_start:cold_end]
        pie_start = cold_end
        pie_end = self.editor_cpp.index(
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::', pie_start
        )
        pie_validation = self.editor_cpp[pie_start:pie_end]
        builder_start = self.editor_cpp.index(
            'BuildIstanaExploreV5DHybridMap(FString& OutMessage)'
        )
        builder_end = self.editor_cpp.index(
            'ValidateIstanaExploreV5DHybridMap(', builder_start
        )
        builder = self.editor_cpp[builder_start:builder_end]

        self.assertIn('TRIADIstanaExploreV5DTreeRealismActor.h', self.editor_cpp)
        self.assertIn('TRIADIstanaExploreV4LandscapeActor.h', self.editor_cpp)
        self.assertIn(
            'TRIADIstanaExploreV5DTreeRealismEditorLibrary.h',
            self.editor_cpp,
        )
        self.assertIn(hook, builder)

        for validation in (cold_validation, pie_validation):
            self.assertIn('int32 V4Count = 0;', validation)
            self.assertIn(
                'FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>',
                validation,
            )
            self.assertIn('V4Count != 1', validation)
            self.assertIn('TreeRealismCount != 1', validation)
            self.assertIn('TreeRealism->Tags.Contains(', validation)
            self.assertIn('ExpectedActorTag()', validation)
            self.assertIn('TreeRealism->V4LandscapeActor != V4', validation)
            self.assertIn('TreeRealism->V5BVisualActor != V5B', validation)
            self.assertIn('TreeRealism->ValidateTreeRealism(', validation)

        self.assertIn('TreeRealism->HasActorBegunPlay()', pie_validation)
        self.assertIn('int32 V4Count = 0;', builder)
        self.assertIn(
            'FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>', builder
        )
        self.assertIn('V4Count != 1', builder)
        self.assertIn('ExistingTreeRealismCount != 0', builder)

        materials_index = builder.index(
            'EnsureGroundVegetationRealismMaterialAssets('
        )
        hook_index = builder.index(hook)
        ground_hook_index = builder.index(
            'ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder('
        )
        late_destination_index = builder.index(
            'if (DestinationExists())', ground_hook_index
        )
        save_index = builder.index(
            'UEditorLoadingAndSavingUtils::SaveMap(', ground_hook_index
        )
        self.assertLess(materials_index, hook_index)
        self.assertLess(hook_index, ground_hook_index)
        self.assertLess(ground_hook_index, late_destination_index)
        self.assertLess(ground_hook_index, save_index)
        self.assertLess(hook_index, late_destination_index)
        self.assertLess(hook_index, save_index)

    def test_dynamic_range_is_atomic_exact_and_runtime_validated(self) -> None:
        hook = 'ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder('
        cold_start = self.editor_cpp.index('bool ValidateHybridWorld(')
        cold_end = self.editor_cpp.index(
            'bool GetValidatedHybridPlayState(', cold_start
        )
        cold_validation = self.editor_cpp[cold_start:cold_end]
        pie_start = cold_end
        pie_end = self.editor_cpp.index(
            'bool UTRIADIstanaExploreV5DHybridEditorLibrary::', pie_start
        )
        pie_validation = self.editor_cpp[pie_start:pie_end]
        builder_start = self.editor_cpp.index(
            'BuildIstanaExploreV5DHybridMap(FString& OutMessage)'
        )
        builder_end = self.editor_cpp.index(
            'ValidateIstanaExploreV5DHybridMap(', builder_start
        )
        builder = self.editor_cpp[builder_start:builder_end]

        self.assertIn(
            'TRIADIstanaExploreV5DDynamicRangeRealismActor.h',
            self.editor_cpp,
        )
        self.assertIn(
            'TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.h',
            self.editor_cpp,
        )
        self.assertIn(hook, builder)
        for validation in (cold_validation, pie_validation):
            self.assertIn('int32 DynamicRangeCount = 0;', validation)
            self.assertIn(
                'FindExactlyOne<ATRIADIstanaExploreV5DDynamicRangeRealismActor>',
                validation,
            )
            self.assertIn('DynamicRangeCount != 1', validation)
            self.assertIn('DynamicRange->Tags.Contains(', validation)
            self.assertIn('DynamicRange->ValidateDynamicRangeRealism(', validation)

        self.assertIn('World->IsGameWorld()', cold_validation)
        self.assertIn('DynamicRange->HasActorBegunPlay()', pie_validation)
        self.assertIn('true,\n            DynamicRangeReport', pie_validation)
        self.assertIn('ExistingDynamicRangeCount != 0', builder)

        tree_hook_index = builder.index(
            'ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder('
        )
        hook_index = builder.index(hook)
        late_destination_index = builder.index(
            'if (DestinationExists())',
            hook_index,
        )
        save_index = builder.index(
            'UEditorLoadingAndSavingUtils::SaveMap(',
            hook_index,
        )
        self.assertLess(tree_hook_index, hook_index)
        self.assertLess(hook_index, late_destination_index)
        self.assertLess(hook_index, save_index)

    def test_landmark_presence_policies_are_explicit_and_builder_is_final(self) -> None:
        for marker in (
            'enum class ELandmarkPresencePolicy',
            'ELandmarkPresencePolicy::Forbidden',
            'ELandmarkPresencePolicy::Optional',
            'ELandmarkPresencePolicy::Required',
            'MatchesLandmarkPresence(MacDonaldHouseCount, MacDonaldHousePolicy)',
            'MatchesLandmarkPresence(TemasekShophouseCount, TemasekShophousePolicy)',
        ):
            self.assertIn(marker, self.editor_cpp)
        builder, _ = extract_braced_block(
            self.editor_cpp,
            'BuildIstanaExploreV5DHybridMap(FString& OutMessage)',
        )
        for marker in (
            'ExistingTemasekShophouseCount != 0',
            'Target->SpawnActor<ATRIADIstanaExploreV5DTemasekShophouseActor>',
            'TemasekShophouse->ConfigureTemasekShophouse(',
            'ELandmarkPresencePolicy::Required',
        ):
            self.assertIn(marker, builder)
