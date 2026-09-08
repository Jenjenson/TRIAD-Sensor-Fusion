#pragma once

#include "CoreMinimal.h"

class UObject;
struct FTRIADIstanaExploreV5DR30FacadeLookdevAssets;

namespace TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetMasterMaterialObjectPath();
const TArray<FString>& GetExpectedAssetObjectPaths();

/** Create one context-safe opaque PBR master and exactly eleven material instances. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Cold/read-only validation of the source contract and exact saved asset roster. */
bool ValidateAssets(FString& OutReport);

/** Load the unchanged R29/R28 presentation assets plus exact R30 overrides. */
bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR30FacadeLookdevAssets& OutAssets,
    FString& OutError);
} // namespace TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory
