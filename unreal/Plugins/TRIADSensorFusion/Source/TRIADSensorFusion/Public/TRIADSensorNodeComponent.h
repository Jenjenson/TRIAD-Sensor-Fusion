#pragma once

#include "Components/ActorComponent.h"
#include "TRIADSensorFusionTypes.h"
#include "TRIADSensorNodeComponent.generated.h"

/** RF receiver-side parameters and deterministic propagation helpers for one node. */
UCLASS(ClassGroup = (TRIAD), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TRIADSENSORFUSION_API UTRIADSensorNodeComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTRIADSensorNodeComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|Sensor Node")
    FTRIADGeodeticSensorNode Definition;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|Sensor Node")
    void ConfigureNode(const FTRIADGeodeticSensorNode& InDefinition);

    UFUNCTION(BlueprintPure, Category = "TRIAD|Sensor Node")
    bool SupportsFrequencyGHz(double FrequencyGHz, double ToleranceGHz = 0.01) const;

    UFUNCTION(BlueprintPure, Category = "TRIAD|Sensor Node")
    double ComputeFreeSpacePathLossDb(double FrequencyGHz, double DistanceMeters) const;

    UFUNCTION(BlueprintPure, Category = "TRIAD|Sensor Node")
    double ComputeNoiseFloorDbm() const;
};
