#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DTreeRealismEditorLibrary.generated.h"

class UWorld;

/** Editor-only asset creation, application and validation for V5D trees. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DTreeRealismEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool ApplyTreeCanopyRealismPassToLoadedV5DHybridMap(
        FString& OutMessage);

    /**
     * C++-only atomic hybrid-builder hook. The trust flag admits only an
     * untitled temp-package world before the hybrid builder's first save.
     */
    static bool ApplyTreeCanopyRealismPassToWorldForTrustedHybridBuilder(
        UWorld* InWorld,
        bool bTrustedUntitledHybridBuilder,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool ValidateTreeCanopyRealismPassInLoadedV5DHybridMap(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool EnsureTreeMaterialResponseAssets(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool EnsureTreeCanopyRealismMeshAssets(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D")
    static bool EnsureTreeCanopyRealismMeshAsset(
        int32 FormIndex,
        FString& OutReport);
};
