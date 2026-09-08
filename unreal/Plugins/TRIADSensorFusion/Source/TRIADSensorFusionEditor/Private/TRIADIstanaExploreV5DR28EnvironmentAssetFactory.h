#pragma once

#include "CoreMinimal.h"

class UObject;
struct FTRIADIstanaExploreV5DR28EnvironmentAssets;

namespace TRIADIstanaExploreV5DR28EnvironmentAssetFactory
{
const FString& GetAssetRootPath();
const FString& GetConnectivePublicRealmMeshObjectPath();
const FString& GetContextArchitecturalDressingMeshObjectPath();
const FString& GetOuterGroundMaterialObjectPath();
const TArray<FString>& GetExpectedAssetObjectPaths();

/** Create the isolated texture-free materials and import both render meshes. */
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);

/** Validate the exact in-memory or persisted R28 namespace. */
bool ValidateAssets(FString& OutReport);

/** Load the exact validated runtime pointers, including the reused annulus. */
bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR28EnvironmentAssets& OutAssets,
    FString& OutError);
} // namespace TRIADIstanaExploreV5DR28EnvironmentAssetFactory
