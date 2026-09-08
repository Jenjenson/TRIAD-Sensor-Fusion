#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5CPorticoEditorLibrary.generated.h"

/**
 * Isolated source-to-editor integration for the V5C central-portico overlay.
 * It creates no map and never saves or mutates a V4/V5/V5B/V8 asset package.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5CPorticoEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Editor")
    static bool ImportIstanaExploreV5CPorticoAssets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Editor")
    static bool ValidateIstanaExploreV5CPorticoAssets(FString& OutReport);

    /** Apply only to the unique V4 owner in the current editor world. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Editor")
    static bool ApplyIstanaExploreV5CPorticoToCurrentWorld(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Editor")
    static bool ValidateIstanaExploreV5CPorticoCurrentWorld(
        FString& OutReport);

    /** Explicit recovery helper; leaves the V5C component empty and hidden. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Editor")
    static bool RestoreIstanaExploreV8PorticoInCurrentWorld(
        FString& OutMessage);
};
