#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV2EditorLibrary.generated.h"

/**
 * Additive editor integration for the evidence-aligned Istana Explore V2 map.
 *
 * V2 deliberately leaves the frozen V1-V5 maps and Explore V1 byte-exact.  It
 * reuses the validated V1 mesh/texture assets, adds V2-owned wind materials,
 * and replaces only the duplicated Explore landscape actor.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreV2EditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V2|Editor")
    static bool CreateIstanaExploreV2WindMaterials(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V2|Editor")
    static bool BuildIstanaExploreV2Map(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V2|Editor")
    static bool ValidateIstanaExploreV2Map(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V2|Editor")
    static bool ValidateIstanaExploreV2PlayWorld(FString& OutReport);

    /** Capture the validated live Player 0 Explore V2 view without overwriting. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V2|Editor")
    static bool CaptureIstanaExploreV2PlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V2|Editor")
    static bool QuiesceIstanaExploreV2PlayWorldForStop(FString& OutMessage);
};
