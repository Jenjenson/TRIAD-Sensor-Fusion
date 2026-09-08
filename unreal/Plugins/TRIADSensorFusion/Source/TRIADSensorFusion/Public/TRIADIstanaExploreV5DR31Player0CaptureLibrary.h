#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR31Player0CaptureLibrary.generated.h"

/**
 * Runtime-only, opt-in Player0 evidence seam for the exact R31 V5D map.
 *
 * The API is deliberately closed over five reviewed poses and one additional
 * raster-fallback image at the unchanged high-occupancy pose. It cannot mutate
 * the map, provider, Cesium, AirSim, simulation, sensor, collision, navigation,
 * or RF state. Calls are admitted only in a cooked Game world launched with a
 * bounded capture run token and output root. R31 pixels are accepted only while
 * the context policy is in its honest local ProviderFallback presentation.
 */
UCLASS()
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DR31Player0CaptureLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Validate the exact cooked Game/Player0/R31/provider/AirSim/Cesium state. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Player0 Evidence")
    static bool GetIstanaExploreV5DR31Player0CaptureState(
        FString& OutReport);

    /** Move and freeze Player0 at one exact reviewed pose ID. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Player0 Evidence")
    static bool SetIstanaExploreV5DR31Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage);

    /**
     * Select and read back the transient render path used by the next image.
     * Only NANITE_ON and RASTER_FALLBACK are accepted. Raster fallback is
     * admitted only at the exact surroundings_oblique_macdonald pose.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Player0 Evidence")
    static bool SetIstanaExploreV5DR31Player0CaptureRenderPath(
        const FString& RenderPathId,
        FString& OutMessage);

    /**
     * Request one exact 2560x1440 SDR PNG for the active reviewed pose.
     * The PowerShell owner must wait for a stable decoded file and then repeat
     * the state readback before accepting the image.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Player0 Evidence")
    static bool CaptureIstanaExploreV5DR31Player0FallbackView(
        const FString& PoseId,
        const FString& RenderPathId,
        FString& OutMessage);

    /** Request a clean exit after the PowerShell owner has accepted all files. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R31 Player0 Evidence")
    static bool FinishIstanaExploreV5DR31Player0CaptureRun(
        FString& OutMessage);
};
