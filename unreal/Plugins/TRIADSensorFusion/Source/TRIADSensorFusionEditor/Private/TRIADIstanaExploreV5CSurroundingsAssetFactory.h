#pragma once

#include "CoreMinimal.h"

class UObject;

namespace TRIADIstanaExploreV5CSurroundingsAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetSurroundingsMeshObjectPath();
const TArray<FString>& GetOrderedMaterialObjectPaths();

/** Create only in a fresh, empty V5C surroundings namespace. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Cold-readback validation of the exact six-asset namespace and sources. */
bool ValidateAssets(FString& OutReport);
} // namespace TRIADIstanaExploreV5CSurroundingsAssetFactory
