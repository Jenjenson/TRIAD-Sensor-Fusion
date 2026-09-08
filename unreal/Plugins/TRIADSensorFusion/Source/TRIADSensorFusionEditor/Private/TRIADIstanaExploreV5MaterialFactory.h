#pragma once

#include "CoreMinimal.h"

class UObject;

namespace TRIADIstanaExploreV5MaterialFactory
{
const FString& GetMaterialRootPath();
const FString& GetGrassBaseMaterialObjectPath();
const FString& GetGrassMaterialInstanceObjectPath();
const FString& GetFountainWaterMaterialObjectPath();
const FString& GetHardscapeStoneMaterialInstanceObjectPath();
const FString& GetContextRenderMaterialInstanceObjectPath();
const FString& GetContextRoofMaterialInstanceObjectPath();

bool CreateFreshExploreV5Materials(
    TArray<UObject*>& OutAssets,
    FString& OutError);

bool ValidateExploreV5Materials(FString& OutReport);
}
