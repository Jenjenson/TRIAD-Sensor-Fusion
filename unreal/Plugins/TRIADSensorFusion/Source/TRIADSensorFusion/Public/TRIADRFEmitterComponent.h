#pragma once

#include "Components/ActorComponent.h"
#include "TRIADSensorFusionTypes.h"
#include "TRIADRFEmitterComponent.generated.h"

/** Describes RF energy emitted by an actor; it does not transmit or control anything. */
UCLASS(ClassGroup = (TRIAD), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TRIADSENSORFUSION_API UTRIADRFEmitterComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTRIADRFEmitterComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TRIAD|RF Emitter")
    FTRIADRFEmitterDefinition Definition;

    UFUNCTION(BlueprintCallable, Category = "TRIAD|RF Emitter")
    void ConfigureEmitter(const FTRIADRFEmitterDefinition& InDefinition);

    UFUNCTION(BlueprintPure, Category = "TRIAD|RF Emitter")
    bool IsEmitting() const;

    UFUNCTION(BlueprintPure, Category = "TRIAD|RF Emitter")
    const TArray<double>& GetCenterFrequenciesGHz() const;
};
