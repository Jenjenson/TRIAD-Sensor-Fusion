#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaPublicViewEditorLibrary.generated.h"

/** Additive editor automation for the isolated Istana public-view namespace. */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaPublicViewEditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Prove that a Remote Control caller reached the intended local project. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool ValidateIstanaPublicViewRemoteControlProject(
        const FString& ExpectedProjectPath,
        FString& OutReport);

    /**
     * Import the complete frozen ten-mesh, 24-texture, and 22-material set
     * into only /Game/TRIAD/IstanaPublicView. Complete existing output is
     * validated and accepted idempotently; partial/invalid output is never
     * overwritten.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool ImportIstanaPublicViewAssets(FString& OutMessage);

    /** Read-only validation of the exact isolated mesh/texture/material set. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool ValidateIstanaPublicViewAssets(FString& OutReport);

    /**
     * Create only /Game/Maps/Istana_PublicView_Exterior_v1 from a new blank
     * world. The operation refuses overwrite, dirty packages, PIE, missing
     * public-view assets, invalid tree placements, or any legacy dependency.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool BuildIstanaPublicViewExteriorMap(FString& OutMessage);

    /** Validate the currently loaded exact public-view destination read-only. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool ValidateIstanaPublicViewExteriorMap(FString& OutReport);

    /** Validate runtime fog suppression, fixed primary camera, and claim label. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool ValidateIstanaPublicViewPlayWorldReadiness(FString& OutReport);

    /**
     * Capture one exact tagged public-view camera from the active PIE game
     * viewport, then restore and verify the primary Player 0 camera.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool CaptureIstanaPublicViewPlayCamera(
        const FString& CameraPreset,
        const FString& OutputFileName,
        FString& OutMessage);

    /** Pause and verify every begun AirSim mode before an explicit PIE stop. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Editor")
    static bool QuiesceIstanaPublicViewPlayWorldForStop(FString& OutMessage);

    /**
     * V2-only runtime readiness. The editor map must first pass the independent
     * hero-v2 preservation validator; the v1 map is never accepted here.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool ValidateIstanaPublicViewHeroV2PlayWorldReadiness(
        FString& OutReport);

    /**
     * Capture a transient acceptance camera in the v2 PIE world. Supported
     * whole-hero presets are HERO_FRONT_CLOSE and HERO_FRONT_OBLIQUE_CLOSE;
     * HERO_FACADE_MACRO is an intentional detail-only façade crop. No camera
     * or surrounding actor is saved into either map.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool CaptureIstanaPublicViewHeroV2PlayCamera(
        const FString& CameraPreset,
        const FString& OutputFileName,
        FString& OutMessage);

    /** V2-map-gated AirSim quiescence before explicit PIE teardown. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V2|Editor")
    static bool QuiesceIstanaPublicViewHeroV2PlayWorldForStop(
        FString& OutMessage);
};
