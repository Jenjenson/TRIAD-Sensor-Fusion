#include "TRIADIstanaExploreV5CSurroundingsEditorLibrary.h"

#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5CGroundContextAssetFactory.h"
#include "TRIADIstanaExploreV5CSurroundingsAssetFactory.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5b"));

ATRIADIstanaPublicViewSceneActor* ResolveCurrentPublicViewScene(
    FString& OutError)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!World || World->WorldType != EWorldType::Editor ||
        !World->GetOutermost() ||
        World->GetOutermost()->GetName() != TargetMapPackage)
    {
        OutError = FString::Printf(
            TEXT("The current editor world must be the exact unsaved V5B target '%s'; actual='%s'."),
            *TargetMapPackage,
            World && World->GetOutermost()
                ? *World->GetOutermost()->GetName()
                : TEXT("null"));
        return nullptr;
    }

    int32 Count = 0;
    ATRIADIstanaPublicViewSceneActor* Result = nullptr;
    for (TActorIterator<ATRIADIstanaPublicViewSceneActor> It(World);
         It;
         ++It)
    {
        Result = *It;
        ++Count;
    }
    if (Count != 1 || !Result)
    {
        OutError = FString::Printf(
            TEXT("The current exact V5B editor world must contain one public-view scene actor; found %d."),
            Count);
        return nullptr;
    }

    OutError.Reset();
    return Result;
}

UStaticMesh* LoadExactV5CSurroundingsMesh()
{
    const FString& Path =
        TRIADIstanaExploreV5CSurroundingsAssetFactory::
            GetSurroundingsMeshObjectPath();
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
    return Mesh && Mesh->GetPathName() == Path ? Mesh : nullptr;
}

UStaticMesh* LoadExactV5CGroundContextMesh()
{
    const FString& Path =
        TRIADIstanaExploreV5CGroundContextAssetFactory::
            GetGroundContextMeshObjectPath();
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Path);
    return Mesh && Mesh->GetPathName() == Path ? Mesh : nullptr;
}

bool ValidateAtomicV5CAssetRoots(FString& OutReport)
{
    FString BuildingsReport;
    FString GroundContextReport;
    const bool bBuildingsValid =
        TRIADIstanaExploreV5CSurroundingsAssetFactory::ValidateAssets(
            BuildingsReport);
    const bool bGroundContextValid =
        TRIADIstanaExploreV5CGroundContextAssetFactory::ValidateAssets(
            GroundContextReport);
    if (!bBuildingsValid || !bGroundContextValid)
    {
        OutReport = FString::Printf(
            TEXT("Atomic V5C asset roots invalid: buildings={%s}; exact three-asset ground-context root={%s}."),
            *BuildingsReport,
            *GroundContextReport);
        return false;
    }

    OutReport = FString::Printf(
        TEXT("Atomic V5C asset roots valid: buildings={%s}; exact three-asset ground-context root={%s}."),
        *BuildingsReport,
        *GroundContextReport);
    return true;
}

void ModifyPresentationObjects(ATRIADIstanaPublicViewSceneActor* Actor)
{
    if (!Actor)
    {
        return;
    }
    Actor->Modify();
    if (Actor->OSMContextBuildingsComponent)
    {
        Actor->OSMContextBuildingsComponent->Modify();
    }
    if (Actor->V5CSurroundingsRenderOnlyComponent)
    {
        Actor->V5CSurroundingsRenderOnlyComponent->Modify();
    }
    if (Actor->V5CGroundContextRenderOnlyComponent)
    {
        Actor->V5CGroundContextRenderOnlyComponent->Modify();
    }
}
} // namespace

bool UTRIADIstanaExploreV5CSurroundingsEditorLibrary::
    ImportIstanaExploreV5CSurroundingsAssets(FString& OutMessage)
{
    FString Existing;
    if (TRIADIstanaExploreV5CSurroundingsAssetFactory::ValidateAssets(
            Existing))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5C_SURROUNDINGS_ASSETS_ALREADY_VALID: ") +
            Existing;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5CSurroundingsAssetFactory::CreateFreshAssets(
            FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_IMPORT_FAILED: ") +
            Error;
        return false;
    }
    if (FreshAssets.Num() != 6 || FreshAssets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(FreshAssets, false) ||
        !TRIADIstanaExploreV5CSurroundingsAssetFactory::ValidateAssets(
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_IMPORT_FAILED_AFTER_FRESH_CREATE: only the exact six V5C surroundings assets were offered to save. ") +
            Error;
        return false;
    }

    OutMessage = TEXT("Imported and cold-readback-validated one exact hash-pinned, identity, Nanite V5C surroundings mesh, one isolated texture-free procedural massing master, and four exact material instances. Synthetic terrain grounding, the hidden anti-gap wall skirt, local facade UV0, screen-adaptive framed/inset glazing, four-by-four macro tonal breakup, procedural glazing roughness, cell variation and plinth cues are explicitly non-authoritative presentation hints; no textures, geometry, physical grade/foundation/facade claim, collision, navigation, map, sensor or RF authority were created or changed.");
    return true;
}

bool UTRIADIstanaExploreV5CSurroundingsEditorLibrary::
    ValidateIstanaExploreV5CSurroundingsAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5CSurroundingsAssetFactory::ValidateAssets(
        OutReport);
}

bool UTRIADIstanaExploreV5CSurroundingsEditorLibrary::
    ApplyIstanaExploreV5CSurroundingsToCurrentWorld(FString& OutMessage)
{
    FString Error;
    ATRIADIstanaPublicViewSceneActor* Actor =
        ResolveCurrentPublicViewScene(Error);
    if (!Actor)
    {
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_APPLY_REFUSED_WORLD: ") +
            Error;
        return false;
    }

    // Make every subsequent validation/load failure an observable safe state,
    // including a missing or corrupt candidate asset root.
    ModifyPresentationObjects(Actor);
    Actor->RestoreLegacyOsmSurroundingsPresentationFailSafe();

    FString AssetReport;
    if (!ValidateAtomicV5CAssetRoots(AssetReport))
    {
        Actor->MarkPackageDirty();
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_APPLY_REFUSED_ASSETS_SAFE_LEGACY_OSM: both V5C siblings were emptied before candidate-root validation. ") +
            AssetReport;
        return false;
    }

    UStaticMesh* Mesh = LoadExactV5CSurroundingsMesh();
    UStaticMesh* GroundContextMesh = LoadExactV5CGroundContextMesh();
    if (!Mesh || !GroundContextMesh)
    {
        Actor->MarkPackageDirty();
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_APPLY_REFUSED_LOAD_SAFE_LEGACY_OSM: the exact V5C building or official planning ground-context mesh could not be loaded from the separately validated roots; both V5C siblings remain empty.");
        return false;
    }

    if (!Actor->ConfigureV5CSurroundingsPresentation(
            Mesh, GroundContextMesh, Error))
    {
        Actor->MarkPackageDirty();
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_APPLY_FAILED_SAFE_LEGACY_OSM: ") +
            Error;
        return false;
    }

    FString PresentationReport;
    FString SceneReport;
    if (!Actor->IsV5CSurroundingsPresentationActive() ||
        !Actor->ValidateV5CSurroundingsPresentation(PresentationReport) ||
        !Actor->ValidatePublicViewScene(SceneReport, false))
    {
        Actor->RestoreLegacyOsmSurroundingsPresentationFailSafe();
        Actor->MarkPackageDirty();
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_APPLY_FAILED_SAFE_LEGACY_OSM: ") +
            (!PresentationReport.IsEmpty()
                ? PresentationReport
                : SceneReport);
        return false;
    }

    Actor->MarkPackageDirty();
    OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_APPLIED_CURRENT_WORLD_UNSAVED: exact V5C official-preferred/fallback buildings and official-source planning ground graphic are atomically visible at identity; legacy ODbL context is hidden. Both V5C siblings remain render-only, NoCollision and no-navigation. The ground graphic grants no road-width/material, elevation/Z, survey, sensor or RF authority. The map is dirty but was not saved.");
    return true;
}

bool UTRIADIstanaExploreV5CSurroundingsEditorLibrary::
    ValidateIstanaExploreV5CSurroundingsCurrentWorld(FString& OutReport)
{
    FString AssetReport;
    if (!ValidateAtomicV5CAssetRoots(AssetReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5C_SURROUNDINGS_WORLD_INVALID: ") +
            AssetReport;
        return false;
    }

    FString Error;
    ATRIADIstanaPublicViewSceneActor* Actor =
        ResolveCurrentPublicViewScene(Error);
    FString PresentationReport;
    FString SceneReport;
    if (!Actor || !Actor->IsV5CSurroundingsPresentationActive() ||
        !Actor->ValidateV5CSurroundingsPresentation(PresentationReport) ||
        !Actor->ValidatePublicViewScene(SceneReport, false))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5C_SURROUNDINGS_WORLD_INVALID: ") +
            (!Error.IsEmpty()
                ? Error
                : (!PresentationReport.IsEmpty()
                    ? PresentationReport
                    : SceneReport));
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5C_SURROUNDINGS_WORLD_VALID_UNSAVED: exact identity V5C buildings and three-asset official planning ground context are atomically visible, legacy ODbL hidden, and fail-safe state readable. Ground context has no road-width/material, elevation/Z or survey authority; neither sibling has collision/navigation/sensor/RF authority. No map save was performed.");
    return true;
}

bool UTRIADIstanaExploreV5CSurroundingsEditorLibrary::
    RestoreIstanaPublicViewOsmContextInCurrentWorld(FString& OutMessage)
{
    FString Error;
    ATRIADIstanaPublicViewSceneActor* Actor =
        ResolveCurrentPublicViewScene(Error);
    if (!Actor)
    {
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_RESTORE_REFUSED: ") +
            Error;
        return false;
    }

    ModifyPresentationObjects(Actor);
    Actor->RestoreLegacyOsmSurroundingsPresentationFailSafe();
    FString Readback;
    if (Actor->IsV5CSurroundingsPresentationActive() ||
        !Actor->ValidateV5CSurroundingsPresentation(Readback))
    {
        OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_RESTORE_FAILED: ") +
            Readback;
        return false;
    }

    Actor->MarkPackageDirty();
    OutMessage = TEXT("EXPLORE_V5C_SURROUNDINGS_RESTORED_SAFE_LEGACY_OSM_UNSAVED: legacy ODbL context is visible; both the V5C building and official planning ground-context siblings are empty, hidden and inactive. No road-width/material, elevation/Z, survey, sensor or RF authority was created. The map is dirty but was not saved.");
    return true;
}
