#include "TRIADSensorFusionWorldSubsystem.h"

#include "EngineUtils.h"
#include "Engine/World.h"
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
    return ATRIADSensorFusionScenarioManager::LoadScenarioConfig(Config, Error) && Config.bEnabled;
}

bool UTRIADSensorFusionWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UTRIADSensorFusionWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    for (TActorIterator<ATRIADSensorFusionScenarioManager> It(&InWorld); It; ++It)
    {
        ScenarioManager = *It;
        return;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIAD_SingaporeSensorFusion_Manager");
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ScenarioManager = InWorld.SpawnActor<ATRIADSensorFusionScenarioManager>(
        ATRIADSensorFusionScenarioManager::StaticClass(),
        FTransform::Identity,
        SpawnParameters);
}

void UTRIADSensorFusionWorldSubsystem::Deinitialize()
{
    ScenarioManager = nullptr;
    Super::Deinitialize();
}
