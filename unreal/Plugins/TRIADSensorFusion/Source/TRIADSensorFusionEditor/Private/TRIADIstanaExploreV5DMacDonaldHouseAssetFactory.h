#pragma once

#include "CoreMinimal.h"

class UObject;
class UStaticMesh;

namespace TRIADIstanaExploreV5DMacDonaldHouseAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetMeshObjectPath();
const TArray<FString>& GetOrderedMaterialObjectPaths();

/** Create the exact render mesh and nine deterministic procedural PBR materials. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/**
 * Roll back the exact root diff owned by the active empty-predecessor import.
 * This is intentionally unavailable for arbitrary/pre-existing asset roots.
 */
bool RollbackActiveFreshAssetTransaction(FString& OutReport);

/** Commit ownership only after save, reload, and cold validation all pass. */
bool CommitActiveFreshAssetTransaction(FString& OutReport);

/** Cold-validate sources, namespace, mesh, provenance, and material graphs. */
bool ValidateAssets(FString& OutReport);

/** Validate and return the sole runtime mesh without granting any truth authority. */
bool LoadValidatedRuntimeMesh(UStaticMesh*& OutMesh, FString& OutError);
} // namespace TRIADIstanaExploreV5DMacDonaldHouseAssetFactory
