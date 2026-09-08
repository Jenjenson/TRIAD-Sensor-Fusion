#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Ssl.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"

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
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString TransactionRelativeRoot(
    TEXT("TRIAD/NativeTransactions/V5DR29CopernicusTerrainFallbackV1"));

bool IsSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsHexDigit(Character))
        {
            return false;
        }
    }
    return true;
}

bool HashFile(
    const FString& Filename,
    FString& OutSha256,
    int64& OutBytes,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
    {
        OutError = TEXT("Could not read '") + Filename + TEXT("'.");
        return false;
    }
    OutBytes = Bytes.Num();
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) ==
        nullptr)
    {
        OutError = TEXT("SHA-256 failed for '") + Filename + TEXT("'.");
        return false;
    }
    OutSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("R29 Copernicus map transaction requires WITH_SSL.");
    return false;
#endif
    return true;
}

UWorld* GetExactLoadedWorld(bool bRequireClean, FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString PackageName = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    if (!World || World->WorldType != EWorldType::Editor || !Package ||
        PackageName != TargetMapPackage || (bRequireClean && Package->IsDirty()))
    {
        OutError = FString::Printf(
            TEXT("Expected exact %s editor world with clean package=%s; actual=%s dirty=%s."),
            *TargetMapPackage,
            bRequireClean ? TEXT("true") : TEXT("either"),
            *PackageName,
            Package && Package->IsDirty() ? TEXT("true") : TEXT("false"));
        return nullptr;
    }
    return World;
}

template <typename T>
T* FindExact(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    T* Result = nullptr;
    if (World)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (IsValid(*It) && (*It)->GetClass() == T::StaticClass())
            {
                Result = Cast<T>(*It);
                ++OutCount;
            }
        }
    }
    return Result;
}

bool ResolveFallbackRoster(
    UWorld* World,
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor*& OutActor,
    int32& OutClassCount,
    int32& OutTagCount,
    FString& OutError)
{
    OutActor = nullptr;
    OutClassCount = 0;
    OutTagCount = 0;
    if (!World)
    {
        OutError = TEXT("R29 Copernicus roster requires a world.");
        return false;
    }
    const FName& Tag =
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
            ExpectedActorTag();
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        const bool bExact = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                StaticClass();
        const bool bTagged = Candidate->Tags.Contains(Tag);
        if (bExact)
        {
            OutActor = Cast<
                ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>(
                    Candidate);
            ++OutClassCount;
        }
        OutTagCount += bTagged ? 1 : 0;
        if (bExact != bTagged)
        {
            OutError = TEXT("R29 Copernicus roster found an untagged owner or an impersonated owner tag.");
            return false;
        }
    }
    return true;
}

bool ValidateSuccessorWorld(
    UWorld* World,
    bool bRequireClean,
    FString& OutReport)
{
    if (!World || !World->GetOutermost() ||
        (bRequireClean && World->GetOutermost()->IsDirty()))
    {
        OutReport = TEXT("R29 Copernicus successor requires the exact loaded world and requested clean-package state.");
        return false;
    }
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Fallback = nullptr;
    int32 FallbackClasses = 0;
    int32 FallbackTags = 0;
    FString RosterError;
    if (!ResolveFallbackRoster(
            World, Fallback, FallbackClasses, FallbackTags, RosterError))
    {
        OutReport = RosterError;
        return false;
    }
    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 GroundCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExact<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExact<ATRIADIstanaExploreV5DContextPolicyActor>(World, PolicyCount);
    ATRIADIstanaExploreV5DGroundVegetationActor* Ground =
        FindExact<ATRIADIstanaExploreV5DGroundVegetationActor>(World, GroundCount);
    FString ActorReport;
    FString FacadeReport;
    FString PolicyReport;
    FString GroundReport;
    FString AssetReport;
    const bool bNegativeAuthority = Policy &&
        Policy->bCesiumLayerIsVisualOnly &&
        !Policy->bCesiumCollisionNavigationSensorOrRfAuthority &&
        !Policy->bTriadReadSerializedOrLoggedProviderToken &&
        !Policy->bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent;
    if (FallbackClasses != 1 || FallbackTags != 1 || !Fallback ||
        SceneCount != 1 || PolicyCount != 1 || GroundCount != 1 ||
        !Scene || !Policy || !Ground ||
        !Fallback->ValidateCopernicusTerrainFallback(ActorReport) ||
        !UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary::
            ValidateR29FacadeReplacementInLoadedV5DHybridMap(FacadeReport) ||
        !Policy->ValidateHybridContext(PolicyReport) ||
        !Ground->ValidateGroundVegetationRealism(GroundReport) ||
        !TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            ValidateAssets(AssetReport) ||
        !bNegativeAuthority ||
        !Scene->TerrainComponent ||
        Scene->TerrainComponent->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics ||
        !Ground->GroundMacroVariationOverlay ||
        Ground->GroundMacroVariationOverlay->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision)
    {
        OutReport = FString::Printf(
            TEXT("R29_COPERNICUS_TERRAIN_SUCCESSOR_INVALID fallbackClass=%d fallbackTag=%d scene=%d policy=%d ground=%d negativeAuthority=%s roster={%s} actor={%s} facadeReport={%s} policyReport={%s} groundReport={%s} assets={%s}"),
            FallbackClasses, FallbackTags, SceneCount, PolicyCount, GroundCount,
            bNegativeAuthority ? TEXT("true") : TEXT("false"),
            *RosterError, *ActorReport, *FacadeReport, *PolicyReport,
            *GroundReport, *AssetReport);
        return false;
    }
    OutReport = TEXT("R29_COPERNICUS_TERRAIN_SUCCESSOR_VALID exactFallbackOwners=1 cesiumPreferred=true localDemOnlyWhenProviderFallback=true exactCoreComplement=true sourceTerrainQueryAndPhysicsPreserved=true sourceTerrainRendererOnlySwappedAtRuntime=true assets=2 vertices=16641 triangles=32768 x100=true boundsXYcm=[-100000,100000] collision=false navigation=false sensor=false rf=false geospatial=false absoluteHeight=false survey=false bareEarth=false mapSavedByValidator=false nativeVisualAcceptance=false. ") +
        ActorReport + TEXT(" ") + FacadeReport + TEXT(" ") + AssetReport;
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary::
    BuildOrValidateCopernicusTerrainFallbackAssets(FString& OutReport)
{
    FString Existing;
    if (TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            ValidateAssets(Existing))
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_ASSET_BUILD_IDEMPOTENT_PASS created=0 mapsSaved=0 ") + Existing;
        return true;
    }
    UStaticMesh* Mesh = nullptr;
    UMaterial* Material = nullptr;
    FString Error;
    if (!TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            CreateFreshAssets(Mesh, Material, Error) ||
        !TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            ValidateAssets(Existing))
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_ASSET_BUILD_FAILED: ") +
            Error + TEXT(" ") + Existing;
        return false;
    }
    OutReport = TEXT("R29_COPERNICUS_TERRAIN_ASSET_BUILD_PASS created=2 mapsSaved=0 sourcePackagesModified=false ") + Existing;
    return true;
}

bool UTRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary::
    ValidateCopernicusTerrainFallbackAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
        ValidateAssets(OutReport);
}

bool UTRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary::
    ConfigureCopernicusTerrainFallbackActor(
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Actor,
        FString& OutReport)
{
    if (!IsValid(Actor))
    {
        OutReport = TEXT("R29 Copernicus configuration requires one valid actor.");
        return false;
    }
    FString AssetReport;
    if (!TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            ValidateAssets(AssetReport))
    {
        OutReport = AssetReport;
        return false;
    }
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(
        nullptr,
        *TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            GetMeshObjectPath());
    UMaterial* Material = LoadObject<UMaterial>(
        nullptr,
        *TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            GetMaterialObjectPath());
    FString Error;
    if (!Actor->ConfigureCopernicusTerrainFallback(Mesh, Material, Error) ||
        !Actor->ValidateCopernicusTerrainFallback(OutReport))
    {
        OutReport = TEXT("R29 Copernicus actor configuration failed: ") +
            Error + TEXT(" ") + OutReport;
        return false;
    }
    OutReport = TEXT("R29_COPERNICUS_TERRAIN_ACTOR_CONFIGURE_PASS mapSaved=false ") + OutReport;
    return true;
}

bool UTRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary::
    ApplyCopernicusTerrainFallbackToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedWorld(true, Error);
    FString MapFilename;
    if (!World ||
        !FPackageName::DoesPackageExist(TargetMapPackage, &MapFilename))
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_REFUSED_MAP: ") + Error;
        return false;
    }
    FString CurrentSha;
    int64 CurrentBytes = INDEX_NONE;
    if (!HashFile(MapFilename, CurrentSha, CurrentBytes, Error))
    {
        OutReport = Error;
        return false;
    }
    FString ExistingReport;
    if (ValidateSuccessorWorld(World, true, ExistingReport))
    {
        OutReport = FString::Printf(
            TEXT("IDEMPOTENT_R29_COPERNICUS_TERRAIN_ALREADY_VALID mapBytes=%lld mapSha256=%s oneSave=false. %s"),
            CurrentBytes, *CurrentSha, *ExistingReport);
        return true;
    }
    const FString ExpectedSha = ExpectedPredecessorSha256.ToUpper();
    if (ExpectedPredecessorBytes <= 0 || !IsSha256(ExpectedSha) ||
        CurrentBytes != ExpectedPredecessorBytes || CurrentSha != ExpectedSha)
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_REFUSED_PREDECESSOR_PIN: ") + ExistingReport;
        return false;
    }

    FString Backup = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    FString AllowedRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(), TransactionRelativeRoot));
    FPaths::NormalizeFilename(Backup);
    FPaths::NormalizeDirectoryName(AllowedRoot);
    FString BackupSha;
    int64 BackupBytes = INDEX_NONE;
    if (Backup.IsEmpty() || FPaths::IsSamePath(Backup, MapFilename) ||
        !FPaths::IsUnderDirectory(Backup, AllowedRoot) ||
        !HashFile(Backup, BackupSha, BackupBytes, Error) ||
        BackupBytes != ExpectedPredecessorBytes || BackupSha != ExpectedSha)
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_REFUSED_BACKUP: exact byte-identical external backup required under Saved/TRIAD/NativeTransactions/V5DR29CopernicusTerrainFallbackV1.");
        return false;
    }
    FString AssetReport;
    if (!TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory::
            ValidateAssets(AssetReport))
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_REFUSED_ASSETS: ") + AssetReport;
        return false;
    }
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Existing = nullptr;
    int32 ExistingClasses = 0;
    int32 ExistingTags = 0;
    if (!ResolveFallbackRoster(
            World, Existing, ExistingClasses, ExistingTags, Error) ||
        ExistingClasses != 0 || ExistingTags != 0)
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_REFUSED_ROSTER: mixed, duplicate, or impersonated owner.");
        return false;
    }

    FString GateMapSha;
    FString GateBackupSha;
    int64 GateMapBytes = INDEX_NONE;
    int64 GateBackupBytes = INDEX_NONE;
    if (World->GetOutermost()->IsDirty() ||
        !HashFile(MapFilename, GateMapSha, GateMapBytes, Error) ||
        !HashFile(Backup, GateBackupSha, GateBackupBytes, Error) ||
        GateMapBytes != ExpectedPredecessorBytes || GateMapSha != ExpectedSha ||
        GateBackupBytes != ExpectedPredecessorBytes ||
        GateBackupSha != ExpectedSha)
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_REFUSED_FINAL_MUTATION_GATE");
        return false;
    }

    FActorSpawnParameters Spawn;
    Spawn.Name = TEXT("TRIAD_IPV5D_R29_CopernicusTerrainFallback");
    Spawn.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    Spawn.ObjectFlags |= RF_Transactional;
    World->Modify();
    auto* Actor = World->SpawnActor<
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>(
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                StaticClass(),
            FTransform::Identity,
            Spawn);
    if (!Actor)
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_FAILED_SPAWN_NO_SAVE rollbackOwnedByWrapper=true");
        return false;
    }
#if WITH_EDITOR
    Actor->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R29 Copernicus Terrain - Visual Fallback Only"));
#endif
    FString ConfigureReport;
    FString PreSaveReport;
    if (!ConfigureCopernicusTerrainFallbackActor(Actor, ConfigureReport) ||
        !ValidateSuccessorWorld(World, false, PreSaveReport))
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_FAILED_PRE_SAVE_VALIDATION rollbackOwnedByWrapper=true ") +
            ConfigureReport + TEXT(" ") + PreSaveReport;
        return false;
    }
    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, TargetMapPackage))
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_FAILED_SAVE rollbackOwnedByWrapper=true");
        return false;
    }

    UWorld* Blank = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UWorld* Reloaded = Blank
        ? UEditorLoadingAndSavingUtils::LoadMap(MapFilename)
        : nullptr;
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString ColdReport;
    if (!Reloaded || !ValidateSuccessorWorld(Reloaded, true, ColdReport))
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_FAILED_COLD_VALIDATION rollbackOwnedByWrapper=true ") + ColdReport;
        return false;
    }
    FString SuccessorSha;
    FString FinalBackupSha;
    int64 SuccessorBytes = INDEX_NONE;
    int64 FinalBackupBytes = INDEX_NONE;
    if (!HashFile(MapFilename, SuccessorSha, SuccessorBytes, Error) ||
        !HashFile(Backup, FinalBackupSha, FinalBackupBytes, Error) ||
        (SuccessorBytes == ExpectedPredecessorBytes &&
            SuccessorSha == ExpectedSha) ||
        FinalBackupBytes != ExpectedPredecessorBytes ||
        FinalBackupSha != ExpectedSha)
    {
        OutReport = TEXT("R29_COPERNICUS_TERRAIN_APPLY_FAILED_FINAL_RECEIPT rollbackOwnedByWrapper=true");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("R29_COPERNICUS_TERRAIN_APPLY_PASS oneSave=true predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backupPreserved=true cesiumPreferred=true x100=true authority=false nativeVisualAcceptance=false. %s"),
        ExpectedPredecessorBytes, *ExpectedSha,
        SuccessorBytes, *SuccessorSha, *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DR29CopernicusTerrainFallbackEditorLibrary::
    ValidateCopernicusTerrainFallbackSuccessorMap(FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedWorld(true, Error);
    if (!World)
    {
        OutReport = Error;
        return false;
    }
    return ValidateSuccessorWorld(World, true, OutReport);
}
