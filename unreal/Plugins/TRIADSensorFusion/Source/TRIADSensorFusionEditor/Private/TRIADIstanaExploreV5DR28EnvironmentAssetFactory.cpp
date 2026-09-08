#include "TRIADIstanaExploreV5DR28EnvironmentAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PhysicsEngine/BodySetup.h"
#include "Ssl.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "UObject/UObjectGlobals.h"

#if WITH_SSL
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include <openssl/sha.h>
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#endif

namespace
{
const FString AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR28"));
const FString MeshRoot(AssetRoot + TEXT("/Meshes"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_R28_Surface_Master"));

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const FString MasterObjectPath(ObjectPath(MaterialRoot, MasterName));
const FString ConnectiveMeshName(
    TEXT("SM_IPV5D_R28_ConnectivePublicRealm_Render"));
const FString ArchitectureMeshName(
    TEXT("SM_IPV5D_R28_ContextArchitecturalDressing_Render"));
const FString ConnectiveMeshObjectPath(ObjectPath(MeshRoot, ConnectiveMeshName));
const FString ArchitectureMeshObjectPath(ObjectPath(MeshRoot, ArchitectureMeshName));
const FString ExistingOuterGroundMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/"
         "SM_IPV5D_OuterGroundLoadingFallback_Render."
         "SM_IPV5D_OuterGroundLoadingFallback_Render"));
const FString OuterGroundMaterialName(TEXT("MI_IPV5D_R28_OuterGround"));
const FString OuterGroundMaterialObjectPath(
    ObjectPath(MaterialRoot, OuterGroundMaterialName));

const FName TintParameter(TEXT("Tint"));
const FName RoughnessParameter(TEXT("Roughness"));
const FName SpecularParameter(TEXT("Specular"));
const FName MacroStrengthParameter(TEXT("MacroStrength"));
const FName MacroScaleParameter(TEXT("MacroScaleCm"));
const FName MicroStrengthParameter(TEXT("MicroStrength"));
const FName MicroScaleParameter(TEXT("MicroScaleCm"));
const FName JointStrengthParameter(TEXT("JointStrength"));
const FName JointSpacingParameter(TEXT("JointSpacingCm"));
const FName RoughnessVariationParameter(TEXT("RoughnessVariation"));
const FName GlazingGrazingStrengthParameter(TEXT("GlazingGrazingStrength"));
const FName MaterialParameterGroup(TEXT("R28 Visual Assumption"));

const FString SurfaceCode(
    TEXT("float macroScale=max(MacroScaleCm,1.0);"
         "float2 macroP=WorldPosition.xy/macroScale;"
         "float2 macroCell=floor(macroP);"
         "float2 macroBlend=frac(macroP);"
         "macroBlend=macroBlend*macroBlend*(3.0-2.0*macroBlend);"
         "float macroA=frac(sin(dot(macroCell,float2(12.9898,78.233)))*43758.5453);"
         "float macroB=frac(sin(dot(macroCell+float2(1.0,0.0),float2(12.9898,78.233)))*43758.5453);"
         "float macroC=frac(sin(dot(macroCell+float2(0.0,1.0),float2(12.9898,78.233)))*43758.5453);"
         "float macroD=frac(sin(dot(macroCell+float2(1.0,1.0),float2(12.9898,78.233)))*43758.5453);"
         "float macroNoise=lerp(lerp(macroA,macroB,macroBlend.x),lerp(macroC,macroD,macroBlend.x),macroBlend.y);"
         "float microScale=max(MicroScaleCm,1.0);"
         "float2 microP=WorldPosition.xy/microScale;"
         "float2 microCell=floor(microP);"
         "float2 microBlend=frac(microP);"
         "microBlend=microBlend*microBlend*(3.0-2.0*microBlend);"
         "float microA=frac(sin(dot(microCell,float2(39.3468,11.1351)))*24634.6345);"
         "float microB=frac(sin(dot(microCell+float2(1.0,0.0),float2(39.3468,11.1351)))*24634.6345);"
         "float microC=frac(sin(dot(microCell+float2(0.0,1.0),float2(39.3468,11.1351)))*24634.6345);"
         "float microD=frac(sin(dot(microCell+float2(1.0,1.0),float2(39.3468,11.1351)))*24634.6345);"
         "float microNoise=lerp(lerp(microA,microB,microBlend.x),lerp(microC,microD,microBlend.x),microBlend.y);"
         "float spacing=max(JointSpacingCm,1.0);"
         "float2 jointUv=frac(WorldPosition.xy/spacing);"
         "float2 edgeDistance=min(jointUv,1.0-jointUv)*spacing;"
         "float jointWidth=max(spacing*0.018,0.75);"
         "float jointMask=1.0-smoothstep(jointWidth,jointWidth*2.0,min(edgeDistance.x,edgeDistance.y));"
         "float variation=1.0+(macroNoise-0.5)*MacroStrength+(microNoise-0.5)*MicroStrength;"
         "float3 baseColor=Tint.rgb*variation*(1.0-JointStrength*jointMask);"
         "float glazingCue=saturate(Fresnel*GlazingGrazingStrength);"
         "return saturate(baseColor*(1.0+glazingCue*0.55)+glazingCue*float3(0.012,0.020,0.028));"));

const FString RoughnessCode(
    TEXT("float microScale=max(MicroScaleCm,1.0);"
         "float2 microP=WorldPosition.xy/microScale;"
         "float2 microCell=floor(microP);"
         "float2 microBlend=frac(microP);"
         "microBlend=microBlend*microBlend*(3.0-2.0*microBlend);"
         "float microA=frac(sin(dot(microCell,float2(17.9137,91.7281)))*31821.2467);"
         "float microB=frac(sin(dot(microCell+float2(1.0,0.0),float2(17.9137,91.7281)))*31821.2467);"
         "float microC=frac(sin(dot(microCell+float2(0.0,1.0),float2(17.9137,91.7281)))*31821.2467);"
         "float microD=frac(sin(dot(microCell+float2(1.0,1.0),float2(17.9137,91.7281)))*31821.2467);"
         "float microNoise=lerp(lerp(microA,microB,microBlend.x),lerp(microC,microD,microBlend.x),microBlend.y);"
         "float spacing=max(JointSpacingCm,1.0);"
         "float2 jointUv=frac(WorldPosition.xy/spacing);"
         "float2 edgeDistance=min(jointUv,1.0-jointUv)*spacing;"
         "float jointWidth=max(spacing*0.018,0.75);"
         "float jointMask=1.0-smoothstep(jointWidth,jointWidth*2.0,min(edgeDistance.x,edgeDistance.y));"
         "return saturate(Roughness+(microNoise-0.5)*RoughnessVariation+jointMask*JointStrength*0.12);"));

struct FMaterialSpec
{
    const TCHAR* Name;
    FLinearColor Tint;
    float Roughness;
    float Specular;
    float MacroStrength;
    float MacroScaleCm;
    float MicroStrength;
    float MicroScaleCm;
    float JointStrength;
    float JointSpacingCm;
    float RoughnessVariation;
    float GlazingGrazingStrength;
};

const FMaterialSpec MaterialSpecs[] = {
    {TEXT("MI_IPV5D_R28_GlassCool"), FLinearColor(0.022f, 0.052f, 0.071f), 0.24f, 0.62f, 0.035f, 145.0f, 0.015f, 42.0f, 0.0f, 100.0f, 0.025f, 0.32f},
    {TEXT("MI_IPV5D_R28_GlassWarm"), FLinearColor(0.072f, 0.055f, 0.038f), 0.31f, 0.48f, 0.045f, 155.0f, 0.018f, 44.0f, 0.0f, 100.0f, 0.025f, 0.26f},
    {TEXT("MI_IPV5D_R28_FrameLight"), FLinearColor(0.48f, 0.46f, 0.40f), 0.58f, 0.18f, 0.055f, 210.0f, 0.035f, 18.0f, 0.0f, 100.0f, 0.035f, 0.0f},
    {TEXT("MI_IPV5D_R28_FrameDark"), FLinearColor(0.065f, 0.075f, 0.080f), 0.49f, 0.28f, 0.045f, 180.0f, 0.032f, 18.0f, 0.0f, 100.0f, 0.035f, 0.0f},
    {TEXT("MI_IPV5D_R28_RoofTrim"), FLinearColor(0.13f, 0.14f, 0.145f), 0.78f, 0.15f, 0.08f, 290.0f, 0.06f, 36.0f, 0.02f, 160.0f, 0.04f, 0.0f},
    {TEXT("MI_IPV5D_R28_Asphalt"), FLinearColor(0.065f, 0.072f, 0.078f), 0.88f, 0.12f, 0.22f, 330.0f, 0.16f, 7.0f, 0.0f, 200.0f, 0.10f, 0.0f},
    {TEXT("MI_IPV5D_R28_Curb"), FLinearColor(0.36f, 0.37f, 0.35f), 0.82f, 0.14f, 0.12f, 185.0f, 0.10f, 9.0f, 0.04f, 100.0f, 0.07f, 0.0f},
    {TEXT("MI_IPV5D_R28_Sidewalk"), FLinearColor(0.30f, 0.29f, 0.26f), 0.84f, 0.13f, 0.16f, 210.0f, 0.12f, 11.0f, 0.18f, 120.0f, 0.08f, 0.0f},
    {TEXT("MI_IPV5D_R28_Verge"), FLinearColor(0.10f, 0.18f, 0.060f), 0.93f, 0.08f, 0.28f, 145.0f, 0.20f, 6.0f, 0.0f, 100.0f, 0.06f, 0.0f},
    {TEXT("MI_IPV5D_R28_OuterGround"), FLinearColor(0.105f, 0.165f, 0.060f), 0.93f, 0.10f, 0.20f, 2600.0f, 0.14f, 18.0f, 0.0f, 100.0f, 0.05f, 0.0f},
};

const TCHAR* const ArchitectureMaterialNames[] = {
    TEXT("MI_IPV5D_R28_GlassCool"),
    TEXT("MI_IPV5D_R28_GlassWarm"),
    TEXT("MI_IPV5D_R28_FrameLight"),
    TEXT("MI_IPV5D_R28_FrameDark"),
    TEXT("MI_IPV5D_R28_RoofTrim")};
const TCHAR* const PublicRealmMaterialNames[] = {
    TEXT("MI_IPV5D_R28_Asphalt"),
    TEXT("MI_IPV5D_R28_Curb"),
    TEXT("MI_IPV5D_R28_Sidewalk"),
    TEXT("MI_IPV5D_R28_Verge")};

struct FSourceSpec
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceSpec SourceSpecs[] = {
    {TEXT("Generated/SM_IPV5D_R28_ConnectivePublicRealm_Render.obj"), 12484549, TEXT("C8CD38ACB1C1730EA928FC31FC825C8803DF7B8F7E13878254776714AFE26842")},
    {TEXT("Generated/SM_IPV5D_R28_ContextArchitecturalDressing_Render.obj"), 59534571, TEXT("77E17482E73AB65222D5B0B0C70A407DB898F360FE1051BB338FDF31C0B88096")},
    {TEXT("Generated/IstanaPublicViewV5DR28EnvironmentalDressing.mtl"), 1233, TEXT("C237F52740740E95F510502F037D8C4CA9A3AA735C8EC47848DCB7B0D97CA946")},
    {TEXT("Generated/IstanaPublicViewV5DR28EnvironmentalDressing.manifest.json"), 20596, TEXT("3366F0B1D6C7A5801538B897488DCF506D1A1014AFB4E8676B64223420C8C641")},
    {TEXT("r28_environmental_dressing.contract.json"), 9317, TEXT("0CA25D72C3FAAEA2AF445E74ED374AD9AAA149DDC4BDF0691DC235435D9C8DF8")},
    {TEXT("../../../IstanaPublicView/Generated/SM_IstanaPublicView_Terrain.obj"), 3909911, TEXT("78AF53572EF53BACB684B5B2103D7BA427999C8223BDDD5C0DE76DEB8B16DD23")},
};

struct FMeshSpec
{
    const FString* Name;
    const FString* ObjectPath;
    const FSourceSpec* Source;
    const TCHAR* const* MaterialNames;
    int32 MaterialCount;
    int32 TriangleCount;
    int32 SourceCornerCount;
    bool bCastShadow;
};

const FMeshSpec ConnectiveSpec = {
    &ConnectiveMeshName,
    &ConnectiveMeshObjectPath,
    &SourceSpecs[0],
    PublicRealmMaterialNames,
    UE_ARRAY_COUNT(PublicRealmMaterialNames),
    32310,
    96930,
    false};
const FMeshSpec ArchitectureSpec = {
    &ArchitectureMeshName,
    &ArchitectureMeshObjectPath,
    &SourceSpecs[1],
    ArchitectureMaterialNames,
    UE_ARRAY_COUNT(ArchitectureMaterialNames),
    149758,
    449274,
    true};

FString SourceRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
             "R28EnvironmentalDressing")));
}

FString SourcePath(const TCHAR* RelativePath)
{
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(SourceRoot(), RelativePath));
}

template <typename T>
T* LoadExact(const FString& Path)
{
    T* Object = LoadObject<T>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

bool ValidateSource(const FSourceSpec& Spec, FString& OutError)
{
    const FString Filename = SourcePath(Spec.RelativePath);
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != Spec.Bytes)
    {
        OutError = FString::Printf(
            TEXT("R28 source byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            Spec.Bytes,
            Bytes.Num());
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) == nullptr ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper() != Spec.Sha256)
    {
        OutError = TEXT("R28 source SHA-256 guard failed for '") + Filename + TEXT("'.");
        return false;
    }
#else
    OutError = TEXT("R28 source admission requires WITH_SSL.");
    return false;
#endif
    return true;
}

bool ValidateSources(FString& OutError)
{
    for (const FSourceSpec& Spec : SourceSpecs)
    {
        if (!ValidateSource(Spec, OutError))
        {
            return false;
        }
    }
    return true;
}

template <typename T>
T* AddExpression(UMaterial* Material, const TCHAR* Description, int32 X, int32 Y)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    T* Expression = Data ? NewObject<T>(Material, NAME_None, RF_Transactional) : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
        Data->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterial* CreateMaster(IAssetTools& AssetTools, FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MasterName,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DR28EnvironmentAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Material || !Data || !Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the isolated R28 surface master.");
        return nullptr;
    }

    auto* Tint = AddExpression<UMaterialExpressionVectorParameter>(
        Material, TEXT("R28.Tint"), -740, -260);
    auto* Roughness = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.Roughness"), -740, -80);
    auto* Specular = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.Specular"), -740, 80);
    auto* MacroStrength = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.MacroStrength"), -740, 240);
    auto* MacroScale = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.MacroScaleCm"), -740, 400);
    auto* MicroStrength = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.MicroStrength"), -740, 560);
    auto* MicroScale = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.MicroScaleCm"), -740, 720);
    auto* JointStrength = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.JointStrength"), -740, 880);
    auto* JointSpacing = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.JointSpacingCm"), -740, 1040);
    auto* RoughnessVariation = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.RoughnessVariation"), -740, 1200);
    auto* GlazingGrazingStrength = AddExpression<UMaterialExpressionScalarParameter>(
        Material, TEXT("R28.GlazingGrazingStrength"), -740, 1360);
    auto* GlazingFresnel = AddExpression<UMaterialExpressionFresnel>(
        Material, TEXT("R28.BoundedOpaqueGlazingFresnel"), -740, 1520);
    auto* WorldPosition = AddExpression<UMaterialExpressionWorldPosition>(
        Material, TEXT("R28.WorldPosition"), -740, 1680);
    auto* Surface = AddExpression<UMaterialExpressionCustom>(
        Material, TEXT("R28.TextureFreeMacroSurface"), -350, -160);
    auto* SurfaceRoughness = AddExpression<UMaterialExpressionCustom>(
        Material, TEXT("R28.TextureFreeRoughnessResponse"), -350, 160);
    if (!Tint || !Roughness || !Specular || !MacroStrength || !MacroScale ||
        !MicroStrength || !MicroScale || !JointStrength || !JointSpacing ||
        !RoughnessVariation || !GlazingGrazingStrength || !GlazingFresnel ||
        !WorldPosition || !Surface || !SurfaceRoughness)
    {
        OutError = TEXT("Could not author the fifteen-node R28 surface graph.");
        return nullptr;
    }
    Tint->ParameterName = TintParameter;
    Tint->DefaultValue = FLinearColor(0.18f, 0.20f, 0.16f);
    Tint->Group = MaterialParameterGroup;
    Roughness->ParameterName = RoughnessParameter;
    Roughness->DefaultValue = 0.82f;
    Roughness->Group = MaterialParameterGroup;
    Specular->ParameterName = SpecularParameter;
    Specular->DefaultValue = 0.15f;
    Specular->Group = MaterialParameterGroup;
    MacroStrength->ParameterName = MacroStrengthParameter;
    MacroStrength->DefaultValue = 0.12f;
    MacroStrength->Group = MaterialParameterGroup;
    MacroScale->ParameterName = MacroScaleParameter;
    MacroScale->DefaultValue = 250.0f;
    MacroScale->Group = MaterialParameterGroup;
    MicroStrength->ParameterName = MicroStrengthParameter;
    MicroStrength->DefaultValue = 0.08f;
    MicroStrength->Group = MaterialParameterGroup;
    MicroScale->ParameterName = MicroScaleParameter;
    MicroScale->DefaultValue = 12.0f;
    MicroScale->Group = MaterialParameterGroup;
    JointStrength->ParameterName = JointStrengthParameter;
    JointStrength->DefaultValue = 0.0f;
    JointStrength->Group = MaterialParameterGroup;
    JointSpacing->ParameterName = JointSpacingParameter;
    JointSpacing->DefaultValue = 120.0f;
    JointSpacing->Group = MaterialParameterGroup;
    RoughnessVariation->ParameterName = RoughnessVariationParameter;
    RoughnessVariation->DefaultValue = 0.06f;
    RoughnessVariation->Group = MaterialParameterGroup;
    GlazingGrazingStrength->ParameterName = GlazingGrazingStrengthParameter;
    GlazingGrazingStrength->DefaultValue = 0.0f;
    GlazingGrazingStrength->Group = MaterialParameterGroup;
    GlazingFresnel->Exponent = 5.0f;
    GlazingFresnel->BaseReflectFraction = 0.04f;
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    Surface->Description = TEXT("Texture-free stable world-cell variation with microdetail, optional paving joints, and bounded opaque glazing cue; visual assumption only");
    Surface->Code = SurfaceCode;
    Surface->OutputType = CMOT_Float3;
    Surface->Inputs.SetNum(10);
    Surface->Inputs[0].InputName = TEXT("Tint");
    Surface->Inputs[0].Input.Connect(0, Tint);
    Surface->Inputs[1].InputName = TEXT("WorldPosition");
    Surface->Inputs[1].Input.Connect(0, WorldPosition);
    Surface->Inputs[2].InputName = TEXT("MacroStrength");
    Surface->Inputs[2].Input.Connect(0, MacroStrength);
    Surface->Inputs[3].InputName = TEXT("MacroScaleCm");
    Surface->Inputs[3].Input.Connect(0, MacroScale);
    Surface->Inputs[4].InputName = TEXT("MicroStrength");
    Surface->Inputs[4].Input.Connect(0, MicroStrength);
    Surface->Inputs[5].InputName = TEXT("MicroScaleCm");
    Surface->Inputs[5].Input.Connect(0, MicroScale);
    Surface->Inputs[6].InputName = TEXT("JointStrength");
    Surface->Inputs[6].Input.Connect(0, JointStrength);
    Surface->Inputs[7].InputName = TEXT("JointSpacingCm");
    Surface->Inputs[7].Input.Connect(0, JointSpacing);
    Surface->Inputs[8].InputName = TEXT("GlazingGrazingStrength");
    Surface->Inputs[8].Input.Connect(0, GlazingGrazingStrength);
    Surface->Inputs[9].InputName = TEXT("Fresnel");
    Surface->Inputs[9].Input.Connect(0, GlazingFresnel);
    SurfaceRoughness->Description = TEXT("Texture-free micro-roughness and paving-joint response; visual assumption only");
    SurfaceRoughness->Code = RoughnessCode;
    SurfaceRoughness->OutputType = CMOT_Float1;
    SurfaceRoughness->Inputs.SetNum(6);
    SurfaceRoughness->Inputs[0].InputName = TEXT("Roughness");
    SurfaceRoughness->Inputs[0].Input.Connect(0, Roughness);
    SurfaceRoughness->Inputs[1].InputName = TEXT("WorldPosition");
    SurfaceRoughness->Inputs[1].Input.Connect(0, WorldPosition);
    SurfaceRoughness->Inputs[2].InputName = TEXT("MicroScaleCm");
    SurfaceRoughness->Inputs[2].Input.Connect(0, MicroScale);
    SurfaceRoughness->Inputs[3].InputName = TEXT("RoughnessVariation");
    SurfaceRoughness->Inputs[3].Input.Connect(0, RoughnessVariation);
    SurfaceRoughness->Inputs[4].InputName = TEXT("JointStrength");
    SurfaceRoughness->Inputs[4].Input.Connect(0, JointStrength);
    SurfaceRoughness->Inputs[5].InputName = TEXT("JointSpacingCm");
    SurfaceRoughness->Inputs[5].Input.Connect(0, JointSpacing);

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;
    Material->bCastRayTracedShadows = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->PhysMaterial = nullptr;
    Material->PhysMaterialMask = nullptr;
    Material->RenderTracePhysicalMaterialOutputs.Reset();
    Data->BaseColor.Connect(0, Surface);
    Data->Roughness.Connect(0, SurfaceRoughness);
    Data->Specular.Connect(0, Specular);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return Material;
}

UMaterialInstanceConstant* CreateInstance(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    FString& OutError)
{
    UMaterialInstanceConstantFactoryNew* Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    if (Factory)
    {
        Factory->InitialParent = Parent;
    }
    UMaterialInstanceConstant* Instance = Factory
        ? Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
              Spec.Name,
              MaterialRoot,
              UMaterialInstanceConstant::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DR28EnvironmentAssets"))))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(TEXT("Could not create R28 material '%s'."), Spec.Name);
        return nullptr;
    }
    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    Instance->SetVectorParameterValueEditorOnly(
        FMaterialParameterInfo(TintParameter), Spec.Tint);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(RoughnessParameter), Spec.Roughness);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(SpecularParameter), Spec.Specular);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MacroStrengthParameter), Spec.MacroStrength);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MacroScaleParameter), Spec.MacroScaleCm);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MicroStrengthParameter), Spec.MicroStrength);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(MicroScaleParameter), Spec.MicroScaleCm);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(JointStrengthParameter), Spec.JointStrength);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(JointSpacingParameter), Spec.JointSpacingCm);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(RoughnessVariationParameter), Spec.RoughnessVariation);
    Instance->SetScalarParameterValueEditorOnly(
        FMaterialParameterInfo(GlazingGrazingStrengthParameter), Spec.GlazingGrazingStrength);
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    return Instance;
}

const FMaterialSpec* FindMaterialSpec(const FString& Name)
{
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        if (Name == Spec.Name)
        {
            return &Spec;
        }
    }
    return nullptr;
}

bool ValidateMaster(UMaterial* Material, FString& OutError)
{
    const UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Material || Material->GetPathName() != MasterObjectPath || !Data ||
        Material->MaterialDomain != MD_Surface || Material->BlendMode != BLEND_Opaque ||
        !Material->GetShadingModels().HasShadingModel(MSM_DefaultLit) ||
        Material->TwoSided || Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections || Material->PhysMaterial ||
        Material->PhysMaterialMask ||
        !Material->RenderTracePhysicalMaterialOutputs.IsEmpty() ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        Data->ExpressionCollection.Expressions.Num() != 15 ||
        !Data->BaseColor.Expression || !Data->Roughness.Expression ||
        !Data->Specular.Expression || Data->Normal.Expression ||
        Data->Opacity.Expression || Data->OpacityMask.Expression ||
        Data->Refraction.Expression || Data->WorldPositionOffset.Expression ||
        Data->PixelDepthOffset.Expression)
    {
        OutError = TEXT("R28 master lost its exact texture-free, opaque, displacement-free graph contract.");
        return false;
    }
    const UMaterialExpressionCustom* Surface = nullptr;
    const UMaterialExpressionCustom* SurfaceRoughness = nullptr;
    const UMaterialExpressionScalarParameter* GlazingGrazingStrength = nullptr;
    const UMaterialExpressionFresnel* GlazingFresnel = nullptr;
    int32 ScalarParameterCount = 0;
    int32 FresnelCount = 0;
    for (const UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        if (const auto* CustomExpression =
                Cast<UMaterialExpressionCustom>(Expression))
        {
            if (CustomExpression->Code == SurfaceCode)
            {
                Surface = CustomExpression;
            }
            else if (CustomExpression->Code == RoughnessCode)
            {
                SurfaceRoughness = CustomExpression;
            }
        }
        else if (const auto* ScalarParameter =
                     Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            ++ScalarParameterCount;
            if (ScalarParameter->ParameterName ==
                GlazingGrazingStrengthParameter)
            {
                GlazingGrazingStrength = ScalarParameter;
            }
        }
        else if (const auto* FresnelExpression =
                     Cast<UMaterialExpressionFresnel>(Expression))
        {
            ++FresnelCount;
            GlazingFresnel = FresnelExpression;
        }
    }
    if (!Surface || Surface->Inputs.Num() != 10 ||
        Surface->OutputType != CMOT_Float3 ||
        !SurfaceRoughness || SurfaceRoughness->Inputs.Num() != 6 ||
        SurfaceRoughness->OutputType != CMOT_Float1 ||
        ScalarParameterCount != 10 ||
        !GlazingGrazingStrength ||
        GlazingGrazingStrength->Desc != TEXT("R28.GlazingGrazingStrength") ||
        GlazingGrazingStrength->Group != MaterialParameterGroup ||
        !FMath::IsNearlyZero(GlazingGrazingStrength->DefaultValue) ||
        FresnelCount != 1 || !GlazingFresnel ||
        GlazingFresnel->Desc != TEXT("R28.BoundedOpaqueGlazingFresnel") ||
        GlazingFresnel->ExponentIn.Expression ||
        GlazingFresnel->BaseReflectFractionIn.Expression ||
        GlazingFresnel->Normal.Expression ||
        !FMath::IsNearlyEqual(GlazingFresnel->Exponent, 5.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            GlazingFresnel->BaseReflectFraction, 0.04f, 0.000001f) ||
        Surface->Inputs[8].InputName != GlazingGrazingStrengthParameter ||
        Surface->Inputs[8].Input.Expression != GlazingGrazingStrength ||
        Surface->Inputs[8].Input.OutputIndex != 0 ||
        Surface->Inputs[9].InputName != TEXT("Fresnel") ||
        Surface->Inputs[9].Input.Expression != GlazingFresnel ||
        Surface->Inputs[9].Input.OutputIndex != 0 ||
        Data->BaseColor.Expression != Surface ||
        Data->Roughness.Expression != SurfaceRoughness)
    {
        OutError = TEXT("R28 master multiscale surface or roughness response drifted.");
        return false;
    }
    return true;
}

bool ValidateInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialSpec& Spec,
    UMaterial* Master,
    FString& OutError)
{
    const bool bGlazingMaterial =
        FString(Spec.Name) == TEXT("MI_IPV5D_R28_GlassCool") ||
        FString(Spec.Name) == TEXT("MI_IPV5D_R28_GlassWarm");
    if (!Instance || Instance->GetPathName() != ObjectPath(MaterialRoot, Spec.Name) ||
        Instance->Parent != Master || Instance->ScalarParameterValues.Num() != 10 ||
        Instance->VectorParameterValues.Num() != 1 ||
        !Instance->TextureParameterValues.IsEmpty() ||
        (bGlazingMaterial &&
         (Spec.GlazingGrazingStrength <= 0.0f ||
          Spec.GlazingGrazingStrength > 0.35f)) ||
        (!bGlazingMaterial &&
         !FMath::IsNearlyZero(Spec.GlazingGrazingStrength)))
    {
        OutError = FString::Printf(TEXT("R28 material instance '%s' lost its exact texture-free policy."), Spec.Name);
        return false;
    }
    float Scalar = 0.0f;
    FLinearColor Tint;
    const auto ScalarMatches = [Instance, &Scalar](FName Name, float Expected)
    {
        return Instance->GetScalarParameterValue(
                   FHashedMaterialParameterInfo(Name), Scalar, true) &&
            FMath::IsNearlyEqual(Scalar, Expected, 0.000001f);
    };
    if (!Instance->GetVectorParameterValue(
            FHashedMaterialParameterInfo(TintParameter), Tint, true) ||
        !Tint.Equals(Spec.Tint, 0.000001f) ||
        !ScalarMatches(RoughnessParameter, Spec.Roughness) ||
        !ScalarMatches(SpecularParameter, Spec.Specular) ||
        !ScalarMatches(MacroStrengthParameter, Spec.MacroStrength) ||
        !ScalarMatches(MacroScaleParameter, Spec.MacroScaleCm) ||
        !ScalarMatches(MicroStrengthParameter, Spec.MicroStrength) ||
        !ScalarMatches(MicroScaleParameter, Spec.MicroScaleCm) ||
        !ScalarMatches(JointStrengthParameter, Spec.JointStrength) ||
        !ScalarMatches(JointSpacingParameter, Spec.JointSpacingCm) ||
        !ScalarMatches(RoughnessVariationParameter, Spec.RoughnessVariation) ||
        !ScalarMatches(
            GlazingGrazingStrengthParameter, Spec.GlazingGrazingStrength))
    {
        OutError = FString::Printf(TEXT("R28 material instance '%s' parameter values drifted."), Spec.Name);
        return false;
    }
    return true;
}

UAssetImportTask* MakeImportTask(const FMeshSpec& Spec)
{
    UFbxImportUI* Options = NewObject<UFbxImportUI>();
    UFbxFactory* Factory = NewObject<UFbxFactory>();
    UAssetImportTask* Task = NewObject<UAssetImportTask>();
    if (!Options || !Factory || !Task || !Options->StaticMeshImportData)
    {
        return nullptr;
    }
    Options->bImportAsSkeletal = false;
    Options->MeshTypeToImport = FBXIT_StaticMesh;
    Options->bAutomatedImportShouldDetectType = false;
    Options->bImportMesh = true;
    Options->bImportMaterials = false;
    Options->bImportTextures = false;
    Options->StaticMeshImportData->bConvertScene = false;
    Options->StaticMeshImportData->bConvertSceneUnit = false;
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->ImportUniformScale = 1.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = true;
    Options->StaticMeshImportData->bRemoveDegenerates = false;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = SourcePath(Spec.Source->RelativePath);
    Task->DestinationPath = MeshRoot;
    Task->DestinationName = *Spec.Name;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool NormalizeMesh(
    UStaticMesh* Mesh,
    const FMeshSpec& Spec,
    const TMap<FString, UMaterialInstanceConstant*>& Materials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FMeshDescription* Description = Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    if (!Mesh || Mesh->GetPathName() != *Spec.ObjectPath || !Description ||
        Description->Triangles().Num() != Spec.TriangleCount ||
        Description->VertexInstances().Num() != Spec.SourceCornerCount ||
        Mesh->GetStaticMaterials().Num() != Spec.MaterialCount)
    {
        OutError = TEXT("An imported R28 mesh lost its exact source topology or material roster.");
        return false;
    }

    TArray<FStaticMaterial> BoundMaterials;
    TSet<FString> SeenNames;
    for (const FStaticMaterial& Imported : Mesh->GetStaticMaterials())
    {
        const FString Name = Imported.ImportedMaterialSlotName.IsNone()
            ? Imported.MaterialSlotName.ToString()
            : Imported.ImportedMaterialSlotName.ToString();
        UMaterialInstanceConstant* const* Material = Materials.Find(Name);
        if (!Material || !*Material || !FindMaterialSpec(Name) || SeenNames.Contains(Name))
        {
            OutError = TEXT("An R28 OBJ material group did not match the isolated semantic material roster.");
            return false;
        }
        SeenNames.Add(Name);
        BoundMaterials.Emplace(*Material, FName(*Name), FName(*Name));
    }
    if (SeenNames.Num() != Spec.MaterialCount)
    {
        OutError = TEXT("An R28 mesh imported an incomplete semantic material roster.");
        return false;
    }

    Mesh->Modify();
    Mesh->SetStaticMaterials(BoundMaterials);
    FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
    SourceModel.BuildSettings.BuildScale3D = FVector::OneVector;
    SourceModel.BuildSettings.bGenerateLightmapUVs = false;
    SourceModel.BuildSettings.bUseFullPrecisionUVs = true;
    SourceModel.BuildSettings.bRecomputeNormals = false;
    SourceModel.BuildSettings.bRecomputeTangents = true;
    SourceModel.BuildSettings.bUseMikkTSpace = true;
    SourceModel.BuildSettings.bRemoveDegenerates = false;
    Mesh->SetLightMapCoordinateIndex(0);
    Mesh->bGenerateMeshDistanceField = false;
    Mesh->NaniteSettings.bEnabled = true;
    Mesh->NaniteSettings.KeepPercentTriangles = 1.0f;
    Mesh->NaniteSettings.TrimRelativeError = 0.0f;
    Mesh->NaniteSettings.FallbackTarget =
        ENaniteFallbackTarget::PercentTriangles;
    Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
    Mesh->NaniteSettings.FallbackRelativeError = 0.0f;
    for (int32 SectionIndex = 0; SectionIndex < Spec.MaterialCount; ++SectionIndex)
    {
        FMeshSectionInfo Section = Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original = Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        Section.bEnableCollision = false;
        Section.bCastShadow = Spec.bCastShadow;
        Section.bAffectDistanceFieldLighting = false;
        Original.bEnableCollision = false;
        Original.bCastShadow = Spec.bCastShadow;
        Original.bAffectDistanceFieldLighting = false;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    }
    Mesh->MarkAsNotHavingNavigationData();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    return true;
}

bool ValidateMesh(
    UStaticMesh* Mesh,
    const FMeshSpec& Spec,
    const TMap<FString, UMaterialInstanceConstant*>& Materials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FMeshDescription* Description = Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    int32 RenderTriangles = 0;
    if (RenderData && RenderData->LODResources.Num() == 1)
    {
        for (const FStaticMeshSection& Section : RenderData->LODResources[0].Sections)
        {
            RenderTriangles += Section.NumTriangles;
        }
    }
    if (!Mesh || Mesh->GetPathName() != *Spec.ObjectPath || !Description ||
        Description->Triangles().Num() != Spec.TriangleCount ||
        Description->VertexInstances().Num() != Spec.SourceCornerCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderTriangles != Spec.TriangleCount ||
        Mesh->GetStaticMaterials().Num() != Spec.MaterialCount ||
        !Mesh->NaniteSettings.bEnabled || !Mesh->HasValidNaniteData() ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError) ||
        Mesh->bHasNavigationData ||
        Mesh->GetNavCollision() ||
        !Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag == CTF_UseComplexAsSimple)
    {
        OutError = FString::Printf(
            TEXT("R28 mesh '%s' lost its exact full-topology Nanite/raster, no-collision, no-navigation contract."),
            Spec.Name);
        return false;
    }
    TSet<FString> Seen;
    for (const FStaticMaterial& Slot : Mesh->GetStaticMaterials())
    {
        const FString Name = Slot.MaterialSlotName.ToString();
        UMaterialInstanceConstant* const* Expected = Materials.Find(Name);
        if (!Expected || Slot.ImportedMaterialSlotName != Slot.MaterialSlotName ||
            Slot.MaterialInterface != *Expected || Seen.Contains(Name))
        {
            OutError = TEXT("An R28 mesh semantic material binding drifted.");
            return false;
        }
        Seen.Add(Name);
    }
    return Seen.Num() == Spec.MaterialCount;
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets)
{
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
        TEXT("AssetRegistry"));
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false);
    return true;
}

bool LoadAndValidateMaterials(
    UMaterial*& OutMaster,
    TMap<FString, UMaterialInstanceConstant*>& OutMaterials,
    FString& OutError)
{
    OutMaster = LoadExact<UMaterial>(MasterObjectPath);
    if (!ValidateMaster(OutMaster, OutError))
    {
        return false;
    }
    OutMaterials.Reset();
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance =
            LoadExact<UMaterialInstanceConstant>(ObjectPath(MaterialRoot, Spec.Name));
        if (!ValidateInstance(Instance, Spec, OutMaster, OutError))
        {
            return false;
        }
        OutMaterials.Add(Spec.Name, Instance);
    }
    return true;
}

bool ValidateInternal(FString& OutReport)
{
    if (!ValidateSources(OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<FAssetData> RootAssets;
    GatherRootAssets(RootAssets);
    constexpr int32 ExpectedAssetCount = 1 + UE_ARRAY_COUNT(MaterialSpecs) + 2;
    if (RootAssets.Num() != ExpectedAssetCount)
    {
        OutReport = FString::Printf(
            TEXT("R28 isolated asset root roster changed: expected=%d actual=%d."),
            ExpectedAssetCount,
            RootAssets.Num());
        return false;
    }
    UMaterial* Master = nullptr;
    TMap<FString, UMaterialInstanceConstant*> Materials;
    if (!LoadAndValidateMaterials(Master, Materials, OutReport) ||
        !ValidateMesh(LoadExact<UStaticMesh>(ConnectiveMeshObjectPath), ConnectiveSpec, Materials, OutReport) ||
        !ValidateMesh(LoadExact<UStaticMesh>(ArchitectureMeshObjectPath), ArchitectureSpec, Materials, OutReport) ||
        !LoadExact<UStaticMesh>(ExistingOuterGroundMeshObjectPath))
    {
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_ASSETS_VALID assets=13 meshes=2 materials=11 architectureTriangles=149758 architectureSourceCorners=449274 architectureWindows=14786 connectivePublicRealmTriangles=32310 priorityBuildingParts=40 priorityBuildingCoverageComplete=true baselineBuildingParts=85 retainedBaselineBuildingParts=41 baselineBuildingPartsNotSelected=44 selectedBuildingParts=128 newlySelectedBuildingParts=87 evidenceCameraCount=4 evidenceCameraUnionParts=86 minimumEvidenceCameraSelectedParts=42 minimumEvidenceCameraProxyCoverage=0.443796940 priorityStreetscapeCameraCount=2 priorityStreetscapeCameraUnionParts=69 minimumPriorityStreetscapeCameraSelectedParts=65 minimumPriorityStreetscapeCameraProxyCoverage=0.721584324 omnidirectionalRadialBands=3 omnidirectionalAzimuthSectors=8 omnidirectionalSectorsMeetingQuota=24 selectedBuildingPartsRing300To500=25 selectedBuildingPartsRing500To750=39 selectedBuildingPartsRing750To995=64 architecturePartCeiling=128 architectureWindowCeiling=15000 architectureTriangleCeiling=150000 architectureSourceCornerCeiling=450000 publicRoadSegments=1923 terrainDrapeSourcePinned=true terrainSamplesResolved=64620 terrainSamplesUnresolved=0 proceduralMicroSurface=true sidewalkJointCues=true roughnessVariation=true viewDependentGlazingCue=true opaqueGlazing=true glazingTransparencyClaimed=false screenSpaceReflections=false textureInputs=0 runtimeGeometry=false collision=false navigation=false existingSurroundingsPublicRealmOuterGroundRfInputsModified=false.");
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DR28EnvironmentAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }
const FString& GetConnectivePublicRealmMeshObjectPath() { return ConnectiveMeshObjectPath; }
const FString& GetContextArchitecturalDressingMeshObjectPath() { return ArchitectureMeshObjectPath; }
const FString& GetOuterGroundMaterialObjectPath() { return OuterGroundMaterialObjectPath; }
const TArray<FString>& GetExpectedAssetObjectPaths()
{
    static const TArray<FString> Paths = []
    {
        TArray<FString> Result = {
            MasterObjectPath,
            ConnectiveMeshObjectPath,
            ArchitectureMeshObjectPath};
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            Result.Add(ObjectPath(MaterialRoot, Spec.Name));
        }
        Result.Sort();
        return Result;
    }();
    return Paths;
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    FString ExistingReport;
    if (ValidateInternal(ExistingReport))
    {
        UMaterial* Master = LoadExact<UMaterial>(MasterObjectPath);
        OutAssets.Add(Master);
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            OutAssets.Add(LoadExact<UMaterialInstanceConstant>(
                ObjectPath(MaterialRoot, Spec.Name)));
        }
        OutAssets.Add(LoadExact<UStaticMesh>(ConnectiveMeshObjectPath));
        OutAssets.Add(LoadExact<UStaticMesh>(ArchitectureMeshObjectPath));
        OutError = ExistingReport;
        return true;
    }
    if (!ValidateSources(OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    GatherRootAssets(Existing);
    if (!Existing.IsEmpty())
    {
        OutError = TEXT("R28 creation refuses an invalid or partially populated isolated asset root.");
        return false;
    }

    IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(
        TEXT("AssetTools")).Get();
    UMaterial* Master = CreateMaster(AssetTools, OutError);
    if (!Master)
    {
        return false;
    }
    OutAssets.Add(Master);
    TMap<FString, UMaterialInstanceConstant*> Materials;
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance =
            CreateInstance(AssetTools, Spec, Master, OutError);
        if (!Instance)
        {
            return false;
        }
        Materials.Add(Spec.Name, Instance);
        OutAssets.Add(Instance);
    }
    UAssetImportTask* ConnectiveTask = MakeImportTask(ConnectiveSpec);
    UAssetImportTask* ArchitectureTask = MakeImportTask(ArchitectureSpec);
    if (!ConnectiveTask || !ArchitectureTask)
    {
        OutError = TEXT("Could not allocate the exact two R28 OBJ import tasks.");
        return false;
    }
    AssetTools.ImportAssetTasks({ConnectiveTask, ArchitectureTask});
    auto ImportedMesh = [](UAssetImportTask* Task, const FString& ExpectedPath)
    {
        for (UObject* Object : Task->GetObjects())
        {
            if (Object && Object->GetPathName() == ExpectedPath)
            {
                return Cast<UStaticMesh>(Object);
            }
        }
        return LoadExact<UStaticMesh>(ExpectedPath);
    };
    UStaticMesh* Connective = ImportedMesh(ConnectiveTask, ConnectiveMeshObjectPath);
    UStaticMesh* Architecture = ImportedMesh(ArchitectureTask, ArchitectureMeshObjectPath);
    if (!NormalizeMesh(Connective, ConnectiveSpec, Materials, OutError) ||
        !NormalizeMesh(Architecture, ArchitectureSpec, Materials, OutError))
    {
        return false;
    }
    OutAssets.Add(Connective);
    OutAssets.Add(Architecture);
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString Report;
    if (!ValidateInternal(Report))
    {
        OutError = TEXT("Fresh R28 assets failed exact validation: ") + Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(OutReport);
}

bool LoadValidatedRuntimeContract(
    FTRIADIstanaExploreV5DR28EnvironmentAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DR28EnvironmentAssets{};
    if (!ValidateInternal(OutError))
    {
        return false;
    }
    OutAssets.ConnectivePublicRealmMesh =
        LoadExact<UStaticMesh>(ConnectiveMeshObjectPath);
    OutAssets.ContextArchitecturalDressingMesh =
        LoadExact<UStaticMesh>(ArchitectureMeshObjectPath);
    OutAssets.OuterGroundMesh =
        LoadExact<UStaticMesh>(ExistingOuterGroundMeshObjectPath);
    OutAssets.OuterGroundMaterial =
        LoadExact<UMaterialInterface>(OuterGroundMaterialObjectPath);
    if (!OutAssets.ConnectivePublicRealmMesh ||
        !OutAssets.ContextArchitecturalDressingMesh ||
        !OutAssets.OuterGroundMesh || !OutAssets.OuterGroundMaterial)
    {
        OutAssets = FTRIADIstanaExploreV5DR28EnvironmentAssets{};
        OutError = TEXT("Validated R28 runtime assets could not be loaded exactly.");
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace TRIADIstanaExploreV5DR28EnvironmentAssetFactory
