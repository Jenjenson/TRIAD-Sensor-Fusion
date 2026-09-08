#pragma once

#include "CoreMinimal.h"

class UObject;

namespace TRIADIstanaExploreV5CPorticoAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetPorticoMeshObjectPath();
const TArray<FString>& GetOrderedMaterialObjectPaths();

/** Create only in a fresh, empty V5C portico namespace. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Cold-readback validation of the exact nine-asset namespace and sources. */
bool ValidateAssets(FString& OutReport);
} // namespace TRIADIstanaExploreV5CPorticoAssetFactory
