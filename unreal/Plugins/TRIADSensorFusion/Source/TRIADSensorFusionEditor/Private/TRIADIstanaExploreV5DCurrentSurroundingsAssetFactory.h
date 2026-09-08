#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

namespace TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetSurroundingsMeshObjectPath();

/** Independent namespace for the additive 43,492-triangle local fallback. */
const FString& GetLocalFallbackSuppressedAssetRootPath();
const FString& GetLocalFallbackSuppressedMeshObjectPath();

/**
 * Import the one hash-pinned render mesh into an otherwise empty V5D
 * surroundings namespace. No world, map, collision, navigation, sensor, or
 * RF authority is created by this operation.
 */
bool CreateFreshAsset(UStaticMesh*& OutAsset, FString& OutError);

/** Validate the persisted one-asset namespace and every pinned source. */
bool ValidateAsset(FString& OutReport);

/**
 * Import the pre-generated, hash-pinned local-fallback suppression V1 mesh.
 * A valid existing asset is returned idempotently. The original canonical
 * mesh and provenance are never replaced or mutated.
 */
bool CreateFreshLocalFallbackSuppressedAsset(
    UStaticMesh*& OutAsset,
    FString& OutError);

/** Cold-validate the persisted additive suppression asset and all inputs. */
bool ValidateLocalFallbackSuppressedAsset(FString& OutReport);

/** Independent V2 namespace omitting the two R24 shells plus the known floating OSM shell. */
const FString& GetLocalFallbackSuppressedV2AssetRootPath();
const FString& GetLocalFallbackSuppressedV2MeshObjectPath();

/** Import the exact three-group V2 render-only derivative without mutating V1. */
bool CreateFreshLocalFallbackSuppressedV2Asset(
    UStaticMesh*& OutAsset,
    FString& OutError);

/** Cold-validate the persisted V2 asset and every hash-pinned input. */
bool ValidateLocalFallbackSuppressedV2Asset(FString& OutReport);
} // namespace TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory
