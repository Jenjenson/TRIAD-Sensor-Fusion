#include "TRIADIstanaExploreV5CGroundContextEditorLibrary.h"

#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5CGroundContextAssetFactory.h"

bool UTRIADIstanaExploreV5CGroundContextEditorLibrary::
    ImportIstanaExploreV5CGroundContextAssets(FString& OutMessage)
{
    FString Existing;
    if (TRIADIstanaExploreV5CGroundContextAssetFactory::ValidateAssets(
            Existing))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5C_GROUND_CONTEXT_ASSETS_ALREADY_VALID: ") +
            Existing;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5C_GROUND_CONTEXT_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5CGroundContextAssetFactory::CreateFreshAssets(
            FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5C_GROUND_CONTEXT_IMPORT_FAILED: ") +
            Error;
        return false;
    }
    if (FreshAssets.Num() != 3 || FreshAssets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(FreshAssets, false) ||
        !TRIADIstanaExploreV5CGroundContextAssetFactory::ValidateAssets(
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5C_GROUND_CONTEXT_IMPORT_FAILED_AFTER_FRESH_CREATE: only the exact three V5C GroundContext assets were offered to save. ") +
            Error;
        return false;
    }

    OutMessage = TEXT("Imported and cold-readback-validated one exact hash-pinned, identity, full-fidelity Nanite official-planning ground-context mesh and two ordered child material instances of MI_IPV5_HardscapeStone. The display-only 0.8 m road-graphic stroke is not a physical road width; no textures, collision, navigation, world, map, survey, elevation/Z, sensor or RF authority were created or changed.");
    return true;
}

bool UTRIADIstanaExploreV5CGroundContextEditorLibrary::
    ValidateIstanaExploreV5CGroundContextAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5CGroundContextAssetFactory::ValidateAssets(
        OutReport);
}
