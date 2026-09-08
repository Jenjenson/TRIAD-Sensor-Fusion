#pragma once

#include "CoreMinimal.h"

class UMaterial;
class UStaticMesh;

namespace TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory
{
bool CreateFreshAssets(
    UStaticMesh*& OutMesh,
    UMaterial*& OutMaterial,
    FString& OutError);
bool ValidateAssets(FString& OutReport);
const FString& GetAssetRootPath();
const FString& GetMeshObjectPath();
const FString& GetMaterialObjectPath();
} // namespace TRIADIstanaExploreV5DR29CopernicusTerrainFallbackAssetFactory
