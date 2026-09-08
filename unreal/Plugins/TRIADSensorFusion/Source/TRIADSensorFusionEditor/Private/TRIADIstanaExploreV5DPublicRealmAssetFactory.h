#pragma once

#include "CoreMinimal.h"

class UObject;
struct FTRIADIstanaExploreV5DPublicRealmAssets;
struct FTRIADIstanaExploreV5DPublicRealmProvenance;

namespace TRIADIstanaExploreV5DPublicRealmAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetCoreMeshObjectPath();
const FString& GetFallbackMeshObjectPath();
const FString& GetConcreteMaterialObjectPath();
const FString& GetAsphaltMaterialObjectPath();
const FString& GetRoadGraphicSuppressionMaterialObjectPath();
const TArray<FString>& GetOrderedSemanticMaterialObjectPaths();

/** Create two meshes, one concrete MIC, and both corrected visual materials. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/**
 * Add only the PBR asphalt and fully clipped RoadGraphic suppressor when the
 * exact persisted three-asset predecessor and empty Visual R2 root exist.
 */
bool CreateFreshVisualMaterialUpgrade(
    TArray<UObject*>& OutAssets,
    FString& OutError);

/** Validate the immutable two-mesh/one-concrete predecessor only. */
bool ValidateLegacyAssets(FString& OutReport);

/** Cold-validate the exact persisted namespace, sources, slots, and topology. */
bool ValidateAssets(FString& OutReport);

/**
 * Load the exact validated runtime assets and the single canonical provenance
 * record. Consumers must not duplicate the source epoch, identifiers, or
 * digests outside this admission boundary.
 */
bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DPublicRealmAssets& OutAssets,
    FTRIADIstanaExploreV5DPublicRealmProvenance& OutProvenance,
    FString& OutError);
} // namespace TRIADIstanaExploreV5DPublicRealmAssetFactory
