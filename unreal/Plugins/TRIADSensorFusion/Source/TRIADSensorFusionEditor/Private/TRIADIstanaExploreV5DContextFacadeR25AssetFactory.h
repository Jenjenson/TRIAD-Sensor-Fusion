#pragma once

#include "CoreMinimal.h"

class UObject;

namespace TRIADIstanaExploreV5DContextFacadeR25AssetFactory
{
/** Fresh additive namespace; the shared V5C materials are never mutated. */
const FString& GetAssetRootPath();
const FString& GetMasterMaterialObjectPath();

/** Official wall, official roof, fallback wall, fallback roof. */
const TArray<FString>& GetOrderedMaterialObjectPaths();

/** Create only in the exact empty R25 namespace. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Cold-readback validation of the exact five-asset material namespace. */
bool ValidateAssets(FString& OutReport);
} // namespace TRIADIstanaExploreV5DContextFacadeR25AssetFactory
