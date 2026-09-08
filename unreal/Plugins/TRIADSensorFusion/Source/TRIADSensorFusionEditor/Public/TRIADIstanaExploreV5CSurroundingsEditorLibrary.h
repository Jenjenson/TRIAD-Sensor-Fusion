#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5CSurroundingsEditorLibrary.generated.h"

/**
 * Hash-pinned source-to-editor integration for the V5C outer surroundings.
 * World apply atomically pairs the isolated building root with the separately
 * validated three-asset official planning ground-context root. It remains
 * transient: it marks the target actor/map dirty but never saves the map.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5CSurroundingsEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Surroundings|Editor")
    static bool ImportIstanaExploreV5CSurroundingsAssets(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Surroundings|Editor")
    static bool ValidateIstanaExploreV5CSurroundingsAssets(
        FString& OutReport);

    /** Atomically apply both V5C roots to the unique current V5B scene. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Surroundings|Editor")
    static bool ApplyIstanaExploreV5CSurroundingsToCurrentWorld(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Surroundings|Editor")
    static bool ValidateIstanaExploreV5CSurroundingsCurrentWorld(
        FString& OutReport);

    /** Recovery helper; leaves both V5C siblings empty and hidden. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Surroundings|Editor")
    static bool RestoreIstanaPublicViewOsmContextInCurrentWorld(
        FString& OutMessage);
};
