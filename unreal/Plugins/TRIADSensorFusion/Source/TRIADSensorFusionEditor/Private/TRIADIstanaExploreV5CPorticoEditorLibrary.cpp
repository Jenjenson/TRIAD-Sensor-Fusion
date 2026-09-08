#include "TRIADIstanaExploreV5CPorticoEditorLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5CPorticoAssetFactory.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
ATRIADIstanaExploreV4LandscapeActor* FindUniqueV4Owner(
    UWorld* World,
    int32& OutCount)
{
    OutCount = 0;
    ATRIADIstanaExploreV4LandscapeActor* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<ATRIADIstanaExploreV4LandscapeActor> It(World); It; ++It)
    {
        Result = *It;
        ++OutCount;
    }
    return OutCount == 1 ? Result : nullptr;
}

ATRIADIstanaExploreV4LandscapeActor* ResolveCurrentV4Owner(
    FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    int32 Count = 0;
    ATRIADIstanaExploreV4LandscapeActor* Actor =
        FindUniqueV4Owner(World, Count);
    if (!World || !Actor || Count != 1)
    {
        OutError = FString::Printf(
            TEXT("The current editor world must contain exactly one V4 landscape owner; found %d."),
            Count);
        return nullptr;
    }
    OutError.Reset();
    return Actor;
}

UStaticMesh* LoadExactV5CMesh()
{
    const FString& Path =
        TRIADIstanaExploreV5CPorticoAssetFactory::
            GetPorticoMeshObjectPath();
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
    return Mesh && Mesh->GetPathName() == Path ? Mesh : nullptr;
}

void ModifyPresentationObjects(ATRIADIstanaExploreV4LandscapeActor* Actor)
{
    if (!Actor)
    {
        return;
    }
    Actor->Modify();
    if (Actor->PorticoV8RenderOnlyComponent)
    {
        Actor->PorticoV8RenderOnlyComponent->Modify();
    }
    if (Actor->PorticoV5CRenderOnlyComponent)
    {
        Actor->PorticoV5CRenderOnlyComponent->Modify();
    }
}
} // namespace

bool UTRIADIstanaExploreV5CPorticoEditorLibrary::
    ImportIstanaExploreV5CPorticoAssets(FString& OutMessage)
{
    FString Existing;
    if (TRIADIstanaExploreV5CPorticoAssetFactory::ValidateAssets(Existing))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5C_PORTICO_ASSETS_ALREADY_VALID: ") +
            Existing;
        return true;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5CPorticoAssetFactory::CreateFreshAssets(
            FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_IMPORT_FAILED: ") + Error;
        return false;
    }
    if (FreshAssets.Num() != 9 || FreshAssets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(FreshAssets, false) ||
        !TRIADIstanaExploreV5CPorticoAssetFactory::ValidateAssets(Error))
    {
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_IMPORT_FAILED_AFTER_FRESH_CREATE: only the exact nine V5C assets were offered to save. ") +
            Error;
        return false;
    }
    OutMessage = TEXT("Imported and cold-readback-validated one exact hash-pinned V5C identity portico mesh and eight ordered opaque/default-lit constant-PBR materials. No textures, collision, navigation, map, V4/V5/V5B/V8 package, sensor or RF authority were created or changed.");
    return true;
}

bool UTRIADIstanaExploreV5CPorticoEditorLibrary::
    ValidateIstanaExploreV5CPorticoAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5CPorticoAssetFactory::ValidateAssets(OutReport);
}

bool UTRIADIstanaExploreV5CPorticoEditorLibrary::
    ApplyIstanaExploreV5CPorticoToCurrentWorld(FString& OutMessage)
{
    FString AssetReport;
    if (!TRIADIstanaExploreV5CPorticoAssetFactory::ValidateAssets(AssetReport))
    {
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_APPLY_REFUSED_ASSETS: ") +
            AssetReport;
        return false;
    }
    FString Error;
    ATRIADIstanaExploreV4LandscapeActor* Actor = ResolveCurrentV4Owner(Error);
    UStaticMesh* Mesh = LoadExactV5CMesh();
    if (!Actor || !Mesh)
    {
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_APPLY_REFUSED_WORLD: ") +
            (Error.IsEmpty()
                ? TEXT("the exact V5C mesh could not be loaded.")
                : Error);
        return false;
    }
    ModifyPresentationObjects(Actor);
    if (!Actor->ConfigurePorticoV5CPresentation(Mesh, Error))
    {
        Actor->MarkPackageDirty();
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_APPLY_FAILED_SAFE_V8: ") +
            Error;
        return false;
    }
    FString Readback;
    if (!Actor->ValidatePorticoV5CPresentation(Readback) ||
        !Actor->IsPorticoV5CPresentationActive())
    {
        Actor->RestorePorticoV8PresentationFailSafe();
        Actor->MarkPackageDirty();
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_APPLY_FAILED_SAFE_V8: ") +
            Readback;
        return false;
    }
    Actor->MarkPackageDirty();
    OutMessage = TEXT("EXPLORE_V5C_PORTICO_APPLIED_CURRENT_WORLD: exact V5C is visible at identity and V8 is hidden; both remain render-only, NoCollision and no-navigation. Save the map explicitly only after visual review.");
    return true;
}

bool UTRIADIstanaExploreV5CPorticoEditorLibrary::
    ValidateIstanaExploreV5CPorticoCurrentWorld(FString& OutReport)
{
    FString AssetReport;
    if (!TRIADIstanaExploreV5CPorticoAssetFactory::ValidateAssets(AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5C_PORTICO_WORLD_INVALID: ") +
            AssetReport;
        return false;
    }
    FString Error;
    ATRIADIstanaExploreV4LandscapeActor* Actor = ResolveCurrentV4Owner(Error);
    FString PresentationReport;
    if (!Actor || !Actor->IsPorticoV5CPresentationActive() ||
        !Actor->ValidatePorticoV5CPresentation(PresentationReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5C_PORTICO_WORLD_INVALID: ") +
            (Error.IsEmpty() ? PresentationReport : Error);
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5C_PORTICO_WORLD_VALID: exact identity V5C visible, exact frozen V8 hidden, fail-safe state persisted; no collision/navigation/sensor/RF authority.");
    return true;
}

bool UTRIADIstanaExploreV5CPorticoEditorLibrary::
    RestoreIstanaExploreV8PorticoInCurrentWorld(FString& OutMessage)
{
    FString Error;
    ATRIADIstanaExploreV4LandscapeActor* Actor = ResolveCurrentV4Owner(Error);
    if (!Actor)
    {
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_RESTORE_REFUSED: ") + Error;
        return false;
    }
    ModifyPresentationObjects(Actor);
    Actor->RestorePorticoV8PresentationFailSafe();
    FString Readback;
    if (!Actor->ValidatePorticoV5CPresentation(Readback) ||
        Actor->IsPorticoV5CPresentationActive())
    {
        OutMessage = TEXT("EXPLORE_V5C_PORTICO_RESTORE_FAILED: ") + Readback;
        return false;
    }
    Actor->MarkPackageDirty();
    OutMessage = TEXT("EXPLORE_V5C_PORTICO_RESTORED_SAFE_V8: V8 visible; V5C empty and hidden. Save the map explicitly if this recovery state is desired.");
    return true;
}
