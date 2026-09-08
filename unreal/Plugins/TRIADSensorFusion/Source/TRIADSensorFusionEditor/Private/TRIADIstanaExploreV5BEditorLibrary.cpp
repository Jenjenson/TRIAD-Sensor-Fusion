#include "TRIADIstanaExploreV5BEditorLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "ScopedTransaction.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV2LandscapeActor.h"
#include "TRIADIstanaExploreV3SupplementActor.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5AppearanceActor.h"
#include "TRIADIstanaExploreV5EditorLibrary.h"
#include "TRIADIstanaExploreV5BAssetFactory.h"
#include "TRIADIstanaExploreV5BVisualActor.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaExploreV5Pawn.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UnrealClient.h"
#include "UObject/Package.h"

namespace
{
const FString SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v5"));
const FString DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v5b"));
const FString DestinationMapObjectPath(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5b.Istana_PublicView_Explore_v5b"));
constexpr int64 ExpectedSourceMapBytes = 12158126;
const FString ExactCollisionMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Building/SM_IstanaPublicView_Building_Collision.SM_IstanaPublicView_Building_Collision"));
const FString V5BGameModeClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreV5GameMode"));
const FName V5BActorTag(TEXT("TRIADIstanaExploreVisualsV5B"));

template <typename TObjectType>
TObjectType* LoadExact(const FString& ObjectPath)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

FString FileMd5(const FString& Filename)
{
    const FMD5Hash Hash = FMD5Hash::HashFile(*Filename);
    return Hash.IsValid() ? LexToString(Hash).ToUpper() : FString();
}

bool ResolvePersistedPackageFile(
    const FString& ObjectPath,
    FString& OutPackageName,
    FString& OutFilename,
    FString& OutError)
{
    OutPackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
    if (OutPackageName.IsEmpty() ||
        !FPackageName::DoesPackageExist(OutPackageName, &OutFilename) ||
        IFileManager::Get().FileSize(*OutFilename) <= 0)
    {
        OutError = TEXT("Could not resolve the persisted package for exact object: ") +
            ObjectPath;
        return false;
    }
    OutError.Reset();
    return true;
}

bool SnapshotOtherV5BPackageHashes(
    TMap<FString, FString>& OutHashes,
    FString& OutError)
{
    OutHashes.Reset();
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets())
    {
        Registry.Get().WaitForCompletion();
    }
    TArray<FAssetData> Assets;
    Registry.Get().GetAssetsByPath(
        FName(*TRIADIstanaExploreV5BAssetFactory::GetAssetRootPath()),
        Assets,
        true,
        false);
    Assets.RemoveAll([](const FAssetData& Asset)
    {
        return TRIADIstanaExploreV5BAssetFactory::
            IsExactUnpersistedAccentTurfLodImportScratch(Asset);
    });
    if (Assets.Num() != 37)
    {
        OutError = FString::Printf(
            TEXT("The accent-turf transaction requires 37 V5B assets; actual=%d."),
            Assets.Num());
        return false;
    }
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.GetObjectPathString() ==
            TRIADIstanaExploreV5BAssetFactory::GetAccentTurfMeshObjectPath())
        {
            continue;
        }
        FString Filename;
        if (!FPackageName::DoesPackageExist(
                Asset.PackageName.ToString(), &Filename))
        {
            OutError = TEXT("A preserved V5B package is not persisted: ") +
                Asset.PackageName.ToString();
            return false;
        }
        const FString Hash = FileMd5(Filename);
        if (Hash.IsEmpty())
        {
            OutError = TEXT("Could not hash preserved V5B package: ") + Filename;
            return false;
        }
        OutHashes.Add(Filename, Hash);
    }
    if (OutHashes.Num() != 36)
    {
        OutError = FString::Printf(
            TEXT("The accent-turf transaction captured %d preserved package hashes instead of 36."),
            OutHashes.Num());
        return false;
    }
    OutError.Reset();
    return true;
}

bool HashSnapshotsMatch(
    const TMap<FString, FString>& Before,
    const TMap<FString, FString>& After,
    FString& OutError)
{
    if (Before.Num() != 36 || After.Num() != 36)
    {
        OutError = TEXT("The preserved V5B package-hash census changed.");
        return false;
    }
    for (const TPair<FString, FString>& Pair : Before)
    {
        const FString* Actual = After.Find(Pair.Key);
        if (!Actual || *Actual != Pair.Value)
        {
            OutError = TEXT("A non-accent V5B package changed during the scoped upgrade: ") +
                Pair.Key;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

struct FAccentTurfDiskBackup
{
    FString Directory;
    FString PackageFilename;
    TMap<FString, FString> BackupByOriginal;
    TMap<FString, FString> OriginalHashes;
};

TArray<FString> PackageArtifactCandidates(const FString& PackageFilename)
{
    const FString Base = FPaths::Combine(
        FPaths::GetPath(PackageFilename),
        FPaths::GetBaseFilename(PackageFilename));
    return {
        Base + TEXT(".uasset"),
        Base + TEXT(".uexp"),
        Base + TEXT(".ubulk"),
        Base + TEXT(".uptnl"),
        Base + TEXT(".m.ubulk"),
        Base + TEXT(".upayload")};
}

bool BackUpAccentTurfPackage(
    const FString& PackageFilename,
    FAccentTurfDiskBackup& OutBackup,
    FString& OutError)
{
    OutBackup = FAccentTurfDiskBackup();
    OutBackup.PackageFilename = PackageFilename;
    OutBackup.Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/Backups/V5B_AccentTurfR11"),
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%f")));
    if (!IFileManager::Get().MakeDirectory(*OutBackup.Directory, true))
    {
        OutError = TEXT("Could not create the accent-turf backup directory: ") +
            OutBackup.Directory;
        return false;
    }
    const TArray<FString> ArtifactCandidates =
        PackageArtifactCandidates(PackageFilename);
    if (ArtifactCandidates.Num() != 6)
    {
        OutError = TEXT("The accent-turf backup requires exactly six canonical package-artifact candidates.");
        return false;
    }
    for (const FString& Original : ArtifactCandidates)
    {
        if (IFileManager::Get().FileSize(*Original) < 0)
        {
            continue;
        }
        const FString OriginalHash = FileMd5(Original);
        const FString Backup =
            FPaths::Combine(OutBackup.Directory, FPaths::GetCleanFilename(Original));
        if (OriginalHash.IsEmpty() ||
            IFileManager::Get().Copy(*Backup, *Original, true, true) != COPY_OK ||
            FileMd5(Backup) != OriginalHash)
        {
            OutError = TEXT("Could not make an exact accent-turf package backup: ") +
                Original;
            return false;
        }
        OutBackup.BackupByOriginal.Add(Original, Backup);
        OutBackup.OriginalHashes.Add(Original, OriginalHash);
    }
    if (!OutBackup.BackupByOriginal.Contains(PackageFilename))
    {
        OutError = TEXT("The canonical accent-turf .uasset was not backed up.");
        return false;
    }
    FString Receipt = TEXT("TRIAD_V5B_ACCENT_TURF_R11_BACKUP_V1\nartifactCandidates=6\n");
    for (const FString& Original : ArtifactCandidates)
    {
        if (const FString* Backup = OutBackup.BackupByOriginal.Find(Original))
        {
            Receipt += FString::Printf(
                TEXT("original=%s\nstate=PRESENT\nbackup=%s\nmd5=%s\n"),
                *Original,
                **Backup,
                *OutBackup.OriginalHashes[Original]);
        }
        else
        {
            Receipt += FString::Printf(
                TEXT("original=%s\nstate=ABSENT\nrollback=DELETE_IF_PRESENT\n"),
                *Original);
        }
    }
    if (!FFileHelper::SaveStringToFile(
            Receipt,
            *FPaths::Combine(OutBackup.Directory, TEXT("backup.receipt.txt")),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = TEXT("Could not persist the accent-turf backup receipt.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreAccentTurfPackage(
    const FAccentTurfDiskBackup& Backup,
    UPackage* Package,
    FString& OutError)
{
    bool bCopySucceeded = true;
    const TArray<FString> ArtifactCandidates =
        PackageArtifactCandidates(Backup.PackageFilename);
    if (ArtifactCandidates.Num() != 6)
    {
        OutError = TEXT("Automatic accent-turf rollback requires exactly six package-artifact candidates.");
        return false;
    }
    for (const FString& Original : ArtifactCandidates)
    {
        const FString* BackupFile = Backup.BackupByOriginal.Find(Original);
        if (BackupFile)
        {
            bCopySucceeded &=
                IFileManager::Get().Copy(*Original, **BackupFile, true, true) ==
                COPY_OK;
        }
        else if (IFileManager::Get().FileSize(*Original) >= 0)
        {
            bCopySucceeded &= IFileManager::Get().Delete(*Original, false, true);
        }
    }
    if (!bCopySucceeded)
    {
        OutError = TEXT("Automatic accent-turf rollback could not restore every package artifact. Backup: ") +
            Backup.Directory;
        return false;
    }
    // Never ask Unreal to deserialize bytes until the complete on-disk package
    // artifact roster proves it is exactly the state captured by the backup.
    for (const FString& Original : ArtifactCandidates)
    {
        const FString* ExpectedHash = Backup.OriginalHashes.Find(Original);
        if (ExpectedHash)
        {
            if (FileMd5(Original) != *ExpectedHash)
            {
                OutError = TEXT("Accent-turf rollback pre-reload hash verification failed: ") +
                    Original + TEXT(" Backup: ") + Backup.Directory;
                return false;
            }
        }
        else if (IFileManager::Get().FileSize(*Original) >= 0)
        {
            OutError = TEXT("Accent-turf rollback pre-reload absence verification failed: ") +
                Original + TEXT(" Backup: ") + Backup.Directory;
            return false;
        }
    }
    if (Package)
    {
        Package->SetDirtyFlag(false);
        FText ReloadError;
        const TArray<UPackage*> Packages = {Package};
        if (!UPackageTools::ReloadPackages(
                Packages,
                ReloadError,
                EReloadPackagesInteractionMode::AssumePositive))
        {
            OutError = TEXT("Accent-turf disk rollback succeeded but package reload failed: ") +
                ReloadError.ToString() + TEXT(" Backup: ") + Backup.Directory;
            return false;
        }
    }
    for (const FString& Original : ArtifactCandidates)
    {
        const FString* ExpectedHash = Backup.OriginalHashes.Find(Original);
        if ((ExpectedHash && FileMd5(Original) != *ExpectedHash) ||
            (!ExpectedHash && IFileManager::Get().FileSize(*Original) >= 0))
        {
            OutError = TEXT("Accent-turf rollback post-reload artifact verification failed: ") +
                Original + TEXT(" Backup: ") + Backup.Directory;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

template <typename TActorType>
TActorType* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    TActorType* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<TActorType> It(World); It; ++It)
    {
        Result = *It;
        ++OutCount;
    }
    return OutCount == 1 ? Result : nullptr;
}

struct FV5BTouchedPackageDirtyFlag
{
    UPackage* Package = nullptr;
    bool bWasDirty = false;
};

TArray<FV5BTouchedPackageDirtyFlag> CaptureTouchedPackageDirtyFlags(
    const TArray<UObject*>& Objects)
{
    TArray<FV5BTouchedPackageDirtyFlag> Result;
    for (UObject* Object : Objects)
    {
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (!Package || Result.ContainsByPredicate(
                [Package](const FV5BTouchedPackageDirtyFlag& State)
                {
                    return State.Package == Package;
                }))
        {
            continue;
        }
        FV5BTouchedPackageDirtyFlag& State = Result.AddDefaulted_GetRef();
        State.Package = Package;
        State.bWasDirty = Package->IsDirty();
    }
    return Result;
}

void RestoreTouchedPackageDirtyFlags(
    const TArray<FV5BTouchedPackageDirtyFlag>& States)
{
    for (const FV5BTouchedPackageDirtyFlag& State : States)
    {
        if (State.Package)
        {
            State.Package->SetDirtyFlag(State.bWasDirty);
        }
    }
}

bool SourceMapFileIsPresent(FString& OutFilename, FString& OutError)
{
    OutFilename.Reset();
    if (!FPackageName::DoesPackageExist(SourceMapPackage, &OutFilename))
    {
        OutError = TEXT("The exact Explore V5 source map package is absent.");
        return false;
    }
    const int64 Bytes = IFileManager::Get().FileSize(*OutFilename);
    if (Bytes != ExpectedSourceMapBytes)
    {
        OutError = FString::Printf(
            TEXT("Explore V5 source map byte guard failed: expected %lld, actual %lld."),
            ExpectedSourceMapBytes,
            Bytes);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSceneV5BHeroBoundary(
    ATRIADIstanaPublicViewSceneActor* Scene,
    bool bRequireRuntime,
    FString& OutError)
{
    UStaticMeshComponent* Hero = Scene ? Scene->BuildingHeroVisualComponent.Get() : nullptr;
    UStaticMeshComponent* Collision = Scene ? Scene->BuildingCollisionComponent.Get() : nullptr;
    UStaticMesh* ExpectedHero = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetPorticoMeshObjectPath());
    if (!Scene || !Hero || !Collision || !ExpectedHero ||
        Hero->GetStaticMesh() != ExpectedHero ||
        Hero->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Hero->GetAttachParent() != Scene->SceneRoot ||
        !Hero->GetRelativeTransform().Equals(FTransform::Identity, 0.0) ||
        (!bRequireRuntime && Hero->OverrideMaterials.Num() != 0) ||
        !Hero->LightingChannels.bChannel0 ||
        Hero->LightingChannels.bChannel2 ||
        Hero->LightingChannels.bChannel1 != bRequireRuntime ||
        !Collision->GetStaticMesh() ||
        Collision->GetStaticMesh()->GetPathName() != ExactCollisionMeshPath ||
        Collision->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics ||
        Collision->IsVisible() || !Collision->bHiddenInGame ||
        Collision->GetAttachParent() != Scene->SceneRoot ||
        !Collision->GetRelativeTransform().Equals(FTransform::Identity, 0.0))
    {
        OutError = TEXT("V5B must rebind only the identity render hero; the exact inherited collision component remains hidden/query-and-physics with its original mesh.");
        return false;
    }
    const TSet<int32> RuntimeFacadeSlots = {1, 6, 7, 8, 9, 10};
    for (int32 Slot = 0; Slot < Hero->GetNumMaterials(); ++Slot)
    {
        if (!Hero->GetMaterial(Slot) ||
            (!bRequireRuntime || !RuntimeFacadeSlots.Contains(Slot)) &&
                Hero->GetMaterial(Slot) != ExpectedHero->GetMaterial(Slot))
        {
            OutError = TEXT("V5B building hero effective materials must come only from the exact trimmed mesh's inherited eleven-slot binding.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateV5BWorld(UWorld* World, bool bRequireRuntime, FString& OutReport)
{
    const FString LogicalPackage = World && World->GetOutermost()
        ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())
        : FString();
    if (!World || !World->PersistentLevel ||
        LogicalPackage != DestinationMapPackage ||
        ATRIADIstanaExploreV5GameMode::StaticClass()->GetPathName() !=
            V5BGameModeClassPath ||
        World->GetWorldSettings()->DefaultGameMode !=
            ATRIADIstanaExploreV5GameMode::StaticClass())
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5B_WORLD_INVALID: exact package, persistent level, or inherited V5 game mode is absent.");
        return false;
    }

    int32 SceneCount = 0;
    int32 V2Count = 0;
    int32 V3Count = 0;
    int32 V4Count = 0;
    int32 V5Count = 0;
    int32 V5BCount = 0;
    int32 RuntimePolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    FindExactlyOne<ATRIADIstanaExploreV2LandscapeActor>(World, V2Count);
    FindExactlyOne<ATRIADIstanaExploreV3SupplementActor>(World, V3Count);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    ATRIADIstanaExploreV5AppearanceActor* V5 =
        FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(World, V5Count);
    ATRIADIstanaExploreV5BVisualActor* V5B =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(World, V5BCount);
    ATRIADIstanaPublicViewRuntimePolicyActor* RuntimePolicy =
        FindExactlyOne<ATRIADIstanaPublicViewRuntimePolicyActor>(
            World, RuntimePolicyCount);
    FString SceneReport;
    FString V4Report;
    FString V5Report;
    FString V5BReport;
    FString BoundaryError;
    if (SceneCount != 1 || V2Count != 1 || V3Count != 1 || V4Count != 1 ||
        V5Count != 1 || V5BCount != 1 || RuntimePolicyCount != 1 ||
        !Scene || !V4 || !V5 || !V5B || !RuntimePolicy ||
        !RuntimePolicy->bRequireIstanaAirSimGameMode ||
        RuntimePolicy->bEnforceFixedPrimaryCamera ||
        RuntimePolicy->RequiredGameModeClassPath != V5BGameModeClassPath ||
        (bRequireRuntime &&
         (!RuntimePolicy->HasActorBegunPlay() ||
          !RuntimePolicy->bRuntimePolicySettledAtRuntime ||
          !RuntimePolicy->bGameModeOverrideVerifiedAtRuntime ||
          !World->GetAuthGameMode() ||
          World->GetAuthGameMode()->GetClass() !=
              ATRIADIstanaExploreV5GameMode::StaticClass())) ||
        V5B->GetActorTransform().Equals(FTransform::Identity, 0.0) == false ||
        !V5B->Tags.Contains(V5BActorTag) ||
        !Scene->ValidatePublicViewScene(SceneReport, false) ||
        !V4->ValidateExploreV4Landscape(V4Report) ||
        !V5->ValidateExploreV5Appearance(V5Report, bRequireRuntime) ||
        !V5B->ValidateExploreV5BVisuals(V5BReport) ||
        (bRequireRuntime && !V5B->ValidateRuntimeFacadePresentation(BoundaryError)) ||
        !ValidateSceneV5BHeroBoundary(Scene, bRequireRuntime, BoundaryError))
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5B_WORLD_INVALID roster Scene/V2/V3/V4/V5/V5B/RuntimePolicy=%d/%d/%d/%d/%d/%d/%d; policyPath='%s' settled=%s gameModeVerified=%s. %s %s %s %s %s"),
            SceneCount,
            V2Count,
            V3Count,
            V4Count,
            V5Count,
            V5BCount,
            RuntimePolicyCount,
            RuntimePolicy ? *RuntimePolicy->RequiredGameModeClassPath : TEXT("<absent>"),
            RuntimePolicy && RuntimePolicy->bRuntimePolicySettledAtRuntime
                ? TEXT("true")
                : TEXT("false"),
            RuntimePolicy && RuntimePolicy->bGameModeOverrideVerifiedAtRuntime
                ? TEXT("true")
                : TEXT("false"),
            *SceneReport,
            *V4Report,
            *V5Report,
            *V5BReport,
            *BoundaryError);
        return false;
    }
    if (bRequireRuntime)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            TInlineComponentArray<UPrimitiveComponent*> Primitives(*It);
            for (const UPrimitiveComponent* Primitive : Primitives)
            {
                if (Primitive != Scene->BuildingHeroVisualComponent.Get() &&
                    (!Primitive->LightingChannels.bChannel0 ||
                     Primitive->LightingChannels.bChannel1 ||
                     Primitive->LightingChannels.bChannel2))
                {
                    OutReport = FString::Printf(
                        TEXT("ISTANA_EXPLORE_V5B_WORLD_INVALID: non-hero primitive '%s' escaped lighting channel 0."),
                        *Primitive->GetPathName());
                    return false;
                }
            }
        }
    }
    OutReport = bRequireRuntime
        ? TEXT("ISTANA_EXPLORE_V5B_WORLD_VALID runtime=true exact Scene/V2/V3/V4/V5/V5B/RuntimePolicy census; the one begun runtime-policy actor is settled and verifies /Script/TRIADSensorFusion.TRIADIstanaExploreV5GameMode; trimmed render hero has exactly six transient direct-parent V5 facade MIDs on slots 1,6,7,8,9,10 and lighting channels 0+1; one actor-owned 2500-lux shadowless/GI-free/volumetric-free facade fill uses isolated channel 1; a render-only hardscape successor removes the 24 legacy planting-bed faces while the hidden original remains collision/RF authority; 18432 low-discrepancy near-field fine-turf appearance-proxy clusters use 340 tuft centres (170 pairs/170 triads), 850 blade roots, a 170/510/170 upright/swept/reclined posture mix, 722/128 C-shape/counter-bend split, exact 2550/850/256-triangle LODs at 1.0/0.10/0.040, and heterogeneous 1.6-4.4 cm source geometry whose frozen serialized-map transforms yield 1.833-2.933 cm source-tip maximums, physical-normal V7 tonality, face-corrected imported-curvature normals fading to world-up over 30-52 m, and wind fading to the 32 m WPO cutoff over the inherited procedural macro/micro PBR lawn; one 5696-triangle variable-apron neutral moist-mulch mound with micro-normal relief, all 1088 borrowed source plants and 2880 deterministic layered infill plants form 3968 formal-bed plants; 64 irregular mulch patches with 48 shrub and 144 calathea successors ground the read-only 720+9 V4 tree census outside the exact central lawn; exactly eight obsolete V2/V3 ground-plant renderers containing 2096 instances are hidden and zero flank successors remain on the lawn; 48 tropical morphology proxies form deterministic groves; 12+1 varied fountain sprays; separated impact ripples; and five staggered 384+256 paver courses validate. Fine-turf proxy only: not botanical/species identification, survey, fountain operating-state truth, or sensor/RF truth.")
        : TEXT("ISTANA_EXPLORE_V5B_WORLD_VALID runtime=false exact Scene/V2/V3/V4/V5/V5B/RuntimePolicy census; the one serialized runtime-policy actor requires /Script/TRIADSensorFusion.TRIADIstanaExploreV5GameMode; trimmed render hero retains zero material overrides and lighting channel 0 only; the runtime facade fill is disabled; a render-only hardscape successor removes the 24 legacy planting-bed faces while the hidden original remains collision/RF authority; 18432 low-discrepancy near-field fine-turf appearance-proxy clusters use 340 tuft centres (170 pairs/170 triads), 850 blade roots, a 170/510/170 upright/swept/reclined posture mix, 722/128 C-shape/counter-bend split, exact 2550/850/256-triangle LODs at 1.0/0.10/0.040, and heterogeneous 1.6-4.4 cm source geometry whose frozen serialized-map transforms yield 1.833-2.933 cm source-tip maximums, physical-normal V7 tonality, face-corrected imported-curvature normals fading to world-up over 30-52 m, and wind fading to the 32 m WPO cutoff over the inherited procedural macro/micro PBR lawn; one 5696-triangle variable-apron neutral moist-mulch mound with micro-normal relief, all 1088 borrowed source plants and 2880 deterministic layered infill plants form 3968 formal-bed plants; 64 irregular mulch patches with 48 shrub and 144 calathea successors ground the read-only 720+9 V4 tree census outside the exact central lawn; exactly eight obsolete V2/V3 ground-plant renderers containing 2096 instances are hidden and zero flank successors remain on the lawn; 48 tropical morphology proxies form deterministic groves; 12+1 varied fountain sprays; separated impact ripples; and five staggered 384+256 paver courses validate. Fine-turf proxy only: not botanical/species identification, survey, fountain operating-state truth, or sensor/RF truth.");
    return true;
}

bool ValidateSourceV5EditorWorld(FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != SourceMapPackage)
    {
        OutError = TEXT("The exact Explore V5 editor source is not loaded.");
        return false;
    }
    FString Report;
    if (!UTRIADIstanaExploreV5EditorLibrary::ValidateIstanaExploreV5Map(Report))
    {
        OutError = TEXT("Explore V5 source validation failed: ") + Report;
        return false;
    }
    OutError.Reset();
    return true;
}

FTRIADIstanaExploreV5BAssetRoster LoadV5BRoster(
    ATRIADIstanaPublicViewSceneActor* Scene,
    ATRIADIstanaExploreV4LandscapeActor* V4)
{
    FTRIADIstanaExploreV5BAssetRoster Assets;
    Assets.HardscapeRenderSuccessorMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetHardscapeRenderSuccessorMeshObjectPath());
    UStaticMeshComponent* SourceHardscape = Scene
        ? Scene->HardscapeComponent.Get()
        : nullptr;
    UStaticMesh* SourceHardscapeMesh = SourceHardscape
        ? SourceHardscape->GetStaticMesh()
        : nullptr;
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        UMaterialInterface* EffectiveMaterial = nullptr;
        if (Assets.HardscapeRenderSuccessorMesh && SourceHardscapeMesh &&
            Assets.HardscapeRenderSuccessorMesh->GetStaticMaterials().IsValidIndex(Slot))
        {
            const FName ImportedName = Assets.HardscapeRenderSuccessorMesh
                ->GetStaticMaterials()[Slot].ImportedMaterialSlotName;
            const int32 SourceSlot =
                SourceHardscapeMesh->GetMaterialIndexFromImportedMaterialSlotName(
                    ImportedName);
            EffectiveMaterial = SourceSlot != INDEX_NONE
                ? SourceHardscape->GetMaterial(SourceSlot)
                : nullptr;
        }
        Assets.HardscapeRenderSuccessorMaterials.Add(EffectiveMaterial);
    }
    Assets.AccentTurfMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetAccentTurfMeshObjectPath());
    Assets.AccentTurfMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetAccentTurfMaterialObjectPath());
    Assets.FormalBedVeneerMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetFormalBedVeneerMeshObjectPath());
    Assets.FormalBedVeneerMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetFormalBedVeneerMaterialObjectPath());
    Assets.TreeBaseMulchMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetTreeBaseMulchMeshObjectPath());
    Assets.FormalBedShrubMesh = V4 && V4->ShrubInstances
        ? V4->ShrubInstances->GetStaticMesh()
        : nullptr;
    Assets.FormalBedShrubMaterial = V4 && V4->ShrubInstances
        ? V4->ShrubInstances->GetMaterial(0)
        : nullptr;
    Assets.FormalBedFlowerMesh = V4 && V4->FlowerInstances
        ? V4->FlowerInstances->GetStaticMesh()
        : nullptr;
    Assets.FormalBedFlowerMaterial = V4 && V4->FlowerInstances
        ? V4->FlowerInstances->GetMaterial(0)
        : nullptr;
    Assets.FormalBedUnderstoreyMesh = V4 && V4->UnderstoreyInstances
        ? V4->UnderstoreyInstances->GetStaticMesh()
        : nullptr;
    Assets.FormalBedUnderstoreyMaterial = V4 && V4->UnderstoreyInstances
        ? V4->UnderstoreyInstances->GetMaterial(0)
        : nullptr;
    Assets.FountainSurfaceMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainSurfaceMeshObjectPath());
    Assets.FountainSurfaceMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainSurfaceMaterialObjectPath());
    Assets.FountainEdgeFoamMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainFoamMeshObjectPath());
    Assets.FountainEdgeFoamMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainFoamMaterialObjectPath());
    Assets.OuterPlumeMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainPlumeMeshObjectPath());
    Assets.OuterPlumeMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainSprayMaterialObjectPath());
    Assets.ImpactRingMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainImpactRingMeshObjectPath());
    Assets.ImpactRingMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainFoamMaterialObjectPath());
    Assets.CentralPlumeMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainCentralPlumeMeshObjectPath());
    Assets.CentralPlumeMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetFountainSprayMaterialObjectPath());
    Assets.InnerPaverWedgeMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetPaverInnerMeshObjectPath());
    Assets.OuterPaverWedgeMesh = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetPaverOuterMeshObjectPath());
    Assets.PaverMaterial = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetPaverMaterialObjectPath());

    const TArray<FString>& Pachira =
        TRIADIstanaExploreV5BAssetFactory::GetPachiraMeshObjectPaths();
    UMaterialInterface* Bark = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetPachiraBarkMaterialObjectPath());
    UMaterialInterface* Leaves = LoadExact<UMaterialInterface>(
        TRIADIstanaExploreV5BAssetFactory::GetPachiraLeavesMaterialObjectPath());
    for (int32 Variant = 0; Variant < 4; ++Variant)
    {
        Assets.PachiraBarkMeshes.Add(
            Pachira.IsValidIndex(Variant * 2)
                ? LoadExact<UStaticMesh>(Pachira[Variant * 2])
                : nullptr);
        Assets.PachiraLeavesMeshes.Add(
            Pachira.IsValidIndex(Variant * 2 + 1)
                ? LoadExact<UStaticMesh>(Pachira[Variant * 2 + 1])
                : nullptr);
        Assets.PachiraBarkMaterials.Add(Bark);
        Assets.PachiraLeavesMaterials.Add(Leaves);
    }
    return Assets;
}

bool DestinationExists()
{
    FString Filename;
    return FPackageName::DoesPackageExist(DestinationMapPackage, &Filename) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath);
}

bool GetValidatedPlayState(
    UWorld*& OutWorld,
    APlayerController*& OutPlayer,
    ATRIADIstanaExploreV5Pawn*& OutPawn,
    ATRIADIstanaExploreV4LandscapeActor*& OutV4,
    ATRIADIstanaExploreV5AppearanceActor*& OutV5,
    ATRIADIstanaExploreV5BVisualActor*& OutV5B,
    FString& OutError)
{
    OutWorld = GEditor ? GEditor->PlayWorld : nullptr;
    FString WorldReport;
    if (!ValidateV5BWorld(OutWorld, true, WorldReport))
    {
        OutError = WorldReport;
        return false;
    }
    OutPlayer = UGameplayStatics::GetPlayerController(OutWorld, 0);
    OutPawn = OutPlayer ? Cast<ATRIADIstanaExploreV5Pawn>(OutPlayer->GetPawn()) : nullptr;
    int32 V4Count = 0;
    int32 V5Count = 0;
    int32 V5BCount = 0;
    OutV4 = FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(OutWorld, V4Count);
    OutV5 = FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(OutWorld, V5Count);
    OutV5B = FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(OutWorld, V5BCount);
    if (!OutPlayer || !OutPawn || !OutPawn->HasExpectedExploreV5CameraProfile() ||
        OutPlayer->GetViewTarget() != OutPawn || V4Count != 1 || V5Count != 1 ||
        V5BCount != 1 || !OutV4 || !OutV5 || !OutV5B ||
        !OutV4->HasActorBegunPlay() || !OutV4->IsWindRuntimeActive() ||
        !OutV5->HasActorBegunPlay() || !OutV5B->HasActorBegunPlay())
    {
        OutError = TEXT("V5B PIE lost exact Player0/V5-pawn/view-target or runtime actor identity.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateV5BPlayWorldIdentityForStop(FString& OutError)
{
    UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
    const FString LogicalPackage = World && World->GetOutermost()
        ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())
        : FString();
    if (!World || !World->PersistentLevel || World->WorldType != EWorldType::PIE ||
        LogicalPackage != DestinationMapPackage)
    {
        OutError = TEXT("The active world is not the exact Explore V5B PIE package.");
        return false;
    }

    int32 SceneCount = 0;
    int32 V2Count = 0;
    int32 V3Count = 0;
    int32 V4Count = 0;
    int32 V5Count = 0;
    int32 V5BCount = 0;
    int32 RuntimePolicyCount = 0;
    FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    FindExactlyOne<ATRIADIstanaExploreV2LandscapeActor>(World, V2Count);
    FindExactlyOne<ATRIADIstanaExploreV3SupplementActor>(World, V3Count);
    FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(World, V5Count);
    FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(World, V5BCount);
    FindExactlyOne<ATRIADIstanaPublicViewRuntimePolicyActor>(
        World, RuntimePolicyCount);
    if (SceneCount != 1 || V2Count != 1 || V3Count != 1 || V4Count != 1 ||
        V5Count != 1 || V5BCount != 1 || RuntimePolicyCount != 1)
    {
        OutError = FString::Printf(
            TEXT("Explore V5B stop identity failed actor census Scene/V2/V3/V4/V5/V5B/RuntimePolicy=%d/%d/%d/%d/%d/%d/%d."),
            SceneCount,
            V2Count,
            V3Count,
            V4Count,
            V5Count,
            V5BCount,
            RuntimePolicyCount);
        return false;
    }

    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5BEditorLibrary::ImportIstanaExploreV5BAssets(
    FString& OutMessage)
{
    FString Existing;
    if (TRIADIstanaExploreV5BAssetFactory::ValidateExploreV5BAssets(Existing))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5B_ASSETS_ALREADY_VALID: ") + Existing;
        return true;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5B_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5BAssetFactory::CreateFreshExploreV5BAssets(
            FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_IMPORT_FAILED: ") + Error;
        return false;
    }
    if (FreshAssets.Num() != 37 || FreshAssets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(FreshAssets, false) ||
        !TRIADIstanaExploreV5BAssetFactory::ValidateExploreV5BAssets(Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_IMPORT_FAILED_AFTER_FRESH_CREATE: only the exact 37 V5B assets were offered to save. ") + Error;
        return false;
    }
    OutMessage = TEXT("Imported and cold-readback-validated the exact 37-asset Explore V5B namespace: trimmed portico render mesh, hardscape successor without the 24 legacy bed faces, zero-texture masked/two-sided modeled-blade fine-turf appearance proxy, a ten-ring variable-apron organic formal-bed mound, one 224-triangle tree-base mulch patch, seven close-range prop meshes, eight lawful Pachira parts, nine provenance/source textures, and eight materials. No botanical/species identification is claimed and no V4/V5 package was saved.");
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::ValidateIstanaExploreV5BAssets(
    FString& OutReport)
{
    return TRIADIstanaExploreV5BAssetFactory::ValidateExploreV5BAssets(OutReport);
}

bool UTRIADIstanaExploreV5BEditorLibrary::
    UpgradeIstanaExploreV5BAccentTurfAsset(FString& OutMessage)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutMessage = TEXT("EXPLORE_V5B_ACCENT_UPGRADE_REFUSED: a non-PIE editor world is required.");
        return false;
    }
    UEditorAssetSubsystem* AssetSubsystem =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5B_ACCENT_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    const FString& AccentObjectPath =
        TRIADIstanaExploreV5BAssetFactory::GetAccentTurfMeshObjectPath();
    FString PackageName;
    FString PackageFilename;
    FString Error;
    if (!ResolvePersistedPackageFile(
            AccentObjectPath, PackageName, PackageFilename, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_ACCENT_UPGRADE_REFUSED: ") + Error;
        return false;
    }
    UStaticMesh* OriginalAsset = LoadExact<UStaticMesh>(AccentObjectPath);
    UPackage* OriginalPackage = OriginalAsset
        ? OriginalAsset->GetOutermost()
        : nullptr;
    if (!OriginalAsset || !OriginalPackage || OriginalPackage->IsDirty())
    {
        OutMessage = TEXT("EXPLORE_V5B_ACCENT_UPGRADE_REFUSED: the exact accent mesh must be loaded and clean.");
        return false;
    }

    bool bAlreadyR11 = false;
    if (!TRIADIstanaExploreV5BAssetFactory::
            ValidateAccentTurfAssetForR11Upgrade(bAlreadyR11, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_ACCENT_R11_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }
    if (bAlreadyR11)
    {
        TMap<FString, FString> Preserved;
        if (!SnapshotOtherV5BPackageHashes(Preserved, Error))
        {
            OutMessage = TEXT("EXPLORE_V5B_ACCENT_R11_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutMessage = TEXT("IDEMPOTENT_V5B_ACCENT_TURF_R11_ALREADY_VALID assetCount=37 preservedPackageHashes=36 pathStable=true mapsSaved=0 fineTurfAppearanceProxy=true botanicalSpeciesClaim=false");
        return true;
    }

    TMap<FString, FString> PreservedBefore;
    FAccentTurfDiskBackup Backup;
    if (!SnapshotOtherV5BPackageHashes(PreservedBefore, Error) ||
        !BackUpAccentTurfPackage(PackageFilename, Backup, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_ACCENT_UPGRADE_REFUSED_BACKUP: ") + Error;
        return false;
    }
    const FString* BackedUpAccentHash =
        Backup.OriginalHashes.Find(PackageFilename);
    const FString OriginalAccentHash = BackedUpAccentHash
        ? *BackedUpAccentHash
        : FString();
    if (OriginalAccentHash.IsEmpty() ||
        FileMd5(PackageFilename) != OriginalAccentHash)
    {
        OutMessage = TEXT("EXPLORE_V5B_ACCENT_UPGRADE_REFUSED_HASH: canonical package hash is unavailable or no longer matches the verified backup snapshot. Backup: ") +
            Backup.Directory;
        return false;
    }

    const auto FailWithRollback = [
        &OutMessage,
        &Backup,
        &PackageName,
        &PreservedBefore](const FString& Failure)
    {
        FString RollbackError;
        UPackage* CurrentPackage = FindPackage(nullptr, *PackageName);
        bool bRolledBack =
            RestoreAccentTurfPackage(Backup, CurrentPackage, RollbackError);
        TMap<FString, FString> PreservedAfterRollback;
        FString PreservationError;
        if (!SnapshotOtherV5BPackageHashes(
                PreservedAfterRollback, PreservationError) ||
            !HashSnapshotsMatch(
                PreservedBefore, PreservedAfterRollback, PreservationError))
        {
            bRolledBack = false;
            RollbackError +=
                TEXT(" Preserved-package verification failed: ") +
                PreservationError;
        }
        const FString RollbackStatus = bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK preservedPackageHashes=36 backup=")) +
                Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError;
        OutMessage = Failure + RollbackStatus;
        return false;
    };

    UStaticMesh* RebuiltAsset = nullptr;
    if (!TRIADIstanaExploreV5BAssetFactory::RebuildExistingAccentTurfAsset(
            RebuiltAsset, Error) ||
        RebuiltAsset != OriginalAsset ||
        RebuiltAsset->GetPathName() != AccentObjectPath)
    {
        return FailWithRollback(
            TEXT("EXPLORE_V5B_ACCENT_UPGRADE_FAILED_IN_PLACE: ") + Error);
    }
    if (!AssetSubsystem->SaveLoadedAsset(RebuiltAsset, false))
    {
        return FailWithRollback(
            TEXT("EXPLORE_V5B_ACCENT_UPGRADE_FAILED_SAVE: only the exact accent mesh was offered to save."));
    }

    FText ReloadError;
    const TArray<UPackage*> PackagesToReload = {OriginalPackage};
    if (!UPackageTools::ReloadPackages(
            PackagesToReload,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        return FailWithRollback(
            TEXT("EXPLORE_V5B_ACCENT_UPGRADE_FAILED_RELOAD: ") +
            ReloadError.ToString());
    }

    TMap<FString, FString> PreservedAfter;
    FString Validation;
    UStaticMesh* ColdAsset = LoadExact<UStaticMesh>(AccentObjectPath);
    if (!ColdAsset ||
        !SnapshotOtherV5BPackageHashes(PreservedAfter, Error) ||
        !HashSnapshotsMatch(PreservedBefore, PreservedAfter, Error) ||
        !TRIADIstanaExploreV5BAssetFactory::ValidateExploreV5BAssets(
            Validation))
    {
        const FString Detail = !Error.IsEmpty() ? Error : Validation;
        return FailWithRollback(
            TEXT("EXPLORE_V5B_ACCENT_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Detail);
    }
    const FString NewAccentHash = FileMd5(PackageFilename);
    if (NewAccentHash.IsEmpty() || NewAccentHash == OriginalAccentHash)
    {
        return FailWithRollback(
            TEXT("EXPLORE_V5B_ACCENT_UPGRADE_FAILED_DISK_DELTA: the canonical accent package did not acquire a distinct valid hash."));
    }

    OutMessage = FString::Printf(
        TEXT("V5B_ACCENT_TURF_R11_UPGRADE_PASS geometryRevision=R11 uniformR10Input=true fineTurfAppearanceProxy=true botanicalSpeciesClaim=false asset=%s assetCount=37 preservedPackageHashes=36 pathStable=true inPlace=true mapReferencesStableByObjectPath=true mapsSaved=0 lodTriangles=2550,680,204 clippedJuvenileBlades=510,170 postureMix=306,306,68 oldMd5=%s newMd5=%s backup=%s %s"),
        *AccentObjectPath,
        *OriginalAccentHash,
        *NewAccentHash,
        *Backup.Directory,
        *Validation);
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::
    RefreshIstanaExploreV5BGroundingUnsaved(FString& OutMessage)
{
    FString AssetReport;
    if (!TRIADIstanaExploreV5BAssetFactory::ValidateExploreV5BAssets(
            AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5B_GROUNDING_REFRESH_REFUSED_ASSETS: exact 37-asset roster required. ") + AssetReport;
        return false;
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World || GEditor->PlayWorld || World->WorldType != EWorldType::Editor ||
        !World->PersistentLevel || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != DestinationMapPackage)
    {
        OutMessage = TEXT("EXPLORE_V5B_GROUNDING_REFRESH_REFUSED_WORLD: load the exact non-PIE /Game/Maps/Istana_PublicView_Explore_v5b editor map.");
        return false;
    }

    int32 SceneCount = 0;
    int32 V2Count = 0;
    int32 V3Count = 0;
    int32 V4Count = 0;
    int32 V5Count = 0;
    int32 V5BCount = 0;
    int32 RuntimePolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV2LandscapeActor* V2 =
        FindExactlyOne<ATRIADIstanaExploreV2LandscapeActor>(World, V2Count);
    ATRIADIstanaExploreV3SupplementActor* V3 =
        FindExactlyOne<ATRIADIstanaExploreV3SupplementActor>(World, V3Count);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    ATRIADIstanaExploreV5AppearanceActor* V5 =
        FindExactlyOne<ATRIADIstanaExploreV5AppearanceActor>(World, V5Count);
    ATRIADIstanaExploreV5BVisualActor* Visual =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(World, V5BCount);
    ATRIADIstanaPublicViewRuntimePolicyActor* RuntimePolicy =
        FindExactlyOne<ATRIADIstanaPublicViewRuntimePolicyActor>(
            World,
            RuntimePolicyCount);
    FString SceneReport;
    FString V4Report;
    FString V5Report;
    FString BoundaryError;
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (ATRIADIstanaExploreV5GameMode::StaticClass()->GetPathName() !=
            V5BGameModeClassPath ||
        !WorldSettings ||
        WorldSettings->DefaultGameMode !=
            ATRIADIstanaExploreV5GameMode::StaticClass() ||
        SceneCount != 1 || V2Count != 1 || V3Count != 1 || V4Count != 1 ||
        V5Count != 1 || V5BCount != 1 || RuntimePolicyCount != 1 ||
        !Scene || !V2 || !V3 || !V4 || !V5 || !Visual || !RuntimePolicy ||
        !RuntimePolicy->bRequireIstanaAirSimGameMode ||
        RuntimePolicy->bEnforceFixedPrimaryCamera ||
        RuntimePolicy->RequiredGameModeClassPath != V5BGameModeClassPath ||
        !Visual->GetActorTransform().Equals(FTransform::Identity, 0.0) ||
        !Visual->Tags.Contains(V5BActorTag) ||
        !Scene->ValidatePublicViewScene(SceneReport, false) ||
        !V4->ValidateExploreV4Landscape(V4Report) ||
        !V5->ValidateExploreV5Appearance(V5Report, false) ||
        !ValidateSceneV5BHeroBoundary(Scene, false, BoundaryError))
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5B_GROUNDING_REFRESH_REFUSED_ROSTER: exact package/persistent level, V5 game-mode class/default, Scene/V2/V3/V4/V5/V5B/RuntimePolicy census, non-runtime policy fields, identity/tag, Scene/V4/V5 validators, and hero boundary required; found %d/%d/%d/%d/%d/%d/%d policyPath='%s'. %s %s %s %s"),
            SceneCount,
            V2Count,
            V3Count,
            V4Count,
            V5Count,
            V5BCount,
            RuntimePolicyCount,
            RuntimePolicy
                ? *RuntimePolicy->RequiredGameModeClassPath
                : TEXT("<absent>"),
            *SceneReport,
            *V4Report,
            *V5Report,
            *BoundaryError);
        return false;
    }

    const FTRIADIstanaExploreV5BAssetRoster Roster =
        LoadV5BRoster(Scene, V4);
    FString Error;
    if (!ATRIADIstanaExploreV5BVisualActor::ValidateAssetRoster(
            Roster,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_GROUNDING_REFRESH_REFUSED_ROSTER_BINDINGS: ") + Error;
        return false;
    }

    TArray<AActor*> TransactionActors = {Scene, V2, V3, V4, Visual};
    const auto AddTransactionActor = [&TransactionActors](AActor* Actor)
    {
        if (Actor)
        {
            TransactionActors.AddUnique(Actor);
        }
    };
    AddTransactionActor(Visual->PublicViewSceneActor);
    AddTransactionActor(Visual->ExploreV4LandscapeActor);
    if (Visual->ExploreV4LandscapeActor &&
        Visual->ExploreV4LandscapeActor->GetWorld() != World)
    {
        int32 PreviousV2Count = 0;
        int32 PreviousV3Count = 0;
        AddTransactionActor(
            FindExactlyOne<ATRIADIstanaExploreV2LandscapeActor>(
                Visual->ExploreV4LandscapeActor->GetWorld(),
                PreviousV2Count));
        AddTransactionActor(
            FindExactlyOne<ATRIADIstanaExploreV3SupplementActor>(
                Visual->ExploreV4LandscapeActor->GetWorld(),
                PreviousV3Count));
    }

    TArray<UObject*> TouchedObjects = {World, World->PersistentLevel};
    for (AActor* Actor : TransactionActors)
    {
        if (!Actor)
        {
            continue;
        }
        TouchedObjects.AddUnique(Actor);
        TInlineComponentArray<UActorComponent*> Components(Actor);
        for (UActorComponent* Component : Components)
        {
            if (Component)
            {
                TouchedObjects.AddUnique(Component);
            }
        }
    }
    const TArray<FV5BTouchedPackageDirtyFlag> PackageDirtyFlagsBefore =
        CaptureTouchedPackageDirtyFlags(TouchedObjects);

    FScopedTransaction Transaction(NSLOCTEXT(
        "TRIADIstanaExploreV5B",
        "RefreshGroundingUnsaved",
        "Refresh Istana Explore V5B grounding (unsaved)"));
    for (UObject* Object : TouchedObjects)
    {
        if (Object)
        {
            Object->Modify();
        }
    }

    if (!Visual->ConfigureExploreV5BVisuals(Scene, V4, Roster, Error))
    {
        Transaction.Cancel();
        RestoreTouchedPackageDirtyFlags(PackageDirtyFlagsBefore);
        OutMessage = TEXT("EXPLORE_V5B_GROUNDING_REFRESH_FAILED_CONFIGURE: ") + Error;
        return false;
    }
    World->MarkPackageDirty();
    OutMessage = TEXT("EXPLORE_V5B_GROUNDING_REFRESHED_UNSAVED: exact 37-asset roster was atomically reapplied to the unique existing V5B visual actor; Configure validated the complete visual state, including 64 mulch, 48 shrub, and 144 calathea render-only instances. Map package is dirty and intentionally UNSAVED.");
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::BuildIstanaExploreV5BMap(
    FString& OutMessage)
{
    FString SourceFilename;
    FString Error;
    if (!SourceMapFileIsPresent(SourceFilename, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_REFUSED_SOURCE_GUARD: ") + Error;
        return false;
    }
    FString AssetReport;
    if (!TRIADIstanaExploreV5BAssetFactory::ValidateExploreV5BAssets(AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_REFUSED_ASSETS: run ImportIstanaExploreV5BAssets first. ") + AssetReport;
        return false;
    }
    if (DestinationExists())
    {
        FString ExistingFilename;
        UWorld* Existing = FPackageName::DoesPackageExist(
                DestinationMapPackage, &ExistingFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(ExistingFilename)
            : nullptr;
        FString ExistingReport;
        if (Existing && ValidateV5BWorld(Existing, false, ExistingReport))
        {
            OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5B_MAP_ALREADY_VALID: ") + ExistingReport;
            return true;
        }
        OutMessage = TEXT("EXPLORE_V5B_BUILD_REFUSED_PARTIAL_DESTINATION: target exists but is not exact; overwrite is refused. ") + ExistingReport;
        return false;
    }

    UWorld* LoadedSource = UEditorLoadingAndSavingUtils::LoadMap(SourceFilename);
    if (!LoadedSource || !ValidateSourceV5EditorWorld(Error) ||
        IFileManager::Get().FileSize(*SourceFilename) != ExpectedSourceMapBytes)
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_REFUSED_V5_SOURCE: ") + Error;
        return false;
    }

    LoadedSource = nullptr;
    if (!FEditorFileUtils::LoadMap(SourceFilename, true, false))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_TEMPLATE: V5 could not be opened as a non-destructive untitled duplicate.");
        return false;
    }
    UWorld* Target = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!Target || !Target->GetOutermost() ||
        !FPackageName::IsTempPackage(Target->GetOutermost()->GetName()))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_TEMPLATE_GATE: duplicated V5 world is not an untitled temp package.");
        return false;
    }
    int32 SceneCount = 0;
    int32 V4Count = 0;
    int32 ExistingV5BCount = 0;
    int32 RuntimePolicyCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(Target, SceneCount);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(Target, V4Count);
    FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(Target, ExistingV5BCount);
    ATRIADIstanaPublicViewRuntimePolicyActor* RuntimePolicy =
        FindExactlyOne<ATRIADIstanaPublicViewRuntimePolicyActor>(
            Target, RuntimePolicyCount);
    FString SceneReport;
    FString V4Report;
    if (SceneCount != 1 || V4Count != 1 || ExistingV5BCount != 0 ||
        RuntimePolicyCount != 1 || !Scene || !V4 || !RuntimePolicy ||
        !Scene->ValidatePublicViewScene(SceneReport, false) ||
        !V4->ValidateExploreV4Landscape(V4Report))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_TEMPLATE_ROSTER: ") + SceneReport + TEXT(" ") + V4Report;
        return false;
    }

    RuntimePolicy->Modify();
    RuntimePolicy->bEnforceFixedPrimaryCamera = false;
    RuntimePolicy->bRequireIstanaAirSimGameMode = true;
    RuntimePolicy->RequiredGameModeClassPath = V5BGameModeClassPath;
    if (RuntimePolicy->bEnforceFixedPrimaryCamera ||
        !RuntimePolicy->bRequireIstanaAirSimGameMode ||
        RuntimePolicy->RequiredGameModeClassPath != V5BGameModeClassPath)
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_RUNTIME_POLICY: exact V5 game-mode requirement could not be assigned to the one inherited runtime-policy actor.");
        return false;
    }

    UStaticMesh* OriginalCollisionMesh = Scene->BuildingCollisionComponent
        ? Scene->BuildingCollisionComponent->GetStaticMesh()
        : nullptr;
    const FTransform OriginalCollisionTransform = Scene->BuildingCollisionComponent
        ? Scene->BuildingCollisionComponent->GetRelativeTransform()
        : FTransform::Identity;
    const ECollisionEnabled::Type OriginalCollisionEnabled = Scene->BuildingCollisionComponent
        ? Scene->BuildingCollisionComponent->GetCollisionEnabled()
        : ECollisionEnabled::NoCollision;
    UStaticMesh* TrimmedHero = LoadExact<UStaticMesh>(
        TRIADIstanaExploreV5BAssetFactory::GetPorticoMeshObjectPath());
    const FTRIADIstanaExploreV5BAssetRoster Roster = LoadV5BRoster(Scene, V4);
    if (!TrimmedHero || !ATRIADIstanaExploreV5BVisualActor::ValidateAssetRoster(Roster, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_ASSET_ROSTER: ") + Error;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreVisualsV5B");
    SpawnParameters.OverrideLevel = Target->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV5BVisualActor* Visual =
        Target->SpawnActor<ATRIADIstanaExploreV5BVisualActor>(
            ATRIADIstanaExploreV5BVisualActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Visual)
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_SPAWN: render-only V5B actor could not be spawned.");
        return false;
    }
    Visual->Tags.AddUnique(V5BActorTag);
    Visual->SetActorLabel(TEXT("TRIAD Istana Explore V5B Hyperreal Visual Successor (Render Only)"));
    if (!Visual->ConfigureExploreV5BVisuals(Scene, V4, Roster, Error) ||
        !Visual->ReapplyExploreV5BVisuals(Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_VISUALS: ") + Error;
        return false;
    }

    Scene->BuildingHeroVisualComponent->Modify();
    Scene->BuildingHeroVisualComponent->SetStaticMesh(TrimmedHero);
    Scene->BuildingHeroVisualComponent->EmptyOverrideMaterials();
    Scene->BuildingHeroVisualComponent->MarkRenderStateDirty();
    if (Scene->BuildingCollisionComponent->GetStaticMesh() != OriginalCollisionMesh ||
        !Scene->BuildingCollisionComponent->GetRelativeTransform().Equals(OriginalCollisionTransform, 0.0) ||
        Scene->BuildingCollisionComponent->GetCollisionEnabled() != OriginalCollisionEnabled ||
        !ValidateSceneV5BHeroBoundary(Scene, false, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_HERO_BOUNDARY: ") + Error;
        return false;
    }

    bool bLateTarget = DestinationExists();
    if (bLateTarget)
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_REFUSED_LATE_DESTINATION: target appeared after preflight.");
        return false;
    }
    Target->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(Target, DestinationMapPackage) ||
        !Target->GetOutermost() || Target->GetOutermost()->GetName() != DestinationMapPackage ||
        IFileManager::Get().FileSize(*SourceFilename) != ExpectedSourceMapBytes)
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_SAVE: only the new target was offered to save; V5 source byte guard changed or save failed.");
        return false;
    }

    UWorld* ReloadedSource = UEditorLoadingAndSavingUtils::LoadMap(SourceFilename);
    if (!ReloadedSource || !ValidateSourceV5EditorWorld(Error) ||
        FindPackage(nullptr, *DestinationMapPackage) ||
        FindObject<UWorld>(nullptr, *DestinationMapObjectPath))
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_COLD_UNLOAD: target could not be unloaded at the still-valid V5 source. ") + Error;
        return false;
    }
    FString DestinationFilename;
    UWorld* ReloadedTarget = FPackageName::DoesPackageExist(
            DestinationMapPackage, &DestinationFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)
        : nullptr;
    FString ColdReport;
    if (!ReloadedTarget || !ValidateV5BWorld(ReloadedTarget, false, ColdReport) ||
        IFileManager::Get().FileSize(*SourceFilename) != ExpectedSourceMapBytes)
    {
        OutMessage = TEXT("EXPLORE_V5B_BUILD_FAILED_COLD_RELOAD: ") + ColdReport;
        return false;
    }
    OutMessage = TEXT("Created and cold-reload-validated /Game/Maps/Istana_PublicView_Explore_v5b. The inherited cross-wing render shell is physically trimmed and the 24 legacy planting-bed render faces are replaced by raised organic mounds, while original collision/RF authority remains unchanged; added layers are a dense deterministic render-only fine-turf appearance proxy with no botanical/species claim, lawful tropical morphology proxies, fountain water/spray/foam, and beveled paver veneers. ") + ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::ValidateIstanaExploreV5BMap(
    FString& OutReport)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    return ValidateV5BWorld(World, false, OutReport);
}

bool UTRIADIstanaExploreV5BEditorLibrary::ValidateIstanaExploreV5BPlayWorld(
    FString& OutReport)
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* V5 = nullptr;
    ATRIADIstanaExploreV5BVisualActor* V5B = nullptr;
    FString Error;
    if (!GetValidatedPlayState(World, Player, Pawn, V4, V5, V5B, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5B_PIE_INVALID: ") + Error;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5B_PIE_VALID exact Player0/V5 camera and exact-one settled runtime-policy actor verifying /Script/TRIADSensorFusion.TRIADIstanaExploreV5GameMode; deterministic low-discrepancy modeled-blade fine-turf appearance proxy with 340 pair/triad tuft centres, 850 blades, and near-field fade, variable-apron layered formal beds, and trimmed render-only hardscape/hero successors; no botanical/species claim; inherited wind; original collision/navigation/sensor/RF authority untouched.");
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::CaptureIstanaExploreV5BPlayView(
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
    if (!OutputFileName.StartsWith(TEXT("explore_v5b_"), ESearchCase::IgnoreCase) ||
        !OutputFileName.EndsWith(TEXT(".png"), ESearchCase::IgnoreCase) ||
        FPaths::GetCleanFilename(OutputFileName) != OutputFileName ||
        FPaths::MakeValidFileName(OutputFileName) != OutputFileName ||
        !FPaths::ValidatePath(OutputFileName, &FilenameReason) || bHasControlCharacter)
    {
        OutMessage = TEXT("Explore V5B capture requires one safe exact explore_v5b_*.png filename. ") + FilenameReason.ToString();
        return false;
    }
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* V5 = nullptr;
    ATRIADIstanaExploreV5BVisualActor* V5B = nullptr;
    FString Error;
    if (!GetValidatedPlayState(World, Player, Pawn, V4, V5, V5B, Error))
    {
        OutMessage = TEXT("Explore V5B capture requires the exact validated Player0 world. ") + Error;
        return false;
    }
    UGameViewportClient* ViewportClient = World->GetGameViewport();
    FViewport* Viewport = ViewportClient ? ViewportClient->Viewport : nullptr;
    if (!Viewport || Viewport->GetSceneHDREnabled())
    {
        OutMessage = TEXT("Explore V5B capture requires the validated SDR game viewport.");
        return false;
    }
    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("TRIAD/IstanaPreviews/ExploreV5B"));
    FString Destination = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(Directory, OutputFileName));
    FPaths::NormalizeFilename(Destination);
    if (!IFileManager::Get().MakeDirectory(*Directory, true) ||
        IFileManager::Get().FileSize(*Destination) >= 0 ||
        FScreenshotRequest::IsScreenshotRequested() || GIsHighResScreenshot)
    {
        OutMessage = TEXT("Explore V5B capture refused an unsafe directory, overwrite, or overlapping request.");
        return false;
    }
    FHighResScreenshotConfig& Config = GetHighResScreenshotConfig();
    Config.SetHDRCapture(false);
    Config.SetForce128BitRendering(false);
    if (!Config.SetResolution(1920, 1080, 1.0f))
    {
        OutMessage = TEXT("UE rejected the exact 1920x1080 V5B capture configuration.");
        return false;
    }
    Config.SetFilename(Destination);
    Config.SetMaskEnabled(false);
    Config.bDumpBufferVisualizationTargets = false;
    Config.bDateTimeBasedNaming = false;
    Config.bDisplayCaptureRegion = false;
    if (!Viewport->TakeHighResScreenShot())
    {
        Config.FilenameOverride.Reset();
        OutMessage = TEXT("UE rejected the exact 1920x1080 V5B game-viewport request.");
        return false;
    }
    OutMessage = TEXT("Accepted exact 1920x1080 HDR-off V5B Player0 capture to '") + Destination + TEXT("'; wrapper must wait for and decode the PNG.");
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::TeleportIstanaExploreV5BPlayPawnForQa(
    FVector WorldLocationCentimeters,
    FRotator WorldRotationDegrees,
    FString& OutMessage)
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* V5 = nullptr;
    ATRIADIstanaExploreV5BVisualActor* V5B = nullptr;
    FString Error;
    if (WorldLocationCentimeters.ContainsNaN() || WorldRotationDegrees.ContainsNaN() ||
        FVector2D(WorldLocationCentimeters.X, WorldLocationCentimeters.Y).Size() > 95000.0 ||
        WorldLocationCentimeters.Z < 50.0 || WorldLocationCentimeters.Z > 20000.0 ||
        !GetValidatedPlayState(World, Player, Pawn, V4, V5, V5B, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_QA_TELEPORT_REFUSED: finite pose required inside 950m / 0.5-200m envelope. ") + Error;
        return false;
    }
    const FRotator Normalized = WorldRotationDegrees.GetNormalized();
    // The interactive pawn intentionally reapplies its private yaw/pitch state
    // every tick. Automated QA supplies an exact full view pose, so freeze
    // only this PIE pawn before readback/capture; PIE teardown destroys it.
    Pawn->SetActorTickEnabled(false);
    if (Pawn->IsActorTickEnabled() ||
        !Pawn->SetActorLocationAndRotation(
            WorldLocationCentimeters, Normalized, false, nullptr, ETeleportType::TeleportPhysics) ||
        !Pawn->GetActorLocation().Equals(WorldLocationCentimeters, 0.1f) ||
        !Pawn->GetActorRotation().Equals(Normalized, 0.1f) ||
        Player->GetViewTarget() != Pawn)
    {
        OutMessage = TEXT("EXPLORE_V5B_QA_TELEPORT_FAILED: pose/view-target readback failed.");
        return false;
    }
    OutMessage = FString::Printf(
        TEXT("Teleported and QA-froze validated Explore V5B Player0 pawn to %s at %s."),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString());
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::TriggerIstanaExploreV5BPlayWindGust(
    float PeakStrengthCm,
    FString& OutMessage)
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* V5 = nullptr;
    ATRIADIstanaExploreV5BVisualActor* V5B = nullptr;
    FString Error;
    if (!FMath::IsFinite(PeakStrengthCm) || PeakStrengthCm < 6.0f || PeakStrengthCm > 120.0f ||
        !GetValidatedPlayState(World, Player, Pawn, V4, V5, V5B, Error))
    {
        OutMessage = TEXT("EXPLORE_V5B_QA_GUST_REFUSED: exact V5B PIE and finite 6-120cm peak required. ") + Error;
        return false;
    }
    V4->TriggerWindGust(PeakStrengthCm);
    OutMessage = FString::Printf(
        TEXT("Triggered deterministic inherited wind on V5B at %.3fcm; %s"),
        PeakStrengthCm,
        *V4->BuildWindRuntimeStateReport());
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::GetIstanaExploreV5BPlayStateReport(
    FString& OutReport)
{
    UWorld* World = nullptr;
    APlayerController* Player = nullptr;
    ATRIADIstanaExploreV5Pawn* Pawn = nullptr;
    ATRIADIstanaExploreV4LandscapeActor* V4 = nullptr;
    ATRIADIstanaExploreV5AppearanceActor* V5 = nullptr;
    ATRIADIstanaExploreV5BVisualActor* V5B = nullptr;
    FString Error;
    if (!GetValidatedPlayState(World, Player, Pawn, V4, V5, V5B, Error))
    {
        OutReport = TEXT("EXPLORE_V5B_QA_STATE_INVALID: ") + Error;
        return false;
    }
    FString VisualReport;
    V5B->ValidateExploreV5BVisuals(VisualReport);
    OutReport = FString::Printf(
        TEXT("map=%s pawn=%s pawnLocationCm=%s pawnRotationDeg=%s viewTargetMatchesPawn=true v5CameraProfile=true %s visuals={%s}"),
        *UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()),
        *Pawn->GetName(),
        *Pawn->GetActorLocation().ToString(),
        *Pawn->GetActorRotation().ToString(),
        *V4->BuildWindRuntimeStateReport(),
        *VisualReport);
    return true;
}

bool UTRIADIstanaExploreV5BEditorLibrary::QuiesceIstanaExploreV5BPlayWorldForStop(
    FString& OutMessage)
{
    FString Error;
    if (!ValidateV5BPlayWorldIdentityForStop(Error))
    {
        OutMessage = TEXT("Refusing scripted Explore V5B PIE stop because exact runtime identity failed. ") + Error;
        return false;
    }
    OutMessage = TEXT("Explore V5B PIE package and actor-class census are exact; policy settlement, active GameMode, Player0/view-target, and facade presentation deliberately do not gate safe scripted stop.");
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5BPackageDirtyRollbackTest,
    "TRIAD.Istana.ExploreV5B.PackageDirtyRollback",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5BPackageDirtyRollbackTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    UPackage* InitiallyClean = CreatePackage(
        TEXT("/Temp/TRIAD_ExploreV5B_DirtyRollback_Clean"));
    UPackage* InitiallyDirty = CreatePackage(
        TEXT("/Temp/TRIAD_ExploreV5B_DirtyRollback_Dirty"));
    TestNotNull(TEXT("Clean transient test package exists"), InitiallyClean);
    TestNotNull(TEXT("Dirty transient test package exists"), InitiallyDirty);
    if (!InitiallyClean || !InitiallyDirty)
    {
        return false;
    }

    InitiallyClean->SetDirtyFlag(false);
    InitiallyDirty->SetDirtyFlag(true);
    const TArray<UObject*> TouchedObjects = {
        InitiallyClean,
        InitiallyDirty,
        InitiallyClean};
    const TArray<FV5BTouchedPackageDirtyFlag> Before =
        CaptureTouchedPackageDirtyFlags(TouchedObjects);
    TestEqual(TEXT("Dirty snapshot de-duplicates touched packages"), Before.Num(), 2);

    InitiallyClean->SetDirtyFlag(true);
    InitiallyDirty->SetDirtyFlag(false);
    RestoreTouchedPackageDirtyFlags(Before);
    TestFalse(
        TEXT("Rollback restores an initially clean touched package"),
        InitiallyClean->IsDirty());
    TestTrue(
        TEXT("Rollback restores an initially dirty touched package"),
        InitiallyDirty->IsDirty());

    InitiallyClean->SetDirtyFlag(false);
    InitiallyDirty->SetDirtyFlag(false);
    return true;
}

#endif
