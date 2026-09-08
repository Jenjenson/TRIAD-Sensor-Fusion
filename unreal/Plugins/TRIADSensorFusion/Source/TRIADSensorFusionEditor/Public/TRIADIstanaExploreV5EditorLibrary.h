#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5EditorLibrary.generated.h"

/**
 * Additive editor integration for the Explore V5 appearance pass.
 *
 * V5 owns only new material assets, one appearance actor, a game-mode/pawn
 * selection and a new map package.  Geometry, transforms, instance counts,
 * collision, navigation, sun pose/colour/intensity and sensor/RF truth stay
 * inherited from the byte-exact Explore V4 source map. V5 alone owns its
 * bounded contact-shadow light-field changes.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaExploreV5EditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool ImportIstanaExploreV5Assets(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool ValidateIstanaExploreV5Assets(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool BuildIstanaExploreV5Map(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool ValidateIstanaExploreV5Map(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool ValidateIstanaExploreV5PlayWorld(FString& OutReport);

    /**
     * Queue an exact 1920x1080 HDR-off Player0 game-viewport PNG. This is the
     * sole V5 helper that also admits a public-validated V4 PIE world, only so
     * QA can produce a genuinely matched baseline; every other helper is V5-only.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool CaptureIstanaExploreV5PlayView(
        const FString& OutputFileName,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool TeleportIstanaExploreV5PlayPawnForQa(
        FVector WorldLocationCentimeters,
        FRotator WorldRotationDegrees,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool TriggerIstanaExploreV5PlayWindGust(
        float PeakStrengthCm,
        FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool GetIstanaExploreV5PlayStateReport(FString& OutReport);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5|Editor")
    static bool QuiesceIstanaExploreV5PlayWorldForStop(FString& OutMessage);
};
