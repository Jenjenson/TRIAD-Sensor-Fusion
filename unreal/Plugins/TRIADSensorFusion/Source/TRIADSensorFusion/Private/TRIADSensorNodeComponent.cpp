#include "TRIADSensorNodeComponent.h"

UTRIADSensorNodeComponent::UTRIADSensorNodeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTRIADSensorNodeComponent::ConfigureNode(const FTRIADGeodeticSensorNode& InDefinition)
{
    Definition = InDefinition;
}

bool UTRIADSensorNodeComponent::SupportsFrequencyGHz(double FrequencyGHz, double ToleranceGHz) const
{
    for (const double SupportedFrequencyGHz : Definition.SupportedFrequenciesGHz)
    {
        if (FMath::IsNearlyEqual(SupportedFrequencyGHz, FrequencyGHz, FMath::Max(ToleranceGHz, 0.0)))
        {
            return true;
        }
    }
    return false;
}

double UTRIADSensorNodeComponent::ComputeFreeSpacePathLossDb(double FrequencyGHz, double DistanceMeters) const
{
    const double SafeDistanceKilometers = FMath::Max(DistanceMeters / 1000.0, 0.001);
    const double SafeFrequencyMegahertz = FMath::Max(FrequencyGHz * 1000.0, 0.001);
    return 32.44 + 20.0 * FMath::LogX(10.0, SafeDistanceKilometers) + 20.0 * FMath::LogX(10.0, SafeFrequencyMegahertz);
}

double UTRIADSensorNodeComponent::ComputeNoiseFloorDbm() const
{
    const double SafeBandwidthHz = FMath::Max(Definition.BandwidthMHz * 1000000.0, 1.0);
    return -174.0 + 10.0 * FMath::LogX(10.0, SafeBandwidthHz) + Definition.NoiseFigureDb;
}
