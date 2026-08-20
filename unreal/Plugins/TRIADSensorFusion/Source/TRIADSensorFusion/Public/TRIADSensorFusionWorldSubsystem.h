#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "TRIADSensorFusionWorldSubsystem.generated.h"

class ATRIADSensorFusionScenarioManager;

/** PIE/Game-only bootstrap. Its creation is gated by the enabled project JSON config. */
UCLASS()
class TRIADSENSORFUSION_API UTRIADSensorFusionWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
    UPROPERTY(Transient)
    TObjectPtr<ATRIADSensorFusionScenarioManager> ScenarioManager;
};
