using UnrealBuildTool;
using System.IO;

public class TRIADSensorFusion : ModuleRules
{
    public TRIADSensorFusion(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // This module consumes AirLib headers through the AirSim runtime API.
        // Those headers contain real C++ exception handlers, so every target
        // compiling this consumer module must use the same exception model.
        bEnableExceptions = true;

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
            "HTTP",
            "InputCore",
            "PhysicsCore",
            "Projects",
            "Slate",
            "SlateCore"
        });

        // Exact hash-bound RF JSON remains NonUFS so the runtime CPU loader can
        // validate and read it from the plugin-relative path in packaged builds.
        string PluginDirectory = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", ".."));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "RF", "IstanaPublicViewRFTightV1.geometry.json"));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "RF", "istana_rf_materials_tight_v1.catalog.json"));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "RF", "istana_rf_scene_tight_v1.contract.json"));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "RF", "IstanaPublicViewRFOneKilometreV2.geometry.json"));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "RF", "istana_rf_materials_one_kilometre_v2.catalog.json"));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "RF", "istana_rf_scene_one_kilometre_v2.contract.json"));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "IstanaHighFidelityRF.example.json"));
        RuntimeDependencies.Add(Path.Combine(
            PluginDirectory,
            "Resources", "IstanaOneKilometreV2RF.example.json"));
    }
}
