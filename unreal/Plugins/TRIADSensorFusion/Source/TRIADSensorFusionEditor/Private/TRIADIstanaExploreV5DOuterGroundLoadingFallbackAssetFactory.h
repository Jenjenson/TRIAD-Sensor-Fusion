#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UStaticMesh;

namespace TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetMeshObjectPath();
const FString& GetMaterialObjectPath();

/**
 * Create the exact material and render mesh in an otherwise empty isolated
 * namespace. A valid persisted pair is returned idempotently. No actor, map,
 * collision, navigation, sensor or RF state is created.
 */
bool CreateFreshAssets(
    UStaticMesh*& OutMesh,
    UMaterial*& OutMaterial,
    FString& OutError);

/** Cold-validate the two persisted assets and all seven pinned sources. */
bool ValidateAssets(FString& OutReport);
} // namespace TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory
