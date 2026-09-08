#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR32Player0CaptureLibrary.generated.h"

/**
 * Runtime-only, opt-in Player0 evidence seam for the exact R32 V5D map.
 *
 * The API is closed over ten reviewed poses: the inherited five-view scene
 * roster, exact turf-readability probes at 12 and 50 metres, and exact turf
 * intersection-distance probes at the 65, 90, and 95 metre fade/cull points.
 * It never mutates map, provider,
 * Cesium, AirSim, simulation, sensor, collision, navigation, RF, or
 * geospatial state. Calls are admitted only in a cooked Game world with the
 * exact R32 and predecessor owners.
 */
UCLASS()
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DR32Player0CaptureLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Validate the exact cooked Game/Player0/R32 and predecessor state. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Player0 Evidence")
    static bool GetIstanaExploreV5DR32Player0CaptureState(
        FString& OutReport);

    /** Move and freeze Player0 at one exact reviewed pose ID. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Player0 Evidence")
    static bool SetIstanaExploreV5DR32Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage);

    /** Request one exact 2560x1440 SDR PNG for the active reviewed pose. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Player0 Evidence")
    static bool CaptureIstanaExploreV5DR32Player0TurfView(
        const FString& PoseId,
        FString& OutMessage);

    /** Request a clean exit after the wrapper has verified all ten files. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R32 Player0 Evidence")
    static bool FinishIstanaExploreV5DR32Player0CaptureRun(
        FString& OutMessage);
};
