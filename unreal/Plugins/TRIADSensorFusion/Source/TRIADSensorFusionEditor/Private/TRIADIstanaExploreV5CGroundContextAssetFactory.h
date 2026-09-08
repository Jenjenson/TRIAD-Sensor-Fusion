#pragma once

#include "CoreMinimal.h"

class UObject;

namespace TRIADIstanaExploreV5CGroundContextAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetGroundContextMeshObjectPath();
const TArray<FString>& GetOrderedMaterialObjectPaths();
int32 GetExpectedImportedTriangleCount();
int32 GetExpectedMaterialCount();

/** Create only in a fresh, empty V5C GroundContext namespace. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Cold-readback validation of the exact three-asset namespace and sources. */
bool ValidateAssets(FString& OutReport);
} // namespace TRIADIstanaExploreV5CGroundContextAssetFactory
