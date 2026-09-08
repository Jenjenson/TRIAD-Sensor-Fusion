#pragma once

#include "CoreMinimal.h"
#include "TRIADSensorFusionTypes.generated.h"

/** Singapore-relevant visual/RF environment presets. No engagement behavior is represented. */
UENUM(BlueprintType)
enum class ETRIADSingaporeWeatherProfile : uint8
{
    Clear,
    LightRain,
    Monsoon,
    Haze,
    Custom
};

/**
 * Weather scenario inputs shared by AirSim visual weather and the RF link budget.
 * Built-in presets are resolved by the scenario manager. Custom values are used only
 * when Profile is Custom. RF attenuation values are explicit scenario parameters,
 * not claims of a complete ITU propagation implementation.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADSingaporeWeatherSettings
{
    GENERATED_BODY()

    FTRIADSingaporeWeatherSettings()
    {
        CycleProfiles.Add(ETRIADSingaporeWeatherProfile::Clear);
        CycleProfiles.Add(ETRIADSingaporeWeatherProfile::LightRain);
        CycleProfiles.Add(ETRIADSingaporeWeatherProfile::Monsoon);
        CycleProfiles.Add(ETRIADSingaporeWeatherProfile::Haze);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather")
    ETRIADSingaporeWeatherProfile Profile = ETRIADSingaporeWeatherProfile::Clear;

    /** Apply the profile to AirSimTriadRuntime weather assets. Metadata/RF hooks remain active when false. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather")
    bool bApplyAirSimVisualWeather = true;

    /** Optional repeatable weather test sweep. Disabled by default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Test Sweep")
    bool bCycleProfiles = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Test Sweep", meta = (ClampMin = "1.0"))
    float CycleIntervalSeconds = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Test Sweep")
    TArray<ETRIADSingaporeWeatherProfile> CycleProfiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CustomRainAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CustomRoadWetnessAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CustomFogAmount = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CustomDustAmount = 0.0f;

    /** AirSim weather wind vector; each component is clamped to [-1, 1]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom")
    FVector CustomWind = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "0.0"))
    double CustomRainRateMillimetersPerHour = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "1.0"))
    double CustomVisibilityMeters = 30000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "0.0"))
    double CustomRFSpecificAttenuationDbPerKmAt2_4GHz = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather|Custom", meta = (ClampMin = "0.0"))
    double CustomRFSpecificAttenuationDbPerKmAt5_8GHz = 0.0;
};

/** A fixed, Cesium-anchored sensor-fusion site. Heights are WGS84 ellipsoid heights. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADGeodeticSensorNode
{
    GENERATED_BODY()

    FTRIADGeodeticSensorNode()
    {
        SupportedFrequenciesGHz.Add(2.4);
        SupportedFrequenciesGHz.Add(5.8);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    FString NodeId = TEXT("SensorNode");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    double LongitudeDegrees = 103.8198;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    double LatitudeDegrees = 1.3521;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    double HeightMeters = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node", meta = (ClampMin = "0.0"))
    double DetectionRangeMeters = 15000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    TArray<double> SupportedFrequenciesGHz;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    double ReceiveAntennaGainDbi = 12.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    double ReceiverSensitivityDbm = -96.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node", meta = (ClampMin = "0.001"))
    double BandwidthMHz = 20.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    double NoiseFigureDb = 5.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    double SystemLossDb = 2.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    bool bEnabled = true;

    /** Diameter scale for the Engine basic-shape marker; visual only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node", meta = (ClampMin = "0.1"))
    float MarkerScale = 4.0f;

    /** Draw a persistent tangent-plane range ring, tall beacon, and state label in development/editor builds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Visualization")
    bool bVisualizeNode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Visualization", meta = (ClampMin = "10.0"))
    float VisualBeaconHeightMeters = 750.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Visualization", meta = (ClampMin = "1.0"))
    float VisualLabelWorldSizeMeters = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Visualization", meta = (ClampMin = "24", ClampMax = "256"))
    int32 VisualRangeRingSegments = 96;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera")
    bool bCaptureCameraFrames = false;

    /** Aim both optical captures at the nearest discovered target each RF sample. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera")
    bool bTrackNearestTarget = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera", meta = (ClampMin = "5.0", ClampMax = "170.0"))
    float CameraFieldOfViewDegrees = 90.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera", meta = (ClampMin = "16", ClampMax = "4096"))
    int32 CameraCaptureWidth = 640;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera", meta = (ClampMin = "16", ClampMax = "4096"))
    int32 CameraCaptureHeight = 360;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera", meta = (ClampMin = "0.1"))
    float CameraCaptureCadenceSeconds = 2.0f;

    /** Rotation in the globe anchor's local East-South-Up actor frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera")
    FRotator CameraRelativeRotation = FRotator::ZeroRotator;

    /** Far value used only to normalize the diagnostic 8-bit depth PNG.
     *  Capture also writes a uint32 little-endian millimetre sidecar; 0 and
     *  0xFFFFFFFF are reserved invalid/saturated values and the sidecar remains
     *  simulation SceneDepth rather than physical-camera depth.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Camera", meta = (ClampMin = "1.0"))
    float DepthNormalizationMaxMeters = 10000.0f;

    /** Geometry-only, explicitly simulated 360-degree search radar. It never depends on an RF emitter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar")
    bool bEnableSearchRadar = true;

    /** Analytic slant-range envelope. Runtime clamps this to at least 5 km when the radar is enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar", meta = (ClampMin = "5000.0"))
    double SearchRadarRangeMeters = 8000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar", meta = (ClampMin = "1.0", ClampMax = "180.0"))
    double SearchRadarElevationFieldOfRegardDegrees = 120.0;

    /** Minimum analytic confidence required to emit a search-radar detection. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    double SearchRadarDetectionThreshold = 0.18;

    /** Nominal one-sigma measurement errors used by the deterministic simulation noise model. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar", meta = (ClampMin = "0.0"))
    double SearchRadarRangeNoiseSigmaMeters = 2.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar", meta = (ClampMin = "0.0"))
    double SearchRadarBearingNoiseSigmaDegrees = 0.20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar", meta = (ClampMin = "0.0"))
    double SearchRadarElevationNoiseSigmaDegrees = 0.15;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Radar", meta = (ClampMin = "0.0"))
    double SearchRadarRadialVelocityNoiseSigmaMetersPerSecond = 0.35;

    /** Narrow visible-light PTZ used only after a search-radar cue. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|EO PTZ")
    bool bEnableEOPTZ = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|EO PTZ", meta = (ClampMin = "500.0"))
    double EOPTZConfirmationRangeMeters = 3000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|EO PTZ", meta = (ClampMin = "1.0", ClampMax = "45.0"))
    float EOPTZFieldOfViewDegrees = 4.0f;

    /** Explicitly synthetic false-colour thermal PTZ used only after a search-radar cue. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Thermal PTZ")
    bool bEnableThermalPTZ = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Thermal PTZ", meta = (ClampMin = "500.0"))
    double ThermalPTZConfirmationRangeMeters = 2500.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Thermal PTZ", meta = (ClampMin = "1.0", ClampMax = "45.0"))
    float ThermalPTZFieldOfViewDegrees = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|PTZ", meta = (ClampMin = "64", ClampMax = "4096"))
    int32 PTZCaptureWidth = 960;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|PTZ", meta = (ClampMin = "64", ClampMax = "4096"))
    int32 PTZCaptureHeight = 540;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|PTZ", meta = (ClampMin = "1.0"))
    float PTZSlewRateDegreesPerSecond = 120.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|PTZ", meta = (ClampMin = "0.0"))
    float PTZSettleSeconds = 0.35f;

    /** Minimum time between bounded latest-frame replacements for one node. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|PTZ", meta = (ClampMin = "0.1"))
    float PTZCaptureCadenceSeconds = 0.75f;

    /** ECollisionChannel numeric value used for fail-closed EO/thermal occlusion checks. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|PTZ")
    int32 PTZLineOfSightTraceChannel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|PTZ")
    bool bPTZTraceComplex = true;

    /** Draw at most this node's selected radar cue/confirmation as clearly labelled SIM DEBUG aids. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Visualization")
    bool bVisualizeLongRangeSensorCue = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node|Long Range Visualization", meta = (ClampMin = "0.05"))
    float LongRangeVisualizationSeconds = 0.75f;
};

/** One latest EO/thermal confirmation state exported by a radar-cued node. */
struct TRIADSENSORFUSION_API FTRIADPTZConfirmationResult
{
    FString TimestampUtc;
    FString NodeId;
    FString SensorId;
    FString SensorType;
    FString Modality;
    FString ContactId;
    FString TrackId;
    FString TargetActor;
    FString RadarCueSensorId;
    FString SlewState = TEXT("IDLE");
    FString FrameRelativePath;
    FString MetadataRelativePath;
    FString ThermalSemantics;
    FString WeatherProfile;
    FString WeatherConfidenceSemantics;
    double SimulationSeconds = 0.0;
    double CueAgeSeconds = 0.0;
    double RangeMeters = 0.0;
    double Confidence = 0.0;
    double WeatherVisibilityMeters = 0.0;
    double WeatherRainRateMillimetersPerHour = 0.0;
    double WeatherConfidenceFactor = 1.0;
    double AzimuthDegrees = 0.0;
    double ElevationDegrees = 0.0;
    double FieldOfViewDegrees = 0.0;
    double PixelExtentWidth = 0.0;
    double PixelExtentHeight = 0.0;
    double BoundingBoxMinimumX = 0.0;
    double BoundingBoxMinimumY = 0.0;
    double BoundingBoxMaximumX = 0.0;
    double BoundingBoxMaximumY = 0.0;
    int32 ImageWidthPixels = 0;
    int32 ImageHeightPixels = 0;
    bool bConfirmed = false;
    bool bSyntheticThermal = false;
    bool bLineOfSight = false;
    FString BlockingActor;
    FString OcclusionSemantics;
};

/** RF behavior attached to a target actor. This is a propagation-only simulation profile. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRFEmitterDefinition
{
    GENERATED_BODY()

    FTRIADRFEmitterDefinition()
    {
        CenterFrequenciesGHz.Add(2.4);
        CenterFrequenciesGHz.Add(5.8);
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    FString EmitterId = TEXT("DefaultDroneRF");

    /** Optional actor-name regex used when selecting a configured profile. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    FString TargetActorNameRegex;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    TArray<double> CenterFrequenciesGHz;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    double TransmitPowerDbm = 23.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    double TransmitAntennaGainDbi = 2.0;

    /**
     * Explicit scenario ground truth / simulated IFF used for detection-only alerts.
     * This value is authored by the scenario and is never inferred from RF energy.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    bool bHostileScenarioTruth = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    bool bEnabled = true;
};

/**
 * Deliberately simple WGS84 scenario perimeter used to describe inbound tracks.
 * Rectangle remains the backwards-compatible default. Circle uses a WGS84
 * geodesic distance from CenterLongitudeDegrees/CenterLatitudeDegrees.
 * Neither shape is a national, FIR, territorial, or restricted-airspace boundary.
 */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADSimulationPerimeter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    FString ReferenceName = TEXT("Singapore_Simulation_Perimeter");

    /** "Rectangle" (default/backwards compatible) or "Circle". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    FString Shape = TEXT("Rectangle");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    double MinimumLongitudeDegrees = 103.6200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    double MaximumLongitudeDegrees = 104.0200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    double MinimumLatitudeDegrees = 1.2200;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    double MaximumLatitudeDegrees = 1.4700;

    /** WGS84 circle center. Used only when Shape is "Circle". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    double CenterLongitudeDegrees = 103.84288055;

    /** WGS84 circle center. Used only when Shape is "Circle". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    double CenterLatitudeDegrees = 1.30709615;

    /** Geodesic radius in metres. Used only when Shape is "Circle". */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach", meta = (ClampMin = "0.01"))
    double RadiusMeters = 1000.0;

    /** Closing-rate deadband used to distinguish APPROACHING/DEPARTING from OUTSIDE. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach", meta = (ClampMin = "0.0"))
    double PhaseRateDeadbandMetersPerSecond = 0.25;
};

UENUM(BlueprintType)
enum class ETRIADDemoTrajectory : uint8
{
    Stationary,
    Circular,
    Linear
};

/** Optional synthetic target used to validate the full RF/LOS telemetry path. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADDemoTargetDefinition
{
    GENERATED_BODY()

    /** Number of actors expanded from this definition. The manager also enforces 64 total. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target|Formation", meta = (ClampMin = "1", ClampMax = "64"))
    int32 SpawnCount = 1;

    /** Maximum columns in the centered ENU formation grid. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target|Formation", meta = (ClampMin = "1", ClampMax = "64"))
    int32 FormationColumns = 4;

    /** Center-to-center east/north spacing in the formation grid. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target|Formation", meta = (ClampMin = "0.0"))
    double FormationSpacingMeters = 25.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    FString ActorName = TEXT("Drone1");

    /** Human-readable route/corridor identifier exported with live approach telemetry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target|Approach")
    FString IngressCorridorId;

    /** Marks this synthetic route as an authored inbound/through-flight scenario. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target|Approach")
    bool bInboundApproachScenario = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    double StartLongitudeDegrees = 103.8198;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    double StartLatitudeDegrees = 1.3521;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    double StartHeightMeters = 250.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    ETRIADDemoTrajectory Trajectory = ETRIADDemoTrajectory::Circular;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target", meta = (ClampMin = "0.0"))
    double CircularRadiusMeters = 500.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    double AngularSpeedDegreesPerSecond = 8.0;

    /** East, North, Up direction for a linear path. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    FVector LinearDirectionEnu = FVector(1.0, 0.0, 0.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target", meta = (ClampMin = "0.0"))
    double LinearDistanceMeters = 2000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target", meta = (ClampMin = "0.0"))
    double LinearSpeedMetersPerSecond = 20.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    bool bPingPongLinearPath = true;

    /** Optional /Game or plugin mesh object path. The default quadrotor/fallback mesh is retained if loading fails. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    FString VisualMeshPath;

    /** Authored simulation input for the analytic search-radar model; not a measured signature. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target|Radar", meta = (ClampMin = "0.0001"))
    double SimulatedRadarCrossSectionSquareMeters = 0.03;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    FTRIADRFEmitterDefinition RFEmitter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Demo Target")
    bool bEnabled = true;
};

/** Runtime-only operator viewer. It presents sensor-gated contacts but never contributes evidence. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADOperatorObserverSettings
{
    GENERATED_BODY()

    /** Show the observer automatically when Play begins. F8 toggles it without changing detection state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer", meta = (ClampMin = "320", ClampMax = "1920"))
    int32 CaptureWidth = 960;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer", meta = (ClampMin = "180", ClampMax = "1080"))
    int32 CaptureHeight = 540;

    /** Seconds between the inbound chase angle and the nearest-reporting-sensor angle. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer", meta = (ClampMin = "2.0", ClampMax = "30.0"))
    float CameraAngleSeconds = 6.0f;

    /** Seconds before a contact is removed if no current sensor observation refreshes it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float ContactFreshnessSeconds = 1.0f;

    /** Seconds before automatically moving focus to another currently detected swarm member. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer", meta = (ClampMin = "2.0", ClampMax = "60.0"))
    float ContactCycleSeconds = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer", meta = (ClampMin = "8", ClampMax = "512"))
    int32 MaximumTrailPoints = 160;
};

/** Top-level Project/Config/SingaporeSensorFusion.json schema. */
USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADSensorFusionScenarioConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario", meta = (ClampMin = "0.01"))
    float SampleCadenceSeconds = 0.5f;

    /** ICU-compatible regular expression matched against UObject actor names. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    FString TargetNameRegex = TEXT("(?i).*(drone1|drone|uav).*");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    FName TargetActorTag = TEXT("DroneTarget");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    bool bAlwaysIncludeDrone1 = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    bool bAttachDefaultEmitterToDiscoveredTargets = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    FTRIADRFEmitterDefinition DefaultEmitter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    TArray<FTRIADRFEmitterDefinition> RFEmitterProfiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    TArray<FTRIADGeodeticSensorNode> SensorNodes;

    /** Fallback analytic-radar simulation RCS for discovered non-demo drone actors. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Radar", meta = (ClampMin = "0.0001"))
    double DefaultSimulatedRadarCrossSectionSquareMeters = 0.03;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    bool bSpawnDemoTargets = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    TArray<FTRIADDemoTargetDefinition> DemoTargets;

    /** Transparent reference circle for approach/inside/departure labels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario|Approach")
    FTRIADSimulationPerimeter SimulationPerimeter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Weather")
    FTRIADSingaporeWeatherSettings Weather;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Operator Observer")
    FTRIADOperatorObserverSettings OperatorObserver;

    /** Draw detected RF links with distinct 2.4 GHz and 5.8 GHz colors. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Visualization")
    bool bVisualizeRFLinks = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Visualization", meta = (ClampMin = "0.05"))
    float RFLinkVisualizationSeconds = 0.75f;

    /** Per-node per-sample draw cap; RF calculation and telemetry remain uncapped. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Visualization", meta = (ClampMin = "1", ClampMax = "256"))
    int32 MaxRFLinksVisualizedPerNode = 32;

    /** Maintain one concise, stable on-screen RF status line per node. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Visualization")
    bool bShowNodeRFStatusOnScreen = true;

    /**
     * Legacy diagnostic/optical trace channel.  When dedicated RF propagation
     * is enabled, this Visibility result is still reported but never supplies
     * RF path loss or gates an RF detection.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    int32 LineOfSightTraceChannel = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    bool bTraceComplex = true;

    /** Legacy binary-LOS detection gate; ignored by the dedicated RF path. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario")
    bool bRequireLineOfSightForDetection = false;

    /** Legacy scalar NLOS loss; ignored by the dedicated RF path. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Scenario", meta = (ClampMin = "0.0"))
    double NonLineOfSightAdditionalLossDb = 30.0;

    /**
     * Opt in to the hash-bound CPU RF geometry and deterministic parametric
     * interaction model.  This remains false for backwards-compatible generic
     * scenarios; enabling it does not confer field or survey validation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    bool bUseDedicatedRFPropagation = false;

    /** Destroy the scenario manager if the dedicated resources cannot be bound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    bool bRequireDedicatedRFReady = false;

    /**
     * Explicit map-composition assertion. TightV1 consumes world coordinates
     * directly; OneKilometreV2 admits only its separately pinned geodetic and
     * EPSG:3414 transformation.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    bool bDedicatedRFFrameIsWorldOriginIdentity = false;

    /**
     * Exact long package name whose authored RF frame is asserted above.
     * PIE prefixes are normalized before comparison.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedWorldPackageName;

    /**
     * Only Plugin/... and Project/... resource specifications are accepted;
     * absolute paths and parent traversal are rejected.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFGeometryResourcePath =
        TEXT("Plugin/Resources/RF/IstanaPublicViewRFTightV1.geometry.json");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFMaterialCatalogResourcePath =
        TEXT("Plugin/Resources/RF/istana_rf_materials_tight_v1.catalog.json");

    /** Scene contract is staged and independently hash-verified at startup. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFSceneContractResourcePath =
        TEXT("Plugin/Resources/RF/istana_rf_scene_tight_v1.contract.json");

    /** Exact source-file hashes required before the transactional runtime load. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedGeometrySha256 =
        TEXT("a3705e22b47fbe4fd38936294890e1ad620bb3c618ac3c93ca6c4719c6edc62a");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedMaterialCatalogSha256 =
        TEXT("0089fed936494ecd6938dc14d34ea8d5540e0896c2dc814414e071b6d1081a63");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedSceneContractSha256 =
        TEXT("19002f898c34c23793038c42892f122642f112bd514a0ebb05e4969dcb9ae591");

    /** Stable semantic IDs checked after the loader has verified the catalog binding. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedGeometryQueryId = TEXT("istana-public-view-rf-tight-v1-main-hero-001@catalog-sha256:0089fed936494ecd6938dc14d34ea8d5540e0896c2dc814414e071b6d1081a63");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedMaterialCatalogId =
        TEXT("istana-public-view-rf-materials-tight-v1-001");

    /** Scenario-pinned domain identity; future geometry revisions may differ. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedModeledCoverageId =
        TEXT("RF_COVERAGE_ISTANA_MAIN_HERO_TIGHT_V1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedModeledCoverageScope =
        TEXT("MAIN_HERO_ONLY_ASSUMPTION_BOUND");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    bool bDedicatedRFExpectedCoverageCoversOneKilometreAoi = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    bool bDedicatedRFExpectedCoverageCoversSurroundings = false;

    /** Empty for TightV1; required when binding a closed geodesic-circle V2. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedStudyDomainId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedStudyDomainType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedStudyCenterLongitudeDegrees = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedStudyCenterLatitudeDegrees = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedStudyRadiusMeters = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    int32 DedicatedRFExpectedStudyPerimeterSampleCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedStudyPerimeterStartAzimuthDegrees = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedStudyPerimeterStepDegrees = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedStudySourceGeodeticCrs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedStudyProjectedConstructionCrs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedStudyLogicalSystem;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedProjectionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedProjectedOriginEastingMeters = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedProjectedOriginNorthingMeters = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedGeometryAxisPolicy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    FString DedicatedRFExpectedGeometryVerticalPolicy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedGeoreferenceOriginHeightMeters = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    double DedicatedRFExpectedGeoreferenceScaleCentimetersPerMeter = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    bool bDedicatedRFExpectedGeoreferenceCartographicOrigin = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Propagation")
    bool bDedicatedRFExpectedGeoreferenceActorTransformIdentity = false;

    /**
     * Legacy configuration name retained for JSON/Blueprint compatibility.
     * Enables detection-only preliminary RF early-warning cues for any emitter
     * corroborated by the configured node threshold; no hostility is inferred.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Alerts")
    bool bEnableThreatAlerts = true;

    /** An emitter must be RF-detected by this many distinct nodes in one sample. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Alerts", meta = (ClampMin = "1"))
    int32 AlertMinimumConfirmingNodes = 2;

    /** Per-target interval between repeated preliminary RF cues. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Alerts", meta = (ClampMin = "0.0"))
    float AlertCooldownSeconds = 3.0f;

    /** Hard per-session preliminary-cue record bound. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Alerts", meta = (ClampMin = "1"))
    int32 MaxAlertRecords = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry")
    bool bWriteJsonl = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry")
    bool bWriteCsv = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry", meta = (ClampMin = "1"))
    int32 MaxTelemetryRecords = 200000;

    /**
     * Atomically replace one bounded current-sample JSON document for local dashboards.
     * This output is independent of the capped session JSONL/CSV streams.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry|Live Snapshot")
    bool bWriteLatestRFSnapshot = true;

    /** Strongest currently detected RF links retained in latest_rf_snapshot.json. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry|Live Snapshot", meta = (ClampMin = "1", ClampMax = "4096"))
    int32 MaxLiveSnapshotDetectedLinks = 512;

    /** Bounded current search-radar detections retained in the v3 live snapshot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry|Live Snapshot", meta = (ClampMin = "1", ClampMax = "4096"))
    int32 MaxLiveSnapshotSearchRadarDetections = 512;

    /** Bounded radar-cued EO/thermal status/confirmation records retained in the v3 live snapshot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry|Live Snapshot", meta = (ClampMin = "1", ClampMax = "256"))
    int32 MaxLiveSnapshotPTZConfirmations = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry", meta = (ClampMin = "1024"))
    int64 MaxTelemetryFileBytes = 67108864;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry", meta = (ClampMin = "1"))
    int32 MaxFramesPerNode = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Telemetry", meta = (ClampMin = "1024"))
    int64 MaxFrameBytesPerNode = 536870912;
};
