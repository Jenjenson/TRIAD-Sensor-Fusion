#include "TRIADAdversarialTrainingManager.h"

#include "CesiumGeoreference.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "JsonObjectConverter.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TRIADDemoDroneActor.h"
#include "TRIADLongRangeSensorModel.h"
#include "TRIADProtectedZoneComponent.h"
#include "TRIADRFEmitterComponent.h"
#include "TRIADRLTrainingModel.h"
#include "TRIADSensorNodeActor.h"
#include "TRIADSensorNodeComponent.h"
#include "TRIADSwarmControllerComponent.h"

namespace
{
constexpr int32 MaximumRLTargetActors = 64;
const TCHAR* DefaultTrainingAssetPath = TEXT("/Game/TRIAD/RL/DA_TRIADRLDefault.DA_TRIADRLDefault");
}

ATRIADAdversarialTrainingManager::ATRIADAdversarialTrainingManager()
{
    PrimaryActorTick.bCanEverTick = false;
    ProtectedZone = CreateDefaultSubobject<UTRIADProtectedZoneComponent>(TEXT("ProtectedZone"));
    Tags.AddUnique(TEXT("TRIADSimulationOnlyRL"));
}

bool ATRIADAdversarialTrainingManager::IsTrainingRequested()
{
    return FParse::Param(FCommandLine::Get(), TEXT("TRIADRLTraining"));
}

bool ATRIADAdversarialTrainingManager::LoadTrainingConfig(
    FTRIADRLTrainingConfig& OutConfig, FString& OutError)
{
    if (const UTRIADRLTrainingDefinition* Definition = LoadObject<UTRIADRLTrainingDefinition>(
            nullptr, DefaultTrainingAssetPath, nullptr, LOAD_NoWarn))
    {
        OutConfig = Definition->Config;
    }
    else
    {
        FString ConfigPath = FPaths::Combine(
            FPaths::ProjectContentDir(), TEXT("TRIAD"), TEXT("RL"), TEXT("DefaultTrainingConfig.json"));
        FString OverridePath;
        if (FParse::Value(FCommandLine::Get(), TEXT("-TRIADRLConfig="), OverridePath) &&
            FPaths::FileExists(OverridePath))
        {
            ConfigPath = FPaths::ConvertRelativePathToFull(OverridePath);
        }
        FString JsonText;
        if (!FFileHelper::LoadFileToString(JsonText, *ConfigPath))
        {
            OutError = FString::Printf(TEXT("Could not load RL definition asset or JSON fallback '%s'."), *ConfigPath);
            return false;
        }
        FText FailureReason;
        if (!FJsonObjectConverter::JsonObjectStringToUStruct(JsonText, &OutConfig, 0, 0, false, &FailureReason))
        {
            OutError = TEXT("Could not deserialize RL definition: ") + FailureReason.ToString();
            return false;
        }
    }
    if (!OutConfig.bEnabled)
    {
        OutError = TEXT("RL definition is disabled.");
        return false;
    }
    return FTRIADRLTrainingModel::ValidateConfig(OutConfig, OutError);
}

void ATRIADAdversarialTrainingManager::BeginPlay()
{
    Super::BeginPlay();
    if (!IsTrainingRequested())
    {
        Destroy();
        return;
    }

    FString Error;
    if (!LoadTrainingConfig(TrainingConfig, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("TRIAD RL training did not start: %s"), *Error);
        Destroy();
        return;
    }
    Georeference = FindGeoreference();
    if (!Georeference)
    {
        UE_LOG(LogTemp, Error, TEXT("TRIAD RL training requires an existing CesiumGeoreference."));
        Destroy();
        return;
    }
    ProtectedZone->Configure(TrainingConfig.ProtectedZone);
    FTRIADRLStepResult InitialResult;
    if (!ResetEpisode(TrainingConfig.RandomSeed, InitialResult, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("TRIAD RL training reset failed: %s"), *Error);
        Destroy();
        return;
    }
    UE_LOG(LogTemp, Display, TEXT("TRIAD simulation-only RL environment ready in Blue placement phase."));
}

void ATRIADAdversarialTrainingManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearEpisodeActors();
    Super::EndPlay(EndPlayReason);
}

bool ATRIADAdversarialTrainingManager::ResetEpisode(
    int32 Seed, FTRIADRLStepResult& OutResult, FString& OutError)
{
    if (!Georeference)
    {
        Georeference = FindGeoreference();
    }
    if (!Georeference || !FTRIADRLTrainingModel::ValidateConfig(TrainingConfig, OutError))
    {
        return false;
    }

    ClearEpisodeActors();
    PreviouslyDetectedTargets.Reset();
    Phase = ETRIADRLPhase::BluePlacement;
    TerminationReason = ETRIADRLTerminationReason::None;
    StepIndex = 0;
    LastDetectedTargetCount = 0;
    SpentSensorBudgetUnits = 0.0;
    AccumulatedBlueReward = 0.0;
    AccumulatedRedReward = 0.0;
    EpisodeSeed = Seed == 0 ? 1 : Seed;
    if (!ResolveMapObjective(OutError))
    {
        Phase = ETRIADRLPhase::Terminal;
        TerminationReason = ETRIADRLTerminationReason::ConstraintViolation;
        return false;
    }
    PreviousMinimumZoneDistanceMeters = TrainingConfig.MaximumDistanceFromZoneMeters;
    BuildStepResult(OutResult);
    OutError.Reset();
    return true;
}

bool ATRIADAdversarialTrainingManager::ApplyBlueAction(
    const FTRIADRLBlueAction& Action, FTRIADRLStepResult& OutResult, FString& OutError)
{
    if (Phase != ETRIADRLPhase::BluePlacement)
    {
        OutError = TEXT("Blue actions are accepted only during the placement phase.");
        return false;
    }
    if (Action.bStopPlacement)
    {
        Phase = ETRIADRLPhase::RedDeployment;
        BuildStepResult(OutResult);
        OutError.Reset();
        return true;
    }
    if (!SpawnSensorAtContinuousPosition(Action.NormalizedPosition, OutError))
    {
        BuildStepResult(OutResult);
        return false;
    }
    AccumulatedBlueReward += TrainingConfig.Rewards.BlueSiteCost * TrainingConfig.SensorCostUnits;
    if (SpawnedSensors.Num() >= TrainingConfig.MaximumSensorSites)
    {
        Phase = ETRIADRLPhase::RedDeployment;
    }
    BuildStepResult(OutResult);
    OutError.Reset();
    return true;
}

bool ATRIADAdversarialTrainingManager::ApplyRedDeploymentAction(
    const FTRIADRLRedDeploymentAction& Action,
    FTRIADRLStepResult& OutResult,
    FString& OutError)
{
    if (Phase != ETRIADRLPhase::RedDeployment)
    {
        OutError = TEXT("Red deployment is accepted only after Blue commits its layout.");
        return false;
    }
    if (!SpawnTargetsFromDeployment(Action, OutError))
    {
        BuildStepResult(OutResult);
        return false;
    }
    PreviousMinimumZoneDistanceMeters = ComputeMinimumZoneDistanceMeters();
    Phase = ETRIADRLPhase::RedMovement;
    BuildStepResult(OutResult);
    OutError.Reset();
    return true;
}

bool ATRIADAdversarialTrainingManager::ApplyRedActions(
    const TArray<FTRIADRLRedAction>& Actions, FTRIADRLStepResult& OutResult, FString& OutError)
{
    if (Phase != ETRIADRLPhase::RedMovement)
    {
        OutError = TEXT("Red actions are accepted only during the movement phase.");
        return false;
    }
    if (Actions.Num() != TargetControllers.Num())
    {
        OutError = FString::Printf(
            TEXT("Red action count %d does not match active synthetic target count %d."),
            Actions.Num(), TargetControllers.Num());
        return false;
    }

    bool bMotionConstraintViolation = false;
    for (int32 Index = 0; Index < Actions.Num(); ++Index)
    {
        if (!TargetControllers[Index] || !TargetControllers[Index]->ApplyNormalizedVelocity(
                Actions[Index].NormalizedVelocityEnu, TrainingConfig.FixedStepSeconds))
        {
            bMotionConstraintViolation = true;
        }
    }
    ++StepIndex;
    const double CurrentMinimumDistance = ComputeMinimumZoneDistanceMeters();
    const int32 DetectedTargetCount = EvaluateDetectedTargets();
    const int32 NewlyDetectedTargets = FMath::Max(DetectedTargetCount - LastDetectedTargetCount, 0);
    const bool bZoneReached = HasProtectedZoneEntry();
    const bool bConstraintViolation = bMotionConstraintViolation || HasConstraintViolation();
    double BlueStepReward = 0.0;
    double RedStepReward = 0.0;
    FTRIADRLTrainingModel::ComputeStepRewards(
        TrainingConfig.Rewards,
        PreviousMinimumZoneDistanceMeters,
        CurrentMinimumDistance,
        NewlyDetectedTargets,
        bZoneReached,
        bConstraintViolation,
        BlueStepReward,
        RedStepReward);
    if (NewlyDetectedTargets > 0)
    {
        BlueStepReward += NewlyDetectedTargets * TrainingConfig.Rewards.BlueEarlyDetection *
            FMath::Clamp(CurrentMinimumDistance / TrainingConfig.MaximumDistanceFromZoneMeters, 0.0, 1.0);
    }
    AccumulatedBlueReward += BlueStepReward;
    AccumulatedRedReward += RedStepReward;
    PreviousMinimumZoneDistanceMeters = CurrentMinimumDistance;
    LastDetectedTargetCount = DetectedTargetCount;

    if (bConstraintViolation)
    {
        Phase = ETRIADRLPhase::Terminal;
        TerminationReason = ETRIADRLTerminationReason::ConstraintViolation;
    }
    else if (bZoneReached)
    {
        Phase = ETRIADRLPhase::Terminal;
        TerminationReason = ETRIADRLTerminationReason::ProtectedZoneReached;
    }
    else if (DetectedTargetCount == SpawnedTargets.Num() && !SpawnedTargets.IsEmpty())
    {
        Phase = ETRIADRLPhase::Terminal;
        TerminationReason = ETRIADRLTerminationReason::AllTargetsConfirmed;
    }
    else if (StepIndex >= TrainingConfig.EpisodeHorizonSteps)
    {
        Phase = ETRIADRLPhase::Terminal;
        TerminationReason = ETRIADRLTerminationReason::HorizonReached;
    }

    BuildStepResult(OutResult);
    OutError.Reset();
    return true;
}

void ATRIADAdversarialTrainingManager::ClearEpisodeActors()
{
    for (ATRIADSensorNodeActor* Sensor : SpawnedSensors)
    {
        if (IsValid(Sensor)) Sensor->Destroy();
    }
    for (ATRIADDemoDroneActor* Target : SpawnedTargets)
    {
        if (IsValid(Target)) Target->Destroy();
    }
    SpawnedSensors.Reset();
    SpawnedTargets.Reset();
    TargetControllers.Reset();
}

bool ATRIADAdversarialTrainingManager::ResolveMapObjective(FString& OutError)
{
    ObjectiveActor = nullptr;
    int32 MatchCount = 0;
    if (GetWorld())
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It) && It->ActorHasTag(TEXT("TRIADRLObjective")))
            {
                ObjectiveActor = *It;
                ++MatchCount;
            }
        }
    }
    if (MatchCount != 1 || !ObjectiveActor || !Georeference)
    {
        OutError = FString::Printf(
            TEXT("RL map requires exactly one actor tagged TRIADRLObjective; found %d."), MatchCount);
        ObjectiveActor = nullptr;
        return false;
    }
    const FVector ObjectiveLongitudeLatitudeHeight =
        Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(ObjectiveActor->GetActorLocation());
    TrainingConfig.ProtectedZone.LongitudeDegrees = ObjectiveLongitudeLatitudeHeight.X;
    TrainingConfig.ProtectedZone.LatitudeDegrees = ObjectiveLongitudeLatitudeHeight.Y;
    TrainingConfig.ProtectedZone.HeightMeters = ObjectiveLongitudeLatitudeHeight.Z;
    ProtectedZone->Configure(TrainingConfig.ProtectedZone);
    return true;
}

bool ATRIADAdversarialTrainingManager::SpawnTargetsFromDeployment(
    const FTRIADRLRedDeploymentAction& Action, FString& OutError)
{
    const double Values[] = {
        Action.NormalizedBearing,
        Action.NormalizedRadius,
        Action.NormalizedAltitude,
        Action.NormalizedSwarmSize,
        Action.NormalizedFormationSpacing};
    for (const double Value : Values)
    {
        if (!FMath::IsFinite(Value) || Value < -1.0 || Value > 1.0)
        {
            OutError = TEXT("Red deployment values must be finite and normalized to [-1, 1].");
            return false;
        }
    }
    if (!GetWorld() || !ObjectiveActor)
    {
        OutError = TEXT("RL world or objective is unavailable for Red deployment.");
        return false;
    }

    const auto UnitInterval = [](double Value) { return (Value + 1.0) * 0.5; };
    const double BearingRadians = FMath::DegreesToRadians(Action.NormalizedBearing * 180.0);
    const double RadiusMeters = FMath::Lerp(
        TrainingConfig.RedMinimumSpawnRadiusMeters,
        TrainingConfig.RedMaximumSpawnRadiusMeters,
        UnitInterval(Action.NormalizedRadius));
    const double AltitudeMeters = FMath::Lerp(
        TrainingConfig.RedMinimumAltitudeMeters,
        TrainingConfig.RedMaximumAltitudeMeters,
        UnitInterval(Action.NormalizedAltitude));
    const int32 Count = FMath::Clamp(
        FMath::RoundToInt(FMath::Lerp(
            static_cast<double>(TrainingConfig.RedMinimumSwarmSize),
            static_cast<double>(TrainingConfig.RedMaximumSwarmSize),
            UnitInterval(Action.NormalizedSwarmSize))),
        1,
        MaximumRLTargetActors);
    const double FormationSpacingMeters = FMath::Lerp(
        TrainingConfig.RedMinimumFormationSpacingMeters,
        TrainingConfig.RedMaximumFormationSpacingMeters,
        UnitInterval(Action.NormalizedFormationSpacing));
    const double RadialEastMeters = FMath::Sin(BearingRadians) * RadiusMeters;
    const double RadialNorthMeters = FMath::Cos(BearingRadians) * RadiusMeters;
    const FVector2D Tangent(FMath::Cos(BearingRadians), -FMath::Sin(BearingRadians));
    constexpr double EarthRadiusMeters = 6378137.0;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const double FormationOffset = (Index - (Count - 1) * 0.5) * FormationSpacingMeters;
        const double EastOffset = RadialEastMeters + Tangent.X * FormationOffset;
        const double NorthOffset = RadialNorthMeters + Tangent.Y * FormationOffset;
        const double CenterLatitude = TrainingConfig.ProtectedZone.LatitudeDegrees;
        const double CosLatitude = FMath::Max(
            FMath::Abs(FMath::Cos(FMath::DegreesToRadians(CenterLatitude))), 0.000001);
        FTRIADDemoTargetDefinition Definition;
        Definition.ActorName = FString::Printf(TEXT("RL_Target_%02d"), Index + 1);
        Definition.StartLongitudeDegrees = TrainingConfig.ProtectedZone.LongitudeDegrees +
            FMath::RadiansToDegrees(EastOffset / (EarthRadiusMeters * CosLatitude));
        Definition.StartLatitudeDegrees = CenterLatitude +
            FMath::RadiansToDegrees(NorthOffset / EarthRadiusMeters);
        Definition.StartHeightMeters = TrainingConfig.ProtectedZone.HeightMeters + AltitudeMeters;
        Definition.Trajectory = ETRIADDemoTrajectory::Stationary;
        Definition.RFEmitter.EmitterId = Definition.ActorName + TEXT("_SyntheticRF");
        Definition.RFEmitter.CenterFrequenciesGHz = {2.437, 5.795};
        Definition.RFEmitter.TransmitPowerDbm = 20.0;
        Definition.RFEmitter.TransmitAntennaGainDbi = 2.0;
        Definition.RFEmitter.bEnabled = true;

        FActorSpawnParameters Parameters;
        Parameters.Name = MakeUniqueObjectName(GetWorld(), ATRIADDemoDroneActor::StaticClass(), FName(*Definition.ActorName));
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ATRIADDemoDroneActor* Target = GetWorld()->SpawnActor<ATRIADDemoDroneActor>(
            ATRIADDemoDroneActor::StaticClass(), FTransform::Identity, Parameters);
        if (!Target)
        {
            OutError = TEXT("Could not spawn an RL synthetic target.");
            return false;
        }
        Target->ConfigureDemoTarget(Definition, Georeference);
        UTRIADSwarmControllerComponent* Controller = NewObject<UTRIADSwarmControllerComponent>(Target);
        Controller->RegisterComponent();
        Controller->Configure(
            Georeference,
            Definition.StartLongitudeDegrees,
            Definition.StartLatitudeDegrees,
            Definition.StartHeightMeters,
            TrainingConfig.MaximumTargetSpeedMetersPerSecond);
        SpawnedTargets.Add(Target);
        TargetControllers.Add(Controller);
    }
    return true;
}

bool ATRIADAdversarialTrainingManager::SpawnSensorAtContinuousPosition(
    const FVector2D& NormalizedPosition, FString& OutError)
{
    if (!FMath::IsFinite(NormalizedPosition.X) || !FMath::IsFinite(NormalizedPosition.Y) ||
        NormalizedPosition.SizeSquared() > 1.0 + UE_DOUBLE_SMALL_NUMBER)
    {
        OutError = TEXT("Blue placement must be a finite point inside the normalized unit disk.");
        return false;
    }
    const FVector2D OffsetMeters = NormalizedPosition * TrainingConfig.BluePlacementRadiusMeters;
    if (OffsetMeters.Length() < TrainingConfig.BlueMinimumObjectiveStandoffMeters)
    {
        OutError = TEXT("Blue placement is inside the protected objective standoff.");
        return false;
    }
    if (SpawnedSensors.Num() >= TrainingConfig.MaximumSensorSites ||
        SpentSensorBudgetUnits + TrainingConfig.SensorCostUnits > TrainingConfig.SensorBudgetUnits + UE_DOUBLE_SMALL_NUMBER)
    {
        OutError = TEXT("Blue sensor-site or budget limit would be exceeded.");
        return false;
    }

    constexpr double EarthRadiusMeters = 6378137.0;
    const double CenterLatitudeRadians = FMath::DegreesToRadians(TrainingConfig.ProtectedZone.LatitudeDegrees);
    const double CosLatitude = FMath::Max(FMath::Abs(FMath::Cos(CenterLatitudeRadians)), 0.000001);
    const double CandidateLongitude = TrainingConfig.ProtectedZone.LongitudeDegrees +
        FMath::RadiansToDegrees(OffsetMeters.X / (EarthRadiusMeters * CosLatitude));
    const double CandidateLatitude = TrainingConfig.ProtectedZone.LatitudeDegrees +
        FMath::RadiansToDegrees(OffsetMeters.Y / EarthRadiusMeters);
    const FVector TraceStart = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
        FVector(CandidateLongitude, CandidateLatitude, TrainingConfig.ProtectedZone.HeightMeters + 1500.0));
    const FVector TraceEnd = Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
        FVector(CandidateLongitude, CandidateLatitude, TrainingConfig.ProtectedZone.HeightMeters - 500.0));
    FHitResult SurfaceHit;
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TRIADRLBluePlacement), true);
    if (!GetWorld()->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
    {
        OutError = TEXT("Blue placement found no loaded collision surface; the action was rejected.");
        return false;
    }
    for (const ATRIADSensorNodeActor* ExistingSensor : SpawnedSensors)
    {
        if (ExistingSensor && FVector::Distance(ExistingSensor->GetActorLocation(), SurfaceHit.ImpactPoint) / 100.0 <
                TrainingConfig.MinimumSensorSeparationMeters)
        {
            OutError = TEXT("Blue placement violates minimum sensor separation.");
            return false;
        }
    }
    const FVector SurfaceLongitudeLatitudeHeight =
        Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(SurfaceHit.ImpactPoint);
    FTRIADGeodeticSensorNode SensorDefinition = TrainingConfig.BlueSensorTemplate;
    SensorDefinition.NodeId = FString::Printf(TEXT("RL_Blue_%02d"), SpawnedSensors.Num() + 1);
    SensorDefinition.LongitudeDegrees = SurfaceLongitudeLatitudeHeight.X;
    SensorDefinition.LatitudeDegrees = SurfaceLongitudeLatitudeHeight.Y;
    SensorDefinition.HeightMeters = SurfaceLongitudeLatitudeHeight.Z + 2.0;
    SensorDefinition.bEnabled = true;
    SensorDefinition.bCaptureCameraFrames = false;
    SensorDefinition.bTrackNearestTarget = false;
    FActorSpawnParameters Parameters;
    Parameters.Name = MakeUniqueObjectName(GetWorld(), ATRIADSensorNodeActor::StaticClass(),
        FName(*FString::Printf(TEXT("TRIAD_RL_Sensor_%02d"), SpawnedSensors.Num() + 1)));
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADSensorNodeActor* Sensor = GetWorld()->SpawnActor<ATRIADSensorNodeActor>(
        ATRIADSensorNodeActor::StaticClass(), FTransform::Identity, Parameters);
    if (!Sensor)
    {
        OutError = TEXT("Could not spawn a Blue sensor at the validated continuous position.");
        return false;
    }
    Sensor->ConfigureNode(
        SensorDefinition,
        Georeference,
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TRIAD"), TEXT("RL"), TEXT("frames")),
        0,
        0);
    SpawnedSensors.Add(Sensor);
    SpentSensorBudgetUnits += TrainingConfig.SensorCostUnits;
    return true;
}

int32 ATRIADAdversarialTrainingManager::EvaluateDetectedTargets()
{
    for (ATRIADDemoDroneActor* Target : SpawnedTargets)
    {
        if (!IsValid(Target) || !Target->RFEmitter) continue;
        int32 ConfirmingNodes = 0;
        for (ATRIADSensorNodeActor* Sensor : SpawnedSensors)
        {
            if (!IsValid(Sensor) || !Sensor->SensorNode) continue;
            const double DistanceMeters = FVector::Distance(Sensor->GetActorLocation(), Target->GetActorLocation()) / 100.0;
            const FTRIADGeodeticSensorNode& Node = Sensor->GetNodeDefinition();
            FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TRIADRLSensorLOS), true);
            QueryParams.AddIgnoredActor(Sensor);
            QueryParams.AddIgnoredActor(Target);
            FHitResult Hit;
            const bool bLineOfSight = !GetWorld()->LineTraceSingleByChannel(
                Hit, Sensor->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, QueryParams);

            bool bNodeDetected = false;
            if (Target->RFEmitter->IsEmitting() && DistanceMeters <= FMath::Max(Node.DetectionRangeMeters, 0.0))
            {
                for (const double FrequencyGHz : Target->RFEmitter->GetCenterFrequenciesGHz())
                {
                    if (!Sensor->SensorNode->SupportsFrequencyGHz(FrequencyGHz)) continue;
                    const double ReceivedPowerDbm = Target->RFEmitter->Definition.TransmitPowerDbm +
                        Target->RFEmitter->Definition.TransmitAntennaGainDbi + Node.ReceiveAntennaGainDbi -
                        Sensor->SensorNode->ComputeFreeSpacePathLossDb(FrequencyGHz, DistanceMeters) - Node.SystemLossDb;
                    if (ReceivedPowerDbm >= Node.ReceiverSensitivityDbm)
                    {
                        bNodeDetected = true;
                        break;
                    }
                }
            }
            if (!bNodeDetected && Node.bEnableSearchRadar)
            {
                FTRIADSearchRadarModelInput Input;
                Input.TrueRangeMeters = DistanceMeters;
                Input.RadarCrossSectionSquareMeters = Target->GetSimulatedRadarCrossSectionSquareMeters();
                Input.RangeEnvelopeMeters = Node.SearchRadarRangeMeters;
                Input.ElevationFieldOfRegardDegrees = Node.SearchRadarElevationFieldOfRegardDegrees;
                Input.DetectionThreshold = Node.SearchRadarDetectionThreshold;
                Input.bLineOfSight = bLineOfSight;
                Input.DeterministicSeed = HashCombine(GetTypeHash(StepIndex), GetTypeHash(Target->GetFName()));
                bNodeDetected = FTRIADLongRangeSensorModel::EvaluateSearchRadar(Input).bDetected;
            }
            ConfirmingNodes += bNodeDetected ? 1 : 0;
        }
        if (ConfirmingNodes >= FMath::Clamp(TrainingConfig.ConfirmationNodeCount, 1, 32))
        {
            PreviouslyDetectedTargets.Add(Target->GetFName());
        }
    }
    return PreviouslyDetectedTargets.Num();
}

double ATRIADAdversarialTrainingManager::ComputeMinimumZoneDistanceMeters() const
{
    double Minimum = TNumericLimits<double>::Max();
    for (const UTRIADSwarmControllerComponent* Controller : TargetControllers)
    {
        if (Controller)
        {
            Minimum = FMath::Min(Minimum, ProtectedZone->SignedHorizontalDistanceMeters(
                Controller->GetLongitudeDegrees(), Controller->GetLatitudeDegrees()));
        }
    }
    return Minimum == TNumericLimits<double>::Max()
        ? TrainingConfig.MaximumDistanceFromZoneMeters
        : Minimum;
}

bool ATRIADAdversarialTrainingManager::HasConstraintViolation() const
{
    for (const UTRIADSwarmControllerComponent* Controller : TargetControllers)
    {
        if (!Controller || ProtectedZone->SignedHorizontalDistanceMeters(
                Controller->GetLongitudeDegrees(), Controller->GetLatitudeDegrees()) >
                TrainingConfig.MaximumDistanceFromZoneMeters)
        {
            return true;
        }
    }
    return false;
}

bool ATRIADAdversarialTrainingManager::HasProtectedZoneEntry() const
{
    return !TargetControllers.IsEmpty() && ComputeMinimumZoneDistanceMeters() <= 0.0;
}

void ATRIADAdversarialTrainingManager::BuildStepResult(FTRIADRLStepResult& OutResult) const
{
    OutResult = FTRIADRLStepResult();
    OutResult.Phase = Phase;
    OutResult.TerminationReason = TerminationReason;
    OutResult.StepIndex = StepIndex;
    OutResult.ActiveTargetCount = SpawnedTargets.Num();
    OutResult.DetectedTargetCount = LastDetectedTargetCount;
    OutResult.MinimumZoneDistanceMeters = ComputeMinimumZoneDistanceMeters();
    OutResult.BlueReward = AccumulatedBlueReward;
    OutResult.RedReward = AccumulatedRedReward;
    OutResult.bTerminal = Phase == ETRIADRLPhase::Terminal;
    const bool bCanPlace = Phase == ETRIADRLPhase::BluePlacement &&
        SpawnedSensors.Num() < TrainingConfig.MaximumSensorSites &&
        SpentSensorBudgetUnits + TrainingConfig.SensorCostUnits <=
            TrainingConfig.SensorBudgetUnits + UE_DOUBLE_SMALL_NUMBER;
    OutResult.BlueActionMask = {bCanPlace, Phase == ETRIADRLPhase::BluePlacement};

    constexpr double EarthRadiusMeters = 6378137.0;
    const double ZoneLatitudeRadians = FMath::DegreesToRadians(TrainingConfig.ProtectedZone.LatitudeDegrees);
    const double CosZoneLatitude = FMath::Max(FMath::Abs(FMath::Cos(ZoneLatitudeRadians)), 0.000001);
    const double SensorPositionScale = FMath::Max(TrainingConfig.BluePlacementRadiusMeters, 1.0);
    OutResult.SensorObservations.Reserve(SpawnedSensors.Num());
    for (const ATRIADSensorNodeActor* Sensor : SpawnedSensors)
    {
        if (!Sensor) continue;
        const FTRIADGeodeticSensorNode& Definition = Sensor->GetNodeDefinition();
        OutResult.SensorObservations.Add(FVector2D(
            FMath::DegreesToRadians(Definition.LongitudeDegrees - TrainingConfig.ProtectedZone.LongitudeDegrees) *
                EarthRadiusMeters * CosZoneLatitude / SensorPositionScale,
            FMath::DegreesToRadians(Definition.LatitudeDegrees - TrainingConfig.ProtectedZone.LatitudeDegrees) *
                EarthRadiusMeters / SensorPositionScale));
    }
    const double PositionScale = FMath::Max(TrainingConfig.MaximumDistanceFromZoneMeters, 1.0);
    const double VelocityScale = FMath::Max(TrainingConfig.MaximumTargetSpeedMetersPerSecond, 0.1);
    OutResult.TargetObservations.Reserve(TargetControllers.Num());
    for (int32 Index = 0; Index < TargetControllers.Num(); ++Index)
    {
        const UTRIADSwarmControllerComponent* Controller = TargetControllers[Index];
        if (!Controller) continue;
        FTRIADRLTargetObservation Observation;
        Observation.NormalizedZoneRelativeEnu = FVector(
            FMath::DegreesToRadians(Controller->GetLongitudeDegrees() - TrainingConfig.ProtectedZone.LongitudeDegrees) *
                EarthRadiusMeters * CosZoneLatitude / PositionScale,
            FMath::DegreesToRadians(Controller->GetLatitudeDegrees() - TrainingConfig.ProtectedZone.LatitudeDegrees) *
                EarthRadiusMeters / PositionScale,
            (Controller->GetHeightMeters() - TrainingConfig.ProtectedZone.HeightMeters) / PositionScale);
        Observation.NormalizedZoneRelativeEnu.X = FMath::Clamp(Observation.NormalizedZoneRelativeEnu.X, -1.5, 1.5);
        Observation.NormalizedZoneRelativeEnu.Y = FMath::Clamp(Observation.NormalizedZoneRelativeEnu.Y, -1.5, 1.5);
        Observation.NormalizedZoneRelativeEnu.Z = FMath::Clamp(Observation.NormalizedZoneRelativeEnu.Z, -1.5, 1.5);
        Observation.NormalizedVelocityEnu = Controller->GetVelocityEnuMetersPerSecond() / VelocityScale;
        Observation.bConfirmedDetected = SpawnedTargets.IsValidIndex(Index) && SpawnedTargets[Index] &&
            PreviouslyDetectedTargets.Contains(SpawnedTargets[Index]->GetFName());
        OutResult.TargetObservations.Add(Observation);
    }
}

ACesiumGeoreference* ATRIADAdversarialTrainingManager::FindGeoreference() const
{
    if (!GetWorld()) return nullptr;
    for (TActorIterator<ACesiumGeoreference> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}
