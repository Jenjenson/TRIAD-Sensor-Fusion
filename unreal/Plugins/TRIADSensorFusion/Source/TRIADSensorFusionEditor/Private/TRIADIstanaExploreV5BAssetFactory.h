#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UObject;
class UStaticMesh;
struct FAssetData;

namespace TRIADIstanaExploreV5BAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetPorticoMeshObjectPath();
const FString& GetHardscapeRenderSuccessorMeshObjectPath();
const FString& GetAccentTurfMeshObjectPath();
const FString& GetAccentTurfMaterialObjectPath();
const FString& GetFormalBedVeneerMeshObjectPath();
const FString& GetTreeBaseMulchMeshObjectPath();
const FString& GetFormalBedVeneerMaterialObjectPath();
const FString& GetFountainSurfaceMeshObjectPath();
const FString& GetFountainFoamMeshObjectPath();
const FString& GetFountainImpactRingMeshObjectPath();
const FString& GetFountainPlumeMeshObjectPath();
const FString& GetFountainCentralPlumeMeshObjectPath();
const FString& GetPaverInnerMeshObjectPath();
const FString& GetPaverOuterMeshObjectPath();
const FString& GetFountainSurfaceMaterialObjectPath();
const FString& GetFountainFoamMaterialObjectPath();
const FString& GetFountainSprayMaterialObjectPath();
const FString& GetPaverMaterialObjectPath();
const FString& GetPachiraBarkMaterialObjectPath();
const FString& GetPachiraLeavesMaterialObjectPath();
const TArray<FString>& GetPachiraMeshObjectPaths();

/**
 * True only for UE5.5's exact unsaved StaticMeshLOD import scratch row.
 * A scratch-named package with any canonical or sidecar artifact is rejected.
 */
bool IsExactUnpersistedAccentTurfLodImportScratch(const FAssetData& Row);

/** Create only a fresh, empty V5B namespace. The caller owns the exact save. */
bool CreateFreshExploreV5BAssets(
    TArray<UObject*>& OutAssets,
    FString& OutError);

/**
 * Reimport only the exact persisted accent-turf mesh UObject in place.
 * The caller owns disk backup, exact single-asset save, cold reload, and
 * rollback. All 37 namespace paths must exist and all packages must be clean.
 */
bool RebuildExistingAccentTurfAsset(
    UStaticMesh*& OutAccentTurf,
    FString& OutError);

/** Admit only the exact clean R10 carrier or an idempotent exact R11 carrier. */
bool ValidateAccentTurfAssetForR11Upgrade(
    bool& bOutAlreadyR11,
    FString& OutError);

/** Validate the exact persisted 37-asset V5B visual namespace. */
bool ValidateExploreV5BAssets(FString& OutReport);
} // namespace TRIADIstanaExploreV5BAssetFactory
