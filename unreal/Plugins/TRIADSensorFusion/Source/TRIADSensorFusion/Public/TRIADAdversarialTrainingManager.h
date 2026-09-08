#pragma once

#include "GameFramework/Actor.h"
#include "TRIADRLTrainingTypes.h"
#include "TRIADAdversarialTrainingManager.generated.h"

class ACesiumGeoreference;
class ATRIADDemoDroneActor;
class ATRIADSensorNodeActor;
class UTRIADProtectedZoneComponent;
class UTRIADSwarmControllerComponent;

/** Deterministic, simulation-only Red/Blue episode owner. */
UCLASS(BlueprintType, NotPlaceable)
class TRIADSENSORFUSION_API ATRIADAdversarialTrainingManager : public AActor
{
    GENERATED_BODY()

public:
    ATRIADAdversarialTrainingManager();

    static bool IsTrainingRequested();
    static bool LoadTrainingConfig(FTRIADRLTrainingConfig& OutConfig, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|RL")
    bool ResetEpisode(int32 Seed, FTRIADRLStepResult& OutResult, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|RL")
    bool ApplyBlueAction(const FTRIADRLBlueAction& Action, FTRIADRLStepResult& OutResult, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|RL")
    bool ApplyRedDeploymentAction(
        const FTRIADRLRedDeploymentAction& Action,
        FTRIADRLStepResult& OutResult,
        FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "TRIAD|RL")
    bool ApplyRedActions(const TArray<FTRIADRLRedAction>& Actions, FTRIADRLStepResult& OutResult, FString& OutError);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    TObjectPtr<UTRIADProtectedZoneComponent> ProtectedZone;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|RL")
    FTRIADRLTrainingConfig TrainingConfig;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void ClearEpisodeActors();
    bool ResolveMapObjective(FString& OutError);
    bool SpawnTargetsFromDeployment(const FTRIADRLRedDeploymentAction& Action, FString& OutError);
    bool SpawnSensorAtContinuousPosition(const FVector2D& NormalizedPosition, FString& OutError);
    void BuildStepResult(FTRIADRLStepResult& OutResult) const;
    int32 EvaluateDetectedTargets();
    double ComputeMinimumZoneDistanceMeters() const;
    bool HasConstraintViolation() const;
    bool HasProtectedZoneEntry() const;
    ACesiumGeoreference* FindGeoreference() const;

    UPROPERTY(Transient)
    TObjectPtr<ACesiumGeoreference> Georeference;

    UPROPERTY(Transient)
    TObjectPtr<AActor> ObjectiveActor;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATRIADSensorNodeActor>> SpawnedSensors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ATRIADDemoDroneActor>> SpawnedTargets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTRIADSwarmControllerComponent>> TargetControllers;

    TSet<FName> PreviouslyDetectedTargets;
    ETRIADRLPhase Phase = ETRIADRLPhase::Inactive;
    ETRIADRLTerminationReason TerminationReason = ETRIADRLTerminationReason::None;
    int32 StepIndex = 0;
    int32 LastDetectedTargetCount = 0;
    double SpentSensorBudgetUnits = 0.0;
    double PreviousMinimumZoneDistanceMeters = 0.0;
    double AccumulatedBlueReward = 0.0;
    double AccumulatedRedReward = 0.0;
    int32 EpisodeSeed = 1;
};
