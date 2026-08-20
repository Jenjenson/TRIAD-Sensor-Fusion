#include "TRIADRFEmitterComponent.h"

UTRIADRFEmitterComponent::UTRIADRFEmitterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTRIADRFEmitterComponent::ConfigureEmitter(const FTRIADRFEmitterDefinition& InDefinition)
{
    Definition = InDefinition;
}

bool UTRIADRFEmitterComponent::IsEmitting() const
{
    return Definition.bEnabled && !Definition.CenterFrequenciesGHz.IsEmpty();
}

const TArray<double>& UTRIADRFEmitterComponent::GetCenterFrequenciesGHz() const
{
    return Definition.CenterFrequenciesGHz;
}
