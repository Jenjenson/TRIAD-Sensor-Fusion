using UnrealBuildTool;

public class TRIADSensorFusionEditor : ModuleRules
{
    public TRIADSensorFusionEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "TRIADSensorFusion"
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "UnrealEd",
            "AssetTools",
            "AssetRegistry",
            "CesiumRuntime",
            "DerivedDataCache",
            "Json",
            "JsonUtilities",
            "MaterialEditor",
            "MeshBuilder",
            "MeshDescription",
            "MeshUtilities",
            "NaniteBuilder",
            "PhysicsCore",
            "RenderCore",
            "RHI",
            "SSL",
            "StaticMeshDescription",
            "TargetPlatform"
        });

        // UE 5.5's generic platform SHA-256 hook is not implemented on
        // Windows. V5C source admission uses the engine-bundled OpenSSL
        // dependency already used by S3Client and DerivedDataCache.
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}
