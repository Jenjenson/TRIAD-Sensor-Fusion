#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

namespace TRIADIstanaExploreV5DTemasekShophouseAssetFactory
{
bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError);
bool ValidateAssets(FString& OutReport);
bool LoadValidatedRuntimeMesh(UStaticMesh*& OutMesh, FString& OutReport);
bool CommitActiveFreshAssetTransaction(FString& OutReport);
bool RollbackActiveFreshAssetTransaction(FString& OutReport);
} // namespace TRIADIstanaExploreV5DTemasekShophouseAssetFactory
