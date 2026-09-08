#pragma once

#include "CoreMinimal.h"

class UObject;
struct FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets;

namespace TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetFacadeMeshObjectPath();
const TArray<FString>& GetExpectedAssetObjectPaths();

/** Create the isolated R29 material instances and import the exact facade OBJ. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Validate source hashes and the exact persisted 12-asset namespace. */
bool ValidateAssets(FString& OutReport);

/** Load the R29 mesh plus the unchanged validated R28 presentation assets. */
bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets& OutAssets,
    FString& OutError);
} // namespace TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory
