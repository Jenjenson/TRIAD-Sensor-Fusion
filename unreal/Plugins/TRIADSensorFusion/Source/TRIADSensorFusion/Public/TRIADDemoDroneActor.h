#pragma once

#include "GameFramework/Actor.h"
#include "TRIADSensorFusionTypes.h"
#include "TRIADDemoDroneActor.generated.h"

class ACesiumGeoreference;
class UCesiumGlobeAnchorComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTRIADRFEmitterComponent;

/** Lightweight synthetic target for validating discovery, RF propagation, and LOS. */
UCLASS(BlueprintType, Blueprintable)
class TRIADSENSORFUSION_API ATRIADDemoDroneActor : public AActor
{
    GENERATED_BODY()

public:
    ATRIADDemoDroneActor();

    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Demo Target")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Demo Target")
    TObjectPtr<UStaticMeshComponent> VisualMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Demo Target")
    TObjectPtr<UTextRenderComponent> TargetLabel;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Demo Target")
    TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "TRIAD|Demo Target")
    TObjectPtr<UTRIADRFEmitterComponent> RFEmitter;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Demo Target")
    void ConfigureDemoTarget(const FTRIADDemoTargetDefinition& InDefinition, ACesiumGeoreference* InGeoreference);

    double GetCurrentHeadingDegrees() const { return CurrentHeadingDegrees; }
    double GetCurrentSpeedMetersPerSecond() const { return CurrentSpeedMetersPerSecond; }
    const FString& GetIngressCorridorId() const { return Definition.IngressCorridorId; }
    bool IsInboundApproachScenarioTarget() const { return Definition.bInboundApproachScenario; }
    double GetSimulatedRadarCrossSectionSquareMeters() const
    {
        return FMath::Max(Definition.SimulatedRadarCrossSectionSquareMeters, 0.0001);
    }

private:
    void UpdateGeodeticPosition(double ElapsedSeconds);

    FTRIADDemoTargetDefinition Definition;
    double TrajectoryStartWorldSeconds = 0.0;
    double CurrentHeadingDegrees = 0.0;
    double CurrentSpeedMetersPerSecond = 0.0;
};
