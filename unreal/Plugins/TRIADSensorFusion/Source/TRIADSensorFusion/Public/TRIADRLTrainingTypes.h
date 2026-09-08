#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TRIADSensorFusionTypes.h"
#include "TRIADRLTrainingTypes.generated.h"

UENUM(BlueprintType)
enum class ETRIADRLPhase : uint8
{
    Inactive,
    BluePlacement,
    RedDeployment,
    RedMovement,
    Terminal
};

UENUM(BlueprintType)
enum class ETRIADRLTerminationReason : uint8
{
    None,
    ProtectedZoneReached,
    AllTargetsConfirmed,
    HorizonReached,
    ConstraintViolation,
    InvalidAction
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLProtectedZoneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Protected Zone")
    double LongitudeDegrees = 103.84288055;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Protected Zone")
    double LatitudeDegrees = 1.30709615;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Protected Zone")
    double HeightMeters = 47.0;

    /** Abstract geofence only. Building contact, damage, and payload delivery are outside this model. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Protected Zone", meta = (ClampMin = "10.0"))
    double RadiusMeters = 150.0;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLRewardWeights
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double BlueDetection = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double BlueEarlyDetection = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double BlueZoneMiss = -10.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double BlueSiteCost = -0.05;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double RedProgress = 0.1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double RedDetected = -1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double RedZoneReached = 10.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Reward")
    double RedConstraintViolation = -10.0;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLTrainingConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL")
    FString SchemaVersion = TEXT("triad.rl_training.v1");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL")
    bool bEnabled = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL")
    int32 RandomSeed = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL", meta = (ClampMin = "0.02", ClampMax = "2.0"))
    double FixedStepSeconds = 0.25;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL", meta = (ClampMin = "1", ClampMax = "100000"))
    int32 EpisodeHorizonSteps = 600;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL", meta = (ClampMin = "1", ClampMax = "32"))
    int32 MaximumSensorSites = 6;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL", meta = (ClampMin = "0.0"))
    double SensorBudgetUnits = 12.0;

    /** Continuous Blue placement disk centred on the map-authored objective. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Blue", meta = (ClampMin = "50.0"))
    double BluePlacementRadiusMeters = 1000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Blue", meta = (ClampMin = "0.0"))
    double BlueMinimumObjectiveStandoffMeters = 100.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Blue", meta = (ClampMin = "0.0"))
    double MinimumSensorSeparationMeters = 25.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Blue", meta = (ClampMin = "0.0"))
    double SensorCostUnits = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Blue")
    FTRIADGeodeticSensorNode BlueSensorTemplate;

    /** Continuous Red deployment annulus. It may extend beyond visible authored geometry but remains bounded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "10.0"))
    double RedMinimumSpawnRadiusMeters = 1200.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "10.0"))
    double RedMaximumSpawnRadiusMeters = 2000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "5.0"))
    double RedMinimumAltitudeMeters = 60.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "5.0"))
    double RedMaximumAltitudeMeters = 180.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "1", ClampMax = "64"))
    int32 RedMinimumSwarmSize = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "1", ClampMax = "64"))
    int32 RedMaximumSwarmSize = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "1.0", ClampMax = "100.0"))
    double RedMinimumFormationSpacingMeters = 10.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red", meta = (ClampMin = "1.0", ClampMax = "100.0"))
    double RedMaximumFormationSpacingMeters = 40.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL", meta = (ClampMin = "0.1", ClampMax = "50.0"))
    double MaximumTargetSpeedMetersPerSecond = 15.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL", meta = (ClampMin = "100.0"))
    double MaximumDistanceFromZoneMeters = 2000.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL", meta = (ClampMin = "1", ClampMax = "32"))
    int32 ConfirmationNodeCount = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL")
    FTRIADRLProtectedZoneDefinition ProtectedZone;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL")
    FTRIADRLRewardWeights Rewards;
};

UCLASS(BlueprintType)
class TRIADSENSORFUSION_API UTRIADRLTrainingDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    FTRIADRLTrainingConfig Config;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLBlueAction
{
    GENERATED_BODY()

    /** Continuous local EN coordinates in the unit disk around the objective. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Blue")
    FVector2D NormalizedPosition = FVector2D::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Blue")
    bool bStopPlacement = false;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLRedDeploymentAction
{
    GENERATED_BODY()

    /** All values are normalized to [-1, 1] and mapped into the configured bounded domain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red")
    double NormalizedBearing = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red")
    double NormalizedRadius = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red")
    double NormalizedAltitude = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red")
    double NormalizedSwarmSize = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red")
    double NormalizedFormationSpacing = 0.0;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLRedAction
{
    GENERATED_BODY()

    /** Each component is normalized to [-1, 1] and is never a motor or vehicle command. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RL|Red")
    FVector NormalizedVelocityEnu = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLTargetObservation
{
    GENERATED_BODY()

    /** Target displacement from the protected-zone centre, normalized by the configured AOI radius. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL|Observation")
    FVector NormalizedZoneRelativeEnu = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL|Observation")
    FVector NormalizedVelocityEnu = FVector::ZeroVector;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL|Observation")
    bool bConfirmedDetected = false;
};

USTRUCT(BlueprintType)
struct TRIADSENSORFUSION_API FTRIADRLStepResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    ETRIADRLPhase Phase = ETRIADRLPhase::Inactive;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    ETRIADRLTerminationReason TerminationReason = ETRIADRLTerminationReason::None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    int32 StepIndex = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    int32 ActiveTargetCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    int32 DetectedTargetCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    double MinimumZoneDistanceMeters = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    double BlueReward = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    double RedReward = 0.0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    bool bTerminal = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    TArray<bool> BlueActionMask;

    /** Committed Blue sites in objective-relative EN coordinates normalized by BluePlacementRadiusMeters. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    TArray<FVector2D> SensorObservations;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    TArray<FTRIADRLTargetObservation> TargetObservations;
};
