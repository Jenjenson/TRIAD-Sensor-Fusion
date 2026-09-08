#pragma once

#include "CoreMinimal.h"

struct FTRIADIstanaExploreV5DR29VegetationAssets;

namespace TRIADIstanaExploreV5DR29VegetationAssetFactory
{
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);
bool ValidateAssets(FString& OutReport);
bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR29VegetationAssets& OutAssets,
    FString& OutError);
const TArray<FString>& GetExpectedAssetObjectPaths();
} // namespace TRIADIstanaExploreV5DR29VegetationAssetFactory
