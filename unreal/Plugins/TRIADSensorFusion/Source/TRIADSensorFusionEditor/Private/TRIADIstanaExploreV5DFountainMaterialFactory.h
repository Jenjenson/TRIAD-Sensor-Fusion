#pragma once

#include "CoreMinimal.h"

class UMaterial;

namespace TRIADIstanaExploreV5DFountainMaterialFactory
{
const FString& GetWaterMaterialObjectPath();
const FString& GetSprayMaterialObjectPath();
const FString& GetEmbeddedWaterSuppressorMaterialObjectPath();

/**
 * Create the exact initially-absent V5D fountain material roster, upgrade the
 * exact persisted R4 water/spray pair with its initially-absent suppressor, or
 * cold-validate the already-complete roster. The operation refuses dirty or
 * mixed state and owns the exact new-package save boundary.
 */
bool EnsureFountainRealismMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    FString& OutReport);

/** Validate the exact persisted three-material roster and active shader maps. */
bool ValidateFountainRealismMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    FString& OutReport);
} // namespace TRIADIstanaExploreV5DFountainMaterialFactory
