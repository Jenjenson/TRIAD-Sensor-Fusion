#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR33Player0CaptureLibrary.generated.h"

/**
 * Runtime-only, opt-in Player0 comparison seam for the exact R33 V5D map.
 *
 * The seam captures the same four fixed poses in GooglePrimary and
 * CwtPresented.  It never treats a requested presentation as provider
 * readiness, never samples height, and never writes map, simulation, sensor,
 * collision, navigation, RF, or geospatial state.  Finish restores
 * GooglePrimary before requesting a clean process exit.
 */
UCLASS()
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DR33Player0CaptureLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Validate exact R33 runtime ownership and report the current state. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 Player0 Evidence")
    static bool GetIstanaExploreV5DR33Player0CaptureState(
        FString& OutReport);

    /** Request GooglePrimary or CwtPresented; CWT readiness remains asynchronous. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 Player0 Evidence")
    static bool SetIstanaExploreV5DR33Player0Presentation(
        const FString& PresentationId,
        FString& OutMessage);

    /** Move and freeze Player0 at one exact comparison pose. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 Player0 Evidence")
    static bool SetIstanaExploreV5DR33Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage);

    /** Capture one exact 2560x1440 SDR state/pose comparison PNG. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 Player0 Evidence")
    static bool CaptureIstanaExploreV5DR33Player0ComparisonView(
        const FString& PresentationId,
        const FString& PoseId,
        FString& OutMessage);

    /** Restore GooglePrimary and request a clean exit. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R33 Player0 Evidence")
    static bool FinishIstanaExploreV5DR33Player0CaptureRun(
        FString& OutMessage);
};
