#include "TRIADIstanaExploreV5DR29VegetationEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
#include "TRIADIstanaExploreV5DPublicRealmActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DR29VegetationAssetFactory.h"
#include "TRIADIstanaExploreV5DHybridEditorLibrary.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Ssl.h"

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
const FString R29TransactionRelativeRoot(
    TEXT("TRIAD/NativeTransactions/V5DR29TropicalVegetationV1"));
const FString R29FacadeEnvironmentClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreV5DR29FacadeEnvironmentActor"));
const FName R29FacadeEnvironmentActorTag(
    TEXT("TRIADIstanaExploreV5DR29FacadeEnvironment"));
const FName R29FacadeValidationFunction(
    TEXT("ValidateR29FacadeEnvironment"));
const FName R29FacadeProviderReadyProperty(TEXT("bProviderReady"));

struct FVegetationRoster
{
    ATRIADIstanaExploreV5DLandmarkVegetationActor* R28 = nullptr;
    ATRIADIstanaExploreV5DR29VegetationActor* R29 = nullptr;
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
};

struct FEnvironmentOwnerRoster
{
    ATRIADIstanaExploreV5DR28EnvironmentActor* R28 = nullptr;
    AActor* R29Facade = nullptr;
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29FacadeClassCount = 0;
    int32 R29FacadeTagCount = 0;
};

template <typename T>
T* FindExactClass(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    T* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (IsValid(Candidate) &&
            Candidate->GetClass() == T::StaticClass())
        {
            Result = Cast<T>(Candidate);
            ++OutCount;
        }
    }
    return Result;
}

bool ResolveVegetationRoster(
    UWorld* World,
    FVegetationRoster& OutRoster,
    FString& OutError)
{
    OutRoster = FVegetationRoster{};
    if (!World)
    {
        OutError = TEXT("R29 vegetation roster requires a world.");
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        const bool bExactR28 = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass();
        const bool bExactR29 = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29VegetationActor::StaticClass();
        const bool bR28Tagged = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DLandmarkVegetationActor::ExpectedActorTag());
        const bool bR29Tagged = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29VegetationActor::ExpectedActorTag());
        if (bExactR28)
        {
            OutRoster.R28 = Cast<ATRIADIstanaExploreV5DLandmarkVegetationActor>(
                Candidate);
            ++OutRoster.R28ClassCount;
        }
        if (bExactR29)
        {
            OutRoster.R29 = Cast<ATRIADIstanaExploreV5DR29VegetationActor>(
                Candidate);
            ++OutRoster.R29ClassCount;
        }
        OutRoster.R28TagCount += bR28Tagged ? 1 : 0;
        OutRoster.R29TagCount += bR29Tagged ? 1 : 0;
        if ((bR28Tagged && !bExactR28) ||
            (bR29Tagged && !bExactR29) ||
            (bR28Tagged && bR29Tagged))
        {
            OutError = TEXT("R29 vegetation roster found an impersonated or dual-owned render-owner tag.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ResolveEnvironmentOwnerRoster(
    UWorld* World,
    FEnvironmentOwnerRoster& OutRoster,
    FString& OutError)
{
    OutRoster = FEnvironmentOwnerRoster{};
    if (!World)
    {
        OutError = TEXT("R29 vegetation environment-owner roster requires a world.");
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        const bool bExactR28 = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass();
        // This narrow string/reflection seam deliberately keeps the standalone
        // vegetation transaction independent of the separately promoted R29
        // facade source. A subclass has a different exact UClass path.
        const bool bExactR29Facade =
            Candidate->GetClass()->GetPathName() ==
                R29FacadeEnvironmentClassPath;
        const bool bR28Tagged = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag());
        const bool bR29FacadeTagged = Candidate->Tags.Contains(
            R29FacadeEnvironmentActorTag);
        if (bExactR28)
        {
            OutRoster.R28 =
                Cast<ATRIADIstanaExploreV5DR28EnvironmentActor>(Candidate);
            ++OutRoster.R28ClassCount;
        }
        if (bExactR29Facade)
        {
            OutRoster.R29Facade = Candidate;
            ++OutRoster.R29FacadeClassCount;
        }
        OutRoster.R28TagCount += bR28Tagged ? 1 : 0;
        OutRoster.R29FacadeTagCount += bR29FacadeTagged ? 1 : 0;
        if ((bR28Tagged && !bExactR28) ||
            (bR29FacadeTagged && !bExactR29Facade) ||
            (bR28Tagged && bR29FacadeTagged))
        {
            OutError = TEXT("R29 vegetation found an impersonated or dual-owned environment-owner tag.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateCombinedWorldR29FacadeOwner(
    AActor* Actor,
    FString& OutReport)
{
    if (!IsValid(Actor) ||
        Actor->GetClass()->GetPathName() != R29FacadeEnvironmentClassPath)
    {
        OutReport = TEXT("R29 vegetation combined-world validation requires the exact R29 facade environment class.");
        return false;
    }
    UFunction* Function = Actor->FindFunction(R29FacadeValidationFunction);
    FStrProperty* ReportProperty = Function
        ? FindFProperty<FStrProperty>(Function, TEXT("OutReport"))
        : nullptr;
    FBoolProperty* ReturnProperty = Function
        ? CastField<FBoolProperty>(Function->GetReturnProperty())
        : nullptr;
    if (!Function || !ReportProperty || !ReturnProperty ||
        !ReportProperty->HasAnyPropertyFlags(CPF_OutParm) ||
        !ReturnProperty->HasAnyPropertyFlags(CPF_ReturnParm))
    {
        OutReport = TEXT("R29 facade actor does not expose the exact reflected validation contract.");
        return false;
    }
    FStructOnScope Parameters(Function);
    uint8* ParameterMemory = Parameters.GetStructMemory();
    if (!ParameterMemory)
    {
        OutReport = TEXT("R29 facade reflected validation parameter allocation failed.");
        return false;
    }
    Actor->ProcessEvent(Function, ParameterMemory);
    OutReport = ReportProperty->GetPropertyValue_InContainer(ParameterMemory);
    return ReturnProperty->GetPropertyValue_InContainer(ParameterMemory);
}

bool ReadCombinedWorldR29FacadeProviderReady(
    const AActor* Actor,
    bool& OutProviderReady,
    FString& OutError)
{
    const FBoolProperty* Property = IsValid(Actor)
        ? FindFProperty<FBoolProperty>(
            Actor->GetClass(),
            R29FacadeProviderReadyProperty)
        : nullptr;
    if (!Property ||
        Actor->GetClass()->GetPathName() != R29FacadeEnvironmentClassPath)
    {
        OutError = TEXT("R29 facade actor does not expose the exact provider-ready property contract.");
        return false;
    }
    OutProviderReady = Property->GetPropertyValue_InContainer(Actor);
    OutError.Reset();
    return true;
}

bool ResolvePublicRealmRoster(
    UWorld* World,
    ATRIADIstanaExploreV5DPublicRealmActor*& OutPublicRealm,
    int32& OutClassCount,
    int32& OutTagCount,
    FString& OutError)
{
    OutPublicRealm = nullptr;
    OutClassCount = 0;
    OutTagCount = 0;
    if (!World)
    {
        OutError = TEXT("R29 vegetation public-realm roster requires a world.");
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        const bool bExact = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DPublicRealmActor::StaticClass();
        const bool bTagged = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DPublicRealmActor::ExpectedActorTag());
        if (bExact)
        {
            OutPublicRealm =
                Cast<ATRIADIstanaExploreV5DPublicRealmActor>(Candidate);
            ++OutClassCount;
        }
        OutTagCount += bTagged ? 1 : 0;
        if (bTagged && !bExact)
        {
            OutError = TEXT("R29 vegetation found an impersonated public-realm owner tag.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

FString BytesToHex(const uint8* Bytes, int32 Count)
{
    FString Result;
    Result.Reserve(Count * 2);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result += FString::Printf(TEXT("%02X"), Bytes[Index]);
    }
    return Result;
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
        OutError = TEXT("Could not read R29 transaction file: ") + Filename;
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
        OutError = TEXT("Could not compute R29 transaction SHA-256: ") +
            Filename;
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    OutSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("R29 transaction SHA-256 admission requires WITH_SSL.");
    return false;
#endif
    OutError.Reset();
    return true;
}

bool IsValidSha256(const FString& Candidate)
{
    if (Candidate.Len() != 64)
    {
        return false;
    }
    for (TCHAR Character : Candidate)
    {
        if (!FChar::IsHexDigit(Character))
        {
            return false;
        }
    }
    return true;
}

UWorld* GetExactLoadedTargetWorld(bool bRequireClean, FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    const FString LogicalPackage = Package
        ? UWorld::RemovePIEPrefix(Package->GetName())
        : FString();
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress() || !World ||
        World->WorldType != EWorldType::Editor || !World->PersistentLevel ||
        !Package || LogicalPackage != TargetMapPackage ||
        (bRequireClean && Package->IsDirty()))
    {
        OutError = FString::Printf(
            TEXT("R29 transaction requires the exact %s non-PIE editor map; actual=%s clean=%s."),
            *TargetMapPackage,
            *LogicalPackage,
            Package && !Package->IsDirty() ? TEXT("true") : TEXT("false"));
        return nullptr;
    }
    OutError.Reset();
    return World;
}

bool ValidateR29SuccessorWorld(
    UWorld* World,
    bool bRequireClean,
    FString& OutReport)
{
    UPackage* Package = World ? World->GetOutermost() : nullptr;
    if (!World || !Package ||
        UWorld::RemovePIEPrefix(Package->GetName()) != TargetMapPackage ||
        (bRequireClean && Package->IsDirty()))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_WORLD_INVALID_MAP_STATE");
        return false;
    }

    FString AssetReport;
    if (!TRIADIstanaExploreV5DR29VegetationAssetFactory::ValidateAssets(
            AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_WORLD_INVALID_ASSETS: ") +
            AssetReport;
        return false;
    }

    FVegetationRoster Roster;
    FString RosterError;
    if (!ResolveVegetationRoster(World, Roster, RosterError) ||
        Roster.R28ClassCount != 0 || Roster.R28TagCount != 0 ||
        Roster.R29ClassCount != 1 || Roster.R29TagCount != 1 || !Roster.R29)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_WORLD_INVALID_RENDER_OWNER_ROSTER r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d error={%s}"),
            Roster.R28ClassCount,
            Roster.R28TagCount,
            Roster.R29ClassCount,
            Roster.R29TagCount,
            *RosterError);
        return false;
    }

    FString ActorReport;
    if (!Roster.R29->ValidateR29Vegetation(ActorReport) ||
        Roster.R29->bCollisionOrNavigationAuthority ||
        Roster.R29->bSensorRfOrGeospatialAuthority ||
        !Roster.R29->bAppearanceOnly)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_WORLD_INVALID_ACTOR: ") +
            ActorReport;
        return false;
    }

    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 PublicRealmClassCount = 0;
    int32 PublicRealmTagCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactClass<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV5DContextPolicyActor* Policy =
        FindExactClass<ATRIADIstanaExploreV5DContextPolicyActor>(
            World,
            PolicyCount);
    FEnvironmentOwnerRoster EnvironmentRoster;
    FString EnvironmentRosterError;
    const bool bEnvironmentRosterResolved = ResolveEnvironmentOwnerRoster(
        World,
        EnvironmentRoster,
        EnvironmentRosterError);
    ATRIADIstanaExploreV5DPublicRealmActor* PublicRealm = nullptr;
    FString PublicRealmRosterError;
    const bool bPublicRealmRosterResolved = ResolvePublicRealmRoster(
        World,
        PublicRealm,
        PublicRealmClassCount,
        PublicRealmTagCount,
        PublicRealmRosterError);
    const bool bR28EnvironmentOwner =
        EnvironmentRoster.R28ClassCount == 1 &&
        EnvironmentRoster.R28TagCount == 1 &&
        EnvironmentRoster.R29FacadeClassCount == 0 &&
        EnvironmentRoster.R29FacadeTagCount == 0 &&
        EnvironmentRoster.R28;
    const bool bR29FacadeEnvironmentOwner =
        EnvironmentRoster.R28ClassCount == 0 &&
        EnvironmentRoster.R28TagCount == 0 &&
        EnvironmentRoster.R29FacadeClassCount == 1 &&
        EnvironmentRoster.R29FacadeTagCount == 1 &&
        EnvironmentRoster.R29Facade;
    FString EnvironmentReport;
    FString FacadeReport;
    FString PublicRealmReport;
    FString R29FacadeProviderError;
    bool bR29FacadeProviderReady = false;
    const bool bR29FacadeProviderReadable =
        !bR29FacadeEnvironmentOwner ||
        ReadCombinedWorldR29FacadeProviderReady(
            EnvironmentRoster.R29Facade,
            bR29FacadeProviderReady,
            R29FacadeProviderError);
    const bool bEnvironmentOwnerValid =
        (bR28EnvironmentOwner &&
         EnvironmentRoster.R28->ValidateR28Environment(EnvironmentReport)) ||
        (bR29FacadeEnvironmentOwner &&
         ValidateCombinedWorldR29FacadeOwner(
             EnvironmentRoster.R29Facade,
             EnvironmentReport));
    const bool bEnvironmentProviderStateMatchesPolicy =
        Policy &&
        ((bR28EnvironmentOwner &&
         EnvironmentRoster.R28->bProviderReady ==
              Policy->bLocalBuildingFallbackCurrentlyHidden) ||
         (bR29FacadeEnvironmentOwner && bR29FacadeProviderReadable &&
          bR29FacadeProviderReady ==
              Policy->bLocalBuildingFallbackCurrentlyHidden));
    const bool bNegativeProviderAuthority =
        Policy && Policy->bCesiumLayerIsVisualOnly &&
        !Policy->bCesiumCollisionNavigationSensorOrRfAuthority &&
        !Policy->bTriadReadSerializedOrLoggedProviderToken &&
        !Policy->bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent;
    if (SceneCount != 1 || PolicyCount != 1 || !Scene || !Policy ||
        !bEnvironmentRosterResolved ||
        bR28EnvironmentOwner == bR29FacadeEnvironmentOwner ||
        !bEnvironmentOwnerValid || !bEnvironmentProviderStateMatchesPolicy ||
        !bPublicRealmRosterResolved || PublicRealmClassCount != 1 ||
        PublicRealmTagCount != 1 || !PublicRealm ||
        !PublicRealm->ValidatePublicRealm(PublicRealmReport) ||
        !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            Scene,
            FacadeReport) ||
        !bNegativeProviderAuthority)
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_WORLD_INVALID_RETAINED_CONTEXT scene=%d policy=%d r28EnvironmentClass=%d r28EnvironmentTag=%d r29FacadeClass=%d r29FacadeTag=%d publicRealmClass=%d publicRealmTag=%d exactlyOneEnvironmentOwner=%s environmentProviderStateMatchesPolicy=%s negativeProviderAuthority=%s environmentRosterError={%s} r29FacadeProviderError={%s} publicRealmRosterError={%s} environment={%s} publicRealm={%s} facade={%s}"),
            SceneCount,
            PolicyCount,
            EnvironmentRoster.R28ClassCount,
            EnvironmentRoster.R28TagCount,
            EnvironmentRoster.R29FacadeClassCount,
            EnvironmentRoster.R29FacadeTagCount,
            PublicRealmClassCount,
            PublicRealmTagCount,
            bR28EnvironmentOwner != bR29FacadeEnvironmentOwner
                ? TEXT("true") : TEXT("false"),
            bEnvironmentProviderStateMatchesPolicy
                ? TEXT("true") : TEXT("false"),
            bNegativeProviderAuthority ? TEXT("true") : TEXT("false"),
            *EnvironmentRosterError,
            *R29FacadeProviderError,
            *PublicRealmRosterError,
            *EnvironmentReport,
            *PublicRealmReport,
            *FacadeReport);
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_WORLD_VALID exactR29RenderOwners=1 exactR28RenderOwners=0 mutualExclusion=true r28WorldPositionsAndCensusPreserved=true grassInstances=6144 treeInstances=7 grassGeographyExact=true treeGeographyExact=true environmentOwner=%s exactlyOneEnvironmentOwner=true environmentR28Retained=%s environmentR29FacadeAccepted=%s ambiguousEnvironmentRoster=false publicRealmActors=1 publicRealmTags=1 publicRealmInvariantPreserved=true contextFacadeR25Retained=true providerVisualOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false geospatialAuthority=false mapSavedByThisValidator=false visualCaptureAccepted=false captureRevalidationRequired=true. "),
        bR28EnvironmentOwner ? TEXT("R28Environment") : TEXT("R29FacadeEnvironment"),
        bR28EnvironmentOwner ? TEXT("true") : TEXT("false"),
        bR29FacadeEnvironmentOwner ? TEXT("true") : TEXT("false")) +
        ActorReport + TEXT(" ") + AssetReport + TEXT(" ") +
        EnvironmentReport + TEXT(" ") + PublicRealmReport + TEXT(" ") +
        FacadeReport;
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR29VegetationEditorLibrary::
    BuildOrValidateR29VegetationAssets(FString& OutReport)
{
    // Do not call ValidateAssets before CreateFreshAssets. LoadObject on a
    // missing object path leaves its package resident, which makes a genuinely
    // fresh destination look occupied to the factory's fail-closed checks.
    int32 ExistingPackageCount = 0;
    for (const FString& ObjectPath :
         TRIADIstanaExploreV5DR29VegetationAssetFactory::
             GetExpectedAssetObjectPaths())
    {
        ExistingPackageCount += FPackageName::DoesPackageExist(
            FPackageName::ObjectPathToPackageName(ObjectPath)) ? 1 : 0;
    }

    TArray<UObject*> Assets;
    FString Error;
    if (!TRIADIstanaExploreV5DR29VegetationAssetFactory::
            CreateFreshAssets(Assets, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSET_BUILD_FAILED: ") +
            Error;
        return false;
    }
    FString ExistingReport;
    if (Assets.Num() != 7 ||
        !TRIADIstanaExploreV5DR29VegetationAssetFactory::
            ValidateAssets(ExistingReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSET_BUILD_FAILED_POST_VALIDATION: ") +
            ExistingReport;
        return false;
    }
    if (ExistingPackageCount == 7)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSET_BUILD_IDEMPOTENT_PASS createdAssets=0 mapsSaved=0 ") +
            ExistingReport;
        return true;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_ASSET_BUILD_PASS createdAssets=7 importedModeledBladeMeshes=3 isolatedMaterialDerivatives=4 sourcePackagesModified=false mapsSaved=0 ") +
        ExistingReport;
    return true;
}

bool UTRIADIstanaExploreV5DR29VegetationEditorLibrary::
    ValidateR29VegetationAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5DR29VegetationAssetFactory::
        ValidateAssets(OutReport);
}

bool UTRIADIstanaExploreV5DR29VegetationEditorLibrary::
    ConfigureR29VegetationActor(
        ATRIADIstanaExploreV5DR29VegetationActor* Actor,
        FString& OutReport)
{
    if (!IsValid(Actor))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_CONFIGURE_REFUSED: actor is null or invalid.");
        return false;
    }
    FTRIADIstanaExploreV5DR29VegetationAssets Assets;
    FString Error;
    if (!TRIADIstanaExploreV5DR29VegetationAssetFactory::
            LoadValidatedRuntimeContract(Assets, Error) ||
        !Actor->ConfigureR29Vegetation(Assets, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_CONFIGURE_FAILED: ") +
            Error;
        return false;
    }
    if (!Actor->ValidateR29Vegetation(OutReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_CONFIGURE_FAILED_POST_VALIDATION: ") +
            OutReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_CONFIGURE_PASS mapMutation=false assetsCreated=false ") +
        OutReport;
    return true;
}

bool UTRIADIstanaExploreV5DR29VegetationEditorLibrary::
    ApplyR29VegetationSuccessorToLoadedHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(true, Error);
    FString DestinationFilename;
    if (!World || !FPackageName::DoesPackageExist(
            TargetMapPackage,
            &DestinationFilename))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_MAP_STATE: ") +
            Error;
        return false;
    }

    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    if (!HashFileSha256(
            DestinationFilename,
            CurrentSha256,
            CurrentBytes,
            Error))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_MAP_HASH: ") +
            Error;
        return false;
    }

    // Semantic idempotence precedes predecessor-pin admission. A complete R29
    // successor may be validated again, but a partial or mixed R28/R29 roster
    // can never enter this path.
    FString ExistingR29Report;
    if (ValidateR29SuccessorWorld(World, true, ExistingR29Report))
    {
        OutReport = FString::Printf(
            TEXT("IDEMPOTENT_EXPLORE_V5D_R29_VEGETATION_ALREADY_VALID currentBytes=%lld currentSha256=%s oneSave=false exactR29RenderOwners=1 exactR28RenderOwners=0 visualCaptureAccepted=false captureRevalidationRequired=true. %s"),
            CurrentBytes,
            *CurrentSha256,
            *ExistingR29Report);
        return true;
    }

    const FString ExpectedSha256 = ExpectedPredecessorSha256.ToUpper();
    if (ExpectedPredecessorBytes <= 0 || !IsValidSha256(ExpectedSha256) ||
        CurrentBytes != ExpectedPredecessorBytes ||
        CurrentSha256 != ExpectedSha256)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_PREDECESSOR_PIN expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s existingR29={%s}"),
            ExpectedPredecessorBytes,
            CurrentBytes,
            *ExpectedSha256,
            *CurrentSha256,
            *ExistingR29Report);
        return false;
    }

    FString BackupFilename = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    FPaths::NormalizeFilename(BackupFilename);
    FString AllowedBackupRoot = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), R29TransactionRelativeRoot));
    FPaths::NormalizeDirectoryName(AllowedBackupRoot);
    FString BackupSha256;
    int64 BackupBytes = INDEX_NONE;
    FString BackupError;
    if (VerifiedExternalBackupFilename.IsEmpty() ||
        FPaths::IsSamePath(BackupFilename, DestinationFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, AllowedBackupRoot) ||
        !HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            BackupError) ||
        BackupBytes != ExpectedPredecessorBytes ||
        BackupSha256 != ExpectedSha256)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_BACKUP exact byte-identical backup under Saved/%s required backup=%s bytes=%lld sha256=%s error={%s}"),
            *R29TransactionRelativeRoot,
            *BackupFilename,
            BackupBytes,
            *BackupSha256,
            *BackupError);
        return false;
    }

    // Validate the complete R29 asset/layout contract before touching the R28
    // actor. BuildDeterministicLayout performs exact multiset comparisons for
    // all 6,144 grass translations and all seven inherited tree anchors.
    FString AssetReport;
    FTRIADIstanaExploreV5DR29VegetationLayout CandidateLayout;
    if (!TRIADIstanaExploreV5DR29VegetationAssetFactory::ValidateAssets(
            AssetReport) ||
        !ATRIADIstanaExploreV5DR29VegetationActor::BuildDeterministicLayout(
            CandidateLayout,
            Error) ||
        CandidateLayout.GrassTotal() !=
            ATRIADIstanaExploreV5DR29VegetationActor::
                ExpectedGrassInstanceCount() ||
        CandidateLayout.TreeTotal() !=
            ATRIADIstanaExploreV5DR29VegetationActor::
                ExpectedTreeInstanceCount())
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_ASSET_OR_GEOGRAPHY_VALIDATION: ") +
            AssetReport + TEXT(" ") + Error;
        return false;
    }

    // Reuse the cold-proven public R28 validator as the semantic predecessor
    // gate. No R28 actor is removed unless the entire predecessor world and
    // its retained provider/context contracts are valid.
    FString R28PredecessorReport;
    if (!UTRIADIstanaExploreV5DHybridEditorLibrary::
            ValidateIstanaExploreV5DR28VisualSuccessorMap(
                R28PredecessorReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_R28_PREDECESSOR_CONTRACT: ") +
            R28PredecessorReport;
        return false;
    }

    FVegetationRoster Roster;
    if (!ResolveVegetationRoster(World, Roster, Error) ||
        Roster.R28ClassCount != 1 || Roster.R28TagCount != 1 || !Roster.R28 ||
        Roster.R29ClassCount != 0 || Roster.R29TagCount != 0)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_PREDECESSOR_ROSTER r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d error={%s}"),
            Roster.R28ClassCount,
            Roster.R28TagCount,
            Roster.R29ClassCount,
            Roster.R29TagCount,
            *Error);
        return false;
    }
    FString R28ActorReport;
    if (!Roster.R28->ValidateLandmarkVegetationR28(R28ActorReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_R28_ACTOR: ") +
            R28ActorReport;
        return false;
    }

    // Close the validation-to-mutation window. The clean map and the wrapper's
    // external rollback copy must still match the caller-supplied R28 pin.
    FString GateMapSha256;
    FString GateBackupSha256;
    int64 GateMapBytes = INDEX_NONE;
    int64 GateBackupBytes = INDEX_NONE;
    FString GateMapError;
    FString GateBackupError;
    if (!World->GetOutermost() || World->GetOutermost()->IsDirty() ||
        !HashFileSha256(
            DestinationFilename,
            GateMapSha256,
            GateMapBytes,
            GateMapError) ||
        !HashFileSha256(
            BackupFilename,
            GateBackupSha256,
            GateBackupBytes,
            GateBackupError) ||
        GateMapBytes != ExpectedPredecessorBytes ||
        GateMapSha256 != ExpectedSha256 ||
        GateBackupBytes != ExpectedPredecessorBytes ||
        GateBackupSha256 != ExpectedSha256)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_FINAL_MUTATION_GATE packageClean=%s mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapError={%s} backupError={%s}"),
            World->GetOutermost() && !World->GetOutermost()->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            GateMapBytes,
            *GateMapSha256,
            GateBackupBytes,
            *GateBackupSha256,
            *GateMapError,
            *GateBackupError);
        return false;
    }

    World->Modify();
    Roster.R28->Modify();
    if (!World->DestroyActor(Roster.R28, true, true))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_REMOVE_R28_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
    FVegetationRoster PostRemoveRoster;
    if (!ResolveVegetationRoster(World, PostRemoveRoster, Error) ||
        PostRemoveRoster.R28ClassCount != 0 ||
        PostRemoveRoster.R28TagCount != 0 ||
        PostRemoveRoster.R29ClassCount != 0 ||
        PostRemoveRoster.R29TagCount != 0)
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_POST_REMOVE_ROSTER_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            Error;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIAD_IPV5D_R29_TropicalVegetation");
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transactional;
    ATRIADIstanaExploreV5DR29VegetationActor* R29Actor =
        World->SpawnActor<ATRIADIstanaExploreV5DR29VegetationActor>(
            ATRIADIstanaExploreV5DR29VegetationActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!R29Actor)
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_SPAWN_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
#if WITH_EDITOR
    R29Actor->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R29 Tropical Vegetation - Render Only"));
#endif
    FString ConfigureReport;
    if (!ConfigureR29VegetationActor(R29Actor, ConfigureReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_CONFIGURE_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ConfigureReport;
        return false;
    }

    FString PreSaveWorldReport;
    if (!ValidateR29SuccessorWorld(World, false, PreSaveWorldReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_PRE_SAVE_WORLD_VALIDATION externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            PreSaveWorldReport;
        return false;
    }

    FString PreSaveMapSha256;
    FString PreSaveBackupSha256;
    int64 PreSaveMapBytes = INDEX_NONE;
    int64 PreSaveBackupBytes = INDEX_NONE;
    FString PreSaveMapError;
    FString PreSaveBackupError;
    if (!HashFileSha256(
            DestinationFilename,
            PreSaveMapSha256,
            PreSaveMapBytes,
            PreSaveMapError) ||
        !HashFileSha256(
            BackupFilename,
            PreSaveBackupSha256,
            PreSaveBackupBytes,
            PreSaveBackupError) ||
        PreSaveMapBytes != ExpectedPredecessorBytes ||
        PreSaveMapSha256 != ExpectedSha256 ||
        PreSaveBackupBytes != ExpectedPredecessorBytes ||
        PreSaveBackupSha256 != ExpectedSha256)
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_PRE_SAVE_PIN_GATE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, TargetMapPackage))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    UWorld* UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UPackage* UnloadPackage = UnloadWorld ? UnloadWorld->GetOutermost() : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) == TargetMapPackage)
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_COLD_UNLOAD externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
    UWorld* ReloadedWorld =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedWorld)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    FString ColdReport;
    if (!ReloadedWorld || !ValidateR29SuccessorWorld(
            ReloadedWorld,
            true,
            ColdReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_COLD_VALIDATION externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ColdReport;
        return false;
    }

    FString SuccessorSha256;
    FString FinalBackupSha256;
    int64 SuccessorBytes = INDEX_NONE;
    int64 FinalBackupBytes = INDEX_NONE;
    FString SuccessorError;
    FString FinalBackupError;
    const bool bSuccessorChanged = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorError) &&
        SuccessorBytes > 0 && IsValidSha256(SuccessorSha256) &&
        (SuccessorBytes != ExpectedPredecessorBytes ||
         SuccessorSha256 != ExpectedSha256);
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        FinalBackupSha256,
        FinalBackupBytes,
        FinalBackupError) &&
        FinalBackupBytes == ExpectedPredecessorBytes &&
        FinalBackupSha256 == ExpectedSha256;
    if (!bSuccessorChanged || !bBackupPreserved)
    {
        OutReport = TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_FAILED_FINAL_RECEIPT externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("EXPLORE_V5D_R29_VEGETATION_APPLY_PASS oneSave=true r28RenderOwnersRemoved=1 r29RenderOwnersAdded=1 coexistenceObserved=false assetsValidatedBeforeMutation=true geographyValidatedBeforeMutation=true grassGeographyExact=true treeGeographyExact=true grassInstances=6144 treeInstances=7 sourceAssetsModified=false collision=false navigation=false sensorAuthority=false rfAuthority=false geospatialAuthority=false externalBackupVerified=true rollbackOwnedByWrapper=true predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s backup=%s visualCaptureAccepted=false captureRevalidationRequired=true. predecessor={%s} configure={%s} cold={%s}"),
        ExpectedPredecessorBytes,
        *ExpectedSha256,
        SuccessorBytes,
        *SuccessorSha256,
        *BackupFilename,
        *R28PredecessorReport,
        *ConfigureReport,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DR29VegetationEditorLibrary::
    ValidateR29VegetationSuccessorMap(FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(true, Error);
    if (!World)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_SUCCESSOR_MAP_INVALID: ") +
            Error;
        return false;
    }
    FString WorldReport;
    if (!ValidateR29SuccessorWorld(World, true, WorldReport))
    {
        OutReport = WorldReport;
        return false;
    }
    FString Filename;
    FString MapSha256;
    int64 MapBytes = INDEX_NONE;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &Filename) ||
        !HashFileSha256(Filename, MapSha256, MapBytes, Error) ||
        MapBytes <= 0 || !IsValidSha256(MapSha256))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_SUCCESSOR_MAP_INVALID_RECEIPT: ") +
            Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_SUCCESSOR_MAP_VALID cleanSavedMap=true bytes=%lld sha256=%s exactR29RenderOwners=1 exactR28RenderOwners=0 mutualExclusion=true grassGeographyExact=true treeGeographyExact=true collision=false navigation=false sensorAuthority=false rfAuthority=false geospatialAuthority=false mapSavedByThisValidator=false visualCaptureAccepted=false captureRevalidationRequired=true. %s"),
        MapBytes,
        *MapSha256,
        *WorldReport);
    return true;
}
