#include "TRIADIstanaExploreV5DHybridEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Cesium3DTileset.h"
#include "CesiumCartographicPolygon.h"
#include "CesiumGeoreference.h"
#include "CesiumIonServer.h"
#include "CesiumPolygonRasterOverlay.h"
#include "Components/LineBatchComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "HighResScreenshot.h"
#include "ImageUtils.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADDemoDroneActor.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5AppearanceActor.h"
#include "TRIADIstanaExploreV5BEditorLibrary.h"
#include "TRIADIstanaExploreV5BVisualActor.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"
#include "TRIADIstanaExploreV5DDynamicRangeRealismActor.h"
#include "TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.h"
#include "TRIADIstanaExploreV5DFountainRealismActor.h"
#include "TRIADIstanaExploreV5DFountainMaterialFactory.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h"
#include "TRIADIstanaExploreV5DMacDonaldHouseActor.h"
#include "TRIADIstanaExploreV5DMacDonaldHouseAssetFactory.h"
#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.h"
#include "TRIADIstanaExploreV5DTemasekShophouseActor.h"
#include "TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h"
#include "TRIADIstanaExploreV5DPublicRealmActor.h"
#include "TRIADIstanaExploreV5DPublicRealmAssetFactory.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "TRIADIstanaExploreV5DTreeRealismEditorLibrary.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaExploreV5Pawn.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "Ssl.h"
#include "SceneView.h"
#include "UnrealClient.h"
#include "UnrealEngine.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"

#if WITH_SSL
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include <openssl/sha.h>
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#endif

namespace
{
const FString SourceMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5b"));
const FString DestinationMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid.Istana_PublicView_Explore_v5d_hybrid"));
constexpr int64 PrePublicRealmDestinationBytes = 37409759;
const FString PrePublicRealmDestinationSha256(
    TEXT("56A2A3B32457C3E34D74CA13C131C5420C0582BBC183BF0B891D0F760E85B842"));
constexpr int64 PrePublicRealmMaterialCorrectionBytes = 37414472;
const FString PrePublicRealmMaterialCorrectionSha256(
    TEXT("CDBEDE32ACBCD18D1DFCE03D01F4BA75CFE57063EE34AA2EAA89629B0E9B6080"));
// Measured only after the first correction save cold-reload-validated. These
// exact bytes now gate both idempotence and every future predecessor upgrade.
constexpr int64 CorrectedPublicRealmMaterialCorrectionBytes = 37414654;
const FString CorrectedPublicRealmMaterialCorrectionSha256(
    TEXT("154732F334F1CFBB45B61F8777EBD8D788599B814EFCB265950D24ADA0BA3A90"));
constexpr int64 PreInheritedPlanningGroundSuppressionBytes = 37414654;
const FString PreInheritedPlanningGroundSuppressionSha256(
    TEXT("154732F334F1CFBB45B61F8777EBD8D788599B814EFCB265950D24ADA0BA3A90"));
// Measured only after the first suppression save passed clean cold validation.
// These exact bytes gate idempotence and every later map successor.
constexpr int64 CorrectedInheritedPlanningGroundSuppressionBytes = 37414604;
const FString CorrectedInheritedPlanningGroundSuppressionSha256(
    TEXT("E01C5ECB476E9723F3FB85AFFE101EE4CEBC719FD493E40C26E3BC85584FFAAB"));
constexpr int64 PreProviderQualityPassBytes = 34991378;
const FString PreProviderQualityPassSha256(
    TEXT("57CA4A4C2440454E1F903ECCA166A6BBB566A2CE0E535AB26FFA62818F1D17C2"));
// Exact final V5D map admitted for the provider-throttle successor. This
// receipt is deliberately independent from the earlier provider-quality
// migration so the latter's verified backup remains immutable.
constexpr int64 PreProviderThrottleSuccessorBytes = 34992305;
const FString PreProviderThrottleSuccessorSha256(
    TEXT("053DE9DE459EAACB8B60BA9DCCEC356D40F1114CE39D0A098A04306327EE40ED"));
constexpr int32 PreProviderThrottleSimultaneousLoads = 64;
// Exact, cold-validated provider-throttle successor promoted on 2026-09-05.
// LocalFallbackSuppressionV2 may migrate only this one predecessor receipt.
constexpr int64 PreLocalFallbackSuppressionV2Bytes = 34992354;
const FString PreLocalFallbackSuppressionV2Sha256(
    TEXT("859734CB9EFCB429AE7D863E677B7B370CC897EB9C100AACA7AAD6F227805815"));
// Exact cold-validated R24B map currently admitted for the additive R25
// context-facade material-only migration.
constexpr int64 PreContextFacadeR25Bytes = 34992354;
const FString PreContextFacadeR25Sha256(
    TEXT("4A5F5514C7C3B508567465BA1F3B2FE8F31F4DAAAC5E317C8C57F1C30B50FD08"));
// Exact cold-validated R25 context-facade successor admitted for the
// additive, render-only R26 landmark-vegetation migration.
constexpr int64 PreLandmarkVegetationR26Bytes = 34993427;
const FString PreLandmarkVegetationR26Sha256(
    TEXT("38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9"));
const FName HybridContextPolicyTag(TEXT("TRIADIstanaExploreV5DContextPolicy"));
const FName HybridVisualTilesetTag(TEXT("TRIADIstanaExploreV5DVisualTileset"));
const FName HybridGeoreferenceTag(TEXT("TRIADIstanaExploreV5DGeoreference"));
const FName HybridSiteClipTag(TEXT("TRIADIstanaExploreV5DProviderSiteClip"));
const FName CesiumDefaultGeoreferenceTag(TEXT("DEFAULT_GEOREFERENCE"));
const FName HumanOnlyOverlayComponentTag(TEXT("TRIADHumanOnlyOverlay"));
// The legacy pawn view starts with the camera mount authored by
// ATRIADIstanaFreeRoamPawn and the exact -5 degree PlayerStart pitch that
// BeginPlay transfers onto the camera. Reassert both before every historical
// pawn teleport so a preceding exact-view QA pose cannot leak yaw/roll into it.
const FVector LegacyQaCameraRelativeLocation(0.0, 0.0, 64.0);
const FRotator LegacyQaCameraRelativeRotation(-5.0, 0.0, 0.0);
const FVector ExactQaViewCameraRelativeLocation(0.0, 0.0, 64.0);
constexpr float ExactQaViewLocationToleranceCentimeters = 0.1f;
constexpr float ExactQaViewRotationToleranceDegrees = 0.05f;
constexpr int64 GooglePhotorealistic3DTilesIonAssetId = 2275207;
constexpr double IstanaLongitudeDegrees = 103.84288055;
constexpr double IstanaLatitudeDegrees = 1.30709615;
constexpr double IstanaFallbackEllipsoidHeightMeters = 47.0;
constexpr int64 HybridCacheBytes = 2LL * 1024LL * 1024LL * 1024LL;
constexpr int32 HybridSimultaneousLoads = 12;
constexpr int32 HybridLoadingDescendantLimit = 20;
constexpr double HybridCulledScreenSpaceError = 8.0;
const TCHAR* CesiumOpaqueClippingMaterialPath =
    TEXT("/CesiumForUnreal/Materials/Instances/MI_CesiumThreeOverlaysAndClipping.MI_CesiumThreeOverlaysAndClipping");
const TCHAR* CesiumTranslucentClippingMaterialPath =
    TEXT("/CesiumForUnreal/Materials/Instances/MI_CesiumThreeOverlaysAndClippingTranslucent.MI_CesiumThreeOverlaysAndClippingTranslucent");
constexpr int32 RequiredNaniteMaterialCount = 9;
const TCHAR* const RequiredNaniteMaterialPackagePaths[
    RequiredNaniteMaterialCount] =
{
    TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafDark"),
    TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafMid"),
    TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_LeafLight"),
    TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Bark"),
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5/M_IPV_HeroSurface_V5"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/M_IPV5_Grass001_Lawn_Base"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings/Materials/M_IPV5C_ContextMassing_Master"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_LawnMacroVariation"),
    TEXT("/Game/TRIAD/IstanaPublicView/Materials/M_IPV_Lawn"),
};

struct FHybridProofViewSuppressionState
{
    TWeakObjectPtr<APlayerController> Player;
    TArray<TWeakObjectPtr<UPrimitiveComponent>> AddedHiddenComponents;
    TArray<TWeakObjectPtr<AActor>> AddedHiddenActors;
    FDelegateHandle ScreenshotProcessedHandle;
    FDelegateHandle PrePieEndedHandle;
};

// High-resolution screenshots complete on a later viewport frame. Keep the
// Player0-only view exclusions alive until completion; they never change
// Cesium overlays, actor visibility, or runtime policy state.
FHybridProofViewSuppressionState GHybridProofViewSuppression;

struct FHybridExactQaViewPoseState
{
    TWeakObjectPtr<ATRIADIstanaExploreV5Pawn> Pawn;
    FVector WorldViewLocationCentimeters = FVector::ZeroVector;
    FRotator WorldViewRotationDegrees = FRotator::ZeroRotator;
};

FHybridExactQaViewPoseState GHybridExactQaViewPose;

void ClearHybridExactQaViewPose()
{
    GHybridExactQaViewPose.Pawn.Reset();
    GHybridExactQaViewPose.WorldViewLocationCentimeters = FVector::ZeroVector;
    GHybridExactQaViewPose.WorldViewRotationDegrees = FRotator::ZeroRotator;
}

void RestoreHybridProofViewSuppression()
{
    if (GHybridProofViewSuppression.ScreenshotProcessedHandle.IsValid())
    {
        FScreenshotRequest::OnScreenshotRequestProcessed().Remove(
            GHybridProofViewSuppression.ScreenshotProcessedHandle);
        GHybridProofViewSuppression.ScreenshotProcessedHandle.Reset();
    }
    if (GHybridProofViewSuppression.PrePieEndedHandle.IsValid())
    {
        FEditorDelegates::PrePIEEnded.Remove(
            GHybridProofViewSuppression.PrePieEndedHandle);
        GHybridProofViewSuppression.PrePieEndedHandle.Reset();
    }
    if (APlayerController* Player = GHybridProofViewSuppression.Player.Get())
    {
        for (const TWeakObjectPtr<UPrimitiveComponent>& WeakComponent :
             GHybridProofViewSuppression.AddedHiddenComponents)
        {
            if (UPrimitiveComponent* Component = WeakComponent.Get())
            {
                Player->HiddenPrimitiveComponents.RemoveAll(
                    [Component](const TWeakObjectPtr<UPrimitiveComponent>& Hidden)
                    {
                        return Hidden.Get() == Component;
                    });
            }
        }
        for (const TWeakObjectPtr<AActor>& WeakActor :
             GHybridProofViewSuppression.AddedHiddenActors)
        {
            if (AActor* Actor = WeakActor.Get())
            {
                Player->HiddenActors.Remove(Actor);
            }
        }
    }
    GHybridProofViewSuppression.Player.Reset();
    GHybridProofViewSuppression.AddedHiddenComponents.Reset();
    GHybridProofViewSuppression.AddedHiddenActors.Reset();
}

void OnHybridProofScreenshotProcessed()
{
    RestoreHybridProofViewSuppression();
}

void OnHybridProofPrePieEnded(bool)
{
    RestoreHybridProofViewSuppression();
}

void AddHybridProofHiddenComponent(
    APlayerController* Player,
    UPrimitiveComponent* Component,
    int32& OutAddedCount)
{
    if (!IsValid(Player) || !IsValid(Component) ||
        Player->HiddenPrimitiveComponents.ContainsByPredicate(
            [Component](const TWeakObjectPtr<UPrimitiveComponent>& Hidden)
            {
                return Hidden.Get() == Component;
            }))
    {
        return;
    }

    Player->HiddenPrimitiveComponents.Add(Component);
    GHybridProofViewSuppression.AddedHiddenComponents.Add(Component);
    ++OutAddedCount;
}

bool BeginHybridProofViewSuppression(
    UWorld* World,
    APlayerController* Player,
    int32& OutLineBatcherCount,
    int32& OutHumanOverlayCount,
    int32& OutDemoTargetCount)
{
    RestoreHybridProofViewSuppression();
    OutLineBatcherCount = 0;
    OutHumanOverlayCount = 0;
    OutDemoTargetCount = 0;
    if (!IsValid(World) || !IsValid(Player))
    {
        return false;
    }

    GHybridProofViewSuppression.Player = Player;
    for (TObjectIterator<ULineBatchComponent> It; It; ++It)
    {
        ULineBatchComponent* LineBatcher = *It;
        if (IsValid(LineBatcher) && LineBatcher->GetWorld() == World)
        {
            AddHybridProofHiddenComponent(
                Player,
                LineBatcher,
                OutLineBatcherCount);
        }
    }

    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        AActor* Actor = *ActorIt;
        if (!IsValid(Actor))
        {
            continue;
        }

        if (Actor->IsA<ATRIADDemoDroneActor>() &&
            !Player->HiddenActors.Contains(Actor))
        {
            Player->HiddenActors.Add(Actor);
            GHybridProofViewSuppression.AddedHiddenActors.Add(Actor);
            ++OutDemoTargetCount;
        }

        TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
        for (UPrimitiveComponent* Component : PrimitiveComponents)
        {
            if (IsValid(Component) &&
                Component->ComponentHasTag(HumanOnlyOverlayComponentTag))
            {
                AddHybridProofHiddenComponent(
                    Player,
                    Component,
                    OutHumanOverlayCount);
            }
        }
    }

    GHybridProofViewSuppression.ScreenshotProcessedHandle =
        FScreenshotRequest::OnScreenshotRequestProcessed().AddStatic(
            &OnHybridProofScreenshotProcessed);
    GHybridProofViewSuppression.PrePieEndedHandle =
        FEditorDelegates::PrePIEEnded.AddStatic(&OnHybridProofPrePieEnded);
    return GHybridProofViewSuppression.ScreenshotProcessedHandle.IsValid() &&
        GHybridProofViewSuppression.PrePieEndedHandle.IsValid();
}

template <typename T>
T* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    T* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<T> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            Result = *It;
            ++OutCount;
        }
    }
    return Result;
}

bool DestinationExists()
{
    FString Filename;
    return FPackageName::DoesPackageExist(DestinationMapPackage, &Filename) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath);
}

bool HashFileSha256(
    const FString& Filename,
    FString& OutSha256,
    int64& OutBytes,
    FString& OutError)
{
    OutSha256.Reset();
    OutBytes = INDEX_NONE;
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
    {
        OutError = TEXT("Could not read the V5D migration file: ") + Filename;
        return false;
    }
    OutBytes = Bytes.Num();
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr)
    {
        OutError = TEXT("Could not compute the V5D migration SHA-256: ") +
            Filename;
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    OutSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5D migration SHA-256 admission requires WITH_SSL.");
    return false;
#endif
    OutError.Reset();
    return true;
}

FString GetPreContextFacadeR25BackupFilename()
{
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/V5DContextFacadeR25_20260906")));
    FString BackupFilename = FPaths::Combine(
        BackupDirectory,
        TEXT("Istana_PublicView_Explore_v5d_hybrid_4A5F5514C7C3.umap"));
    FPaths::NormalizeFilename(BackupFilename);
    return BackupFilename;
}

bool CreateVerifiedPreContextFacadeR25MapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    OutBackupFilename = GetPreContextFacadeR25BackupFilename();
    if (!IFileManager::Get().MakeDirectory(
            *FPaths::GetPath(OutBackupFilename), true))
    {
        OutError = TEXT("Could not create the exact R25 context-facade backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting R25 context-facade predecessor backup.");
        return false;
    }
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreContextFacadeR25Bytes ||
        BackupSha256 != PreContextFacadeR25Sha256)
    {
        OutError = FString::Printf(
            TEXT("The R25 context-facade predecessor backup failed exact verification: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PreContextFacadeR25Bytes,
            BackupBytes,
            *PreContextFacadeR25Sha256,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreVerifiedPreContextFacadeR25MapViaSiblingTemp(
    const FString& BackupFilename,
    const FString& DestinationFilename,
    FString& OutError)
{
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreContextFacadeR25Bytes ||
        BackupSha256 != PreContextFacadeR25Sha256)
    {
        OutError = TEXT("R25 context-facade restore refused an unverified predecessor backup. ") +
            OutError;
        return false;
    }

    const FString DestinationDirectory = FPaths::GetPath(DestinationFilename);
    FString SiblingTemporary = FPaths::CreateTempFilename(
        *DestinationDirectory,
        TEXT("TRIAD_ContextFacadeR25_Restore_"),
        TEXT(".tmp"));
    FPaths::NormalizeFilename(SiblingTemporary);
    if (!FPaths::IsSamePath(
            FPaths::GetPath(SiblingTemporary), DestinationDirectory) ||
        IFileManager::Get().FileExists(*SiblingTemporary))
    {
        OutError = TEXT("R25 context-facade restore could not reserve a verified sibling temporary path.");
        return false;
    }
    ON_SCOPE_EXIT
    {
        if (IFileManager::Get().FileExists(*SiblingTemporary))
        {
            IFileManager::Get().Delete(
                *SiblingTemporary, false, true, true);
        }
    };
    if (IFileManager::Get().Copy(
            *SiblingTemporary,
            *BackupFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("R25 context-facade restore could not copy the backup to its verified sibling temporary.");
        return false;
    }
    FString TemporarySha256;
    int64 TemporaryBytes = INDEX_NONE;
    if (!HashFileSha256(
            SiblingTemporary,
            TemporarySha256,
            TemporaryBytes,
            OutError) ||
        TemporaryBytes != PreContextFacadeR25Bytes ||
        TemporarySha256 != PreContextFacadeR25Sha256)
    {
        OutError = TEXT("R25 context-facade restore sibling temporary failed exact verification. ") +
            OutError;
        return false;
    }
    if (!IFileManager::Get().Move(
            *DestinationFilename,
            *SiblingTemporary,
            true,
            true,
            false,
            false))
    {
        OutError = TEXT("R25 context-facade restore could not replace the destination from the verified sibling temporary; the backup remains preserved.");
        return false;
    }

    FString RestoredSha256;
    int64 RestoredBytes = INDEX_NONE;
    FString RestoredHashError;
    FString PreservedBackupSha256;
    int64 PreservedBackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bDestinationRestored = HashFileSha256(
        DestinationFilename,
        RestoredSha256,
        RestoredBytes,
        RestoredHashError) &&
        RestoredBytes == PreContextFacadeR25Bytes &&
        RestoredSha256 == PreContextFacadeR25Sha256;
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        PreservedBackupSha256,
        PreservedBackupBytes,
        BackupHashError) &&
        PreservedBackupBytes == PreContextFacadeR25Bytes &&
        PreservedBackupSha256 == PreContextFacadeR25Sha256;
    if (!bDestinationRestored || !bBackupPreserved)
    {
        OutError = FString::Printf(
            TEXT("R25 context-facade sibling-temp restore post-move verification failed: destinationRestored=%s restoredBytes=%lld restoredSha256=%s backupPreserved=%s restoredHash={%s} backupHash={%s}."),
            bDestinationRestored ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *RestoredHashError,
            *BackupHashError);
        return false;
    }
    OutError.Reset();
    return true;
}

FString GetPreLandmarkVegetationR26BackupFilename()
{
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/V5DLandmarkVegetationR26_20260906")));
    FString BackupFilename = FPaths::Combine(
        BackupDirectory,
        TEXT("Istana_PublicView_Explore_v5d_hybrid_38114240B7A0.umap"));
    FPaths::NormalizeFilename(BackupFilename);
    return BackupFilename;
}

bool CreateVerifiedPreLandmarkVegetationR26MapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    OutBackupFilename = GetPreLandmarkVegetationR26BackupFilename();
    if (!IFileManager::Get().MakeDirectory(
            *FPaths::GetPath(OutBackupFilename), true))
    {
        OutError = TEXT("Could not create the exact R26 landmark-vegetation backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting R26 landmark-vegetation predecessor backup.");
        return false;
    }

    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreLandmarkVegetationR26Bytes ||
        BackupSha256 != PreLandmarkVegetationR26Sha256)
    {
        OutError = FString::Printf(
            TEXT("The R26 landmark-vegetation predecessor backup failed exact verification: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PreLandmarkVegetationR26Bytes,
            BackupBytes,
            *PreLandmarkVegetationR26Sha256,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreVerifiedPreLandmarkVegetationR26MapViaSiblingTemp(
    const FString& BackupFilename,
    const FString& DestinationFilename,
    FString& OutError)
{
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreLandmarkVegetationR26Bytes ||
        BackupSha256 != PreLandmarkVegetationR26Sha256)
    {
        OutError = TEXT("R26 landmark-vegetation restore refused an unverified predecessor backup. ") +
            OutError;
        return false;
    }

    const FString DestinationDirectory = FPaths::GetPath(DestinationFilename);
    FString SiblingTemporary = FPaths::CreateTempFilename(
        *DestinationDirectory,
        TEXT("TRIAD_LandmarkVegetationR26_Restore_"),
        TEXT(".tmp"));
    FPaths::NormalizeFilename(SiblingTemporary);
    if (!FPaths::IsSamePath(
            FPaths::GetPath(SiblingTemporary), DestinationDirectory) ||
        IFileManager::Get().FileExists(*SiblingTemporary))
    {
        OutError = TEXT("R26 landmark-vegetation restore could not reserve a verified sibling temporary path.");
        return false;
    }
    ON_SCOPE_EXIT
    {
        if (IFileManager::Get().FileExists(*SiblingTemporary))
        {
            IFileManager::Get().Delete(
                *SiblingTemporary, false, true, true);
        }
    };

    if (IFileManager::Get().Copy(
            *SiblingTemporary,
            *BackupFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("R26 landmark-vegetation restore could not copy the backup to its verified sibling temporary.");
        return false;
    }
    FString TemporarySha256;
    int64 TemporaryBytes = INDEX_NONE;
    if (!HashFileSha256(
            SiblingTemporary,
            TemporarySha256,
            TemporaryBytes,
            OutError) ||
        TemporaryBytes != PreLandmarkVegetationR26Bytes ||
        TemporarySha256 != PreLandmarkVegetationR26Sha256)
    {
        OutError = TEXT("R26 landmark-vegetation restore sibling temporary failed exact verification. ") +
            OutError;
        return false;
    }
    if (!IFileManager::Get().Move(
            *DestinationFilename,
            *SiblingTemporary,
            true,
            true,
            false,
            false))
    {
        OutError = TEXT("R26 landmark-vegetation restore could not replace the destination from the verified sibling temporary; the backup remains preserved.");
        return false;
    }

    FString RestoredSha256;
    int64 RestoredBytes = INDEX_NONE;
    FString RestoredHashError;
    FString PreservedBackupSha256;
    int64 PreservedBackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bDestinationRestored = HashFileSha256(
        DestinationFilename,
        RestoredSha256,
        RestoredBytes,
        RestoredHashError) &&
        RestoredBytes == PreLandmarkVegetationR26Bytes &&
        RestoredSha256 == PreLandmarkVegetationR26Sha256;
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        PreservedBackupSha256,
        PreservedBackupBytes,
        BackupHashError) &&
        PreservedBackupBytes == PreLandmarkVegetationR26Bytes &&
        PreservedBackupSha256 == PreLandmarkVegetationR26Sha256;
    if (!bDestinationRestored || !bBackupPreserved)
    {
        OutError = FString::Printf(
            TEXT("R26 landmark-vegetation sibling-temp restore post-move verification failed: destinationRestored=%s restoredBytes=%lld restoredSha256=%s backupPreserved=%s restoredHash={%s} backupHash={%s}."),
            bDestinationRestored ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *RestoredHashError,
            *BackupHashError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateVerifiedPreProviderQualityMapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/V5DProviderQualityPass_20260905")));
    OutBackupFilename = FPaths::Combine(
        BackupDirectory,
        TEXT("Istana_PublicView_Explore_v5d_hybrid_57CA4A4C2440.umap"));
    FPaths::NormalizeFilename(OutBackupFilename);
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutError = TEXT("Could not create the exact V5D provider-quality backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting V5D provider-quality predecessor backup.");
        return false;
    }

    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreProviderQualityPassBytes ||
        BackupSha256 != PreProviderQualityPassSha256)
    {
        OutError = FString::Printf(
            TEXT("The V5D provider-quality predecessor backup failed exact verification: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PreProviderQualityPassBytes,
            BackupBytes,
            *PreProviderQualityPassSha256,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreVerifiedPreProviderQualityMapViaSiblingTemp(
    const FString& BackupFilename,
    const FString& DestinationFilename,
    FString& OutError)
{
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreProviderQualityPassBytes ||
        BackupSha256 != PreProviderQualityPassSha256)
    {
        OutError = TEXT("Provider-quality restore refused an unverified predecessor backup. ") +
            OutError;
        return false;
    }

    const FString DestinationDirectory = FPaths::GetPath(DestinationFilename);
    FString SiblingTemporary = FPaths::CreateTempFilename(
        *DestinationDirectory,
        TEXT("TRIAD_ProviderQuality_Restore_"),
        TEXT(".tmp"));
    FPaths::NormalizeFilename(SiblingTemporary);
    if (!FPaths::IsSamePath(
            FPaths::GetPath(SiblingTemporary),
            DestinationDirectory) ||
        IFileManager::Get().FileExists(*SiblingTemporary))
    {
        OutError = TEXT("Provider-quality restore could not reserve a verified sibling temporary path.");
        return false;
    }
    ON_SCOPE_EXIT
    {
        if (IFileManager::Get().FileExists(*SiblingTemporary))
        {
            IFileManager::Get().Delete(
                *SiblingTemporary,
                false,
                true,
                true);
        }
    };

    if (IFileManager::Get().Copy(
            *SiblingTemporary,
            *BackupFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Provider-quality restore could not copy the backup to its verified sibling temporary.");
        return false;
    }
    FString TemporarySha256;
    int64 TemporaryBytes = INDEX_NONE;
    if (!HashFileSha256(
            SiblingTemporary,
            TemporarySha256,
            TemporaryBytes,
            OutError) ||
        TemporaryBytes != PreProviderQualityPassBytes ||
        TemporarySha256 != PreProviderQualityPassSha256)
    {
        OutError = TEXT("Provider-quality restore sibling temporary failed exact verification. ") +
            OutError;
        return false;
    }
    if (!IFileManager::Get().Move(
            *DestinationFilename,
            *SiblingTemporary,
            true,
            true,
            false,
            false))
    {
        OutError = TEXT("Provider-quality restore could not replace the destination from the verified sibling temporary; the backup remains preserved.");
        return false;
    }

    FString RestoredSha256;
    int64 RestoredBytes = INDEX_NONE;
    FString RestoredHashError;
    FString PreservedBackupSha256;
    int64 PreservedBackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bDestinationRestored = HashFileSha256(
        DestinationFilename,
        RestoredSha256,
        RestoredBytes,
        RestoredHashError) &&
        RestoredBytes == PreProviderQualityPassBytes &&
        RestoredSha256 == PreProviderQualityPassSha256;
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        PreservedBackupSha256,
        PreservedBackupBytes,
        BackupHashError) &&
        PreservedBackupBytes == PreProviderQualityPassBytes &&
        PreservedBackupSha256 == PreProviderQualityPassSha256;
    if (!bDestinationRestored || !bBackupPreserved)
    {
        OutError = FString::Printf(
            TEXT("Provider-quality sibling-temp restore post-move verification failed: destinationRestored=%s restoredBytes=%lld restoredSha256=%s backupPreserved=%s restoredHash={%s} backupHash={%s}."),
            bDestinationRestored ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *RestoredHashError,
            *BackupHashError);
        return false;
    }
    OutError.Reset();
    return true;
}

FString GetPreProviderThrottleSuccessorBackupFilename()
{
    FString BackupFilename = FPaths::Combine(
        FPaths::ConvertRelativePathToFull(
            FPaths::Combine(
                FPaths::ProjectSavedDir(),
                TEXT("TRIAD/MapBackups/V5DProviderThrottleSuccessor_20260905"))),
        TEXT("Istana_PublicView_Explore_v5d_hybrid_053DE9DE459E.umap"));
    FPaths::NormalizeFilename(BackupFilename);
    return BackupFilename;
}

bool CreateVerifiedPreProviderThrottleSuccessorMapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    OutBackupFilename = GetPreProviderThrottleSuccessorBackupFilename();
    const FString BackupDirectory = FPaths::GetPath(OutBackupFilename);
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutError = TEXT("Could not create the exact V5D provider-throttle successor backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting V5D provider-throttle predecessor backup.");
        return false;
    }

    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreProviderThrottleSuccessorBytes ||
        BackupSha256 != PreProviderThrottleSuccessorSha256)
    {
        OutError = FString::Printf(
            TEXT("The V5D provider-throttle predecessor backup failed exact verification: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PreProviderThrottleSuccessorBytes,
            BackupBytes,
            *PreProviderThrottleSuccessorSha256,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreVerifiedPreProviderThrottleSuccessorMapViaSiblingTemp(
    const FString& BackupFilename,
    const FString& DestinationFilename,
    FString& OutError)
{
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreProviderThrottleSuccessorBytes ||
        BackupSha256 != PreProviderThrottleSuccessorSha256)
    {
        OutError = TEXT("Provider-throttle restore refused an unverified predecessor backup. ") +
            OutError;
        return false;
    }

    const FString DestinationDirectory = FPaths::GetPath(DestinationFilename);
    FString SiblingTemporary = FPaths::CreateTempFilename(
        *DestinationDirectory,
        TEXT("TRIAD_ProviderThrottle_Restore_"),
        TEXT(".tmp"));
    FPaths::NormalizeFilename(SiblingTemporary);
    if (!FPaths::IsSamePath(
            FPaths::GetPath(SiblingTemporary),
            DestinationDirectory) ||
        IFileManager::Get().FileExists(*SiblingTemporary))
    {
        OutError = TEXT("Provider-throttle restore could not reserve a verified sibling temporary path.");
        return false;
    }
    ON_SCOPE_EXIT
    {
        if (IFileManager::Get().FileExists(*SiblingTemporary))
        {
            IFileManager::Get().Delete(
                *SiblingTemporary,
                false,
                true,
                true);
        }
    };

    if (IFileManager::Get().Copy(
            *SiblingTemporary,
            *BackupFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Provider-throttle restore could not copy the backup to its verified sibling temporary.");
        return false;
    }
    FString TemporarySha256;
    int64 TemporaryBytes = INDEX_NONE;
    if (!HashFileSha256(
            SiblingTemporary,
            TemporarySha256,
            TemporaryBytes,
            OutError) ||
        TemporaryBytes != PreProviderThrottleSuccessorBytes ||
        TemporarySha256 != PreProviderThrottleSuccessorSha256)
    {
        OutError = TEXT("Provider-throttle restore sibling temporary failed exact verification. ") +
            OutError;
        return false;
    }
    if (!IFileManager::Get().Move(
            *DestinationFilename,
            *SiblingTemporary,
            true,
            true,
            false,
            false))
    {
        OutError = TEXT("Provider-throttle restore could not replace the destination from the verified sibling temporary; the backup remains preserved.");
        return false;
    }

    FString RestoredSha256;
    int64 RestoredBytes = INDEX_NONE;
    FString RestoredHashError;
    FString PreservedBackupSha256;
    int64 PreservedBackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bDestinationRestored = HashFileSha256(
        DestinationFilename,
        RestoredSha256,
        RestoredBytes,
        RestoredHashError) &&
        RestoredBytes == PreProviderThrottleSuccessorBytes &&
        RestoredSha256 == PreProviderThrottleSuccessorSha256;
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        PreservedBackupSha256,
        PreservedBackupBytes,
        BackupHashError) &&
        PreservedBackupBytes == PreProviderThrottleSuccessorBytes &&
        PreservedBackupSha256 == PreProviderThrottleSuccessorSha256;
    if (!bDestinationRestored || !bBackupPreserved)
    {
        OutError = FString::Printf(
            TEXT("Provider-throttle sibling-temp restore post-move verification failed: destinationRestored=%s restoredBytes=%lld restoredSha256=%s backupPreserved=%s restoredHash={%s} backupHash={%s}."),
            bDestinationRestored ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *RestoredHashError,
            *BackupHashError);
        return false;
    }
    OutError.Reset();
    return true;
}

FString GetPreLocalFallbackSuppressionV2BackupFilename()
{
    FString Filename = FPaths::Combine(
        FPaths::ConvertRelativePathToFull(FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/V5DLocalFallbackSuppressionV2_20260906"))),
        TEXT("Istana_PublicView_Explore_v5d_hybrid_859734CB9EFC.umap"));
    FPaths::NormalizeFilename(Filename);
    return Filename;
}

bool CreateVerifiedPreLocalFallbackSuppressionV2MapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    OutBackupFilename = GetPreLocalFallbackSuppressionV2BackupFilename();
    if (!IFileManager::Get().MakeDirectory(
            *FPaths::GetPath(OutBackupFilename), true))
    {
        OutError = TEXT("Could not create the exact suppression-V2 migration backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting suppression-V2 predecessor backup.");
        return false;
    }
    FString Sha256;
    int64 Bytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename, Sha256, Bytes, OutError) ||
        Bytes != PreLocalFallbackSuppressionV2Bytes ||
        Sha256 != PreLocalFallbackSuppressionV2Sha256)
    {
        OutError = FString::Printf(
            TEXT("Suppression-V2 predecessor backup failed exact verification: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PreLocalFallbackSuppressionV2Bytes,
            Bytes,
            *PreLocalFallbackSuppressionV2Sha256,
            *Sha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreVerifiedPreLocalFallbackSuppressionV2MapViaSiblingTemp(
    const FString& BackupFilename,
    const FString& DestinationFilename,
    FString& OutError)
{
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreLocalFallbackSuppressionV2Bytes ||
        BackupSha256 != PreLocalFallbackSuppressionV2Sha256)
    {
        OutError = TEXT("Suppression-V2 restore refused an unverified predecessor backup. ") +
            OutError;
        return false;
    }

    const FString DestinationDirectory = FPaths::GetPath(DestinationFilename);
    FString SiblingTemporary = FPaths::CreateTempFilename(
        *DestinationDirectory,
        TEXT("TRIAD_LocalFallbackSuppressionV2_Restore_"),
        TEXT(".tmp"));
    FPaths::NormalizeFilename(SiblingTemporary);
    if (!FPaths::IsSamePath(
            FPaths::GetPath(SiblingTemporary), DestinationDirectory) ||
        IFileManager::Get().FileExists(*SiblingTemporary))
    {
        OutError = TEXT("Suppression-V2 restore could not reserve a sibling temporary path.");
        return false;
    }
    ON_SCOPE_EXIT
    {
        if (IFileManager::Get().FileExists(*SiblingTemporary))
        {
            IFileManager::Get().Delete(
                *SiblingTemporary, false, true, true);
        }
    };
    if (IFileManager::Get().Copy(
            *SiblingTemporary,
            *BackupFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Suppression-V2 restore could not copy the backup to its sibling temporary.");
        return false;
    }
    FString TemporarySha256;
    int64 TemporaryBytes = INDEX_NONE;
    if (!HashFileSha256(
            SiblingTemporary,
            TemporarySha256,
            TemporaryBytes,
            OutError) ||
        TemporaryBytes != PreLocalFallbackSuppressionV2Bytes ||
        TemporarySha256 != PreLocalFallbackSuppressionV2Sha256)
    {
        OutError = TEXT("Suppression-V2 restore sibling temporary failed exact verification. ") +
            OutError;
        return false;
    }
    if (!IFileManager::Get().Move(
            *DestinationFilename,
            *SiblingTemporary,
            true,
            true,
            false,
            false))
    {
        OutError = TEXT("Suppression-V2 restore could not atomically replace the destination; the backup remains preserved.");
        return false;
    }

    FString RestoredSha256;
    int64 RestoredBytes = INDEX_NONE;
    FString RestoredHashError;
    FString PreservedBackupSha256;
    int64 PreservedBackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bDestinationRestored = HashFileSha256(
        DestinationFilename,
        RestoredSha256,
        RestoredBytes,
        RestoredHashError) &&
        RestoredBytes == PreLocalFallbackSuppressionV2Bytes &&
        RestoredSha256 == PreLocalFallbackSuppressionV2Sha256;
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        PreservedBackupSha256,
        PreservedBackupBytes,
        BackupHashError) &&
        PreservedBackupBytes == PreLocalFallbackSuppressionV2Bytes &&
        PreservedBackupSha256 == PreLocalFallbackSuppressionV2Sha256;
    if (!bDestinationRestored || !bBackupPreserved)
    {
        OutError = FString::Printf(
            TEXT("Suppression-V2 restore post-move verification failed: destinationRestored=%s restoredBytes=%lld restoredSha256=%s backupPreserved=%s restoredHash={%s} backupHash={%s}."),
            bDestinationRestored ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *RestoredHashError,
            *BackupHashError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateVerifiedPrePublicRealmMapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/V5DPublicRealmMigration_20260831")));
    OutBackupFilename = FPaths::Combine(
        BackupDirectory,
        TEXT("Istana_PublicView_Explore_v5d_hybrid_56A2A3B3.umap"));
    FPaths::NormalizeFilename(OutBackupFilename);
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutError = TEXT("Could not create the exact V5D public-realm migration backup directory.");
        return false;
    }

    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting V5D public-realm map backup.");
        return false;
    }

    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PrePublicRealmDestinationBytes ||
        BackupSha256 != PrePublicRealmDestinationSha256)
    {
        OutError = FString::Printf(
            TEXT("The V5D public-realm migration backup failed exact verification: bytes=%lld sha256=%s. %s"),
            BackupBytes,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateVerifiedPrePublicRealmMaterialCorrectionMapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/"
                 "V5DPublicRealmMaterialCorrection_20260831")));
    OutBackupFilename = FPaths::Combine(
        BackupDirectory,
        TEXT("Istana_PublicView_Explore_v5d_hybrid_CDBEDE32.umap"));
    FPaths::NormalizeFilename(OutBackupFilename);
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutError = TEXT("Could not create the exact V5D public-realm material-correction backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting V5D public-realm material-correction map backup.");
        return false;
    }
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PrePublicRealmMaterialCorrectionBytes ||
        BackupSha256 != PrePublicRealmMaterialCorrectionSha256)
    {
        OutError = FString::Printf(
            TEXT("The V5D public-realm material-correction backup failed exact verification: bytes=%lld sha256=%s. %s"),
            BackupBytes,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateVerifiedPreInheritedPlanningGroundSuppressionMapBackup(
    const FString& SourceFilename,
    FString& OutBackupFilename,
    FString& OutError)
{
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/"
                 "V5DInheritedPlanningGroundSuppression_20260831")));
    OutBackupFilename = FPaths::Combine(
        BackupDirectory,
        TEXT("Istana_PublicView_Explore_v5d_hybrid_154732F3.umap"));
    FPaths::NormalizeFilename(OutBackupFilename);
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutError = TEXT("Could not create the exact V5D inherited-planning-ground suppression backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting V5D inherited planning-ground suppression map backup.");
        return false;
    }
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != PreInheritedPlanningGroundSuppressionBytes ||
        BackupSha256 != PreInheritedPlanningGroundSuppressionSha256)
    {
        OutError = FString::Printf(
            TEXT("The V5D inherited-planning-ground suppression backup failed exact verification: bytes=%lld sha256=%s. %s"),
            BackupBytes,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateVerifiedPreMacDonaldHouseR24MapBackup(
    const FString& SourceFilename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutBackupFilename,
    FString& OutError)
{
    if (ExpectedBytes <= 0 || ExpectedSha256.Len() != 64)
    {
        OutError = TEXT("R24A MacDonald House backup requires a valid admitted predecessor byte/hash receipt.");
        return false;
    }
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/V5DMacDonaldHouseR24_20260903")));
    OutBackupFilename = FPaths::Combine(
        BackupDirectory,
        FString::Printf(
            TEXT("Istana_PublicView_Explore_v5d_hybrid_%s.umap"),
            *ExpectedSha256.Left(12)));
    FPaths::NormalizeFilename(OutBackupFilename);
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutError = TEXT("Could not create the exact R24A MacDonald House migration backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting R24A MacDonald House predecessor backup.");
        return false;
    }
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != ExpectedBytes || BackupSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("The R24A MacDonald House predecessor backup failed exact verification: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            ExpectedBytes,
            BackupBytes,
            *ExpectedSha256,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreVerifiedPreMacDonaldHouseR24MapViaSiblingTemp(
    const FString& BackupFilename,
    const FString& DestinationFilename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != ExpectedBytes || BackupSha256 != ExpectedSha256)
    {
        OutError = TEXT("R24A sibling-temp restore refused an unverified predecessor backup. ") +
            OutError;
        return false;
    }

    const FString DestinationDirectory = FPaths::GetPath(DestinationFilename);
    FString SiblingTemporary = FPaths::CreateTempFilename(
        *DestinationDirectory,
        TEXT("TRIAD_R24_MacDonald_Restore_"),
        TEXT(".tmp"));
    FPaths::NormalizeFilename(SiblingTemporary);
    if (!FPaths::IsSamePath(
            FPaths::GetPath(SiblingTemporary),
            DestinationDirectory) ||
        IFileManager::Get().FileExists(*SiblingTemporary))
    {
        OutError = TEXT("R24A sibling-temp restore could not reserve a verified temporary path.");
        return false;
    }
    ON_SCOPE_EXIT
    {
        if (IFileManager::Get().FileExists(*SiblingTemporary))
        {
            IFileManager::Get().Delete(
                *SiblingTemporary,
                false,
                true,
                true);
        }
    };

    if (IFileManager::Get().Copy(
            *SiblingTemporary,
            *BackupFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("R24A sibling-temp restore could not copy the backup to its verified temporary.");
        return false;
    }
    FString TemporarySha256;
    int64 TemporaryBytes = INDEX_NONE;
    if (!HashFileSha256(
            SiblingTemporary,
            TemporarySha256,
            TemporaryBytes,
            OutError) ||
        TemporaryBytes != ExpectedBytes ||
        TemporarySha256 != ExpectedSha256)
    {
        OutError = TEXT("R24A sibling-temp restore temporary failed exact verification. ") +
            OutError;
        return false;
    }
    if (!IFileManager::Get().Move(
            *DestinationFilename,
            *SiblingTemporary,
            true,
            true,
            false,
            false))
    {
        OutError = TEXT("R24A sibling-temp restore could not replace the destination from the verified temporary; the backup remains preserved.");
        return false;
    }

    FString FinalSha256;
    int64 FinalBytes = INDEX_NONE;
    FString FinalHashError;
    FString PreservedBackupSha256;
    int64 PreservedBackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bFinalExact = HashFileSha256(
        DestinationFilename,
        FinalSha256,
        FinalBytes,
        FinalHashError) &&
        FinalBytes == ExpectedBytes && FinalSha256 == ExpectedSha256;
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        PreservedBackupSha256,
        PreservedBackupBytes,
        BackupHashError) &&
        PreservedBackupBytes == ExpectedBytes &&
        PreservedBackupSha256 == ExpectedSha256;
    if (!bFinalExact || !bBackupPreserved)
    {
        OutError = FString::Printf(
            TEXT("R24A sibling-temp restore post-move verification failed: finalExact=%s finalBytes=%lld finalSha256=%s backupPreserved=%s finalHash={%s} backupHash={%s}."),
            bFinalExact ? TEXT("true") : TEXT("false"),
            FinalBytes,
            *FinalSha256,
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *FinalHashError,
            *BackupHashError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateVerifiedPreTemasekShophouseR24MapBackup(
    const FString& SourceFilename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutBackupFilename,
    FString& OutError)
{
    if (ExpectedBytes <= 0 || ExpectedSha256.Len() != 64)
    {
        OutError = TEXT("R24B Temasek Shophouse backup requires a valid admitted predecessor byte/hash receipt.");
        return false;
    }
    const FString BackupDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/MapBackups/V5DTemasekShophouseR24_20260903")));
    OutBackupFilename = FPaths::Combine(
        BackupDirectory,
        FString::Printf(
            TEXT("Istana_PublicView_Explore_v5d_hybrid_%s.umap"),
            *ExpectedSha256.Left(12)));
    FPaths::NormalizeFilename(OutBackupFilename);
    if (!IFileManager::Get().MakeDirectory(*BackupDirectory, true))
    {
        OutError = TEXT("Could not create the exact R24B Temasek Shophouse migration backup directory.");
        return false;
    }
    if (!IFileManager::Get().FileExists(*OutBackupFilename) &&
        IFileManager::Get().Copy(
            *OutBackupFilename,
            *SourceFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("Could not create the non-overwriting R24B Temasek Shophouse predecessor backup.");
        return false;
    }
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            OutBackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != ExpectedBytes || BackupSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("The R24B Temasek Shophouse predecessor backup failed exact verification: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            ExpectedBytes,
            BackupBytes,
            *ExpectedSha256,
            *BackupSha256,
            *OutError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreVerifiedPreTemasekShophouseR24MapViaSiblingTemp(
    const FString& BackupFilename,
    const FString& DestinationFilename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    if (!HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            OutError) ||
        BackupBytes != ExpectedBytes || BackupSha256 != ExpectedSha256)
    {
        OutError = TEXT("R24B sibling-temp restore refused an unverified predecessor backup. ") +
            OutError;
        return false;
    }

    const FString DestinationDirectory = FPaths::GetPath(DestinationFilename);
    FString SiblingTemporary = FPaths::CreateTempFilename(
        *DestinationDirectory,
        TEXT("TRIAD_R24_Temasek_Restore_"),
        TEXT(".tmp"));
    FPaths::NormalizeFilename(SiblingTemporary);
    if (!FPaths::IsSamePath(
            FPaths::GetPath(SiblingTemporary),
            DestinationDirectory) ||
        IFileManager::Get().FileExists(*SiblingTemporary))
    {
        OutError = TEXT("R24B sibling-temp restore could not reserve a verified temporary path.");
        return false;
    }
    ON_SCOPE_EXIT
    {
        if (IFileManager::Get().FileExists(*SiblingTemporary))
        {
            IFileManager::Get().Delete(
                *SiblingTemporary,
                false,
                true,
                true);
        }
    };

    if (IFileManager::Get().Copy(
            *SiblingTemporary,
            *BackupFilename,
            false,
            true) != COPY_OK)
    {
        OutError = TEXT("R24B sibling-temp restore could not copy the backup to its verified temporary.");
        return false;
    }
    FString TemporarySha256;
    int64 TemporaryBytes = INDEX_NONE;
    if (!HashFileSha256(
            SiblingTemporary,
            TemporarySha256,
            TemporaryBytes,
            OutError) ||
        TemporaryBytes != ExpectedBytes ||
        TemporarySha256 != ExpectedSha256)
    {
        OutError = TEXT("R24B sibling-temp restore temporary failed exact verification. ") +
            OutError;
        return false;
    }
    if (!IFileManager::Get().Move(
            *DestinationFilename,
            *SiblingTemporary,
            true,
            true,
            false,
            false))
    {
        OutError = TEXT("R24B sibling-temp restore could not replace the destination from the verified temporary; the backup remains preserved.");
        return false;
    }

    FString FinalSha256;
    int64 FinalBytes = INDEX_NONE;
    FString FinalHashError;
    FString PreservedBackupSha256;
    int64 PreservedBackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bFinalExact = HashFileSha256(
        DestinationFilename,
        FinalSha256,
        FinalBytes,
        FinalHashError) &&
        FinalBytes == ExpectedBytes && FinalSha256 == ExpectedSha256;
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        PreservedBackupSha256,
        PreservedBackupBytes,
        BackupHashError) &&
        PreservedBackupBytes == ExpectedBytes &&
        PreservedBackupSha256 == ExpectedSha256;
    if (!bFinalExact || !bBackupPreserved)
    {
        OutError = FString::Printf(
            TEXT("R24B sibling-temp restore post-move verification failed: finalExact=%s finalBytes=%lld finalSha256=%s backupPreserved=%s finalHash={%s} backupHash={%s}."),
            bFinalExact ? TEXT("true") : TEXT("false"),
            FinalBytes,
            *FinalSha256,
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *FinalHashError,
            *BackupHashError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool CanRunRequiredNaniteMaterialOperation(
    const TCHAR* Operation,
    FString& OutReport)
{
    if (!GIsEditor || !GEditor)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_%s_REFUSED_NOT_EDITOR"),
            Operation);
        return false;
    }
    if (GEditor->PlayWorld || GEditor->IsPlaySessionInProgress())
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_%s_REFUSED_PIE"),
            Operation);
        return false;
    }
    return true;
}

bool LoadRequiredNaniteMaterialsClean(
    TArray<UMaterial*>& OutMaterials,
    FString& OutReport)
{
    OutMaterials.Reset();
    OutMaterials.Reserve(RequiredNaniteMaterialCount);
    TSet<FString> SeenPackagePaths;

    for (const TCHAR* RequiredPackagePath :
         RequiredNaniteMaterialPackagePaths)
    {
        const FString PackagePath(RequiredPackagePath);
        if (!FPackageName::IsValidLongPackageName(PackagePath) ||
            SeenPackagePaths.Contains(PackagePath))
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_INVALID_SPEC package=%s loaded=%d expected=%d"),
                *PackagePath,
                OutMaterials.Num(),
                RequiredNaniteMaterialCount);
            return false;
        }
        SeenPackagePaths.Add(PackagePath);

        const FString ObjectPath = FString::Printf(
            TEXT("%s.%s"),
            *PackagePath,
            *FPackageName::GetShortName(PackagePath));
        UObject* Object = StaticLoadObject(
            UObject::StaticClass(),
            nullptr,
            *ObjectPath,
            nullptr,
            LOAD_NoRedirects);
        if (!Object)
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_INVALID_LOAD package=%s object=%s reason=missing_or_redirected loaded=%d expected=%d"),
                *PackagePath,
                *ObjectPath,
                OutMaterials.Num(),
                RequiredNaniteMaterialCount);
            return false;
        }

        UMaterial* Material = Cast<UMaterial>(Object);
        if (!Material)
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_INVALID_TYPE package=%s object=%s class=%s expected=UMaterial"),
                *PackagePath,
                *ObjectPath,
                *Object->GetClass()->GetPathName());
            return false;
        }
        UPackage* Package = Material->GetOutermost();
        if (Material->GetPathName() != ObjectPath || !Package ||
            Package->GetName() != PackagePath)
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_INVALID_IDENTITY expected=%s actual=%s package=%s"),
                *ObjectPath,
                *Material->GetPathName(),
                Package ? *Package->GetName() : TEXT("null"));
            return false;
        }
        if (Package->IsDirty())
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_REFUSED_DIRTY_PACKAGE package=%s"),
                *PackagePath);
            return false;
        }
        OutMaterials.Add(Material);
    }

    if (OutMaterials.Num() != RequiredNaniteMaterialCount ||
        SeenPackagePaths.Num() != RequiredNaniteMaterialCount)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_INVALID_ROSTER loaded=%d uniquePackages=%d expected=%d"),
            OutMaterials.Num(),
            SeenPackagePaths.Num(),
            RequiredNaniteMaterialCount);
        return false;
    }
    return true;
}

enum class ELandmarkPresencePolicy : uint8
{
    Forbidden,
    Optional,
    Required
};

bool MatchesLandmarkPresence(
    int32 Count,
    ELandmarkPresencePolicy Policy)
{
    switch (Policy)
    {
    case ELandmarkPresencePolicy::Forbidden:
        return Count == 0;
    case ELandmarkPresencePolicy::Optional:
        return Count <= 1;
    case ELandmarkPresencePolicy::Required:
        return Count == 1;
    default:
        return false;
    }
}

const TCHAR* LandmarkPresencePolicyName(ELandmarkPresencePolicy Policy)
{
    switch (Policy)
    {
    case ELandmarkPresencePolicy::Forbidden:
        return TEXT("Forbidden");
    case ELandmarkPresencePolicy::Optional:
        return TEXT("Optional");
    case ELandmarkPresencePolicy::Required:
        return TEXT("Required");
    default:
        return TEXT("Invalid");
    }
}

bool ValidateLegacyCurrentSurroundingsMigrationPredecessor(
    const ATRIADIstanaExploreV5DContextPolicyActor* Policy,
    const ATRIADIstanaPublicViewSceneActor* Scene,
    FString& OutReport)
{
    FString AssetReport;
    const UStaticMeshComponent* Component = Policy
        ? Policy->CurrentSurroundingsRenderOnlyComponent
        : nullptr;
    const UStaticMesh* Mesh = Component
        ? Component->GetStaticMesh()
        : nullptr;
    const UStaticMeshComponent* OuterGroundComponent = Policy
        ? Policy->OuterGroundLoadingFallbackRenderOnlyComponent
        : nullptr;
    const auto IsHiddenRenderer = [](const UStaticMeshComponent* Candidate)
    {
        return Candidate && !Candidate->IsVisible() &&
            Candidate->bHiddenInGame;
    };
#if WITH_EDITOR
    const bool bPolicyHiddenInEditor = Policy && Policy->IsHiddenEd();
#else
    const bool bPolicyHiddenInEditor = false;
#endif
    if (!Policy || !Scene ||
        !TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::ValidateAsset(
            AssetReport) ||
        !Component || !Mesh ||
        Mesh->GetPathName() !=
            TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
                GetSurroundingsMeshObjectPath() ||
        !Policy->bCurrentSurroundingsRenderOnly ||
        Policy->bCurrentSurroundingsCollisionNavigationSensorOrRfAuthority ||
        Policy->bCurrentSurroundingsMeasuredSurveyAsBuiltOrHyperreal ||
        Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !Policy->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        Policy->IsHidden() || bPolicyHiddenInEditor ||
        !Policy->GetRootComponent() ||
        Policy->GetRootComponent()->Mobility != EComponentMobility::Static ||
        Component->GetAttachParent() != Policy->GetRootComponent() ||
        !Component->GetRelativeTransform().Equals(
            FTransform::Identity,
            0.001) ||
        !Component->GetComponentTransform().Equals(
            FTransform::Identity,
            0.001) ||
        Component->Mobility != EComponentMobility::Static ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation() ||
        !Component->CastShadow || !Component->bRenderInMainPass ||
        Component->GetNumOverrideMaterials() != 0 ||
        Component->GetOverlayMaterial() != nullptr ||
        !Component->ComponentTags.Contains(
            TEXT("TRIADV5DCurrentPublicContextRenderOnly")) ||
        Component->ComponentTags.Contains(TEXT("TRIADHumanOnlyOverlay")) ||
        !Component->IsVisible() || Component->bHiddenInGame ||
        !OuterGroundComponent || OuterGroundComponent->GetStaticMesh() ||
        OuterGroundComponent->GetAttachParent() !=
            Policy->GetRootComponent() ||
        !OuterGroundComponent->GetRelativeTransform().Equals(
            FTransform::Identity,
            0.001) ||
        OuterGroundComponent->Mobility != EComponentMobility::Static ||
        OuterGroundComponent->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        OuterGroundComponent->GetGenerateOverlapEvents() ||
        OuterGroundComponent->CanEverAffectNavigation() ||
        OuterGroundComponent->CastShadow ||
        OuterGroundComponent->bCastContactShadow ||
        OuterGroundComponent->bAffectDistanceFieldLighting ||
        OuterGroundComponent->bAffectDynamicIndirectLighting ||
        !OuterGroundComponent->bRenderInMainPass ||
        OuterGroundComponent->bRenderCustomDepth ||
        OuterGroundComponent->GetNumOverrideMaterials() != 0 ||
        OuterGroundComponent->GetOverlayMaterial() != nullptr ||
        !OuterGroundComponent->ComponentTags.Contains(
            TEXT("TRIADV5DOuterGroundLoadingFallbackRenderOnly")) ||
        !OuterGroundComponent->IsVisible() ||
        OuterGroundComponent->bHiddenInGame ||
        !Policy->bOuterGroundLoadingFallbackRenderOnly ||
        Policy->bOuterGroundLoadingFallbackCollisionNavigationSensorRfTerrainAuthority ||
        Policy->bOuterGroundLoadingFallbackSurveyAsBuilt ||
        !IsHiddenRenderer(Scene->ContextBuildingsComponent) ||
        !IsHiddenRenderer(Scene->OSMContextBuildingsComponent) ||
        !IsHiddenRenderer(Scene->V5CSurroundingsRenderOnlyComponent) ||
        !IsHiddenRenderer(Scene->V5CGroundContextRenderOnlyComponent))
    {
        OutReport =
            TEXT("LEGACY_CURRENT_SURROUNDINGS_MIGRATION_PREDECESSOR_INVALID: exact 43,544-triangle asset, component state, or negative-authority lineage changed. ") +
            AssetReport;
        return false;
    }
    OutReport =
        TEXT("LEGACY_CURRENT_SURROUNDINGS_MIGRATION_PREDECESSOR_VALID exactCanonicalAsset=true triangles=43544 componentVisible=true inheritedFallbacksHidden=true renderOnly=true collisionNavigationSensorRfAuthority=false surveyAsBuiltHyperreal=false.");
    return true;
}

bool ValidateProviderQualityMigrationPredecessorWorld(
    UWorld* World,
    ACesium3DTileset*& OutTileset,
    FString& OutReport)
{
    OutTileset = nullptr;
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage || MapPackage->IsDirty())
    {
        OutReport = TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: requires the clean exact V5D destination package.");
        return false;
    }

    FString DestinationFilename;
    FString DiskSha256;
    int64 DiskBytes = INDEX_NONE;
    FString DiskHashError;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename) ||
        !HashFileSha256(
            DestinationFilename,
            DiskSha256,
            DiskBytes,
            DiskHashError) ||
        DiskBytes != PreProviderQualityPassBytes ||
        DiskSha256 != PreProviderQualityPassSha256)
    {
        OutReport = FString::Printf(
            TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: exact disk receipt changed; expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s hash={%s}."),
            PreProviderQualityPassBytes,
            DiskBytes,
            *PreProviderQualityPassSha256,
            *DiskSha256,
            *DiskHashError);
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 TilesetCount = 0;
    int32 GeoreferenceCount = 0;
    int32 SiteClipCount = 0;
    int32 MacDonaldHouseCount = 0;
    int32 TemasekShophouseCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    ACesium3DTileset* Tileset =
        FindExactlyOne<ACesium3DTileset>(World, TilesetCount);
    ACesiumGeoreference* Georeference =
        FindExactlyOne<ACesiumGeoreference>(World, GeoreferenceCount);
    ACesiumCartographicPolygon* SiteClip =
        FindExactlyOne<ACesiumCartographicPolygon>(World, SiteClipCount);
    ATRIADIstanaExploreV5DMacDonaldHouseActor* MacDonaldHouse =
        FindExactlyOne<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
            World,
            MacDonaldHouseCount);
    ATRIADIstanaExploreV5DTemasekShophouseActor* TemasekShophouse =
        FindExactlyOne<ATRIADIstanaExploreV5DTemasekShophouseActor>(
            World,
            TemasekShophouseCount);
    TArray<UCesiumPolygonRasterOverlay*> SiteClipOverlays;
    if (Tileset)
    {
        Tileset->GetComponents<UCesiumPolygonRasterOverlay>(SiteClipOverlays);
    }
    if (SceneCount != 1 || PolicyCount != 1 || TilesetCount != 1 ||
        GeoreferenceCount != 1 || SiteClipCount != 1 ||
        MacDonaldHouseCount != 1 || TemasekShophouseCount != 1 ||
        SiteClipOverlays.Num() != 1 || !Scene || !Policy || !Tileset ||
        !Georeference || !SiteClip || !MacDonaldHouse ||
        !TemasekShophouse || !SiteClipOverlays[0] ||
        !Policy->Tags.Contains(HybridContextPolicyTag) ||
        !Tileset->Tags.Contains(HybridVisualTilesetTag) ||
        !Georeference->Tags.Contains(HybridGeoreferenceTag) ||
        !SiteClip->Tags.Contains(HybridSiteClipTag) ||
        !MacDonaldHouse->Tags.Contains(
            ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedActorTag()) ||
        !TemasekShophouse->Tags.Contains(
            ATRIADIstanaExploreV5DTemasekShophouseActor::ExpectedActorTag()))
    {
        OutReport = FString::Printf(
            TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: sole tagged roster changed; scene=%d policy=%d tileset=%d georef=%d siteClip=%d clipOverlay=%d macDonald=%d temasek=%d."),
            SceneCount,
            PolicyCount,
            TilesetCount,
            GeoreferenceCount,
            SiteClipCount,
            SiteClipOverlays.Num(),
            MacDonaldHouseCount,
            TemasekShophouseCount);
        return false;
    }

    if (Tileset->GetTilesetSource() != ETilesetSource::FromCesiumIon ||
        Tileset->GetIonAssetID() != GooglePhotorealistic3DTilesIonAssetId ||
        !FMath::IsNearlyEqual(
            Tileset->GetMaximumScreenSpaceError(),
            1.0,
            0.000001) ||
        Tileset->ApplyDpiScaling != EApplyDpiScaling::UseProjectDefault ||
        Tileset->ForbidHoles ||
        Tileset->MaximumCachedBytes != HybridCacheBytes ||
        Tileset->MaximumSimultaneousTileLoads !=
            PreProviderThrottleSimultaneousLoads ||
        Tileset->LoadingDescendantLimit != HybridLoadingDescendantLimit ||
        !Tileset->ShowCreditsOnScreen || !Tileset->PreloadAncestors ||
        !Tileset->PreloadSiblings || !Tileset->EnableFrustumCulling ||
        Tileset->EnableFogCulling ||
        !Tileset->EnforceCulledScreenSpaceError ||
        !FMath::IsNearlyEqual(
            Tileset->CulledScreenSpaceError,
            HybridCulledScreenSpaceError,
            0.000001) ||
        Tileset->GetUseLodTransitions() ||
        Tileset->GetCreatePhysicsMeshes() ||
        Tileset->GetCreateNavCollision() ||
        Tileset->GetGenerateSmoothNormals() ||
        Tileset->GetIgnoreKhrMaterialsUnlit())
    {
        OutReport = TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: exact historical Cesium tuple changed; expected ApplyDpiScaling=UseProjectDefault, ForbidHoles=false, MaximumSimultaneousTileLoads=64, and PreloadSiblings=true.");
        return false;
    }

    const FVector Origin = Georeference->GetOriginLongitudeLatitudeHeight();
    if (Georeference->GetOriginPlacement() !=
            EOriginPlacement::CartographicOrigin ||
        !FMath::IsNearlyEqual(Origin.X, IstanaLongitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(Origin.Y, IstanaLatitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(
            Origin.Z,
            IstanaFallbackEllipsoidHeightMeters,
            0.000001) ||
        !FMath::IsNearlyEqual(Georeference->GetScale(), 100.0, 0.000001) ||
        !Georeference->Tags.Contains(CesiumDefaultGeoreferenceTag) ||
        Tileset->GetGeoreference().Get() != Georeference ||
        !SiteClip->GlobeAnchor ||
        SiteClip->GlobeAnchor->GetGeoreference().Get() != Georeference ||
        SiteClip->GlobeAnchor->GetResolvedGeoreference() != Georeference ||
        !SiteClip->GetActorTransform().Equals(FTransform::Identity, 0.001))
    {
        OutReport = TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: exact geospatial anchor or explicit bindings changed.");
        return false;
    }

    UCesiumPolygonRasterOverlay* SiteClipOverlay = SiteClipOverlays[0];
    const int32 ExpectedSiteClipSplinePoints =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSplinePoints();
    if (!SiteClip->Polygon || !SiteClip->Polygon->IsClosedLoop() ||
        SiteClip->Polygon->GetNumberOfSplinePoints() !=
            ExpectedSiteClipSplinePoints ||
        SiteClipOverlay->MaterialLayerKey != TEXT("Clipping") ||
        SiteClipOverlay->InvertSelection ||
        !SiteClipOverlay->ExcludeSelectedTiles ||
        SiteClipOverlay->Polygons.Num() != 1 ||
        SiteClipOverlay->Polygons[0].Get() != SiteClip ||
        !Tileset->GetMaterial() || !Tileset->GetTranslucentMaterial() ||
        Tileset->GetMaterial()->GetPathName() !=
            CesiumOpaqueClippingMaterialPath ||
        Tileset->GetTranslucentMaterial()->GetPathName() !=
            CesiumTranslucentClippingMaterialPath)
    {
        OutReport = TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: exact sole provider site clip or clipping materials changed.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedSiteClipSplinePoints; ++Index)
    {
        const FVector Point = SiteClip->Polygon->GetLocationAtSplinePoint(
            Index,
            ESplineCoordinateSpace::Local);
        const FVector2D ExpectedPoint =
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedProviderSiteClipPointCentimeters(Index);
        if (!FMath::IsNearlyZero(Point.Z, 0.01) ||
            !FVector2D(Point.X, Point.Y).Equals(ExpectedPoint, 0.1) ||
            SiteClip->Polygon->GetSplinePointType(Index) !=
                ESplinePointType::Linear)
        {
            OutReport = FString::Printf(
                TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: clip point %d changed."),
                Index);
            return false;
        }
    }

    FString CurrentSurroundingsReport;
    FString MacDonaldHouseReport;
    FString TemasekShophouseReport;
    if (Policy->RequiredProviderReadyConsecutiveSamples != 3 ||
        !Policy->bCesiumLayerIsVisualOnly ||
        Policy->bCesiumCollisionNavigationSensorOrRfAuthority ||
        Policy->bTriadReadSerializedOrLoggedProviderToken ||
        Policy->bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent ||
        !ValidateLegacyCurrentSurroundingsMigrationPredecessor(
            Policy,
            Scene,
            CurrentSurroundingsReport) ||
        !MacDonaldHouse->ValidateMacDonaldHouse(MacDonaldHouseReport) ||
        !TemasekShophouse->ValidateTemasekShophouse(
            TemasekShophouseReport) ||
        MacDonaldHouse->bProviderReady !=
            Policy->bLocalBuildingFallbackCurrentlyHidden ||
        TemasekShophouse->bProviderReady !=
            Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !MacDonaldHouse->bDedicatedOverlayVisible ||
        !TemasekShophouse->bDedicatedOverlayVisible)
    {
        OutReport = TEXT("PROVIDER_QUALITY_PREDECESSOR_INVALID: render-only context, landmark, readiness, or negative-authority contract changed. ") +
            CurrentSurroundingsReport + TEXT(" ") + MacDonaldHouseReport +
            TEXT(" ") + TemasekShophouseReport;
        return false;
    }

    OutTileset = Tileset;
    OutReport = TEXT("PROVIDER_QUALITY_PREDECESSOR_VALID clean=true soleTaggedTileset=true soleTaggedGeoreference=true soleTaggedSiteClip=true exactTwoLandmarks=true legacyApplyDpiScaling=UseProjectDefault legacyForbidHoles=false loadingDescendantLimit=20 culledSse=8.0 geospatialAnchorUnchanged=true renderOnlyNegativeAuthority=true.");
    return true;
}

bool ValidateProviderThrottleMigrationPredecessorWorld(
    UWorld* World,
    ACesium3DTileset*& OutTileset,
    FString& OutReport)
{
    OutTileset = nullptr;
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage || MapPackage->IsDirty())
    {
        OutReport = TEXT("PROVIDER_THROTTLE_PREDECESSOR_INVALID: requires the clean exact V5D destination package.");
        return false;
    }

    FString DestinationFilename;
    FString DiskSha256;
    int64 DiskBytes = INDEX_NONE;
    FString DiskHashError;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename) ||
        !HashFileSha256(
            DestinationFilename,
            DiskSha256,
            DiskBytes,
            DiskHashError) ||
        DiskBytes != PreProviderThrottleSuccessorBytes ||
        DiskSha256 != PreProviderThrottleSuccessorSha256)
    {
        OutReport = FString::Printf(
            TEXT("PROVIDER_THROTTLE_PREDECESSOR_INVALID: exact disk receipt changed; expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s hash={%s}."),
            PreProviderThrottleSuccessorBytes,
            DiskBytes,
            *PreProviderThrottleSuccessorSha256,
            *DiskSha256,
            *DiskHashError);
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 TilesetCount = 0;
    int32 GeoreferenceCount = 0;
    int32 SiteClipCount = 0;
    int32 MacDonaldHouseCount = 0;
    int32 TemasekShophouseCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    ACesium3DTileset* Tileset =
        FindExactlyOne<ACesium3DTileset>(World, TilesetCount);
    ACesiumGeoreference* Georeference =
        FindExactlyOne<ACesiumGeoreference>(World, GeoreferenceCount);
    ACesiumCartographicPolygon* SiteClip =
        FindExactlyOne<ACesiumCartographicPolygon>(World, SiteClipCount);
    ATRIADIstanaExploreV5DMacDonaldHouseActor* MacDonaldHouse =
        FindExactlyOne<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
            World,
            MacDonaldHouseCount);
    ATRIADIstanaExploreV5DTemasekShophouseActor* TemasekShophouse =
        FindExactlyOne<ATRIADIstanaExploreV5DTemasekShophouseActor>(
            World,
            TemasekShophouseCount);
    TArray<UCesiumPolygonRasterOverlay*> SiteClipOverlays;
    if (Tileset)
    {
        Tileset->GetComponents<UCesiumPolygonRasterOverlay>(SiteClipOverlays);
    }
    if (SceneCount != 1 || PolicyCount != 1 || TilesetCount != 1 ||
        GeoreferenceCount != 1 || SiteClipCount != 1 ||
        MacDonaldHouseCount != 1 || TemasekShophouseCount != 1 ||
        SiteClipOverlays.Num() != 1 || !Scene || !Policy || !Tileset ||
        !Georeference || !SiteClip || !MacDonaldHouse ||
        !TemasekShophouse || !SiteClipOverlays[0] ||
        !Policy->Tags.Contains(HybridContextPolicyTag) ||
        !Tileset->Tags.Contains(HybridVisualTilesetTag) ||
        !Georeference->Tags.Contains(HybridGeoreferenceTag) ||
        !SiteClip->Tags.Contains(HybridSiteClipTag) ||
        !MacDonaldHouse->Tags.Contains(
            ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedActorTag()) ||
        !TemasekShophouse->Tags.Contains(
            ATRIADIstanaExploreV5DTemasekShophouseActor::ExpectedActorTag()))
    {
        OutReport = FString::Printf(
            TEXT("PROVIDER_THROTTLE_PREDECESSOR_INVALID: sole tagged roster changed; scene=%d policy=%d tileset=%d georef=%d siteClip=%d clipOverlay=%d macDonald=%d temasek=%d."),
            SceneCount,
            PolicyCount,
            TilesetCount,
            GeoreferenceCount,
            SiteClipCount,
            SiteClipOverlays.Num(),
            MacDonaldHouseCount,
            TemasekShophouseCount);
        return false;
    }

    if (Tileset->GetTilesetSource() != ETilesetSource::FromCesiumIon ||
        Tileset->GetIonAssetID() != GooglePhotorealistic3DTilesIonAssetId ||
        !FMath::IsNearlyEqual(
            Tileset->GetMaximumScreenSpaceError(),
            1.0,
            0.000001) ||
        Tileset->ApplyDpiScaling != EApplyDpiScaling::No ||
        !Tileset->ForbidHoles ||
        Tileset->MaximumCachedBytes != HybridCacheBytes ||
        Tileset->MaximumSimultaneousTileLoads !=
            PreProviderThrottleSimultaneousLoads ||
        Tileset->LoadingDescendantLimit != HybridLoadingDescendantLimit ||
        !Tileset->ShowCreditsOnScreen || !Tileset->PreloadAncestors ||
        !Tileset->PreloadSiblings || !Tileset->EnableFrustumCulling ||
        Tileset->EnableFogCulling ||
        !Tileset->EnforceCulledScreenSpaceError ||
        !FMath::IsNearlyEqual(
            Tileset->CulledScreenSpaceError,
            HybridCulledScreenSpaceError,
            0.000001) ||
        Tileset->GetUseLodTransitions() ||
        Tileset->GetCreatePhysicsMeshes() ||
        Tileset->GetCreateNavCollision() ||
        Tileset->GetGenerateSmoothNormals() ||
        Tileset->GetIgnoreKhrMaterialsUnlit())
    {
        OutReport = TEXT("PROVIDER_THROTTLE_PREDECESSOR_INVALID: exact final-map Cesium tuple changed; expected MaximumSimultaneousTileLoads=64 and PreloadSiblings=true with every other provider-quality setting already final.");
        return false;
    }

    const FVector Origin = Georeference->GetOriginLongitudeLatitudeHeight();
    UCesiumPolygonRasterOverlay* SiteClipOverlay = SiteClipOverlays[0];
    if (Georeference->GetOriginPlacement() !=
            EOriginPlacement::CartographicOrigin ||
        !FMath::IsNearlyEqual(Origin.X, IstanaLongitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(Origin.Y, IstanaLatitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(
            Origin.Z,
            IstanaFallbackEllipsoidHeightMeters,
            0.000001) ||
        !FMath::IsNearlyEqual(Georeference->GetScale(), 100.0, 0.000001) ||
        !Georeference->Tags.Contains(CesiumDefaultGeoreferenceTag) ||
        Tileset->GetGeoreference().Get() != Georeference ||
        !SiteClip->GlobeAnchor ||
        SiteClip->GlobeAnchor->GetGeoreference().Get() != Georeference ||
        SiteClip->GlobeAnchor->GetResolvedGeoreference() != Georeference ||
        !SiteClip->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SiteClip->Polygon || !SiteClip->Polygon->IsClosedLoop() ||
        SiteClip->Polygon->GetNumberOfSplinePoints() !=
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedProviderSiteClipSplinePoints() ||
        SiteClipOverlay->MaterialLayerKey != TEXT("Clipping") ||
        SiteClipOverlay->InvertSelection ||
        !SiteClipOverlay->ExcludeSelectedTiles ||
        SiteClipOverlay->Polygons.Num() != 1 ||
        SiteClipOverlay->Polygons[0].Get() != SiteClip ||
        !Tileset->GetMaterial() || !Tileset->GetTranslucentMaterial() ||
        Tileset->GetMaterial()->GetPathName() !=
            CesiumOpaqueClippingMaterialPath ||
        Tileset->GetTranslucentMaterial()->GetPathName() !=
            CesiumTranslucentClippingMaterialPath)
    {
        OutReport = TEXT("PROVIDER_THROTTLE_PREDECESSOR_INVALID: exact geospatial anchor, provider clip, or clipping-compatible materials changed.");
        return false;
    }

    FString CurrentSurroundingsReport;
    FString MacDonaldHouseReport;
    FString TemasekShophouseReport;
    if (Policy->RequiredProviderReadyConsecutiveSamples != 3 ||
        !Policy->bCesiumLayerIsVisualOnly ||
        Policy->bCesiumCollisionNavigationSensorOrRfAuthority ||
        Policy->bTriadReadSerializedOrLoggedProviderToken ||
        Policy->bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent ||
        !Policy->bProviderContentClippedFromAuthoredCore ||
        !Policy->ValidateCurrentSurroundingsSuccessorForInheritedScene(
            Scene,
            CurrentSurroundingsReport) ||
        !MacDonaldHouse->ValidateMacDonaldHouse(MacDonaldHouseReport) ||
        !TemasekShophouse->ValidateTemasekShophouse(
            TemasekShophouseReport) ||
        MacDonaldHouse->bProviderReady !=
            Policy->bLocalBuildingFallbackCurrentlyHidden ||
        TemasekShophouse->bProviderReady !=
            Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !MacDonaldHouse->bDedicatedOverlayVisible ||
        !TemasekShophouse->bDedicatedOverlayVisible)
    {
        OutReport = TEXT("PROVIDER_THROTTLE_PREDECESSOR_INVALID: render-only context, landmark, readiness, or negative-authority contract changed. ") +
            CurrentSurroundingsReport + TEXT(" ") + MacDonaldHouseReport +
            TEXT(" ") + TemasekShophouseReport;
        return false;
    }

    OutTileset = Tileset;
    OutReport = TEXT("PROVIDER_THROTTLE_PREDECESSOR_VALID clean=true exactDiskReceipt=true soleTaggedTileset=true exactTwoLandmarks=true simultaneousLoads=64 preloadAncestors=true preloadSiblings=true allOtherProviderQualitySettingsFinal=true geospatialAnchorUnchanged=true renderOnlyNegativeAuthority=true.");
    return true;
}

bool ValidateHybridWorld(
    UWorld* World,
    FString& OutReport,
    ELandmarkPresencePolicy MacDonaldHousePolicy,
    ELandmarkPresencePolicy TemasekShophousePolicy,
    ELandmarkPresencePolicy LandmarkVegetationPolicy =
        ELandmarkPresencePolicy::Optional,
    bool bValidateLandmarkVegetationActor = true)
{
    const FString LogicalPackage = World && World->GetOutermost()
        ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())
        : FString();
    if (!World || !World->PersistentLevel ||
        LogicalPackage != DestinationMapPackage)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_MAP_INVALID: exact V5D hybrid map is not loaded.");
        return false;
    }

    int32 SceneCount = 0;
    int32 V4Count = 0;
    int32 V5Count = 0;
    int32 V5BCount = 0;
    int32 PolicyCount = 0;
    int32 FountainRealismCount = 0;
    int32 GroundVegetationCount = 0;
    int32 TreeRealismCount = 0;
    int32 DynamicRangeCount = 0;
    int32 PublicRealmCount = 0;
    int32 MacDonaldHouseCount = 0;
    int32 TemasekShophouseCount = 0;
    int32 LandmarkVegetationCount = 0;
    int32 TilesetCount = 0;
    int32 GeoreferenceCount = 0;
    int32 SiteClipCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    ATRIADIstanaExploreV5AppearanceActor* V5 =
        FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(World, V5Count);
    ATRIADIstanaExploreV5BVisualActor* V5B =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(World, V5BCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World, PolicyCount);
    ATRIADIstanaExploreV5DFountainRealismActor* FountainRealism =
        FindExactlyOne<ATRIADIstanaExploreV5DFountainRealismActor>(
            World,
            FountainRealismCount);
    ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            World,
            GroundVegetationCount);
    ATRIADIstanaExploreV5DTreeRealismActor* TreeRealism =
        FindExactlyOne<ATRIADIstanaExploreV5DTreeRealismActor>(
            World,
            TreeRealismCount);
    ATRIADIstanaExploreV5DDynamicRangeRealismActor* DynamicRange =
        FindExactlyOne<ATRIADIstanaExploreV5DDynamicRangeRealismActor>(
            World,
            DynamicRangeCount);
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
            World,
            PublicRealmCount);
    ATRIADIstanaExploreV5DMacDonaldHouseActor* MacDonaldHouse =
        FindExactlyOne<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
            World,
            MacDonaldHouseCount);
    ATRIADIstanaExploreV5DTemasekShophouseActor* TemasekShophouse =
        FindExactlyOne<ATRIADIstanaExploreV5DTemasekShophouseActor>(
            World,
            TemasekShophouseCount);
    ATRIADIstanaExploreV5DLandmarkVegetationActor* LandmarkVegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
            World,
            LandmarkVegetationCount);
    ACesium3DTileset* Tileset =
        FindExactlyOne<ACesium3DTileset>(World, TilesetCount);
    ACesiumGeoreference* Georeference =
        FindExactlyOne<ACesiumGeoreference>(World, GeoreferenceCount);
    ACesiumCartographicPolygon* SiteClip =
        FindExactlyOne<ACesiumCartographicPolygon>(World, SiteClipCount);
    TArray<UCesiumPolygonRasterOverlay*> SiteClipOverlays;
    if (Tileset)
    {
        Tileset->GetComponents<UCesiumPolygonRasterOverlay>(SiteClipOverlays);
    }
    if (SceneCount != 1 || V4Count != 1 || V5Count != 1 ||
        V5BCount != 1 ||
        PolicyCount != 1 ||
        FountainRealismCount != 1 ||
        GroundVegetationCount != 1 || TreeRealismCount != 1 ||
        DynamicRangeCount != 1 || PublicRealmCount != 1 ||
        !MatchesLandmarkPresence(MacDonaldHouseCount, MacDonaldHousePolicy) ||
        !MatchesLandmarkPresence(TemasekShophouseCount, TemasekShophousePolicy) ||
        !MatchesLandmarkPresence(
            LandmarkVegetationCount, LandmarkVegetationPolicy) ||
        TilesetCount != 1 || GeoreferenceCount != 1 || SiteClipCount != 1 ||
        SiteClipOverlays.Num() != 1 || !Scene || !V4 || !V5 || !V5B ||
        !Policy || !FountainRealism ||
        !GroundVegetation || !TreeRealism || !DynamicRange || !PublicRealm ||
        !Tileset ||
        !Georeference || !SiteClip || !SiteClipOverlays[0] ||
        !Policy->Tags.Contains(HybridContextPolicyTag) ||
        !FountainRealism->Tags.Contains(
            ATRIADIstanaExploreV5DFountainRealismActor::
                ExpectedActorTag()) ||
        !TreeRealism->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag()) ||
        !DynamicRange->Tags.Contains(
            ATRIADIstanaExploreV5DDynamicRangeRealismActor::ExpectedActorTag()) ||
        !PublicRealm->Tags.Contains(
            ATRIADIstanaExploreV5DPublicRealmActor::ExpectedActorTag()) ||
        (MacDonaldHouse &&
         !MacDonaldHouse->Tags.Contains(
             ATRIADIstanaExploreV5DMacDonaldHouseActor::ExpectedActorTag())) ||
        (TemasekShophouse &&
         !TemasekShophouse->Tags.Contains(
             ATRIADIstanaExploreV5DTemasekShophouseActor::ExpectedActorTag())) ||
        (LandmarkVegetation &&
         !LandmarkVegetation->Tags.Contains(
             ATRIADIstanaExploreV5DLandmarkVegetationActor::
                 ExpectedActorTag())) ||
        V5->PublicViewSceneActor != Scene ||
        V5->ExploreV4LandscapeActor != V4 ||
        FountainRealism->V5AppearanceActor != V5 ||
        FountainRealism->V5BVisualActor != V5B ||
        TreeRealism->V4LandscapeActor != V4 ||
        TreeRealism->V5BVisualActor != V5B ||
        !Tileset->Tags.Contains(HybridVisualTilesetTag) ||
        !Georeference->Tags.Contains(HybridGeoreferenceTag) ||
        !SiteClip->Tags.Contains(HybridSiteClipTag))
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_HYBRID_MAP_INVALID: actor roster, source bindings, or tags changed; scene=%d v4=%d v5=%d v5b=%d policy=%d fountainRealism=%d groundVegetation=%d treeRealism=%d dynamicRange=%d publicRealm=%d macDonaldHouse=%d macDonaldPolicy=%s temasekShophouse=%d temasekPolicy=%s landmarkVegetation=%d landmarkVegetationPolicy=%s tileset=%d georef=%d siteClip=%d clipOverlay=%d."),
            SceneCount,
            V4Count,
            V5Count,
            V5BCount,
            PolicyCount,
            FountainRealismCount,
            GroundVegetationCount,
            TreeRealismCount,
            DynamicRangeCount,
            PublicRealmCount,
            MacDonaldHouseCount,
            LandmarkPresencePolicyName(MacDonaldHousePolicy),
            TemasekShophouseCount,
            LandmarkPresencePolicyName(TemasekShophousePolicy),
            LandmarkVegetationCount,
            LandmarkPresencePolicyName(LandmarkVegetationPolicy),
            TilesetCount,
            GeoreferenceCount,
            SiteClipCount,
            SiteClipOverlays.Num());
        return false;
    }

    const FVector Origin = Georeference->GetOriginLongitudeLatitudeHeight();
    if (Georeference->GetOriginPlacement() !=
            EOriginPlacement::CartographicOrigin ||
        !FMath::IsNearlyEqual(Origin.X, IstanaLongitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(Origin.Y, IstanaLatitudeDegrees, 0.00000001) ||
        !FMath::IsNearlyEqual(
            Origin.Z,
            IstanaFallbackEllipsoidHeightMeters,
            0.000001) ||
        !FMath::IsNearlyEqual(Georeference->GetScale(), 100.0, 0.000001) ||
        !Georeference->Tags.Contains(CesiumDefaultGeoreferenceTag) ||
        Tileset->GetGeoreference().Get() != Georeference ||
        !SiteClip->GlobeAnchor ||
        SiteClip->GlobeAnchor->GetGeoreference().Get() != Georeference ||
        SiteClip->GlobeAnchor->GetResolvedGeoreference() != Georeference ||
        !SiteClip->GetActorTransform().Equals(FTransform::Identity, 0.001))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_MAP_INVALID: Istana georeference or explicit tileset binding changed.");
        return false;
    }

    UCesiumPolygonRasterOverlay* SiteClipOverlay = SiteClipOverlays[0];
    const int32 ExpectedSiteClipSplinePoints =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSplinePoints();
    if (!SiteClip->Polygon || !SiteClip->Polygon->IsClosedLoop() ||
        SiteClip->Polygon->GetNumberOfSplinePoints() !=
            ExpectedSiteClipSplinePoints ||
        SiteClipOverlay->MaterialLayerKey != TEXT("Clipping") ||
        SiteClipOverlay->InvertSelection ||
        !SiteClipOverlay->ExcludeSelectedTiles ||
        SiteClipOverlay->Polygons.Num() != 1 ||
        SiteClipOverlay->Polygons[0].Get() != SiteClip ||
        !Tileset->GetMaterial() || !Tileset->GetTranslucentMaterial() ||
        Tileset->GetMaterial()->GetPathName() !=
            CesiumOpaqueClippingMaterialPath ||
        Tileset->GetTranslucentMaterial()->GetPathName() !=
            CesiumTranslucentClippingMaterialPath)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_MAP_INVALID: authored-core clipping polygon, overlay, or clipping-compatible Cesium materials changed.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedSiteClipSplinePoints; ++Index)
    {
        const FVector Point = SiteClip->Polygon->GetLocationAtSplinePoint(
            Index,
            ESplineCoordinateSpace::Local);
        const FVector2D ExpectedPoint =
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedProviderSiteClipPointCentimeters(Index);
        if (!FMath::IsNearlyZero(Point.Z, 0.01) ||
            !FVector2D(Point.X, Point.Y).Equals(ExpectedPoint, 0.1) ||
            SiteClip->Polygon->GetSplinePointType(Index) !=
                ESplinePointType::Linear)
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_HYBRID_MAP_INVALID: provider clip point %d left the exact linear irregular-ellipse footprint."),
                Index);
            return false;
        }
    }

    FString PolicyReport;
    FString V5AppearanceReport;
    FString FountainRealismReport;
    FString GroundVegetationReport;
    FString TreeRealismReport;
    FString DynamicRangeReport;
    FString PublicRealmReport;
    FString MacDonaldHouseReport;
    FString TemasekShophouseReport;
    FString LandmarkVegetationReport;
    if (!Policy->ValidateHybridContext(PolicyReport) ||
        !V5->ValidateExploreV5Appearance(
            V5AppearanceReport,
            World->IsGameWorld()) ||
        !FountainRealism->ValidateFountainRealism(
            FountainRealismReport) ||
        !GroundVegetation->ValidateGroundVegetationRealism(
            GroundVegetationReport) ||
        !TreeRealism->ValidateTreeRealism(TreeRealismReport) ||
        !DynamicRange->ValidateDynamicRangeRealism(
            World->IsGameWorld(),
            DynamicRangeReport) ||
        !PublicRealm->ValidatePublicRealm(PublicRealmReport) ||
        (MacDonaldHouse &&
         (!MacDonaldHouse->ValidateMacDonaldHouse(MacDonaldHouseReport) ||
           MacDonaldHouse->bProviderReady !=
               Policy->bLocalBuildingFallbackCurrentlyHidden ||
           !MacDonaldHouse->bDedicatedOverlayVisible)) ||
        (TemasekShophouse &&
         (!TemasekShophouse->ValidateTemasekShophouse(
              TemasekShophouseReport) ||
          TemasekShophouse->bProviderReady !=
              Policy->bLocalBuildingFallbackCurrentlyHidden ||
          !TemasekShophouse->bDedicatedOverlayVisible)) ||
        (bValidateLandmarkVegetationActor && LandmarkVegetation &&
         !LandmarkVegetation->ValidateLandmarkVegetation(
             LandmarkVegetationReport)))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_MAP_INVALID: ") +
            PolicyReport + TEXT(" ") + V5AppearanceReport + TEXT(" ") +
            FountainRealismReport + TEXT(" ") + GroundVegetationReport +
            TEXT(" ") + TreeRealismReport + TEXT(" ") +
            DynamicRangeReport;
        OutReport += TEXT(" ") + PublicRealmReport;
        OutReport += TEXT(" ") + MacDonaldHouseReport;
        OutReport += TEXT(" ") + TemasekShophouseReport;
        OutReport += TEXT(" ") + LandmarkVegetationReport;
        return false;
    }

    if (LandmarkVegetation && !bValidateLandmarkVegetationActor)
    {
        LandmarkVegetationReport =
            TEXT("landmarkVegetationActorContractValidationDeferred=true");
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_HYBRID_MAP_VALID map=%s inheritedV5B=1 cesiumGeoreference=(%.8f,%.8f,%.3f) ionAssetId=%lld maximumSse=%.1f applyDpiScaling=false maximumCachedBytesSetting=%lld simultaneousLoads=%d forbidHoles=true loadingDescendantLimit=%d preloadAncestors=true preloadSiblings=false enforceCulledSse=true culledSse=%.1f useLodTransitions=false generateSmoothNormals=false preserveKhrMaterialsUnlit=true authoredCoreClipShape=irregularEllipse64 clipCenterMeters=(%.1f,%.1f) clipSemiAxesMeters=(%.1f,%.1f) clipSplinePoints=%d createPhysics=false createNav=false visualOnly=true deterministicLocalSimulationLayersPreserved=true cesiumStandardPersistentHttpRequestCacheAcknowledged=true triadProviderTokenReadSerializedOrLogged=false triadProviderContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false r24MacDonaldHouseCount=%d r24MacDonaldDedicatedOverlayVisible=%s r24TemasekShophouseCount=%d r24TemasekDedicatedOverlayVisible=%s landmarkVegetationCount=%d landmarkVegetationMapIntegrated=%s r24VisibilityInvariantAcrossProviderTransitions=true r24ProviderStateTelemetryOnly=true r24CoarseLocalLandmarkShellsSuppressed=true r24SuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490 r24ProviderAndDedicatedOverlayOverlapUnresolved=true r24DedicatedProviderExclusion=false r24ProviderReadyLiveSuccessor=false outerGroundLoadingFallbackProviderCoupled=true policy={%s} v5Appearance={%s} fountainRealism={%s} groundVegetation={%s} treeRealism={%s} dynamicRange={%s} publicRealm={%s} macDonaldHouse={%s} temasekShophouse={%s} landmarkVegetation={%s}"),
        *LogicalPackage,
        Origin.X,
        Origin.Y,
        Origin.Z,
        Tileset->GetIonAssetID(),
        Tileset->GetMaximumScreenSpaceError(),
        Tileset->MaximumCachedBytes,
        Tileset->MaximumSimultaneousTileLoads,
        Tileset->LoadingDescendantLimit,
        Tileset->CulledScreenSpaceError,
        Policy->ProviderSiteClipCenterMeters.X,
        Policy->ProviderSiteClipCenterMeters.Y,
        Policy->ProviderSiteClipSemiAxesMeters.X,
        Policy->ProviderSiteClipSemiAxesMeters.Y,
        ExpectedSiteClipSplinePoints,
        MacDonaldHouseCount,
        MacDonaldHouse && MacDonaldHouse->bDedicatedOverlayVisible
            ? TEXT("true")
            : TEXT("false"),
        TemasekShophouseCount,
        TemasekShophouse && TemasekShophouse->bDedicatedOverlayVisible
            ? TEXT("true")
            : TEXT("false"),
        LandmarkVegetationCount,
        LandmarkVegetation ? TEXT("true") : TEXT("false"),
        *PolicyReport,
        *V5AppearanceReport,
        *FountainRealismReport,
        *GroundVegetationReport,
        *TreeRealismReport,
        *DynamicRangeReport,
        *PublicRealmReport,
        *MacDonaldHouseReport,
        *TemasekShophouseReport,
        *LandmarkVegetationReport);
    return true;
}

bool ValidateLandmarkVegetationR27World(
    UWorld* World,
    FString& OutReport)
{
    FString GrassMaterialReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateLandmarkGrassMaterialsR27(GrassMaterialReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_INVALID: ") +
            GrassMaterialReport;
        return false;
    }

    FString TemasekAssetReport;
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::ValidateAssets(
            TemasekAssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_INVALID: exact zero-baked-foliage Temasek Phase-2 assets are required. ") +
            TemasekAssetReport;
        return false;
    }

    FString R25AssetReport;
    if (!TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
            R25AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_INVALID: retained R25 facade assets failed validation. ") +
            R25AssetReport;
        return false;
    }

    FString HybridReport;
    if (!ValidateHybridWorld(
            World,
            HybridReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_INVALID: ") +
            HybridReport;
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World, PolicyCount);
    FString R25Report;
    if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
        !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            Scene, R25Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_INVALID: exact R25 facade contract was not retained. ") +
            R25Report + TEXT(" ") + HybridReport;
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_VALID landmarkVegetationR27=true mapIntegrated=true exactlyOneActor=true contextFacadeR25=true legacyTemasekBakedFoliageRemoved=true temasekMeshTriangles=15760 temasekMaterialSlots=15 bakedFoliageRenderComponents=0 isolatedR27GrassMaterials=4 exactDerivativeGraph=true compiledMaterials=4 sourceStableVisibilityMeters=20,28 targetStableVisibilityMeters=65,90 visibilityGateCount=1 componentFadeIntegrated=true grassCullCm=6500,9000 evidenceRangeMeters=72.8 materialVisibilityAtEvidenceRange=%.3f tallestCarrierTipProjectionPixelsAtEvidenceRange=%.3f projectionAssumption=perpendicularPinholeMaxSourceTip wpoDisableCm=2400 grassInstances=3072 maximumGrassPerSite=2048 visualCaptureAccepted=false captureRevalidationRequired=true deterministic=true worldSpace=true nearestLandmarkPartition=true renderOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false sourceAssetsModified=false geometryExport=false providerSettingsUnchanged=true providerClipUnchanged=true. %s %s %s %s"),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedMaterialVisibilityAtEvidenceView(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView(),
        *GrassMaterialReport,
        *TemasekAssetReport,
        *R25Report,
        *HybridReport);
    return true;
}

bool ValidateR28VisualSuccessorWorld(
    UWorld* World,
    FString& OutReport)
{
    FString EnvironmentAssetReport;
    if (!UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary::
            ValidateR28EnvironmentAssets(EnvironmentAssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_INVALID: R28 environment assets failed validation. ") +
            EnvironmentAssetReport;
        return false;
    }

    FString LandmarkAssetReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateReusableLandmarkVegetationAssetsR28(
                LandmarkAssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_INVALID: R28 landmark assets failed validation. ") +
            LandmarkAssetReport;
        return false;
    }

    FString TemasekAssetReport;
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::ValidateAssets(
            TemasekAssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_INVALID: exact zero-baked-foliage Temasek Phase-2 assets are required. ") +
            TemasekAssetReport;
        return false;
    }

    FString R25AssetReport;
    if (!TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
            R25AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_INVALID: retained R25 facade assets failed validation. ") +
            R25AssetReport;
        return false;
    }

    // The generic world validator deliberately remains the historical R27
    // contract. Require the complete base roster while deferring only the
    // landmark actor's version-specific validation to the R28 call below.
    FString HybridReport;
    if (!ValidateHybridWorld(
            World,
            HybridReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required,
            false))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_INVALID: retained hybrid/provider contract failed with landmark validation deferred. ") +
            HybridReport;
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 EnvironmentCount = 0;
    int32 LandmarkVegetationCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    ATRIADIstanaExploreV5DR28EnvironmentActor* Environment =
        FindExactlyOne<ATRIADIstanaExploreV5DR28EnvironmentActor>(
            World,
            EnvironmentCount);
    ATRIADIstanaExploreV5DLandmarkVegetationActor* LandmarkVegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
            World,
            LandmarkVegetationCount);
    FString EnvironmentReport;
    FString LandmarkReport;
    FString R25Report;
    const bool bProviderNegativeAuthority =
        Policy && Policy->bCesiumLayerIsVisualOnly &&
        !Policy->bCesiumCollisionNavigationSensorOrRfAuthority &&
        !Policy->bTriadReadSerializedOrLoggedProviderToken &&
        !Policy->bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent;
    if (SceneCount != 1 || PolicyCount != 1 ||
        EnvironmentCount != 1 || LandmarkVegetationCount != 1 ||
        !Scene || !Policy || !Environment || !LandmarkVegetation ||
        Environment->GetClass() !=
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass() ||
        LandmarkVegetation->GetClass() !=
            ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass() ||
        !Environment->ValidateR28Environment(EnvironmentReport) ||
        !LandmarkVegetation->ValidateLandmarkVegetationR28(
            LandmarkReport) ||
        Environment->bProviderReady !=
            Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            Scene,
            R25Report) ||
        !bProviderNegativeAuthority)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_INVALID: scene=%d policy=%d environment=%d landmarkVegetation=%d environmentProviderReadyMatchesPolicy=%s providerNegativeAuthority=%s environment={%s} landmark={%s} r25={%s} hybrid={%s}"),
            SceneCount,
            PolicyCount,
            EnvironmentCount,
            LandmarkVegetationCount,
            Environment && Policy &&
                    Environment->bProviderReady ==
                        Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            bProviderNegativeAuthority ? TEXT("true") : TEXT("false"),
            *EnvironmentReport,
            *LandmarkReport,
            *R25Report,
            *HybridReport);
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_VALID combinedR28=true environmentActors=1 landmarkVegetationActors=1 environmentProviderReadyMatchesPolicy=true environmentAssets=13 landmarkAssets=14 landmarkGrassInstances=6144 landmarkGrassMaximumPerSite=4096 temasekTreeRoster=umbrella,dome,umbrella temasekPhase2Retained=true legacyTemasekBakedFoliageRemoved=true contextFacadeR25Retained=true providerSettingsUnchanged=true providerClipUnchanged=true providerVisualOnly=true providerCollisionNavigationSensorRfAuthority=false providerTokenReadSerializedOrLogged=false providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false renderOnly=true visualCaptureAccepted=false captureRevalidationRequired=true. environmentAssets={%s} landmarkAssets={%s} environment={%s} landmark={%s} temasekAssets={%s} r25Assets={%s} r25={%s} hybrid={%s}"),
        *EnvironmentAssetReport,
        *LandmarkAssetReport,
        *EnvironmentReport,
        *LandmarkReport,
        *TemasekAssetReport,
        *R25AssetReport,
        *R25Report,
        *HybridReport);
    return true;
}

bool HasStableHybridVisualPolicy(
    const ATRIADIstanaExploreV5DContextPolicyActor* Policy)
{
    return Policy &&
        Policy->bProviderSiteClipCurrentlyActive &&
        Policy->bAuthoredCoreVisualsCurrentlyVisible &&
        !Policy->bAerialProviderHandoffRequested &&
        !Policy->bAerialProviderHandoffCurrentlyActive &&
        Policy->AerialProviderReadySamples == 0;
}

bool GetValidatedHybridPlayState(
    UWorld*& OutWorld,
    APlayerController*& OutPlayer,
    ATRIADIstanaExploreV5Pawn*& OutPawn,
    ATRIADIstanaExploreV5DContextPolicyActor*& OutPolicy,
    ACesium3DTileset*& OutTileset,
    FString& OutError,
    bool bRequireR28VisualSuccessor = false,
    FString* OutWorldContractReport = nullptr)
{
    OutWorld = GEditor ? GEditor->PlayWorld : nullptr;
    OutPlayer = nullptr;
    OutPawn = nullptr;
    OutPolicy = nullptr;
    OutTileset = nullptr;
    FString WorldReport;
    const bool bWorldContractValid = OutWorld &&
        (bRequireR28VisualSuccessor
            ? ValidateR28VisualSuccessorWorld(OutWorld, WorldReport)
            : ValidateHybridWorld(
                  OutWorld,
                  WorldReport,
                  ELandmarkPresencePolicy::Required,
                  ELandmarkPresencePolicy::Required));
    if (OutWorldContractReport)
    {
        *OutWorldContractReport = WorldReport;
    }
    if (!OutWorld || OutWorld->WorldType != EWorldType::PIE ||
        !bWorldContractValid)
    {
        OutError = bRequireR28VisualSuccessor
            ? TEXT("The active world is not the exact validated Explore V5D R28 visual-successor PIE package. ") +
                  WorldReport
            : TEXT("The active world is not the exact validated Explore V5D hybrid PIE package. ") +
            WorldReport;
        return false;
    }

    int32 PolicyCount = 0;
    int32 GroundVegetationCount = 0;
    int32 TreeRealismCount = 0;
    int32 DynamicRangeCount = 0;
    int32 PublicRealmCount = 0;
    int32 TilesetCount = 0;
    int32 V4Count = 0;
    int32 V5Count = 0;
    int32 V5BCount = 0;
    OutPolicy = FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
        OutWorld,
        PolicyCount);
    OutTileset = FindExactlyOne<ACesium3DTileset>(OutWorld, TilesetCount);
    ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            OutWorld,
            GroundVegetationCount);
    ATRIADIstanaExploreV5DTreeRealismActor* TreeRealism =
        FindExactlyOne<ATRIADIstanaExploreV5DTreeRealismActor>(
            OutWorld,
            TreeRealismCount);
    ATRIADIstanaExploreV5DDynamicRangeRealismActor* DynamicRange =
        FindExactlyOne<ATRIADIstanaExploreV5DDynamicRangeRealismActor>(
            OutWorld,
            DynamicRangeCount);
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
            OutWorld,
            PublicRealmCount);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(OutWorld, V4Count);
    ATRIADIstanaExploreV5AppearanceActor* V5 =
        FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(OutWorld, V5Count);
    ATRIADIstanaExploreV5BVisualActor* V5B =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(OutWorld, V5BCount);
    OutPlayer = UGameplayStatics::GetPlayerController(OutWorld, 0);
    OutPawn = OutPlayer
        ? Cast<ATRIADIstanaExploreV5Pawn>(OutPlayer->GetPawn())
        : nullptr;
    FString PolicyReport;
    FString V5AppearanceReport;
    FString GroundVegetationReport;
    FString TreeRealismReport;
    FString DynamicRangeReport;
    FString PublicRealmReport;
    const float LoadProgress = OutTileset
        ? OutTileset->GetLoadProgress()
        : -1.0f;
    if (PolicyCount != 1 || GroundVegetationCount != 1 ||
        TreeRealismCount != 1 ||
        DynamicRangeCount != 1 || PublicRealmCount != 1 ||
        TilesetCount != 1 || V4Count != 1 || V5Count != 1 ||
        V5BCount != 1 ||
        !OutPolicy || !GroundVegetation || !TreeRealism || !DynamicRange ||
        !PublicRealm ||
        !OutTileset ||
        !V4 || !V5 || !V5B ||
        !OutPlayer || !OutPawn ||
        !OutWorld->GetAuthGameMode() ||
        OutWorld->GetAuthGameMode()->GetClass() !=
            ATRIADIstanaExploreV5GameMode::StaticClass() ||
        !OutPolicy->HasActorBegunPlay() ||
        !OutPolicy->IsActorTickEnabled() ||
        !GroundVegetation->HasActorBegunPlay() ||
        !TreeRealism->HasActorBegunPlay() ||
        !DynamicRange->HasActorBegunPlay() ||
        !PublicRealm->HasActorBegunPlay() ||
        !V5->HasActorBegunPlay() ||
        !TreeRealism->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag()) ||
        !DynamicRange->Tags.Contains(
            ATRIADIstanaExploreV5DDynamicRangeRealismActor::ExpectedActorTag()) ||
        !PublicRealm->Tags.Contains(
            ATRIADIstanaExploreV5DPublicRealmActor::ExpectedActorTag()) ||
        V5->PublicViewSceneActor == nullptr ||
        V5->ExploreV4LandscapeActor != V4 ||
        TreeRealism->V4LandscapeActor != V4 ||
        TreeRealism->V5BVisualActor != V5B ||
        !V5B->HasActorBegunPlay() ||
        !OutPawn->HasExpectedExploreV5CameraProfile() ||
        OutPlayer->GetViewTarget() != OutPawn ||
        !FMath::IsFinite(LoadProgress) || LoadProgress < 0.0f ||
        LoadProgress > 100.0f ||
        !OutPolicy->ValidateHybridContext(PolicyReport) ||
        !V5->ValidateExploreV5Appearance(V5AppearanceReport, true) ||
        !GroundVegetation->ValidateGroundVegetationRealism(
            GroundVegetationReport) ||
        !TreeRealism->ValidateTreeRealism(TreeRealismReport) ||
        !DynamicRange->ValidateDynamicRangeRealism(
            true,
            DynamicRangeReport) ||
        !PublicRealm->ValidatePublicRealm(PublicRealmReport))
    {
        OutError = FString::Printf(
            TEXT("Explore V5D PIE lost exact Player0/V5 pawn, game mode, enabled ContextPolicy tick, runtime actors, finite stream progress, hybrid policy, V5 appearance, ground-realism, tree-realism, dynamic-range, or public-realm contract; policy=%d groundVegetation=%d treeRealism=%d dynamicRange=%d publicRealm=%d tileset=%d v4=%d v5=%d v5b=%d loadProgress=%.3f. policy={%s} v5Appearance={%s} groundVegetation={%s} treeRealism={%s} dynamicRange={%s} publicRealm={%s}"),
            PolicyCount,
            GroundVegetationCount,
            TreeRealismCount,
            DynamicRangeCount,
            PublicRealmCount,
            TilesetCount,
            V4Count,
            V5Count,
            V5BCount,
            LoadProgress,
            *PolicyReport,
            *V5AppearanceReport,
            *GroundVegetationReport,
            *TreeRealismReport,
            *DynamicRangeReport,
            *PublicRealmReport);
        return false;
    }

    OutError.Reset();
    return true;
}

bool ValidateTemasekShophouseOverlay(
    UWorld* World,
    bool bRequireOverlay,
    FString& OutReport)
{
    int32 Count = 0;
    ATRIADIstanaExploreV5DTemasekShophouseActor* Actor =
        FindExactlyOne<ATRIADIstanaExploreV5DTemasekShophouseActor>(
            World,
            Count);
    int32 PolicyCount = 0;
    const ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    FString ActorReport;
    const bool bValid = Count <= 1 &&
        (!bRequireOverlay || Count == 1) &&
        PolicyCount == 1 && Policy &&
        (!Actor ||
         (Actor->ValidateTemasekShophouse(ActorReport) &&
          Actor->bProviderReady ==
              Policy->bLocalBuildingFallbackCurrentlyHidden &&
          Actor->bDedicatedOverlayVisible));
    OutReport = FString::Printf(
        TEXT("temasekShophouseCount=%d required=%s coarseLocalShellSuppressed=true providerAndDedicatedOverlayOverlapUnresolved=true providerStateTelemetryOnly=true renderOnly=true actor={%s}"),
        Count,
        bRequireOverlay ? TEXT("true") : TEXT("false"),
        *ActorReport);
    return bValid;
}

bool ValidateR28Player0Presentation(
    UWorld* World,
    const ATRIADIstanaExploreV5DContextPolicyActor* Policy,
    bool bRequirePlayer0Visible,
    FString& OutReport)
{
    int32 EnvironmentCount = 0;
    const ATRIADIstanaExploreV5DR28EnvironmentActor* Environment =
        FindExactlyOne<ATRIADIstanaExploreV5DR28EnvironmentActor>(
            World,
            EnvironmentCount);
    FString EnvironmentReport;
    const UStaticMeshComponent* Components[] = {
        Environment ? Environment->ConnectivePublicRealmRenderOnly.Get() : nullptr,
        Environment
            ? Environment->ContextArchitecturalDressingRenderOnly.Get()
            : nullptr,
        Environment
            ? Environment->OuterGroundColourReliefOverlayRenderOnly.Get()
            : nullptr};
    bool bAllPlayer0Visible = true;
    bool bAllPlayer0Hidden = true;
    bool bAllSensorSceneCaptureExcluded = true;
    for (const UStaticMeshComponent* Component : Components)
    {
        bAllPlayer0Visible = bAllPlayer0Visible && Component &&
            Component->IsVisible() && !Component->bHiddenInGame;
        bAllPlayer0Hidden = bAllPlayer0Hidden && Component &&
            !Component->IsVisible() && Component->bHiddenInGame;
        bAllSensorSceneCaptureExcluded =
            bAllSensorSceneCaptureExcluded && Component &&
            Component->bHiddenInSceneCapture &&
            !Component->bVisibleInSceneCaptureOnly;
    }
    const bool bExpectedPlayer0Visible =
        Environment && !Environment->bProviderReady;
    const bool bExactPlayer0Visibility = bExpectedPlayer0Visible
        ? bAllPlayer0Visible
        : bAllPlayer0Hidden;
    const bool bNegativeAuthority = Environment &&
        Environment->bRenderOnly &&
        !Environment->bCollisionNavigationSensorOrRfAuthority &&
        !Environment->bMeasuredSurveyAsBuiltOrCurrentCompleteClaimed &&
        !Environment->bExistingSimulationOrRfInputsModified &&
        !Environment->bRuntimeGeometryGenerated;
    if (!World || EnvironmentCount != 1 || !Environment || !Policy ||
        Environment->GetClass() !=
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass() ||
        Environment->IsHidden() || !Environment->HasActorBegunPlay() ||
        !Environment->ValidateR28Environment(EnvironmentReport) ||
        Environment->bProviderReady !=
            Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !bExactPlayer0Visibility || !bAllSensorSceneCaptureExcluded ||
        !bNegativeAuthority ||
        (bRequirePlayer0Visible && !bExpectedPlayer0Visible))
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_R28_PLAYER0_PRESENTATION_INVALID environment=%d providerReadyMatchesPolicy=%s player0Visible=%s player0Hidden=%s sceneCaptureSensorExcluded=%s negativeAuthority=%s requiredPlayer0Visible=%s environment={%s}"),
            EnvironmentCount,
            Environment && Policy && Environment->bProviderReady ==
                    Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            bAllPlayer0Visible ? TEXT("true") : TEXT("false"),
            bAllPlayer0Hidden ? TEXT("true") : TEXT("false"),
            bAllSensorSceneCaptureExcluded ? TEXT("true") : TEXT("false"),
            bNegativeAuthority ? TEXT("true") : TEXT("false"),
            bRequirePlayer0Visible ? TEXT("true") : TEXT("false"),
            *EnvironmentReport);
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R28_PLAYER0_PRESENTATION_VALID r28VisualSuccessor=true r28EnvironmentProviderReady=%s r28EnvironmentPlayer0Visible=%s r28EnvironmentSceneCaptureSensorExcluded=true r28EnvironmentRenderOnly=true r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false existingSimulationOrRfInputsModified=false runtimeGeometryGenerated=false environment={%s}"),
        Environment->bProviderReady ? TEXT("true") : TEXT("false"),
        bExpectedPlayer0Visible ? TEXT("true") : TEXT("false"),
        *EnvironmentReport);
    return true;
}
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    EnsureIstanaExploreV5DFountainRealismMaterialAssets(
        FString& OutReport)
{
    TArray<UMaterial*> Materials;
    if (!TRIADIstanaExploreV5DFountainMaterialFactory::
            EnsureFountainRealismMaterialAssets(Materials, OutReport) ||
        Materials.Num() != 3 || Materials.Contains(nullptr))
    {
        if (OutReport.IsEmpty())
        {
            OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED: exact three-material result was not returned.");
        }
        return false;
    }
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    UpgradeIstanaExploreV5DRequiredNaniteMaterialUsage(FString& OutReport)
{
    if (!CanRunRequiredNaniteMaterialOperation(TEXT("UPGRADE"), OutReport))
    {
        return false;
    }

    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!AssetSubsystem)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_UPGRADE_FAILED_ASSET_SUBSYSTEM");
        return false;
    }

    TArray<UMaterial*> Materials;
    if (!LoadRequiredNaniteMaterialsClean(Materials, OutReport))
    {
        return false;
    }
    for (const UMaterial* Material : Materials)
    {
        if (!Material->GetUsageByFlag(MATUSAGE_Nanite) &&
            (!Material->bAutomaticallySetUsageInEditor ||
             (Material->MaterialDomain != MD_Surface &&
              Material->MaterialDomain != MD_DeferredDecal &&
              Material->MaterialDomain != MD_Volume)))
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_UPGRADE_REFUSED_UNSUPPORTED material=%s domain=%d automaticallySetUsage=%s"),
                *Material->GetPathName(),
                static_cast<int32>(Material->MaterialDomain),
                Material->bAutomaticallySetUsageInEditor
                    ? TEXT("true")
                    : TEXT("false"));
            return false;
        }
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    int32 ChangedCount = 0;
    int32 RecompiledCount = 0;
    for (UMaterial* Material : Materials)
    {
        if (Material->bUsedWithNanite &&
            Material->GetUsageByFlag(MATUSAGE_Nanite))
        {
            continue;
        }

        Material->Modify();
        Material->PreEditChange(nullptr);
        bool bNeedsRecompile = false;
        const bool bUsageSet = Material->SetMaterialUsage(
            bNeedsRecompile,
            MATUSAGE_Nanite);
        Material->PostEditChange();
        if (!bUsageSet || !bNeedsRecompile ||
            !Material->bUsedWithNanite ||
            !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
            !Material->GetOutermost()->IsDirty())
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_UPGRADE_FAILED_SET material=%s usageSet=%s needsRecompile=%s flag=%s packageDirty=%s"),
                *Material->GetPathName(),
                bUsageSet ? TEXT("true") : TEXT("false"),
                bNeedsRecompile ? TEXT("true") : TEXT("false"),
                Material->GetUsageByFlag(MATUSAGE_Nanite)
                    ? TEXT("true")
                    : TEXT("false"),
                Material->GetOutermost()->IsDirty()
                    ? TEXT("true")
                    : TEXT("false"));
            return false;
        }
        ++ChangedCount;
        ++RecompiledCount;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<UObject*> AssetsToSave;
    AssetsToSave.Reserve(RequiredNaniteMaterialCount);
    for (UMaterial* Material : Materials)
    {
        AssetsToSave.Add(Material);
    }
    if (AssetsToSave.Num() != RequiredNaniteMaterialCount ||
        !AssetSubsystem->SaveLoadedAssets(AssetsToSave, true))
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_UPGRADE_FAILED_SAVE saveOffer=%d expected=%d"),
            AssetsToSave.Num(),
            RequiredNaniteMaterialCount);
        return false;
    }

    for (const UMaterial* Material : Materials)
    {
        if (!Material->bUsedWithNanite ||
            !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
            Material->GetOutermost()->IsDirty())
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_UPGRADE_FAILED_VERIFY material=%s flag=%s packageClean=%s"),
                *Material->GetPathName(),
                Material->GetUsageByFlag(MATUSAGE_Nanite)
                    ? TEXT("true")
                    : TEXT("false"),
                Material->GetOutermost()->IsDirty()
                    ? TEXT("false")
                    : TEXT("true"));
            return false;
        }
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_UPGRADE_PASS materials=%d changed=%d recompiled=%d saveOffer=%d allFlags=true packagesClean=true exactPaths=true"),
        Materials.Num(),
        ChangedCount,
        RecompiledCount,
        AssetsToSave.Num());
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DRequiredNaniteMaterialUsage(
        FString& OutReport)
{
    if (!CanRunRequiredNaniteMaterialOperation(TEXT("VALIDATE"), OutReport))
    {
        return false;
    }

    TArray<UMaterial*> Materials;
    if (!LoadRequiredNaniteMaterialsClean(Materials, OutReport))
    {
        return false;
    }
    for (const UMaterial* Material : Materials)
    {
        if (!Material->bUsedWithNanite ||
            !Material->GetUsageByFlag(MATUSAGE_Nanite))
        {
            OutReport = FString::Printf(
                TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_INVALID_FLAG material=%s usage=MATUSAGE_Nanite expected=true"),
                *Material->GetPathName());
            return false;
        }
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_NANITE_MATERIAL_USAGE_VALID materials=%d usage=MATUSAGE_Nanite allFlags=true packagesClean=true exactPaths=true"),
        Materials.Num());
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    BuildIstanaExploreV5DHybridMap(FString& OutMessage)
{
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE start"));
    FString SourceFilename;
    if (!FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_SOURCE: exact V5B source map is absent.");
        return false;
    }
    const int64 SourceBytesBefore = IFileManager::Get().FileSize(*SourceFilename);
    if (SourceBytesBefore <= 0)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_SOURCE: V5B source file is empty or unreadable.");
        return false;
    }
    if (DestinationExists())
    {
        FString DestinationFilename;
        UWorld* Existing = FPackageName::DoesPackageExist(
                DestinationMapPackage,
                &DestinationFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
            : nullptr;
        if (Existing)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        FString ExistingReport;
        if (Existing && ValidateHybridWorld(
                Existing,
                ExistingReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_HYBRID_MAP_ALREADY_VALID: ") +
                ExistingReport;
            return true;
        }
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_PARTIAL_DESTINATION: target exists but is not exact; overwrite is refused. ") +
            ExistingReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE destination_absent"));

    FString CurrentSurroundingsAssetReport;
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE current_asset_preflight_begin"));
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedAsset(
            CurrentSurroundingsAssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_CURRENT_SURROUNDINGS: no world was duplicated or mutated. ") +
            CurrentSurroundingsAssetReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE current_asset_preflight_pass"));

    FString OuterGroundAssetReport;
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE outer_ground_asset_preflight_begin"));
    if (!TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
            ValidateAssets(OuterGroundAssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_OUTER_GROUND: no world was duplicated or mutated. ") +
            OuterGroundAssetReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE outer_ground_asset_preflight_pass"));

    UStaticMesh* MacDonaldHouseMesh = nullptr;
    FString MacDonaldHouseAssetReport;
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_macdonald_asset_preflight_begin"));
    if (!TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::
            LoadValidatedRuntimeMesh(
                MacDonaldHouseMesh,
                MacDonaldHouseAssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_R24_MACDONALD_ASSETS: no world was duplicated or mutated. ") +
            MacDonaldHouseAssetReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_macdonald_asset_preflight_pass"));

    UStaticMesh* TemasekShophouseMesh = nullptr;
    FString TemasekShophouseAssetReport;
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_temasek_asset_preflight_begin"));
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::
            LoadValidatedRuntimeMesh(
                TemasekShophouseMesh,
                TemasekShophouseAssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_R24_TEMASEK_ASSETS: no world was duplicated or mutated. ") +
            TemasekShophouseAssetReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_temasek_asset_preflight_pass"));

    FTRIADIstanaExploreV5DPublicRealmAssets PublicRealmAssets;
    FTRIADIstanaExploreV5DPublicRealmProvenance PublicRealmProvenance;
    FString PublicRealmAssetReport;
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE public_realm_asset_preflight_begin"));
    if (!TRIADIstanaExploreV5DPublicRealmAssetFactory::
            LoadValidatedRuntimeContract(
                PublicRealmAssets,
                PublicRealmProvenance,
                PublicRealmAssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_PUBLIC_REALM: no world was duplicated or mutated. ") +
            PublicRealmAssetReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE public_realm_asset_preflight_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE source_load_begin"));
    UWorld* SourceWorld = UEditorLoadingAndSavingUtils::LoadMap(SourceFilename);
    if (SourceWorld)
    {
        UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE source_finish_compilation_begin"));
        FAssetCompilingManager::Get().FinishAllCompilation();
        UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE source_finish_compilation_pass"));
    }
    FString SourceReport;
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE source_validation_begin"));
    if (!SourceWorld ||
        !UTRIADIstanaExploreV5BEditorLibrary::ValidateIstanaExploreV5BMap(
            SourceReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_SOURCE: V5B validation failed. ") +
            SourceReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE source_validation_pass"));

    SourceWorld = nullptr;
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE duplicate_begin"));
    if (!FEditorFileUtils::LoadMap(SourceFilename, true, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_TEMPLATE: V5B could not be opened as a non-destructive untitled duplicate.");
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE duplicate_pass"));
    UWorld* Target = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!Target || !Target->GetOutermost() ||
        !FPackageName::IsTempPackage(Target->GetOutermost()->GetName()))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_TEMPLATE_GATE: duplicated V5B world is not an untitled temp package.");
        return false;
    }

    int32 SceneCount = 0;
    int32 V4Count = 0;
    int32 V5Count = 0;
    int32 V5BCount = 0;
    int32 ExistingPolicyCount = 0;
    int32 ExistingFountainRealismCount = 0;
    int32 ExistingGroundVegetationCount = 0;
    int32 ExistingTreeRealismCount = 0;
    int32 ExistingDynamicRangeCount = 0;
    int32 ExistingPublicRealmCount = 0;
    int32 ExistingMacDonaldHouseCount = 0;
    int32 ExistingTemasekShophouseCount = 0;
    int32 ExistingTilesetCount = 0;
    int32 ExistingGeoreferenceCount = 0;
    int32 ExistingSiteClipCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(Target, SceneCount);
    FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(Target, V4Count);
    ATRIADIstanaExploreV5AppearanceActor* V5 =
        FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(
            Target,
            V5Count);
    ATRIADIstanaExploreV5BVisualActor* V5B =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(
            Target,
            V5BCount);
    FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
        Target, ExistingPolicyCount);
    FindExactlyOne<ATRIADIstanaExploreV5DFountainRealismActor>(
        Target,
        ExistingFountainRealismCount);
    FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
        Target,
        ExistingGroundVegetationCount);
    FindExactlyOne<ATRIADIstanaExploreV5DTreeRealismActor>(
        Target,
        ExistingTreeRealismCount);
    FindExactlyOne<ATRIADIstanaExploreV5DDynamicRangeRealismActor>(
        Target,
        ExistingDynamicRangeCount);
    FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
        Target,
        ExistingPublicRealmCount);
    FindExactlyOne<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
        Target,
        ExistingMacDonaldHouseCount);
    FindExactlyOne<ATRIADIstanaExploreV5DTemasekShophouseActor>(
        Target,
        ExistingTemasekShophouseCount);
    FindExactlyOne<ACesium3DTileset>(Target, ExistingTilesetCount);
    FindExactlyOne<ACesiumGeoreference>(Target, ExistingGeoreferenceCount);
    FindExactlyOne<ACesiumCartographicPolygon>(Target, ExistingSiteClipCount);
    if (SceneCount != 1 || !Scene || V4Count != 1 || V5Count != 1 ||
        V5BCount != 1 || !V5 || !V5B ||
        ExistingPolicyCount != 0 ||
        ExistingFountainRealismCount != 0 ||
        ExistingGroundVegetationCount != 0 || ExistingTreeRealismCount != 0 ||
        ExistingDynamicRangeCount != 0 ||
        ExistingPublicRealmCount != 0 ||
        ExistingMacDonaldHouseCount != 0 ||
        ExistingTemasekShophouseCount != 0 ||
        ExistingTilesetCount != 0 || ExistingGeoreferenceCount != 0 ||
        ExistingSiteClipCount != 0)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_TEMPLATE_ROSTER: scene=%d v4=%d v5=%d v5b=%d policy=%d fountainRealism=%d groundVegetation=%d treeRealism=%d dynamicRange=%d publicRealm=%d macDonaldHouse=%d temasekShophouse=%d tileset=%d georef=%d siteClip=%d."),
            SceneCount,
            V4Count,
            V5Count,
            V5BCount,
            ExistingPolicyCount,
            ExistingFountainRealismCount,
            ExistingGroundVegetationCount,
            ExistingTreeRealismCount,
            ExistingDynamicRangeCount,
            ExistingPublicRealmCount,
            ExistingMacDonaldHouseCount,
            ExistingTemasekShophouseCount,
            ExistingTilesetCount,
            ExistingGeoreferenceCount,
            ExistingSiteClipCount);
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE template_roster_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE cesium_setup_begin"));
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.OverrideLevel = Target->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SpawnParameters.Name = TEXT("CesiumGeoreference_IstanaV5DHybrid");
    ACesiumGeoreference* Georeference = Target->SpawnActor<ACesiumGeoreference>(
        ACesiumGeoreference::StaticClass(),
        FTransform::Identity,
        SpawnParameters);
    if (!Georeference)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_SPAWN: georeference could not be spawned.");
        return false;
    }
    Georeference->Tags.AddUnique(HybridGeoreferenceTag);
    Georeference->Tags.AddUnique(CesiumDefaultGeoreferenceTag);
    Georeference->SetActorLabel(TEXT("Cesium Georeference - Istana V5D Hybrid"));
    Georeference->SetOriginPlacement(EOriginPlacement::CartographicOrigin);
    Georeference->SetOriginLongitudeLatitudeHeight(FVector(
        IstanaLongitudeDegrees,
        IstanaLatitudeDegrees,
        IstanaFallbackEllipsoidHeightMeters));
    Georeference->SetScale(100.0);
    if (ACesiumGeoreference::GetDefaultGeoreference(Target) != Georeference)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_GEOREFERENCE: intended Istana georeference did not become Cesium's unique persistent-level default.");
        return false;
    }

    SpawnParameters.Name = TEXT("CesiumProviderSiteClip_IstanaV5D");
    ACesiumCartographicPolygon* SiteClip =
        Target->SpawnActor<ACesiumCartographicPolygon>(
            ACesiumCartographicPolygon::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!SiteClip || !SiteClip->Polygon || !SiteClip->GlobeAnchor)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_SPAWN: authored-core provider clipping polygon could not be spawned.");
        return false;
    }
    SiteClip->Tags.AddUnique(HybridSiteClipTag);
    SiteClip->SetActorLabel(TEXT("Cesium Provider Clip - Preserve Authored Istana Core"));
    SiteClip->GlobeAnchor->SetGeoreference(
        TSoftObjectPtr<ACesiumGeoreference>(Georeference));
    // SetGeoreference preserves an already-valid ECEF anchor. Reasserting the
    // authored local transform then re-anchors the polygon at the exact Istana
    // georeference origin instead of retaining any default-georeference offset.
    SiteClip->SetActorTransform(
        FTransform::Identity,
        false,
        nullptr,
        ETeleportType::TeleportPhysics);
    SiteClip->GlobeAnchor->Sync();
    if (SiteClip->GlobeAnchor->GetResolvedGeoreference() != Georeference ||
        !SiteClip->GetActorTransform().Equals(FTransform::Identity, 0.001))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_SITE_CLIP_ANCHOR: Cesium did not retain the exact identity Istana-local clip anchor.");
        return false;
    }
    SiteClip->Polygon->ClearSplinePoints(false);
    const int32 SiteClipSplinePoints =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSplinePoints();
    for (int32 Index = 0; Index < SiteClipSplinePoints; ++Index)
    {
        const FVector2D Point =
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedProviderSiteClipPointCentimeters(Index);
        SiteClip->Polygon->AddSplinePoint(
            FVector(Point.X, Point.Y, 0.0),
            ESplineCoordinateSpace::Local,
            false);
        SiteClip->Polygon->SetSplinePointType(
            Index,
            ESplinePointType::Linear,
            false);
    }
    SiteClip->Polygon->SetClosedLoop(true, false);
    SiteClip->Polygon->UpdateSpline();

    SpawnParameters.Name = TEXT("CesiumPhotorealisticContext_IstanaV5D");
    ACesium3DTileset* Tileset = Target->SpawnActor<ACesium3DTileset>(
        ACesium3DTileset::StaticClass(),
        FTransform::Identity,
        SpawnParameters);
    if (!Tileset)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_SPAWN: visual-only Cesium tileset could not be spawned.");
        return false;
    }
    Tileset->Tags.AddUnique(HybridVisualTilesetTag);
    Tileset->SetActorLabel(TEXT("Cesium Photorealistic 3D Tiles - V5D Visual Context Only"));
    Tileset->SetGeoreference(
        TSoftObjectPtr<ACesiumGeoreference>(Georeference));
    Tileset->SetTilesetSource(ETilesetSource::FromCesiumIon);
    Tileset->SetIonAssetID(GooglePhotorealistic3DTilesIonAssetId);
    Tileset->SetCesiumIonServer(UCesiumIonServer::GetServerForNewObjects());
    // One screen pixel is the V5D high-fidelity context target. The exact
    // provider-safe 12 caps concurrent loads; 2 GiB is the non-required-tile cache target,
    // not a hard process-memory ceiling for tiles required to render the view.
    // Keep a parent rendered until its requested children are available. Pin
    // the normal gradual-refinement limit; a larger value makes a larger LOD
    // tier appear at once and does not improve the final one-pixel target.
    Tileset->SetMaximumScreenSpaceError(1.0);
    Tileset->ApplyDpiScaling = EApplyDpiScaling::No;
    Tileset->MaximumCachedBytes = HybridCacheBytes;
    Tileset->MaximumSimultaneousTileLoads = HybridSimultaneousLoads;
    Tileset->ForbidHoles = true;
    Tileset->LoadingDescendantLimit = HybridLoadingDescendantLimit;
    Tileset->ShowCreditsOnScreen = true;
    Tileset->PreloadAncestors = true;
    Tileset->PreloadSiblings = false;
    Tileset->EnableFrustumCulling = true;
    Tileset->EnableFogCulling = false;
    Tileset->EnforceCulledScreenSpaceError = true;
    Tileset->CulledScreenSpaceError = HybridCulledScreenSpaceError;
    Tileset->SetUseLodTransitions(false);
    Tileset->SetCreatePhysicsMeshes(false);
    Tileset->SetCreateNavCollision(false);
    Tileset->SetGenerateSmoothNormals(false);
    Tileset->SetIgnoreKhrMaterialsUnlit(false);

    UMaterialInterface* CesiumOpaqueClippingMaterial =
        LoadObject<UMaterialInterface>(
            nullptr,
            CesiumOpaqueClippingMaterialPath);
    UMaterialInterface* CesiumTranslucentClippingMaterial =
        LoadObject<UMaterialInterface>(
            nullptr,
            CesiumTranslucentClippingMaterialPath);
    if (!CesiumOpaqueClippingMaterial ||
        !CesiumTranslucentClippingMaterial)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_CLIP_MATERIAL: Cesium clipping-compatible opaque/translucent materials are unavailable.");
        return false;
    }
    Tileset->SetMaterial(CesiumOpaqueClippingMaterial);
    Tileset->SetTranslucentMaterial(CesiumTranslucentClippingMaterial);

    UCesiumPolygonRasterOverlay* SiteClipOverlay =
        NewObject<UCesiumPolygonRasterOverlay>(
            Tileset,
            TEXT("IstanaV5DAuthoredCoreClippingOverlay"),
            RF_Transactional);
    if (!SiteClipOverlay)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_CLIP_OVERLAY: provider clipping overlay could not be created.");
        return false;
    }
    SiteClipOverlay->MaterialLayerKey = TEXT("Clipping");
    SiteClipOverlay->Polygons.Add(
        TSoftObjectPtr<ACesiumCartographicPolygon>(SiteClip));
    SiteClipOverlay->InvertSelection = false;
    SiteClipOverlay->ExcludeSelectedTiles = true;
    Tileset->AddInstanceComponent(SiteClipOverlay);
    SiteClipOverlay->SetAutoActivate(true);
    SiteClipOverlay->RegisterComponent();
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE cesium_setup_pass"));

    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DContextPolicy");
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        Target->SpawnActor<ATRIADIstanaExploreV5DContextPolicyActor>(
            ATRIADIstanaExploreV5DContextPolicyActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Policy)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_SPAWN: runtime context policy could not be spawned.");
        return false;
    }
    Policy->Tags.AddUnique(HybridContextPolicyTag);
    Policy->SetActorLabel(TEXT("TRIAD Istana V5D Hybrid Visual Context Policy"));

    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DPublicRealm");
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE public_realm_config_begin"));
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        Target->SpawnActor<ATRIADIstanaExploreV5DPublicRealmActor>(
            ATRIADIstanaExploreV5DPublicRealmActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    FString PublicRealmReport;
    if (!PublicRealm ||
        !PublicRealm->ConfigurePublicRealm(
            PublicRealmAssets,
            PublicRealmProvenance,
            PublicRealmReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_PUBLIC_REALM: target has not been saved. ") +
            PublicRealmReport;
        return false;
    }
    PublicRealm->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D Public Realm Render-Only Partition"));
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE public_realm_config_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE fountain_materials_begin"));
    TArray<UMaterial*> FountainMaterials;
    FString FountainMaterialsReport;
    if (!TRIADIstanaExploreV5DFountainMaterialFactory::
            EnsureFountainRealismMaterialAssets(
                FountainMaterials,
                FountainMaterialsReport) ||
        FountainMaterials.Num() != 3 || FountainMaterials.Contains(nullptr) ||
        FountainMaterials[0]->GetPathName() !=
            TRIADIstanaExploreV5DFountainMaterialFactory::
                GetWaterMaterialObjectPath() ||
        FountainMaterials[1]->GetPathName() !=
            TRIADIstanaExploreV5DFountainMaterialFactory::
                GetSprayMaterialObjectPath() ||
        FountainMaterials[2]->GetPathName() !=
            TRIADIstanaExploreV5DFountainMaterialFactory::
                GetEmbeddedWaterSuppressorMaterialObjectPath())
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_FOUNTAIN_MATERIALS: target has not been saved. ") +
            FountainMaterialsReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE fountain_materials_pass"));
    UMaterialInterface* FountainWaterMaterial = FountainMaterials[0];
    UMaterialInterface* FountainSprayMaterial = FountainMaterials[1];
    UMaterialInterface* EmbeddedWaterSuppressorMaterial =
        FountainMaterials[2];

    // Replace only the copied V5B fountain renderers before the ground pass
    // asks the context policy to validate its complete authored-core visual
    // roster. Source meshes/materials/transforms/census and every collision,
    // navigation, sensor, RF, and hydraulic truth surface remain untouched.
    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DFountainRealism");
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE fountain_config_begin"));
    ATRIADIstanaExploreV5DFountainRealismActor* FountainRealism =
        Target->SpawnActor<ATRIADIstanaExploreV5DFountainRealismActor>(
            ATRIADIstanaExploreV5DFountainRealismActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    FString FountainRealismReport;
    if (!FountainRealism ||
        !FountainRealism->ConfigureFountainRealism(
            V5,
            V5B,
            FountainWaterMaterial,
            FountainSprayMaterial,
            EmbeddedWaterSuppressorMaterial,
            true,
            FountainRealismReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_FOUNTAIN_REALISM: target has not been saved. ") +
            FountainRealismReport;
        return false;
    }
    FountainRealism->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D Fountain Appearance Successor"));
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE fountain_config_pass"));

    // Tree transitions reuse the isolated V5D soil material, while the ground
    // actor's strict post-configuration policy validation requires the tree
    // actor to be present. Materialize that shared asset first, then build the
    // tree owner before the ground owner so the complete runtime-owner contract
    // is already present when ground validation runs.
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE ground_materials_begin"));
    FString GroundVegetationMaterialsReport;
    if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
            EnsureGroundVegetationRealismMaterialAssets(
                GroundVegetationMaterialsReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_GROUND_VEGETATION_MATERIALS: target has not been saved. ") +
            GroundVegetationMaterialsReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE ground_materials_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE tree_begin"));
    FString TreeRealismReport;
    if (!UTRIADIstanaExploreV5DTreeRealismEditorLibrary::
            ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder(
                Target,
                true,
                TreeRealismReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_TREE_REALISM: target has not been saved. ") +
            TreeRealismReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE tree_pass"));

    // Fountain and tree setup both require the exact inherited V5/V5B
    // presentation while they snapshot their sources. Only after both strict
    // source gates succeed may the dated current-context owner hide the V5C
    // building renderer. Subsequent validators admit that replacement only
    // through the exact fail-closed V5D policy contract.
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE current_surroundings_begin"));
    const FString& CurrentSurroundingsMeshPath =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedCurrentSurroundingsMeshObjectPath();
    UStaticMesh* CurrentSurroundingsMesh =
        LoadObject<UStaticMesh>(nullptr, *CurrentSurroundingsMeshPath);
    const FString& OuterGroundMeshPath =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedOuterGroundLoadingFallbackMeshObjectPath();
    UStaticMesh* OuterGroundMesh =
        LoadObject<UStaticMesh>(nullptr, *OuterGroundMeshPath);
    FString CurrentSurroundingsReport;
    if (!CurrentSurroundingsMesh ||
        CurrentSurroundingsMesh->GetPathName() != CurrentSurroundingsMeshPath ||
        !OuterGroundMesh ||
        OuterGroundMesh->GetPathName() != OuterGroundMeshPath ||
        !Policy->ConfigureCurrentSurroundingsAndOuterGroundPresentation(
            CurrentSurroundingsMesh,
            OuterGroundMesh,
            Scene,
            CurrentSurroundingsReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_CURRENT_SURROUNDINGS: target has not been saved. ") +
            CurrentSurroundingsReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE current_surroundings_pass"));

    // The volunteered OSM-bound landmark sits outside the existing 185 x 245 m
    // authored-core provider clip. Keep that clip unchanged. Global provider
    // readiness is recorded only as telemetry; without landmark-specific
    // readiness proof it must never gate this dedicated high-detail overlay.
    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DR24MacDonaldHouse");
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_macdonald_config_begin"));
    ATRIADIstanaExploreV5DMacDonaldHouseActor* MacDonaldHouse =
        Target->SpawnActor<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
            ATRIADIstanaExploreV5DMacDonaldHouseActor::StaticClass(),
            ATRIADIstanaExploreV5DMacDonaldHouseActor::
                ExpectedPlacementTransform(),
            SpawnParameters);
    FString MacDonaldHouseReport;
    if (!MacDonaldHouse ||
        !MacDonaldHouse->ConfigureMacDonaldHouse(
            MacDonaldHouseMesh,
            Policy->bLocalBuildingFallbackCurrentlyHidden,
            MacDonaldHouseReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_R24_MACDONALD: target has not been saved. ") +
            MacDonaldHouseReport;
        return false;
    }
    MacDonaldHouse->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R24 MacDonald House - Persistent Render Only"));
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_macdonald_config_pass"));

    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DR24TemasekShophouse");
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_temasek_config_begin"));
    ATRIADIstanaExploreV5DTemasekShophouseActor* TemasekShophouse =
        Target->SpawnActor<ATRIADIstanaExploreV5DTemasekShophouseActor>(
            ATRIADIstanaExploreV5DTemasekShophouseActor::StaticClass(),
            ATRIADIstanaExploreV5DTemasekShophouseActor::
                ExpectedPlacementTransform(),
            SpawnParameters);
    FString TemasekShophouseReport;
    if (!TemasekShophouse ||
        !TemasekShophouse->ConfigureTemasekShophouse(
            TemasekShophouseMesh,
            Policy->bLocalBuildingFallbackCurrentlyHidden,
            TemasekShophouseReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_R24_TEMASEK: target has not been saved. ") +
            TemasekShophouseReport;
        return false;
    }
    TemasekShophouse->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R24 Temasek Shophouse - Persistent Render Only"));
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE r24_temasek_config_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE ground_begin"));
    FString GroundVegetationReport;
    if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
            ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder(
                Target,
                true,
                GroundVegetationReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_GROUND_VEGETATION: target has not been saved. ") +
            GroundVegetationReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE ground_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE dynamic_range_begin"));
    FString DynamicRangeReport;
    if (!UTRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary::
            ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder(
                Target,
                true,
                DynamicRangeReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_DYNAMIC_RANGE: target has not been saved. ") +
            DynamicRangeReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE dynamic_range_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE save_begin"));
    if (DestinationExists())
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_REFUSED_LATE_DESTINATION: target appeared after preflight.");
        return false;
    }
    Target->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            Target, DestinationMapPackage) ||
        !Target->GetOutermost() ||
        Target->GetOutermost()->GetName() != DestinationMapPackage ||
        IFileManager::Get().FileSize(*SourceFilename) != SourceBytesBefore)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_SAVE: only the new target was offered to save; V5B source guard changed or save failed.");
        return false;
    }
    FString PreSaveReport;
    if (!ValidateHybridWorld(
            Target,
            PreSaveReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_PRE_RELOAD_VALIDATION: final two-landmark contract failed after save. ") +
            PreSaveReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE save_pass"));

    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE reload_source_begin"));
    UWorld* ReloadedSource = UEditorLoadingAndSavingUtils::LoadMap(SourceFilename);
    FString ReloadedSourceReport;
    if (!ReloadedSource ||
        !UTRIADIstanaExploreV5BEditorLibrary::ValidateIstanaExploreV5BMap(
            ReloadedSourceReport) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath) ||
        IFileManager::Get().FileSize(*SourceFilename) != SourceBytesBefore)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_COLD_UNLOAD: target could not be unloaded at the still-valid V5B source. ") +
            ReloadedSourceReport;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD_V5D_BUILD_STAGE reload_source_pass"));

    FString DestinationFilename;
    UWorld* ReloadedTarget = FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
        : nullptr;
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    FString ColdReport;
    if (!ReloadedTarget || !ValidateHybridWorld(
            ReloadedTarget,
            ColdReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required) ||
        IFileManager::Get().FileSize(*SourceFilename) != SourceBytesBefore)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_BUILD_FAILED_COLD_RELOAD: ") +
            ColdReport;
        return false;
    }
    OutMessage = TEXT("Created and cold-reload-validated /Game/Maps/Istana_PublicView_Explore_v5d_hybrid with exact persistent MacDonald House and Temasek Shophouse render-only overlays. It retains the authored V5B Istana/site and deterministic simulation layers, replaces only the copied V5B fountain renderers with exact V5D-owned world-XY PNO water and zero-floor spray materials plus deterministic asymmetric spray fragments, anisotropic impact ripples, and a layered central disturbance while preserving source meshes/materials/transforms/census, adds a world-stochastic chroma/roughness lawn overlay plus deterministic grass/soil/understorey micro-detail, restores automatic near-to-distance tree LODs through isolated V5D derivatives and adds render-only root/litter transitions, applies an isolated local-exposure dynamic-range pass while preserving the exact tagged 80000-lux sun and V5 physical-camera profile, and clips streamed provider geometry out of a compact deterministic 64-vertex irregular-ellipse visual core centered 55 m along the ceremonial axis with 185 m/245 m semi-axes at every view altitude. Provider load readiness controls only the render-only local fallbacks after the pinned three-sample/98-percent dwell and 90-percent restore hysteresis; the authored core and clipping overlay stay active, with no aerial handoff. It adds a hash-pinned 0-300 m core/fallback public-realm partition with official planning-road graphics plus public OSM road ribbons and only explicitly tagged sidewalk/low-kerb presentation; that fallback follows the same provider-load readiness policy while the whole actor keeps the stable authored-core/clipping policy, and it remains scene-capture-excluded, NoCollision, non-navigable, render-only, and non-survey/non-RF authority. It uses the deterministic 2026-08-31 public-OSM building context through the exact 43,492-triangle suppression-V1 local fallback: only OSM ways 46521250 and 1551538490 are removed from that coarse mesh, while every other building, vertex, UV, material slot, the canonical source asset, and RF inputs remain intact. It also adds a bounded 1,000-1,250 m render-only outer-ground loading annulus that follows the same provider-readiness transition and has no terrain, collision, navigation, sensor, RF, survey, or as-built authority. R24 adds one distinct 7,080-triangle MacDonald House public-exterior renderer bound to volunteered OSM way 46521250 and one distinct 25,600-triangle Temasek Shophouse public-exterior renderer bound to volunteered OSM way 1551538490, each with disclosed non-uniform visual-fit limitations and deterministic texture-free PBR priors. Because global Cesium load progress is not landmark-specific readiness proof, both dedicated overlays stay visible through provider false/true/false transitions; their exact coarse local shells are suppressed, but possible provider/dedicated-overlay overlap remains explicit because no landmark-specific provider exclusion is claimed. Neither public layer claims measured height, facade, aperture, material calibration, survey, as-built, 1:1, collision, navigation, sensor, or RF authority. Live provider photorealistic context remains visual-only at SSE 1. TRIAD code does not explicitly request, read, serialize, or log the provider token and does not export, geometrically trace, analyse, derive, or bake provider content. Cesium for Unreal retains its standard in-memory tile cache and persistent SQLite HTTP request cache. No Cesium or appearance-only layer received collision, navigation, sensor, or RF authority. ") +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DPublicRealmPassToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_REFUSED_MAP: load the exact V5D hybrid destination map first.");
        return false;
    }

    int32 ExistingPublicRealmCount = 0;
    FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
        World,
        ExistingPublicRealmCount);
    if (ExistingPublicRealmCount == 1 && !MapPackage->IsDirty())
    {
        FString ExistingReport;
        if (ValidateHybridWorld(World, ExistingReport,
                ELandmarkPresencePolicy::Forbidden,
                ELandmarkPresencePolicy::Forbidden))
        {
            OutMessage =
                TEXT("IDEMPOTENT_EXPLORE_V5D_PUBLIC_REALM_PASS_ALREADY_VALID: ") +
                ExistingReport;
            return true;
        }
    }
    if (ExistingPublicRealmCount != 0 || MapPackage->IsDirty())
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_REFUSED_PARTIAL_OR_DIRTY: publicRealm=%d packageDirty=%s."),
            ExistingPublicRealmCount,
            MapPackage->IsDirty() ? TEXT("true") : TEXT("false"));
        return false;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_REFUSED_DISK_MAP: exact destination file is absent.");
        return false;
    }
    FString DestinationSha256;
    int64 DestinationBytes = INDEX_NONE;
    FString Error;
    if (!HashFileSha256(
            DestinationFilename,
            DestinationSha256,
            DestinationBytes,
            Error) ||
        DestinationBytes != PrePublicRealmDestinationBytes ||
        DestinationSha256 != PrePublicRealmDestinationSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_REFUSED_PREDECESSOR_HASH: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PrePublicRealmDestinationBytes,
            DestinationBytes,
            *PrePublicRealmDestinationSha256,
            *DestinationSha256,
            *Error);
        return false;
    }

    FTRIADIstanaExploreV5DPublicRealmAssets Assets;
    FTRIADIstanaExploreV5DPublicRealmProvenance Provenance;
    if (!TRIADIstanaExploreV5DPublicRealmAssetFactory::
            LoadValidatedRuntimeContract(Assets, Provenance, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_REFUSED_ASSETS: map was not changed. ") +
            Error;
        return false;
    }

    FString BackupFilename;
    if (!CreateVerifiedPrePublicRealmMapBackup(
            DestinationFilename,
            BackupFilename,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }

    auto ColdRestoreVerifiedPredecessor =
        [&](ATRIADIstanaExploreV5DPublicRealmActor* SpawnedActor,
            FString& OutRollbackReport) -> bool
    {
        if (SpawnedActor)
        {
            SpawnedActor->Destroy();
        }

        // Clearing the transient dirty bit is safe only as an immediate prelude
        // to unloading this mutated world. Never return with that in-memory
        // package masquerading as the admitted predecessor.
        MapPackage->SetDirtyFlag(false);
        FString SourceFilename;
        UWorld* RollbackSource =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        if (!RollbackSource)
        {
            MapPackage->SetDirtyFlag(true);
            OutRollbackReport =
                TEXT("rollback failed to unload the mutated destination through the exact source map; destination package was left dirty.");
            return false;
        }

        UWorld* RestoredTarget =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredTarget)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage =
            RestoredTarget ? RestoredTarget->GetOutermost() : nullptr;
        int32 RestoredPublicRealmCount = 0;
        if (RestoredTarget)
        {
            FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
                RestoredTarget,
                RestoredPublicRealmCount);
        }
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bDiskStillExact = HashFileSha256(
            DestinationFilename,
            RestoredSha256,
            RestoredBytes,
            HashError) &&
            RestoredBytes == PrePublicRealmDestinationBytes &&
            RestoredSha256 == PrePublicRealmDestinationSha256;
        const bool bRestored = RestoredTarget && RestoredPackage &&
            UWorld::RemovePIEPrefix(RestoredPackage->GetName()) ==
                DestinationMapPackage &&
            !RestoredPackage->IsDirty() && RestoredPublicRealmCount == 0 &&
            bDiskStillExact;
        OutRollbackReport = FString::Printf(
            TEXT("cold predecessor restore=%s package=%s packageDirty=%s publicRealm=%d bytes=%lld sha256=%s hashError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            RestoredPackage ? *RestoredPackage->GetName() : TEXT("<null>"),
            RestoredPackage && RestoredPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            RestoredPublicRealmCount,
            RestoredBytes,
            *RestoredSha256,
            *HashError);
        return bRestored;
    };

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DPublicRealm");
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        World->SpawnActor<ATRIADIstanaExploreV5DPublicRealmActor>(
            ATRIADIstanaExploreV5DPublicRealmActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    FString PublicRealmReport;
    if (!PublicRealm ||
        !PublicRealm->ConfigurePublicRealm(
            Assets,
            Provenance,
            PublicRealmReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = ColdRestoreVerifiedPredecessor(
            PublicRealm,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_FAILED_CONFIGURE: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *PublicRealmReport,
            *RollbackReport);
        return false;
    }
    PublicRealm->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D Public Realm Render-Only Partition"));

    FString PreSaveReport;
    if (!ValidateHybridWorld(World, PreSaveReport,
            ELandmarkPresencePolicy::Forbidden,
            ELandmarkPresencePolicy::Forbidden))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = ColdRestoreVerifiedPredecessor(
            PublicRealm,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_FAILED_PRE_SAVE_VALIDATION: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_FAILED_SAVE: verified predecessor backup remains at ") +
            BackupFilename;
        return false;
    }

    FString SourceFilename;
    UWorld* ReloadedSource =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    if (!ReloadedSource)
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_FAILED_COLD_UNLOAD: map was saved and verified predecessor backup remains at ") +
            BackupFilename;
        return false;
    }

    UWorld* ReloadedTarget = UEditorLoadingAndSavingUtils::LoadMap(
        DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    FString ColdReport;
    if (!ReloadedTarget ||
        !ValidateHybridWorld(ReloadedTarget, ColdReport,
            ELandmarkPresencePolicy::Forbidden,
            ELandmarkPresencePolicy::Forbidden))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_FAILED_COLD_VALIDATION: saved map requires recovery from verified backup=") +
            BackupFilename + TEXT(". ") + ColdReport;
        return false;
    }

    OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_APPLY_PASS: exact hash-pinned predecessor was backed up, one identity render-only public-realm actor was configured, only the destination map was saved, and the result cold-reload-validated; backup=") +
        BackupFilename + TEXT(". ") + ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DPublicRealmMaterialCorrectionToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_MAP: load the exact V5D hybrid destination map first.");
        return false;
    }

    int32 PublicRealmCount = 0;
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
            World,
            PublicRealmCount);
    if (PublicRealmCount == 1 && PublicRealm && !MapPackage->IsDirty())
    {
        FString ExistingReport;
        if (ValidateHybridWorld(World, ExistingReport,
                ELandmarkPresencePolicy::Forbidden,
                ELandmarkPresencePolicy::Forbidden))
        {
            FString ExistingFilename;
            FString ExistingSha256;
            int64 ExistingBytes = INDEX_NONE;
            FString ExistingHashError;
            const bool bFinalHashPinned =
                CorrectedPublicRealmMaterialCorrectionBytes > 0 &&
                !CorrectedPublicRealmMaterialCorrectionSha256.IsEmpty();
            const bool bExistingHashRead =
                FPackageName::DoesPackageExist(
                    DestinationMapPackage,
                    &ExistingFilename) &&
                HashFileSha256(
                    ExistingFilename,
                    ExistingSha256,
                    ExistingBytes,
                    ExistingHashError);
            const bool bPlanningGroundSuccessorHashPinned =
                CorrectedInheritedPlanningGroundSuppressionBytes > 0 &&
                !CorrectedInheritedPlanningGroundSuppressionSha256.IsEmpty();
            const bool bPlanningGroundSuccessorHashExact =
                bExistingHashRead && bPlanningGroundSuccessorHashPinned &&
                ExistingBytes ==
                    CorrectedInheritedPlanningGroundSuppressionBytes &&
                ExistingSha256 ==
                    CorrectedInheritedPlanningGroundSuppressionSha256;
            if (bPlanningGroundSuccessorHashExact)
            {
                OutMessage =
                    TEXT("IDEMPOTENT_EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_SUPERSEDED_BY_PINNED_PLANNING_GROUND_SUPPRESSION: R2 material bindings remain valid in the exact clean downstream map successor. ") +
                    ExistingReport;
                return true;
            }
            const bool bExistingHashExact = bExistingHashRead &&
                bFinalHashPinned &&
                ExistingBytes ==
                    CorrectedPublicRealmMaterialCorrectionBytes &&
                ExistingSha256 ==
                    CorrectedPublicRealmMaterialCorrectionSha256;
            if (!bExistingHashExact)
            {
                OutMessage = FString::Printf(
                    TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_CORRECTED_HASH_UNPINNED_OR_DRIFTED: finalHashPinned=%s expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s packageClean=true structural={%s} hashError={%s}."),
                    bFinalHashPinned ? TEXT("true") : TEXT("false"),
                    CorrectedPublicRealmMaterialCorrectionBytes,
                    ExistingBytes,
                    *CorrectedPublicRealmMaterialCorrectionSha256,
                    *ExistingSha256,
                    *ExistingReport,
                    *ExistingHashError);
                return false;
            }
            OutMessage =
                TEXT("IDEMPOTENT_EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_ALREADY_VALID: exact corrected disk hash and clean cold structural state are pinned. ") +
                ExistingReport;
            return true;
        }
    }
    FString LegacyReport;
    if (PublicRealmCount != 1 || !PublicRealm || MapPackage->IsDirty() ||
        !PublicRealm->ValidateLegacyVisualMaterialContract(LegacyReport))
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_PARTIAL_DIRTY_OR_NONLEGACY: publicRealm=%d packageDirty=%s legacy={%s}."),
            PublicRealmCount,
            MapPackage->IsDirty() ? TEXT("true") : TEXT("false"),
            *LegacyReport);
        return false;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_DISK_MAP: exact destination file is absent.");
        return false;
    }
    FString DestinationSha256;
    int64 DestinationBytes = INDEX_NONE;
    FString Error;
    if (!HashFileSha256(
            DestinationFilename,
            DestinationSha256,
            DestinationBytes,
            Error) ||
        DestinationBytes != PrePublicRealmMaterialCorrectionBytes ||
        DestinationSha256 != PrePublicRealmMaterialCorrectionSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_PREDECESSOR_HASH: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PrePublicRealmMaterialCorrectionBytes,
            DestinationBytes,
            *PrePublicRealmMaterialCorrectionSha256,
            *DestinationSha256,
            *Error);
        return false;
    }

    FTRIADIstanaExploreV5DPublicRealmAssets Assets;
    FTRIADIstanaExploreV5DPublicRealmProvenance Provenance;
    if (!TRIADIstanaExploreV5DPublicRealmAssetFactory::
            LoadValidatedRuntimeContract(Assets, Provenance, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_ASSETS: map was not changed. ") +
            Error;
        return false;
    }
    if (PublicRealm->SavedProvenance.SchemaRevision !=
            Provenance.SchemaRevision ||
        PublicRealm->SavedProvenance.SourceEpoch != Provenance.SourceEpoch ||
        PublicRealm->SavedProvenance.CoreSourceIdentifier !=
            Provenance.CoreSourceIdentifier ||
        PublicRealm->SavedProvenance.CoreSourceSha256 !=
            Provenance.CoreSourceSha256 ||
        PublicRealm->SavedProvenance.FallbackSourceIdentifier !=
            Provenance.FallbackSourceIdentifier ||
        PublicRealm->SavedProvenance.FallbackSourceSha256 !=
            Provenance.FallbackSourceSha256)
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_PROVENANCE_DRIFT: the exact visual predecessor and validated runtime contract disagree; map was not changed.");
        return false;
    }

    FString BackupFilename;
    if (!CreateVerifiedPrePublicRealmMaterialCorrectionMapBackup(
            DestinationFilename,
            BackupFilename,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }

    auto ColdRestoreVerifiedMaterialPredecessor =
        [&](FString& OutRollbackReport) -> bool
    {
        // Clear only the currently loaded destination package immediately
        // before unloading it. MapPackage may no longer be the live package
        // when rollback follows a post-save cold-reload failure.
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (CurrentPackage &&
            UWorld::RemovePIEPrefix(CurrentPackage->GetName()) ==
                DestinationMapPackage)
        {
            CurrentPackage->SetDirtyFlag(false);
        }
        FString SourceFilename;
        UWorld* RollbackSource =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        if (!RollbackSource)
        {
            UWorld* FailedCurrentWorld = GEditor
                ? GEditor->GetEditorWorldContext().World()
                : nullptr;
            UPackage* FailedCurrentPackage = FailedCurrentWorld
                ? FailedCurrentWorld->GetOutermost()
                : nullptr;
            if (FailedCurrentPackage &&
                UWorld::RemovePIEPrefix(FailedCurrentPackage->GetName()) ==
                    DestinationMapPackage)
            {
                FailedCurrentPackage->SetDirtyFlag(true);
            }
            OutRollbackReport = TEXT("rollback failed to unload the mutated destination through the exact source map; no disk copy was attempted and the verified backup remains authoritative.");
            return false;
        }

        FString BackupSha256;
        int64 BackupBytes = INDEX_NONE;
        FString BackupHashError;
        const bool bBackupStillExact = HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            BackupHashError) &&
            BackupBytes == PrePublicRealmMaterialCorrectionBytes &&
            BackupSha256 == PrePublicRealmMaterialCorrectionSha256;
        if (!bBackupStillExact ||
            IFileManager::Get().Copy(
                *DestinationFilename,
                *BackupFilename,
                true,
                true) != COPY_OK)
        {
            OutRollbackReport = FString::Printf(
                TEXT("rollback unloaded the mutated destination but could not restore the exact verified backup: backupExact=%s bytes=%lld sha256=%s hashError={%s}."),
                bBackupStillExact ? TEXT("true") : TEXT("false"),
                BackupBytes,
                *BackupSha256,
                *BackupHashError);
            return false;
        }

        FString CopiedSha256;
        int64 CopiedBytes = INDEX_NONE;
        FString CopiedHashError;
        const bool bCopiedDiskExact = HashFileSha256(
            DestinationFilename,
            CopiedSha256,
            CopiedBytes,
            CopiedHashError) &&
            CopiedBytes == PrePublicRealmMaterialCorrectionBytes &&
            CopiedSha256 == PrePublicRealmMaterialCorrectionSha256;
        if (!bCopiedDiskExact)
        {
            OutRollbackReport = FString::Printf(
                TEXT("rollback copied the verified backup but destination hash verification failed: bytes=%lld sha256=%s hashError={%s}."),
                CopiedBytes,
                *CopiedSha256,
                *CopiedHashError);
            return false;
        }
        UWorld* RestoredTarget =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredTarget)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage =
            RestoredTarget ? RestoredTarget->GetOutermost() : nullptr;
        int32 RestoredPublicRealmCount = 0;
        ATRIADIstanaExploreV5DPublicRealmActor* RestoredPublicRealm =
            RestoredTarget
            ? FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
                  RestoredTarget,
                  RestoredPublicRealmCount)
            : nullptr;
        FString RestoredLegacyReport;
        const bool bLegacyValid = RestoredPublicRealm &&
            RestoredPublicRealm->ValidateLegacyVisualMaterialContract(
                RestoredLegacyReport);
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bDiskStillExact = HashFileSha256(
            DestinationFilename,
            RestoredSha256,
            RestoredBytes,
            HashError) &&
            RestoredBytes == PrePublicRealmMaterialCorrectionBytes &&
            RestoredSha256 == PrePublicRealmMaterialCorrectionSha256;
        const bool bRestored = RestoredTarget && RestoredPackage &&
            UWorld::RemovePIEPrefix(RestoredPackage->GetName()) ==
                DestinationMapPackage &&
            !RestoredPackage->IsDirty() && RestoredPublicRealmCount == 1 &&
            bLegacyValid && bDiskStillExact;
        OutRollbackReport = FString::Printf(
            TEXT("cold material predecessor restore=%s package=%s packageDirty=%s publicRealm=%d legacyValid=%s bytes=%lld sha256=%s legacy={%s} hashError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            RestoredPackage ? *RestoredPackage->GetName() : TEXT("<null>"),
            RestoredPackage && RestoredPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            RestoredPublicRealmCount,
            bLegacyValid ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            *RestoredLegacyReport,
            *HashError);
        return bRestored;
    };

    FString UpgradeReport;
    if (!PublicRealm->UpgradeVisualMaterials(Assets, UpgradeReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedMaterialPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_FAILED_UPGRADE: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *UpgradeReport,
            *RollbackReport);
        return false;
    }

    FString PreSaveReport;
    if (!ValidateHybridWorld(World, PreSaveReport,
            ELandmarkPresencePolicy::Forbidden,
            ELandmarkPresencePolicy::Forbidden))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedMaterialPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_FAILED_PRE_SAVE_VALIDATION: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedMaterialPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_FAILED_SAVE: rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* ReloadedSource =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    if (!ReloadedSource)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedMaterialPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_FAILED_COLD_UNLOAD: saved mutation rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }
    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    FString ColdReport;
    UPackage* ReloadedPackage =
        ReloadedTarget ? ReloadedTarget->GetOutermost() : nullptr;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateHybridWorld(ReloadedTarget, ColdReport,
            ELandmarkPresencePolicy::Forbidden,
            ELandmarkPresencePolicy::Forbidden))
    {
        const bool bColdPackageClean =
            ReloadedPackage && !ReloadedPackage->IsDirty();
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedMaterialPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_FAILED_COLD_VALIDATION: saved mutation rollbackSucceeded=%s verifiedBackup=%s coldPackageClean=%s cold={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            bColdPackageClean ? TEXT("true") : TEXT("false"),
            *ColdReport,
            *RollbackReport);
        return false;
    }

    FString CorrectedSha256;
    int64 CorrectedBytes = INDEX_NONE;
    FString CorrectedHashError;
    if (!HashFileSha256(
            DestinationFilename,
            CorrectedSha256,
            CorrectedBytes,
            CorrectedHashError))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedMaterialPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_FAILED_FINAL_HASH: saved mutation rollbackSucceeded=%s verifiedBackup=%s hashError={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *CorrectedHashError,
            *RollbackReport);
        return false;
    }
    const bool bFinalHashPinned =
        CorrectedPublicRealmMaterialCorrectionBytes > 0 &&
        !CorrectedPublicRealmMaterialCorrectionSha256.IsEmpty();
    if (bFinalHashPinned &&
        (CorrectedBytes != CorrectedPublicRealmMaterialCorrectionBytes ||
         CorrectedSha256 !=
             CorrectedPublicRealmMaterialCorrectionSha256))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedMaterialPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_FAILED_FINAL_HASH_DRIFT: saved mutation rollbackSucceeded=%s expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            CorrectedPublicRealmMaterialCorrectionBytes,
            CorrectedBytes,
            *CorrectedPublicRealmMaterialCorrectionSha256,
            *CorrectedSha256,
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_PUBLIC_REALM_MATERIAL_CORRECTION_PASS: exact CDBE hash-pinned predecessor was backed up; only the existing actor's appearance revision, road material references, and exact component override slots were changed; both mesh packages, source bindings, concrete, transforms, visibility, provider readiness, collision, navigation, provenance, R15 ground packages, and sensor/RF authority remained unchanged; result is clean, cold-reload-validated, and measured for final idempotence pinning; correctedBytes=%lld correctedSha256=%s backup=%s. %s"),
        CorrectedBytes,
        *CorrectedSha256,
        *BackupFilename,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DInheritedPlanningGroundSuppressionToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_MAP: load the exact V5D hybrid destination map first.");
        return false;
    }
    if (MapPackage->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_DIRTY_MAP: the exact destination package must be clean.");
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 PublicRealmCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
            World,
            PublicRealmCount);
    UStaticMeshComponent* PlanningGround = Scene
        ? Scene->V5CGroundContextRenderOnlyComponent
        : nullptr;
    if (SceneCount != 1 || PolicyCount != 1 || PublicRealmCount != 1 ||
        !Scene || !Policy || !PublicRealm || !PlanningGround ||
        !Policy->CurrentSurroundingsRenderOnlyComponent ||
        !Policy->CurrentSurroundingsRenderOnlyComponent->GetStaticMesh())
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_ROSTER: scene=%d policy=%d publicRealm=%d planningGround=%s currentSurroundings=%s."),
            SceneCount,
            PolicyCount,
            PublicRealmCount,
            PlanningGround ? TEXT("valid") : TEXT("null"),
            Policy && Policy->CurrentSurroundingsRenderOnlyComponent &&
                    Policy->CurrentSurroundingsRenderOnlyComponent->GetStaticMesh()
                ? TEXT("valid")
                : TEXT("invalid"));
        return false;
    }
    if (PlanningGround->GetFName() !=
            FName(TEXT("V5COfficialPlanningGroundContextRenderOnlySuccessor")) ||
        PreInheritedPlanningGroundSuppressionBytes !=
            CorrectedPublicRealmMaterialCorrectionBytes ||
        PreInheritedPlanningGroundSuppressionSha256 !=
            CorrectedPublicRealmMaterialCorrectionSha256)
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_PREDECESSOR_IDENTITY: exact R2 material-correction predecessor or inherited planning-ground identity changed.");
        return false;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_DISK_MAP: exact destination file is absent.");
        return false;
    }
    FString DestinationSha256;
    int64 DestinationBytes = INDEX_NONE;
    FString Error;
    if (!HashFileSha256(
            DestinationFilename,
            DestinationSha256,
            DestinationBytes,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_DISK_HASH: ") +
            Error;
        return false;
    }

    const bool bFullyHidden =
        !PlanningGround->IsVisible() && PlanningGround->bHiddenInGame;
    if (bFullyHidden)
    {
        FString ExistingReport;
        const bool bStructurallyValid =
            ValidateHybridWorld(World, ExistingReport,
                ELandmarkPresencePolicy::Forbidden,
                ELandmarkPresencePolicy::Forbidden);
        const bool bFinalHashPinned =
            CorrectedInheritedPlanningGroundSuppressionBytes > 0 &&
            !CorrectedInheritedPlanningGroundSuppressionSha256.IsEmpty();
        const int64 ExistingBytes = DestinationBytes;
        const FString ExistingSha256 = DestinationSha256;
        const bool bExistingHashExact = bFinalHashPinned &&
            ExistingBytes == CorrectedInheritedPlanningGroundSuppressionBytes &&
            ExistingSha256 == CorrectedInheritedPlanningGroundSuppressionSha256;
        if (!bStructurallyValid || !bExistingHashExact)
        {
            OutMessage = FString::Printf(
                TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_CORRECTED_HASH_UNPINNED_OR_DRIFTED: structuralValid=%s finalHashPinned=%s expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s structural={%s}."),
                bStructurallyValid ? TEXT("true") : TEXT("false"),
                bFinalHashPinned ? TEXT("true") : TEXT("false"),
                CorrectedInheritedPlanningGroundSuppressionBytes,
                ExistingBytes,
                *CorrectedInheritedPlanningGroundSuppressionSha256,
                *ExistingSha256,
                *ExistingReport);
            return false;
        }
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_ALREADY_VALID: exact corrected disk hash, clean package, and fully hidden cold structural state are pinned. ") +
            ExistingReport;
        return true;
    }

    if (!PlanningGround->IsVisible() || PlanningGround->bHiddenInGame)
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_PARTIAL_VISIBILITY: predecessor must read visible=true and hiddenInGame=false.");
        return false;
    }
    if (DestinationBytes != PreInheritedPlanningGroundSuppressionBytes ||
        DestinationSha256 != PreInheritedPlanningGroundSuppressionSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_PREDECESSOR_HASH: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
            PreInheritedPlanningGroundSuppressionBytes,
            DestinationBytes,
            *PreInheritedPlanningGroundSuppressionSha256,
            *DestinationSha256);
        return false;
    }

    FString BackupFilename;
    if (!CreateVerifiedPreInheritedPlanningGroundSuppressionMapBackup(
            DestinationFilename,
            BackupFilename,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }
    if (!PlanningGround->IsVisible() || PlanningGround->bHiddenInGame ||
        MapPackage->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_REFUSED_PREDECESSOR_STATE: backup was verified, but the clean predecessor no longer reads visible=true and hiddenInGame=false; map was not changed.");
        return false;
    }

    auto ColdRestoreVerifiedPlanningGroundPredecessor =
        [&](FString& OutRollbackReport) -> bool
    {
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (CurrentPackage &&
            UWorld::RemovePIEPrefix(CurrentPackage->GetName()) ==
                DestinationMapPackage)
        {
            CurrentPackage->SetDirtyFlag(false);
        }
        FString SourceFilename;
        UWorld* RollbackSource =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        if (!RollbackSource)
        {
            UWorld* FailedCurrentWorld = GEditor
                ? GEditor->GetEditorWorldContext().World()
                : nullptr;
            UPackage* FailedCurrentPackage = FailedCurrentWorld
                ? FailedCurrentWorld->GetOutermost()
                : nullptr;
            if (FailedCurrentPackage &&
                UWorld::RemovePIEPrefix(FailedCurrentPackage->GetName()) ==
                    DestinationMapPackage)
            {
                FailedCurrentPackage->SetDirtyFlag(true);
            }
            OutRollbackReport = TEXT("rollback failed to unload the mutated destination through the exact source map; no disk copy was attempted and the verified backup remains authoritative.");
            return false;
        }

        FString BackupSha256;
        int64 BackupBytes = INDEX_NONE;
        FString BackupHashError;
        const bool bBackupStillExact = HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            BackupHashError) &&
            BackupBytes == PreInheritedPlanningGroundSuppressionBytes &&
            BackupSha256 == PreInheritedPlanningGroundSuppressionSha256;
        if (!bBackupStillExact ||
            IFileManager::Get().Copy(
                *DestinationFilename,
                *BackupFilename,
                true,
                true) != COPY_OK)
        {
            OutRollbackReport = FString::Printf(
                TEXT("rollback unloaded the mutated destination but could not restore the exact verified planning-ground predecessor: backupExact=%s bytes=%lld sha256=%s hashError={%s}."),
                bBackupStillExact ? TEXT("true") : TEXT("false"),
                BackupBytes,
                *BackupSha256,
                *BackupHashError);
            return false;
        }

        FString CopiedSha256;
        int64 CopiedBytes = INDEX_NONE;
        FString CopiedHashError;
        const bool bCopiedDiskExact = HashFileSha256(
                DestinationFilename,
                CopiedSha256,
                CopiedBytes,
                CopiedHashError) &&
            CopiedBytes == PreInheritedPlanningGroundSuppressionBytes &&
            CopiedSha256 == PreInheritedPlanningGroundSuppressionSha256;
        if (!bCopiedDiskExact)
        {
            OutRollbackReport = FString::Printf(
                TEXT("rollback copied the verified planning-ground predecessor but destination hash verification failed: bytes=%lld sha256=%s hashError={%s}."),
                CopiedBytes,
                *CopiedSha256,
                *CopiedHashError);
            return false;
        }

        UWorld* RestoredTarget =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredTarget)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage =
            RestoredTarget ? RestoredTarget->GetOutermost() : nullptr;
        int32 RestoredSceneCount = 0;
        int32 RestoredPolicyCount = 0;
        int32 RestoredPublicRealmCount = 0;
        ATRIADIstanaPublicViewSceneActor* RestoredScene = RestoredTarget
            ? FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(
                  RestoredTarget,
                  RestoredSceneCount)
            : nullptr;
        ATRIADIstanaExploreV5DContextPolicyActor* RestoredPolicy =
            RestoredTarget
            ? FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
                  RestoredTarget,
                  RestoredPolicyCount)
            : nullptr;
        ATRIADIstanaExploreV5DPublicRealmActor* RestoredPublicRealm =
            RestoredTarget
            ? FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
                  RestoredTarget,
                  RestoredPublicRealmCount)
            : nullptr;
        UStaticMeshComponent* RestoredPlanningGround = RestoredScene
            ? RestoredScene->V5CGroundContextRenderOnlyComponent
            : nullptr;
        FString RestoredPublicRealmReport;
        const bool bRestoredPublicRealmValid = RestoredPublicRealm &&
            RestoredPublicRealm->ValidatePublicRealm(
                RestoredPublicRealmReport);
        const bool bRestored = RestoredTarget && RestoredPackage &&
            UWorld::RemovePIEPrefix(RestoredPackage->GetName()) ==
                DestinationMapPackage &&
            !RestoredPackage->IsDirty() && RestoredSceneCount == 1 &&
            RestoredPolicyCount == 1 && RestoredPublicRealmCount == 1 &&
            RestoredPolicy && RestoredPlanningGround &&
            RestoredPlanningGround->IsVisible() &&
            !RestoredPlanningGround->bHiddenInGame &&
            bRestoredPublicRealmValid;
        OutRollbackReport = FString::Printf(
            TEXT("cold planning-ground predecessor restore=%s package=%s packageDirty=%s scene=%d policy=%d publicRealm=%d visible=%s hiddenInGame=%s bytes=%lld sha256=%s publicRealm={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            RestoredPackage ? *RestoredPackage->GetName() : TEXT("<null>"),
            RestoredPackage && RestoredPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            RestoredSceneCount,
            RestoredPolicyCount,
            RestoredPublicRealmCount,
            RestoredPlanningGround && RestoredPlanningGround->IsVisible()
                ? TEXT("true")
                : TEXT("false"),
            RestoredPlanningGround && RestoredPlanningGround->bHiddenInGame
                ? TEXT("true")
                : TEXT("false"),
            CopiedBytes,
            *CopiedSha256,
            *RestoredPublicRealmReport);
        return bRestored;
    };

    UStaticMesh* const OriginalMesh = PlanningGround->GetStaticMesh();
    const FTransform OriginalRelativeTransform =
        PlanningGround->GetRelativeTransform();
    const FTransform OriginalWorldTransform =
        PlanningGround->GetComponentTransform();
    const EComponentMobility::Type OriginalMobility =
        PlanningGround->Mobility;
    const ECollisionEnabled::Type OriginalCollision =
        PlanningGround->GetCollisionEnabled();
    const FCollisionResponseContainer OriginalResponses =
        PlanningGround->GetCollisionResponseToChannels();
    const bool bOriginalGenerateOverlaps =
        PlanningGround->GetGenerateOverlapEvents();
    const bool bOriginalCanAffectNavigation =
        PlanningGround->CanEverAffectNavigation();
    const bool bOriginalAutoActivate = PlanningGround->bAutoActivate;
    const bool bOriginalActive = PlanningGround->IsActive();
    const bool bOriginalHiddenInSceneCapture =
        PlanningGround->bHiddenInSceneCapture;
    const bool bOriginalCastShadow = PlanningGround->CastShadow;
    const bool bOriginalCastContactShadow =
        PlanningGround->bCastContactShadow;
    const bool bOriginalRenderInMainPass =
        PlanningGround->bRenderInMainPass;
    const TArray<FName> OriginalTags = PlanningGround->ComponentTags;
    UMaterialInterface* const OriginalOverlay =
        PlanningGround->GetOverlayMaterial();
    TArray<UMaterialInterface*> OriginalMaterials;
    for (int32 Index = 0; Index < PlanningGround->GetNumMaterials(); ++Index)
    {
        OriginalMaterials.Add(PlanningGround->GetMaterial(Index));
    }
    const auto CaptureVisibility = [](const UStaticMeshComponent* Component)
    {
        return TPair<bool, bool>(
            Component && Component->IsVisible(),
            Component && Component->bHiddenInGame);
    };
    const TPair<bool, bool> OriginalCurrentVisibility = CaptureVisibility(
        Policy->CurrentSurroundingsRenderOnlyComponent);
    const TPair<bool, bool> OriginalContextVisibility = CaptureVisibility(
        Scene->ContextBuildingsComponent);
    const TPair<bool, bool> OriginalOsmVisibility = CaptureVisibility(
        Scene->OSMContextBuildingsComponent);
    const TPair<bool, bool> OriginalV5CBuildingVisibility = CaptureVisibility(
        Scene->V5CSurroundingsRenderOnlyComponent);

    Scene->Modify();
    PlanningGround->Modify();
    FString ConfigureReport;
    if (!Policy->SuppressInheritedPlanningGroundPresentation(
            Scene,
            ConfigureReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_CONFIGURATION: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s configure={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *ConfigureReport,
            *RollbackReport);
        return false;
    }
    if (PlanningGround->IsVisible() || !PlanningGround->bHiddenInGame)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_SUCCESSOR_STATE: no map save occurred; expected visible=false and hiddenInGame=true; rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    bool bMaterialsUnchanged =
        PlanningGround->GetNumMaterials() == OriginalMaterials.Num();
    for (int32 Index = 0;
         bMaterialsUnchanged && Index < OriginalMaterials.Num();
         ++Index)
    {
        bMaterialsUnchanged =
            PlanningGround->GetMaterial(Index) == OriginalMaterials[Index];
    }
    const bool bOnlyPlanningGroundVisibilityChanged =
        !PlanningGround->IsVisible() &&
        PlanningGround->bHiddenInGame &&
        PlanningGround->GetStaticMesh() == OriginalMesh &&
        PlanningGround->GetRelativeTransform().Equals(
            OriginalRelativeTransform,
            0.001) &&
        PlanningGround->GetComponentTransform().Equals(
            OriginalWorldTransform,
            0.001) &&
        PlanningGround->Mobility == OriginalMobility &&
        PlanningGround->GetCollisionEnabled() == OriginalCollision &&
        PlanningGround->GetCollisionResponseToChannels() ==
            OriginalResponses &&
        PlanningGround->GetGenerateOverlapEvents() ==
            bOriginalGenerateOverlaps &&
        PlanningGround->CanEverAffectNavigation() ==
            bOriginalCanAffectNavigation &&
        PlanningGround->bAutoActivate == bOriginalAutoActivate &&
        PlanningGround->IsActive() == bOriginalActive &&
        PlanningGround->bHiddenInSceneCapture ==
            bOriginalHiddenInSceneCapture &&
        PlanningGround->CastShadow == bOriginalCastShadow &&
        PlanningGround->bCastContactShadow ==
            bOriginalCastContactShadow &&
        PlanningGround->bRenderInMainPass == bOriginalRenderInMainPass &&
        PlanningGround->ComponentTags == OriginalTags &&
        PlanningGround->GetOverlayMaterial() == OriginalOverlay &&
        bMaterialsUnchanged &&
        CaptureVisibility(Policy->CurrentSurroundingsRenderOnlyComponent) ==
            OriginalCurrentVisibility &&
        CaptureVisibility(Scene->ContextBuildingsComponent) ==
            OriginalContextVisibility &&
        CaptureVisibility(Scene->OSMContextBuildingsComponent) ==
            OriginalOsmVisibility &&
        CaptureVisibility(Scene->V5CSurroundingsRenderOnlyComponent) ==
            OriginalV5CBuildingVisibility;
    if (!bOnlyPlanningGroundVisibilityChanged)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_SCOPE: no map save occurred; only IsVisible true->false and bHiddenInGame false->true were permitted; rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    FString PreSaveReport;
    if (!ValidateHybridWorld(World, PreSaveReport,
            ELandmarkPresencePolicy::Forbidden,
            ELandmarkPresencePolicy::Forbidden))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_PRE_SAVE_VALIDATION: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s cold={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_SAVE: rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* ReloadedSource =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    if (!ReloadedSource)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_COLD_UNLOAD: saved mutation rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }
    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    FString ColdReport;
    UPackage* ReloadedPackage =
        ReloadedTarget ? ReloadedTarget->GetOutermost() : nullptr;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateHybridWorld(ReloadedTarget, ColdReport,
            ELandmarkPresencePolicy::Forbidden,
            ELandmarkPresencePolicy::Forbidden))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_COLD_VALIDATION: saved mutation rollbackSucceeded=%s verifiedBackup=%s cold={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *ColdReport,
            *RollbackReport);
        return false;
    }

    FString FinalSha256;
    int64 FinalBytes = INDEX_NONE;
    FString FinalHashError;
    if (!HashFileSha256(
            DestinationFilename,
            FinalSha256,
            FinalBytes,
            FinalHashError))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_FINAL_HASH: saved mutation rollbackSucceeded=%s verifiedBackup=%s hashError={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *FinalHashError,
            *RollbackReport);
        return false;
    }
    const bool bFinalHashPinned =
        CorrectedInheritedPlanningGroundSuppressionBytes > 0 &&
        !CorrectedInheritedPlanningGroundSuppressionSha256.IsEmpty();
    if (bFinalHashPinned &&
        (FinalBytes != CorrectedInheritedPlanningGroundSuppressionBytes ||
         FinalSha256 != CorrectedInheritedPlanningGroundSuppressionSha256))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            ColdRestoreVerifiedPlanningGroundPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_FAILED_FINAL_HASH_DRIFT: saved mutation rollbackSucceeded=%s expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            CorrectedInheritedPlanningGroundSuppressionBytes,
            FinalBytes,
            *CorrectedInheritedPlanningGroundSuppressionSha256,
            *FinalSha256,
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_INHERITED_PLANNING_GROUND_SUPPRESSION_PASS: exact 154732F3 hash-pinned R2 predecessor was backed up; only the inherited V5C planning-ground component IsVisible and bHiddenInGame readbacks changed; mesh, materials, transforms, activation, collision, navigation, tags, public-realm R2 bindings, provider state, all asset packages, and V5C source behavior remained unchanged; result is clean, cold-reload-validated, and measured for final idempotence pinning; correctedBytes=%lld correctedSha256=%s backup=%s. %s"),
        FinalBytes,
        *FinalSha256,
        *BackupFilename,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DProviderQualityPassToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    FString SuppressedSurroundingsAssetReport;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedAsset(
                SuppressedSurroundingsAssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_SUPPRESSED_SURROUNDINGS_ASSET: map was not changed. ") +
            SuppressedSurroundingsAssetReport;
        return false;
    }
    const FString& SuppressedSurroundingsMeshPath =
        TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            GetLocalFallbackSuppressedMeshObjectPath();
    UStaticMesh* SuppressedSurroundingsMesh = LoadObject<UStaticMesh>(
        nullptr,
        *SuppressedSurroundingsMeshPath);
    if (!SuppressedSurroundingsMesh ||
        SuppressedSurroundingsMesh->GetPathName() !=
            SuppressedSurroundingsMeshPath)
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_SUPPRESSED_SURROUNDINGS_LOAD: exact validated successor mesh could not be loaded; map was not changed.");
        return false;
    }

    FString OuterGroundAssetReport;
    if (!TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
            ValidateAssets(OuterGroundAssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_OUTER_GROUND_ASSET: map was not changed. ") +
            OuterGroundAssetReport;
        return false;
    }
    const FString& OuterGroundMeshPath =
        TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
            GetMeshObjectPath();
    UStaticMesh* OuterGroundMesh = LoadObject<UStaticMesh>(
        nullptr,
        *OuterGroundMeshPath);
    if (!OuterGroundMesh ||
        OuterGroundMesh->GetPathName() != OuterGroundMeshPath)
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_OUTER_GROUND_LOAD: exact validated outer-ground mesh could not be loaded; map was not changed.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage || MapPackage->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_MAP: load the clean exact V5D hybrid destination map first.");
        return false;
    }

    ACesium3DTileset* Tileset = nullptr;
    FString PredecessorReport;
    if (!ValidateProviderQualityMigrationPredecessorWorld(
            World,
            Tileset,
            PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_PREDECESSOR_CONTRACT: ") +
            PredecessorReport;
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
        !Policy->CurrentSurroundingsRenderOnlyComponent ||
        !Policy->OuterGroundLoadingFallbackRenderOnlyComponent)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_POLICY_ROSTER: scene=%d policy=%d exact native fallback components are required; map was not changed."),
            SceneCount,
            PolicyCount);
        return false;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_DISK_MAP: exact destination file is absent.");
        return false;
    }
    FString PredecessorSha256;
    int64 PredecessorBytes = INDEX_NONE;
    FString Error;
    if (!HashFileSha256(
            DestinationFilename,
            PredecessorSha256,
            PredecessorBytes,
            Error) ||
        PredecessorBytes != PreProviderQualityPassBytes ||
        PredecessorSha256 != PreProviderQualityPassSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_PREDECESSOR_HASH: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s. %s"),
            PreProviderQualityPassBytes,
            PredecessorBytes,
            *PreProviderQualityPassSha256,
            *PredecessorSha256,
            *Error);
        return false;
    }

    FString BackupFilename;
    if (!CreateVerifiedPreProviderQualityMapBackup(
            DestinationFilename,
            BackupFilename,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }
    ACesium3DTileset* ReverifiedTileset = nullptr;
    FString ReverifiedPredecessorReport;
    if (!ValidateProviderQualityMigrationPredecessorWorld(
            World,
            ReverifiedTileset,
            ReverifiedPredecessorReport) ||
        ReverifiedTileset != Tileset)
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_REFUSED_POST_BACKUP_DRIFT: verified backup is preserved and the map was not changed. ") +
            ReverifiedPredecessorReport;
        return false;
    }

    const EApplyDpiScaling OriginalApplyDpiScaling =
        Tileset->ApplyDpiScaling;
    const bool bOriginalForbidHoles = Tileset->ForbidHoles;
    const ETilesetSource OriginalTilesetSource = Tileset->GetTilesetSource();
    const int64 OriginalIonAssetId = Tileset->GetIonAssetID();
    const double OriginalMaximumScreenSpaceError =
        Tileset->GetMaximumScreenSpaceError();
    const int64 OriginalMaximumCachedBytes = Tileset->MaximumCachedBytes;
    const int32 OriginalMaximumSimultaneousTileLoads =
        Tileset->MaximumSimultaneousTileLoads;
    const int32 OriginalLoadingDescendantLimit =
        Tileset->LoadingDescendantLimit;
    const bool bOriginalShowCreditsOnScreen = Tileset->ShowCreditsOnScreen;
    const bool bOriginalPreloadAncestors = Tileset->PreloadAncestors;
    const bool bOriginalPreloadSiblings = Tileset->PreloadSiblings;
    const bool bOriginalEnableFrustumCulling = Tileset->EnableFrustumCulling;
    const bool bOriginalEnableFogCulling = Tileset->EnableFogCulling;
    const bool bOriginalEnforceCulledScreenSpaceError =
        Tileset->EnforceCulledScreenSpaceError;
    const double OriginalCulledScreenSpaceError =
        Tileset->CulledScreenSpaceError;
    const bool bOriginalUseLodTransitions = Tileset->GetUseLodTransitions();
    const bool bOriginalCreatePhysicsMeshes =
        Tileset->GetCreatePhysicsMeshes();
    const bool bOriginalCreateNavCollision = Tileset->GetCreateNavCollision();
    const bool bOriginalGenerateSmoothNormals =
        Tileset->GetGenerateSmoothNormals();
    const bool bOriginalIgnoreKhrMaterialsUnlit =
        Tileset->GetIgnoreKhrMaterialsUnlit();
    const TSoftObjectPtr<ACesiumGeoreference> OriginalGeoreference =
        Tileset->GetGeoreference();
    UMaterialInterface* const OriginalMaterial = Tileset->GetMaterial();
    UMaterialInterface* const OriginalTranslucentMaterial =
        Tileset->GetTranslucentMaterial();
    const FTransform OriginalTransform = Tileset->GetActorTransform();
    const TArray<FName> OriginalTags = Tileset->Tags;
    UStaticMesh* const OriginalCurrentSurroundingsMesh =
        Policy->CurrentSurroundingsRenderOnlyComponent->GetStaticMesh();
    UStaticMesh* const OriginalOuterGroundMesh =
        Policy->OuterGroundLoadingFallbackRenderOnlyComponent->GetStaticMesh();

    auto RestoreVerifiedPredecessor =
        [&](FString& OutRollbackReport) -> bool
    {
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (CurrentPackage &&
            UWorld::RemovePIEPrefix(CurrentPackage->GetName()) ==
                DestinationMapPackage)
        {
            CurrentPackage->SetDirtyFlag(false);
        }
        FString SourceFilename;
        UWorld* RollbackUnloadWorld =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        bool bRollbackUsedBlankUnloadWorld = false;
        if (!RollbackUnloadWorld)
        {
            // A failed attempt to load the preferred source map must not make
            // disk rollback depend on repeating the same failed operation.
            // The unsaved blank editor world is a safe, destination-independent
            // unload boundary; the destination package was made clean above.
            RollbackUnloadWorld =
                UEditorLoadingAndSavingUtils::NewBlankMap(false);
            bRollbackUsedBlankUnloadWorld = true;
        }
        UPackage* RollbackUnloadPackage = RollbackUnloadWorld
            ? RollbackUnloadWorld->GetOutermost()
            : nullptr;
        if (!RollbackUnloadWorld || !RollbackUnloadPackage ||
            UWorld::RemovePIEPrefix(RollbackUnloadPackage->GetName()) ==
                DestinationMapPackage)
        {
            OutRollbackReport = TEXT("disk rollback could not establish either the exact source map or a blank destination-independent unload world; the verified backup remains authoritative.");
            return false;
        }

        FString RestoreError;
        if (!RestoreVerifiedPreProviderQualityMapViaSiblingTemp(
                BackupFilename,
                DestinationFilename,
                RestoreError))
        {
            OutRollbackReport = TEXT("disk rollback could not restore the verified predecessor: ") +
                RestoreError;
            return false;
        }
        UWorld* RestoredTarget =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredTarget)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage =
            RestoredTarget ? RestoredTarget->GetOutermost() : nullptr;
        ACesium3DTileset* RestoredTileset = nullptr;
        FString RestoredContract;
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bRestored = RestoredTarget && RestoredPackage &&
            !RestoredPackage->IsDirty() &&
            ValidateProviderQualityMigrationPredecessorWorld(
                RestoredTarget,
                RestoredTileset,
                RestoredContract) &&
            HashFileSha256(
                DestinationFilename,
                RestoredSha256,
                RestoredBytes,
                HashError) &&
            RestoredBytes == PreProviderQualityPassBytes &&
            RestoredSha256 == PreProviderQualityPassSha256;
        OutRollbackReport = FString::Printf(
            TEXT("coldDiskPredecessorRestore=%s blankUnloadFallback=%s packageDirty=%s bytes=%lld sha256=%s contract={%s} hashError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            bRollbackUsedBlankUnloadWorld ? TEXT("true") : TEXT("false"),
            RestoredPackage && RestoredPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            *RestoredContract,
            *HashError);
        return bRestored;
    };

    const auto HasOnlyExpectedVisualQualitySuccessorDelta = [&]()
    {
        FString SuccessorPolicyReport;
        return Tileset->ApplyDpiScaling == EApplyDpiScaling::No &&
            Tileset->ForbidHoles &&
            OriginalApplyDpiScaling == EApplyDpiScaling::UseProjectDefault &&
            !bOriginalForbidHoles &&
            Tileset->GetTilesetSource() == OriginalTilesetSource &&
            Tileset->GetIonAssetID() == OriginalIonAssetId &&
            FMath::IsNearlyEqual(
                Tileset->GetMaximumScreenSpaceError(),
                OriginalMaximumScreenSpaceError,
                0.000001) &&
            Tileset->MaximumCachedBytes == OriginalMaximumCachedBytes &&
            Tileset->MaximumSimultaneousTileLoads ==
                OriginalMaximumSimultaneousTileLoads &&
            Tileset->LoadingDescendantLimit ==
                OriginalLoadingDescendantLimit &&
            OriginalLoadingDescendantLimit == HybridLoadingDescendantLimit &&
            Tileset->ShowCreditsOnScreen == bOriginalShowCreditsOnScreen &&
            Tileset->PreloadAncestors == bOriginalPreloadAncestors &&
            Tileset->PreloadSiblings == bOriginalPreloadSiblings &&
            Tileset->EnableFrustumCulling == bOriginalEnableFrustumCulling &&
            Tileset->EnableFogCulling == bOriginalEnableFogCulling &&
            Tileset->EnforceCulledScreenSpaceError ==
                bOriginalEnforceCulledScreenSpaceError &&
            FMath::IsNearlyEqual(
                Tileset->CulledScreenSpaceError,
                OriginalCulledScreenSpaceError,
                0.000001) &&
            FMath::IsNearlyEqual(
                OriginalCulledScreenSpaceError,
                HybridCulledScreenSpaceError,
                0.000001) &&
            Tileset->GetUseLodTransitions() == bOriginalUseLodTransitions &&
            Tileset->GetCreatePhysicsMeshes() ==
                bOriginalCreatePhysicsMeshes &&
            Tileset->GetCreateNavCollision() == bOriginalCreateNavCollision &&
            Tileset->GetGenerateSmoothNormals() ==
                bOriginalGenerateSmoothNormals &&
            Tileset->GetIgnoreKhrMaterialsUnlit() ==
                bOriginalIgnoreKhrMaterialsUnlit &&
            Tileset->GetGeoreference() == OriginalGeoreference &&
            Tileset->GetMaterial() == OriginalMaterial &&
            Tileset->GetTranslucentMaterial() ==
                OriginalTranslucentMaterial &&
            Tileset->GetActorTransform().Equals(OriginalTransform, 0.001) &&
            Tileset->Tags == OriginalTags &&
            OriginalCurrentSurroundingsMesh &&
            OriginalCurrentSurroundingsMesh->GetPathName() ==
                TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
                    GetSurroundingsMeshObjectPath() &&
            OriginalOuterGroundMesh == nullptr &&
            Policy->CurrentSurroundingsRenderOnlyComponent->GetStaticMesh() ==
                SuppressedSurroundingsMesh &&
            Policy->OuterGroundLoadingFallbackRenderOnlyComponent->
                GetStaticMesh() == OuterGroundMesh &&
            Policy->ValidateCurrentSurroundingsSuccessorForInheritedScene(
                Scene,
                SuccessorPolicyReport);
    };

    Tileset->Modify();
    Policy->Modify();
    Policy->CurrentSurroundingsRenderOnlyComponent->Modify();
    Policy->OuterGroundLoadingFallbackRenderOnlyComponent->Modify();
    Tileset->ApplyDpiScaling = EApplyDpiScaling::No;
    Tileset->ForbidHoles = true;
    FString FallbackConfigurationReport;
    if (!Policy->ConfigureCurrentSurroundingsAndOuterGroundPresentation(
            SuppressedSurroundingsMesh,
            OuterGroundMesh,
            Scene,
            FallbackConfigurationReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_FAILED_FALLBACK_CONFIGURATION: no save occurred; coldRollbackSucceeded=%s verifiedBackup=%s configuration={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *FallbackConfigurationReport,
            *RollbackReport);
        return false;
    }
    if (!HasOnlyExpectedVisualQualitySuccessorDelta())
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_FAILED_SCOPE: no save occurred; only the admitted visual-quality successor delta was permitted; coldRollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    FString PreSaveReport;
    if (!ValidateHybridWorld(
            World,
            PreSaveReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_FAILED_PRE_SAVE_VALIDATION: no save occurred; coldRollbackSucceeded=%s verifiedBackup=%s validation={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_FAILED_SAVE: rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* ReloadedSource =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    bool bColdUnloadUsedBlankWorld = false;
    if (!ReloadedSource)
    {
        ReloadedSource = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        bColdUnloadUsedBlankWorld = true;
    }
    UPackage* ReloadedSourcePackage = ReloadedSource
        ? ReloadedSource->GetOutermost()
        : nullptr;
    if (!ReloadedSource || !ReloadedSourcePackage ||
        UWorld::RemovePIEPrefix(ReloadedSourcePackage->GetName()) ==
            DestinationMapPackage)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_FAILED_COLD_UNLOAD: rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }
    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage =
        ReloadedTarget ? ReloadedTarget->GetOutermost() : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateHybridWorld(
            ReloadedTarget,
            ColdReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_FAILED_COLD_VALIDATION: rollbackSucceeded=%s verifiedBackup=%s cold={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *ColdReport,
            *RollbackReport);
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bSuccessorHasReceipt = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError) &&
        SuccessorBytes > 0 && SuccessorSha256.Len() == 64 &&
        (SuccessorBytes != PreProviderQualityPassBytes ||
         SuccessorSha256 != PreProviderQualityPassSha256);
    const bool bBackupStillExact = HashFileSha256(
        BackupFilename,
        BackupSha256,
        BackupBytes,
        BackupHashError) &&
        BackupBytes == PreProviderQualityPassBytes &&
        BackupSha256 == PreProviderQualityPassSha256;
    if (!bSuccessorHasReceipt || !bBackupStillExact)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_FAILED_FINAL_RECEIPT: successorReceipt=%s backupExact=%s successorBytes=%lld successorSha256=%s backupBytes=%lld backupSha256=%s rollbackSucceeded=%s successorHash={%s} backupHash={%s} rollback={%s}"),
            bSuccessorHasReceipt ? TEXT("true") : TEXT("false"),
            bBackupStillExact ? TEXT("true") : TEXT("false"),
            SuccessorBytes,
            *SuccessorSha256,
            BackupBytes,
            *BackupSha256,
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *SuccessorHashError,
            *BackupHashError,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_PROVIDER_QUALITY_APPLY_PASS: exact hash-pinned predecessor was backed up without overwrite; mutatedProperties=ApplyDpiScaling,ForbidHoles,CurrentSurroundingsStaticMesh,OuterGroundLoadingFallbackStaticMesh applyDpiScaling=false forbidHoles=true loadingDescendantLimit=20 culledSse=8.0 maximumCachedBytesSetting=%lld localFallbackSuppressionV1=true currentContextTriangles=43492 suppressedTriangles=52 exactSuppressedSourceKeys=OSM:way:46521250+OSM:way:1551538490 outerGroundLoadingFallbackTriangles=1280 outerGroundProviderCoupled=true geospatialAnchorUnchanged=true authoredCoreClipUnchanged=true exactTwoLandmarks=true collisionNavigationSensorRfTerrainAuthority=false coldUnloadUsedBlankWorld=%s predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s successorReceiptRequiresPinning=true. %s"),
        HybridCacheBytes,
        bColdUnloadUsedBlankWorld ? TEXT("true") : TEXT("false"),
        PredecessorBytes,
        *PredecessorSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *BackupFilename,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DProviderThrottleSuccessorToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage || MapPackage->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_MAP: load the clean exact V5D hybrid destination map first.");
        return false;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_DISK_MAP: exact destination file is absent.");
        return false;
    }

    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    FString CurrentHashError;
    if (!HashFileSha256(
            DestinationFilename,
            CurrentSha256,
            CurrentBytes,
            CurrentHashError))
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_DISK_HASH: map was not changed. ") +
            CurrentHashError;
        return false;
    }

    const FString BackupFilename =
        GetPreProviderThrottleSuccessorBackupFilename();
    if (CurrentBytes != PreProviderThrottleSuccessorBytes ||
        CurrentSha256 != PreProviderThrottleSuccessorSha256)
    {
        FString BackupSha256;
        int64 BackupBytes = INDEX_NONE;
        FString BackupHashError;
        const bool bBackupIsExact = HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            BackupHashError) &&
            BackupBytes == PreProviderThrottleSuccessorBytes &&
            BackupSha256 == PreProviderThrottleSuccessorSha256;
        FString CurrentContract;
        const bool bCurrentSuccessorIsValid =
            CurrentBytes > 0 && CurrentSha256.Len() == 64 &&
            MapPackage && !MapPackage->IsDirty() &&
            ValidateHybridWorld(
                World,
                CurrentContract,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required);
        if (bBackupIsExact && bCurrentSuccessorIsValid)
        {
            OutMessage = FString::Printf(
                TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_ALREADY_APPLIED: idempotent=true clean=true exactPredecessorBackup=true simultaneousLoads=12 preloadAncestors=true preloadSiblings=false providerReadyGateUnchanged=true collisionNavigationSensorRfTerrainAuthority=false geometryExport=false currentBytes=%lld currentSha256=%s backup=%s. %s"),
                CurrentBytes,
                *CurrentSha256,
                *BackupFilename,
                *CurrentContract);
            return true;
        }
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_PREDECESSOR_HASH: map was not changed; neither the exact predecessor nor a cold-valid successor with its exact predecessor backup is present. expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s backupExact=%s backupBytes=%lld backupSha256=%s currentContract={%s} backupHash={%s}."),
            PreProviderThrottleSuccessorBytes,
            CurrentBytes,
            *PreProviderThrottleSuccessorSha256,
            *CurrentSha256,
            bBackupIsExact ? TEXT("true") : TEXT("false"),
            BackupBytes,
            *BackupSha256,
            *CurrentContract,
            *BackupHashError);
        return false;
    }

    ACesium3DTileset* Tileset = nullptr;
    FString PredecessorReport;
    if (!ValidateProviderThrottleMigrationPredecessorWorld(
            World,
            Tileset,
            PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_PREDECESSOR_CONTRACT: map was not changed. ") +
            PredecessorReport;
        return false;
    }

    FString VerifiedBackupFilename;
    FString Error;
    if (!CreateVerifiedPreProviderThrottleSuccessorMapBackup(
            DestinationFilename,
            VerifiedBackupFilename,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }
    if (VerifiedBackupFilename != BackupFilename)
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_BACKUP_PATH: verified backup path drifted; map was not changed.");
        return false;
    }

    ACesium3DTileset* ReverifiedTileset = nullptr;
    FString ReverifiedPredecessorReport;
    if (!ValidateProviderThrottleMigrationPredecessorWorld(
            World,
            ReverifiedTileset,
            ReverifiedPredecessorReport) ||
        ReverifiedTileset != Tileset)
    {
        OutMessage = TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_REFUSED_POST_BACKUP_DRIFT: verified backup is preserved and the map was not changed. ") +
            ReverifiedPredecessorReport;
        return false;
    }

    const ETilesetSource OriginalTilesetSource = Tileset->GetTilesetSource();
    const int64 OriginalIonAssetId = Tileset->GetIonAssetID();
    const double OriginalMaximumScreenSpaceError =
        Tileset->GetMaximumScreenSpaceError();
    const EApplyDpiScaling OriginalApplyDpiScaling =
        Tileset->ApplyDpiScaling;
    const int64 OriginalMaximumCachedBytes = Tileset->MaximumCachedBytes;
    const int32 OriginalMaximumSimultaneousTileLoads =
        Tileset->MaximumSimultaneousTileLoads;
    const bool bOriginalForbidHoles = Tileset->ForbidHoles;
    const int32 OriginalLoadingDescendantLimit =
        Tileset->LoadingDescendantLimit;
    const bool bOriginalShowCreditsOnScreen = Tileset->ShowCreditsOnScreen;
    const bool bOriginalPreloadAncestors = Tileset->PreloadAncestors;
    const bool bOriginalPreloadSiblings = Tileset->PreloadSiblings;
    const bool bOriginalEnableFrustumCulling = Tileset->EnableFrustumCulling;
    const bool bOriginalEnableFogCulling = Tileset->EnableFogCulling;
    const bool bOriginalEnforceCulledScreenSpaceError =
        Tileset->EnforceCulledScreenSpaceError;
    const double OriginalCulledScreenSpaceError =
        Tileset->CulledScreenSpaceError;
    const bool bOriginalUseLodTransitions = Tileset->GetUseLodTransitions();
    const bool bOriginalCreatePhysicsMeshes =
        Tileset->GetCreatePhysicsMeshes();
    const bool bOriginalCreateNavCollision = Tileset->GetCreateNavCollision();
    const bool bOriginalGenerateSmoothNormals =
        Tileset->GetGenerateSmoothNormals();
    const bool bOriginalIgnoreKhrMaterialsUnlit =
        Tileset->GetIgnoreKhrMaterialsUnlit();
    const TSoftObjectPtr<ACesiumGeoreference> OriginalGeoreference =
        Tileset->GetGeoreference();
    UMaterialInterface* const OriginalMaterial = Tileset->GetMaterial();
    UMaterialInterface* const OriginalTranslucentMaterial =
        Tileset->GetTranslucentMaterial();
    const FTransform OriginalTransform = Tileset->GetActorTransform();
    const TArray<FName> OriginalTags = Tileset->Tags;

    auto RestoreVerifiedPredecessor =
        [&](FString& OutRollbackReport) -> bool
    {
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (CurrentPackage &&
            UWorld::RemovePIEPrefix(CurrentPackage->GetName()) ==
                DestinationMapPackage)
        {
            CurrentPackage->SetDirtyFlag(false);
        }

        FString SourceFilename;
        UWorld* RollbackUnloadWorld =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        bool bRollbackUsedBlankUnloadWorld = false;
        if (!RollbackUnloadWorld)
        {
            RollbackUnloadWorld =
                UEditorLoadingAndSavingUtils::NewBlankMap(false);
            bRollbackUsedBlankUnloadWorld = true;
        }
        UPackage* RollbackUnloadPackage = RollbackUnloadWorld
            ? RollbackUnloadWorld->GetOutermost()
            : nullptr;
        if (!RollbackUnloadWorld || !RollbackUnloadPackage ||
            UWorld::RemovePIEPrefix(RollbackUnloadPackage->GetName()) ==
                DestinationMapPackage)
        {
            OutRollbackReport = TEXT("disk rollback could not establish either the exact source map or a blank destination-independent unload world; the verified backup remains authoritative.");
            return false;
        }

        FString RestoreError;
        if (!RestoreVerifiedPreProviderThrottleSuccessorMapViaSiblingTemp(
                VerifiedBackupFilename,
                DestinationFilename,
                RestoreError))
        {
            OutRollbackReport = TEXT("disk rollback could not restore the verified provider-throttle predecessor: ") +
                RestoreError;
            return false;
        }
        UWorld* RestoredTarget =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredTarget)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage =
            RestoredTarget ? RestoredTarget->GetOutermost() : nullptr;
        ACesium3DTileset* RestoredTileset = nullptr;
        FString RestoredContract;
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bRestored = RestoredTarget && RestoredPackage &&
            !RestoredPackage->IsDirty() &&
            ValidateProviderThrottleMigrationPredecessorWorld(
                RestoredTarget,
                RestoredTileset,
                RestoredContract) &&
            HashFileSha256(
                DestinationFilename,
                RestoredSha256,
                RestoredBytes,
                HashError) &&
            RestoredBytes == PreProviderThrottleSuccessorBytes &&
            RestoredSha256 == PreProviderThrottleSuccessorSha256;
        OutRollbackReport = FString::Printf(
            TEXT("coldDiskPredecessorRestore=%s blankUnloadFallback=%s packageDirty=%s bytes=%lld sha256=%s contract={%s} hashError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            bRollbackUsedBlankUnloadWorld ? TEXT("true") : TEXT("false"),
            RestoredPackage && RestoredPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            *RestoredContract,
            *HashError);
        return bRestored;
    };

    const auto HasOnlyExpectedProviderThrottleSuccessorDelta = [&]()
    {
        return OriginalMaximumSimultaneousTileLoads ==
                PreProviderThrottleSimultaneousLoads &&
            Tileset->MaximumSimultaneousTileLoads ==
                HybridSimultaneousLoads &&
            bOriginalPreloadSiblings && !Tileset->PreloadSiblings &&
            Tileset->GetTilesetSource() == OriginalTilesetSource &&
            Tileset->GetIonAssetID() == OriginalIonAssetId &&
            FMath::IsNearlyEqual(
                Tileset->GetMaximumScreenSpaceError(),
                OriginalMaximumScreenSpaceError,
                0.000001) &&
            Tileset->ApplyDpiScaling == OriginalApplyDpiScaling &&
            Tileset->MaximumCachedBytes == OriginalMaximumCachedBytes &&
            Tileset->ForbidHoles == bOriginalForbidHoles &&
            Tileset->LoadingDescendantLimit ==
                OriginalLoadingDescendantLimit &&
            Tileset->ShowCreditsOnScreen == bOriginalShowCreditsOnScreen &&
            Tileset->PreloadAncestors == bOriginalPreloadAncestors &&
            bOriginalPreloadAncestors &&
            Tileset->EnableFrustumCulling == bOriginalEnableFrustumCulling &&
            Tileset->EnableFogCulling == bOriginalEnableFogCulling &&
            Tileset->EnforceCulledScreenSpaceError ==
                bOriginalEnforceCulledScreenSpaceError &&
            FMath::IsNearlyEqual(
                Tileset->CulledScreenSpaceError,
                OriginalCulledScreenSpaceError,
                0.000001) &&
            Tileset->GetUseLodTransitions() == bOriginalUseLodTransitions &&
            Tileset->GetCreatePhysicsMeshes() ==
                bOriginalCreatePhysicsMeshes &&
            Tileset->GetCreateNavCollision() == bOriginalCreateNavCollision &&
            Tileset->GetGenerateSmoothNormals() ==
                bOriginalGenerateSmoothNormals &&
            Tileset->GetIgnoreKhrMaterialsUnlit() ==
                bOriginalIgnoreKhrMaterialsUnlit &&
            Tileset->GetGeoreference() == OriginalGeoreference &&
            Tileset->GetMaterial() == OriginalMaterial &&
            Tileset->GetTranslucentMaterial() ==
                OriginalTranslucentMaterial &&
            Tileset->GetActorTransform().Equals(OriginalTransform, 0.001) &&
            Tileset->Tags == OriginalTags;
    };

    Tileset->Modify();
    Tileset->MaximumSimultaneousTileLoads = HybridSimultaneousLoads;
    Tileset->PreloadSiblings = false;
    if (!HasOnlyExpectedProviderThrottleSuccessorDelta())
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_FAILED_SCOPE: no save occurred; only MaximumSimultaneousTileLoads 64->12 and PreloadSiblings true->false were permitted; coldRollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *RollbackReport);
        return false;
    }

    FString PreSaveReport;
    if (!ValidateHybridWorld(
            World,
            PreSaveReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_FAILED_PRE_SAVE_VALIDATION: no save occurred; coldRollbackSucceeded=%s verifiedBackup=%s validation={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_FAILED_SAVE: coldRollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* ReloadedSource =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    bool bColdUnloadUsedBlankWorld = false;
    if (!ReloadedSource)
    {
        ReloadedSource = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        bColdUnloadUsedBlankWorld = true;
    }
    UPackage* ReloadedSourcePackage = ReloadedSource
        ? ReloadedSource->GetOutermost()
        : nullptr;
    if (!ReloadedSource || !ReloadedSourcePackage ||
        UWorld::RemovePIEPrefix(ReloadedSourcePackage->GetName()) ==
            DestinationMapPackage)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_FAILED_COLD_UNLOAD: coldRollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *RollbackReport);
        return false;
    }

    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage =
        ReloadedTarget ? ReloadedTarget->GetOutermost() : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateHybridWorld(
            ReloadedTarget,
            ColdReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_FAILED_COLD_VALIDATION: coldRollbackSucceeded=%s verifiedBackup=%s cold={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *ColdReport,
            *RollbackReport);
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bSuccessorHasReceipt = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError) &&
        SuccessorBytes > 0 && SuccessorSha256.Len() == 64 &&
        (SuccessorBytes != PreProviderThrottleSuccessorBytes ||
         SuccessorSha256 != PreProviderThrottleSuccessorSha256);
    const bool bBackupStillExact = HashFileSha256(
        VerifiedBackupFilename,
        BackupSha256,
        BackupBytes,
        BackupHashError) &&
        BackupBytes == PreProviderThrottleSuccessorBytes &&
        BackupSha256 == PreProviderThrottleSuccessorSha256;
    if (!bSuccessorHasReceipt || !bBackupStillExact)
    {
        FString RollbackReport;
        const bool bRollbackSucceeded =
            RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_FAILED_FINAL_RECEIPT: successorReceipt=%s backupExact=%s successorBytes=%lld successorSha256=%s backupBytes=%lld backupSha256=%s coldRollbackSucceeded=%s successorHash={%s} backupHash={%s} rollback={%s}"),
            bSuccessorHasReceipt ? TEXT("true") : TEXT("false"),
            bBackupStillExact ? TEXT("true") : TEXT("false"),
            SuccessorBytes,
            *SuccessorSha256,
            BackupBytes,
            *BackupSha256,
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *SuccessorHashError,
            *BackupHashError,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_PROVIDER_THROTTLE_APPLY_PASS: exact hash-pinned predecessor was backed up without overwrite; oneSave=true mutatedProperties=MaximumSimultaneousTileLoads,PreloadSiblings simultaneousLoads=64->12 preloadSiblings=true->false preloadAncestors=true maximumSse=1.0 maximumCachedBytesSetting=%lld providerReadyGateUnchanged=true providerReadyThresholdPercent=98 providerReadyRequiredConsecutiveSamples=3 geometryExport=false geospatialAnchorUnchanged=true authoredCoreClipUnchanged=true collisionNavigationSensorRfTerrainAuthority=false coldUnloadUsedBlankWorld=%s predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s successorReceiptRequiresPinning=true. %s"),
        HybridCacheBytes,
        bColdUnloadUsedBlankWorld ? TEXT("true") : TEXT("false"),
        CurrentBytes,
        *CurrentSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *VerifiedBackupFilename,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DLocalFallbackSuppressionV2ToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    FString DestinationFilename;
    if (!World || !Package || !World->PersistentLevel ||
        LogicalPackage != DestinationMapPackage || Package->IsDirty() ||
        !FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_MAP_STATE: load the exact clean saved V5D hybrid map first.");
        return false;
    }

    FString V2AssetReport;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedV2Asset(V2AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_ASSET: import and cold-validate the exact V2 asset first. ") +
            V2AssetReport;
        return false;
    }
    UStaticMesh* V2Mesh = LoadObject<UStaticMesh>(
        nullptr,
        *TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            GetLocalFallbackSuppressedV2MeshObjectPath());
    if (!V2Mesh || V2Mesh->GetPathName() !=
            TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
                GetLocalFallbackSuppressedV2MeshObjectPath())
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_ASSET_IDENTITY: exact V2 mesh object path is unavailable.");
        return false;
    }

    const auto ValidateExactSuppressionVersion = [](
        UWorld* Candidate,
        bool bRequireV2,
        FString& OutReport) -> bool
    {
        FString HybridReport;
        if (!ValidateHybridWorld(
                Candidate,
                HybridReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required))
        {
            OutReport = HybridReport;
            return false;
        }
        int32 SceneCount = 0;
        int32 PolicyCount = 0;
        ATRIADIstanaPublicViewSceneActor* Scene =
            FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(
                Candidate, SceneCount);
        ATRIADIstanaExploreV5DContextPolicyActor* Policy =
            FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
                Candidate, PolicyCount);
        FString SuppressionReport;
        const bool bSuppressionValid = SceneCount == 1 && PolicyCount == 1 &&
            Scene && Policy &&
            (bRequireV2
                ? Policy->ValidateCurrentSurroundingsV2SuccessorForInheritedScene(
                      Scene, SuppressionReport)
                : Policy->ValidateCurrentSurroundingsSuccessorForInheritedScene(
                      Scene, SuppressionReport));
        if (!bSuppressionValid)
        {
            OutReport = TEXT("Exact local-fallback suppression version validation failed: ") +
                SuppressionReport + TEXT(" ") + HybridReport;
            return false;
        }
        OutReport = HybridReport + TEXT(" ") + SuppressionReport;
        return true;
    };

    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    FString CurrentHashError;
    if (!HashFileSha256(
            DestinationFilename,
            CurrentSha256,
            CurrentBytes,
            CurrentHashError))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_HASH: ") +
            CurrentHashError;
        return false;
    }

    const FString ExpectedBackupFilename =
        GetPreLocalFallbackSuppressionV2BackupFilename();
    if (CurrentBytes != PreLocalFallbackSuppressionV2Bytes ||
        CurrentSha256 != PreLocalFallbackSuppressionV2Sha256)
    {
        FString BackupSha256;
        int64 BackupBytes = INDEX_NONE;
        FString BackupHashError;
        FString ExistingReport;
        const bool bBackupExact = IFileManager::Get().FileExists(
                *ExpectedBackupFilename) &&
            HashFileSha256(
                ExpectedBackupFilename,
                BackupSha256,
                BackupBytes,
                BackupHashError) &&
            BackupBytes == PreLocalFallbackSuppressionV2Bytes &&
            BackupSha256 == PreLocalFallbackSuppressionV2Sha256;
        if (bBackupExact &&
            ValidateExactSuppressionVersion(World, true, ExistingReport))
        {
            OutMessage = FString::Printf(
                TEXT("IDEMPOTENT_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_ALREADY_VALID: predecessor backup remains exact; current saved map is an exact semantic V2 successor; currentBytes=%lld currentSha256=%s backup=%s. %s"),
                CurrentBytes,
                *CurrentSha256,
                *ExpectedBackupFilename,
                *ExistingReport);
            return true;
        }
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_PREDECESSOR: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s backupExact=%s validation={%s} hash={%s} backupHash={%s}."),
            PreLocalFallbackSuppressionV2Bytes,
            CurrentBytes,
            *PreLocalFallbackSuppressionV2Sha256,
            *CurrentSha256,
            bBackupExact ? TEXT("true") : TEXT("false"),
            *ExistingReport,
            *CurrentHashError,
            *BackupHashError);
        return false;
    }

    FString PredecessorReport;
    if (!ValidateExactSuppressionVersion(World, false, PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_PREDECESSOR_CONTRACT: ") +
            PredecessorReport;
        return false;
    }

    FString VerifiedBackupFilename;
    FString BackupError;
    if (!CreateVerifiedPreLocalFallbackSuppressionV2MapBackup(
            DestinationFilename,
            VerifiedBackupFilename,
            BackupError))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_BACKUP: ") +
            BackupError;
        return false;
    }

    FString RecheckedSha256;
    int64 RecheckedBytes = INDEX_NONE;
    FString RecheckError;
    FString RecheckedContract;
    if (!HashFileSha256(
            DestinationFilename,
            RecheckedSha256,
            RecheckedBytes,
            RecheckError) ||
        RecheckedBytes != PreLocalFallbackSuppressionV2Bytes ||
        RecheckedSha256 != PreLocalFallbackSuppressionV2Sha256 ||
        !ValidateExactSuppressionVersion(World, false, RecheckedContract))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_RECHECK: predecessor changed after backup; no mutation occurred. ") +
            RecheckError + TEXT(" ") + RecheckedContract;
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World, PolicyCount);
    UStaticMeshComponent* Component = Policy
        ? Policy->CurrentSurroundingsRenderOnlyComponent
        : nullptr;
    UStaticMesh* OriginalMesh = Component ? Component->GetStaticMesh() : nullptr;
    if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
        !Component || !OriginalMesh ||
        OriginalMesh->GetPathName() !=
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedCurrentSurroundingsMeshObjectPath())
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_REFUSED_TARGET: predecessor lost its exactly-one scene/policy or exact V1 component mesh; no mutation occurred.");
        return false;
    }

    const auto RestoreVerifiedPredecessor =
        [&](FString& OutRollbackReport) -> bool
    {
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (CurrentPackage &&
            UWorld::RemovePIEPrefix(CurrentPackage->GetName()) ==
                DestinationMapPackage)
        {
            CurrentPackage->SetDirtyFlag(false);
        }
        FString SourceFilename;
        UWorld* UnloadWorld =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        bool bUsedBlankWorld = false;
        if (!UnloadWorld)
        {
            UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
            bUsedBlankWorld = true;
        }
        UPackage* UnloadPackage = UnloadWorld
            ? UnloadWorld->GetOutermost()
            : nullptr;
        if (!UnloadWorld || !UnloadPackage ||
            UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
                DestinationMapPackage)
        {
            OutRollbackReport = TEXT("could not establish a destination-independent unload world; verified backup remains authoritative.");
            return false;
        }
        FString RestoreError;
        if (!RestoreVerifiedPreLocalFallbackSuppressionV2MapViaSiblingTemp(
                VerifiedBackupFilename,
                DestinationFilename,
                RestoreError))
        {
            OutRollbackReport = TEXT("could not restore the verified V2 predecessor: ") +
                RestoreError;
            return false;
        }
        UWorld* RestoredWorld =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredWorld)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage = RestoredWorld
            ? RestoredWorld->GetOutermost()
            : nullptr;
        FString RestoredContract;
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString RestoredHashError;
        const bool bRestored = RestoredWorld && RestoredPackage &&
            !RestoredPackage->IsDirty() &&
            ValidateExactSuppressionVersion(
                RestoredWorld, false, RestoredContract) &&
            HashFileSha256(
                DestinationFilename,
                RestoredSha256,
                RestoredBytes,
                RestoredHashError) &&
            RestoredBytes == PreLocalFallbackSuppressionV2Bytes &&
            RestoredSha256 == PreLocalFallbackSuppressionV2Sha256;
        OutRollbackReport = FString::Printf(
            TEXT("coldDiskPredecessorRestore=%s blankUnloadFallback=%s bytes=%lld sha256=%s contract={%s} hashError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            bUsedBlankWorld ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            *RestoredContract,
            *RestoredHashError);
        return bRestored;
    };

    Component->Modify();
    Component->SetStaticMesh(V2Mesh);
    if (Component->GetStaticMesh() != V2Mesh ||
        V2Mesh->GetPathName() !=
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedCurrentSurroundingsV2MeshObjectPath())
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_FAILED_SCOPE: only the CurrentSurroundingsRenderOnly mesh reference V1->V2 was permitted; coldRollbackSucceeded=%s backup=%s rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *RollbackReport);
        return false;
    }

    FString PreSaveReport;
    if (!ValidateExactSuppressionVersion(World, true, PreSaveReport))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_FAILED_PRE_SAVE_VALIDATION: no save occurred; coldRollbackSucceeded=%s backup=%s validation={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World, DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_FAILED_SAVE: coldRollbackSucceeded=%s backup=%s rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* UnloadWorld =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    bool bColdUnloadUsedBlankWorld = false;
    if (!UnloadWorld)
    {
        UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        bColdUnloadUsedBlankWorld = true;
    }
    UPackage* UnloadPackage = UnloadWorld
        ? UnloadWorld->GetOutermost()
        : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
            DestinationMapPackage)
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_FAILED_COLD_UNLOAD: coldRollbackSucceeded=%s backup=%s rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *RollbackReport);
        return false;
    }

    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedTarget
        ? ReloadedTarget->GetOutermost()
        : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateExactSuppressionVersion(
            ReloadedTarget, true, ColdReport))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_FAILED_COLD_VALIDATION: coldRollbackSucceeded=%s backup=%s cold={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *VerifiedBackupFilename,
            *ColdReport,
            *RollbackReport);
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bSuccessorReceipt = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError) &&
        SuccessorBytes > 0 && SuccessorSha256.Len() == 64 &&
        (SuccessorBytes != PreLocalFallbackSuppressionV2Bytes ||
         SuccessorSha256 != PreLocalFallbackSuppressionV2Sha256);
    const bool bBackupExact = HashFileSha256(
        VerifiedBackupFilename,
        BackupSha256,
        BackupBytes,
        BackupHashError) &&
        BackupBytes == PreLocalFallbackSuppressionV2Bytes &&
        BackupSha256 == PreLocalFallbackSuppressionV2Sha256;
    if (!bSuccessorReceipt || !bBackupExact)
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_FAILED_FINAL_RECEIPT: successorReceipt=%s backupExact=%s successorBytes=%lld successorSha256=%s coldRollbackSucceeded=%s successorHash={%s} backupHash={%s} rollback={%s}"),
            bSuccessorReceipt ? TEXT("true") : TEXT("false"),
            bBackupExact ? TEXT("true") : TEXT("false"),
            SuccessorBytes,
            *SuccessorSha256,
            bRollback ? TEXT("true") : TEXT("false"),
            *SuccessorHashError,
            *BackupHashError,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_APPLY_PASS: exact provider-throttle predecessor was backed up without overwrite; oneSave=true soleMapDelta=CurrentSurroundingsRenderOnly.StaticMesh V1Triangles=43492 V2Triangles=43448 omittedSourceKey=OSM:way:429681826 omittedObjGroup=OSM_way_429681826_P00 additionalSuppressedTriangles=44 totalSuppressedTriangles=96 V1AssetUnchanged=true canonicalAssetUnchanged=true providerSettingsUnchanged=true providerReadyGateUnchanged=true geometryExport=false collisionNavigationSensorRfTerrainAuthority=false coldUnloadUsedBlankWorld=%s predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s successorReceiptRequiresPinning=true. %s"),
        bColdUnloadUsedBlankWorld ? TEXT("true") : TEXT("false"),
        CurrentBytes,
        *CurrentSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *VerifiedBackupFilename,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DMacDonaldHouseR24ToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UStaticMesh* MacDonaldHouseMesh = nullptr;
    FString AssetReport;
    if (!TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::
            LoadValidatedRuntimeMesh(MacDonaldHouseMesh, AssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_ASSETS: map was not changed. ") +
            AssetReport;
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_MAP: load the exact V5D hybrid destination map first.");
        return false;
    }

    int32 ExistingMacDonaldHouseCount = 0;
    FindExactlyOne<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
        World,
        ExistingMacDonaldHouseCount);
    if (ExistingMacDonaldHouseCount == 1 && !MapPackage->IsDirty())
    {
        FString ExistingReport;
        FString ExistingFinalReport;
        if (ValidateHybridWorld(
                World,
                ExistingReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Forbidden) ||
            ValidateHybridWorld(
                World,
                ExistingFinalReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required))
        {
            OutMessage =
                TEXT("IDEMPOTENT_EXPLORE_V5D_R24_MACDONALD_ALREADY_VALID: ") +
                (ExistingFinalReport.IsEmpty()
                    ? ExistingReport
                    : ExistingFinalReport);
            return true;
        }
    }
    if (ExistingMacDonaldHouseCount != 0 || MapPackage->IsDirty())
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_PARTIAL_OR_DIRTY: macDonaldHouse=%d packageDirty=%s."),
            ExistingMacDonaldHouseCount,
            MapPackage->IsDirty() ? TEXT("true") : TEXT("false"));
        return false;
    }

    FString PredecessorReport;
    if (!ValidateHybridWorld(World, PredecessorReport,
            ELandmarkPresencePolicy::Forbidden,
            ELandmarkPresencePolicy::Forbidden))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_PREDECESSOR_CONTRACT: clean map lacks the exact no-landmark V5D contract. ") +
            PredecessorReport;
        return false;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_DISK_MAP: exact destination file is absent.");
        return false;
    }
    FString PredecessorSha256;
    int64 PredecessorBytes = INDEX_NONE;
    FString Error;
    if (!HashFileSha256(
            DestinationFilename,
            PredecessorSha256,
            PredecessorBytes,
            Error) ||
        PredecessorBytes <= 0 || PredecessorSha256.Len() != 64)
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_PREDECESSOR_HASH: ") +
            Error;
        return false;
    }

    FString BackupFilename;
    if (!CreateVerifiedPreMacDonaldHouseR24MapBackup(
            DestinationFilename,
            PredecessorBytes,
            PredecessorSha256,
            BackupFilename,
            Error))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }

    auto RestoreVerifiedPredecessor =
        [&](ATRIADIstanaExploreV5DMacDonaldHouseActor* SpawnedActor,
            bool bRestoreDisk,
            FString& OutRollbackReport) -> bool
    {
        if (IsValid(SpawnedActor))
        {
            SpawnedActor->Destroy();
        }
        if (UWorld* CurrentWorld = GEditor
                ? GEditor->GetEditorWorldContext().World()
                : nullptr)
        {
            if (UPackage* CurrentPackage = CurrentWorld->GetOutermost())
            {
                CurrentPackage->SetDirtyFlag(false);
            }
        }

        bool bDiskRestoreSucceeded = true;
        FString SiblingTempRestoreError;
        if (bRestoreDisk)
        {
            bDiskRestoreSucceeded =
                RestoreVerifiedPreMacDonaldHouseR24MapViaSiblingTemp(
                    BackupFilename,
                    DestinationFilename,
                    PredecessorBytes,
                    PredecessorSha256,
                    SiblingTempRestoreError);
        }

        FString SourceFilename;
        UWorld* ReloadedSource =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        UWorld* RestoredTarget = ReloadedSource
            ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
            : nullptr;
        if (RestoredTarget)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage = RestoredTarget
            ? RestoredTarget->GetOutermost()
            : nullptr;
        int32 RestoredMacDonaldHouseCount = 0;
        if (RestoredTarget)
        {
            FindExactlyOne<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
                RestoredTarget,
                RestoredMacDonaldHouseCount);
        }
        FString RestoredContract;
        const bool bRestoredContract = RestoredTarget &&
            ValidateHybridWorld(RestoredTarget, RestoredContract,
                ELandmarkPresencePolicy::Forbidden,
                ELandmarkPresencePolicy::Forbidden);
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bRestoredDisk = HashFileSha256(
            DestinationFilename,
            RestoredSha256,
            RestoredBytes,
            HashError) &&
            RestoredBytes == PredecessorBytes &&
            RestoredSha256 == PredecessorSha256;
        const bool bRestored = bDiskRestoreSucceeded && ReloadedSource &&
            RestoredTarget && RestoredPackage && !RestoredPackage->IsDirty() &&
            RestoredMacDonaldHouseCount == 0 && bRestoredContract &&
            bRestoredDisk;
        OutRollbackReport = FString::Printf(
            TEXT("cold predecessor restore=%s verifiedSiblingTempReplace=%s backupPreserved=true osAtomicReplaceClaimed=false macDonaldHouse=%d bytes=%lld sha256=%s contract={%s} hashError={%s} siblingTempRestoreError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            bDiskRestoreSucceeded ? TEXT("true") : TEXT("false"),
            RestoredMacDonaldHouseCount,
            RestoredBytes,
            *RestoredSha256,
            *RestoredContract,
            *HashError,
            *SiblingTempRestoreError);
        return bRestored;
    };

    int32 PolicyCount = 0;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    if (PolicyCount != 1 || !Policy)
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_REFUSED_POLICY: semantic predecessor unexpectedly lacks one context policy; verified backup remains at ") +
            BackupFilename;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DR24MacDonaldHouse");
    ATRIADIstanaExploreV5DMacDonaldHouseActor* MacDonaldHouse =
        World->SpawnActor<ATRIADIstanaExploreV5DMacDonaldHouseActor>(
            ATRIADIstanaExploreV5DMacDonaldHouseActor::StaticClass(),
            ATRIADIstanaExploreV5DMacDonaldHouseActor::
                ExpectedPlacementTransform(),
            SpawnParameters);
    FString MacDonaldHouseReport;
    if (!MacDonaldHouse ||
        !MacDonaldHouse->ConfigureMacDonaldHouse(
            MacDonaldHouseMesh,
            Policy->bLocalBuildingFallbackCurrentlyHidden,
            MacDonaldHouseReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            MacDonaldHouse,
            false,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_FAILED_CONFIGURE: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *MacDonaldHouseReport,
            *RollbackReport);
        return false;
    }
    MacDonaldHouse->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R24 MacDonald House - Persistent Render Only"));

    FString PreSaveReport;
    if (!ValidateHybridWorld(World, PreSaveReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Forbidden))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            MacDonaldHouse,
            false,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_FAILED_PRE_SAVE_VALIDATION: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            MacDonaldHouse,
            true,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_FAILED_SAVE: rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bSuccessorHasReceipt = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError);
    const bool bBackupStillExact = HashFileSha256(
        BackupFilename,
        BackupSha256,
        BackupBytes,
        BackupHashError) &&
        BackupBytes == PredecessorBytes &&
        BackupSha256 == PredecessorSha256;
    if (!bSuccessorHasReceipt || !bBackupStillExact ||
        (SuccessorBytes == PredecessorBytes &&
         SuccessorSha256 == PredecessorSha256))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            nullptr,
            true,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_FAILED_POST_SAVE_RECEIPT: rollbackSucceeded=%s successorBytes=%lld successorSha256=%s backupStillExact=%s verifiedBackup=%s successorHashError={%s} backupHashError={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            SuccessorBytes,
            *SuccessorSha256,
            bBackupStillExact ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *SuccessorHashError,
            *BackupHashError,
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* ReloadedSource =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    UWorld* ReloadedTarget = ReloadedSource
        ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
        : nullptr;
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedTarget
        ? ReloadedTarget->GetOutermost()
        : nullptr;
    FString ColdReport;
    if (!ReloadedSource || !ReloadedTarget || !ReloadedPackage ||
        ReloadedPackage->IsDirty() ||
        !ValidateHybridWorld(ReloadedTarget, ColdReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Forbidden))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            nullptr,
            true,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_FAILED_COLD_VALIDATION: rollbackSucceeded=%s verifiedBackup=%s cold={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *ColdReport,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_R24_MACDONALD_APPLY_PASS: a clean semantically exact no-landmark V5D predecessor was receipted and backed up without overwrite; one exact persistent render-only actor was placed from volunteered OSM way 46521250 and cold-reload-validated; predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s outsideAuthoredCoreClip=true existingProviderClipUnchanged=true dedicatedProviderExclusion=false providerStateTelemetryOnly=true visibilityInvariantAcrossProviderTransitions=true coarseShellRetained=true providerReadyLiveSuccessor=false collisionNavigationSensorRfAuthority=false surveyAsBuiltOneToOne=false materialsCalibrated=false. %s"),
        PredecessorBytes,
        *PredecessorSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *BackupFilename,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UStaticMesh* TemasekShophouseMesh = nullptr;
    FString AssetReport;
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::
            LoadValidatedRuntimeMesh(TemasekShophouseMesh, AssetReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_ASSETS: map was not changed. ") +
            AssetReport;
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = MapPackage
        ? UWorld::RemovePIEPrefix(MapPackage->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !MapPackage ||
        LogicalPackage != DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_MAP: load the exact V5D hybrid destination map first.");
        return false;
    }

    int32 ExistingTemasekShophouseCount = 0;
    ATRIADIstanaExploreV5DTemasekShophouseActor* ExistingTemasekShophouse =
        FindExactlyOne<ATRIADIstanaExploreV5DTemasekShophouseActor>(
        World,
        ExistingTemasekShophouseCount);
    if (ExistingTemasekShophouseCount == 1 && !MapPackage->IsDirty())
    {
        FString ExistingReport;
        FString ExistingOverlayReport;
        if (ValidateHybridWorld(World, ExistingReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required) &&
            ExistingTemasekShophouse &&
            ValidateTemasekShophouseOverlay(
                World,
                true,
                ExistingOverlayReport))
        {
            OutMessage =
                TEXT("IDEMPOTENT_EXPLORE_V5D_R24_TEMASEK_ALREADY_VALID: ") +
                ExistingReport + TEXT(" ") + ExistingOverlayReport;
            return true;
        }
    }
    if (ExistingTemasekShophouseCount != 0 || MapPackage->IsDirty())
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_PARTIAL_OR_DIRTY: temasekShophouse=%d packageDirty=%s."),
            ExistingTemasekShophouseCount,
            MapPackage->IsDirty() ? TEXT("true") : TEXT("false"));
        return false;
    }

    FString PredecessorReport;
    FString PredecessorOverlayReport;
    if (!ValidateHybridWorld(World, PredecessorReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Forbidden) ||
        !ValidateTemasekShophouseOverlay(
            World,
            false,
            PredecessorOverlayReport))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_PREDECESSOR_CONTRACT: clean map must satisfy the exactly-one-MacDonald/no-Temasek predecessor contract. ") +
            PredecessorReport + TEXT(" ") + PredecessorOverlayReport;
        return false;
    }

    FString DestinationFilename;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_DISK_MAP: exact destination file is absent.");
        return false;
    }
    FString PredecessorSha256;
    int64 PredecessorBytes = INDEX_NONE;
    FString Error;
    if (!HashFileSha256(
            DestinationFilename,
            PredecessorSha256,
            PredecessorBytes,
            Error) ||
        PredecessorBytes <= 0 || PredecessorSha256.Len() != 64)
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_PREDECESSOR_HASH: ") +
            Error;
        return false;
    }

    FString BackupFilename;
    if (!CreateVerifiedPreTemasekShophouseR24MapBackup(
            DestinationFilename,
            PredecessorBytes,
            PredecessorSha256,
            BackupFilename,
            Error))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }

    auto RestoreVerifiedPredecessor =
        [&](ATRIADIstanaExploreV5DTemasekShophouseActor* SpawnedActor,
            bool bRestoreDisk,
            FString& OutRollbackReport) -> bool
    {
        if (IsValid(SpawnedActor))
        {
            SpawnedActor->Destroy();
        }
        if (UWorld* CurrentWorld = GEditor
                ? GEditor->GetEditorWorldContext().World()
                : nullptr)
        {
            if (UPackage* CurrentPackage = CurrentWorld->GetOutermost())
            {
                CurrentPackage->SetDirtyFlag(false);
            }
        }

        bool bDiskRestoreSucceeded = true;
        FString SiblingTempRestoreError;
        if (bRestoreDisk)
        {
            bDiskRestoreSucceeded =
                RestoreVerifiedPreTemasekShophouseR24MapViaSiblingTemp(
                    BackupFilename,
                    DestinationFilename,
                    PredecessorBytes,
                    PredecessorSha256,
                    SiblingTempRestoreError);
        }

        FString SourceFilename;
        UWorld* ReloadedSource =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        UWorld* RestoredTarget = ReloadedSource
            ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
            : nullptr;
        if (RestoredTarget)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage = RestoredTarget
            ? RestoredTarget->GetOutermost()
            : nullptr;
        int32 RestoredTemasekShophouseCount = 0;
        if (RestoredTarget)
        {
            FindExactlyOne<ATRIADIstanaExploreV5DTemasekShophouseActor>(
                RestoredTarget,
                RestoredTemasekShophouseCount);
        }
        FString RestoredContract;
        FString RestoredOverlayContract;
        const bool bRestoredContract = RestoredTarget &&
            ValidateHybridWorld(RestoredTarget, RestoredContract,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Forbidden) &&
            ValidateTemasekShophouseOverlay(
                RestoredTarget,
                false,
                RestoredOverlayContract);
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bRestoredDisk = HashFileSha256(
            DestinationFilename,
            RestoredSha256,
            RestoredBytes,
            HashError) &&
            RestoredBytes == PredecessorBytes &&
            RestoredSha256 == PredecessorSha256;
        const bool bRestored = bDiskRestoreSucceeded && ReloadedSource &&
            RestoredTarget && RestoredPackage && !RestoredPackage->IsDirty() &&
            RestoredTemasekShophouseCount == 0 && bRestoredContract &&
            bRestoredDisk;
        OutRollbackReport = FString::Printf(
            TEXT("cold predecessor restore=%s verifiedSiblingTempReplace=%s backupPreserved=true osAtomicReplaceClaimed=false temasekShophouse=%d bytes=%lld sha256=%s contract={%s} overlay={%s} hashError={%s} siblingTempRestoreError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            bDiskRestoreSucceeded ? TEXT("true") : TEXT("false"),
            RestoredTemasekShophouseCount,
            RestoredBytes,
            *RestoredSha256,
            *RestoredContract,
            *RestoredOverlayContract,
            *HashError,
            *SiblingTempRestoreError);
        return bRestored;
    };

    int32 PolicyCount = 0;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    if (PolicyCount != 1 || !Policy)
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_REFUSED_POLICY: semantic predecessor unexpectedly lacks one context policy; verified backup remains at ") +
            BackupFilename;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DR24TemasekShophouse");
    ATRIADIstanaExploreV5DTemasekShophouseActor* TemasekShophouse =
        World->SpawnActor<ATRIADIstanaExploreV5DTemasekShophouseActor>(
            ATRIADIstanaExploreV5DTemasekShophouseActor::StaticClass(),
            ATRIADIstanaExploreV5DTemasekShophouseActor::
                ExpectedPlacementTransform(),
            SpawnParameters);
    FString TemasekShophouseReport;
    if (!TemasekShophouse ||
        !TemasekShophouse->ConfigureTemasekShophouse(
            TemasekShophouseMesh,
            Policy->bLocalBuildingFallbackCurrentlyHidden,
            TemasekShophouseReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            TemasekShophouse,
            false,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_FAILED_CONFIGURE: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *TemasekShophouseReport,
            *RollbackReport);
        return false;
    }
    TemasekShophouse->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R24 Temasek Shophouse - Persistent Render Only"));

    FString PreSaveReport;
    FString PreSaveOverlayReport;
    if (!ValidateHybridWorld(World, PreSaveReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required) ||
        !ValidateTemasekShophouseOverlay(
            World,
            true,
            PreSaveOverlayReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            TemasekShophouse,
            false,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_FAILED_PRE_SAVE_VALIDATION: no map save occurred; rollbackSucceeded=%s verifiedBackup=%s. %s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *(PreSaveReport + TEXT(" ") + PreSaveOverlayReport),
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            TemasekShophouse,
            true,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_FAILED_SAVE: rollbackSucceeded=%s verifiedBackup=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *RollbackReport);
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bSuccessorHasReceipt = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError);
    const bool bBackupStillExact = HashFileSha256(
        BackupFilename,
        BackupSha256,
        BackupBytes,
        BackupHashError) &&
        BackupBytes == PredecessorBytes &&
        BackupSha256 == PredecessorSha256;
    if (!bSuccessorHasReceipt || !bBackupStillExact ||
        (SuccessorBytes == PredecessorBytes &&
         SuccessorSha256 == PredecessorSha256))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            nullptr,
            true,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_FAILED_POST_SAVE_RECEIPT: rollbackSucceeded=%s successorBytes=%lld successorSha256=%s backupStillExact=%s verifiedBackup=%s successorHashError={%s} backupHashError={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            SuccessorBytes,
            *SuccessorSha256,
            bBackupStillExact ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *SuccessorHashError,
            *BackupHashError,
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* ReloadedSource =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    UWorld* ReloadedTarget = ReloadedSource
        ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
        : nullptr;
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedTarget
        ? ReloadedTarget->GetOutermost()
        : nullptr;
    FString ColdReport;
    FString ColdOverlayReport;
    if (!ReloadedSource || !ReloadedTarget || !ReloadedPackage ||
        ReloadedPackage->IsDirty() ||
        !ValidateHybridWorld(ReloadedTarget, ColdReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required) ||
        !ValidateTemasekShophouseOverlay(
            ReloadedTarget,
            true,
            ColdOverlayReport))
    {
        FString RollbackReport;
        const bool bRollbackSucceeded = RestoreVerifiedPredecessor(
            nullptr,
            true,
            RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_FAILED_COLD_VALIDATION: rollbackSucceeded=%s verifiedBackup=%s cold={%s} rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *BackupFilename,
            *(ColdReport + TEXT(" ") + ColdOverlayReport),
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_R24_TEMASEK_APPLY_PASS: a clean exactly-one-MacDonald/no-Temasek predecessor was receipted and backed up without overwrite; one exact persistent render-only Temasek actor was placed from volunteered OSM way 1551538490 and cold-reload-validated; predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s providerAndCoarseShellOverlapUnresolved=true existingProviderClipUnchanged=true dedicatedProviderExclusion=false providerStateTelemetryOnly=true visibilityInvariantAcrossProviderTransitions=true coarseShellRetained=true providerReadyLiveSuccessor=false collisionNavigationSensorRfAuthority=false surveyAsBuiltOneToOne=false materialsCalibrated=false. %s"),
        PredecessorBytes,
        *PredecessorSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *BackupFilename,
        *(ColdReport + TEXT(" ") + ColdOverlayReport));
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    FString AssetReport;
    if (!TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
            AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_ASSETS: import and cold-validate the exact five additive materials first. ") +
            AssetReport;
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    FString DestinationFilename;
    if (!World || !Package || !World->PersistentLevel ||
        LogicalPackage != DestinationMapPackage || Package->IsDirty() ||
        !FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_MAP_STATE: load the exact clean saved V5D hybrid map first.");
        return false;
    }

    const auto ResolvePolicyAndScene = [](
        UWorld* Candidate,
        ATRIADIstanaPublicViewSceneActor*& OutScene,
        ATRIADIstanaExploreV5DContextPolicyActor*& OutPolicy,
        FString& OutReport) -> bool
    {
        int32 SceneCount = 0;
        int32 PolicyCount = 0;
        OutScene = FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(
            Candidate, SceneCount);
        OutPolicy = FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            Candidate, PolicyCount);
        if (SceneCount != 1 || PolicyCount != 1 || !OutScene || !OutPolicy)
        {
            OutReport = FString::Printf(
                TEXT("R25 context-facade world requires exactly one scene and policy; scene=%d policy=%d."),
                SceneCount,
                PolicyCount);
            return false;
        }
        OutReport.Reset();
        return true;
    };

    const auto ValidateExactR25World = [&ResolvePolicyAndScene](
        UWorld* Candidate,
        FString& OutReport) -> bool
    {
        FString HybridReport;
        if (!ValidateHybridWorld(
                Candidate,
                HybridReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required))
        {
            OutReport = HybridReport;
            return false;
        }
        ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
        ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
        FString ResolveReport;
        FString R25Report;
        if (!ResolvePolicyAndScene(
                Candidate, Scene, Policy, ResolveReport) ||
            !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
                Scene, R25Report))
        {
            OutReport = ResolveReport + TEXT(" ") + R25Report + TEXT(" ") +
                HybridReport;
            return false;
        }
        OutReport = HybridReport + TEXT(" ") + R25Report;
        return true;
    };

    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    FString HashError;
    if (!HashFileSha256(
            DestinationFilename,
            CurrentSha256,
            CurrentBytes,
            HashError))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_HASH: ") +
            HashError;
        return false;
    }

    const FString ExpectedBackupFilename =
        GetPreContextFacadeR25BackupFilename();
    if (CurrentBytes != PreContextFacadeR25Bytes ||
        CurrentSha256 != PreContextFacadeR25Sha256)
    {
        FString BackupSha256;
        int64 BackupBytes = INDEX_NONE;
        FString BackupHashError;
        FString ExistingReport;
        const bool bBackupExact = IFileManager::Get().FileExists(
                *ExpectedBackupFilename) &&
            HashFileSha256(
                ExpectedBackupFilename,
                BackupSha256,
                BackupBytes,
                BackupHashError) &&
            BackupBytes == PreContextFacadeR25Bytes &&
            BackupSha256 == PreContextFacadeR25Sha256;
        if (bBackupExact &&
            ValidateExactR25World(World, ExistingReport))
        {
            OutMessage = FString::Printf(
                TEXT("IDEMPOTENT_EXPLORE_V5D_CONTEXT_FACADE_R25_ALREADY_VALID: predecessor backup remains exact; current clean map has the exact V2 mesh and 17-entry R25 override roster; currentBytes=%lld currentSha256=%s backup=%s. %s"),
                CurrentBytes,
                *CurrentSha256,
                *ExpectedBackupFilename,
                *ExistingReport);
            return true;
        }
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_PREDECESSOR: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s backupExact=%s validation={%s} hash={%s} backupHash={%s}."),
            PreContextFacadeR25Bytes,
            CurrentBytes,
            *PreContextFacadeR25Sha256,
            *CurrentSha256,
            bBackupExact ? TEXT("true") : TEXT("false"),
            *ExistingReport,
            *HashError,
            *BackupHashError);
        return false;
    }

    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    FString ResolveReport;
    if (!ResolvePolicyAndScene(World, Scene, Policy, ResolveReport) ||
        !Policy->CurrentSurroundingsRenderOnlyComponent ||
        Policy->CurrentSurroundingsRenderOnlyComponent->
            GetNumOverrideMaterials() != 0)
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_TARGET: predecessor must have exactly one scene/policy and zero component overrides. ") +
            ResolveReport;
        return false;
    }
    FString PredecessorHybridReport;
    FString PredecessorV2Report;
    if (!ValidateHybridWorld(
            World,
            PredecessorHybridReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required) ||
        !Policy->ValidateCurrentSurroundingsV2SuccessorForInheritedScene(
            Scene, PredecessorV2Report))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_PREDECESSOR_CONTRACT: ") +
            PredecessorHybridReport + TEXT(" ") + PredecessorV2Report;
        return false;
    }

    FString VerifiedBackupFilename;
    FString BackupError;
    if (!CreateVerifiedPreContextFacadeR25MapBackup(
            DestinationFilename,
            VerifiedBackupFilename,
            BackupError))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_BACKUP: ") +
            BackupError;
        return false;
    }

    FString RecheckedSha256;
    int64 RecheckedBytes = INDEX_NONE;
    FString RecheckError;
    if (!HashFileSha256(
            DestinationFilename,
            RecheckedSha256,
            RecheckedBytes,
            RecheckError) ||
        RecheckedBytes != PreContextFacadeR25Bytes ||
        RecheckedSha256 != PreContextFacadeR25Sha256 ||
        Package->IsDirty() ||
        Policy->CurrentSurroundingsRenderOnlyComponent->
            GetNumOverrideMaterials() != 0)
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_REFUSED_RECHECK: predecessor changed after backup; no mutation occurred. ") +
            RecheckError;
        return false;
    }

    UStaticMeshComponent* Component =
        Policy->CurrentSurroundingsRenderOnlyComponent;
    UStaticMesh* const OriginalMesh = Component->GetStaticMesh();
    const FTransform OriginalRelativeTransform =
        Component->GetRelativeTransform();
    UStaticMesh* const OriginalOuterMesh =
        Policy->OuterGroundLoadingFallbackRenderOnlyComponent
        ? Policy->OuterGroundLoadingFallbackRenderOnlyComponent->GetStaticMesh()
        : nullptr;

    const auto RestoreVerifiedPredecessor =
        [&](FString& OutRollbackReport) -> bool
    {
        if (UWorld* CurrentWorld = GEditor
                ? GEditor->GetEditorWorldContext().World()
                : nullptr)
        {
            if (UPackage* CurrentPackage = CurrentWorld->GetOutermost())
            {
                CurrentPackage->SetDirtyFlag(false);
            }
        }
        FString SourceFilename;
        UWorld* UnloadWorld =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        bool bUsedBlankWorld = false;
        if (!UnloadWorld)
        {
            UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
            bUsedBlankWorld = true;
        }
        UPackage* UnloadPackage = UnloadWorld
            ? UnloadWorld->GetOutermost()
            : nullptr;
        if (!UnloadWorld || !UnloadPackage ||
            UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
                DestinationMapPackage)
        {
            OutRollbackReport = TEXT("could not establish a destination-independent unload world; verified R25 backup remains authoritative.");
            return false;
        }
        FString RestoreError;
        if (!RestoreVerifiedPreContextFacadeR25MapViaSiblingTemp(
                VerifiedBackupFilename,
                DestinationFilename,
                RestoreError))
        {
            OutRollbackReport = TEXT("could not restore the verified pre-R25 map: ") +
                RestoreError;
            return false;
        }
        UWorld* RestoredWorld =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredWorld)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage = RestoredWorld
            ? RestoredWorld->GetOutermost()
            : nullptr;
        ATRIADIstanaPublicViewSceneActor* RestoredScene = nullptr;
        ATRIADIstanaExploreV5DContextPolicyActor* RestoredPolicy = nullptr;
        FString RestoredResolveReport;
        FString RestoredHybridReport;
        FString RestoredV2Report;
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString RestoredHashError;
        const bool bRestored = RestoredWorld && RestoredPackage &&
            !RestoredPackage->IsDirty() &&
            ResolvePolicyAndScene(
                RestoredWorld,
                RestoredScene,
                RestoredPolicy,
                RestoredResolveReport) &&
            RestoredPolicy->CurrentSurroundingsRenderOnlyComponent &&
            RestoredPolicy->CurrentSurroundingsRenderOnlyComponent->
                GetNumOverrideMaterials() == 0 &&
            ValidateHybridWorld(
                RestoredWorld,
                RestoredHybridReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required) &&
            RestoredPolicy->ValidateCurrentSurroundingsV2SuccessorForInheritedScene(
                RestoredScene, RestoredV2Report) &&
            HashFileSha256(
                DestinationFilename,
                RestoredSha256,
                RestoredBytes,
                RestoredHashError) &&
            RestoredBytes == PreContextFacadeR25Bytes &&
            RestoredSha256 == PreContextFacadeR25Sha256;
        OutRollbackReport = FString::Printf(
            TEXT("coldDiskPredecessorRestore=%s blankUnloadFallback=%s bytes=%lld sha256=%s resolve={%s} hybrid={%s} v2={%s} hash={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            bUsedBlankWorld ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            *RestoredResolveReport,
            *RestoredHybridReport,
            *RestoredV2Report,
            *RestoredHashError);
        return bRestored;
    };

    FString ApplyReport;
    if (!Policy->ApplyCurrentSurroundingsContextFacadeR25(ApplyReport) ||
        Component->GetStaticMesh() != OriginalMesh ||
        !Component->GetRelativeTransform().Equals(
            OriginalRelativeTransform, 0.001) ||
        !Policy->OuterGroundLoadingFallbackRenderOnlyComponent ||
        Policy->OuterGroundLoadingFallbackRenderOnlyComponent->GetStaticMesh() !=
            OriginalOuterMesh)
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_FAILED_SCOPE: only the exact 17 current-surroundings material overrides were permitted; coldRollbackSucceeded=%s apply={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *ApplyReport,
            *RollbackReport);
        return false;
    }

    FString PreSaveReport;
    if (!ValidateExactR25World(World, PreSaveReport))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_FAILED_PRE_SAVE_VALIDATION: no save occurred; coldRollbackSucceeded=%s validation={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World, DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_FAILED_SAVE: coldRollbackSucceeded=%s rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* UnloadWorld =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    bool bColdUnloadUsedBlankWorld = false;
    if (!UnloadWorld)
    {
        UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        bColdUnloadUsedBlankWorld = true;
    }
    UPackage* UnloadPackage = UnloadWorld
        ? UnloadWorld->GetOutermost()
        : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
            DestinationMapPackage)
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_FAILED_COLD_UNLOAD: coldRollbackSucceeded=%s rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *RollbackReport);
        return false;
    }

    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedTarget
        ? ReloadedTarget->GetOutermost()
        : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateExactR25World(ReloadedTarget, ColdReport))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_FAILED_COLD_VALIDATION: coldRollbackSucceeded=%s cold={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *ColdReport,
            *RollbackReport);
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bSuccessorReceipt = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError) &&
        SuccessorBytes > 0 && SuccessorSha256.Len() == 64 &&
        (SuccessorBytes != PreContextFacadeR25Bytes ||
         SuccessorSha256 != PreContextFacadeR25Sha256);
    const bool bBackupExact = HashFileSha256(
        VerifiedBackupFilename,
        BackupSha256,
        BackupBytes,
        BackupHashError) &&
        BackupBytes == PreContextFacadeR25Bytes &&
        BackupSha256 == PreContextFacadeR25Sha256;
    if (!bSuccessorReceipt || !bBackupExact)
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_FAILED_FINAL_RECEIPT: successorReceipt=%s backupExact=%s successorBytes=%lld successorSha256=%s coldRollbackSucceeded=%s successorHash={%s} backupHash={%s} rollback={%s}"),
            bSuccessorReceipt ? TEXT("true") : TEXT("false"),
            bBackupExact ? TEXT("true") : TEXT("false"),
            SuccessorBytes,
            *SuccessorSha256,
            bRollback ? TEXT("true") : TEXT("false"),
            *SuccessorHashError,
            *BackupHashError,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_PASS: exact R24B predecessor was backed up without overwrite; oneSave=true soleMapDelta=CurrentSurroundingsRenderOnly.OverrideMaterials materialOverrides=17 V2MeshUnchanged=true V2Triangles=43448 landmarksUnchanged=true providerSettingsUnchanged=true providerReadyGateUnchanged=true geometryExport=false collisionNavigationSensorRfAuthority=false sharedV5CAssetsMutated=false coldUnloadUsedBlankWorld=%s predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s successorReceiptRequiresPinning=true. %s"),
        bColdUnloadUsedBlankWorld ? TEXT("true") : TEXT("false"),
        CurrentBytes,
        *CurrentSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *VerifiedBackupFilename,
        *ColdReport);
    return true;
}


bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap(
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    FString ReusableAssetReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateReusableLandmarkVegetationAssets(ReusableAssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_ASSETS: all 14 exact reusable assets and the deterministic layout must validate before map mutation. ") +
            ReusableAssetReport;
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    FString DestinationFilename;
    if (!World || !World->PersistentLevel || !Package ||
        LogicalPackage != DestinationMapPackage || Package->IsDirty() ||
        !FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_MAP_STATE: load the exact clean saved V5D hybrid map first.");
        return false;
    }

    const auto ValidateR25WorldWithVegetationPolicy = [](
        UWorld* Candidate,
        ELandmarkPresencePolicy VegetationPolicy,
        FString& OutReport) -> bool
    {
        FString HybridReport;
        if (!ValidateHybridWorld(
                Candidate,
                HybridReport,
                ELandmarkPresencePolicy::Required,
                ELandmarkPresencePolicy::Required,
                VegetationPolicy))
        {
            OutReport = HybridReport;
            return false;
        }

        int32 SceneCount = 0;
        int32 PolicyCount = 0;
        ATRIADIstanaPublicViewSceneActor* Scene =
            FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(
                Candidate, SceneCount);
        ATRIADIstanaExploreV5DContextPolicyActor* Policy =
            FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
                Candidate, PolicyCount);
        FString R25Report;
        if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
            !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
                Scene, R25Report))
        {
            OutReport = FString::Printf(
                TEXT("R26 landmark-vegetation world did not retain the exact R25 facade contract; scene=%d policy=%d r25={%s} hybrid={%s}"),
                SceneCount,
                PolicyCount,
                *R25Report,
                *HybridReport);
            return false;
        }
        OutReport = R25Report + TEXT(" ") + HybridReport;
        return true;
    };

    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    FString HashError;
    if (!HashFileSha256(
            DestinationFilename,
            CurrentSha256,
            CurrentBytes,
            HashError))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_HASH: ") +
            HashError;
        return false;
    }

    int32 ExistingVegetationCount = 0;
    FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
        World, ExistingVegetationCount);
    const FString ExpectedBackupFilename =
        GetPreLandmarkVegetationR26BackupFilename();
    if (CurrentBytes != PreLandmarkVegetationR26Bytes ||
        CurrentSha256 != PreLandmarkVegetationR26Sha256)
    {
        FString BackupSha256;
        int64 BackupBytes = INDEX_NONE;
        FString BackupHashError;
        FString ExistingReport;
        const bool bBackupExact = IFileManager::Get().FileExists(
                *ExpectedBackupFilename) &&
            HashFileSha256(
                ExpectedBackupFilename,
                BackupSha256,
                BackupBytes,
                BackupHashError) &&
            BackupBytes == PreLandmarkVegetationR26Bytes &&
            BackupSha256 == PreLandmarkVegetationR26Sha256;
        if (ExistingVegetationCount == 1 && !Package->IsDirty() &&
            bBackupExact &&
            ValidateR25WorldWithVegetationPolicy(
                World,
                ELandmarkPresencePolicy::Required,
                ExistingReport))
        {
            OutMessage = FString::Printf(
                TEXT("IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R26_ALREADY_VALID: predecessor backup remains exact; current clean map contains exactly one validated deterministic render-only vegetation actor and retains R25; landmarkVegetationR26=true contextFacadeR25=true currentBytes=%lld currentSha256=%s backup=%s. %s"),
                CurrentBytes,
                *CurrentSha256,
                *ExpectedBackupFilename,
                *ExistingReport);
            return true;
        }
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_PREDECESSOR: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s existingVegetation=%d backupExact=%s validation={%s} hash={%s} backupHash={%s}."),
            PreLandmarkVegetationR26Bytes,
            CurrentBytes,
            *PreLandmarkVegetationR26Sha256,
            *CurrentSha256,
            ExistingVegetationCount,
            bBackupExact ? TEXT("true") : TEXT("false"),
            *ExistingReport,
            *HashError,
            *BackupHashError);
        return false;
    }

    if (ExistingVegetationCount != 0 || Package->IsDirty())
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_PARTIAL_OR_DIRTY: landmarkVegetation=%d packageDirty=%s."),
            ExistingVegetationCount,
            Package->IsDirty() ? TEXT("true") : TEXT("false"));
        return false;
    }
    FString PredecessorReport;
    if (!ValidateR25WorldWithVegetationPolicy(
            World,
            ELandmarkPresencePolicy::Forbidden,
            PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_PREDECESSOR_CONTRACT: exact disk receipt lacks the exact R25/no-landmark-vegetation semantic contract. ") +
            PredecessorReport;
        return false;
    }

    FString VerifiedBackupFilename;
    FString BackupError;
    if (!CreateVerifiedPreLandmarkVegetationR26MapBackup(
            DestinationFilename,
            VerifiedBackupFilename,
            BackupError))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_BACKUP: map was not changed. ") +
            BackupError;
        return false;
    }

    FString RecheckedSha256;
    int64 RecheckedBytes = INDEX_NONE;
    FString RecheckError;
    int32 RecheckedVegetationCount = 0;
    FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
        World, RecheckedVegetationCount);
    if (!HashFileSha256(
            DestinationFilename,
            RecheckedSha256,
            RecheckedBytes,
            RecheckError) ||
        RecheckedBytes != PreLandmarkVegetationR26Bytes ||
        RecheckedSha256 != PreLandmarkVegetationR26Sha256 ||
        Package->IsDirty() || RecheckedVegetationCount != 0)
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_REFUSED_RECHECK: predecessor changed after backup; no mutation occurred. ") +
            RecheckError;
        return false;
    }

    const auto RestoreVerifiedPredecessor = [
        &ValidateR25WorldWithVegetationPolicy,
        &VerifiedBackupFilename,
        &DestinationFilename](FString& OutRollbackReport) -> bool
    {
        if (UWorld* CurrentWorld = GEditor
                ? GEditor->GetEditorWorldContext().World()
                : nullptr)
        {
            if (UPackage* CurrentPackage = CurrentWorld->GetOutermost())
            {
                CurrentPackage->SetDirtyFlag(false);
            }
        }

        FString SourceFilename;
        UWorld* UnloadWorld =
            FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
            : nullptr;
        bool bUsedBlankWorld = false;
        if (!UnloadWorld)
        {
            UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
            bUsedBlankWorld = true;
        }
        UPackage* UnloadPackage = UnloadWorld
            ? UnloadWorld->GetOutermost()
            : nullptr;
        if (!UnloadWorld || !UnloadPackage ||
            UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
                DestinationMapPackage)
        {
            OutRollbackReport = TEXT("could not establish a destination-independent unload world; verified R26 predecessor backup remains authoritative.");
            return false;
        }

        FString RestoreError;
        if (!RestoreVerifiedPreLandmarkVegetationR26MapViaSiblingTemp(
                VerifiedBackupFilename,
                DestinationFilename,
                RestoreError))
        {
            OutRollbackReport = TEXT("could not restore the verified pre-R26 map: ") +
                RestoreError;
            return false;
        }
        UWorld* RestoredWorld =
            UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
        if (RestoredWorld)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage = RestoredWorld
            ? RestoredWorld->GetOutermost()
            : nullptr;
        int32 RestoredVegetationCount = 0;
        if (RestoredWorld)
        {
            FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
                RestoredWorld, RestoredVegetationCount);
        }
        FString RestoredContract;
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString RestoredHashError;
        const bool bRestored = RestoredWorld && RestoredPackage &&
            !RestoredPackage->IsDirty() && RestoredVegetationCount == 0 &&
            ValidateR25WorldWithVegetationPolicy(
                RestoredWorld,
                ELandmarkPresencePolicy::Forbidden,
                RestoredContract) &&
            HashFileSha256(
                DestinationFilename,
                RestoredSha256,
                RestoredBytes,
                RestoredHashError) &&
            RestoredBytes == PreLandmarkVegetationR26Bytes &&
            RestoredSha256 == PreLandmarkVegetationR26Sha256;
        OutRollbackReport = FString::Printf(
            TEXT("coldDiskPredecessorRestore=%s blankUnloadFallback=%s landmarkVegetation=%d bytes=%lld sha256=%s contract={%s} hash={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            bUsedBlankWorld ? TEXT("true") : TEXT("false"),
            RestoredVegetationCount,
            RestoredBytes,
            *RestoredSha256,
            *RestoredContract,
            *RestoredHashError);
        return bRestored;
    };

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.Name =
        TEXT("TRIADIstanaExploreV5DR26LandmarkVegetation");
    ATRIADIstanaExploreV5DLandmarkVegetationActor* LandmarkVegetation =
        World->SpawnActor<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
            ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    FString ConfigureReport;
    if (!LandmarkVegetation ||
        !UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ConfigureLandmarkVegetationActor(
                LandmarkVegetation, ConfigureReport))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_FAILED_CONFIGURE: no map save occurred; coldRollbackSucceeded=%s configure={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *ConfigureReport,
            *RollbackReport);
        return false;
    }
    LandmarkVegetation->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R26 Landmark Vegetation - Persistent Render Only"));

    FString PreSaveReport;
    if (!ValidateR25WorldWithVegetationPolicy(
            World,
            ELandmarkPresencePolicy::Required,
            PreSaveReport))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_FAILED_PRE_SAVE_VALIDATION: no map save occurred; coldRollbackSucceeded=%s validation={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *PreSaveReport,
            *RollbackReport);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World, DestinationMapPackage))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_FAILED_SAVE: coldRollbackSucceeded=%s rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *RollbackReport);
        return false;
    }

    FString SourceFilename;
    UWorld* UnloadWorld =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    bool bColdUnloadUsedBlankWorld = false;
    if (!UnloadWorld)
    {
        UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        bColdUnloadUsedBlankWorld = true;
    }
    UPackage* UnloadPackage = UnloadWorld
        ? UnloadWorld->GetOutermost()
        : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
            DestinationMapPackage)
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_FAILED_COLD_UNLOAD: coldRollbackSucceeded=%s rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *RollbackReport);
        return false;
    }

    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedTarget
        ? ReloadedTarget->GetOutermost()
        : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateR25WorldWithVegetationPolicy(
            ReloadedTarget,
            ELandmarkPresencePolicy::Required,
            ColdReport))
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_FAILED_COLD_VALIDATION: coldRollbackSucceeded=%s cold={%s} rollback={%s}"),
            bRollback ? TEXT("true") : TEXT("false"),
            *ColdReport,
            *RollbackReport);
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    const bool bSuccessorReceipt = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError) &&
        SuccessorBytes > 0 && SuccessorSha256.Len() == 64 &&
        (SuccessorBytes != PreLandmarkVegetationR26Bytes ||
         SuccessorSha256 != PreLandmarkVegetationR26Sha256);
    const bool bBackupExact = HashFileSha256(
        VerifiedBackupFilename,
        BackupSha256,
        BackupBytes,
        BackupHashError) &&
        BackupBytes == PreLandmarkVegetationR26Bytes &&
        BackupSha256 == PreLandmarkVegetationR26Sha256;
    if (!bSuccessorReceipt || !bBackupExact)
    {
        FString RollbackReport;
        const bool bRollback = RestoreVerifiedPredecessor(RollbackReport);
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_FAILED_FINAL_RECEIPT: successorReceipt=%s backupExact=%s successorBytes=%lld successorSha256=%s coldRollbackSucceeded=%s successorHash={%s} backupHash={%s} rollback={%s}"),
            bSuccessorReceipt ? TEXT("true") : TEXT("false"),
            bBackupExact ? TEXT("true") : TEXT("false"),
            SuccessorBytes,
            *SuccessorSha256,
            bRollback ? TEXT("true") : TEXT("false"),
            *SuccessorHashError,
            *BackupHashError,
            *RollbackReport);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_PASS: exact R25 predecessor was backed up without overwrite; oneSave=true onePersistentActor=true landmarkVegetationR26=true contextFacadeR25=true exactReusableAssets=14 macDonaldGrass=1536 temasekGrass=1536 maximumGrassPerSite=2048 worldSpaceDeterministic=true nearestLandmarkPartition=true renderOnly=true sourceAssetsModified=false geometryExport=false collisionNavigationSensorRfAuthority=false providerSettingsUnchanged=true providerClipUnchanged=true contextFacadeOverridesUnchanged=17 coldUnloadUsedBlankWorld=%s predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s successorReceiptRequiresPinning=true. %s"),
        bColdUnloadUsedBlankWorld ? TEXT("true") : TEXT("false"),
        CurrentBytes,
        *CurrentSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *VerifiedBackupFilename,
        *ColdReport);
    return true;
}


bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DMacDonaldHouseR24SuccessorMap(
        FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateHybridWorld(World, OutReport,
        ELandmarkPresencePolicy::Required,
        ELandmarkPresencePolicy::Forbidden);
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DHybridMap(FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateHybridWorld(World, OutReport,
        ELandmarkPresencePolicy::Required,
        ELandmarkPresencePolicy::Required);
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DLocalFallbackSuppressionV2SuccessorMap(
        FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString HybridReport;
    if (!ValidateHybridWorld(
            World,
            HybridReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        OutReport = HybridReport;
        return false;
    }
    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World, PolicyCount);
    FString V2Report;
    if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
        !Policy->ValidateCurrentSurroundingsV2SuccessorForInheritedScene(
            Scene, V2Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_MAP_INVALID: ") +
            V2Report + TEXT(" ") + HybridReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_MAP_VALID: ") +
        HybridReport + TEXT(" ") + V2Report;
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DContextFacadeR25SuccessorMap(
        FString& OutReport)
{
    FString AssetReport;
    if (!TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
            AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_MAP_INVALID: ") +
            AssetReport;
        return false;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString HybridReport;
    if (!ValidateHybridWorld(
            World,
            HybridReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        OutReport = HybridReport;
        return false;
    }
    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World, PolicyCount);
    FString R25Report;
    if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
        !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            Scene, R25Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_MAP_INVALID: ") +
            R25Report + TEXT(" ") + HybridReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_MAP_VALID: ") +
        AssetReport + TEXT(" ") + HybridReport + TEXT(" ") + R25Report;
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap(
        FString& OutReport)
{
    FString ReusableAssetReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateReusableLandmarkVegetationAssets(ReusableAssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_INVALID: ") +
            ReusableAssetReport;
        return false;
    }
    FString R25AssetReport;
    if (!TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
            R25AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_INVALID: retained R25 assets failed validation. ") +
            R25AssetReport;
        return false;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    FString HybridReport;
    if (!ValidateHybridWorld(
            World,
            HybridReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_INVALID: ") +
            HybridReport;
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World, PolicyCount);
    FString R25Report;
    if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
        !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            Scene, R25Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_INVALID: exact R25 facade contract was not retained. ") +
            R25Report + TEXT(" ") + HybridReport;
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID: compatibilityEndpoint=true appliedActorContract=R27 landmarkVegetationR26HistoricalNameOnly=true mapIntegrated=true exactlyOneActor=true contextFacadeR25=true exactAssets=14 isolatedR27GrassMaterials=4 deterministic=true worldSpace=true nearestLandmarkPartition=true r26VisualAcceptanceFailed=true r27FarCameraCorrectionConfigured=true grassCullCm=6500,9000 evidenceRangeMeters=72.8 materialVisibilityAtEvidenceRangeConfigured=true visibilityGateCount=1 componentFadeIntegrated=true tallestCarrierTipProjectionPixelsConfigured=true projectionAssumption=perpendicularPinholeMaxSourceTip wpoDisableCm=2400 grassInstances=3072 maximumGrassPerSite=2048 visualCaptureAccepted=false captureRevalidationRequired=true renderOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false sourceAssetsModified=false geometryExport=false providerSettingsUnchanged=true providerClipUnchanged=true. ") +
        ReusableAssetReport + TEXT(" ") + R25AssetReport + TEXT(" ") +
        R25Report + TEXT(" ") + HybridReport;
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    FString DestinationFilename;
    if (!World || !World->PersistentLevel || !Package ||
        LogicalPackage != DestinationMapPackage || Package->IsDirty() ||
        !FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_MAP_STATE: load the exact clean saved V5D hybrid map first.");
        return false;
    }

    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    FString CurrentHashError;
    if (!HashFileSha256(
            DestinationFilename,
            CurrentSha256,
            CurrentBytes,
            CurrentHashError))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_HASH: ") +
            CurrentHashError;
        return false;
    }

    FString ExistingR27Report;
    if (ValidateLandmarkVegetationR27World(World, ExistingR27Report))
    {
        OutMessage = FString::Printf(
            TEXT("IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R27_ALREADY_VALID currentBytes=%lld currentSha256=%s visualCaptureAccepted=false captureRevalidationRequired=true. %s"),
            CurrentBytes,
            *CurrentSha256,
            *ExistingR27Report);
        return true;
    }

    FString ExpectedSha256 = ExpectedPredecessorSha256.ToUpper();
    bool bExpectedSha256Valid = ExpectedSha256.Len() == 64;
    for (TCHAR Character : ExpectedSha256)
    {
        bExpectedSha256Valid =
            bExpectedSha256Valid && FChar::IsHexDigit(Character);
    }
    if (ExpectedPredecessorBytes <= 0 || !bExpectedSha256Valid ||
        CurrentBytes != ExpectedPredecessorBytes ||
        CurrentSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_PREDECESSOR_PIN: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s existingValidation={%s}"),
            ExpectedPredecessorBytes,
            CurrentBytes,
            *ExpectedSha256,
            *CurrentSha256,
            *ExistingR27Report);
        return false;
    }

    FString BackupFilename = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    FPaths::NormalizeFilename(BackupFilename);
    FString AllowedBackupRoot = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/NativeTransactions/V5DLandmarkVegetationR27V1")));
    FPaths::NormalizeDirectoryName(AllowedBackupRoot);
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    if (VerifiedExternalBackupFilename.IsEmpty() ||
        FPaths::IsSamePath(BackupFilename, DestinationFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, AllowedBackupRoot) ||
        !HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            BackupHashError) ||
        BackupBytes != ExpectedPredecessorBytes ||
        BackupSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_BACKUP: an exact external backup under the bounded R27 transaction root is required; backup=%s bytes=%lld sha256=%s hash={%s}"),
            *BackupFilename,
            BackupBytes,
            *BackupSha256,
            *BackupHashError);
        return false;
    }

    FString ReusableAssetReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateReusableLandmarkVegetationAssets(ReusableAssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_ASSETS: ") +
            ReusableAssetReport;
        return false;
    }
    FString TemasekAssetReport;
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::ValidateAssets(
            TemasekAssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_TEMASEK_PHASE2: ") +
            TemasekAssetReport;
        return false;
    }

    FString PredecessorHybridReport;
    if (!ValidateHybridWorld(
            World,
            PredecessorHybridReport,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required,
            ELandmarkPresencePolicy::Required,
            false))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_PREDECESSOR_CONTRACT: ") +
            PredecessorHybridReport;
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 VegetationCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactlyOne<ATRIADIstanaExploreV5DContextPolicyActor>(
            World, PolicyCount);
    ATRIADIstanaExploreV5DLandmarkVegetationActor* Vegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
            World, VegetationCount);
    FString R25Report;
    if (SceneCount != 1 || PolicyCount != 1 || VegetationCount != 1 ||
        !Scene || !Policy || !Vegetation ||
        !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            Scene, R25Report))
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_PREDECESSOR_ROSTER: scene=%d policy=%d landmarkVegetation=%d r25={%s}"),
            SceneCount,
            PolicyCount,
            VegetationCount,
            *R25Report);
        return false;
    }

    // Close the validation-to-mutation window. The wrapper-created backup and
    // predecessor map must still be byte-exact immediately before the first
    // persistent actor edit; a dirty package is never admitted.
    FString MutationGateMapSha256;
    FString MutationGateBackupSha256;
    int64 MutationGateMapBytes = INDEX_NONE;
    int64 MutationGateBackupBytes = INDEX_NONE;
    FString MutationGateMapError;
    FString MutationGateBackupError;
    if (!World->GetOutermost() || World->GetOutermost()->IsDirty() ||
        !HashFileSha256(
            DestinationFilename,
            MutationGateMapSha256,
            MutationGateMapBytes,
            MutationGateMapError) ||
        !HashFileSha256(
            BackupFilename,
            MutationGateBackupSha256,
            MutationGateBackupBytes,
            MutationGateBackupError) ||
        MutationGateMapBytes != ExpectedPredecessorBytes ||
        MutationGateMapSha256 != ExpectedSha256 ||
        MutationGateBackupBytes != ExpectedPredecessorBytes ||
        MutationGateBackupSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_REFUSED_FINAL_MUTATION_GATE: packageClean=%s mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapHash={%s} backupHash={%s}"),
            World->GetOutermost() && !World->GetOutermost()->IsDirty()
                ? TEXT("true") : TEXT("false"),
            MutationGateMapBytes,
            *MutationGateMapSha256,
            MutationGateBackupBytes,
            *MutationGateBackupSha256,
            *MutationGateMapError,
            *MutationGateBackupError);
        return false;
    }

    FString ConfigureReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ConfigureLandmarkVegetationActor(Vegetation, ConfigureReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_FAILED_CONFIGURE_NO_SAVE: externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ConfigureReport;
        return false;
    }
    Vegetation->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R27 Landmark Vegetation - Persistent Render Only"));

    FString PreSaveReport;
    if (!ValidateLandmarkVegetationR27World(World, PreSaveReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_FAILED_PRE_SAVE_VALIDATION: externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            PreSaveReport;
        return false;
    }

    FString PreSaveDiskSha256;
    FString PreSaveBackupSha256;
    int64 PreSaveDiskBytes = INDEX_NONE;
    int64 PreSaveBackupBytes = INDEX_NONE;
    FString PreSaveDiskHashError;
    FString PreSaveBackupHashError;
    if (!HashFileSha256(
            DestinationFilename,
            PreSaveDiskSha256,
            PreSaveDiskBytes,
            PreSaveDiskHashError) ||
        !HashFileSha256(
            BackupFilename,
            PreSaveBackupSha256,
            PreSaveBackupBytes,
            PreSaveBackupHashError) ||
        PreSaveDiskBytes != ExpectedPredecessorBytes ||
        PreSaveDiskSha256 != ExpectedSha256 ||
        PreSaveBackupBytes != ExpectedPredecessorBytes ||
        PreSaveBackupSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_FAILED_PRE_SAVE_PIN_GATE: externalBackupVerified=true rollbackOwnedByWrapper=true diskBytes=%lld diskSha256=%s backupBytes=%lld backupSha256=%s diskHash={%s} backupHash={%s}"),
            PreSaveDiskBytes,
            *PreSaveDiskSha256,
            PreSaveBackupBytes,
            *PreSaveBackupSha256,
            *PreSaveDiskHashError,
            *PreSaveBackupHashError);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World, DestinationMapPackage))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_FAILED_SAVE: externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    FString SourceFilename;
    UWorld* UnloadWorld =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    bool bColdUnloadUsedBlankWorld = false;
    if (!UnloadWorld)
    {
        UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        bColdUnloadUsedBlankWorld = true;
    }
    UPackage* UnloadPackage = UnloadWorld
        ? UnloadWorld->GetOutermost()
        : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
            DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_FAILED_COLD_UNLOAD: externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedTarget
        ? ReloadedTarget->GetOutermost()
        : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateLandmarkVegetationR27World(ReloadedTarget, ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_FAILED_COLD_VALIDATION: externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ColdReport;
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString RecheckedBackupSha256;
    int64 RecheckedBackupBytes = INDEX_NONE;
    FString RecheckedBackupHashError;
    const bool bSuccessorValid = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError) &&
        SuccessorBytes > 0 && SuccessorSha256.Len() == 64 &&
        (SuccessorBytes != ExpectedPredecessorBytes ||
         SuccessorSha256 != ExpectedSha256);
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        RecheckedBackupSha256,
        RecheckedBackupBytes,
        RecheckedBackupHashError) &&
        RecheckedBackupBytes == ExpectedPredecessorBytes &&
        RecheckedBackupSha256 == ExpectedSha256;
    if (!bSuccessorValid || !bBackupPreserved)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_FAILED_FINAL_RECEIPT: successorValid=%s backupPreserved=%s externalBackupVerified=true rollbackOwnedByWrapper=true successorHash={%s} backupHash={%s}"),
            bSuccessorValid ? TEXT("true") : TEXT("false"),
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *SuccessorHashError,
            *RecheckedBackupHashError);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_LANDMARK_VEGETATION_R27_APPLY_PASS oneSave=true onePersistentActorReconfigured=true isolatedGrassMaterials=4 exactDerivativeGraph=true compiledMaterials=4 sourceMaterialsModified=false legacyTemasekBakedFoliageRemoved=true temasekMeshTriangles=15760 temasekMaterialSlots=15 bakedFoliageRenderComponents=0 sourceStableVisibilityMeters=20,28 targetStableVisibilityMeters=65,90 visibilityGateCount=1 componentFadeIntegrated=true grassCullCm=6500,9000 evidenceRangeMeters=72.8 materialVisibilityAtEvidenceRange=%.3f tallestCarrierTipProjectionPixelsAtEvidenceRange=%.3f projectionAssumption=perpendicularPinholeMaxSourceTip wpoDisableCm=2400 grassInstances=3072 maximumGrassPerSite=2048 visualCaptureAccepted=false captureRevalidationRequired=true contextFacadeR25=true renderOnly=true collisionNavigationSensorRfAuthority=false externalBackupVerified=true rollbackOwnedByWrapper=true coldUnloadUsedBlankWorld=%s predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s. %s"),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedMaterialVisibilityAtEvidenceView(),
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView(),
        bColdUnloadUsedBlankWorld ? TEXT("true") : TEXT("false"),
        ExpectedPredecessorBytes,
        *ExpectedSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *BackupFilename,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap(
        FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    if (!Package || Package->IsDirty())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_INVALID: exact saved clean editor world is required.");
        return false;
    }
    FString SemanticReport;
    if (!ValidateLandmarkVegetationR27World(World, SemanticReport))
    {
        OutReport = SemanticReport;
        return false;
    }
    FString DestinationFilename;
    FString MapSha256;
    int64 MapBytes = INDEX_NONE;
    FString HashError;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename) ||
        !HashFileSha256(
            DestinationFilename,
            MapSha256,
            MapBytes,
            HashError) ||
        MapBytes <= 0 || MapSha256.Len() != 64)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_MAP_INVALID_RECEIPT: ") +
            HashError;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R27_SUCCESSOR_VALID cleanSavedMap=true bytes=%lld sha256=%s visualCaptureAccepted=false captureRevalidationRequired=true. %s"),
        MapBytes,
        *MapSha256,
        *SemanticReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutMessage)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_EDITOR_STATE: requires a non-PIE editor world.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    FString DestinationFilename;
    if (!World || !World->PersistentLevel || !Package ||
        LogicalPackage != DestinationMapPackage || Package->IsDirty() ||
        !FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_MAP_STATE: load the exact clean saved V5D hybrid map first.");
        return false;
    }

    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    FString CurrentHashError;
    if (!HashFileSha256(
            DestinationFilename,
            CurrentSha256,
            CurrentBytes,
            CurrentHashError))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_HASH: ") +
            CurrentHashError;
        return false;
    }

    // Idempotence is semantic and precedes every predecessor parameter gate.
    // A complete saved R28 world therefore remains callable with stale R27
    // input pins, while a partial/mixed R28 state can never take this path.
    FString ExistingR28Report;
    if (ValidateR28VisualSuccessorWorld(World, ExistingR28Report))
    {
        OutMessage = FString::Printf(
            TEXT("IDEMPOTENT_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_ALREADY_VALID currentBytes=%lld currentSha256=%s oneSave=false visualCaptureAccepted=false captureRevalidationRequired=true. %s"),
            CurrentBytes,
            *CurrentSha256,
            *ExistingR28Report);
        return true;
    }

    FString ExpectedSha256 = ExpectedPredecessorSha256.ToUpper();
    bool bExpectedSha256Valid = ExpectedSha256.Len() == 64;
    for (TCHAR Character : ExpectedSha256)
    {
        bExpectedSha256Valid =
            bExpectedSha256Valid && FChar::IsHexDigit(Character);
    }
    if (ExpectedPredecessorBytes <= 0 || !bExpectedSha256Valid ||
        CurrentBytes != ExpectedPredecessorBytes ||
        CurrentSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_PREDECESSOR_PIN: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s existingR28Validation={%s}"),
            ExpectedPredecessorBytes,
            CurrentBytes,
            *ExpectedSha256,
            *CurrentSha256,
            *ExistingR28Report);
        return false;
    }

    FString BackupFilename = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    FPaths::NormalizeFilename(BackupFilename);
    FString AllowedBackupRoot = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/NativeTransactions/V5DVisualRealismR28V1")));
    FPaths::NormalizeDirectoryName(AllowedBackupRoot);
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupHashError;
    if (VerifiedExternalBackupFilename.IsEmpty() ||
        FPaths::IsSamePath(BackupFilename, DestinationFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, AllowedBackupRoot) ||
        !HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            BackupHashError) ||
        BackupBytes != ExpectedPredecessorBytes ||
        BackupSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_BACKUP: an exact external backup under Saved/TRIAD/NativeTransactions/V5DVisualRealismR28V1 is required; backup=%s bytes=%lld sha256=%s hash={%s}"),
            *BackupFilename,
            BackupBytes,
            *BackupSha256,
            *BackupHashError);
        return false;
    }

    FString LandmarkAssetReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ValidateReusableLandmarkVegetationAssetsR28(
                LandmarkAssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_LANDMARK_ASSETS: ") +
            LandmarkAssetReport;
        return false;
    }

    // The mutation predecessor is not merely hash-shaped: it must still be
    // the complete valid R27 world, including Phase-2, R25 and provider
    // negative-authority contracts.
    FString R27PredecessorReport;
    if (!ValidateLandmarkVegetationR27World(
            World,
            R27PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_R27_PREDECESSOR_CONTRACT: ") +
            R27PredecessorReport;
        return false;
    }

    int32 EnvironmentCount = 0;
    int32 VegetationCount = 0;
    ATRIADIstanaExploreV5DR28EnvironmentActor* EnvironmentPredecessor =
        FindExactlyOne<ATRIADIstanaExploreV5DR28EnvironmentActor>(
            World,
            EnvironmentCount);
    ATRIADIstanaExploreV5DLandmarkVegetationActor* Vegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
            World,
            VegetationCount);
    if (EnvironmentCount != 0 || EnvironmentPredecessor ||
        VegetationCount != 1 || !Vegetation ||
        Vegetation->GetClass() !=
            ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass())
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_PREDECESSOR_ROSTER: r28Environment=%d landmarkVegetation=%d exactLandmarkClass=%s"),
            EnvironmentCount,
            VegetationCount,
            Vegetation && Vegetation->GetClass() ==
                    ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass()
                ? TEXT("true")
                : TEXT("false"));
        return false;
    }

    // Close the validation-to-mutation window. Both map and external backup
    // must remain the exact R27 bytes immediately before the first persistent
    // world edit. The native wrapper owns rollback after this point.
    FString MutationGateMapSha256;
    FString MutationGateBackupSha256;
    int64 MutationGateMapBytes = INDEX_NONE;
    int64 MutationGateBackupBytes = INDEX_NONE;
    FString MutationGateMapError;
    FString MutationGateBackupError;
    if (!World->GetOutermost() || World->GetOutermost()->IsDirty() ||
        !HashFileSha256(
            DestinationFilename,
            MutationGateMapSha256,
            MutationGateMapBytes,
            MutationGateMapError) ||
        !HashFileSha256(
            BackupFilename,
            MutationGateBackupSha256,
            MutationGateBackupBytes,
            MutationGateBackupError) ||
        MutationGateMapBytes != ExpectedPredecessorBytes ||
        MutationGateMapSha256 != ExpectedSha256 ||
        MutationGateBackupBytes != ExpectedPredecessorBytes ||
        MutationGateBackupSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_FINAL_MUTATION_GATE: packageClean=%s mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapHash={%s} backupHash={%s}"),
            World->GetOutermost() && !World->GetOutermost()->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            MutationGateMapBytes,
            *MutationGateMapSha256,
            MutationGateBackupBytes,
            *MutationGateBackupSha256,
            *MutationGateMapError,
            *MutationGateBackupError);
        return false;
    }

    FString EnvironmentApplyReport;
    if (!UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary::
            ApplyR28EnvironmentToLoadedV5DHybridMap(
                EnvironmentApplyReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_ENVIRONMENT_NO_SAVE: externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            EnvironmentApplyReport;
        return false;
    }

    FString LandmarkConfigureReport;
    if (!UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary::
            ConfigureLandmarkVegetationActorR28(
                Vegetation,
                LandmarkConfigureReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_LANDMARK_NO_SAVE: externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            LandmarkConfigureReport;
        return false;
    }
    Vegetation->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R28 Landmark Vegetation - Persistent Render Only"));

    FString PreSaveReport;
    if (!ValidateR28VisualSuccessorWorld(World, PreSaveReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_PRE_SAVE_VALIDATION: externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            PreSaveReport;
        return false;
    }

    FString PreSaveDiskSha256;
    FString PreSaveBackupSha256;
    int64 PreSaveDiskBytes = INDEX_NONE;
    int64 PreSaveBackupBytes = INDEX_NONE;
    FString PreSaveDiskHashError;
    FString PreSaveBackupHashError;
    if (!HashFileSha256(
            DestinationFilename,
            PreSaveDiskSha256,
            PreSaveDiskBytes,
            PreSaveDiskHashError) ||
        !HashFileSha256(
            BackupFilename,
            PreSaveBackupSha256,
            PreSaveBackupBytes,
            PreSaveBackupHashError) ||
        PreSaveDiskBytes != ExpectedPredecessorBytes ||
        PreSaveDiskSha256 != ExpectedSha256 ||
        PreSaveBackupBytes != ExpectedPredecessorBytes ||
        PreSaveBackupSha256 != ExpectedSha256)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_PRE_SAVE_PIN_GATE: externalBackupVerified=true rollbackOwnedByWrapper=true diskBytes=%lld diskSha256=%s backupBytes=%lld backupSha256=%s diskHash={%s} backupHash={%s}"),
            PreSaveDiskBytes,
            *PreSaveDiskSha256,
            PreSaveBackupBytes,
            *PreSaveBackupSha256,
            *PreSaveDiskHashError,
            *PreSaveBackupHashError);
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World,
            DestinationMapPackage))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_SAVE: externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    FString SourceFilename;
    UWorld* UnloadWorld =
        FPackageName::DoesPackageExist(SourceMapPackage, &SourceFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)
        : nullptr;
    bool bColdUnloadUsedBlankWorld = false;
    if (!UnloadWorld)
    {
        UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
        bColdUnloadUsedBlankWorld = true;
    }
    UPackage* UnloadPackage = UnloadWorld
        ? UnloadWorld->GetOutermost()
        : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
            DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_COLD_UNLOAD: externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    UWorld* ReloadedTarget =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedTarget)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedTarget
        ? ReloadedTarget->GetOutermost()
        : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ReloadedPackage || ReloadedPackage->IsDirty() ||
        !ValidateR28VisualSuccessorWorld(ReloadedTarget, ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_COLD_VALIDATION: externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ColdReport;
        return false;
    }

    FString SuccessorSha256;
    int64 SuccessorBytes = INDEX_NONE;
    FString SuccessorHashError;
    FString RecheckedBackupSha256;
    int64 RecheckedBackupBytes = INDEX_NONE;
    FString RecheckedBackupHashError;
    const bool bSuccessorValid = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorHashError) &&
        SuccessorBytes > 0 && SuccessorSha256.Len() == 64 &&
        (SuccessorBytes != ExpectedPredecessorBytes ||
         SuccessorSha256 != ExpectedSha256);
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        RecheckedBackupSha256,
        RecheckedBackupBytes,
        RecheckedBackupHashError) &&
        RecheckedBackupBytes == ExpectedPredecessorBytes &&
        RecheckedBackupSha256 == ExpectedSha256;
    if (!bSuccessorValid || !bBackupPreserved)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_FAILED_FINAL_RECEIPT: successorValid=%s backupPreserved=%s externalBackupVerified=true rollbackOwnedByWrapper=true successorHash={%s} backupHash={%s}"),
            bSuccessorValid ? TEXT("true") : TEXT("false"),
            bBackupPreserved ? TEXT("true") : TEXT("false"),
            *SuccessorHashError,
            *RecheckedBackupHashError);
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_PASS oneSave=true environmentActorAdded=1 landmarkActorReconfigured=1 environmentAssets=13 landmarkAssets=14 landmarkGrassInstances=6144 landmarkGrassMaximumPerSite=4096 temasekTreeRoster=umbrella,dome,umbrella contextFacadeR25Retained=true temasekPhase2Retained=true providerSettingsUnchanged=true providerClipUnchanged=true collisionNavigationSensorRfAuthority=false externalBackupVerified=true rollbackOwnedByWrapper=true coldUnloadUsedBlankWorld=%s predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s visualCaptureAccepted=false captureRevalidationRequired=true. environmentApply={%s} landmarkConfigure={%s} cold={%s}"),
        bColdUnloadUsedBlankWorld ? TEXT("true") : TEXT("false"),
        ExpectedPredecessorBytes,
        *ExpectedSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *BackupFilename,
        *EnvironmentApplyReport,
        *LandmarkConfigureReport,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DR28VisualSuccessorMap(FString& OutReport)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    if (!World || !World->PersistentLevel || !Package ||
        LogicalPackage != DestinationMapPackage || Package->IsDirty())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_INVALID: exact saved clean editor world is required.");
        return false;
    }
    FString SemanticReport;
    if (!ValidateR28VisualSuccessorWorld(World, SemanticReport))
    {
        OutReport = SemanticReport;
        return false;
    }
    FString DestinationFilename;
    FString MapSha256;
    int64 MapBytes = INDEX_NONE;
    FString HashError;
    if (!FPackageName::DoesPackageExist(
            DestinationMapPackage,
            &DestinationFilename) ||
        !HashFileSha256(
            DestinationFilename,
            MapSha256,
            MapBytes,
            HashError) ||
        MapBytes <= 0 || MapSha256.Len() != 64)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_INVALID_RECEIPT: ") +
            HashError;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID cleanSavedMap=true bytes=%lld sha256=%s visualCaptureAccepted=false captureRevalidationRequired=true. %s"),
        MapBytes,
        *MapSha256,
        *SemanticReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DHybridPlayWorld(FString& OutReport)
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    if (!GetValidatedHybridPlayState(
            World,
            Player,
            Pawn,
            Policy,
            Tileset,
            Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_PIE_INVALID: ") + Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_HYBRID_PIE_VALID exactPlayer0V5Camera=true cesiumLoadProgress=%.3f localFallbackHidden=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s aerialProviderHandoffRequested=%s aerialProviderHandoffActive=%s stableVisualPolicy=true inheritedDeterministicSimulationLayersPreserved=true visualOnlyProvider=true collisionNavigationSensorRfAuthority=false."),
        Tileset->GetLoadProgress(),
        Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("true")
            : TEXT("false"),
        Policy->bProviderSiteClipCurrentlyActive
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAerialProviderHandoffRequested
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAerialProviderHandoffCurrentlyActive
            ? TEXT("true")
            : TEXT("false"));
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld(FString& OutReport)
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    FString R28WorldReport;
    if (!GetValidatedHybridPlayState(
            World,
            Player,
            Pawn,
            Policy,
            Tileset,
            Error,
            true,
            &R28WorldReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_PIE_INVALID: ") +
            Error;
        return false;
    }
    FString R28Player0Report;
    if (!ValidateR28Player0Presentation(
            World,
            Policy,
            false,
            R28Player0Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_PIE_INVALID: ") +
            R28Player0Report;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_PIE_VALID exactPlayer0V5Camera=true cesiumLoadProgress=%.3f localFallbackHidden=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s stableVisualPolicy=true r28MapWorldValidated=true r28ProviderNegativeAuthority=true r28World={%s} r28Player0={%s}"),
        Tileset->GetLoadProgress(),
        Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("true")
            : TEXT("false"),
        Policy->bProviderSiteClipCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible ? TEXT("true") : TEXT("false"),
        *R28WorldReport,
        *R28Player0Report);
    return true;
}

static bool GetIstanaExploreV5DHybridPlayStateReportForContract(
    bool bRequireR28VisualSuccessor,
    FString& OutReport)
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    FString R28WorldReport;
    if (!GetValidatedHybridPlayState(
            World,
            Player,
            Pawn,
            Policy,
            Tileset,
            Error,
            bRequireR28VisualSuccessor,
            &R28WorldReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_QA_STATE_INVALID: ") + Error;
        return false;
    }
    FString R28Player0Report;
    if (bRequireR28VisualSuccessor &&
        !ValidateR28Player0Presentation(
            World,
            Policy,
            false,
            R28Player0Report))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_QA_STATE_INVALID: ") +
            R28Player0Report;
        return false;
    }
    FString PolicyReport;
    Policy->ValidateHybridContext(PolicyReport);
    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
    ViewRotation.Normalize();
    const UCameraComponent* Camera = Pawn->GetExploreCameraComponent();
    const bool bExactQaViewPose =
        GHybridExactQaViewPose.Pawn.Get() == Pawn &&
        !Pawn->IsActorTickEnabled() &&
        Camera &&
        Camera->GetRelativeLocation().Equals(
            ExactQaViewCameraRelativeLocation,
            ExactQaViewLocationToleranceCentimeters) &&
        Camera->GetRelativeRotation().Equals(
            GHybridExactQaViewPose.WorldViewRotationDegrees,
            ExactQaViewRotationToleranceDegrees) &&
        Pawn->GetActorLocation().Equals(
            GHybridExactQaViewPose.WorldViewLocationCentimeters -
                ExactQaViewCameraRelativeLocation,
            ExactQaViewLocationToleranceCentimeters) &&
        Pawn->GetActorRotation().Equals(
            FRotator::ZeroRotator,
            ExactQaViewRotationToleranceDegrees) &&
        ViewLocation.Equals(
            GHybridExactQaViewPose.WorldViewLocationCentimeters,
            ExactQaViewLocationToleranceCentimeters) &&
        ViewRotation.Equals(
            GHybridExactQaViewPose.WorldViewRotationDegrees,
            ExactQaViewRotationToleranceDegrees);
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    const bool bProviderReadyForProof =
        Tileset->GetLoadProgress() >= 98.0f &&
        Policy->bLocalBuildingFallbackCurrentlyHidden &&
        bStableVisualPolicy;
    const FString BaseReport = FString::Printf(
        TEXT("map=%s pawn=%s pawnLocationCm=%s pawnRotationDeg=%s viewLocationCm=%s viewRotationDeg=%s exactQaViewPose=%s viewTargetMatchesPawn=true v5CameraProfile=true cesiumLoadProgress=%.3f providerReadyForProof=%s localFallbackHidden=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s aerialProviderHandoffRequested=%s aerialProviderHandoffActive=%s stableVisualPolicy=%s policy={%s}"),
        *UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()),
        *Pawn->GetName(),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString(),
        *ViewLocation.ToString(),
        *ViewRotation.ToString(),
        bExactQaViewPose ? TEXT("true") : TEXT("false"),
        Tileset->GetLoadProgress(),
        bProviderReadyForProof ? TEXT("true") : TEXT("false"),
        Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("true")
            : TEXT("false"),
        Policy->bProviderSiteClipCurrentlyActive
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAerialProviderHandoffRequested
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAerialProviderHandoffCurrentlyActive
            ? TEXT("true")
            : TEXT("false"),
        bStableVisualPolicy ? TEXT("true") : TEXT("false"),
        *PolicyReport);
    OutReport = bRequireR28VisualSuccessor
        ? TEXT("ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_QA_STATE_VALID r28VisualSuccessor=true r28MapWorldValidated=true r28ProviderNegativeAuthority=true ") +
              BaseReport + TEXT(" r28World={") + R28WorldReport +
              TEXT("} r28Player0={") + R28Player0Report + TEXT("}")
        : BaseReport;
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    GetIstanaExploreV5DHybridPlayStateReport(FString& OutReport)
{
    // Keep the legacy R27 endpoint structurally and behaviorally intact. The
    // versioned R28 endpoint below is the only public route that opts into the
    // combined R28 world contract.
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    if (!GetValidatedHybridPlayState(
            World,
            Player,
            Pawn,
            Policy,
            Tileset,
            Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_HYBRID_QA_STATE_INVALID: ") +
            Error;
        return false;
    }
    FString PolicyReport;
    Policy->ValidateHybridContext(PolicyReport);
    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = FRotator::ZeroRotator;
    Player->GetPlayerViewPoint(ViewLocation, ViewRotation);
    ViewRotation.Normalize();
    const UCameraComponent* Camera = Pawn->GetExploreCameraComponent();
    const bool bExactQaViewPose =
        GHybridExactQaViewPose.Pawn.Get() == Pawn &&
        !Pawn->IsActorTickEnabled() &&
        Camera &&
        Camera->GetRelativeLocation().Equals(
            ExactQaViewCameraRelativeLocation,
            ExactQaViewLocationToleranceCentimeters) &&
        Camera->GetRelativeRotation().Equals(
            GHybridExactQaViewPose.WorldViewRotationDegrees,
            ExactQaViewRotationToleranceDegrees) &&
        Pawn->GetActorLocation().Equals(
            GHybridExactQaViewPose.WorldViewLocationCentimeters -
                ExactQaViewCameraRelativeLocation,
            ExactQaViewLocationToleranceCentimeters) &&
        Pawn->GetActorRotation().Equals(
            FRotator::ZeroRotator,
            ExactQaViewRotationToleranceDegrees) &&
        ViewLocation.Equals(
            GHybridExactQaViewPose.WorldViewLocationCentimeters,
            ExactQaViewLocationToleranceCentimeters) &&
        ViewRotation.Equals(
            GHybridExactQaViewPose.WorldViewRotationDegrees,
            ExactQaViewRotationToleranceDegrees);
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    const bool bProviderReadyForProof =
        Tileset->GetLoadProgress() >= 98.0f &&
        Policy->bLocalBuildingFallbackCurrentlyHidden &&
        bStableVisualPolicy;
    OutReport = FString::Printf(
        TEXT("map=%s pawn=%s pawnLocationCm=%s pawnRotationDeg=%s viewLocationCm=%s viewRotationDeg=%s exactQaViewPose=%s viewTargetMatchesPawn=true v5CameraProfile=true cesiumLoadProgress=%.3f providerReadyForProof=%s localFallbackHidden=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s aerialProviderHandoffRequested=%s aerialProviderHandoffActive=%s stableVisualPolicy=%s policy={%s}"),
        *UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()),
        *Pawn->GetName(),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString(),
        *ViewLocation.ToString(),
        *ViewRotation.ToString(),
        bExactQaViewPose ? TEXT("true") : TEXT("false"),
        Tileset->GetLoadProgress(),
        bProviderReadyForProof ? TEXT("true") : TEXT("false"),
        Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("true")
            : TEXT("false"),
        Policy->bProviderSiteClipCurrentlyActive
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAerialProviderHandoffRequested
            ? TEXT("true")
            : TEXT("false"),
        Policy->bAerialProviderHandoffCurrentlyActive
            ? TEXT("true")
            : TEXT("false"),
        bStableVisualPolicy ? TEXT("true") : TEXT("false"),
        *PolicyReport);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    GetIstanaExploreV5DR28VisualSuccessorPlayStateReport(FString& OutReport)
{
    return GetIstanaExploreV5DHybridPlayStateReportForContract(
        true,
        OutReport);
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    TeleportIstanaExploreV5DHybridPlayPawnForQa(
        FVector WorldLocationCentimeters,
        FRotator WorldRotationDegrees,
        FString& OutMessage)
{
    if (FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_QA_TELEPORT_REFUSED_CAPTURE_ACTIVE: wait for the outstanding proof screenshot to finish.");
        return false;
    }
    RestoreHybridProofViewSuppression();
    ClearHybridExactQaViewPose();

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    if (WorldLocationCentimeters.ContainsNaN() ||
        WorldRotationDegrees.ContainsNaN() ||
        FVector2D(
            WorldLocationCentimeters.X,
            WorldLocationCentimeters.Y).Size() > 120000.0 ||
        WorldLocationCentimeters.Z < 50.0 ||
        WorldLocationCentimeters.Z > 50000.0 ||
        !GetValidatedHybridPlayState(
            World,
            Player,
            Pawn,
            Policy,
            Tileset,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_QA_TELEPORT_REFUSED: finite pose required inside the exact 1.2 km / 0.5-500 m QA envelope. ") +
            Error;
        return false;
    }
    UCameraComponent* Camera = Pawn->GetExploreCameraComponent();
    if (!Camera)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_QA_TELEPORT_REFUSED_CAMERA: the validated Player0 pawn lost its legacy Explore camera.");
        return false;
    }
    const FRotator Normalized = WorldRotationDegrees.GetNormalized();
    Pawn->SetActorTickEnabled(false);
    Camera->SetRelativeLocation(LegacyQaCameraRelativeLocation);
    Camera->SetRelativeRotation(LegacyQaCameraRelativeRotation);
    Camera->Activate(true);
    if (Pawn->IsActorTickEnabled() ||
        !Pawn->SetActorLocationAndRotation(
            WorldLocationCentimeters,
            Normalized,
            false,
            nullptr,
            ETeleportType::TeleportPhysics) ||
        !Pawn->GetActorLocation().Equals(WorldLocationCentimeters, 0.1f) ||
        !Pawn->GetActorRotation().Equals(Normalized, 0.1f) ||
        !Camera->GetRelativeLocation().Equals(
            LegacyQaCameraRelativeLocation,
            ExactQaViewLocationToleranceCentimeters) ||
        !Camera->GetRelativeRotation().Equals(
            LegacyQaCameraRelativeRotation,
            ExactQaViewRotationToleranceDegrees) ||
        Player->GetViewTarget() != Pawn)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_QA_TELEPORT_FAILED: pawn pose, exact legacy camera-relative pose, or view-target readback failed.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Teleported and QA-froze validated Explore V5D hybrid Player0 pawn to %s at %s with exact legacy cameraRelativeLocationCm=%s cameraRelativeRotationDeg=%s; Cesium will refine the new view before proof capture."),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString(),
        *Camera->GetRelativeLocation().ToString(),
        *Camera->GetRelativeRotation().ToString());
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    SetIstanaExploreV5DHybridPlayViewPoseForQa(
        FVector WorldViewLocationCentimeters,
        FRotator WorldViewRotationDegrees,
        FString& OutMessage)
{
    if (FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_REFUSED_CAPTURE_ACTIVE: wait for the outstanding proof screenshot to finish.");
        return false;
    }
    RestoreHybridProofViewSuppression();
    ClearHybridExactQaViewPose();

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    if (WorldViewLocationCentimeters.ContainsNaN() ||
        WorldViewRotationDegrees.ContainsNaN() ||
        FVector2D(
            WorldViewLocationCentimeters.X,
            WorldViewLocationCentimeters.Y).Size() > 120000.0 ||
        WorldViewLocationCentimeters.Z < 50.0 ||
        WorldViewLocationCentimeters.Z > 50000.0 ||
        !GetValidatedHybridPlayState(
            World,
            Player,
            Pawn,
            Policy,
            Tileset,
            Error) ||
        !HasStableHybridVisualPolicy(Policy))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_REFUSED: finite view pose, exact validated PIE Player0, stable visual policy, and the 1.2 km / 0.5-500 m QA envelope are required. ") +
            Error;
        return false;
    }

    UCameraComponent* Camera = Pawn->GetExploreCameraComponent();
    APlayerCameraManager* CameraManager = Player->PlayerCameraManager;
    if (!Camera || !CameraManager || Player->GetViewTarget() != Pawn)
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_REFUSED: exact Player0 camera/view-target state is unavailable.");
        return false;
    }

    const FRotator NormalizedViewRotation =
        WorldViewRotationDegrees.GetNormalized();
    const FVector PawnLocation =
        WorldViewLocationCentimeters - ExactQaViewCameraRelativeLocation;
    Pawn->SetActorTickEnabled(false);
    if (Pawn->IsActorTickEnabled() ||
        !Pawn->SetActorLocationAndRotation(
            PawnLocation,
            FRotator::ZeroRotator,
            false,
            nullptr,
            ETeleportType::TeleportPhysics))
    {
        OutMessage = TEXT("EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_FAILED: could not freeze and place the validated Player0 pawn.");
        return false;
    }
    Camera->SetRelativeLocation(ExactQaViewCameraRelativeLocation);
    Camera->SetRelativeRotation(NormalizedViewRotation);
    Camera->Activate(true);
    CameraManager->UpdateCamera(0.0f);

    FVector ActualViewLocation = FVector::ZeroVector;
    FRotator ActualViewRotation = FRotator::ZeroRotator;
    Player->GetPlayerViewPoint(ActualViewLocation, ActualViewRotation);
    ActualViewRotation.Normalize();
    if (!Pawn->GetActorLocation().Equals(
            PawnLocation,
            ExactQaViewLocationToleranceCentimeters) ||
        !Pawn->GetActorRotation().Equals(
            FRotator::ZeroRotator,
            ExactQaViewRotationToleranceDegrees) ||
        !Camera->GetRelativeLocation().Equals(
            ExactQaViewCameraRelativeLocation,
            ExactQaViewLocationToleranceCentimeters) ||
        !Camera->GetRelativeRotation().Equals(
            NormalizedViewRotation,
            ExactQaViewRotationToleranceDegrees) ||
        !ActualViewLocation.Equals(
            WorldViewLocationCentimeters,
            ExactQaViewLocationToleranceCentimeters) ||
        !ActualViewRotation.Equals(
            NormalizedViewRotation,
            ExactQaViewRotationToleranceDegrees) ||
        Player->GetViewTarget() != Pawn)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_FAILED_READBACK: requestedViewLocationCm=%s actualViewLocationCm=%s requestedViewRotationDeg=%s actualViewRotationDeg=%s."),
            *WorldViewLocationCentimeters.ToString(),
            *ActualViewLocation.ToString(),
            *NormalizedViewRotation.ToString(),
            *ActualViewRotation.ToString());
        return false;
    }

    GHybridExactQaViewPose.Pawn = Pawn;
    GHybridExactQaViewPose.WorldViewLocationCentimeters =
        WorldViewLocationCentimeters;
    GHybridExactQaViewPose.WorldViewRotationDegrees =
        NormalizedViewRotation;
    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_HYBRID_EXACT_QA_VIEW_POSE_PASS: exactQaViewPose=true viewLocationCm=%s viewRotationDeg=%s pawnLocationCm=%s cameraRelativeLocationCm=%s; PlayerCameraManager refreshed and readback is within 0.1 cm / 0.05 deg without persistent mutation."),
        *ActualViewLocation.ToString(),
        *ActualViewRotation.ToString(),
        *Pawn->GetActorLocation().ToString(),
        *Camera->GetRelativeLocation().ToString());
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa(
        FVector WorldViewLocationCentimeters,
        FRotator WorldViewRotationDegrees,
        FString& OutMessage)
{
    if (FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_REFUSED_CAPTURE_ACTIVE: wait for the outstanding proof screenshot to finish.");
        return false;
    }
    RestoreHybridProofViewSuppression();
    ClearHybridExactQaViewPose();

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    FString R28WorldReport;
    if (WorldViewLocationCentimeters.ContainsNaN() ||
        WorldViewRotationDegrees.ContainsNaN() ||
        FVector2D(
            WorldViewLocationCentimeters.X,
            WorldViewLocationCentimeters.Y).Size() > 120000.0 ||
        WorldViewLocationCentimeters.Z < 50.0 ||
        WorldViewLocationCentimeters.Z > 50000.0 ||
        !GetValidatedHybridPlayState(
            World,
            Player,
            Pawn,
            Policy,
            Tileset,
            Error,
            true,
            &R28WorldReport) ||
        !HasStableHybridVisualPolicy(Policy))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_REFUSED: finite view pose, exact R28-validated PIE Player0, stable visual policy, and the 1.2 km / 0.5-500 m QA envelope are required. ") +
            Error;
        return false;
    }
    FString R28Player0Report;
    if (!ValidateR28Player0Presentation(
            World,
            Policy,
            false,
            R28Player0Report))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_REFUSED: ") +
            R28Player0Report;
        return false;
    }

    UCameraComponent* Camera = Pawn->GetExploreCameraComponent();
    APlayerCameraManager* CameraManager = Player->PlayerCameraManager;
    if (!Camera || !CameraManager || Player->GetViewTarget() != Pawn)
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_REFUSED: exact Player0 camera/view-target state is unavailable.");
        return false;
    }

    const FRotator NormalizedViewRotation =
        WorldViewRotationDegrees.GetNormalized();
    const FVector PawnLocation =
        WorldViewLocationCentimeters - ExactQaViewCameraRelativeLocation;
    Pawn->SetActorTickEnabled(false);
    if (Pawn->IsActorTickEnabled() ||
        !Pawn->SetActorLocationAndRotation(
            PawnLocation,
            FRotator::ZeroRotator,
            false,
            nullptr,
            ETeleportType::TeleportPhysics))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_FAILED: could not freeze and place the validated Player0 pawn.");
        return false;
    }
    Camera->SetRelativeLocation(ExactQaViewCameraRelativeLocation);
    Camera->SetRelativeRotation(NormalizedViewRotation);
    Camera->Activate(true);
    CameraManager->UpdateCamera(0.0f);

    FVector ActualViewLocation = FVector::ZeroVector;
    FRotator ActualViewRotation = FRotator::ZeroRotator;
    Player->GetPlayerViewPoint(ActualViewLocation, ActualViewRotation);
    ActualViewRotation.Normalize();
    if (!Pawn->GetActorLocation().Equals(
            PawnLocation,
            ExactQaViewLocationToleranceCentimeters) ||
        !Pawn->GetActorRotation().Equals(
            FRotator::ZeroRotator,
            ExactQaViewRotationToleranceDegrees) ||
        !Camera->GetRelativeLocation().Equals(
            ExactQaViewCameraRelativeLocation,
            ExactQaViewLocationToleranceCentimeters) ||
        !Camera->GetRelativeRotation().Equals(
            NormalizedViewRotation,
            ExactQaViewRotationToleranceDegrees) ||
        !ActualViewLocation.Equals(
            WorldViewLocationCentimeters,
            ExactQaViewLocationToleranceCentimeters) ||
        !ActualViewRotation.Equals(
            NormalizedViewRotation,
            ExactQaViewRotationToleranceDegrees) ||
        Player->GetViewTarget() != Pawn)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_FAILED_READBACK: requestedViewLocationCm=%s actualViewLocationCm=%s requestedViewRotationDeg=%s actualViewRotationDeg=%s."),
            *WorldViewLocationCentimeters.ToString(),
            *ActualViewLocation.ToString(),
            *NormalizedViewRotation.ToString(),
            *ActualViewRotation.ToString());
        return false;
    }

    GHybridExactQaViewPose.Pawn = Pawn;
    GHybridExactQaViewPose.WorldViewLocationCentimeters =
        WorldViewLocationCentimeters;
    GHybridExactQaViewPose.WorldViewRotationDegrees =
        NormalizedViewRotation;
    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_PASS: exactQaViewPose=true r28VisualSuccessor=true r28MapWorldValidated=true r28ProviderNegativeAuthority=true viewLocationCm=%s viewRotationDeg=%s pawnLocationCm=%s cameraRelativeLocationCm=%s r28World={%s} r28Player0={%s}; PlayerCameraManager refreshed and readback is within 0.1 cm / 0.05 deg without persistent mutation."),
        *ActualViewLocation.ToString(),
        *ActualViewRotation.ToString(),
        *Pawn->GetActorLocation().ToString(),
        *Camera->GetRelativeLocation().ToString(),
        *R28WorldReport,
        *R28Player0Report);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    CaptureIstanaExploreV5DHybridPlayView(
        const FString& OutputFileName,
        FString& OutMessage)
{
    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |= Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    FText FilenameReason;
    if (!OutputFileName.StartsWith(
            TEXT("explore_v5d_"),
            ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5D capture requires one safe exact explore_v5d_*.png filename. ") +
            FilenameReason.ToString();
        return false;
    }

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    const bool bValidatedPlayState = GetValidatedHybridPlayState(
        World,
        Player,
        Pawn,
        Policy,
        Tileset,
        Error);
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    if (!bValidatedPlayState ||
        !Tileset || Tileset->GetLoadProgress() < 98.0f ||
        !Policy || !Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !bStableVisualPolicy)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D proof capture requires exact validated Player0, provider load >=98, hidden local building fallback, and the stable clipping/authored-core policy tuple; loadProgress=%.3f hidden=%s aerialRequested=%s aerialActive=%s stableVisualPolicy=%s. %s"),
            Tileset ? Tileset->GetLoadProgress() : -1.0f,
            Policy && Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffRequested
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffCurrentlyActive
                ? TEXT("true")
                : TEXT("false"),
            bStableVisualPolicy ? TEXT("true") : TEXT("false"),
            *Error);
        return false;
    }

    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Explore V5D proof capture requires the validated SDR game viewport.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV5D"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5D proof capture refused an unsafe directory, overwrite, or overlapping request.");
        return false;
    }
    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D proof-capture configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;
    int32 SuppressedLineBatcherCount = 0;
    int32 SuppressedHumanOverlayCount = 0;
    int32 SuppressedDemoTargetCount = 0;
    if (!BeginHybridProofViewSuppression(
            World,
            Player,
            SuppressedLineBatcherCount,
            SuppressedHumanOverlayCount,
            SuppressedDemoTargetCount))
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D proof capture could not install its reversible Player0-only debug-visual filter.");
        return false;
    }
    if (!Viewport->TakeHighResScreenShot())
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D game-viewport request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Accepted exact 2560x1440 HDR-off V5D Player0 proof capture at Cesium load %.3f with aerialProviderHandoffRequested=%s aerialProviderHandoffActive=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s to '%s'; capture-only Player0 filter hides %d world line batchers, %d tagged human overlays, and %d optional demo targets until screenshot completion without changing RF/simulation; wrapper must wait for and decode the PNG."),
        Tileset->GetLoadProgress(),
        Policy->bAerialProviderHandoffRequested ? TEXT("true") : TEXT("false"),
        Policy->bAerialProviderHandoffCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bProviderSiteClipCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible ? TEXT("true") : TEXT("false"),
        *Destination,
        SuppressedLineBatcherCount,
        SuppressedHumanOverlayCount,
        SuppressedDemoTargetCount);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    CaptureIstanaExploreV5DR28VisualSuccessorPlayView(
        const FString& OutputFileName,
        FString& OutMessage)
{
    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |= Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    FText FilenameReason;
    if (!OutputFileName.StartsWith(
            TEXT("explore_v5d_"),
            ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5D R28 provider-ready capture requires one safe exact explore_v5d_*.png filename. ") +
            FilenameReason.ToString();
        return false;
    }

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    FString R28WorldReport;
    const bool bValidatedPlayState = GetValidatedHybridPlayState(
        World,
        Player,
        Pawn,
        Policy,
        Tileset,
        Error,
        true,
        &R28WorldReport);
    FString R28Player0Report;
    const bool bR28Player0Valid = bValidatedPlayState &&
        ValidateR28Player0Presentation(
            World,
            Policy,
            false,
            R28Player0Report);
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    if (!bValidatedPlayState || !bR28Player0Valid ||
        !Tileset || Tileset->GetLoadProgress() < 98.0f ||
        !Policy || !Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !bStableVisualPolicy)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D R28 provider-ready proof capture requires the combined R28 world contract, exact Player0, provider load >=98, hidden local fallback/R28 environment, and the stable clipping/authored-core policy tuple; loadProgress=%.3f hidden=%s stableVisualPolicy=%s. %s %s"),
            Tileset ? Tileset->GetLoadProgress() : -1.0f,
            Policy && Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            bStableVisualPolicy ? TEXT("true") : TEXT("false"),
            *Error,
            *R28Player0Report);
        return false;
    }

    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Explore V5D R28 provider-ready proof capture requires the validated SDR game viewport.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV5D"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5D R28 provider-ready proof capture refused an unsafe directory, overwrite, or overlapping request.");
        return false;
    }
    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D R28 provider-ready proof-capture configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;
    int32 SuppressedLineBatcherCount = 0;
    int32 SuppressedHumanOverlayCount = 0;
    int32 SuppressedDemoTargetCount = 0;
    if (!BeginHybridProofViewSuppression(
            World,
            Player,
            SuppressedLineBatcherCount,
            SuppressedHumanOverlayCount,
            SuppressedDemoTargetCount))
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D R28 provider-ready proof capture could not install its reversible Player0-only debug-visual filter.");
        return false;
    }
    if (!Viewport->TakeHighResScreenShot())
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D R28 provider-ready game-viewport request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Accepted exact 2560x1440 HDR-off V5D R28 visual-successor Player0 proof capture at Cesium load %.3f with r28VisualSuccessor=true r28MapWorldValidated=true r28EnvironmentPlayer0Visible=false r28EnvironmentSceneCaptureSensorExcluded=true r28ProviderNegativeAuthority=true providerSiteClipActive=%s authoredCoreVisualsVisible=%s to '%s'; capture-only Player0 filter hides %d world line batchers, %d tagged human overlays, and %d optional demo targets until screenshot completion without changing terrain/collision/navigation/sensor/RF authority; r28World={%s} r28Player0={%s}; wrapper must wait for and decode the PNG."),
        Tileset->GetLoadProgress(),
        Policy->bProviderSiteClipCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible ? TEXT("true") : TEXT("false"),
        *Destination,
        SuppressedLineBatcherCount,
        SuppressedHumanOverlayCount,
        SuppressedDemoTargetCount,
        *R28WorldReport,
        *R28Player0Report);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    CaptureIstanaExploreV5DHybridGrassSemanticMattePlayView(
        const FString& OutputFileName,
    FString& OutMessage)
{
    constexpr TCHAR MatteFilenamePrefix[] =
        TEXT("explore_v5d_r23_grass_semantic_matte_");
    constexpr int32 CaptureWidth = 2560;
    constexpr int32 CaptureHeight = 1440;
    constexpr float RequiredHorizontalFovDegrees = 80.0f;
    constexpr float RequiredAspectRatio = 16.0f / 9.0f;
    constexpr float MaximumGrassDepthCentimeters = 5000.0f;
    constexpr float DepthEqualityEpsilonCentimeters = 0.25f;
    constexpr int32 ExpectedTotalInstances = 18944;

    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |= Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    FText FilenameReason;
    if (!OutputFileName.StartsWith(
            MatteFilenamePrefix,
            ESearchCase::CaseSensitive) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        OutputFileName.Len() <= FCString::Strlen(MatteFilenamePrefix) + 4 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte requires one safe exact explore_v5d_r23_grass_semantic_matte_*.png filename. ") +
            FilenameReason.ToString();
        return false;
    }

    static bool bGrassMatteCaptureActive = false;
    if (bGrassMatteCaptureActive ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte refused an overlapping matte or high-resolution screenshot request.");
        return false;
    }

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    const bool bValidatedPlayState = GetValidatedHybridPlayState(
        World,
        Player,
        Pawn,
        Policy,
        Tileset,
        Error);
    const float LoadProgress = Tileset ? Tileset->GetLoadProgress() : -1.0f;
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    if (!bValidatedPlayState || !Tileset ||
        !FMath::IsFinite(LoadProgress) || LoadProgress < 98.0f ||
        LoadProgress > 100.0f || !Policy ||
        !Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !bStableVisualPolicy)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D R23 grass semantic matte requires exact validated Player0, finite provider load >=98, hidden local fallback, and the stable clipping/authored-core policy tuple; loadProgress=%.3f hidden=%s stableVisualPolicy=%s. %s"),
            LoadProgress,
            Policy && Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            bStableVisualPolicy ? TEXT("true") : TEXT("false"),
            *Error);
        return false;
    }

    int32 GroundVegetationCount = 0;
    ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            World,
            GroundVegetationCount);
    FString GroundReportBefore;
    if (GroundVegetationCount != 1 || !IsValid(GroundVegetation) ||
        GroundVegetation->GetWorld() != World ||
        !GroundVegetation->ValidateGroundVegetationRealism(
            GroundReportBefore) ||
        !GroundReportBefore.StartsWith(
            TEXT("ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID"),
            ESearchCase::CaseSensitive) ||
        !GroundReportBefore.Contains(
            TEXT("grassPresentationRevision=R23"),
            ESearchCase::CaseSensitive) ||
        !GroundReportBefore.Contains(
            TEXT("stagedLegacyMigration=R14ToR20ToR23"),
            ESearchCase::CaseSensitive) ||
        !GroundReportBefore.Contains(
            TEXT("grassMaterialRevision=R23"),
            ESearchCase::CaseSensitive) ||
        !GroundReportBefore.Contains(
            TEXT("groundOverlayMaterialRevision=R23"),
            ESearchCase::CaseSensitive) ||
        !GroundReportBefore.Contains(
            TEXT("edgeGrassMaterialRevision=R23"),
            ESearchCase::CaseSensitive) ||
        !GroundReportBefore.Contains(
            TEXT("runtimeMaterialRevisionMarker=23"),
            ESearchCase::CaseSensitive))
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte requires exactly one fully validated R23 ground-vegetation actor. ") +
            GroundReportBefore;
        return false;
    }

    struct FExpectedIntegerConsoleVariable
    {
        const TCHAR* Name;
        int32 Value;
    };
    constexpr FExpectedIntegerConsoleVariable ExpectedIntegerConsoleVariables[] = {
        {TEXT("foliage.SplitFactor"), 16},
        {TEXT("foliage.ForceLOD"), -1},
        {TEXT("foliage.OnlyLOD"), -1},
        {TEXT("foliage.DisableCull"), 0},
        {TEXT("foliage.CullAll"), 0},
        {TEXT("foliage.DitheredLOD"), 1},
        {TEXT("foliage.OverestimateLOD"), 0},
        {TEXT("foliage.MaxTrianglesToRender"), 100000000},
        {TEXT("foliage.MinVertsToSplitNode"), 8192},
        {TEXT("foliage.MaxEndCullDistance"), 0},
        {TEXT("foliage.MinLOD"), -1},
        {TEXT("foliage.CullAllInVertexShader"), 0},
        {TEXT("r.MeshStreaming"), 0},
        {TEXT("r.SceneCapture.DepthPrepassOptimization"), 0}};
    for (const FExpectedIntegerConsoleVariable& Expected :
         ExpectedIntegerConsoleVariables)
    {
        IConsoleVariable* ConsoleVariable =
            IConsoleManager::Get().FindConsoleVariable(Expected.Name);
        if (!ConsoleVariable || ConsoleVariable->GetInt() != Expected.Value)
        {
            OutMessage = FString::Printf(
                TEXT("Explore V5D R23 grass semantic matte requires the exact non-mutating HISM integer CVar baseline; %s expected %d, found %s."),
                Expected.Name,
                Expected.Value,
                ConsoleVariable
                    ? *FString::FromInt(ConsoleVariable->GetInt())
                    : TEXT("missing"));
            return false;
        }
    }

    struct FExpectedFloatConsoleVariable
    {
        const TCHAR* Name;
        float Value;
        float Tolerance;
    };
    constexpr FExpectedFloatConsoleVariable ExpectedFloatConsoleVariables[] = {
        {TEXT("foliage.MinimumScreenSize"), 0.000005f, 0.00000001f},
        {TEXT("foliage.LODDistanceScale"), 1.0f, 0.000001f},
        {TEXT("foliage.RandomLODRange"), 0.0f, 0.000001f}};
    for (const FExpectedFloatConsoleVariable& Expected :
         ExpectedFloatConsoleVariables)
    {
        IConsoleVariable* ConsoleVariable =
            IConsoleManager::Get().FindConsoleVariable(Expected.Name);
        if (!ConsoleVariable ||
            !FMath::IsNearlyEqual(
                ConsoleVariable->GetFloat(),
                Expected.Value,
                Expected.Tolerance))
        {
            OutMessage = FString::Printf(
                TEXT("Explore V5D R23 grass semantic matte requires the exact non-mutating HISM float CVar baseline; %s expected %.9f, found %s."),
                Expected.Name,
                Expected.Value,
                ConsoleVariable
                    ? *FString::Printf(
                          TEXT("%.9f"),
                          ConsoleVariable->GetFloat())
                    : TEXT("missing"));
            return false;
        }
    }
    const float CachedViewDistanceScale =
        GetCachedScalabilityCVars().ViewDistanceScale;
    if (!FMath::IsNearlyEqual(CachedViewDistanceScale, 1.0f, 0.000001f))
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D R23 grass semantic matte requires cached HISM view-distance scale 1.0, found %.9f."),
            CachedViewDistanceScale);
        return false;
    }
    const float ValidatedFoliageMinimumScreenSize =
        IConsoleManager::Get()
            .FindConsoleVariable(TEXT("foliage.MinimumScreenSize"))
            ->GetFloat();
    const float ValidatedFoliageLodDistanceScale =
        IConsoleManager::Get()
            .FindConsoleVariable(TEXT("foliage.LODDistanceScale"))
            ->GetFloat();
    const float ValidatedFoliageRandomLodRange =
        IConsoleManager::Get()
            .FindConsoleVariable(TEXT("foliage.RandomLODRange"))
            ->GetFloat();

    UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GroundVegetation->GrassManicuredInstances,
        GroundVegetation->GrassHumidInstances,
        GroundVegetation->GrassShadeInstances,
        GroundVegetation->GrassDryEdgeInstances,
        GroundVegetation->EdgeGrassInstances};
    const TCHAR* const ExpectedComponentNames[] = {
        TEXT("V5DGrassManicuredMicroClumps"),
        TEXT("V5DGrassHumidMicroClumps"),
        TEXT("V5DGrassShadeMicroClumps"),
        TEXT("V5DGrassDryEdgeMicroClumps"),
        TEXT("V5DEdgeGrassSeasonalAccents")};
    const TCHAR* const ExpectedMaterialPaths[] = {
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Manicured.M_IPV5D_Turf_Manicured"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Humid.M_IPV5D_Turf_Humid"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Shade.M_IPV5D_Turf_Shade"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_DryEdge.M_IPV5D_Turf_DryEdge"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_GrassMedium_EdgeFade.M_IPV5D_GrassMedium_EdgeFade")};
    constexpr int32 ExpectedInstanceCounts[] = {
        12461, 3975, 1150, 846, 512};
    constexpr int32 ExpectedCullStarts[] = {
        2600, 2600, 2600, 2600, 3000};
    constexpr int32 ExpectedCullEnds[] = {
        3400, 3400, 3400, 3400, 4500};
    constexpr int32 ExpectedWpoDisableDistances[] = {
        2400, 2400, 2400, 2400, 2400};
    static_assert(
        UE_ARRAY_COUNT(GrassComponents) == 5 &&
            UE_ARRAY_COUNT(ExpectedComponentNames) == 5 &&
            UE_ARRAY_COUNT(ExpectedMaterialPaths) == 5 &&
            UE_ARRAY_COUNT(ExpectedInstanceCounts) == 5 &&
            UE_ARRAY_COUNT(ExpectedCullStarts) == 5 &&
            UE_ARRAY_COUNT(ExpectedCullEnds) == 5 &&
            UE_ARRAY_COUNT(ExpectedWpoDisableDistances) == 5,
        "The semantic-matte grass roster must remain exactly five rows.");

    int32 VerifiedTotalInstances = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassComponents); ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            GrassComponents[Index];
        int32 CullStart = 0;
        int32 CullEnd = 0;
        if (Component)
        {
            Component->GetCullDistances(CullStart, CullEnd);
        }
        UMaterialInterface* Material = Component
            ? Component->GetMaterial(0)
            : nullptr;
        if (!IsValid(Component) || Component->GetOwner() != GroundVegetation ||
            Component->GetWorld() != World || !Component->IsRegistered() ||
            Component->GetFName() != FName(ExpectedComponentNames[Index]) ||
            !Component->GetStaticMesh() ||
            Component->GetStaticMesh()->IsCompiling() ||
            !Component->GetStaticMesh()->HasValidRenderData(false) ||
            !IsValid(Material) ||
            Material->GetPathName() != ExpectedMaterialPaths[Index] ||
            Component->GetInstanceCount() != ExpectedInstanceCounts[Index] ||
            CullStart != ExpectedCullStarts[Index] ||
            CullEnd != ExpectedCullEnds[Index] ||
            Component->WorldPositionOffsetDisableDistance !=
                ExpectedWpoDisableDistances[Index] ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                0.60f,
                0.0001f) ||
            Component->bEnableDensityScaling ||
            !FMath::IsNearlyEqual(
                Component->CurrentDensityScaling,
                1.0f,
                0.0001f) ||
            Component->InstancingRandomSeed == 0 ||
            Component->ForcedLodModel != 0 || Component->bOverrideMinLOD ||
            Component->MinLOD != 0 ||
            !Component->IsVisible() || Component->bHiddenInGame ||
            !Component->bRenderInMainPass ||
            Component->bHiddenInSceneCapture ||
            Component->GetCollisionEnabled() !=
                ECollisionEnabled::NoCollision ||
            Component->CanEverAffectNavigation())
        {
            OutMessage = FString::Printf(
                TEXT("Explore V5D R23 grass semantic matte rejected grass roster row %d (%s): exact owner/name/material/count/cull/WPO/LOD/density/render-data/random-seed/visibility/render-only state drifted."),
                Index,
                ExpectedComponentNames[Index]);
            return false;
        }
        VerifiedTotalInstances += Component->GetInstanceCount();
    }
    if (VerifiedTotalInstances != ExpectedTotalInstances)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D R23 grass semantic matte expected exactly %d instances, found %d."),
            ExpectedTotalInstances,
            VerifiedTotalInstances);
        return false;
    }
    FString TransformValidationReport;
    if (!GroundVegetation->ValidateOwnedGrassInstanceTransforms(
            TransformValidationReport))
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte rejected the exact deterministic instance-transform proof. ") +
            TransformValidationReport;
        return false;
    }
    int32 SynchronouslyRebuiltClusterTrees = 0;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassComponents); ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            GrassComponents[Index];
        if (!Component->BuildTreeIfOutdated(
                /*Async*/ false,
                /*ForceUpdate*/ true) ||
            !Component->IsTreeFullyBuilt() ||
            Component->GetNumRenderInstances() !=
                ExpectedInstanceCounts[Index])
        {
            OutMessage = FString::Printf(
                TEXT("Explore V5D R23 grass semantic matte could not synchronously rebuild exact grass cluster tree %d under the pinned CVar baseline."),
                Index);
            return false;
        }
        ++SynchronouslyRebuiltClusterTrees;
    }
    if (SynchronouslyRebuiltClusterTrees != 5)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte did not synchronously rebuild exactly five grass cluster trees.");
        return false;
    }

    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    ULocalPlayer* LocalPlayer = Player ? Player->GetLocalPlayer() : nullptr;
    APlayerCameraManager* CameraManager = Player
        ? Player->PlayerCameraManager
        : nullptr;
    FSceneViewProjectionData ProjectionData;
    FVector PlayerViewLocation = FVector::ZeroVector;
    FRotator PlayerViewRotation = FRotator::ZeroRotator;
    if (Player)
    {
        Player->GetPlayerViewPoint(PlayerViewLocation, PlayerViewRotation);
    }
    const FMinimalViewInfo* CameraView = CameraManager
        ? &CameraManager->GetCameraCacheView()
        : nullptr;
    if (!Viewport || !LocalPlayer || !CameraManager || !CameraView ||
        !LocalPlayer->GetProjectionData(
            Viewport,
            ProjectionData,
            INDEX_NONE) ||
        !ProjectionData.IsValidViewRectangle() ||
        !ProjectionData.IsPerspectiveProjection() ||
        ProjectionData.ProjectionMatrix.ContainsNaN() ||
        CameraView->ProjectionMode != ECameraProjectionMode::Perspective ||
        !FMath::IsNearlyEqual(
            CameraView->FOV,
            RequiredHorizontalFovDegrees,
            0.001f))
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte requires the exact finite perspective Player0 projection and 80-degree camera cache.");
        return false;
    }
    const FIntRect ProjectionRect = ProjectionData.GetConstrainedViewRect();
    const float ProjectionAspectRatio =
        static_cast<float>(ProjectionRect.Width()) /
        static_cast<float>(ProjectionRect.Height());
    if (!FMath::IsNearlyEqual(
            ProjectionAspectRatio,
            RequiredAspectRatio,
            0.001f) ||
        (ProjectionData.ViewOrigin - PlayerViewLocation).Size() >
            ExactQaViewLocationToleranceCentimeters)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D R23 grass semantic matte requires an exact 16:9 Player0 projection and matching view origin; aspect=%.9f originDeltaCm=%.6f."),
            ProjectionAspectRatio,
            (ProjectionData.ViewOrigin - PlayerViewLocation).Size());
        return false;
    }

    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV5D"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte refused an unsafe directory or overwrite.");
        return false;
    }

    const auto CountRegisteredCaptureComponents = [World]() -> int32
    {
        int32 Count = 0;
        for (TObjectIterator<USceneCaptureComponent2D> It; It; ++It)
        {
            if (IsValid(*It) && It->GetWorld() == World && It->IsRegistered())
            {
                ++Count;
            }
        }
        return Count;
    };
    const int32 RegisteredCaptureComponentsBefore =
        CountRegisteredCaptureComponents();

    bGrassMatteCaptureActive = true;
    ON_SCOPE_EXIT
    {
        bGrassMatteCaptureActive = false;
    };

    UTextureRenderTarget2D* RenderTarget =
        NewObject<UTextureRenderTarget2D>(
            GetTransientPackage(),
            NAME_None,
            RF_Transient);
    USceneCaptureComponent2D* CaptureComponent =
        NewObject<USceneCaptureComponent2D>(
            GetTransientPackage(),
            NAME_None,
            RF_Transient);
    bool bTransientCaptureCleaned = false;
    const auto CleanupTransientCapture = [&]()
    {
        if (bTransientCaptureCleaned)
        {
            return;
        }
        if (CaptureComponent)
        {
            CaptureComponent->ClearShowOnlyComponents();
            CaptureComponent->ClearHiddenComponents();
            CaptureComponent->HiddenActors.Reset();
            CaptureComponent->ShowOnlyActors.Reset();
            CaptureComponent->TextureTarget = nullptr;
            if (CaptureComponent->IsRegistered())
            {
                CaptureComponent->UnregisterComponent();
            }
            CaptureComponent->DestroyComponent();
        }
        if (RenderTarget)
        {
            RenderTarget->ReleaseResource();
        }
        bTransientCaptureCleaned = true;
    };
    ON_SCOPE_EXIT
    {
        CleanupTransientCapture();
    };

    if (!RenderTarget || !CaptureComponent)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte could not allocate its two transient capture objects.");
        return false;
    }
    RenderTarget->ClearColor = FLinearColor(
        MaximumGrassDepthCentimeters * 4.0f,
        0.0f,
        0.0f,
        0.0f);
    RenderTarget->InitCustomFormat(
        CaptureWidth,
        CaptureHeight,
        PF_R32_FLOAT,
        true);
    RenderTarget->UpdateResourceImmediate(true);

    CaptureComponent->SetMobility(EComponentMobility::Movable);
    CaptureComponent->SetWorldLocationAndRotation(
        PlayerViewLocation,
        PlayerViewRotation);
    CaptureComponent->ProjectionType = ECameraProjectionMode::Perspective;
    CaptureComponent->FOVAngle = CameraView->FOV;
    CaptureComponent->bUseCustomProjectionMatrix = true;
    CaptureComponent->CustomProjectionMatrix = ProjectionData.ProjectionMatrix;
    CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
    CaptureComponent->bCaptureEveryFrame = false;
    CaptureComponent->bCaptureOnMovement = false;
    CaptureComponent->bAlwaysPersistRenderingState = false;
    CaptureComponent->bRenderInMainRenderer = false;
    CaptureComponent->bIgnoreScreenPercentage = true;
    CaptureComponent->bUseRayTracingIfEnabled = false;
    CaptureComponent->LODDistanceFactor = 1.0f;
    CaptureComponent->MaxViewDistanceOverride = 0.0f;
    CaptureComponent->TextureTarget = RenderTarget;
    CaptureComponent->ShowFlags.SetAntiAliasing(false);
    CaptureComponent->ShowFlags.SetTemporalAA(false);
    CaptureComponent->ShowFlags.SetMotionBlur(false);
    CaptureComponent->ShowFlags.SetMaterials(true);
    CaptureComponent->ShowFlags.SetInstancedStaticMeshes(true);
    CaptureComponent->ShowFlags.SetStaticMeshes(true);

    int32 HiddenLineBatcherCount = 0;
    int32 HiddenHumanOverlayCount = 0;
    int32 HiddenDemoTargetCount = 0;
    for (TObjectIterator<ULineBatchComponent> It; It; ++It)
    {
        ULineBatchComponent* LineBatcher = *It;
        if (IsValid(LineBatcher) && LineBatcher->GetWorld() == World)
        {
            CaptureComponent->HideComponent(LineBatcher);
            ++HiddenLineBatcherCount;
        }
    }
    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        AActor* Actor = *ActorIt;
        if (!IsValid(Actor))
        {
            continue;
        }
        if (Actor->IsA<ATRIADDemoDroneActor>())
        {
            CaptureComponent->HideActorComponents(Actor);
            ++HiddenDemoTargetCount;
        }
        TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(Actor);
        for (UPrimitiveComponent* Component : PrimitiveComponents)
        {
            if (IsValid(Component) &&
                Component->ComponentHasTag(HumanOnlyOverlayComponentTag))
            {
                CaptureComponent->HideComponent(Component);
                ++HiddenHumanOverlayCount;
            }
        }
    }

    CaptureComponent->RegisterComponentWithWorld(World);
    if (!CaptureComponent->IsRegistered() ||
        CountRegisteredCaptureComponents() !=
            RegisteredCaptureComponentsBefore + 1)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte could not register exactly one transient scene-capture component.");
        return false;
    }

    CaptureComponent->PrimitiveRenderMode =
        ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    for (UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
    {
        CaptureComponent->ShowOnlyComponent(Component);
    }
    if (CaptureComponent->ShowOnlyComponents.Num() != 5)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte could not install exactly five show-only grass components.");
        return false;
    }

    const auto CaptureLinearDepth = [CaptureComponent, RenderTarget](
        TArray<FLinearColor>& OutDepth) -> bool
    {
        CaptureComponent->bCameraCutThisFrame = true;
        CaptureComponent->CaptureScene();
        FTextureRenderTargetResource* Resource =
            RenderTarget->GameThread_GetRenderTargetResource();
        return Resource && Resource->ReadLinearColorPixels(
            OutDepth,
            FReadSurfaceDataFlags(RCM_MinMax),
            FIntRect(0, 0, CaptureWidth, CaptureHeight)) &&
            OutDepth.Num() == CaptureWidth * CaptureHeight;
    };

    TArray<FLinearColor> GrassOnlyDepth;
    if (!CaptureLinearDepth(GrassOnlyDepth))
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte could not read the exact show-only R32F grass depth pass.");
        return false;
    }
    CaptureComponent->ClearShowOnlyComponents();
    CaptureComponent->PrimitiveRenderMode =
        ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
    TArray<FLinearColor> FullSceneDepth;
    if (!CaptureLinearDepth(FullSceneDepth))
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte could not read the exact full-scene R32F occlusion depth pass.");
        return false;
    }

    TArray<FColor> MattePixels;
    MattePixels.SetNumUninitialized(CaptureWidth * CaptureHeight);
    int64 ForegroundPixelCount = 0;
    for (int32 PixelIndex = 0; PixelIndex < MattePixels.Num(); ++PixelIndex)
    {
        const float GrassDepth = GrassOnlyDepth[PixelIndex].R;
        const float SceneDepth = FullSceneDepth[PixelIndex].R;
        const bool bFiniteGrassDepth = FMath::IsFinite(GrassDepth) &&
            GrassDepth > 0.0f &&
            GrassDepth < MaximumGrassDepthCentimeters;
        const bool bVisibleGrass = bFiniteGrassDepth &&
            FMath::IsFinite(SceneDepth) && SceneDepth > 0.0f &&
            FMath::Abs(GrassDepth - SceneDepth) <=
                DepthEqualityEpsilonCentimeters;
        const uint8 Value = bVisibleGrass ? 255 : 0;
        MattePixels[PixelIndex] = FColor(Value, Value, Value, 255);
        ForegroundPixelCount += bVisibleGrass ? 1 : 0;
    }
    const int64 BackgroundPixelCount =
        static_cast<int64>(MattePixels.Num()) - ForegroundPixelCount;
    TArray64<uint8> CompressedPng;
    FImageUtils::PNGCompressImageArray(
        CaptureWidth,
        CaptureHeight,
        TArrayView64<const FColor>(
            MattePixels.GetData(),
            MattePixels.Num()),
        CompressedPng);
    if (CompressedPng.Num() <= 0)
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte could not encode its exact binary RGBA8 PNG.");
        return false;
    }

    CleanupTransientCapture();
    const int32 RegisteredCaptureComponentsAfter =
        CountRegisteredCaptureComponents();
    UWorld* WorldAfter = nullptr;
    APlayerController* PlayerAfter = nullptr;
    ATRIADIstanaExploreV5Pawn* PawnAfter = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* PolicyAfter = nullptr;
    ACesium3DTileset* TilesetAfter = nullptr;
    FString PostStateError;
    FVector PlayerViewLocationAfter = FVector::ZeroVector;
    FRotator PlayerViewRotationAfter = FRotator::ZeroRotator;
    if (Player)
    {
        Player->GetPlayerViewPoint(
            PlayerViewLocationAfter,
            PlayerViewRotationAfter);
    }
    FString GroundReportAfter;
    const bool bPostStateValid = GetValidatedHybridPlayState(
        WorldAfter,
        PlayerAfter,
        PawnAfter,
        PolicyAfter,
        TilesetAfter,
        PostStateError);
    if (!bPostStateValid || WorldAfter != World || PlayerAfter != Player ||
        PawnAfter != Pawn || PolicyAfter != Policy || TilesetAfter != Tileset ||
        RegisteredCaptureComponentsAfter != RegisteredCaptureComponentsBefore ||
        !GroundVegetation->ValidateGroundVegetationRealism(
            GroundReportAfter) ||
        GroundReportAfter != GroundReportBefore ||
        (PlayerViewLocationAfter - PlayerViewLocation).Size() >
            ExactQaViewLocationToleranceCentimeters ||
        !PlayerViewRotationAfter.Equals(
            PlayerViewRotation,
            ExactQaViewRotationToleranceDegrees))
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte failed its post-capture world/ground/view/transient-component preservation gate. ") +
            PostStateError + TEXT(" ") + GroundReportAfter;
        return false;
    }

    FString TemporaryDestination = Destination + TEXT(".tmp.") +
        FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT
    {
        if (!TemporaryDestination.IsEmpty())
        {
            IFileManager::Get().Delete(
                *TemporaryDestination,
                false,
                true,
                true);
        }
    };
    if (!FFileHelper::SaveArrayToFile(
            CompressedPng,
            *TemporaryDestination) ||
        IFileManager::Get().FileSize(*TemporaryDestination) <= 0 ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        !IFileManager::Get().Move(
            *Destination,
            *TemporaryDestination,
            false,
            false,
            false,
            true))
    {
        OutMessage = TEXT("Explore V5D R23 grass semantic matte could not atomically publish its no-overwrite PNG.");
        return false;
    }
    TemporaryDestination.Reset();

    OutMessage = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_GRASS_MATTE_CAPTURED method=TransientSceneCapture2DShowOnlyLinearDepthVisibleMask width=2560 height=1440 encoding=PNG_RGBA8_BINARY foreground=255 background=0 alpha=255 captureSource=SCS_SceneDepth primitiveRenderMode=PRM_UseShowOnlyList occlusion=GrassDepthEqualsSceneCaptureRenderableDepth occlusionScope=SceneCaptureDepthWritingPrimitivesAfterDeclaredSuppressions playerVisibleOcclusionComplete=false depthEqualityEpsilonCm=0.25 maxGrassDepthCm=5000 rowOrigin=TopLeftReadSurfaceOrder componentCount=5 instanceCount=18944 showOnlyComponents=5 grassPresentationRevision=R23 originalR23OpacityMaterials=true materialOverridesApplied=0 customDepthStencilApplied=0 taa=false projectionMatrixCopied=true cullingFovDegrees=%.6f projectionAspectRatio=%.9f cvarFoliageSplitFactor=16 cvarFoliageForceLod=-1 cvarFoliageOnlyLod=-1 cvarFoliageDisableCull=0 cvarFoliageCullAll=0 cvarFoliageDitheredLod=1 cvarFoliageOverestimateLod=0 cvarFoliageMaxTrianglesToRender=100000000 cvarFoliageMinVertsToSplitNode=8192 cvarFoliageMaxEndCullDistance=0 cvarFoliageMinLod=-1 cvarFoliageCullAllInVertexShader=0 cvarMeshStreaming=0 cvarSceneCaptureDepthPrepassOptimization=0 cvarFoliageMinimumScreenSize=%.9f cvarFoliageLodDistanceScale=%.6f cvarFoliageRandomLodRange=%.6f cachedViewDistanceScale=%.6f clusterTreesSynchronouslyRebuiltUnderPinnedCvars=true componentTransformsEqualDeterministicSavedLayout=true verifiedInstanceTransforms=18944 registeredSceneCaptureComponentsDeltaAfter=0 playerViewChanged=false strictZeroAtOrBeyondEnd=false foregroundPixelCount=%lld backgroundPixelCount=%lld hiddenLineBatchers=%d hiddenHumanOverlays=%d hiddenDemoTargets=%d component0=V5DGrassManicuredMicroClumps material0=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Manicured.M_IPV5D_Turf_Manicured instances0=12461 cullCm0=2600,3400 wpoDisableCm0=2400 lodScale0=0.60 component1=V5DGrassHumidMicroClumps material1=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Humid.M_IPV5D_Turf_Humid instances1=3975 cullCm1=2600,3400 wpoDisableCm1=2400 lodScale1=0.60 component2=V5DGrassShadeMicroClumps material2=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Shade.M_IPV5D_Turf_Shade instances2=1150 cullCm2=2600,3400 wpoDisableCm2=2400 lodScale2=0.60 component3=V5DGrassDryEdgeMicroClumps material3=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_DryEdge.M_IPV5D_Turf_DryEdge instances3=846 cullCm3=2600,3400 wpoDisableCm3=2400 lodScale3=0.60 component4=V5DEdgeGrassSeasonalAccents material4=/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_GrassMedium_EdgeFade.M_IPV5D_GrassMedium_EdgeFade instances4=512 cullCm4=3000,4500 wpoDisableCm4=2400 lodScale4=0.60 destination=%s"),
        CameraView->FOV,
        ProjectionAspectRatio,
        ValidatedFoliageMinimumScreenSize,
        ValidatedFoliageLodDistanceScale,
        ValidatedFoliageRandomLodRange,
        CachedViewDistanceScale,
        ForegroundPixelCount,
        BackgroundPixelCount,
        HiddenLineBatcherCount,
        HiddenHumanOverlayCount,
        HiddenDemoTargetCount,
        *Destination);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    CaptureIstanaExploreV5DHybridPublicRealmCoreHiddenDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage)
{
    constexpr TCHAR DiagnosticFilenamePrefix[] =
        TEXT("explore_v5d_diagnostic_public_realm_core_hidden_");
    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |= Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    FText FilenameReason;
    if (!OutputFileName.StartsWith(
            DiagnosticFilenamePrefix,
            ESearchCase::CaseSensitive) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        OutputFileName.Len() <=
            FCString::Strlen(DiagnosticFilenamePrefix) + 4 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5D public-realm-core-hidden diagnostic requires one safe exact explore_v5d_diagnostic_public_realm_core_hidden_*.png filename. ") +
            FilenameReason.ToString();
        return false;
    }

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    const bool bValidatedPlayState = GetValidatedHybridPlayState(
        World,
        Player,
        Pawn,
        Policy,
        Tileset,
        Error);
    const float LoadProgress = Tileset
        ? Tileset->GetLoadProgress()
        : -1.0f;
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    if (!bValidatedPlayState || !Tileset ||
        !FMath::IsFinite(LoadProgress) || LoadProgress < 98.0f ||
        LoadProgress > 100.0f ||
        !Policy || !Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !bStableVisualPolicy)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D non-proof public-realm-core-hidden diagnostic requires exact validated Player0, finite provider load >=98, hidden local fallback, and the stable clipping/authored-core policy tuple; loadProgress=%.3f hidden=%s aerialRequested=%s aerialActive=%s stableVisualPolicy=%s. %s"),
            LoadProgress,
            Policy && Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffRequested
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffCurrentlyActive
                ? TEXT("true")
                : TEXT("false"),
            bStableVisualPolicy ? TEXT("true") : TEXT("false"),
            *Error);
        return false;
    }

    int32 PublicRealmCount = 0;
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm =
        FindExactlyOne<ATRIADIstanaExploreV5DPublicRealmActor>(
            World,
            PublicRealmCount);
    UStaticMeshComponent* CorePublicRealm = PublicRealm
        ? PublicRealm->CorePublicRealmRenderOnly.Get()
        : nullptr;
    UStaticMeshComponent* FallbackPublicRealm = PublicRealm
        ? PublicRealm->FallbackPublicRealmRenderOnly.Get()
        : nullptr;
    const FName CoreComponentName(TEXT("CorePublicRealmRenderOnly"));
    const FName FallbackComponentName(TEXT("FallbackPublicRealmRenderOnly"));
    const FName CoreComponentTag(TEXT("TRIADV5DCorePublicRealmRenderOnly"));
    const FName FallbackComponentTag(
        TEXT("TRIADV5DFallbackPublicRealmRenderOnly"));
    const FName HumanOnlyOverlayTag(TEXT("TRIADHumanOnlyOverlay"));
    const auto IsExactRenderOnlyComponent =
        [&HumanOnlyOverlayTag](
            const UStaticMeshComponent* Component,
            const USceneComponent* ExpectedParent,
            const FName& ExpectedName,
            const FName& ExpectedTag,
            const bool bExpectedVisible) -> bool
    {
        return IsValid(Component) && Component->IsRegistered() &&
            Component->GetFName() == ExpectedName &&
            Component->GetAttachParent() == ExpectedParent &&
            Component->GetStaticMesh() &&
            Component->Mobility == EComponentMobility::Static &&
            Component->GetRelativeTransform().Equals(
                FTransform::Identity,
                0.001) &&
            Component->GetCollisionEnabled() ==
                ECollisionEnabled::NoCollision &&
            Component->GetCollisionResponseToChannels() ==
                FCollisionResponseContainer(ECR_Ignore) &&
            !Component->GetGenerateOverlapEvents() &&
            !Component->CanEverAffectNavigation() &&
            Component->bRenderInMainPass &&
            !Component->bRenderCustomDepth &&
            Component->bHiddenInSceneCapture &&
            Component->ComponentHasTag(ExpectedTag) &&
            !Component->ComponentHasTag(HumanOnlyOverlayTag) &&
            Component->IsVisible() == bExpectedVisible &&
            Component->bHiddenInGame != bExpectedVisible;
    };
    FString PublicRealmReport = TEXT("not evaluated");
    const bool bPublicRealmContractValid = IsValid(PublicRealm) &&
        PublicRealm->GetWorld() == World &&
        PublicRealm->ValidatePublicRealm(PublicRealmReport);
    const bool bCoreExactVisible = IsExactRenderOnlyComponent(
        CorePublicRealm,
        PublicRealm ? PublicRealm->SceneRoot.Get() : nullptr,
        CoreComponentName,
        CoreComponentTag,
        true);
    const bool bFallbackExactHidden = IsExactRenderOnlyComponent(
        FallbackPublicRealm,
        PublicRealm ? PublicRealm->SceneRoot.Get() : nullptr,
        FallbackComponentName,
        FallbackComponentTag,
        false);
    if (PublicRealmCount != 1 || !bPublicRealmContractValid ||
        !PublicRealm->bProviderReady || !bCoreExactVisible ||
        !bFallbackExactHidden)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D non-proof public-realm-core-hidden diagnostic refused a non-exact provider-ready public-realm renderer state; publicRealmCount=%d contractValid=%s providerReady=%s coreExactVisibleRegistered=%s fallbackExactHiddenRegistered=%s report={%s}."),
            PublicRealmCount,
            bPublicRealmContractValid ? TEXT("true") : TEXT("false"),
            PublicRealm && PublicRealm->bProviderReady
                ? TEXT("true")
                : TEXT("false"),
            bCoreExactVisible ? TEXT("true") : TEXT("false"),
            bFallbackExactHidden ? TEXT("true") : TEXT("false"),
            *PublicRealmReport);
        return false;
    }

    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Explore V5D non-proof public-realm-core-hidden diagnostic requires the validated SDR game viewport.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV5D"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5D non-proof public-realm-core-hidden diagnostic refused an unsafe directory, overwrite, or overlapping request.");
        return false;
    }

    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D non-proof public-realm-core-hidden diagnostic configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;

    int32 SuppressedLineBatcherCount = 0;
    int32 SuppressedHumanOverlayCount = 0;
    int32 SuppressedDemoTargetCount = 0;
    if (!BeginHybridProofViewSuppression(
            World,
            Player,
            SuppressedLineBatcherCount,
            SuppressedHumanOverlayCount,
            SuppressedDemoTargetCount))
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D non-proof public-realm-core-hidden diagnostic could not install its reversible Player0-only debug-visual filter.");
        return false;
    }
    int32 SuppressedCoreCount = 0;
    AddHybridProofHiddenComponent(
        Player,
        CorePublicRealm,
        SuppressedCoreCount);
    if (SuppressedCoreCount != 1)
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D non-proof public-realm-core-hidden diagnostic could not add exactly the validated core renderer to Player0's reversible hidden-primitive set.");
        return false;
    }
    if (!Viewport->TakeHighResScreenShot())
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D non-proof public-realm-core-hidden diagnostic game-viewport request.");
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("NON-PROOF PUBLIC-REALM CORE-HIDDEN DIAGNOSTIC: accepted exact 2560x1440 HDR-off provider-ready V5D Player0 public-realm-core-hidden attribution capture at cesiumLoadProgress=%.3f with localFallbackHidden=true aerialProviderHandoffRequested=%s aerialProviderHandoffActive=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s exactlyOnePublicRealm=true publicRealmCorePersistentVisibleRegistered=true publicRealmCorePlayer0Excluded=true publicRealmFallbackPersistentHiddenRegistered=true to '%s'; capture-only Player0 filter hides %d world line batchers, %d tagged human overlays, %d optional demo targets, plus exactly %d public-realm core until screenshot completion without changing component visibility, tick, collision, navigation, map, sensor, RF, or the normal proof path; wrapper MUST wait for and decode the PNG, then require successful post-screenshot GetIstanaExploreV5DHybridPlayStateReport and ValidateIstanaExploreV5DHybridPlayWorld readbacks before accepting this diagnostic or issuing another capture/teleport."),
        LoadProgress,
        Policy->bAerialProviderHandoffRequested ? TEXT("true") : TEXT("false"),
        Policy->bAerialProviderHandoffCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bProviderSiteClipCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible ? TEXT("true") : TEXT("false"),
        *Destination,
        SuppressedLineBatcherCount,
        SuppressedHumanOverlayCount,
        SuppressedDemoTargetCount,
        SuppressedCoreCount);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    CaptureIstanaExploreV5DHybridGroundMacroOverlayHiddenDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage)
{
    constexpr TCHAR DiagnosticFilenamePrefix[] =
        TEXT("explore_v5d_diagnostic_ground_macro_overlay_hidden_");
    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |= Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    FText FilenameReason;
    if (!OutputFileName.StartsWith(
            DiagnosticFilenamePrefix,
            ESearchCase::CaseSensitive) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        OutputFileName.Len() <=
            FCString::Strlen(DiagnosticFilenamePrefix) + 4 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5D ground-macro-overlay-hidden diagnostic requires one safe exact explore_v5d_diagnostic_ground_macro_overlay_hidden_*.png filename. ") +
            FilenameReason.ToString();
        return false;
    }

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    const bool bValidatedPlayState = GetValidatedHybridPlayState(
        World,
        Player,
        Pawn,
        Policy,
        Tileset,
        Error);
    const float LoadProgress = Tileset
        ? Tileset->GetLoadProgress()
        : -1.0f;
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    if (!bValidatedPlayState || !Tileset ||
        !FMath::IsFinite(LoadProgress) || LoadProgress < 98.0f ||
        LoadProgress > 100.0f ||
        !Policy || !Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !bStableVisualPolicy)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D non-proof ground-macro-overlay-hidden diagnostic requires exact validated Player0, finite provider load >=98, hidden local fallback, and the stable clipping/authored-core policy tuple; loadProgress=%.3f hidden=%s aerialRequested=%s aerialActive=%s stableVisualPolicy=%s. %s"),
            LoadProgress,
            Policy && Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffRequested
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffCurrentlyActive
                ? TEXT("true")
                : TEXT("false"),
            bStableVisualPolicy ? TEXT("true") : TEXT("false"),
            *Error);
        return false;
    }

    int32 GroundVegetationCount = 0;
    ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            World,
            GroundVegetationCount);
    UStaticMeshComponent* GroundMacroOverlay = GroundVegetation
        ? GroundVegetation->GroundMacroVariationOverlay.Get()
        : nullptr;
    ATRIADIstanaPublicViewSceneActor* PublicViewScene = GroundVegetation
        ? GroundVegetation->PublicViewSceneActor.Get()
        : nullptr;
    UStaticMeshComponent* SourceTerrain = PublicViewScene
        ? PublicViewScene->TerrainComponent.Get()
        : nullptr;
    const FName GroundMacroOverlayName(
        TEXT("V5DGroundMacroVariationOverlay"));
    const FName SourceTerrainName(TEXT("TerrainVisualCollision"));
    FString GroundVegetationReport = TEXT("not evaluated");
    const bool bGroundVegetationContractValid = IsValid(GroundVegetation) &&
        GroundVegetation->GetWorld() == World &&
        GroundVegetation->ValidateGroundVegetationRealism(
            GroundVegetationReport);
    const bool bGroundMacroOverlayExactVisibleRegistered =
        IsValid(GroundMacroOverlay) && GroundMacroOverlay->IsRegistered() &&
        GroundMacroOverlay->GetWorld() == World &&
        GroundMacroOverlay->GetOwner() == GroundVegetation &&
        GroundMacroOverlay->GetFName() == GroundMacroOverlayName &&
        GroundMacroOverlay->GetStaticMesh() &&
        GroundMacroOverlay->GetStaticMesh() ==
            GroundVegetation->SavedAssets.GroundOverlayMesh &&
        GroundMacroOverlay->GetMaterial(0) &&
        GroundMacroOverlay->GetMaterial(0) ==
            GroundVegetation->SavedAssets.GroundOverlayMaterial &&
        GroundMacroOverlay->Mobility == EComponentMobility::Movable &&
        GroundMacroOverlay->GetCollisionEnabled() ==
            ECollisionEnabled::NoCollision &&
        GroundMacroOverlay->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore) &&
        !GroundMacroOverlay->GetGenerateOverlapEvents() &&
        !GroundMacroOverlay->CanEverAffectNavigation() &&
        GroundMacroOverlay->bRenderInMainPass &&
        !GroundMacroOverlay->bRenderCustomDepth &&
        GroundMacroOverlay->IsVisible() &&
        !GroundMacroOverlay->bHiddenInGame &&
        !GroundMacroOverlay->CastShadow &&
        !GroundMacroOverlay->bCastContactShadow &&
        !GroundMacroOverlay->bAffectDistanceFieldLighting;
    const bool bSourceTerrainExactHiddenCollisionPreserved =
        IsValid(PublicViewScene) && PublicViewScene->GetWorld() == World &&
        IsValid(SourceTerrain) && SourceTerrain->IsRegistered() &&
        SourceTerrain->GetWorld() == World &&
        SourceTerrain->GetOwner() == PublicViewScene &&
        SourceTerrain->GetFName() == SourceTerrainName &&
        !SourceTerrain->IsVisible() && SourceTerrain->bHiddenInGame &&
        SourceTerrain->GetCollisionEnabled() ==
            ECollisionEnabled::QueryAndPhysics;
    if (GroundVegetationCount != 1 ||
        !bGroundVegetationContractValid ||
        !GroundVegetation->bSourceTerrainRendererHiddenForReadyProvider ||
        !bGroundMacroOverlayExactVisibleRegistered ||
        !bSourceTerrainExactHiddenCollisionPreserved)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D non-proof ground-macro-overlay-hidden diagnostic refused a non-exact provider-ready ground renderer state; groundVegetationCount=%d contractValid=%s sourceTerrainPolicyHidden=%s overlayExactVisibleRegistered=%s sourceTerrainExactHiddenCollisionPreserved=%s report={%s}."),
            GroundVegetationCount,
            bGroundVegetationContractValid ? TEXT("true") : TEXT("false"),
            GroundVegetation &&
                    GroundVegetation->bSourceTerrainRendererHiddenForReadyProvider
                ? TEXT("true")
                : TEXT("false"),
            bGroundMacroOverlayExactVisibleRegistered
                ? TEXT("true")
                : TEXT("false"),
            bSourceTerrainExactHiddenCollisionPreserved
                ? TEXT("true")
                : TEXT("false"),
            *GroundVegetationReport);
        return false;
    }

    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Explore V5D non-proof ground-macro-overlay-hidden diagnostic requires the validated SDR game viewport.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV5D"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5D non-proof ground-macro-overlay-hidden diagnostic refused an unsafe directory, overwrite, or overlapping request.");
        return false;
    }

    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D non-proof ground-macro-overlay-hidden diagnostic configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;

    int32 SuppressedLineBatcherCount = 0;
    int32 SuppressedHumanOverlayCount = 0;
    int32 SuppressedDemoTargetCount = 0;
    if (!BeginHybridProofViewSuppression(
            World,
            Player,
            SuppressedLineBatcherCount,
            SuppressedHumanOverlayCount,
            SuppressedDemoTargetCount))
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D non-proof ground-macro-overlay-hidden diagnostic could not install its reversible Player0-only debug-visual filter.");
        return false;
    }
    int32 SuppressedGroundMacroOverlayCount = 0;
    AddHybridProofHiddenComponent(
        Player,
        GroundMacroOverlay,
        SuppressedGroundMacroOverlayCount);
    if (SuppressedGroundMacroOverlayCount != 1)
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D non-proof ground-macro-overlay-hidden diagnostic could not add exactly the validated overlay renderer to Player0's reversible hidden-primitive set.");
        return false;
    }
    if (!Viewport->TakeHighResScreenShot())
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D non-proof ground-macro-overlay-hidden diagnostic game-viewport request.");
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("NON-PROOF GROUND-MACRO-OVERLAY-HIDDEN DIAGNOSTIC: accepted exact 2560x1440 HDR-off provider-ready V5D Player0 ground-macro-overlay-hidden attribution capture at cesiumLoadProgress=%.3f with localFallbackHidden=true aerialProviderHandoffRequested=%s aerialProviderHandoffActive=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s exactlyOneGroundVegetation=true groundMacroOverlayPersistentVisibleRegistered=true groundMacroOverlayPlayer0Excluded=true sourceTerrainRendererPersistentHidden=true sourceTerrainCollisionQueryAndPhysics=true to '%s'; capture-only Player0 filter hides %d world line batchers, %d tagged human overlays, %d optional demo targets, plus exactly %d ground macro overlay until screenshot completion without changing component visibility, tick, collision, navigation, map, simulation, sensor, or RF state or the normal proof path; wrapper MUST wait for and decode the PNG and screenshot-restoration completion, then require successful post-screenshot GetIstanaExploreV5DHybridPlayStateReport and ValidateIstanaExploreV5DHybridPlayWorld readbacks before accepting this diagnostic or issuing another capture/teleport."),
        LoadProgress,
        Policy->bAerialProviderHandoffRequested ? TEXT("true") : TEXT("false"),
        Policy->bAerialProviderHandoffCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bProviderSiteClipCurrentlyActive ? TEXT("true") : TEXT("false"),
        Policy->bAuthoredCoreVisualsCurrentlyVisible ? TEXT("true") : TEXT("false"),
        *Destination,
        SuppressedLineBatcherCount,
        SuppressedHumanOverlayCount,
        SuppressedDemoTargetCount,
        SuppressedGroundMacroOverlayCount);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    CaptureIstanaExploreV5DHybridDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage)
{
    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |= Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    constexpr TCHAR DiagnosticFilenamePrefix[] =
        TEXT("explore_v5d_diagnostic_");
    FText FilenameReason;
    if (!OutputFileName.StartsWith(
            DiagnosticFilenamePrefix,
            ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        OutputFileName.Len() <=
            FCString::Strlen(DiagnosticFilenamePrefix) + 4 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5D diagnostic capture requires one safe exact explore_v5d_diagnostic_*.png filename. ") +
            FilenameReason.ToString();
        return false;
    }

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    const bool bValidatedPlayState = GetValidatedHybridPlayState(
        World,
        Player,
        Pawn,
        Policy,
        Tileset,
        Error);
    const float LoadProgress = Tileset
        ? Tileset->GetLoadProgress()
        : -1.0f;
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    if (!bValidatedPlayState || !Tileset ||
        !FMath::IsFinite(LoadProgress) || LoadProgress < 0.0f ||
        LoadProgress > 100.0f || !bStableVisualPolicy)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D non-proof diagnostic capture requires exact validated Player0, the stable clipping/authored-core policy tuple, and finite provider progress; loadProgress=%.3f localFallbackVisible=%s providerSiteClipActive=%s authoredCoreVisualsVisible=%s aerialRequested=%s aerialActive=%s stableVisualPolicy=%s. %s"),
            LoadProgress,
            Policy && !Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bProviderSiteClipCurrentlyActive
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAuthoredCoreVisualsCurrentlyVisible
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffRequested
                ? TEXT("true")
                : TEXT("false"),
            Policy && Policy->bAerialProviderHandoffCurrentlyActive
                ? TEXT("true")
                : TEXT("false"),
            bStableVisualPolicy ? TEXT("true") : TEXT("false"),
            *Error);
        return false;
    }

    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Explore V5D non-proof diagnostic capture requires the validated SDR game viewport.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV5D"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5D non-proof diagnostic capture refused an unsafe directory, overwrite, or overlapping request.");
        return false;
    }
    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D non-proof diagnostic-capture configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;
    int32 SuppressedLineBatcherCount = 0;
    int32 SuppressedHumanOverlayCount = 0;
    int32 SuppressedDemoTargetCount = 0;
    if (!BeginHybridProofViewSuppression(
            World,
            Player,
            SuppressedLineBatcherCount,
            SuppressedHumanOverlayCount,
            SuppressedDemoTargetCount))
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D non-proof diagnostic capture could not install its reversible Player0-only debug-visual filter.");
        return false;
    }
    if (!Viewport->TakeHighResScreenShot())
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D non-proof diagnostic game-viewport request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("NON-PROOF DIAGNOSTIC EVIDENCE: accepted exact 2560x1440 HDR-off V5D Player0 capture with cesiumLoadProgress=%.3f localFallbackVisible=%s localFallbackHidden=%s authoredCoreVisualsVisible=true aerialProviderHandoffRequested=false aerialProviderHandoffActive=false groundHeight=true to '%s'; capture-only Player0 filter hides %d world line batchers, %d tagged human overlays, and %d optional demo targets until screenshot completion without changing RF/simulation; wrapper must wait for and decode the PNG."),
        LoadProgress,
        Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("false")
            : TEXT("true"),
        Policy->bLocalBuildingFallbackCurrentlyHidden
            ? TEXT("true")
            : TEXT("false"),
        *Destination,
        SuppressedLineBatcherCount,
        SuppressedHumanOverlayCount,
        SuppressedDemoTargetCount);
    return true;
}

bool UTRIADIstanaExploreV5DHybridEditorLibrary::
    CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView(
        const FString& OutputFileName,
        FString& OutMessage)
{
    bool bHasControlCharacter = false;
    for (const TCHAR Character : OutputFileName)
    {
        bHasControlCharacter |= Character < static_cast<TCHAR>(0x20) ||
            Character == static_cast<TCHAR>(0x7f);
    }
    constexpr TCHAR DiagnosticFilenamePrefix[] =
        TEXT("explore_v5d_diagnostic_");
    FText FilenameReason;
    if (!OutputFileName.StartsWith(
            DiagnosticFilenamePrefix,
            ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        OutputFileName.Len() <=
            FCString::Strlen(DiagnosticFilenamePrefix) + 4 ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) ||
        bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5D R28 diagnostic capture requires one safe exact explore_v5d_diagnostic_*.png filename. ") +
            FilenameReason.ToString();
        return false;
    }

    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ACesium3DTileset* Tileset = nullptr;
    FString Error;
    FString R28WorldReport;
    const bool bValidatedPlayState = GetValidatedHybridPlayState(
        World,
        Player,
        Pawn,
        Policy,
        Tileset,
        Error,
        true,
        &R28WorldReport);
    FString R28Player0Report;
    const bool bR28Player0Valid = bValidatedPlayState &&
        ValidateR28Player0Presentation(
            World,
            Policy,
            true,
            R28Player0Report);
    const float LoadProgress = Tileset
        ? Tileset->GetLoadProgress()
        : -1.0f;
    const bool bStableVisualPolicy = HasStableHybridVisualPolicy(Policy);
    if (!bValidatedPlayState || !bR28Player0Valid || !Tileset ||
        !FMath::IsFinite(LoadProgress) || LoadProgress < 0.0f ||
        LoadProgress > 100.0f || !Policy ||
        Policy->bLocalBuildingFallbackCurrentlyHidden ||
        !bStableVisualPolicy)
    {
        OutMessage = FString::Printf(
            TEXT("Explore V5D R28 non-proof diagnostic capture requires the combined R28 world contract, exact Player0, visible local fallback/R28 environment, stable clipping/authored-core policy, and finite provider progress; loadProgress=%.3f localFallbackVisible=%s stableVisualPolicy=%s. %s %s"),
            LoadProgress,
            Policy && !Policy->bLocalBuildingFallbackCurrentlyHidden
                ? TEXT("true")
                : TEXT("false"),
            bStableVisualPolicy ? TEXT("true") : TEXT("false"),
            *Error,
            *R28Player0Report);
        return false;
    }

    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Explore V5D R28 non-proof diagnostic capture requires the validated SDR game viewport.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/IstanaPreviews/ExploreV5D"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5D R28 non-proof diagnostic capture refused an unsafe directory, overwrite, or overlapping request.");
        return false;
    }
    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(2560, 1440, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D R28 diagnostic-capture configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;
    int32 SuppressedLineBatcherCount = 0;
    int32 SuppressedHumanOverlayCount = 0;
    int32 SuppressedDemoTargetCount = 0;
    if (!BeginHybridProofViewSuppression(
            World,
            Player,
            SuppressedLineBatcherCount,
            SuppressedHumanOverlayCount,
            SuppressedDemoTargetCount))
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("Explore V5D R28 non-proof diagnostic capture could not install its reversible Player0-only debug-visual filter.");
        return false;
    }
    if (!Viewport->TakeHighResScreenShot())
    {
        RestoreHybridProofViewSuppression();
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact 2560x1440 V5D R28 non-proof diagnostic game-viewport request.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("R28 VISUAL SUCCESSOR DIAGNOSTIC EVIDENCE: accepted exact 2560x1440 HDR-off V5D Player0 capture with cesiumLoadProgress=%.3f localFallbackVisible=true localFallbackHidden=false r28VisualSuccessor=true r28MapWorldValidated=true r28EnvironmentPlayer0Visible=true r28EnvironmentSceneCaptureSensorExcluded=true r28EnvironmentRenderOnly=true r28ProviderNegativeAuthority=true to '%s'; capture-only Player0 filter hides %d world line batchers, %d tagged human overlays, and %d optional demo targets until screenshot completion without changing terrain/collision/navigation/sensor/RF authority; r28World={%s} r28Player0={%s}; wrapper must wait for and decode the PNG."),
        LoadProgress,
        *Destination,
        SuppressedLineBatcherCount,
        SuppressedHumanOverlayCount,
        SuppressedDemoTargetCount,
        *R28WorldReport,
        *R28Player0Report);
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DHybridBuildAndValidateTest,
    "TRIAD.Istana.ExploreV5D.Hybrid.BuildAndValidate",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DHybridBuildAndValidateTest::RunTest(
    const FString& Parameters)
{
    FString Message;
    if (!UTRIADIstanaExploreV5DHybridEditorLibrary::
            BuildIstanaExploreV5DHybridMap(Message))
    {
        AddError(Message);
        return false;
    }
    AddInfo(Message);
    FString Report;
    if (!UTRIADIstanaExploreV5DHybridEditorLibrary::
            ValidateIstanaExploreV5DHybridMap(Report))
    {
        AddError(Report);
        return false;
    }
    AddInfo(Report);
    return true;
}
#endif
