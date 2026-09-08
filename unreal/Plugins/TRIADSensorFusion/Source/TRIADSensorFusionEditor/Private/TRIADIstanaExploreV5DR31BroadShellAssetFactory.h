#pragma once

#include "CoreMinimal.h"

class UObject;

namespace TRIADIstanaExploreV5DR31BroadShellAssetFactory
{
/** Fresh additive namespace; no predecessor material or mesh is mutated. */
const FString& GetAssetRootPath();
const FString& GetMasterMaterialObjectPath();

/** Official wall, official roof, fallback wall, fallback roof. */
const TArray<FString>& GetOrderedMaterialObjectPaths();

/** Create one texture-backed master and exactly four role instances. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Cold validation of source pins, exact five-asset roster and compiled materials. */
bool ValidateAssets(FString& OutReport);
} // namespace TRIADIstanaExploreV5DR31BroadShellAssetFactory
