#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.generated.h"

/**
 * Remote-callable, editor-only admission boundary for the current public OSM
 * V5D surroundings presentation. This library creates or validates only the
 * isolated mesh/material assets; it never edits an actor or map.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor")
    static bool ImportIstanaExploreV5DCurrentSurroundingsAsset(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor")
    static bool ValidateIstanaExploreV5DCurrentSurroundingsAsset(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor")
    static bool ImportIstanaExploreV5DLocalFallbackSuppressedAsset(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor")
    static bool ValidateIstanaExploreV5DLocalFallbackSuppressedAsset(
        FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor")
    static bool ImportIstanaExploreV5DLocalFallbackSuppressionV2(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor")
    static bool ValidateIstanaExploreV5DLocalFallbackSuppressionV2(
        FString& OutReport);

    /**
     * Create only the additive five-material R25 context-facade namespace.
     * The V2 mesh, shared V5C materials, actors, and maps are not modified.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor",
        meta = (DisplayName = "Import Istana Explore V5D Context Facade R25 Assets"))
    static bool ImportIstanaExploreV5DContextFacadeR25Assets(
        FString& OutMessage);

    UFUNCTION(
        BlueprintCallable,
        Category = "TRIAD|Istana Explore V5D|Current Surroundings|Editor",
        meta = (DisplayName = "Validate Istana Explore V5D Context Facade R25 Assets"))
    static bool ValidateIstanaExploreV5DContextFacadeR25Assets(
        FString& OutReport);
};
