#pragma once

#include "CoreMinimal.h"

struct FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets;

namespace TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory
{
const FString& GetAssetRootPath();
const TArray<FString>& GetExpectedMaterialObjectPaths();

/**
 * Duplicate exactly four clean R29 grass materials into an isolated R32 root.
 * Only the base-colour and roughness custom-expression bodies are replaced;
 * mesh geometry, opacity, WPO, normals, source packages, and maps are untouched.
 */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Cold/read-only validation of the four saved R32 material derivatives. */
bool ValidateAssets(FString& OutReport);

/** Load exact R29 meshes plus the isolated R32 render-only materials. */
bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& OutAssets,
    FString& OutError);
} // namespace TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory
