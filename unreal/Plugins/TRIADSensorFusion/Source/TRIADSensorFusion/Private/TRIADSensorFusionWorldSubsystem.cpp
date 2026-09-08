#include "TRIADSensorFusionWorldSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "TRIADAdversarialTrainingManager.h"
#include "TRIADSensorFusionScenarioManager.h"

bool UTRIADSensorFusionWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!Super::ShouldCreateSubsystem(Outer))
    {
        return false;
    }

    const UWorld* World = Cast<UWorld>(Outer);
    if (!World || !DoesSupportWorldType(World->WorldType))
    {
        return false;
    }

    FTRIADSensorFusionScenarioConfig Config;
    FString Error;
    const bool bSensorFusionEnabled =
        ATRIADSensorFusionScenarioManager::LoadScenarioConfig(Config, Error) && Config.bEnabled;
    return bSensorFusionEnabled || ATRIADAdversarialTrainingManager::IsTrainingRequested();
}

bool UTRIADSensorFusionWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UTRIADSensorFusionWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    FTRIADSensorFusionScenarioConfig Config;
    FString ConfigError;
    const bool bSensorFusionEnabled =
        ATRIADSensorFusionScenarioManager::LoadScenarioConfig(Config, ConfigError) && Config.bEnabled;
    if (bSensorFusionEnabled)
    {
        for (TActorIterator<ATRIADSensorFusionScenarioManager> It(&InWorld); It; ++It)
        {
            ScenarioManager = *It;
            break;
        }
        if (!ScenarioManager)
        {
            FActorSpawnParameters SpawnParameters;
            SpawnParameters.Name = TEXT("TRIAD_SingaporeSensorFusion_Manager");
            SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            ScenarioManager = InWorld.SpawnActor<ATRIADSensorFusionScenarioManager>(
                ATRIADSensorFusionScenarioManager::StaticClass(),
                FTransform::Identity,
                SpawnParameters);
        }
    }

    if (ATRIADAdversarialTrainingManager::IsTrainingRequested())
    {
        FActorSpawnParameters SpawnParameters;
        SpawnParameters.Name = TEXT("TRIAD_AdversarialTraining_Manager");
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AdversarialTrainingManager = InWorld.SpawnActor<ATRIADAdversarialTrainingManager>(
            ATRIADAdversarialTrainingManager::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    }
}

void UTRIADSensorFusionWorldSubsystem::Deinitialize()
{
    ScenarioManager = nullptr;
    AdversarialTrainingManager = nullptr;
    Super::Deinitialize();
}
