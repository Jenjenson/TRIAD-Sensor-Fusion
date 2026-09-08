#include "TRIADIstanaExploreV5DFountainMaterialFactory.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Editor.h"
#include "Factories/MaterialFactoryNew.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PackageTools.h"
#include "RHIFeatureLevel.h"
#include "ShaderCompiler.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <initializer_list>

namespace
{
const FString MaterialRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/FountainRealism/Materials"));
const FString WaterAssetName(TEXT("M_IPV5D_FountainWater"));
const FString SprayAssetName(TEXT("M_IPV5D_FountainSpray"));
const FString EmbeddedWaterSuppressorAssetName(
    TEXT("M_IPV5D_FountainEmbeddedWaterSuppressor"));
const FName AssetCreateContext(
    TEXT("TRIAD.CreateIstanaExploreV5DFountainMaterials"));
const FName ParameterGroup(TEXT("Istana Explore V5D Fountain"));

FString ObjectPath(const FString& AssetName)
{
    return MaterialRoot + TEXT("/") + AssetName + TEXT(".") + AssetName;
}

FString PackagePath(const FString& AssetName)
{
    return MaterialRoot + TEXT("/") + AssetName;
}

const FString WaterObjectPath = ObjectPath(WaterAssetName);
const FString SprayObjectPath = ObjectPath(SprayAssetName);
const FString EmbeddedWaterSuppressorObjectPath =
    ObjectPath(EmbeddedWaterSuppressorAssetName);

const TArray<FString>& R4AssetNames()
{
    static const TArray<FString> Names = {WaterAssetName, SprayAssetName};
    return Names;
}

const TArray<FString>& R5AssetNames()
{
    static const TArray<FString> Names = {
        WaterAssetName,
        SprayAssetName,
        EmbeddedWaterSuppressorAssetName};
    return Names;
}

const TArray<FString>& SuppressorAssetNames()
{
    static const TArray<FString> Names = {
        EmbeddedWaterSuppressorAssetName};
    return Names;
}

constexpr float WaterRoughness = 0.18f;
constexpr float WaterSpecular = 0.50f;
const FLinearColor WaterTint(0.018f, 0.045f, 0.055f, 1.0f);

constexpr float SprayDepthFadeCm = 6.0f;
constexpr float SprayMaxOpacity = 0.07f;
const FLinearColor SprayTint(0.62f, 0.70f, 0.74f, 1.0f);

const FString WaterNormalDescription(
    TEXT("TRIAD_IPV5D_WORLD_XY_CALM_WORLDSPACE_DUAL_WAVE_NORMAL_R4"));
const FString WaterNormalCode(
    TEXT("const float Tau = 6.28318530718;\n")
    TEXT("float2 p = AbsoluteWorldPosition.xy * 0.01;\n")
    TEXT("float2 d0 = normalize(float2(0.93969262, 0.34202014));\n")
    TEXT("float2 d1 = normalize(float2(-0.54463904, 0.83867057));\n")
    TEXT("float a = Tau * (dot(p, d0) / 3.40 + TimeSeconds * 0.055);\n")
    TEXT("float b = Tau * (dot(p, d1) / 1.20 - TimeSeconds * 0.11);\n")
    TEXT("float2 slope = 0.0020 * cos(a) * d0 + 0.0010 * cos(b) * d1;\n")
    TEXT("return normalize(float3(-slope.x, -slope.y, 1.0));"));

const FString SprayOpacityDescription(
    TEXT("TRIAD_IPV5D_SPRAY_UNLIT_SPARSE_BREAKUP_R4"));
const FString SprayOpacityCode(
    TEXT("float streak = 0.5 + 0.5 * sin((UV.y * 18.0 - TimeSeconds * 1.7) * 6.28318530718 + sin(UV.x * 6.28318530718) * 2.2);\n")
    TEXT("float breakup = 0.5 + 0.5 * sin((UV.x * 11.0 + UV.y * 7.0 + TimeSeconds * 0.9) * 6.28318530718);\n")
    TEXT("float strand = smoothstep(0.72, 0.94, streak * breakup);\n")
    TEXT("float axial = smoothstep(0.03, 0.12, UV.y) * (1.0 - smoothstep(0.84, 1.0, UV.y));\n")
    TEXT("return 0.07 * strand * axial;"));

const TCHAR* const ArtifactExtensions[] = {
    TEXT(".uasset"),
    TEXT(".uexp"),
    TEXT(".ubulk"),
    TEXT(".uptnl"),
    TEXT(".m.ubulk"),
    TEXT(".upayload")};

template <typename T>
T* LoadExact(const FString& ExactObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ExactObjectPath, nullptr, LOAD_NoRedirects);
    return Object && Object->GetPathName() == ExactObjectPath ? Object : nullptr;
}

template <typename T>
T* AddExpression(
    UMaterial* Material,
    const TCHAR* NodeId,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    T* Expression = Data
        ? NewObject<T>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = NodeId;
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
        Data->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpressionDepthFade* AddDepthFade(
    UMaterial* Material,
    const TCHAR* NodeId,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UClass* DepthFadeClass = LoadObject<UClass>(
        nullptr,
        TEXT("/Script/Engine.MaterialExpressionDepthFade"));
    UMaterialExpression* Expression = Data && DepthFadeClass &&
            DepthFadeClass->IsChildOf(UMaterialExpression::StaticClass())
        ? NewObject<UMaterialExpression>(
              Material,
              DepthFadeClass,
              NAME_None,
              RF_Transactional)
        : nullptr;
    if (!Expression)
    {
        return nullptr;
    }
    Expression->Desc = NodeId;
    Expression->MaterialExpressionEditorX = EditorX;
    Expression->MaterialExpressionEditorY = EditorY;
    Data->ExpressionCollection.Expressions.Add(Expression);
    // UE 5.5 does not export DepthFade's StaticClass symbol. The reflected
    // class/IsChildOf gate above makes this layout-only cast safe.
    return static_cast<UMaterialExpressionDepthFade*>(Expression);
}

UMaterialExpressionScalarParameter* AddScalar(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    float Value,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material,
            NodeId,
            EditorX,
            EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = Value;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionVectorParameter* AddVector(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    const FLinearColor& Value,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionVectorParameter* Parameter =
        AddExpression<UMaterialExpressionVectorParameter>(
            Material,
            NodeId,
            EditorX,
            EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = Value;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionCustom* AddCustom(
    UMaterial* Material,
    const TCHAR* NodeId,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    std::initializer_list<const TCHAR*> InputNames,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionCustom* Custom = AddExpression<UMaterialExpressionCustom>(
        Material,
        NodeId,
        EditorX,
        EditorY);
    if (!Custom)
    {
        return nullptr;
    }
    Custom->Description = Description;
    Custom->Code = Code;
    Custom->OutputType = OutputType;
    Custom->Inputs.Reset(static_cast<int32>(InputNames.size()));
    for (const TCHAR* InputName : InputNames)
    {
        FCustomInput& Input = Custom->Inputs.AddDefaulted_GetRef();
        Input.InputName = InputName;
    }
    Custom->AdditionalOutputs.Reset();
    Custom->AdditionalDefines.Reset();
    Custom->IncludeFilePaths.Reset();
    return Custom;
}

UMaterial* CreateEmptyMaterial(
    IAssetTools& AssetTools,
    const FString& AssetName,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              AssetName,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              AssetCreateContext))
        : nullptr;
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data ||
        !Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create an exact empty V5D fountain material: ") +
            AssetName;
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->bUseMaterialAttributes = false;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    return Material;
}

void FinalizeMaterial(UMaterial* Material)
{
    if (!Material)
    {
        return;
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
}

UMaterial* CreateWaterMaterial(IAssetTools& AssetTools, FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(
        AssetTools,
        WaterAssetName,
        OutError);
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material,
            TEXT("V5D.Water.WorldPositionNoOffsets"),
            -900,
            -120);
    UMaterialExpressionTime* Time = AddExpression<UMaterialExpressionTime>(
        Material,
        TEXT("V5D.Water.TimeSeconds"),
        -900,
        10);
    UMaterialExpressionCustom* Normal = AddCustom(
        Material,
        TEXT("V5D.Water.WorldXyDualWaveNormal"),
        WaterNormalDescription,
        WaterNormalCode,
        CMOT_Float3,
        {TEXT("AbsoluteWorldPosition"), TEXT("TimeSeconds")},
        -650,
        -70);
    UMaterialExpressionVectorParameter* Tint = AddVector(
        Material,
        TEXT("V5D.Water.Tint"),
        TEXT("WaterTint"),
        WaterTint,
        -380,
        -310);
    UMaterialExpressionScalarParameter* Roughness = AddScalar(
        Material,
        TEXT("V5D.Water.Roughness"),
        TEXT("Roughness"),
        WaterRoughness,
        -380,
        -190);
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material,
        TEXT("V5D.Water.Specular"),
        TEXT("Specular"),
        WaterSpecular,
        -380,
        -70);
    if (!Data || !WorldPosition || !Time || !Normal || !Tint ||
        !Roughness || !Specular)
    {
        OutError = TEXT("Could not allocate the exact V5D fountain-water graph.");
        return nullptr;
    }

    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    Time->bIgnorePause = false;
    Time->bOverride_Period = false;
    Time->Period = 0.0f;
    Normal->Inputs[0].Input.Connect(0, WorldPosition);
    Normal->Inputs[1].Input.Connect(0, Time);
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = false;
    Material->bScreenSpaceReflections = false;
    Data->BaseColor.Connect(0, Tint);
    Data->Roughness.Connect(0, Roughness);
    Data->Specular.Connect(0, Specular);
    Data->Normal.Connect(0, Normal);
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

UMaterial* CreateSprayMaterial(IAssetTools& AssetTools, FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(
        AssetTools,
        SprayAssetName,
        OutError);
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionTextureCoordinate* Uv =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material,
            TEXT("V5D.Spray.UV0"),
            -880,
            30);
    UMaterialExpressionTime* Time = AddExpression<UMaterialExpressionTime>(
        Material,
        TEXT("V5D.Spray.TimeSeconds"),
        -880,
        150);
    UMaterialExpressionCustom* Opacity = AddCustom(
        Material,
        TEXT("V5D.Spray.ZeroFloorBreakup"),
        SprayOpacityDescription,
        SprayOpacityCode,
        CMOT_Float1,
        {TEXT("UV"), TEXT("TimeSeconds")},
        -620,
        80);
    UMaterialExpressionVectorParameter* Tint = AddVector(
        Material,
        TEXT("V5D.Spray.Tint"),
        TEXT("SprayTint"),
        SprayTint,
        -350,
        -220);
    UMaterialExpressionScalarParameter* FadeDistance = AddScalar(
        Material,
        TEXT("V5D.Spray.DepthFadeDistanceCm"),
        TEXT("DepthFadeDistanceCm"),
        SprayDepthFadeCm,
        -350,
        220);
    UMaterialExpressionDepthFade* DepthFade = AddDepthFade(
        Material,
        TEXT("V5D.Spray.DepthFade6Cm"),
        -80,
        100);
    if (!Data || !Uv || !Time || !Opacity || !Tint || !FadeDistance ||
        !DepthFade)
    {
        OutError = TEXT("Could not allocate the exact V5D fountain-spray graph.");
        return nullptr;
    }

    Uv->CoordinateIndex = 0;
    Uv->UTiling = 1.0f;
    Uv->VTiling = 1.0f;
    Time->bIgnorePause = false;
    Time->bOverride_Period = false;
    Time->Period = 0.0f;
    Opacity->Inputs[0].Input.Connect(0, Uv);
    Opacity->Inputs[1].Input.Connect(0, Time);
    DepthFade->InOpacity.Connect(0, Opacity);
    DepthFade->FadeDistance.Connect(0, FadeDistance);
    DepthFade->OpacityDefault = SprayMaxOpacity;
    DepthFade->FadeDistanceDefault = SprayDepthFadeCm;

    Material->BlendMode = BLEND_Additive;
    Material->TwoSided = false;
    Material->SetShadingModel(MSM_Unlit);
    Material->bTangentSpaceNormal = false;
    Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
    Material->RefractionMethod = RM_PixelNormalOffset;
    Material->bScreenSpaceReflections = false;
    Data->EmissiveColor.Connect(0, Tint);
    Data->Opacity.Connect(0, DepthFade);
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

UMaterial* CreateEmbeddedWaterSuppressorMaterial(
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(
        AssetTools,
        EmbeddedWaterSuppressorAssetName,
        OutError);
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Data || !Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not allocate the exact zero-output V5D embedded-water suppressor graph.");
        return nullptr;
    }

    Material->BlendMode = BLEND_Additive;
    Material->TwoSided = false;
    Material->SetShadingModel(MSM_Unlit);
    Material->bTangentSpaceNormal = false;
    Material->TranslucencyLightingMode = TLM_VolumetricNonDirectional;
    Material->RefractionMethod = RM_PixelNormalOffset;
    Material->bScreenSpaceReflections = false;
    Data->EmissiveColor.UseConstant = 1;
    Data->EmissiveColor.Constant = FColor::Black;
    Data->Opacity.UseConstant = 1;
    Data->Opacity.Constant = 0.0f;
    // The empty expression collection and explicit constant-zero outputs
    // intentionally compile to zero emissive and opacity contribution. This
    // is an exact render-only suppressor, not an approximation of water.
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

bool InputIs(
    const FExpressionInput& Input,
    const UMaterialExpression* Expression,
    int32 OutputIndex = 0)
{
    return Input.Expression == Expression && Input.OutputIndex == OutputIndex;
}

template <typename T>
const T* FindExactNode(
    const UMaterialEditorOnlyData* Data,
    const TCHAR* NodeId)
{
    const T* Match = nullptr;
    if (!Data)
    {
        return nullptr;
    }
    for (const UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        if (Expression && Expression->Desc == NodeId)
        {
            if (Match || !Cast<T>(Expression))
            {
                return nullptr;
            }
            Match = Cast<T>(Expression);
        }
    }
    return Match;
}

const UMaterialExpressionDepthFade* FindExactDepthFadeNode(
    const UMaterialEditorOnlyData* Data,
    const TCHAR* NodeId)
{
    const UMaterialExpressionDepthFade* Match = nullptr;
    if (!Data)
    {
        return nullptr;
    }
    for (const UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        if (!Expression || Expression->Desc != NodeId)
        {
            continue;
        }
        if (Match || !Expression->GetClass() ||
            Expression->GetClass()->GetPathName() !=
                TEXT("/Script/Engine.MaterialExpressionDepthFade"))
        {
            return nullptr;
        }
        Match = static_cast<const UMaterialExpressionDepthFade*>(Expression);
    }
    return Match;
}

bool ScalarIs(
    const UMaterialExpressionScalarParameter* Parameter,
    const TCHAR* Name,
    float Value)
{
    return Parameter && Parameter->ParameterName == Name &&
        Parameter->Group == ParameterGroup &&
        FMath::IsNearlyEqual(Parameter->DefaultValue, Value, 0.000001f) &&
        !Parameter->bUseCustomPrimitiveData &&
        Parameter->PrimitiveDataIndex == 0;
}

bool VectorIs(
    const UMaterialExpressionVectorParameter* Parameter,
    const TCHAR* Name,
    const FLinearColor& Value)
{
    return Parameter && Parameter->ParameterName == Name &&
        Parameter->Group == ParameterGroup &&
        Parameter->DefaultValue.Equals(Value, 0.000001f) &&
        !Parameter->bUseCustomPrimitiveData &&
        Parameter->PrimitiveDataIndex == 0;
}

bool HasNoCustomizedUvConnections(const UMaterialEditorOnlyData* Data)
{
    if (!Data)
    {
        return false;
    }
    for (const FVector2MaterialInput& Input : Data->CustomizedUVs)
    {
        if (Input.Expression)
        {
            return false;
        }
    }
    return true;
}

bool HasNoGeometryMutationPath(
    const UMaterial* Material,
    const UMaterialEditorOnlyData* Data)
{
    return Material && Data && !Material->bEnableTessellation &&
        !Material->bEnableDisplacementFade &&
        FMath::IsNearlyZero(
            Material->MaxWorldPositionOffsetDisplacement,
            0.000001f) &&
        !Data->WorldPositionOffset.Expression &&
        !Data->Displacement.Expression &&
        !Data->PixelDepthOffset.Expression &&
        !Data->MaterialAttributes.Expression &&
        !Data->FrontMaterial.Expression &&
        HasNoCustomizedUvConnections(Data);
}

bool CustomIsExact(
    const UMaterialExpressionCustom* Custom,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    std::initializer_list<const TCHAR*> InputNames,
    std::initializer_list<const UMaterialExpression*> InputExpressions)
{
    if (!Custom || Custom->Description != Description ||
        Custom->Code != Code || Custom->OutputType != OutputType ||
        Custom->AdditionalOutputs.Num() != 0 ||
        Custom->AdditionalDefines.Num() != 0 ||
        Custom->IncludeFilePaths.Num() != 0 ||
        Custom->Inputs.Num() != static_cast<int32>(InputNames.size()) ||
        InputNames.size() != InputExpressions.size())
    {
        return false;
    }
    auto ExpressionIt = InputExpressions.begin();
    int32 Index = 0;
    for (const TCHAR* InputName : InputNames)
    {
        if (Custom->Inputs[Index].InputName != InputName ||
            !InputIs(Custom->Inputs[Index].Input, *ExpressionIt))
        {
            return false;
        }
        ++Index;
        ++ExpressionIt;
    }
    return true;
}

bool ValidateWaterGraph(UMaterial* Material, FString& OutError)
{
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const auto* WorldPosition = FindExactNode<UMaterialExpressionWorldPosition>(
        Data,
        TEXT("V5D.Water.WorldPositionNoOffsets"));
    const auto* Time = FindExactNode<UMaterialExpressionTime>(
        Data,
        TEXT("V5D.Water.TimeSeconds"));
    const auto* Normal = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("V5D.Water.WorldXyDualWaveNormal"));
    const auto* Tint = FindExactNode<UMaterialExpressionVectorParameter>(
        Data,
        TEXT("V5D.Water.Tint"));
    const auto* Roughness = FindExactNode<UMaterialExpressionScalarParameter>(
        Data,
        TEXT("V5D.Water.Roughness"));
    const auto* Specular = FindExactNode<UMaterialExpressionScalarParameter>(
        Data,
        TEXT("V5D.Water.Specular"));
    if (!Material || !Data ||
        Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != WaterObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Material->bScreenSpaceReflections ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !HasNoGeometryMutationPath(Material, Data) ||
        Data->ExpressionCollection.Expressions.Num() != 6 ||
        !WorldPosition ||
        WorldPosition->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        !Time || Time->bIgnorePause || Time->bOverride_Period ||
        !FMath::IsNearlyZero(Time->Period, 0.000001f) ||
        !CustomIsExact(
            Normal,
            WaterNormalDescription,
            WaterNormalCode,
            CMOT_Float3,
            {TEXT("AbsoluteWorldPosition"), TEXT("TimeSeconds")},
            {WorldPosition, Time}) ||
        !VectorIs(Tint, TEXT("WaterTint"), WaterTint) ||
        !ScalarIs(Roughness, TEXT("Roughness"), WaterRoughness) ||
        !ScalarIs(Specular, TEXT("Specular"), WaterSpecular) ||
        !InputIs(Data->BaseColor, Tint) ||
        !InputIs(Data->Roughness, Roughness) ||
        !InputIs(Data->Specular, Specular) ||
        !InputIs(Data->Normal, Normal) ||
        Data->Opacity.Expression || Data->Refraction.Expression ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->EmissiveColor.Expression || Data->OpacityMask.Expression ||
        Data->AmbientOcclusion.Expression ||
        Data->SubsurfaceColor.Expression || Data->ClearCoat.Expression ||
        Data->ClearCoatRoughness.Expression ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression)
    {
        OutError = TEXT("The V5D fountain-water graph lost its exact opaque calm world-XY world-space dual-wave, SSR-off, or no-geometry-mutation contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSprayGraph(UMaterial* Material, FString& OutError)
{
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const auto* Uv = FindExactNode<UMaterialExpressionTextureCoordinate>(
        Data,
        TEXT("V5D.Spray.UV0"));
    const auto* Time = FindExactNode<UMaterialExpressionTime>(
        Data,
        TEXT("V5D.Spray.TimeSeconds"));
    const auto* Opacity = FindExactNode<UMaterialExpressionCustom>(
        Data,
        TEXT("V5D.Spray.ZeroFloorBreakup"));
    const auto* Tint = FindExactNode<UMaterialExpressionVectorParameter>(
        Data,
        TEXT("V5D.Spray.Tint"));
    const auto* FadeDistance = FindExactNode<UMaterialExpressionScalarParameter>(
        Data,
        TEXT("V5D.Spray.DepthFadeDistanceCm"));
    const auto* DepthFade = FindExactDepthFadeNode(
        Data,
        TEXT("V5D.Spray.DepthFade6Cm"));
    if (!Material || !Data ||
        Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != SprayObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Additive || Material->TwoSided ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Material->TranslucencyLightingMode != TLM_VolumetricNonDirectional ||
        Material->RefractionMethod != RM_PixelNormalOffset ||
        Material->bScreenSpaceReflections ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit) ||
        !HasNoGeometryMutationPath(Material, Data) ||
        Data->ExpressionCollection.Expressions.Num() != 6 ||
        !Uv || Uv->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(Uv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(Uv->VTiling, 1.0f, 0.000001f) ||
        !Time || Time->bIgnorePause || Time->bOverride_Period ||
        !FMath::IsNearlyZero(Time->Period, 0.000001f) ||
        !CustomIsExact(
            Opacity,
            SprayOpacityDescription,
            SprayOpacityCode,
            CMOT_Float1,
            {TEXT("UV"), TEXT("TimeSeconds")},
            {Uv, Time}) ||
        !VectorIs(Tint, TEXT("SprayTint"), SprayTint) ||
        !ScalarIs(
            FadeDistance,
            TEXT("DepthFadeDistanceCm"),
            SprayDepthFadeCm) ||
        !DepthFade || !InputIs(DepthFade->InOpacity, Opacity) ||
        !InputIs(DepthFade->FadeDistance, FadeDistance) ||
        !FMath::IsNearlyEqual(
            DepthFade->OpacityDefault,
            SprayMaxOpacity,
            0.000001f) ||
        !FMath::IsNearlyEqual(
            DepthFade->FadeDistanceDefault,
            SprayDepthFadeCm,
            0.000001f) ||
        Data->BaseColor.Expression || Data->Roughness.Expression ||
        Data->Specular.Expression ||
        !InputIs(Data->EmissiveColor, Tint) ||
        !InputIs(Data->Opacity, DepthFade) ||
        Data->Normal.Expression || Data->Refraction.Expression ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->OpacityMask.Expression ||
        Data->AmbientOcclusion.Expression ||
        Data->SubsurfaceColor.Expression || Data->ClearCoat.Expression ||
        Data->ClearCoatRoughness.Expression ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression)
    {
        OutError = TEXT("The V5D fountain-spray graph lost its exact single-sided additive-unlit sparse breakup, refraction-unplugged, 6 cm depth fade, no-normal, or no-WPO contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateEmbeddedWaterSuppressorGraph(
    UMaterial* Material,
    FString& OutError)
{
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data ||
        Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != EmbeddedWaterSuppressorObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Additive || Material->TwoSided ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Material->TranslucencyLightingMode != TLM_VolumetricNonDirectional ||
        Material->RefractionMethod != RM_PixelNormalOffset ||
        Material->bScreenSpaceReflections ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit) ||
        !HasNoGeometryMutationPath(Material, Data) ||
        !Data->ExpressionCollection.Expressions.IsEmpty() ||
        !Data->EmissiveColor.UseConstant ||
        Data->EmissiveColor.Constant != FColor::Black ||
        !Data->Opacity.UseConstant ||
        !FMath::IsNearlyZero(Data->Opacity.Constant, 0.000001f) ||
        Data->BaseColor.Expression || Data->Metallic.Expression ||
        Data->Specular.Expression || Data->Roughness.Expression ||
        Data->Anisotropy.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->OpacityMask.Expression ||
        Data->Normal.Expression || Data->Tangent.Expression ||
        Data->WorldPositionOffset.Expression ||
        Data->Displacement.Expression ||
        Data->SubsurfaceColor.Expression || Data->ClearCoat.Expression ||
        Data->ClearCoatRoughness.Expression ||
        Data->AmbientOcclusion.Expression || Data->Refraction.Expression ||
        Data->MaterialAttributes.Expression ||
        Data->PixelDepthOffset.Expression ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression || Data->FrontMaterial.Expression)
    {
        OutError = TEXT("The V5D embedded-water suppressor lost its exact zero-expression, zero-output, single-sided additive-unlit, refraction-unplugged, SSR-off, or no-geometry-mutation contract.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompiledMaterial(UMaterial* Material, FString& OutError)
{
    const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(FeatureLevel)
        : nullptr;
    if (Resource && !Resource->IsGameThreadShaderMapComplete())
    {
        Resource->SubmitCompileJobs_GameThread(
            EShaderCompileJobPriority::High);
        Resource->FinishCompilation();
    }
    FMaterialShaderMap* ShaderMap = Resource
        ? Resource->GetGameThreadShaderMap()
        : nullptr;
    const bool bMaterialMapDdcEnabled = IsMaterialMapDDCEnabled();
    const bool bShaderJobCacheDdcEnabled = IsShaderJobCacheDDCEnabled();
    const bool bCompileStateAccepted = ShaderMap &&
        (ShaderMap->CompiledSuccessfully() ||
         (!bMaterialMapDdcEnabled && bShaderJobCacheDdcEnabled));
    const bool bValid = Material && Resource &&
        Resource->IsCompilationFinished() &&
        Resource->IsGameThreadShaderMapComplete() &&
        bCompileStateAccepted && ShaderMap->IsValidForRendering() &&
        !Resource->IsDefaultMaterial() &&
        Resource->GetCompileErrors().IsEmpty();
    if (!bValid)
    {
        const FString Errors = Resource
            ? FString::Join(Resource->GetCompileErrors(), TEXT(" | "))
            : TEXT("material resource absent");
        OutError = FString::Printf(
            TEXT("V5D fountain material failed its active-feature-level shader/default-fallback gate: material=%s featureLevel=%d resource=%d shaderMap=%d valid=%d default=%d errors=%s"),
            Material ? *Material->GetPathName() : TEXT("<null>"),
            static_cast<int32>(FeatureLevel),
            Resource ? 1 : 0,
            ShaderMap ? 1 : 0,
            ShaderMap && ShaderMap->IsValidForRendering() ? 1 : 0,
            Resource && Resource->IsDefaultMaterial() ? 1 : 0,
            *Errors);
        return false;
    }
    OutError.Reset();
    return true;
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets, FString& OutError)
{
    FAssetRegistryModule& Module =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (Module.Get().IsLoadingAssets())
    {
        Module.Get().WaitForCompletion();
    }
    if (Module.Get().IsLoadingAssets())
    {
        OutError = TEXT("Asset Registry discovery did not finish for the V5D fountain material root.");
        return false;
    }
    OutAssets.Reset();
    Module.Get().GetAssetsByPath(
        FName(*MaterialRoot),
        OutAssets,
        true,
        false);
    OutError.Reset();
    return true;
}

FString CanonicalFilename(const FString& AssetName)
{
    FString Filename = FPackageName::LongPackageNameToFilename(
        PackagePath(AssetName),
        FPackageName::GetAssetPackageExtension());
    Filename = FPaths::ConvertRelativePathToFull(Filename);
    FPaths::NormalizeFilename(Filename);
    return Filename;
}

TArray<FString> ArtifactPaths(const FString& AssetName)
{
    const FString Canonical = CanonicalFilename(AssetName);
    const FString Stem = FPaths::Combine(
        FPaths::GetPath(Canonical),
        FPaths::GetBaseFilename(Canonical));
    TArray<FString> Paths;
    for (const TCHAR* Extension : ArtifactExtensions)
    {
        FString Path = Stem + Extension;
        FPaths::NormalizeFilename(Path);
        Paths.Add(Path);
    }
    return Paths;
}

struct FProtectedArtifactBytes
{
    FString AssetName;
    FString Filename;
    bool bExisted = false;
    TArray<uint8> Bytes;
};

bool AllArtifactsAbsent(
    const TArray<FString>& AssetNames,
    FString& OutError)
{
    for (const FString& AssetName : AssetNames)
    {
        for (const FString& Artifact : ArtifactPaths(AssetName))
        {
            if (IFileManager::Get().FileSize(*Artifact) >= 0)
            {
                OutError = TEXT("A V5D fountain material artifact expected to be absent already exists: ") +
                    Artifact;
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool AllFreshArtifactsAbsent(FString& OutError)
{
    return AllArtifactsAbsent(R5AssetNames(), OutError);
}

bool CaptureProtectedArtifacts(
    const TArray<FString>& AssetNames,
    TArray<FProtectedArtifactBytes>& OutRecords,
    FString& OutError)
{
    OutRecords.Reset();
    for (const FString& AssetName : AssetNames)
    {
        for (const FString& Artifact : ArtifactPaths(AssetName))
        {
            FProtectedArtifactBytes& Record = OutRecords.AddDefaulted_GetRef();
            Record.AssetName = AssetName;
            Record.Filename = Artifact;
            const int64 FileSize = IFileManager::Get().FileSize(*Artifact);
            Record.bExisted = FileSize >= 0;
            if (Record.bExisted &&
                (!FFileHelper::LoadFileToArray(Record.Bytes, *Artifact) ||
                 static_cast<int64>(Record.Bytes.Num()) != FileSize))
            {
                OutError = TEXT("Could not freeze exact V5D fountain material artifact bytes: ") +
                    Artifact;
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateProtectedArtifacts(
    const TArray<FProtectedArtifactBytes>& Records,
    FString& OutError)
{
    if (Records.Num() !=
        R4AssetNames().Num() * UE_ARRAY_COUNT(ArtifactExtensions))
    {
        OutError = TEXT("The protected R4 fountain-material artifact roster changed.");
        return false;
    }
    for (const FProtectedArtifactBytes& Record : Records)
    {
        const int64 FileSize =
            IFileManager::Get().FileSize(*Record.Filename);
        if ((FileSize >= 0) != Record.bExisted)
        {
            OutError = TEXT("A protected R4 fountain-material artifact changed presence: ") +
                Record.Filename;
            return false;
        }
        if (Record.bExisted)
        {
            TArray<uint8> CurrentBytes;
            if (!FFileHelper::LoadFileToArray(
                    CurrentBytes,
                    *Record.Filename) ||
                static_cast<int64>(CurrentBytes.Num()) != FileSize ||
                CurrentBytes != Record.Bytes)
            {
                OutError = TEXT("A protected R4 fountain-material artifact changed bytes: ") +
                    Record.Filename;
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool CreateBackupDirectory(FString& OutDirectory, FString& OutError)
{
    FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/Backups/V5D_FountainMaterials")));
    FPaths::NormalizeDirectoryName(BackupRoot);
    OutDirectory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        BackupRoot,
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%f"))));
    FPaths::NormalizeDirectoryName(OutDirectory);
    if (!OutDirectory.StartsWith(
            BackupRoot + TEXT("/"),
            ESearchCase::IgnoreCase) ||
        IFileManager::Get().DirectoryExists(*OutDirectory) ||
        !IFileManager::Get().MakeDirectory(*OutDirectory, true))
    {
        OutError = TEXT("Could not create a contained V5D fountain-material backup directory: ") +
            OutDirectory;
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateAbsentStateReceipt(FString& OutDirectory, FString& OutError)
{
    if (!CreateBackupDirectory(OutDirectory, OutError))
    {
        return false;
    }
    FString Receipt = TEXT("TRIAD_V5D_FOUNTAIN_MATERIAL_ABSENT_BACKUP_V2\n");
    for (const FString& AssetName : R5AssetNames())
    {
        Receipt += TEXT("package=") + PackagePath(AssetName) + TEXT("\n");
        for (const FString& Artifact : ArtifactPaths(AssetName))
        {
            Receipt += TEXT("original=") + Artifact +
                TEXT("\nstate=ABSENT\nrollback=DELETE_IF_PRESENT\n");
        }
    }
    const FString ReceiptFilename = FPaths::Combine(
        OutDirectory,
        TEXT("backup.receipt.txt"));
    FString Readback;
    if (!FFileHelper::SaveStringToFile(
            Receipt,
            *ReceiptFilename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !FFileHelper::LoadFileToString(Readback, *ReceiptFilename) ||
        Readback != Receipt || !AllFreshArtifactsAbsent(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D fountain-material absent-state receipt failed exact readback.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateR4UpgradeReceipt(
    const TArray<FProtectedArtifactBytes>& ProtectedArtifacts,
    FString& OutDirectory,
    FString& OutError)
{
    if (!ValidateProtectedArtifacts(ProtectedArtifacts, OutError) ||
        !AllArtifactsAbsent(SuppressorAssetNames(), OutError) ||
        !CreateBackupDirectory(OutDirectory, OutError))
    {
        return false;
    }
    FString Receipt = TEXT("TRIAD_V5D_FOUNTAIN_MATERIAL_R4_TO_R5_BACKUP_V1\n");
    Receipt += TEXT("mode=CREATE_SUPPRESSOR_ONLY\n");
    for (const FProtectedArtifactBytes& Record : ProtectedArtifacts)
    {
        Receipt += TEXT("asset=") + Record.AssetName + TEXT("\noriginal=") +
            Record.Filename + TEXT("\nstate=") +
            (Record.bExisted ? TEXT("PRESENT") : TEXT("ABSENT")) +
            TEXT("\nbytes=") +
            FString::Printf(TEXT("%d"), Record.Bytes.Num()) +
            TEXT("\nrollback=BYTE_IDENTICAL_REQUIRED\n");
    }
    for (const FString& Artifact :
         ArtifactPaths(EmbeddedWaterSuppressorAssetName))
    {
        Receipt += TEXT("asset=") + EmbeddedWaterSuppressorAssetName +
            TEXT("\noriginal=") + Artifact +
            TEXT("\nstate=ABSENT\nrollback=DELETE_IF_PRESENT\n");
    }
    const FString ReceiptFilename = FPaths::Combine(
        OutDirectory,
        TEXT("backup.receipt.txt"));
    FString Readback;
    if (!FFileHelper::SaveStringToFile(
            Receipt,
            *ReceiptFilename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !FFileHelper::LoadFileToString(Readback, *ReceiptFilename) ||
        Readback != Receipt ||
        !ValidateProtectedArtifacts(ProtectedArtifacts, OutError) ||
        !AllArtifactsAbsent(SuppressorAssetNames(), OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D R4-to-R5 fountain-material receipt failed exact readback.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool RollBackCreatedPackages(
    const TArray<FString>& CreatedAssetNames,
    FString& OutError)
{
    TArray<UPackage*> Packages;
    for (const FString& AssetName : CreatedAssetNames)
    {
        if (UPackage* Package = FindPackage(nullptr, *PackagePath(AssetName)))
        {
            Packages.Add(Package);
        }
    }
    if (!Packages.IsEmpty())
    {
        FText UnloadError;
        if (!UPackageTools::UnloadPackages(Packages, UnloadError, true))
        {
            OutError = TEXT("V5D fountain-material rollback refused artifact deletion because exact package unload failed: ") +
                UnloadError.ToString();
            return false;
        }
    }
    for (const FString& AssetName : CreatedAssetNames)
    {
        if (FindPackage(nullptr, *PackagePath(AssetName)))
        {
            OutError = TEXT("V5D fountain-material rollback still resolves a package after unload: ") +
                PackagePath(AssetName);
            return false;
        }
    }

    bool bDeleted = true;
    TArray<FString> CanonicalFiles;
    for (const FString& AssetName : CreatedAssetNames)
    {
        CanonicalFiles.Add(CanonicalFilename(AssetName));
        for (const FString& Artifact : ArtifactPaths(AssetName))
        {
            if (IFileManager::Get().FileSize(*Artifact) >= 0)
            {
                bDeleted &= IFileManager::Get().Delete(
                    *Artifact,
                    false,
                    true);
            }
        }
    }
    IAssetRegistry::GetChecked().ScanModifiedAssetFiles(CanonicalFiles);
    if (!bDeleted || !AllArtifactsAbsent(CreatedAssetNames, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D fountain-material rollback did not restore exact artifact absence.");
        }
        return false;
    }
    for (const FString& AssetName : CreatedAssetNames)
    {
        const FString Object = ObjectPath(AssetName);
        if (FindObject<UMaterial>(nullptr, *Object) ||
            IAssetRegistry::GetChecked().GetAssetByObjectPath(
                FSoftObjectPath(Object)).IsValid())
        {
            OutError = TEXT("V5D fountain-material rollback left a loaded object or Asset Registry row: ") +
                Object;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

class FScopedCreatedMaterialRollback final
{
public:
    FScopedCreatedMaterialRollback(
        const TArray<FString>& InCreatedAssetNames,
        const TArray<FProtectedArtifactBytes>* InProtectedArtifacts,
        TArray<UMaterial*>& InMaterials,
        FString& InReport)
        : CreatedAssetNames(InCreatedAssetNames)
        , ProtectedArtifacts(InProtectedArtifacts)
        , Materials(InMaterials)
        , Report(InReport)
    {
    }

    ~FScopedCreatedMaterialRollback()
    {
        if (bCommitted)
        {
            return;
        }
        Materials.Reset();
        FString RollbackError;
        if (!RollBackCreatedPackages(CreatedAssetNames, RollbackError))
        {
            Report += TEXT(" AUTOMATIC_ROLLBACK_FAILED ") + RollbackError;
            return;
        }
        if (ProtectedArtifacts &&
            !ValidateProtectedArtifacts(*ProtectedArtifacts, RollbackError))
        {
            Report += TEXT(" AUTOMATIC_ROLLBACK_PROTECTED_BYTES_FAILED ") +
                RollbackError;
            return;
        }
        Report += ProtectedArtifacts
            ? TEXT(" AUTOMATIC_ROLLBACK_OK newArtifactsAbsent=true existingPairBytesUntouched=true")
            : TEXT(" AUTOMATIC_ROLLBACK_OK restoredToAbsent=true");
    }

    void Commit()
    {
        bCommitted = true;
    }

private:
    const TArray<FString>& CreatedAssetNames;
    const TArray<FProtectedArtifactBytes>* ProtectedArtifacts = nullptr;
    TArray<UMaterial*>& Materials;
    FString& Report;
    bool bCommitted = false;
};

bool ExistingStateIsCompletelyAbsent(FString& OutError)
{
    TArray<FAssetData> RootAssets;
    if (!GatherRootAssets(RootAssets, OutError) || !RootAssets.IsEmpty())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Fresh V5D fountain material creation requires an empty exact material root.");
        }
        return false;
    }
    for (const FString& AssetName : R5AssetNames())
    {
        const FString Package = PackagePath(AssetName);
        const FString Object = ObjectPath(AssetName);
        if (FPackageName::DoesPackageExist(Package) ||
            FindPackage(nullptr, *Package) ||
            FindObject<UObject>(nullptr, *Object) ||
            IAssetRegistry::GetChecked().GetAssetByObjectPath(
                FSoftObjectPath(Object)).IsValid())
        {
            OutError = TEXT("V5D fountain material state is partial or mixed at: ") +
                Object;
            return false;
        }
    }
    return AllFreshArtifactsAbsent(OutError);
}

bool EmbeddedWaterSuppressorStateIsCompletelyAbsent(FString& OutError)
{
    const FString Package = PackagePath(EmbeddedWaterSuppressorAssetName);
    if (FPackageName::DoesPackageExist(Package) ||
        FindPackage(nullptr, *Package) ||
        FindObject<UObject>(nullptr, *EmbeddedWaterSuppressorObjectPath) ||
        IAssetRegistry::GetChecked().GetAssetByObjectPath(
            FSoftObjectPath(EmbeddedWaterSuppressorObjectPath)).IsValid())
    {
        OutError = TEXT("The R4-to-R5 upgrade requires the embedded-water suppressor to be exactly absent.");
        return false;
    }
    return AllArtifactsAbsent(SuppressorAssetNames(), OutError);
}

bool ValidateInstalledR4Pair(
    UMaterial*& OutWater,
    UMaterial*& OutSpray,
    FString& OutError)
{
    OutWater = nullptr;
    OutSpray = nullptr;
    TArray<FAssetData> RootAssets;
    if (!GatherRootAssets(RootAssets, OutError) || RootAssets.Num() != 2)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("The installed R4 fountain material root requires exactly two assets; found %d."),
                RootAssets.Num());
        }
        return false;
    }
    TSet<FString> Expected = {WaterObjectPath, SprayObjectPath};
    for (const FAssetData& Asset : RootAssets)
    {
        if (!Expected.Remove(Asset.GetObjectPathString()))
        {
            OutError = TEXT("The installed R4 fountain material root contains an unexpected asset: ") +
                Asset.GetObjectPathString();
            return false;
        }
    }
    if (!Expected.IsEmpty())
    {
        OutError = TEXT("The installed R4 fountain material root is missing an exact asset.");
        return false;
    }

    UMaterial* Water = LoadExact<UMaterial>(WaterObjectPath);
    UMaterial* Spray = LoadExact<UMaterial>(SprayObjectPath);
    if (!Water || !Spray || !Water->GetOutermost() ||
        !Spray->GetOutermost() || Water->GetOutermost()->IsDirty() ||
        Spray->GetOutermost()->IsDirty() ||
        Water->GetOutermost()->GetName() != PackagePath(WaterAssetName) ||
        Spray->GetOutermost()->GetName() != PackagePath(SprayAssetName) ||
        !ValidateWaterGraph(Water, OutError) ||
        !ValidateSprayGraph(Spray, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The installed R4 fountain material pair is absent, dirty, mispackaged, or graph-invalid.");
        }
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateCompiledMaterial(Water, OutError) ||
        !ValidateCompiledMaterial(Spray, OutError))
    {
        return false;
    }
    OutWater = Water;
    OutSpray = Spray;
    OutError.Reset();
    return true;
}

bool UpgradeInstalledR4Pair(
    TArray<UMaterial*>& OutMaterials,
    FString& OutReport)
{
    FString Error;
    if (!EmbeddedWaterSuppressorStateIsCompletelyAbsent(Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED_MIXED: ") + Error;
        return false;
    }

    TArray<FProtectedArtifactBytes> ProtectedR4Artifacts;
    if (!CaptureProtectedArtifacts(
            R4AssetNames(),
            ProtectedR4Artifacts,
            Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED_R4_SNAPSHOT: ") +
            Error;
        return false;
    }
    UMaterial* Water = nullptr;
    UMaterial* Spray = nullptr;
    if (!ValidateInstalledR4Pair(Water, Spray, Error) ||
        !ValidateProtectedArtifacts(ProtectedR4Artifacts, Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED_INVALID_R4_PAIR: ") +
            Error;
        return false;
    }

    FString BackupDirectory;
    if (!CreateR4UpgradeReceipt(
            ProtectedR4Artifacts,
            BackupDirectory,
            Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED_BACKUP: ") + Error;
        return false;
    }
    FScopedCreatedMaterialRollback Rollback(
        SuppressorAssetNames(),
        &ProtectedR4Artifacts,
        OutMaterials,
        OutReport);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterial* Suppressor = CreateEmbeddedWaterSuppressorMaterial(
        AssetTools,
        Error);
    if (!Suppressor ||
        Suppressor->GetPathName() != EmbeddedWaterSuppressorObjectPath)
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_UPGRADE_CREATE: ") +
            Error;
        return false;
    }
    OutMaterials = {Water, Spray, Suppressor};
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateWaterGraph(Water, Error) ||
        !ValidateSprayGraph(Spray, Error) ||
        !ValidateEmbeddedWaterSuppressorGraph(Suppressor, Error) ||
        !ValidateCompiledMaterial(Water, Error) ||
        !ValidateCompiledMaterial(Spray, Error) ||
        !ValidateCompiledMaterial(Suppressor, Error) ||
        Water->GetOutermost()->IsDirty() ||
        Spray->GetOutermost()->IsDirty() ||
        !ValidateProtectedArtifacts(ProtectedR4Artifacts, Error))
    {
        if (Error.IsEmpty())
        {
            Error = TEXT("The protected R4 pair became dirty during suppressor hot validation.");
        }
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_UPGRADE_HOT_VALIDATION: ") +
            Error;
        return false;
    }

    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    TArray<UObject*> ExactSaveTargets = {Suppressor};
    if (!Assets || ExactSaveTargets.Num() != 1 ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false) ||
        Water->GetOutermost()->IsDirty() ||
        Spray->GetOutermost()->IsDirty() ||
        !ValidateProtectedArtifacts(ProtectedR4Artifacts, Error))
    {
        if (Error.IsEmpty())
        {
            Error = TEXT("Only the exact new suppressor package was offered to save, but the one-package save boundary failed.");
        }
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_UPGRADE_SAVE: ") +
            Error;
        return false;
    }

    UPackage* SuppressorPackage = Suppressor->GetOutermost();
    if (SuppressorPackage)
    {
        SuppressorPackage->SetDirtyFlag(false);
    }
    OutMaterials.Reset();
    Suppressor = nullptr;
    ExactSaveTargets.Reset();
    TArray<UPackage*> ReloadPackages = {SuppressorPackage};
    FText ReloadError;
    if (ReloadPackages.Num() != 1 || ReloadPackages.Contains(nullptr) ||
        !UPackageTools::ReloadPackages(
            ReloadPackages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_UPGRADE_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }
    ReloadPackages.Reset();
    if (!TRIADIstanaExploreV5DFountainMaterialFactory::
            ValidateFountainRealismMaterialAssets(OutMaterials, Error) ||
        OutMaterials.Num() != 3 ||
        !ValidateProtectedArtifacts(ProtectedR4Artifacts, Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_UPGRADE_COLD_VALIDATION: ") +
            Error;
        return false;
    }
    Rollback.Commit();
    OutReport = FString::Printf(
        TEXT("V5D_FOUNTAIN_MATERIALS_UPGRADE_PASS revision=R5 previousRevision=R4 count=3 existingPackages=2 newPackages=1 saveTargets=1 reloadTargets=1 coldValidatedMaterials=3 exactRoster=true existingPairBytesUntouched=true suppressor=M_IPV5D_FountainEmbeddedWaterSuppressor suppressorBlend=Additive suppressorShading=Unlit suppressorOutputs=Zero suppressorRefraction=Unplugged compiledActiveFeatureLevelValidated=true backup=%s"),
        *BackupDirectory);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DFountainMaterialFactory
{
const FString& GetWaterMaterialObjectPath()
{
    return WaterObjectPath;
}

const FString& GetSprayMaterialObjectPath()
{
    return SprayObjectPath;
}

const FString& GetEmbeddedWaterSuppressorMaterialObjectPath()
{
    return EmbeddedWaterSuppressorObjectPath;
}

bool ValidateFountainRealismMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    FString& OutReport)
{
    OutMaterials.Reset();
    OutReport.Reset();
    TArray<FAssetData> RootAssets;
    if (!GatherRootAssets(RootAssets, OutReport) || RootAssets.Num() != 3)
    {
        if (OutReport.IsEmpty())
        {
            OutReport = FString::Printf(
                TEXT("V5D fountain material root requires exactly three assets; found %d."),
                RootAssets.Num());
        }
        return false;
    }
    TSet<FString> Expected = {
        WaterObjectPath,
        SprayObjectPath,
        EmbeddedWaterSuppressorObjectPath};
    for (const FAssetData& Asset : RootAssets)
    {
        if (!Expected.Remove(Asset.GetObjectPathString()))
        {
            OutReport = TEXT("V5D fountain material root contains an unexpected asset: ") +
                Asset.GetObjectPathString();
            return false;
        }
    }
    if (!Expected.IsEmpty())
    {
        OutReport = TEXT("V5D fountain material root is missing an exact asset.");
        return false;
    }

    UMaterial* Water = LoadExact<UMaterial>(WaterObjectPath);
    UMaterial* Spray = LoadExact<UMaterial>(SprayObjectPath);
    UMaterial* Suppressor =
        LoadExact<UMaterial>(EmbeddedWaterSuppressorObjectPath);
    if (!Water || !Spray || !Suppressor || !Water->GetOutermost() ||
        !Spray->GetOutermost() || !Suppressor->GetOutermost() ||
        Water->GetOutermost()->IsDirty() ||
        Spray->GetOutermost()->IsDirty() ||
        Suppressor->GetOutermost()->IsDirty() ||
        Water->GetOutermost()->GetName() != PackagePath(WaterAssetName) ||
        Spray->GetOutermost()->GetName() != PackagePath(SprayAssetName) ||
        Suppressor->GetOutermost()->GetName() !=
            PackagePath(EmbeddedWaterSuppressorAssetName) ||
        !ValidateWaterGraph(Water, OutReport) ||
        !ValidateSprayGraph(Spray, OutReport) ||
        !ValidateEmbeddedWaterSuppressorGraph(Suppressor, OutReport))
    {
        if (OutReport.IsEmpty())
        {
            OutReport = TEXT("V5D fountain material roster is absent, dirty, mispackaged, or graph-invalid.");
        }
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateCompiledMaterial(Water, OutReport) ||
        !ValidateCompiledMaterial(Spray, OutReport) ||
        !ValidateCompiledMaterial(Suppressor, OutReport))
    {
        return false;
    }
    OutMaterials = {Water, Spray, Suppressor};
    OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_VALID revision=R5 count=3 exactRoster=true water=M_IPV5D_FountainWater waterBlend=Opaque waterNormal=worldXYCalmWorldSpace wavelengthsMeters=3.40,1.20 slopes=0.0020,0.0010 waterRoughness=0.18 waterSpecular=0.50 waterRefraction=NotApplicableOpaque waterSsr=false spray=M_IPV5D_FountainSpray sprayBlend=Additive sprayShading=Unlit sprayLighting=VolumetricNonDirectional sprayRefraction=Unplugged sprayTwoSided=false sprayMaxOpacity=0.07 sprayOpacityFloor=0 sprayDepthFadeCm=6 suppressor=M_IPV5D_FountainEmbeddedWaterSuppressor suppressorBlend=Additive suppressorShading=Unlit suppressorLighting=VolumetricNonDirectional suppressorRefraction=Unplugged suppressorTwoSided=false suppressorExpressions=0 suppressorOutputs=Zero noSourceV5OrV5BMutation=true compiledActiveFeatureLevelValidated=true packagesClean=true");
    return true;
}

bool EnsureFountainRealismMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    FString& OutReport)
{
    OutMaterials.Reset();
    OutReport.Reset();
    if (!GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED: a non-PIE editor is required.");
        return false;
    }

    const bool bWaterPackage =
        FPackageName::DoesPackageExist(PackagePath(WaterAssetName));
    const bool bSprayPackage =
        FPackageName::DoesPackageExist(PackagePath(SprayAssetName));
    const bool bSuppressorPackage = FPackageName::DoesPackageExist(
        PackagePath(EmbeddedWaterSuppressorAssetName));
    if (bWaterPackage && bSprayPackage && bSuppressorPackage)
    {
        if (!ValidateFountainRealismMaterialAssets(
                OutMaterials,
                OutReport))
        {
            OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED_INVALID_EXISTING: ") +
                OutReport;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_") + OutReport;
        return true;
    }
    if (bWaterPackage && bSprayPackage && !bSuppressorPackage)
    {
        return UpgradeInstalledR4Pair(OutMaterials, OutReport);
    }
    FString Error;
    if (bWaterPackage || bSprayPackage || bSuppressorPackage ||
        !ExistingStateIsCompletelyAbsent(Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED_MIXED: ") + Error;
        return false;
    }

    FString BackupDirectory;
    if (!CreateAbsentStateReceipt(BackupDirectory, Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_REFUSED_BACKUP: ") + Error;
        return false;
    }
    FScopedCreatedMaterialRollback Rollback(
        R5AssetNames(),
        nullptr,
        OutMaterials,
        OutReport);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterial* Water = CreateWaterMaterial(AssetTools, Error);
    UMaterial* Spray = Water
        ? CreateSprayMaterial(AssetTools, Error)
        : nullptr;
    UMaterial* Suppressor = Spray
        ? CreateEmbeddedWaterSuppressorMaterial(AssetTools, Error)
        : nullptr;
    if (!Water || !Spray || Water->GetPathName() != WaterObjectPath ||
        Spray->GetPathName() != SprayObjectPath || !Suppressor ||
        Suppressor->GetPathName() != EmbeddedWaterSuppressorObjectPath)
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_CREATE: ") + Error;
        return false;
    }
    OutMaterials = {Water, Spray, Suppressor};
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateWaterGraph(Water, Error) ||
        !ValidateSprayGraph(Spray, Error) ||
        !ValidateEmbeddedWaterSuppressorGraph(Suppressor, Error) ||
        !ValidateCompiledMaterial(Water, Error) ||
        !ValidateCompiledMaterial(Spray, Error) ||
        !ValidateCompiledMaterial(Suppressor, Error))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_HOT_VALIDATION: ") + Error;
        return false;
    }

    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    TArray<UObject*> ExactSaveTargets = {Water, Spray, Suppressor};
    if (!Assets || ExactSaveTargets.Num() != 3 ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_SAVE: only the exact three new packages were offered to save.");
        return false;
    }

    TArray<UPackage*> Packages = {
        Water->GetOutermost(),
        Spray->GetOutermost(),
        Suppressor->GetOutermost()};
    for (UPackage* Package : Packages)
    {
        if (Package)
        {
            Package->SetDirtyFlag(false);
        }
    }
    OutMaterials.Reset();
    Water = nullptr;
    Spray = nullptr;
    Suppressor = nullptr;
    ExactSaveTargets.Reset();
    FText ReloadError;
    if (Packages.Num() != 3 || Packages.Contains(nullptr) ||
        !UPackageTools::ReloadPackages(
            Packages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }
    Packages.Reset();
    if (!ValidateFountainRealismMaterialAssets(OutMaterials, Error) ||
        OutMaterials.Num() != 3)
    {
        OutReport = TEXT("V5D_FOUNTAIN_MATERIALS_FAILED_COLD_VALIDATION: ") +
            Error;
        return false;
    }
    Rollback.Commit();
    OutReport = FString::Printf(
        TEXT("V5D_FOUNTAIN_MATERIALS_CREATE_PASS revision=R5 count=3 newPackages=3 saveTargets=3 reloadTargets=3 coldValidatedMaterials=3 exactRoster=true sourceV5V5BMaterialsUntouched=true waterBlend=Opaque waterNormal=worldXYCalmWorldSpace waterRefraction=NotApplicableOpaque waterSsr=false sprayBlend=Additive sprayShading=Unlit sprayRefraction=Unplugged sprayTwoSided=false sprayMaxOpacity=0.07 sprayOpacityFloor=0 suppressor=M_IPV5D_FountainEmbeddedWaterSuppressor suppressorBlend=Additive suppressorShading=Unlit suppressorOutputs=Zero suppressorRefraction=Unplugged compiledActiveFeatureLevelValidated=true initialArtifactsAbsent=true backup=%s"),
        *BackupDirectory);
    return true;
}
} // namespace TRIADIstanaExploreV5DFountainMaterialFactory
