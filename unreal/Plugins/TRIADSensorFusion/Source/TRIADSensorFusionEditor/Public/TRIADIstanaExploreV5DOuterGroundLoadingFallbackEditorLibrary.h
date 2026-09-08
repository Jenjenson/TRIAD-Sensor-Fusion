#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary.generated.h"

/**
 * Editor-only, isolated import/validation endpoint for the bounded V5D outer
 * ground provider-loading render asset. It never edits an actor or map.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Outer Ground Loading Fallback|Editor")
    static bool ImportIstanaExploreV5DOuterGroundLoadingFallbackAssets(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Outer Ground Loading Fallback|Editor")
    static bool ValidateIstanaExploreV5DOuterGroundLoadingFallbackAssets(
        FString& OutReport);
};
