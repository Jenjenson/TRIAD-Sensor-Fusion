#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaExploreV5DR30Player0CaptureLibrary.generated.h"

/**
 * Runtime-only, opt-in Player0 evidence seam for the exact R30 V5D map.
 *
 * The API is deliberately closed over five reviewed poses. It cannot mutate
 * the map, provider, Cesium, AirSim, simulation, sensor, collision, navigation,
 * or RF state. Calls are admitted only in a cooked Game world launched with a
 * bounded capture run token and output root. R30 pixels are accepted only while
 * the context policy is in its honest local ProviderFallback presentation.
 */
UCLASS()
class TRIADSENSORFUSION_API
    UTRIADIstanaExploreV5DR30Player0CaptureLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Validate the exact cooked Game/Player0/R30/provider/AirSim/Cesium state. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Player0 Evidence")
    static bool GetIstanaExploreV5DR30Player0CaptureState(
        FString& OutReport);

    /** Move and freeze Player0 at one exact reviewed pose ID. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Player0 Evidence")
    static bool SetIstanaExploreV5DR30Player0CapturePose(
        const FString& PoseId,
        FString& OutMessage);

    /**
     * Request one exact 2560x1440 SDR PNG for the active reviewed pose.
     * The PowerShell owner must wait for a stable decoded file and then repeat
     * the state readback before accepting the image.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Player0 Evidence")
    static bool CaptureIstanaExploreV5DR30Player0FallbackView(
        const FString& PoseId,
        FString& OutMessage);

    /** Request a clean exit after the PowerShell owner has accepted all files. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Explore V5D|R30 Player0 Evidence")
    static bool FinishIstanaExploreV5DR30Player0CaptureRun(
        FString& OutMessage);
};
