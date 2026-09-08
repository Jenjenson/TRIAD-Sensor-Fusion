#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.generated.h"

/** Callable, editor-only R28 asset and no-save map integration boundary. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R28|Editor")
    static bool EnsureR28EnvironmentAssets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R28|Editor")
    static bool ValidateR28EnvironmentAssets(FString& OutReport);

    /**
     * Spawn and configure exactly one identity R28 actor in the exact loaded
     * V5D hybrid map.  This endpoint never saves the map; the guarded caller
     * owns the combined transaction's single map commit.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R28|Editor")
    static bool ApplyR28EnvironmentToLoadedV5DHybridMap(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R28|Editor")
    static bool ValidateR28EnvironmentInLoadedV5DHybridMap(FString& OutReport);
};
