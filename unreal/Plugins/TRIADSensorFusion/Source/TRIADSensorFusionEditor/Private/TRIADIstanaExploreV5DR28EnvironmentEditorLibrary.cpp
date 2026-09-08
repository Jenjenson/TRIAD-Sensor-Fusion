#include "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentAssetFactory.h"
#include "UObject/Package.h"

namespace
{
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));

UWorld* GetExactLoadedTargetWorld(FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString PackageName = World && World->GetOutermost()
        ? World->GetOutermost()->GetName()
        : FString();
    if (!World || World->WorldType != EWorldType::Editor ||
        PackageName != TargetMapPackage)
    {
        OutError = FString::Printf(
            TEXT("R28 map operation requires exact loaded editor map '%s'; actual='%s'."),
            *TargetMapPackage,
            *PackageName);
        return nullptr;
    }
    OutError.Reset();
    return World;
}

bool HasExactFreshSaveRoster(
    const TArray<UObject*>& Assets,
    FString& OutError)
{
    TArray<FString> Actual;
    TSet<UPackage*> Packages;
    for (UObject* Asset : Assets)
    {
        UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
        if (!Asset || !Package ||
            !Package->HasAnyPackageFlags(PKG_NewlyCreated) ||
            FPackageName::DoesPackageExist(Package->GetName()))
        {
            OutError = TEXT("An R28 save target is null, not newly created, or already persisted.");
            return false;
        }
        Actual.Add(Asset->GetPathName());
        Packages.Add(Package);
    }
    Actual.Sort();
    if (Actual !=
            TRIADIstanaExploreV5DR28EnvironmentAssetFactory::
                GetExpectedAssetObjectPaths() ||
        Packages.Num() != Assets.Num())
    {
        OutError = TEXT("The R28 fresh save roster is not the exact 13-asset isolated namespace.");
        return false;
    }
    return true;
}

bool ResolveExactActorRoster(
    UWorld* World,
    ATRIADIstanaExploreV5DR28EnvironmentActor*& OutActor,
    int32& OutClassCount,
    int32& OutTagCount,
    FString& OutError)
{
    OutActor = nullptr;
    OutClassCount = 0;
    OutTagCount = 0;
    if (!World)
    {
        OutError = TEXT("R28 actor resolution requires a world.");
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        const bool bExactClass = Candidate &&
            Candidate->GetClass() ==
                ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass();
        const bool bTagged = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag());
        if (bExactClass)
        {
            ++OutClassCount;
            OutActor = Cast<ATRIADIstanaExploreV5DR28EnvironmentActor>(Candidate);
        }
        if (bTagged)
        {
            ++OutTagCount;
            if (!bExactClass)
            {
                OutError = TEXT("The R28 tag is owned by a non-exact actor class.");
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary::
    EnsureR28EnvironmentAssets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DR28EnvironmentAssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R28_ENVIRONMENT_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_ASSET_BUILD_REFUSED: editor asset subsystem unavailable.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5DR28EnvironmentAssetFactory::CreateFreshAssets(
            FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_ASSET_BUILD_FAILED: ") + Error;
        return false;
    }
    if (!HasExactFreshSaveRoster(FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_ASSET_BUILD_FAILED_ROSTER: ") + Error;
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_ASSET_BUILD_FAILED_ATOMIC_SAVE: exact 13-asset save request failed; guarded native transaction must restore the initially absent R28 namespace.");
        return false;
    }
    for (UObject* Asset : FreshAssets)
    {
        if (UPackage* Package = Asset ? Asset->GetOutermost() : nullptr)
        {
            Package->SetDirtyFlag(false);
        }
    }
    FString SavedReport;
    if (!TRIADIstanaExploreV5DR28EnvironmentAssetFactory::ValidateAssets(
            SavedReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_ASSET_BUILD_FAILED_POST_SAVE_VALIDATION: ") +
            SavedReport;
        return false;
    }
    OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_ASSET_BUILD_PASS: exact 13-asset isolated namespace saved and validated; no actor, map, frozen surroundings/public-realm/outer-ground package, collision, navigation, sensor, or RF input was changed. ") +
        SavedReport;
    return true;
}

bool UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary::
    ValidateR28EnvironmentAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5DR28EnvironmentAssetFactory::ValidateAssets(
        OutReport);
}

bool UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary::
    ApplyR28EnvironmentToLoadedV5DHybridMap(FString& OutMessage)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_APPLY_REFUSED: ") + Error;
        return false;
    }
    ATRIADIstanaExploreV5DR28EnvironmentActor* ExistingActor = nullptr;
    int32 ClassCount = 0;
    int32 TagCount = 0;
    if (!ResolveExactActorRoster(
            World, ExistingActor, ClassCount, TagCount, Error) ||
        ClassCount != 0 || TagCount != 0)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R28_ENVIRONMENT_APPLY_REFUSED: exact zero-predecessor actor contract failed (class=%d tag=%d): %s"),
            ClassCount,
            TagCount,
            *Error);
        return false;
    }
    FString AssetMessage;
    if (!EnsureR28EnvironmentAssets(AssetMessage))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_APPLY_FAILED_ASSETS: ") + AssetMessage;
        return false;
    }
    FTRIADIstanaExploreV5DR28EnvironmentAssets Assets;
    if (!TRIADIstanaExploreV5DR28EnvironmentAssetFactory::
            LoadValidatedRuntimeContract(Assets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_APPLY_FAILED_RUNTIME_CONTRACT: ") + Error;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIAD_IPV5D_R28_Environment");
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transactional;
    ATRIADIstanaExploreV5DR28EnvironmentActor* Actor =
        World->SpawnActor<ATRIADIstanaExploreV5DR28EnvironmentActor>(
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Actor)
    {
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_APPLY_FAILED: actor spawn returned null.");
        return false;
    }
#if WITH_EDITOR
    Actor->SetActorLabel(TEXT("TRIAD V5D R28 Environment (Render Only)"));
#endif
    if (!Actor->ConfigureR28Environment(Assets, false, Error))
    {
        World->DestroyActor(Actor);
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_APPLY_FAILED_CONFIGURE: ") + Error;
        return false;
    }
    FString ActorReport;
    if (!Actor->ValidateR28Environment(ActorReport))
    {
        World->DestroyActor(Actor);
        OutMessage = TEXT("EXPLORE_V5D_R28_ENVIRONMENT_APPLY_FAILED_VALIDATE: ") + ActorReport;
        return false;
    }
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_APPLY_PASS mapSaved=false providerReady=false componentsVisible=3; caller owns the combined transaction's single map save. ") +
        ActorReport + TEXT(" ") + AssetMessage;
    return true;
}

bool UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary::
    ValidateR28EnvironmentInLoadedV5DHybridMap(FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = Error;
        return false;
    }
    ATRIADIstanaExploreV5DR28EnvironmentActor* Actor = nullptr;
    int32 ClassCount = 0;
    int32 TagCount = 0;
    if (!ResolveExactActorRoster(World, Actor, ClassCount, TagCount, Error) ||
        ClassCount != 1 || TagCount != 1 || !Actor)
    {
        OutReport = FString::Printf(
            TEXT("R28 loaded-map actor roster is not exact: class=%d tag=%d %s"),
            ClassCount,
            TagCount,
            *Error);
        return false;
    }
    FString AssetReport;
    FString ActorReport;
    if (!TRIADIstanaExploreV5DR28EnvironmentAssetFactory::ValidateAssets(
            AssetReport) ||
        !Actor->ValidateR28Environment(ActorReport))
    {
        OutReport = AssetReport + TEXT(" ") + ActorReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_MAP_VALID classActors=1 taggedActors=1 identity=true providerVisibilityMirrored=true mapSavedByThisValidator=false. ") +
        ActorReport + TEXT(" ") + AssetReport;
    return true;
}
