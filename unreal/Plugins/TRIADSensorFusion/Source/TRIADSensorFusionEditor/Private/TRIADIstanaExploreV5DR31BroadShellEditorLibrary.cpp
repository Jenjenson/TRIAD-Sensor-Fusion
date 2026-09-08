#include "TRIADIstanaExploreV5DR31BroadShellEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Ssl.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"
#include "TRIADIstanaExploreV5DR31BroadShellAssetFactory.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
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

bool IsValidSha256(const FString& Value)
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
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
    {
        OutError = TEXT("Could not read guarded R31 file: ") + Filename;
        return false;
    }
    OutBytes = Bytes.Num();
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) == nullptr)
    {
        OutError = TEXT("SHA-256 failed for guarded R31 file: ") + Filename;
        return false;
    }
    OutSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH);
#else
    OutError = TEXT("R31 broad-shell commit requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
}

UWorld* GetExactLoadedTargetWorld(FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString PackageName = World && World->GetOutermost()
        ? World->GetOutermost()->GetName()
        : FString();
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress() || !World ||
        World->WorldType != EWorldType::Editor || !World->PersistentLevel ||
        PackageName != TargetMapPackage)
    {
        OutError = FString::Printf(
            TEXT("R31 broad-shell operation requires exact loaded non-PIE editor map '%s'; actual='%s'."),
            *TargetMapPackage, *PackageName);
        return nullptr;
    }
    OutError.Reset();
    return World;
}

struct FR31WorldRoster
{
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ATRIADIstanaExploreV5DR30FacadeLookdevActor* R30 = nullptr;
    ATRIADIstanaExploreV5DR29VegetationActor* Vegetation = nullptr;
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Terrain = nullptr;
    ATRIADIstanaExploreV5DTreeRealismActor* Trees = nullptr;
    UStaticMeshComponent* ExactV2Component = nullptr;
    int32 PolicyCount = 0;
    int32 SceneCount = 0;
    int32 R30ClassCount = 0;
    int32 R30TagCount = 0;
    int32 R28FacadeClassCount = 0;
    int32 R28FacadeTagCount = 0;
    int32 R29FacadeClassCount = 0;
    int32 R29FacadeTagCount = 0;
    int32 VegetationClassCount = 0;
    int32 VegetationTagCount = 0;
    int32 LandmarkVegetationClassCount = 0;
    int32 LandmarkVegetationTagCount = 0;
    int32 TerrainClassCount = 0;
    int32 TerrainTagCount = 0;
    int32 TreeClassCount = 0;
    int32 TreeTagCount = 0;
    int32 ExactV2ComponentCount = 0;
};

bool ResolveR31WorldRoster(
    UWorld* World,
    FR31WorldRoster& OutRoster,
    FString& OutError)
{
    OutRoster = FR31WorldRoster{};
    if (!World)
    {
        OutError = TEXT("R31 broad-shell roster resolution requires a world.");
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (Candidate && Candidate->GetClass() ==
                ATRIADIstanaExploreV5DContextPolicyActor::StaticClass())
        {
            ++OutRoster.PolicyCount;
            OutRoster.Policy = Cast<
                ATRIADIstanaExploreV5DContextPolicyActor>(Candidate);
        }
        if (Candidate && Candidate->GetClass() ==
                ATRIADIstanaPublicViewSceneActor::StaticClass())
        {
            ++OutRoster.SceneCount;
            OutRoster.Scene = Cast<ATRIADIstanaPublicViewSceneActor>(Candidate);
        }
        const bool bExactR30 = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::StaticClass();
        const bool bTaggedR30 = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedActorTag());
        OutRoster.R30ClassCount += bExactR30 ? 1 : 0;
        OutRoster.R30TagCount += bTaggedR30 ? 1 : 0;
        if (bExactR30)
        {
            OutRoster.R30 = Cast<
                ATRIADIstanaExploreV5DR30FacadeLookdevActor>(Candidate);
        }
        if (bTaggedR30 && !bExactR30)
        {
            OutError = TEXT("R31 refused an R30 actor tag on a non-exact class.");
            return false;
        }
        const bool bExactR28Facade = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass();
        const bool bTaggedR28Facade = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag());
        const bool bExactR29Facade = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::StaticClass();
        const bool bTaggedR29Facade = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::ExpectedActorTag());
        const bool bExactVegetation = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29VegetationActor::StaticClass();
        const bool bTaggedVegetation = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29VegetationActor::ExpectedActorTag());
        const bool bExactLandmarkVegetation = Candidate &&
            Candidate->GetClass() ==
                ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass();
        const bool bTaggedLandmarkVegetation = Candidate &&
            Candidate->Tags.Contains(
                ATRIADIstanaExploreV5DLandmarkVegetationActor::ExpectedActorTag());
        const bool bExactTerrain = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::StaticClass();
        const bool bTaggedTerrain = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::ExpectedActorTag());
        const bool bExactTrees = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DTreeRealismActor::StaticClass();
        const bool bTaggedTrees = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag());
        OutRoster.R28FacadeClassCount += bExactR28Facade ? 1 : 0;
        OutRoster.R28FacadeTagCount += bTaggedR28Facade ? 1 : 0;
        OutRoster.R29FacadeClassCount += bExactR29Facade ? 1 : 0;
        OutRoster.R29FacadeTagCount += bTaggedR29Facade ? 1 : 0;
        OutRoster.VegetationClassCount += bExactVegetation ? 1 : 0;
        OutRoster.VegetationTagCount += bTaggedVegetation ? 1 : 0;
        OutRoster.LandmarkVegetationClassCount +=
            bExactLandmarkVegetation ? 1 : 0;
        OutRoster.LandmarkVegetationTagCount +=
            bTaggedLandmarkVegetation ? 1 : 0;
        OutRoster.TerrainClassCount += bExactTerrain ? 1 : 0;
        OutRoster.TerrainTagCount += bTaggedTerrain ? 1 : 0;
        OutRoster.TreeClassCount += bExactTrees ? 1 : 0;
        OutRoster.TreeTagCount += bTaggedTrees ? 1 : 0;
        if (bExactVegetation)
        {
            OutRoster.Vegetation = Cast<ATRIADIstanaExploreV5DR29VegetationActor>(Candidate);
        }
        if (bExactTerrain)
        {
            OutRoster.Terrain = Cast<ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>(Candidate);
        }
        if (bExactTrees)
        {
            OutRoster.Trees = Cast<ATRIADIstanaExploreV5DTreeRealismActor>(Candidate);
        }
        if ((bTaggedR28Facade && !bExactR28Facade) ||
            (bTaggedR29Facade && !bExactR29Facade) ||
            (bTaggedVegetation && !bExactVegetation) ||
            (bTaggedLandmarkVegetation && !bExactLandmarkVegetation) ||
            (bTaggedTerrain && !bExactTerrain) ||
            (bTaggedTrees && !bExactTrees))
        {
            OutError = TEXT("R31 refused a protected R28/R29 facade, vegetation, terrain, or tree tag on a non-exact class.");
            return false;
        }
        TInlineComponentArray<UStaticMeshComponent*> MeshComponents(Candidate);
        for (UStaticMeshComponent* MeshComponent : MeshComponents)
        {
            if (MeshComponent && MeshComponent->GetStaticMesh() &&
                MeshComponent->GetStaticMesh()->GetPathName() ==
                    ATRIADIstanaExploreV5DContextPolicyActor::
                        ExpectedCurrentSurroundingsV2MeshObjectPath())
            {
                ++OutRoster.ExactV2ComponentCount;
                OutRoster.ExactV2Component = MeshComponent;
            }
        }
    }
    if (OutRoster.PolicyCount != 1 || OutRoster.SceneCount != 1 ||
        OutRoster.R30ClassCount != 1 || OutRoster.R30TagCount != 1 ||
        !OutRoster.Policy || !OutRoster.Scene || !OutRoster.R30 ||
        OutRoster.R28FacadeClassCount != 0 ||
        OutRoster.R28FacadeTagCount != 0 ||
        OutRoster.R29FacadeClassCount != 0 ||
        OutRoster.R29FacadeTagCount != 0 ||
        OutRoster.VegetationClassCount != 1 ||
        OutRoster.VegetationTagCount != 1 || !OutRoster.Vegetation ||
        OutRoster.LandmarkVegetationClassCount != 0 ||
        OutRoster.LandmarkVegetationTagCount != 0 ||
        OutRoster.TerrainClassCount != 1 ||
        OutRoster.TerrainTagCount != 1 || !OutRoster.Terrain ||
        OutRoster.TreeClassCount != 1 || OutRoster.TreeTagCount != 1 ||
        !OutRoster.Trees || OutRoster.ExactV2ComponentCount != 1 ||
        OutRoster.ExactV2Component !=
            OutRoster.Policy->CurrentSurroundingsRenderOnlyComponent)
    {
        OutError = FString::Printf(
            TEXT("R31 requires exact promoted-R30 world ownership: policies=%d scenes=%d r30=%d/%d r28Facade=%d/%d r29Facade=%d/%d r29Vegetation=%d/%d r28LandmarkVegetation=%d/%d r29Terrain=%d/%d trees=%d/%d exactV2Components=%d."),
            OutRoster.PolicyCount, OutRoster.SceneCount,
            OutRoster.R30ClassCount, OutRoster.R30TagCount,
            OutRoster.R28FacadeClassCount, OutRoster.R28FacadeTagCount,
            OutRoster.R29FacadeClassCount, OutRoster.R29FacadeTagCount,
            OutRoster.VegetationClassCount, OutRoster.VegetationTagCount,
            OutRoster.LandmarkVegetationClassCount,
            OutRoster.LandmarkVegetationTagCount,
            OutRoster.TerrainClassCount, OutRoster.TerrainTagCount,
            OutRoster.TreeClassCount, OutRoster.TreeTagCount,
            OutRoster.ExactV2ComponentCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidatePromotedR30Predecessor(
    UWorld* World,
    FR31WorldRoster& OutRoster,
    FString& OutReport)
{
    FString RosterError;
    FString R30Report;
    FString R25Report;
    FString HybridReport;
    FString VegetationReport;
    FString TerrainReport;
    FString TreeReport;
    if (!ResolveR31WorldRoster(World, OutRoster, RosterError) ||
        !OutRoster.R30->ValidateR30FacadeLookdev(R30Report) ||
        !OutRoster.Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            OutRoster.Scene, R25Report) ||
        !OutRoster.Policy->ValidateHybridContext(HybridReport) ||
        !OutRoster.Vegetation->ValidateR29Vegetation(VegetationReport) ||
        !OutRoster.Terrain->ValidateCopernicusTerrainFallback(TerrainReport) ||
        !OutRoster.Trees->ValidateTreeRealism(TreeReport) ||
        OutRoster.R30->bProviderReady !=
            OutRoster.Policy->bLocalBuildingFallbackCurrentlyHidden)
    {
        OutReport = TEXT("R31 predecessor validation failed; R30 commit/capture must precede R31: roster={") +
            RosterError + TEXT("} r30={") + R30Report + TEXT("} r25={") +
            R25Report + TEXT("} hybrid={") + HybridReport +
            TEXT("} vegetation={") + VegetationReport + TEXT("} terrain={") +
            TerrainReport + TEXT("} trees={") + TreeReport + TEXT("}");
        return false;
    }
    OutReport = TEXT("R31_PREDECESSOR_VALID r28FacadeOwners=0 r29FacadeOwners=0 r30Owners=1 r28LandmarkVegetationOwners=0 strictR25Shell=true exactV2MeshComponents=1 triangles=43448 identityTransform=true exactR29Vegetation=true exactR29Terrain=true exactTreeRealism=true providerStateMatchesContext=true providerHandoffValidated=true.");
    return true;
}

bool ValidateR31World(
    UWorld* World,
    FR31WorldRoster& OutRoster,
    FString& OutReport)
{
    FString RosterError;
    FString R30Report;
    FString R31Report;
    FString HybridReport;
    FString VegetationReport;
    FString TerrainReport;
    FString TreeReport;
    if (!ResolveR31WorldRoster(World, OutRoster, RosterError) ||
        !OutRoster.R30->ValidateR30FacadeLookdev(R30Report) ||
        !OutRoster.Policy->ValidateCurrentSurroundingsBroadShellR31ForInheritedScene(
            OutRoster.Scene, R31Report) ||
        !OutRoster.Policy->ValidateHybridContext(HybridReport) ||
        !OutRoster.Vegetation->ValidateR29Vegetation(VegetationReport) ||
        !OutRoster.Terrain->ValidateCopernicusTerrainFallback(TerrainReport) ||
        !OutRoster.Trees->ValidateTreeRealism(TreeReport) ||
        OutRoster.R30->bProviderReady !=
            OutRoster.Policy->bLocalBuildingFallbackCurrentlyHidden)
    {
        OutReport = TEXT("R31 loaded-world validation failed: roster={") +
            RosterError + TEXT("} r30={") + R30Report + TEXT("} r31={") +
            R31Report + TEXT("} hybrid={") + HybridReport +
            TEXT("} vegetation={") + VegetationReport + TEXT("} terrain={") +
            TerrainReport + TEXT("} trees={") + TreeReport + TEXT("}");
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_MAP_VALID r28FacadeOwners=0 r29FacadeOwners=0 r30Owners=1 r31MaterialOwner=contextPolicyInPlace exactV2MeshComponents=1 retainedTriangles=43448 retainedSlots=17 identityTransform=true exactR31Overrides=17 exactR29Vegetation=true exactR29Terrain=true exactTreeRealism=true providerVisibilityStatePreserved=true r30FacadeCueCoexists=true duplicateShellComponents=0 collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false geographyModified=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        R31Report;
    return true;
}

bool HasExactFreshSaveRoster(
    const TArray<UObject*>& Assets,
    FString& OutError)
{
    const TArray<FString>& Expected =
        TRIADIstanaExploreV5DR31BroadShellAssetFactory::
            GetOrderedMaterialObjectPaths();
    TSet<FString> ExpectedPaths;
    for (const FString& Path : Expected)
    {
        ExpectedPaths.Add(Path);
    }
    ExpectedPaths.Add(
        TRIADIstanaExploreV5DR31BroadShellAssetFactory::
            GetMasterMaterialObjectPath());
    TSet<FString> ActualPaths;
    for (UObject* Asset : Assets)
    {
        UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
        if (!Asset || !Package ||
            !Package->HasAnyPackageFlags(PKG_NewlyCreated) ||
            FPackageName::DoesPackageExist(Package->GetName()) ||
            ActualPaths.Contains(Asset->GetPathName()))
        {
            OutError = TEXT("R31 fresh save roster contains a non-new, persisted, null, or duplicate asset.");
            return false;
        }
        ActualPaths.Add(Asset->GetPathName());
    }
    if (ActualPaths.Num() != 5 || ActualPaths.Num() != ExpectedPaths.Num())
    {
        OutError = TEXT("R31 fresh save roster must contain exactly one master and four MICs.");
        return false;
    }
    for (const FString& Path : ExpectedPaths)
    {
        if (!ActualPaths.Contains(Path))
        {
            OutError = TEXT("R31 fresh save roster omitted exact asset: ") + Path;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool UndoR31AndValidateR30(
    UWorld* World,
    TUniquePtr<FScopedTransaction>& Transaction,
    UStaticMesh* ExpectedMesh,
    const FTransform& ExpectedRelativeTransform,
    bool bExpectedVisibility,
    bool bExpectedHiddenInGame,
    bool bExpectedProviderState,
    bool bPackageWasDirty,
    FString& OutReport)
{
    if (!GEditor || !World || !Transaction || !Transaction->IsOutstanding())
    {
        OutReport = TEXT("R31 in-memory rollback could not close an outstanding editor transaction.");
        return false;
    }

    Transaction.Reset();
    const bool bUndoSucceeded = GEditor->UndoTransaction(false);
    FR31WorldRoster Restored;
    FString PredecessorReport;
    const bool bPredecessorRestored = bUndoSucceeded &&
        ValidatePromotedR30Predecessor(World, Restored, PredecessorReport);
    UStaticMeshComponent* RestoredComponent = bPredecessorRestored
        ? Restored.Policy->CurrentSurroundingsRenderOnlyComponent
        : nullptr;
    const bool bComponentStateRestored = RestoredComponent &&
        RestoredComponent->GetStaticMesh() == ExpectedMesh &&
        RestoredComponent->GetRelativeTransform().Equals(
            ExpectedRelativeTransform, 0.001) &&
        RestoredComponent->IsVisible() == bExpectedVisibility &&
        RestoredComponent->bHiddenInGame == bExpectedHiddenInGame &&
        Restored.Policy->bLocalBuildingFallbackCurrentlyHidden ==
            bExpectedProviderState;
    const bool bRestored = bPredecessorRestored && bComponentStateRestored;
    if (bRestored && World->GetOutermost())
    {
        World->GetOutermost()->SetDirtyFlag(bPackageWasDirty);
    }
    if (!bRestored)
    {
        OutReport = TEXT("R31 undo failed to restore the strict R30/R25 predecessor and exact mesh/transform/visibility/provider state; external wrapper rollback is required. predecessor={") +
            PredecessorReport + TEXT("}");
        return false;
    }
    OutReport = TEXT("R31_IN_MEMORY_ROLLBACK_VALID undoTransaction=true strictR30=true strictR25Shell=true exactV2Mesh=true identityTransform=true providerStateRestored=true packageDirtyStateRestored=true.");
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR31BroadShellEditorLibrary::
    EnsureR31BroadShellAssets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DR31BroadShellAssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R31_BROAD_SHELL_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_ASSET_BUILD_REFUSED: editor asset subsystem unavailable.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5DR31BroadShellAssetFactory::CreateFreshAssets(
            FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_ASSET_BUILD_FAILED: ") +
            Error;
        return false;
    }
    if (!HasExactFreshSaveRoster(FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_ASSET_BUILD_FAILED_ROSTER: ") +
            Error;
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_ASSET_BUILD_FAILED_ATOMIC_SAVE: exact five-asset save failed; wrapper must remove the initially absent R31 root.");
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
    if (!TRIADIstanaExploreV5DR31BroadShellAssetFactory::ValidateAssets(
            SavedReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_ASSET_BUILD_FAILED_POST_SAVE_VALIDATION: ") +
            SavedReport;
        return false;
    }
    OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_ASSET_BUILD_PASS exactAssets=5 exactNewPackages=5 mapMutation=false predecessorPackagesModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false visualCaptureAccepted=false. ") +
        SavedReport;
    return true;
}

bool UTRIADIstanaExploreV5DR31BroadShellEditorLibrary::
    ValidateR31BroadShellAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5DR31BroadShellAssetFactory::
        ValidateAssets(OutReport);
}

bool UTRIADIstanaExploreV5DR31BroadShellEditorLibrary::
    ApplyR31BroadShellToLoadedV5DHybridMap(FString& OutMessage)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_APPLY_REFUSED_WORLD: ") +
            Error;
        return false;
    }
    FString AssetReport;
    if (!TRIADIstanaExploreV5DR31BroadShellAssetFactory::ValidateAssets(
            AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_APPLY_REFUSED_ASSETS: ") +
            AssetReport;
        return false;
    }
    FR31WorldRoster Predecessor;
    FString PredecessorReport;
    if (!ValidatePromotedR30Predecessor(
            World, Predecessor, PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_APPLY_REFUSED_PREDECESSOR: ") +
            PredecessorReport;
        return false;
    }
    const bool bPackageWasDirty = World->GetOutermost()->IsDirty();
    UStaticMeshComponent* Component =
        Predecessor.Policy->CurrentSurroundingsRenderOnlyComponent;
    UStaticMesh* ExactMesh = Component ? Component->GetStaticMesh() : nullptr;
    const FTransform ExactRelativeTransform = Component
        ? Component->GetRelativeTransform()
        : FTransform::Identity;
    const bool bExactVisibility = Component && Component->IsVisible();
    const bool bExactHiddenInGame = Component && Component->bHiddenInGame;
    const bool bExactProviderState =
        Predecessor.Policy->bLocalBuildingFallbackCurrentlyHidden;

    TUniquePtr<FScopedTransaction> Transaction =
        MakeUnique<FScopedTransaction>(
            NSLOCTEXT(
                "TRIAD",
                "ApplyIstanaExploreV5DR31BroadShell",
                 "Apply Istana Explore V5D R31 Broad Shell"));
    if (!Transaction || !Transaction->IsOutstanding())
    {
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_APPLY_REFUSED_NO_UNDO_TRANSACTION: no map mutation was attempted.");
        return false;
    }
    Predecessor.Policy->Modify();
    if (!Predecessor.Policy->ApplyCurrentSurroundingsBroadShellR31(Error))
    {
        FString RollbackReport;
        const bool bRolledBack = UndoR31AndValidateR30(
            World, Transaction, ExactMesh, ExactRelativeTransform,
            bExactVisibility, bExactHiddenInGame, bExactProviderState,
            bPackageWasDirty, RollbackReport);
        OutMessage = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_APPLY_FAILED: ") +
            Error + TEXT(" inMemoryRollbackSucceeded=") +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }
    FR31WorldRoster Successor;
    FString SuccessorReport;
    const bool bStatePreserved = Component &&
        Component->GetStaticMesh() == ExactMesh &&
        Component->GetRelativeTransform().Equals(ExactRelativeTransform, 0.001) &&
        Component->IsVisible() == bExactVisibility &&
        Component->bHiddenInGame == bExactHiddenInGame &&
        Predecessor.Policy->bLocalBuildingFallbackCurrentlyHidden ==
            bExactProviderState;
    if (!bStatePreserved ||
        !ValidateR31World(World, Successor, SuccessorReport))
    {
        FString RestoredReport;
        const bool bRestored = UndoR31AndValidateR30(
            World, Transaction, ExactMesh, ExactRelativeTransform,
            bExactVisibility, bExactHiddenInGame, bExactProviderState,
            bPackageWasDirty, RestoredReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R31_BROAD_SHELL_APPLY_FAILED_VALIDATION inMemoryRollback=")) +
            (bRestored ? TEXT("true") : TEXT("false")) + TEXT(" successor={") +
            SuccessorReport + TEXT("} rollback={") + RestoredReport + TEXT("}");
        return false;
    }
    Transaction.Reset();
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_APPLY_PASS mapSaved=false callerOwnsSingleCommit=true componentReusedInPlace=true duplicateShellComponents=0 exactV2Mesh=true retainedTriangles=43448 retainedSlots=17 identityTransform=true exactR31Overrides=17 r30FacadeCueCoexists=true providerStatePreserved=true collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false geographyModified=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        SuccessorReport + TEXT(" ") + AssetReport;
    return true;
}

bool UTRIADIstanaExploreV5DR31BroadShellEditorLibrary::
    ValidateR31BroadShellInLoadedV5DHybridMap(FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = Error;
        return false;
    }
    FString AssetReport;
    if (!TRIADIstanaExploreV5DR31BroadShellAssetFactory::ValidateAssets(
            AssetReport))
    {
        OutReport = AssetReport;
        return false;
    }
    FR31WorldRoster Roster;
    if (!ValidateR31World(World, Roster, OutReport))
    {
        return false;
    }
    OutReport += TEXT(" ") + AssetReport;
    return true;
}

bool UTRIADIstanaExploreV5DR31BroadShellEditorLibrary::
    CommitR31BroadShellToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_REFUSED_WORLD: ") +
            Error;
        return false;
    }
    FString DestinationFilename;
    const FString ExpectedSha256 = ExpectedPredecessorSha256.ToUpper();
    const FString BackupFilename = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    const FString TransactionRoot = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/NativeTransactions/V5DBroadShellR31V1")));
    if (ExpectedPredecessorBytes <= 0 || !IsValidSha256(ExpectedSha256) ||
        !FPackageName::DoesPackageExist(
            TargetMapPackage, &DestinationFilename) ||
        DestinationFilename.IsEmpty() || BackupFilename.IsEmpty() ||
        FPaths::IsSamePath(DestinationFilename, BackupFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, TransactionRoot) ||
        !World->GetOutermost() || World->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_REFUSED_INPUT: exact clean map, positive preimage bytes/SHA-256, and an external backup below the bounded R31 transaction root are required.");
        return false;
    }

    FString MapSha256;
    FString BackupSha256;
    int64 MapBytes = INDEX_NONE;
    int64 BackupBytes = INDEX_NONE;
    FString MapError;
    FString BackupError;
    if (!HashFileSha256(DestinationFilename, MapSha256, MapBytes, MapError) ||
        !HashFileSha256(BackupFilename, BackupSha256, BackupBytes, BackupError) ||
        MapBytes != ExpectedPredecessorBytes || MapSha256 != ExpectedSha256 ||
        BackupBytes != ExpectedPredecessorBytes || BackupSha256 != ExpectedSha256)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_REFUSED_PREIMAGE mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapError={%s} backupError={%s}"),
            MapBytes, *MapSha256, BackupBytes, *BackupSha256,
            *MapError, *BackupError);
        return false;
    }

    FR31WorldRoster Predecessor;
    FString PredecessorReport;
    if (!ValidatePromotedR30Predecessor(
            World, Predecessor, PredecessorReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_REFUSED_PREDECESSOR: ") +
            PredecessorReport;
        return false;
    }

    FString ApplyReport;
    if (!ApplyR31BroadShellToLoadedV5DHybridMap(ApplyReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_FAILED_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ApplyReport;
        return false;
    }

    FString PreSaveMapSha256;
    FString PreSaveBackupSha256;
    int64 PreSaveMapBytes = INDEX_NONE;
    int64 PreSaveBackupBytes = INDEX_NONE;
    FString PreSaveMapError;
    FString PreSaveBackupError;
    if (!HashFileSha256(
            DestinationFilename, PreSaveMapSha256,
            PreSaveMapBytes, PreSaveMapError) ||
        !HashFileSha256(
            BackupFilename, PreSaveBackupSha256,
            PreSaveBackupBytes, PreSaveBackupError) ||
        PreSaveMapBytes != ExpectedPredecessorBytes ||
        PreSaveMapSha256 != ExpectedSha256 ||
        PreSaveBackupBytes != ExpectedPredecessorBytes ||
        PreSaveBackupSha256 != ExpectedSha256)
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_FAILED_PRE_SAVE_PIN_GATE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, TargetMapPackage))
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_FAILED_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
    UWorld* UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UPackage* UnloadPackage = UnloadWorld ? UnloadWorld->GetOutermost() : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) == TargetMapPackage)
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_FAILED_COLD_UNLOAD externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
    UWorld* ReloadedWorld =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedWorld)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    FString ColdReport;
    if (!ReloadedWorld || !ReloadedWorld->GetOutermost() ||
        ReloadedWorld->GetOutermost()->IsDirty() ||
        !ValidateR31BroadShellInLoadedV5DHybridMap(ColdReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_FAILED_COLD_VALIDATION externalBackupVerified=true rollbackOwnedByWrapper=true ") +
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
        DestinationFilename, SuccessorSha256, SuccessorBytes,
        SuccessorError) &&
        SuccessorBytes > 0 && IsValidSha256(SuccessorSha256) &&
        (SuccessorBytes != ExpectedPredecessorBytes ||
         SuccessorSha256 != ExpectedSha256);
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename, FinalBackupSha256, FinalBackupBytes,
        FinalBackupError) &&
        FinalBackupBytes == ExpectedPredecessorBytes &&
        FinalBackupSha256 == ExpectedSha256;
    if (!bSuccessorChanged || !bBackupPreserved)
    {
        OutReport = TEXT("EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_FAILED_FINAL_RECEIPT externalBackupVerified=true rollbackOwnedByWrapper=true successorError={") +
            SuccessorError + TEXT("} backupError={") + FinalBackupError +
            TEXT("}");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_COMMIT_PASS oneSave=true coldReload=true predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s externalBackupBytes=%lld externalBackupSha256=%s r30Owners=1 r31MaterialOwner=contextPolicyInPlace exactV2Mesh=true retainedTriangles=43448 retainedSlots=17 identityTransform=true exactR31Overrides=17 providerHandoffPreserved=true duplicateShellComponents=0 collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false geographyModified=false visualCaptureAccepted=false captureRevalidationRequired=true. %s %s"),
        ExpectedPredecessorBytes, *ExpectedSha256,
        SuccessorBytes, *SuccessorSha256,
        FinalBackupBytes, *FinalBackupSha256,
        *ApplyReport, *ColdReport);
    return true;
}
