#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5CGroundContextEditorLibrary.generated.h"

/**
 * Isolated source-to-editor integration for the V5C official planning
 * ground-context asset root. It creates exactly one render-only static mesh
 * and two child material instances; it never changes a world or map.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API
    UTRIADIstanaExploreV5CGroundContextEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Ground Context|Editor")
    static bool ImportIstanaExploreV5CGroundContextAssets(
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5C|Ground Context|Editor")
    static bool ValidateIstanaExploreV5CGroundContextAssets(
        FString& OutReport);
};
