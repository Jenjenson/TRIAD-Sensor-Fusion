#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaPublicViewHeroV3EditorLibrary.generated.h"

/**
 * Additive, fail-closed integration for the v3 Istana public-view hero.
 *
 * The v3 workflow owns only a new hero/material namespace and a new map copy.
 * It never overwrites the v1/v2 maps or assets, and migration changes only the
 * public-view scene actor's BuildingHeroVisualComponent on a v2 duplicate.
 * The v1-authored collision, terrain, hardscape, context, vegetation, runtime
 * policy, and cameras inherited by v2 remain authoritative and byte/state
 * protected.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaPublicViewHeroV3EditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Prove that a Remote Control caller reached the intended local project. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|Editor")
    static bool ValidateIstanaPublicViewHeroV3RemoteControlProject(
        const FString& ExpectedProjectPath,
        FString& OutReport);

    /**
     * Import the frozen hero mesh and HeroMaterialsV3 pack into new v3-only
     * namespaces. A complete valid set is accepted idempotently; any partial
     * or invalid existing set is rejected and never overwritten.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|Editor")
    static bool ImportIstanaPublicViewHeroV3Assets(FString& OutMessage);

    /** Read-only source-freeze and imported-asset validation. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|Editor")
    static bool ValidateIstanaPublicViewHeroV3Assets(FString& OutReport);

    /**
     * Duplicate the exact v2 public-view map to the new v3 destination and
     * replace only BuildingHeroVisualComponent on the duplicate.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|Editor")
    static bool MigrateIstanaPublicViewExteriorMapToHeroV3(FString& OutMessage);

    /** Validate the currently loaded exact v3 map and hero-only preservation. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|Editor")
    static bool ValidateIstanaPublicViewHeroV3Map(FString& OutReport);

    /**
     * Validate authoritative non-simulated PIE on the exact V3 map. This is a
     * runtime check only: no map, asset, renderer setting, or camera is saved.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|QA")
    static bool ValidateIstanaPublicViewHeroV3PlayWorldReadiness(
        FString& OutReport);

    /**
     * Capture an isolated 4K hero QA frame through a transient scene-local
     * camera. The camera uses manual D65 exposure/color calibration and a
     * scoped TSR history-quality override which is restored before return.
     * Supported presets are HERO_FRONT, HERO_OBLIQUE_RIGHT,
     * HERO_OBLIQUE_LEFT, HERO_FACADE_MACRO, HERO_ORBIT_RIGHT, and
     * HERO_ORBIT_LEFT.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|QA")
    static bool CaptureIstanaPublicViewHeroV3QualityFrame(
        const FString& CameraPreset,
        const FString& OutputFileName,
        int32 TemporalWarmupFrames,
        FString& OutMessage);

    /** Pause every discovered AirSim mode before guarded V3 PIE teardown. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V3|QA")
    static bool QuiesceIstanaPublicViewHeroV3PlayWorldForStop(
        FString& OutMessage);
};
