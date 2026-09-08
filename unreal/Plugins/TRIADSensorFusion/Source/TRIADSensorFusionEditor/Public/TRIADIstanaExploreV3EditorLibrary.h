#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV3EditorLibrary.generated.h"

/**
 * Non-overwriting editor integration for the lawful-source Explore V3 layer.
 *
 * Import and map creation fail closed on partial outputs, altered frozen
 * source hashes, changed V1-V6/Explore V1/V2 packages, changed inherited
 * Explore V2 state, unsafe Island Tree LODs, or an identity/collision mismatch
 * in the render-only V7 portico detail.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreV3EditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool ImportIstanaExploreV3Assets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool BuildIstanaExploreV3Map(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool ValidateIstanaExploreV3Assets(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool ValidateIstanaExploreV3Map(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool ValidateIstanaExploreV3PlayWorld(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool CaptureIstanaExploreV3PlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool TriggerIstanaExploreV3PlayWindGust(
        float PeakStrengthCm,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool MoveIstanaExploreV3PlayPawnForQa(
        FVector DeltaCentimeters,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool GetIstanaExploreV3PlayStateReport(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V3|Editor")
    static bool QuiesceIstanaExploreV3PlayWorldForStop(FString& OutMessage);
};
