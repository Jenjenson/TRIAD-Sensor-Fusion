#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "TRIADIstanaPublicViewHeroV4EditorLibrary.generated.h"

/**
 * Additive, fail-closed integration surface for the V4 Istana public-view hero.
 *
 * V4 owns only new geometry/material namespaces and a new map duplicated from
 * the exact V3 map. It may reference the frozen V3 texture assets in place but
 * never edits or copies them. V1/V2/V3 assets and maps, collision, terrain,
 * hardscape, context, vegetation, world lighting, cameras, renderer settings,
 * and project settings remain outside the V4 mutation boundary.
 *
 * Until the independently reviewed geometry and material freezes are bound in
 * the implementation, every operation other than project-identity validation
 * returns false before any package is loaded for mutation, created, or saved.
 */
UCLASS()
class TRIADSENSORFUSIONEDITOR_API UTRIADIstanaPublicViewHeroV4EditorLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Prove that a Remote Control caller reached the intended local project. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|Editor")
    static bool ValidateIstanaPublicViewHeroV4RemoteControlProject(
        const FString& ExpectedProjectPath,
        FString& OutReport);

    /**
     * Import the exact frozen V4 hero mesh and create only additive V4 masters
     * and instances. V4 material instances reference the frozen V3 UTextures;
     * no texture pixel or package is copied. Existing partial/unknown V4 state
     * is rejected and never overwritten.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|Editor")
    static bool ImportIstanaPublicViewHeroV4Assets(FString& OutMessage);

    /** Read-only validation of the frozen sources and imported V4 asset set. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|Editor")
    static bool ValidateIstanaPublicViewHeroV4Assets(FString& OutReport);

    /**
     * Duplicate the exact V3 public-view map to the V4 destination and replace
     * only BuildingHeroVisualComponent on the duplicate. V1 collision and all
     * surroundings/cameras remain byte/state protected.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|Editor")
    static bool MigrateIstanaPublicViewExteriorMapToHeroV4(FString& OutMessage);

    /** Validate the loaded exact V4 map and hero-only preservation invariants. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|Editor")
    static bool ValidateIstanaPublicViewHeroV4Map(FString& OutReport);

    /** Validate isolated non-simulated PIE on the exact V4 map. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|QA")
    static bool ValidateIstanaPublicViewHeroV4PlayWorldReadiness(
        FString& OutReport);

    /**
     * Capture one neutral 4K hero QA frame with transient camera-local
     * calibration and scoped TSR/SSGI/SSR quality overrides restored exactly.
     */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|QA")
    static bool CaptureIstanaPublicViewHeroV4QualityFrame(
        const FString& CameraPreset,
        const FString& OutputFileName,
        int32 TemporalWarmupFrames,
        FString& OutMessage);

    /** Quiesce discovered AirSim modes before guarded V4 PIE teardown. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Istana Public View|Hero V4|QA")
    static bool QuiesceIstanaPublicViewHeroV4PlayWorldForStop(
        FString& OutMessage);
};
