using UnrealBuildTool;
using System.IO;

public class TRIADSensorFusion : ModuleRules
{
    public TRIADSensorFusion(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // AirSimTriadRuntime keeps this exported legacy API under Source/Weather
        // instead of Public/Weather, so expose that sibling module root explicitly.
        PrivateIncludePaths.Add(Path.GetFullPath(Path.Combine(
            ModuleDirectory,
            "..", "..", "..", "AirSimTriadRuntime", "Source")));

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "CesiumRuntime",
            "AirSimTriadRuntime"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Json",
            "JsonUtilities",
            "InputCore",
            "Slate",
            "SlateCore"
        });
    }
}
