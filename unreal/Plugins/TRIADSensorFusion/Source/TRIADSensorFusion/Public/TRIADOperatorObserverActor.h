#pragma once

#include "GameFramework/Actor.h"
#include "TRIADSensorFusionTypes.h"
#include "TRIADOperatorObserverActor.generated.h"

class ACesiumGeoreference;
class ATRIADSensorNodeActor;
class USceneCaptureComponent2D;
class USceneComponent;
class UTextureRenderTarget2D;
class STRIADOperatorSlateWidget;

/** One current-sample contact that passed at least one simulated sensor gate. */
struct TRIADSENSORFUSION_API FTRIADOperatorObservedContact
{
    TWeakObjectPtr<AActor> TargetActor;
    TWeakObjectPtr<ATRIADSensorNodeActor> ReportingNode;
    FString ContactId;
    FString RadarTrackId;
    FString TargetActorName;
    FString ReportingNodeId;
    FString SensorSummary;
    FString IngressCorridorId;
    FString AirspaceState;
    FString WeatherProfile;
    double ObservationSimulationSeconds = 0.0;
    double ReportedRangeMeters = 0.0;
    double RadarConfidence = 0.0;
    double MaximumRFSnrDb = -TNumericLimits<double>::Max();
    double EstimatedLongitudeDegrees = 0.0;
    double EstimatedLatitudeDegrees = 0.0;
    double EstimatedHeightMeters = 0.0;
    double DistanceToPerimeterMeters = 0.0;
    double ApproachRateMetersPerSecond = 0.0;
    double HeadingDegrees = 0.0;
    double SpeedMetersPerSecond = 0.0;
    bool bDetectedBySearchRadar = false;
    bool bDetectedByRF = false;
    bool bHasRadarPositionEstimate = false;
    bool bOutsideSimulationPerimeter = false;
    bool bInboundApproachScenario = false;
    bool bAuthoredAttackScenario = false;
};

/**
 * Presentation-only camera and tactical overlay for current sensor contacts.
 * It never contributes a detection and deliberately labels its chase camera as
 * simulation truth rather than sensor evidence.
 */
UCLASS(NotPlaceable)
class TRIADSENSORFUSION_API ATRIADOperatorObserverActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADOperatorObserverActor();

    virtual void Tick(float DeltaSeconds) override;

    void InitializeObserver(
        const FTRIADOperatorObserverSettings& InSettings,
        const FTRIADSimulationPerimeter& InPerimeter,
        ACesiumGeoreference* InGeoreference,
        const TArray<TObjectPtr<ATRIADSensorNodeActor>>& InSensorNodes);

    /** Replace the current sample atomically. An empty array fails the display closed. */
    void UpdateDetectedContacts(
        const TArray<FTRIADOperatorObservedContact>& InContacts,
        double SampleSimulationSeconds);

    bool HasFreshSelectedContact() const;
    const FTRIADOperatorObservedContact* GetSelectedContact() const;
    const TArray<FTRIADOperatorObservedContact>& GetCurrentContacts() const { return CurrentContacts; }
    const TArray<FTRIADGeodeticSensorNode>& GetMinimapNodes() const { return MinimapNodes; }
    const TArray<FVector2D>& GetSelectedRadarTrail() const { return SelectedRadarTrail; }
    const FTRIADSimulationPerimeter& GetSimulationPerimeter() const { return SimulationPerimeter; }
    UTextureRenderTarget2D* GetObserverRenderTarget() const { return ObserverRenderTarget; }
    bool TryGetFreshSensorView(
        UTextureRenderTarget2D*& OutRenderTarget,
        FTRIADPTZConfirmationResult& OutConfirmation) const;
    FString GetCameraModeLabel() const;
    bool IsOverlayVisible() const { return bOverlayVisible; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void EnsureOverlay();
    void RemoveOverlay();
    void SelectBestOrRetainedContact(bool bForceAdvance);
    void UpdateObserverCamera();
    void DrawOperatorWorldAids() const;
    int32 ScoreContact(const FTRIADOperatorObservedContact& Contact) const;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Operator Observer")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, Category = "TRIAD|Operator Observer")
    TObjectPtr<USceneCaptureComponent2D> ObserverCapture;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> ObserverRenderTarget;

    UPROPERTY(Transient)
    TObjectPtr<ACesiumGeoreference> Georeference;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATRIADSensorNodeActor>> SensorNodes;

    FTRIADOperatorObserverSettings Settings;
    FTRIADSimulationPerimeter SimulationPerimeter;
    TArray<FTRIADGeodeticSensorNode> MinimapNodes;
    TArray<FTRIADOperatorObservedContact> CurrentContacts;
    TArray<FVector2D> SelectedRadarTrail;
    FString SelectedContactId;
    double LastSampleSimulationSeconds = -TNumericLimits<double>::Max();
    double LastContactSelectionSimulationSeconds = -TNumericLimits<double>::Max();
    int32 SelectedContactIndex = INDEX_NONE;
    bool bOverlayVisible = true;
    bool bInitialized = false;
    bool bShowingSensorAngle = false;
    bool bValidationScreenshotRequested = false;
    bool bScreenMessageStateCaptured = false;
    bool bPreviousOnScreenDebugMessagesEnabled = true;
    bool bPreviousOnScreenDebugMessagesDisplay = true;
    TSharedPtr<STRIADOperatorSlateWidget> OverlayWidget;
};
