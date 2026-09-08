#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TRIADIstanaRuntimePolicyActor.generated.h"

class ACameraActor;
class UMaterialParameterCollection;
class UWorld;

/**
 * Destination-map-only runtime safeguards for the Istana context map.
 *
 * This actor deliberately contains no credentials or source URLs. It prevents
 * the sensor-fusion scenario manager from instantiating AirSim visual weather
 * in maps that use the inherited Singapore sky, clears the inherited height
 * fog without deleting sky/cloud actors, applies a transient facade-camera
 * profile, and performs a bounded zero-progress refresh. The refined authored
 * exterior remains the default visual/collision primary. A separate explicit
 * opt-in can reversibly yield only its pixels to exact-ready streamed context
 * while retaining authored collision.
 */
UCLASS(BlueprintType)
class TRIADSENSORFUSION_API ATRIADIstanaRuntimePolicyActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADIstanaRuntimePolicyActor();

    /** Return true when the world contains an enabled visual-weather suppression policy. */
    static bool ShouldSuppressAirSimVisualWeather(UWorld* World);

    /** Read back the current AirSim weather state rather than trusting a setter call. */
    UFUNCTION(BlueprintPure, Category = "TRIAD|Istana|Runtime")
    bool IsVisualWeatherSuppressionActive() const;

    /** Read back every inherited exponential-height-fog component. */
    bool IsExponentialHeightFogSuppressionActive(
        int32& OutComponentCount) const;

    /** Read back the transient facade-camera quality profile from PlayWorld. */
    bool IsRuntimeCameraQualityProfileActive(FString& OutReason) const;

    /**
     * Read back the reversible streamed-primary visual state. Collision on
     * the authored exterior must remain active even when its renderer yields.
     */
    bool IsStreamedPrimaryVisualActive(
        int32& OutTilesetCount,
        float& OutMinimumLoadProgress,
        FString& OutReason) const;

    /** Read back visual-only boundary suppression without changing AOI state. */
    bool IsVisualAcceptanceStudyOverlaySuppressionActive(
        int32& OutHiddenComponentCount) const;

    /**
     * Prove the loaded AirSim settings are the explicit TRIAD visual-only
     * acceptance profile: ComputerVision, RPC disabled, and no enabled lidar.
     * This is deliberately separate from production map/runtime readiness.
     */
    bool IsVisualAcceptanceAirSimProfileActive(FString& OutReason) const;

    /**
     * Pause active AirSim simulation modes before an explicit PIE teardown.
     * This is a best-effort quiescence request; the caller must still wait a
     * bounded drain interval before asking Unreal Editor to end PIE.
     */
    bool RequestAirSimQuiescenceForTeardown(int32& OutSimModeCount);

    /** Metadata authored by the v2 builder and checked by its validator. */
    void ConfigureMapMetadata(
        const FVector& InPreservedGeoreferenceOriginLongitudeLatitudeHeight,
        const FVector& InStudyCenterLongitudeLatitudeHeight);

    /** Record destination-only terrain calibration provenance. */
    void ConfigureGroundCalibration(
        double InGroundHeightMeters,
        int32 InAcceptedSampleCount,
        bool bInCalibrated,
        const FString& InHeightSource);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime")
    bool bSuppressAirSimVisualWeather = true;

    /** Always clear inherited exponential/volumetric fog in this v2 map. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bSuppressInheritedExponentialHeightFog = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float ExponentialFogSuppressionInitialDelaySeconds = 2.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float ExponentialFogSuppressionRetryDelaySeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "1", ClampMax = "12"))
    int32 MaximumExponentialFogSuppressionAttempts = 8;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bExponentialHeightFogSuppressionVerifiedAtRuntime = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    int32 ExponentialHeightFogComponentCountAtRuntime = 0;

    /** Require the persisted TRIAD-owned AirSim GameMode wrapper in this map. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bRequireIstanaAirSimGameMode = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    FString RequiredGameModeClassPath = TEXT("/Script/TRIADSensorFusion.TRIADIstanaAirSimGameMode");

    /** Runtime proof that PIE actually instantiated the required GameMode. */
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bGameModeOverrideVerifiedAtRuntime = false;

    /**
     * AirSim selects its ExternalCamera during startup. Reassert the tagged
     * destination camera for a short bounded window after initialization.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime")
    bool bEnforceTaggedRuntimeCameraForPlayer0 = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    FName RequiredRuntimeCameraTag = TEXT("TRIADIstanaRuntimeV2Camera");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float RuntimeCameraEnforcementInitialDelaySeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float RuntimeCameraEnforcementRetryDelaySeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "1", ClampMax = "12"))
    int32 MaximumRuntimeCameraEnforcementAttempts = 8;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bRuntimeCameraEnforcementComplete = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bRuntimeCameraViewTargetVerifiedAtRuntime = false;

    /** Runtime-only camera tuning; no editor-map post-process values are saved. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bApplyRuntimeCameraQualityProfile = true;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bRuntimeCameraQualityProfileVerifiedAtRuntime = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bAirSimQuiescenceRequestedForTeardown = false;

    /**
     * Runtime-only proof that the explicitly flagged visual-acceptance PIE
     * uses view-driven Cesium refinement. Never serialized to the editor map.
     */
    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bVisualAcceptanceStreamingPolicyAppliedAtRuntime = false;

    /**
     * Explicit opt-in only: yield refined authored pixels to loaded Cesium
     * photogrammetry while retaining authored collision/offline fallback.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime")
    bool bPreferStreamedIstanaVisualWhenReady = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "0.25", ClampMax = "5.0"))
    float StreamedPrimaryEvaluationIntervalSeconds = 1.0f;

    /** Hysteresis: restore the authored renderer when any tileset falls below this level. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "95.0", ClampMax = "99.9"))
    float StreamedPrimaryRestoreBelowLoadProgress = 99.0f;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bStreamedPrimaryVisualActiveAtRuntime = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bAuthoredExteriorFallbackVisibleAtRuntime = true;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bAuthoredExteriorCollisionPreservedAtRuntime = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    int32 StreamedPrimaryTilesetCountAtRuntime = 0;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    float StreamedPrimaryMinimumLoadProgressAtRuntime = 0.0f;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    bool bVisualAcceptanceStudyOverlaysHiddenAtRuntime = false;

    UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Runtime")
    int32 VisualAcceptanceHiddenStudyOverlayCountAtRuntime = 0;

    /**
     * Refresh a tileset only if it is still exactly at zero progress after the
     * initial grace period. This is a bounded recovery for stale provider
     * sessions, not a continuous network retry loop.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime")
    bool bRefreshUnreadyCesiumTilesets = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "1.0", ClampMax = "60.0"))
    float InitialTilesetCheckDelaySeconds = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "1.0", ClampMax = "120.0"))
    float TilesetRetryDelaySeconds = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Istana|Runtime", meta = (ClampMin = "0", ClampMax = "3"))
    int32 MaximumTilesetRefreshAttempts = 1;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Metadata")
    FVector PreservedGeoreferenceOriginLongitudeLatitudeHeight = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Metadata")
    FVector StudyCenterLongitudeLatitudeHeight = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Metadata")
    bool bExteriorGroundHeightCalibrated = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Metadata")
    double ExteriorGroundHeightMeters = 47.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Metadata")
    int32 ExteriorGroundAcceptedSampleCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Istana|Metadata")
    FString ExteriorGroundHeightSource = TEXT("PUBLIC_SRTM30_APPROXIMATE_FALLBACK_NOT_SURVEY_GRADE");

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    /** Hard cook dependency for AirSim's runtime string-loaded weather MPC. */
    UPROPERTY()
    TObjectPtr<UMaterialParameterCollection> WeatherGlobalParamsCookReference;

    void ApplyClearVisualWeatherPolicy();
    void ApplyClearExponentialHeightFogPolicy();
    void EnforceClearExponentialHeightFog();
    void ApplyVisualAcceptanceStreamingPolicy();
    void ApplyRuntimeCameraQualityProfile(ACameraActor* RuntimeCamera);
    void EnforceRuntimeCameraForPlayer0();
    void EvaluateStreamedPrimaryVisual();
    void RefreshUnreadyTilesets();

    FTimerHandle TilesetRefreshTimerHandle;
    FTimerHandle RuntimeCameraEnforcementTimerHandle;
    FTimerHandle ExponentialFogSuppressionTimerHandle;
    FTimerHandle StreamedPrimaryEvaluationTimerHandle;
    int32 TilesetRefreshAttempts = 0;
    int32 RuntimeCameraEnforcementAttempts = 0;
    int32 ExponentialFogSuppressionAttempts = 0;
};
