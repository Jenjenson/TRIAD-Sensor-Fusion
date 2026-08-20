#pragma once

#include "GameFramework/Actor.h"
#include "TRIADSensorFusionTypes.h"
#include "TRIADSensorNodeActor.generated.h"

class ACesiumGeoreference;
class UCesiumGlobeAnchorComponent;
class USceneComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;
class UTextureRenderTarget2D;
class UTRIADSensorNodeComponent;

/** Visible Cesium-anchored site containing the RF receiver and RGB/depth cameras. */
UCLASS(BlueprintType, Blueprintable)
class TRIADSENSORFUSION_API ATRIADSensorNodeActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADSensorNodeActor();

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node")
    TObjectPtr<UStaticMeshComponent> MarkerMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node")
    TObjectPtr<UTextRenderComponent> NodeLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node")
    TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node")
    TObjectPtr<UTRIADSensorNodeComponent> SensorNode;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node|Camera")
    TObjectPtr<USceneCaptureComponent2D> RGBCapture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node|Camera")
    TObjectPtr<USceneCaptureComponent2D> DepthCapture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node|Long Range PTZ")
    TObjectPtr<USceneCaptureComponent2D> EOPTZCapture;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Node|Long Range PTZ")
    TObjectPtr<USceneCaptureComponent2D> ThermalPTZCapture;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Sensor Node")
    void ConfigureNode(
        const FTRIADGeodeticSensorNode& InDefinition,
        ACesiumGeoreference* InGeoreference,
        const FString& InFramesRootDirectory,
        int32 InMaxFrames,
        int64 InMaxFrameBytes);

    /** Point the RGB and depth capture boresights at a world-space target. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Sensor Node|Camera")
    void AimCamerasAtWorldLocation(const FVector& TargetWorldLocation);

    /** Accept one geometry-only search-radar cue and drive EO/thermal slew/settle/capture state. */
    void CueLongRangeSensors(
        AActor* Target,
        const FString& ContactId,
        const FString& TrackId,
        double MeasuredRangeMeters,
        double MeasuredBearingDegrees,
        double MeasuredElevationDegrees,
        double RadarConfidence,
        double SimulationSeconds);

    /** No current radar detection at this node; positive confirmation evidence is cleared. */
    void ClearLongRangeCue();

    void GetLatestPTZConfirmations(TArray<FTRIADPTZConfirmationResult>& OutConfirmations) const;

    /**
     * Expose the live EO render target only while this node has a fresh, confirmed
     * line-of-sight capture for the requested radar track. This prevents the
     * operator viewer from presenting a stale prior frame as current evidence.
     */
    bool TryGetFreshOperatorEOView(
        const FString& TrackId,
        double MaximumCueAgeSeconds,
        UTextureRenderTarget2D*& OutRenderTarget,
        FTRIADPTZConfirmationResult& OutConfirmation) const;

    /** Human-only search-radar cue aid; line batchers remain excluded from sensor captures. */
    void RecordSearchRadarDetection(
        const FVector& TargetWorldLocation,
        const FString& TrackId,
        double RangeMeters,
        double Confidence,
        float VisualizationSeconds);

    /** Reset the node's visible RF counters/state at the start of one scenario sample. */
    void BeginRFVisualizationSample();

    /** Draw and account for one detected, band-specific RF link. */
    void RecordRFDetection(
        const FVector& TargetWorldLocation,
        double FrequencyGHz,
        bool bPreliminaryCueEvidence,
        double SnrDb,
        float VisualizationSeconds,
        bool bDrawLink,
        int32 MaxLinksToDraw);

    /** Promote a single-node RF candidate to a corroborated multi-node cue. Detection-only. */
    void MarkRFMultinodePreliminaryCue();

    /** Refresh the persistent node label and optional stable on-screen status line. */
    void FinalizeRFVisualizationSample(bool bShowOnScreenStatus, float StatusSeconds);

    /** Stamp subsequent RGB/depth frame metadata with the active environment. */
    void SetWeatherMetadata(
        const FString& ProfileName,
        bool bVisualWeatherApplied,
        double RainRateMillimetersPerHour,
        double VisibilityMeters);

    const FTRIADGeodeticSensorNode& GetNodeDefinition() const;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    enum class EVisualDetectionState : uint8
    {
        Idle,
        RFCandidate,
        RFMultinodeCue
    };

    enum class EPTZSlewState : uint8
    {
        Idle,
        Slewing,
        Settling,
        Settled
    };

    FColor GetStateColor() const;
    FString GetStateText() const;
    void ApplyVisualState();
    void UpdateWorldLabel();
    void DrawPersistentNodeVisualization() const;
    void InitializeCaptureTargets();
    void InitializeLongRangeCaptureTargets();
    /**
     * Exclude TRIAD's operator-only labels, site markers, and debug line batchers
     * from both model-facing capture passes. Returns true only when the filter
     * has been installed on both captures for the current world.
     */
    bool RefreshCaptureExclusionList();
    void CaptureCameraFrame();
    void UpdateLongRangePTZ(float DeltaSeconds);
    void CaptureLongRangeConfirmation();
    bool ProjectTargetIntoCapture(
        const USceneCaptureComponent2D* Capture,
        const AActor* Target,
        int32 Width,
        int32 Height,
        double& OutMinimumX,
        double& OutMinimumY,
        double& OutMaximumX,
        double& OutMaximumY) const;
    bool ComputePTZLineOfSight(
        const USceneCaptureComponent2D* Capture,
        const AActor* Target,
        FString& OutBlockingActor) const;
    bool PublishLongRangeFrame(
        const FString& ModalityDirectoryName,
        const TArray<FColor>& Pixels,
        const FTRIADPTZConfirmationResult& Result,
        bool bSyntheticThermal,
        FString& OutFrameRelativePath,
        FString& OutMetadataRelativePath) const;
    void StopFrameCapture(const FString& Reason);

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> RGBRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> DepthRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> EOPTZRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> ThermalPTZRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;

    FString FrameDirectory;
    FString LongRangeTelemetryRootDirectory;
    FTimerHandle CaptureTimerHandle;
    int32 CapturedFrameCount = 0;
    int32 MaximumFrameCount = 0;
    int64 CapturedFrameBytes = 0;
    int64 MaximumFrameBytes = 0;
    bool bCaptureLimitReported = false;
    bool bDebugVisualCaptureFilterActive = false;
    TWeakObjectPtr<AActor> CurrentRadarCueTarget;
    FString CurrentSensorContactId;
    FString CurrentRadarTrackId;
    FRotator DesiredPTZWorldRotation = FRotator::ZeroRotator;
    EPTZSlewState PTZSlewState = EPTZSlewState::Idle;
    /** Acquisition time for slew/settle sequencing; does not represent cue freshness. */
    double RadarCueStartSimulationSeconds = 0.0;
    /** Updated on every accepted radar sample so exported cueAgeSeconds stays fresh. */
    double LastRadarCueSimulationSeconds = 0.0;
    double PTZSettleStartSimulationSeconds = 0.0;
    double LastPTZCaptureSimulationSeconds = -TNumericLimits<double>::Max();
    double CurrentRadarMeasuredRangeMeters = 0.0;
    double CurrentRadarMeasuredBearingDegrees = 0.0;
    double CurrentRadarMeasuredElevationDegrees = 0.0;
    double CurrentRadarConfidence = 0.0;
    TArray<FTRIADPTZConfirmationResult> LatestPTZConfirmations;
    EVisualDetectionState VisualDetectionState = EVisualDetectionState::Idle;
    int32 SampleRFDetectionCount = 0;
    int32 SamplePreliminaryCueEvidenceCount = 0;
    int32 SampleRFLinksDrawn = 0;
    int32 SampleSearchRadarDetectionCount = 0;
    double SampleMaximumSnrDb = -TNumericLimits<double>::Max();
    TArray<double> SampleDetectedFrequenciesGHz;
    FString ActiveWeatherProfileName = TEXT("Clear");
    bool bActiveVisualWeatherApplied = false;
    double ActiveRainRateMillimetersPerHour = 0.0;
    double ActiveVisibilityMeters = 30000.0;
};
