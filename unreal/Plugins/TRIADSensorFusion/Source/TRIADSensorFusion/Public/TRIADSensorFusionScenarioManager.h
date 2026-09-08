#pragma once

#include "GameFramework/Actor.h"
#include "TRIADRFIndexedGeometryQuery.h"
#include "TRIADRFInteractionModel.h"
#include "TRIADSensorFusionTypes.h"
#include <limits>
#include "TRIADSensorFusionScenarioManager.generated.h"

class ACesiumGeoreference;
class ATRIADDemoDroneActor;
class ATRIADOperatorObserverActor;
class ATRIADSensorNodeActor;
class UTRIADRFEmitterComponent;

/** Owns one runtime-only Singapore sensor-fusion simulation session. */
UCLASS(BlueprintType, Blueprintable, NotPlaceable)
class TRIADSENSORFUSION_API ATRIADSensorFusionScenarioManager : public AActor
{
    GENERATED_BODY()

public:
    ATRIADSensorFusionScenarioManager();

    static FString GetScenarioConfigPath();
    static bool LoadScenarioConfig(FTRIADSensorFusionScenarioConfig& OutConfig, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Sensor Fusion")
    void SampleScenarioNow();

    /** Apply a Singapore weather profile immediately; suitable for repeatable detection-only tests. */
    UFUNCTION(BlueprintCallable, Category = "TRIAD|Sensor Fusion|Weather")
    void SetWeatherProfile(ETRIADSingaporeWeatherProfile NewProfile);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Sensor Fusion")
    FTRIADSensorFusionScenarioConfig ScenarioConfig;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    ACesiumGeoreference* FindGeoreference() const;
    void SpawnSensorNodes();
    void SpawnDemoTargets();
    void SpawnOperatorObserver();
    void UpdateTargetApproachTelemetry(const TArray<AActor*>& Targets);
    double ComputeSignedDistanceToSimulationPerimeterMeters(double LongitudeDegrees, double LatitudeDegrees) const;
    void ApplyActiveWeatherProfile();
    void CycleWeatherProfile();
    double GetWeatherSpecificAttenuationDbPerKm(double FrequencyGHz) const;
    bool InitializeDedicatedRFPropagation(FString& OutError);
    bool ResolveDedicatedRFResourcePath(
        const FString& ResourceSpecification,
        FString& OutResolvedPath,
        FString& OutError) const;
    void DiscoverTargets(TArray<AActor*>& OutTargets) const;
    bool IsConfiguredTarget(const AActor* Actor) const;
    UTRIADRFEmitterComponent* FindOrAttachEmitter(AActor* Target);
    const FTRIADRFEmitterDefinition& SelectEmitterDefinition(const AActor* Target) const;
    bool SampleNodeTargetLink(
        ATRIADSensorNodeActor* Node,
        AActor* Target,
        UTRIADRFEmitterComponent* Emitter,
        double& OutMaxDetectedSnrDb,
        double& OutSlantDistanceMeters);
    bool SampleNodeTargetSearchRadar(
        ATRIADSensorNodeActor* Node,
        AActor* Target,
        const FVector& TargetVelocityWorldCentimetersPerSecond,
        FString& OutTrackId,
        double& OutMeasuredRangeMeters,
        double& OutMeasuredBearingDegrees,
        double& OutMeasuredElevationDegrees,
        double& OutConfidence);
    bool ComputeLineOfSight(const ATRIADSensorNodeActor* Node, const AActor* Target, FString& OutBlockingActor) const;
    struct FRFPropagationSample
    {
        bool bPathValid = false;
        bool bDedicated = false;
        bool bDegraded = false;
        FTRIADRFPathEvaluation PathEvaluation;
        FString PropagationMode;
        FString Readiness;
        FString FailureReason;
        bool bAoiAdmissionRequired = false;
        bool bAoiEndpointsAdmitted = false;
        FString AoiAdmissionDomainId;
        double TransmitterAoiSignedDistanceMeters = std::numeric_limits<double>::quiet_NaN();
        double ReceiverAoiSignedDistanceMeters = std::numeric_limits<double>::quiet_NaN();
        FVector GeometryTransmitterCentimeters = FVector::ZeroVector;
        FVector GeometryReceiverCentimeters = FVector::ZeroVector;
        bool bGeometryFrameTransformValid = false;
    };
    bool EvaluateDedicatedRFPath(
        const FVector& TransmitterWorldCentimeters,
        const FVector& ReceiverWorldCentimeters,
        double FrequencyGHz,
        FRFPropagationSample& OutSample) const;
    void AddRFPropagationTelemetryFields(
        const TSharedRef<class FJsonObject>& Json,
        const FRFPropagationSample& Propagation,
        double SystemLossDb,
        double WeatherLossDb) const;
    TSharedRef<class FJsonObject> MakeRFPropagationStatusJson() const;
    void EmitRFEarlyWarningCue(
        AActor* Target,
        const TMap<FString, double>& ConfirmingNodeDistancesMeters,
        double MaxSnrDb);
    void WriteLatestRFSnapshot();
    void AppendAlertRecord(const TSharedRef<class FJsonObject>& AlertRecord);
    void InitializeTelemetry();
    void AppendTelemetryRecord(const TSharedRef<class FJsonObject>& JsonRecord, const FString& CsvRecord);
    void FlushTelemetryBuffers();
    void DisableTelemetryFormat(bool bJsonl, const FString& Reason);

    UPROPERTY(Transient)
    TObjectPtr<ACesiumGeoreference> Georeference;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATRIADSensorNodeActor>> SpawnedSensorNodes;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATRIADDemoDroneActor>> SpawnedDemoTargets;

    UPROPERTY(Transient)
    TObjectPtr<ATRIADOperatorObserverActor> OperatorObserver;

    FTimerHandle SampleTimerHandle;
    FTimerHandle WeatherCycleTimerHandle;
    int32 WeatherCycleIndex = 0;
    FString ActiveWeatherProfileName = TEXT("Clear");
    float ActiveWeatherRainAmount = 0.0f;
    float ActiveWeatherRoadWetnessAmount = 0.0f;
    float ActiveWeatherFogAmount = 0.0f;
    float ActiveWeatherDustAmount = 0.0f;
    FVector ActiveWeatherWind = FVector::ZeroVector;
    double ActiveWeatherRainRateMillimetersPerHour = 0.0;
    double ActiveWeatherVisibilityMeters = 30000.0;
    double ActiveWeatherRFSpecificAttenuationDbPerKmAt2_4GHz = 0.0;
    double ActiveWeatherRFSpecificAttenuationDbPerKmAt5_8GHz = 0.0;
    bool bAirSimVisualWeatherApplied = false;
    bool bAirSimWeatherInitialized = false;
    bool bAirSimWeatherActorsVerified = false;
    TUniquePtr<FTRIADRFIndexedGeometryQuery> DedicatedRFGeometryQuery;
    TUniquePtr<FTRIADDeterministicRFInteractionModel> DedicatedRFInteractionModel;
    FTRIADRFIndexedGeometryMetadata DedicatedRFMetadata;
    FString ActiveRFPropagationMode = TEXT("LEGACY_VISIBILITY_BINARY_NLOS");
    FString DedicatedRFReadiness = TEXT("DEDICATED_RF_DISABLED_LEGACY_MODE");
    FString DedicatedRFFailureReason;
    FString DedicatedRFGeometrySha256;
    FString DedicatedRFMaterialCatalogSha256;
    FString DedicatedRFSceneContractSha256;
    bool bDedicatedRFReady = false;
    bool bDedicatedRFDegraded = false;
    FString TelemetryDirectory;
    FString JsonlTelemetryPath;
    FString CsvTelemetryPath;
    FString AlertJsonlPath;
    FString LatestRFSnapshotPath;
    int64 JsonlBytesWritten = 0;
    int64 CsvBytesWritten = 0;
    int64 AlertBytesWritten = 0;
    int32 TelemetryRecordsWritten = 0;
    int32 AlertRecordsWritten = 0;
    bool bJsonlActive = false;
    bool bCsvActive = false;
    bool bAlertsActive = false;
    bool bLiveSnapshotWriteFailureLogged = false;
    FString PendingJsonlBuffer;
    FString PendingCsvBuffer;
    FString PendingAlertBuffer;
    TArray<TSharedPtr<class FJsonObject>> CurrentDetectedRFSnapshotLinks;
    TArray<TSharedPtr<class FJsonObject>> CurrentSearchRadarDetections;
    TMap<FString, double> LastAlertSimulationSecondsByTarget;

    struct FTargetApproachStatus
    {
        FString TargetActor;
        FString IngressCorridorId;
        FString AirspaceState = TEXT("NOT_EVALUATED");
        double LongitudeDegrees = 0.0;
        double LatitudeDegrees = 0.0;
        double HeightMeters = 0.0;
        double DistanceToPerimeterMeters = 0.0;
        double ApproachRateMetersPerSecond = 0.0;
        double HeadingDegrees = 0.0;
        double SpeedMetersPerSecond = 0.0;
        bool bOutsideSimulationPerimeter = false;
        bool bInboundApproachScenario = false;
        bool bKinematicsAvailable = false;
        bool bHostileScenarioTruth = false;
    };

    struct FPreviousTargetApproachSample
    {
        double LongitudeDegrees = 0.0;
        double LatitudeDegrees = 0.0;
        double SignedDistanceToPerimeterMeters = 0.0;
        double SimulationSeconds = 0.0;
    };

    TMap<FString, FTargetApproachStatus> CurrentTargetApproachStatusByActor;
    TMap<FString, FPreviousTargetApproachSample> PreviousTargetApproachSampleByActor;

    struct FPreviousRadarTargetSample
    {
        FVector WorldLocation = FVector::ZeroVector;
        double SimulationSeconds = 0.0;
    };

    TMap<FString, FPreviousRadarTargetSample> PreviousRadarTargetSampleByActor;
};
