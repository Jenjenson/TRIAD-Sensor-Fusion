#include "TRIADIstanaExploreV5MaterialFactory.h"

#include "TRIADIstanaPublicViewHeroV5EditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <initializer_list>

namespace
{
const FString MaterialRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials"));

const FString GrassBaseName(TEXT("M_IPV5_Grass001_Lawn_Base"));
const FString GrassInstanceName(TEXT("MI_IPV5_Grass001_Lawn"));
const FString FountainWaterName(TEXT("M_IPV5_FountainWater"));
const FString HardscapeStoneName(TEXT("MI_IPV5_HardscapeStone"));
const FString ContextRenderName(TEXT("MI_IPV5_ContextRender"));
const FString ContextRoofName(TEXT("MI_IPV5_ContextRoof"));

FString MakeObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const FString GrassBaseObjectPath = MakeObjectPath(MaterialRoot, GrassBaseName);
const FString GrassInstanceObjectPath =
    MakeObjectPath(MaterialRoot, GrassInstanceName);
const FString FountainWaterObjectPath =
    MakeObjectPath(MaterialRoot, FountainWaterName);
const FString HardscapeStoneObjectPath =
    MakeObjectPath(MaterialRoot, HardscapeStoneName);
const FString ContextRenderObjectPath =
    MakeObjectPath(MaterialRoot, ContextRenderName);
const FString ContextRoofObjectPath =
    MakeObjectPath(MaterialRoot, ContextRoofName);

const TArray<FString>& ExactMaterialObjectPaths()
{
    static const TArray<FString> Paths = {
        GrassBaseObjectPath,
        GrassInstanceObjectPath,
        FountainWaterObjectPath,
        HardscapeStoneObjectPath,
        ContextRenderObjectPath,
        ContextRoofObjectPath};
    return Paths;
}

const FString GrassBaseColorTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/"
         "T_IPV4_Grass001_BaseColor.T_IPV4_Grass001_BaseColor"));
const FString GrassNormalDxTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/"
         "T_IPV4_Grass001_NormalDX.T_IPV4_Grass001_NormalDX"));
const FString GrassRoughnessTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/"
         "T_IPV4_Grass001_Roughness.T_IPV4_Grass001_Roughness"));
const FString GrassAoTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/"
         "T_IPV4_Grass001_AmbientOcclusion."
         "T_IPV4_Grass001_AmbientOcclusion"));

const FString HeroStoneParentPath(
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5/"
         "MI_IPV_Hero_Stone_V5.MI_IPV_Hero_Stone_V5"));
const FString HeroRenderParentPath(
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5/"
         "MI_IPV_Hero_Render_V5.MI_IPV_Hero_Render_V5"));
const FString HeroSlateParentPath(
    TEXT("/Game/TRIAD/IstanaPublicView/HeroMaterialsV5/"
         "MI_IPV_Hero_Slate_V5.MI_IPV_Hero_Slate_V5"));

const FName AssetCreateContext(
    TEXT("TRIAD.CreateIstanaExploreV5Materials"));
const FName ParameterGroup(TEXT("Istana Explore V5"));

constexpr float PrimaryTileMeters = 1.4f;
constexpr float SecondaryTileMeters = 2.37f;
constexpr float MacroTileMeters = 29.0f;
constexpr float SecondaryRotationRadians = FMath::DegreesToRadians(37.0f);
constexpr float SecondaryOffsetU = 13.17f;
constexpr float SecondaryOffsetV = -7.43f;
constexpr float GrassDesaturation = 0.22f;
constexpr float GrassSecondaryBlend = 0.38f;
constexpr float GrassMicroNormalStrength = 0.48f;
constexpr float GrassFarMicroNormalStrength = 0.06f;
constexpr float GrassFadeStartCm = 1200.0f;
constexpr float GrassFadeRangeCm = 4200.0f;
constexpr float GrassRoughnessBias = 0.035f;

const FLinearColor GrassLawnTint(0.940f, 0.985f, 0.918f, 1.0f);
const FLinearColor GrassMacroTintLow(0.952f, 0.960f, 0.940f, 1.0f);
const FLinearColor GrassMacroTintHigh(1.011f, 1.015f, 0.995f, 1.0f);
const FLinearColor GrassLuminanceFactors(
    0.212639005872f,
    0.715168678768f,
    0.0721923153607f,
    0.0f);

constexpr float WaterVelocityAx = 0.015f;
constexpr float WaterVelocityAy = 0.005f;
constexpr float WaterVelocityBx = -0.008f;
constexpr float WaterVelocityBy = 0.022f;
constexpr float WaterFieldScaleA = 0.82f;
constexpr float WaterFieldScaleB = 1.31f;
constexpr float WaterRoughness = 0.08f;
constexpr float WaterSpecular = 0.50f;
constexpr float WaterOpacity = 0.46f;
constexpr float WaterOpacityMin = 0.35f;
constexpr float WaterOpacityMax = 0.65f;
constexpr float WaterDepthFadeCm = 80.0f;
constexpr float WaterDepthFadeMinCm = 25.0f;
constexpr float WaterDepthFadeMaxCm = 150.0f;
constexpr float WaterIor = 1.333f;
const FLinearColor WaterTint(0.012f, 0.055f, 0.070f, 1.0f);

const FString GrassRotatedNormalCode(
    TEXT("return normalize(float3(\n")
    TEXT("    0.79863551 * RotatedNormal.x + 0.60181502 * RotatedNormal.y,\n")
    TEXT("   -0.60181502 * RotatedNormal.x + 0.79863551 * RotatedNormal.y,\n")
    TEXT("    RotatedNormal.z));"));

const FString GrassMacroFieldDescription(
    TEXT("TRIAD_IPV5_LAWN_MACRO_HEALTH_MOW_RESPONSE_V2_DEPERIODIZED"));
const FString GrassMacroFieldCode(
    TEXT("float2 p = MacroUV;\n")
    TEXT("float2 i = floor(p);\n")
    TEXT("float2 f = frac(p);\n")
    TEXT("f = f * f * (3.0 - 2.0 * f);\n")
    TEXT("float4 h = frac(sin(float4(\n")
    TEXT("    dot(i, float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(1.0, 0.0), float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(0.0, 1.0), float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(1.0, 1.0), float2(127.1, 311.7)))) * 43758.5453);\n")
    TEXT("float broad = lerp(lerp(h.x, h.y, f.x), lerp(h.z, h.w, f.x), f.y);\n")
    TEXT("float2 q = p * 2.173 + float2(11.17, -7.43);\n")
    TEXT("float2 j = floor(q);\n")
    TEXT("float2 g = frac(q);\n")
    TEXT("g = g * g * (3.0 - 2.0 * g);\n")
    TEXT("float4 k = frac(sin(float4(\n")
    TEXT("    dot(j, float2(269.5, 183.3)),\n")
    TEXT("    dot(j + float2(1.0, 0.0), float2(269.5, 183.3)),\n")
    TEXT("    dot(j + float2(0.0, 1.0), float2(269.5, 183.3)),\n")
    TEXT("    dot(j + float2(1.0, 1.0), float2(269.5, 183.3)))) * 43758.5453);\n")
    TEXT("float detail = lerp(lerp(k.x, k.y, g.x), lerp(k.z, k.w, g.x), g.y);\n")
    TEXT("float stripePhase = p.x * 1.35 + p.y * 0.11 + 0.55 * (broad - 0.50) + 0.20 * (detail - 0.50);\n")
    TEXT("float stripe = 0.5 + 0.5 * sin(6.28318530718 * stripePhase);\n")
    TEXT("return saturate(0.50 + 0.62 * (broad - 0.50) + 0.28 * (detail - 0.50) + 0.035 * (stripe - 0.50));"));

const FString WaterAnalyticNormalCode(
    TEXT("const float Tau = 6.28318530718;\n")
    TEXT("float2 PhaseA = UV0 / FieldScaleA + TimeSeconds * VelocityA;\n")
    TEXT("float2 PhaseB = UV0 / FieldScaleB + TimeSeconds * VelocityB;\n")
    TEXT("float2 SlopeA = float2(\n")
    TEXT("    sin(Tau * (PhaseA.x + PhaseA.y * 0.31)),\n")
    TEXT("    cos(Tau * (PhaseA.y - PhaseA.x * 0.27)));\n")
    TEXT("float2 SlopeB = float2(\n")
    TEXT("    cos(Tau * (PhaseB.x - PhaseB.y * 0.43)),\n")
    TEXT("    sin(Tau * (PhaseB.y + PhaseB.x * 0.39)));\n")
    TEXT("return normalize(float3(SlopeA * 0.055 + SlopeB * 0.038, 1.0));"));

template <typename T>
T* LoadExact(const FString& ExactObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ExactObjectPath);
    return Object && Object->GetPathName() == ExactObjectPath ? Object : nullptr;
}

bool GatherMaterialRootAssets(
    TArray<FAssetData>& OutAssets,
    FString& OutError)
{
    FAssetRegistryModule& RegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (RegistryModule.Get().IsLoadingAssets())
    {
        RegistryModule.Get().WaitForCompletion();
    }
    if (RegistryModule.Get().IsLoadingAssets())
    {
        OutError =
            TEXT("Asset Registry discovery did not complete; the exact Explore V5 material roster is not trustworthy.");
        return false;
    }
    OutAssets.Reset();
    RegistryModule.Get().GetAssetsByPath(
        FName(*MaterialRoot), OutAssets, true, false);
    OutError.Reset();
    return true;
}

bool ValidateEmptyMaterialRoot(FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherMaterialRootAssets(Assets, OutError))
    {
        return false;
    }
    if (!Assets.IsEmpty())
    {
        TArray<FString> Paths;
        for (const FAssetData& Asset : Assets)
        {
            Paths.Add(Asset.GetObjectPathString());
        }
        Paths.Sort();
        OutError = FString::Printf(
            TEXT("CreateFreshExploreV5Materials requires an empty exact material root; found [%s]."),
            *FString::Join(Paths, TEXT(", ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactMaterialRootRoster(FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherMaterialRootAssets(Assets, OutError))
    {
        return false;
    }
    TSet<FString> Expected;
    for (const FString& Path : ExactMaterialObjectPaths())
    {
        Expected.Add(Path);
    }
    TSet<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        const FString Path = Asset.GetObjectPathString();
        if (!Expected.Contains(Path) || Actual.Contains(Path))
        {
            OutError = FString::Printf(
                TEXT("Explore V5 material root contains an unexpected or duplicate asset '%s'."),
                *Path);
            return false;
        }
        Actual.Add(Path);
    }
    if (Assets.Num() != Expected.Num() || Actual.Num() != Expected.Num())
    {
        TArray<FString> Missing;
        for (const FString& Path : ExactMaterialObjectPaths())
        {
            if (!Actual.Contains(Path))
            {
                Missing.Add(Path);
            }
        }
        OutError = FString::Printf(
            TEXT("Explore V5 material root has %d assets instead of exactly six; missing [%s]."),
            Assets.Num(),
            *FString::Join(Missing, TEXT(", ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateHeroV5Dependencies(FString& OutError)
{
    FString HeroReport;
    if (!UTRIADIstanaPublicViewHeroV5EditorLibrary::
            ValidateIstanaPublicViewHeroV5Assets(HeroReport))
    {
        OutError = TEXT("Explore V5 refused invalid Hero V5 parent assets: ") +
            HeroReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool RollBackFreshMaterialRoot(
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    TArray<FAssetData> AssetData;
    if (!GatherMaterialRootAssets(AssetData, OutError))
    {
        OutAssets.Reset();
        return false;
    }
    TSet<FString> Expected;
    for (const FString& Path : ExactMaterialObjectPaths())
    {
        Expected.Add(Path);
    }
    TArray<UObject*> ObjectsToDelete;
    for (const FAssetData& Data : AssetData)
    {
        const FString ObjectPath = Data.GetObjectPathString();
        if (!Expected.Contains(ObjectPath))
        {
            continue;
        }
        UObject* Object = FindObject<UObject>(nullptr, *ObjectPath);
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (!Object || Object->GetPathName() != ObjectPath || !Package ||
            !Package->HasAnyPackageFlags(PKG_NewlyCreated) ||
            FPackageName::DoesPackageExist(Package->GetName()))
        {
            OutAssets.Reset();
            OutError = FString::Printf(
                TEXT("Fresh Explore V5 rollback refused asset '%s' because it is not a loaded, unsaved, newly-created package."),
                *ObjectPath);
            return false;
        }
        ObjectsToDelete.AddUnique(Object);
    }
    for (const FString& ObjectPath : ExactMaterialObjectPaths())
    {
        if (UObject* Object = FindObject<UObject>(nullptr, *ObjectPath))
        {
            UPackage* Package = Object->GetOutermost();
            if (!Package || !Package->HasAnyPackageFlags(PKG_NewlyCreated) ||
                FPackageName::DoesPackageExist(Package->GetName()))
            {
                OutAssets.Reset();
                OutError = FString::Printf(
                    TEXT("Fresh Explore V5 rollback refused loaded asset '%s' because its package is not safely disposable."),
                    *ObjectPath);
                return false;
            }
            ObjectsToDelete.AddUnique(Object);
        }
    }
    const int32 RequestedDeletes = ObjectsToDelete.Num();
    const int32 DeletedObjects = RequestedDeletes > 0
        ? ObjectTools::DeleteObjectsUnchecked(ObjectsToDelete)
        : 0;
    ObjectsToDelete.Reset();
    OutAssets.Reset();

    TArray<FAssetData> RemainingAssets;
    FString GatherError;
    if (DeletedObjects < RequestedDeletes ||
        !GatherMaterialRootAssets(RemainingAssets, GatherError))
    {
        OutError = GatherError.IsEmpty()
            ? TEXT("Fresh Explore V5 rollback could not remove every newly-created in-memory material asset.")
            : GatherError;
        return false;
    }
    for (const FAssetData& Remaining : RemainingAssets)
    {
        if (Expected.Contains(Remaining.GetObjectPathString()))
        {
            OutError = FString::Printf(
                TEXT("Fresh Explore V5 rollback left asset '%s'."),
                *Remaining.GetObjectPathString());
            return false;
        }
    }
    for (const FString& ObjectPath : ExactMaterialObjectPaths())
    {
        if (FindObject<UObject>(nullptr, *ObjectPath))
        {
            OutError = FString::Printf(
                TEXT("Fresh Explore V5 rollback left loaded object '%s'."),
                *ObjectPath);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

class FScopedFreshMaterialRollback final
{
public:
    FScopedFreshMaterialRollback(
        TArray<UObject*>& InAssets,
        FString& InError)
        : Assets(InAssets)
        , Error(InError)
    {
    }

    ~FScopedFreshMaterialRollback()
    {
        if (bCommitted)
        {
            return;
        }
        FString RollbackError;
        if (!RollBackFreshMaterialRoot(Assets, RollbackError))
        {
            if (!Error.IsEmpty())
            {
                Error += TEXT(" ");
            }
            Error += TEXT("ROLLBACK_FAILED: ") + RollbackError;
        }
    }

    void Commit()
    {
        bCommitted = true;
    }

private:
    TArray<UObject*>& Assets;
    FString& Error;
    bool bCommitted = false;
};

template <typename T>
T* AddExpression(
    UMaterial* Material,
    const TCHAR* NodeId,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    T* Expression = EditorOnly
        ? NewObject<T>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = NodeId;
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
        EditorOnly->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpressionDepthFade* AddDepthFadeExpression(
    UMaterial* Material,
    const TCHAR* NodeId,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UClass* DepthFadeClass = LoadObject<UClass>(
        nullptr, TEXT("/Script/Engine.MaterialExpressionDepthFade"));
    UMaterialExpression* Expression = EditorOnly && DepthFadeClass &&
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
    EditorOnly->ExpressionCollection.Expressions.Add(Expression);
    // UE 5.5's DepthFade class is intentionally not ENGINE_API-exported. The
    // exact reflected class gate above makes this layout-only cast safe while
    // avoiding a plugin link against its unexported GetPrivateStaticClass.
    return static_cast<UMaterialExpressionDepthFade*>(Expression);
}

UMaterialExpressionScalarParameter* AddScalarParameter(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    float DefaultValue,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionVectorParameter* AddVectorParameter(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    const FLinearColor& DefaultValue,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionVectorParameter* Parameter =
        AddExpression<UMaterialExpressionVectorParameter>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionTextureSampleParameter2D* AddTextureParameter(
    UMaterial* Material,
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    UTexture2D* Texture,
    EMaterialSamplerType SamplerType,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionTextureSampleParameter2D* Parameter =
        AddExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = ParameterGroup;
        Parameter->Texture = Texture;
        Parameter->SamplerType = SamplerType;
        Parameter->SamplerSource = SSM_FromTextureAsset;
        Parameter->MipValueMode = TMVM_None;
        Parameter->ConstMipValue = 0;
        Parameter->AutomaticViewMipBias = true;
    }
    return Parameter;
}

UMaterialExpressionCustom* AddCustomExpression(
    UMaterial* Material,
    const TCHAR* NodeId,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    std::initializer_list<const TCHAR*> InputNames,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionCustom* Expression =
        AddExpression<UMaterialExpressionCustom>(
            Material, NodeId, EditorX, EditorY);
    if (!Expression)
    {
        return nullptr;
    }
    Expression->Description = Description;
    Expression->Code = Code;
    Expression->OutputType = OutputType;
    Expression->Inputs.Reset(static_cast<int32>(InputNames.size()));
    for (const TCHAR* InputName : InputNames)
    {
        FCustomInput& Input = Expression->Inputs.AddDefaulted_GetRef();
        Input.InputName = InputName;
    }
    return Expression;
}

struct FMaterialInstanceSpec
{
    FString AssetName;
    FString ObjectPath;
    FString ParentObjectPath;
    TArray<TPair<FName, float>> Scalars;
    TArray<TPair<FName, FLinearColor>> Vectors;
};

FMaterialInstanceSpec MakeGrassInstanceSpec()
{
    FMaterialInstanceSpec Spec;
    Spec.AssetName = GrassInstanceName;
    Spec.ObjectPath = GrassInstanceObjectPath;
    Spec.ParentObjectPath = GrassBaseObjectPath;
    Spec.Scalars.Emplace(TEXT("Desaturation"), GrassDesaturation);
    Spec.Scalars.Emplace(
        TEXT("SecondaryBlendStrength"), GrassSecondaryBlend);
    Spec.Scalars.Emplace(
        TEXT("MicroNormalStrength"), GrassMicroNormalStrength);
    Spec.Vectors.Emplace(TEXT("LawnTint"), GrassLawnTint);
    return Spec;
}

FMaterialInstanceSpec MakeContextInstanceSpec(
    const FString& AssetName,
    const FString& ObjectPath,
    const FString& ParentObjectPath,
    float TileMeters,
    float DetailTileMeters,
    float MacroMeters,
    float NormalStrength,
    float DetailNormalStrength,
    float RoughnessBias,
    float MacroAlbedoStrength,
    float MacroRoughnessStrength,
    float HeightMillimetres)
{
    FMaterialInstanceSpec Spec;
    Spec.AssetName = AssetName;
    Spec.ObjectPath = ObjectPath;
    Spec.ParentObjectPath = ParentObjectPath;
    Spec.Scalars.Emplace(TEXT("TileMeters"), TileMeters);
    Spec.Scalars.Emplace(TEXT("DetailTileMeters"), DetailTileMeters);
    Spec.Scalars.Emplace(TEXT("MacroTileMeters"), MacroMeters);
    Spec.Scalars.Emplace(TEXT("NormalStrength"), NormalStrength);
    Spec.Scalars.Emplace(
        TEXT("DetailNormalStrength"), DetailNormalStrength);
    Spec.Scalars.Emplace(TEXT("RoughnessBias"), RoughnessBias);
    Spec.Scalars.Emplace(
        TEXT("MacroAlbedoStrength"), MacroAlbedoStrength);
    Spec.Scalars.Emplace(
        TEXT("MacroRoughnessStrength"), MacroRoughnessStrength);
    Spec.Scalars.Emplace(TEXT("WeatheringStrength"), 0.0f);
    Spec.Scalars.Emplace(TEXT("SillDirtStrength"), 0.0f);
    Spec.Scalars.Emplace(TEXT("CorniceRunoffStrength"), 0.0f);
    Spec.Scalars.Emplace(TEXT("GroundContactDampStrength"), 0.0f);
    Spec.Scalars.Emplace(TEXT("CavityDirtStrength"), 0.0f);
    Spec.Scalars.Emplace(TEXT("HeightMillimetres"), HeightMillimetres);
    Spec.Scalars.Emplace(TEXT("BumpOffsetStrength"), 0.0f);
    Spec.Scalars.Emplace(TEXT("ExposedMetalMaskStrength"), 0.0f);
    return Spec;
}

const TArray<FMaterialInstanceSpec>& ContextInstanceSpecs()
{
    static const TArray<FMaterialInstanceSpec> Specs = {
        MakeContextInstanceSpec(
            HardscapeStoneName,
            HardscapeStoneObjectPath,
            HeroStoneParentPath,
            1.8f,
            0.14f,
            10.0f,
            0.42f,
            0.36f,
            0.07f,
            0.010f,
            0.020f,
            0.45f),
        MakeContextInstanceSpec(
            ContextRenderName,
            ContextRenderObjectPath,
            HeroRenderParentPath,
            2.5f,
            0.16f,
            14.0f,
            0.42f,
            0.28f,
            0.06f,
            0.010f,
            0.022f,
            0.35f),
        MakeContextInstanceSpec(
            ContextRoofName,
            ContextRoofObjectPath,
            HeroSlateParentPath,
            4.0f,
            0.16f,
            14.0f,
            0.50f,
            0.35f,
            0.08f,
            0.014f,
            0.028f,
            0.40f)};
    return Specs;
}

bool LoadGrassTextures(
    UTexture2D*& OutBaseColor,
    UTexture2D*& OutNormalDx,
    UTexture2D*& OutRoughness,
    UTexture2D*& OutAo,
    FString& OutError)
{
    OutBaseColor = LoadExact<UTexture2D>(GrassBaseColorTexturePath);
    OutNormalDx = LoadExact<UTexture2D>(GrassNormalDxTexturePath);
    OutRoughness = LoadExact<UTexture2D>(GrassRoughnessTexturePath);
    OutAo = LoadExact<UTexture2D>(GrassAoTexturePath);
    if (!OutBaseColor || !OutNormalDx || !OutRoughness || !OutAo)
    {
        OutError =
            TEXT("Explore V5 grass requires the four exact owned V4 Grass001 textures.");
        return false;
    }
    if (!OutBaseColor->SRGB ||
        OutBaseColor->CompressionSettings != TC_Default ||
        OutBaseColor->AddressX != TA_Wrap ||
        OutBaseColor->AddressY != TA_Wrap ||
        OutNormalDx->SRGB ||
        OutNormalDx->CompressionSettings != TC_Normalmap ||
        OutNormalDx->AddressX != TA_Wrap ||
        OutNormalDx->AddressY != TA_Wrap ||
        OutNormalDx->bFlipGreenChannel ||
        OutRoughness->SRGB ||
        OutRoughness->CompressionSettings != TC_Masks ||
        OutRoughness->AddressX != TA_Wrap ||
        OutRoughness->AddressY != TA_Wrap ||
        OutAo->SRGB ||
        OutAo->CompressionSettings != TC_Masks ||
        OutAo->AddressX != TA_Wrap ||
        OutAo->AddressY != TA_Wrap)
    {
        OutError =
            TEXT("The exact V4 Grass001 textures no longer have their proven color/NormalDX/mask, wrap-address, or DirectX-normal import settings.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ParentSupportsSpec(
    UMaterialInterface* Parent,
    const FMaterialInstanceSpec& Spec,
    FString& OutError)
{
    if (!Parent || Parent->GetPathName() != Spec.ParentObjectPath ||
        Parent->GetNaniteOverride())
    {
        OutError = FString::Printf(
            TEXT("Explore V5 material '%s' lacks its exact proven override-free parent '%s'."),
            *Spec.AssetName,
            *Spec.ParentObjectPath);
        return false;
    }
    for (const TPair<FName, float>& Pair : Spec.Scalars)
    {
        float ExistingValue = 0.0f;
        if (!Parent->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Pair.Key),
                ExistingValue,
                false))
        {
            OutError = FString::Printf(
                TEXT("Proven parent '%s' lacks scalar parameter '%s'."),
                *Spec.ParentObjectPath,
                *Pair.Key.ToString());
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : Spec.Vectors)
    {
        FLinearColor ExistingValue;
        if (!Parent->GetVectorParameterValue(
                FHashedMaterialParameterInfo(Pair.Key),
                ExistingValue,
                false))
        {
            OutError = FString::Printf(
                TEXT("Parent '%s' lacks vector parameter '%s'."),
                *Spec.ParentObjectPath,
                *Pair.Key.ToString());
            return false;
        }
    }
    OutError.Reset();
    return true;
}

UMaterial* CreateGrassBase(
    IAssetTools& AssetTools,
    UTexture2D* BaseColorTexture,
    UTexture2D* NormalDxTexture,
    UTexture2D* RoughnessTexture,
    UTexture2D* AoTexture,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            GrassBaseName,
            MaterialRoot,
            UMaterial::StaticClass(),
            Factory,
            AssetCreateContext))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the fresh Explore V5 lawn base material.");
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* Uv0 =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("Grass.UV0_Metres"), -1900, 0);
    UMaterialExpressionScalarParameter* PrimaryMeters = AddScalarParameter(
        Material, TEXT("Grass.PrimaryTileMeters"),
        TEXT("PrimaryTileMeters"), PrimaryTileMeters, -1900, 120);
    UMaterialExpressionDivide* PrimaryUv =
        AddExpression<UMaterialExpressionDivide>(
            Material, TEXT("Grass.PrimaryUV"), -1680, 0);
    UMaterialExpressionScalarParameter* SecondaryMeters = AddScalarParameter(
        Material, TEXT("Grass.SecondaryTileMeters"),
        TEXT("SecondaryTileMeters"), SecondaryTileMeters, -1900, 260);
    UMaterialExpressionDivide* SecondaryUnrotated =
        AddExpression<UMaterialExpressionDivide>(
            Material, TEXT("Grass.SecondaryUnrotatedUV"), -1680, 230);
    UMaterialExpressionConstant* RotationRadians =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("Grass.StaticRotation37Degrees"), -1680, 360);
    UMaterialExpressionRotator* RotatedSecondary =
        AddExpression<UMaterialExpressionRotator>(
            Material, TEXT("Grass.RotateSecondaryUV"), -1450, 230);
    UMaterialExpressionConstant2Vector* SecondaryOffset =
        AddExpression<UMaterialExpressionConstant2Vector>(
            Material, TEXT("Grass.SecondaryStaticOffset"), -1450, 370);
    UMaterialExpressionAdd* SecondaryUv =
        AddExpression<UMaterialExpressionAdd>(
            Material, TEXT("Grass.SecondaryUV"), -1220, 230);
    UMaterialExpressionScalarParameter* MacroMeters = AddScalarParameter(
        Material, TEXT("Grass.MacroTileMeters"),
        TEXT("MacroTileMeters"), MacroTileMeters, -1900, 500);
    UMaterialExpressionDivide* MacroUv =
        AddExpression<UMaterialExpressionDivide>(
            Material, TEXT("Grass.MacroUV"), -1680, 500);

    UMaterialExpressionTextureSampleParameter2D* BasePrimary =
        AddTextureParameter(
            Material, TEXT("Grass.BaseColorPrimary"),
            TEXT("Grass001BaseColorPrimary"), BaseColorTexture,
            SAMPLERTYPE_Color, -980, -520);
    UMaterialExpressionTextureSampleParameter2D* BaseSecondary =
        AddTextureParameter(
            Material, TEXT("Grass.BaseColorSecondary"),
            TEXT("Grass001BaseColorSecondary"), BaseColorTexture,
            SAMPLERTYPE_Color, -980, -380);
    UMaterialExpressionTextureSampleParameter2D* NormalPrimary =
        AddTextureParameter(
            Material, TEXT("Grass.NormalDXPrimary"),
            TEXT("Grass001NormalDXPrimary"), NormalDxTexture,
            SAMPLERTYPE_Normal, -980, -180);
    UMaterialExpressionTextureSampleParameter2D* NormalSecondary =
        AddTextureParameter(
            Material, TEXT("Grass.NormalDXSecondary"),
            TEXT("Grass001NormalDXSecondary"), NormalDxTexture,
            SAMPLERTYPE_Normal, -980, -40);
    UMaterialExpressionTextureSampleParameter2D* RoughPrimary =
        AddTextureParameter(
            Material, TEXT("Grass.RoughnessPrimary"),
            TEXT("Grass001RoughnessPrimary"), RoughnessTexture,
            SAMPLERTYPE_Masks, -980, 170);
    UMaterialExpressionTextureSampleParameter2D* RoughSecondary =
        AddTextureParameter(
            Material, TEXT("Grass.RoughnessSecondary"),
            TEXT("Grass001RoughnessSecondary"), RoughnessTexture,
            SAMPLERTYPE_Masks, -980, 310);
    UMaterialExpressionTextureSampleParameter2D* AoPrimary =
        AddTextureParameter(
            Material, TEXT("Grass.AOPrimary"),
            TEXT("Grass001AOPrimary"), AoTexture,
            SAMPLERTYPE_Masks, -980, 500);
    UMaterialExpressionTextureSampleParameter2D* AoSecondary =
        AddTextureParameter(
            Material, TEXT("Grass.AOSecondary"),
            TEXT("Grass001AOSecondary"), AoTexture,
            SAMPLERTYPE_Masks, -980, 640);
    UMaterialExpressionCustom* MacroField = AddCustomExpression(
        Material,
        TEXT("Grass.ProceduralMacroField"),
        GrassMacroFieldDescription,
        GrassMacroFieldCode,
        CMOT_Float1,
        {TEXT("MacroUV")},
        -980,
        810);

    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("Grass.WorldPositionForDistanceOnly"), -1450, 720);
    UMaterialExpressionCameraPositionWS* CameraPosition =
        AddExpression<UMaterialExpressionCameraPositionWS>(
            Material, TEXT("Grass.CameraPositionForDistanceOnly"), -1450, 830);
    UMaterialExpressionSubtract* CameraDelta =
        AddExpression<UMaterialExpressionSubtract>(
            Material, TEXT("Grass.CameraDelta"), -1220, 760);
    UMaterialExpressionLength* CameraDistance =
        AddExpression<UMaterialExpressionLength>(
            Material, TEXT("Grass.CameraDistanceCm"), -1000, 950);
    UMaterialExpressionScalarParameter* FadeStart = AddScalarParameter(
        Material, TEXT("Grass.MicroFadeStartCm"),
        TEXT("MicroFadeStartCm"), GrassFadeStartCm, -780, 950);
    UMaterialExpressionSubtract* DistancePastStart =
        AddExpression<UMaterialExpressionSubtract>(
            Material, TEXT("Grass.DistancePastFadeStart"), -560, 950);
    UMaterialExpressionScalarParameter* FadeRange = AddScalarParameter(
        Material, TEXT("Grass.MicroFadeRangeCm"),
        TEXT("MicroFadeRangeCm"), GrassFadeRangeCm, -560, 1080);
    UMaterialExpressionDivide* FadeRatio =
        AddExpression<UMaterialExpressionDivide>(
            Material, TEXT("Grass.DistanceFadeRatio"), -340, 950);
    UMaterialExpressionSaturate* FarFade =
        AddExpression<UMaterialExpressionSaturate>(
            Material, TEXT("Grass.SaturatedFarFade"), -120, 950);
    UMaterialExpressionOneMinus* NearFade =
        AddExpression<UMaterialExpressionOneMinus>(
            Material, TEXT("Grass.NearMicroFade"), 100, 950);
    UMaterialExpressionScalarParameter* SecondaryStrength =
        AddScalarParameter(
            Material, TEXT("Grass.SecondaryBlendStrength"),
            TEXT("SecondaryBlendStrength"), GrassSecondaryBlend,
            100, 1060);
    UMaterialExpressionMultiply* SecondaryAlpha =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("Grass.SecondaryBlendAlpha"), 320, 950);

    UMaterialExpressionLinearInterpolate* BaseBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Grass.BaseColorDetailBlend"), -700, -450);
    UMaterialExpressionScalarParameter* Desaturation = AddScalarParameter(
        Material, TEXT("Grass.Desaturation"),
        TEXT("Desaturation"), GrassDesaturation, -470, -540);
    UMaterialExpressionDesaturation* DesaturatedBase =
        AddExpression<UMaterialExpressionDesaturation>(
            Material, TEXT("Grass.DesaturateBaseColor"), -250, -450);
    UMaterialExpressionVectorParameter* LawnTint = AddVectorParameter(
        Material, TEXT("Grass.LawnTint"),
        TEXT("LawnTint"), GrassLawnTint, -250, -580);
    UMaterialExpressionMultiply* TintedColor =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("Grass.DarkerTintedColor"), -20, -450);
    UMaterialExpressionVectorParameter* MacroTintLow = AddVectorParameter(
        Material, TEXT("Grass.MacroTintLow"),
        TEXT("MacroTintLow"), GrassMacroTintLow, -20, -610);
    UMaterialExpressionVectorParameter* MacroTintHigh = AddVectorParameter(
        Material, TEXT("Grass.MacroTintHigh"),
        TEXT("MacroTintHigh"), GrassMacroTintHigh, -20, -720);
    UMaterialExpressionLinearInterpolate* MacroTint =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Grass.LowAmplitudeMacroTint"), 210, -560);
    UMaterialExpressionMultiply* FinalBaseColor =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("Grass.FinalBaseColor"), 450, -450);

    UMaterialExpressionLinearInterpolate* RoughnessBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Grass.RoughnessDetailBlend"), -700, 210);
    UMaterialExpressionScalarParameter* RoughnessBias = AddScalarParameter(
        Material, TEXT("Grass.RoughnessBias"),
        TEXT("RoughnessBias"), GrassRoughnessBias, -470, 180);
    UMaterialExpressionAdd* RoughnessAdd =
        AddExpression<UMaterialExpressionAdd>(
            Material, TEXT("Grass.RoughnessWithBias"), -250, 210);
    UMaterialExpressionSaturate* FinalRoughness =
        AddExpression<UMaterialExpressionSaturate>(
            Material, TEXT("Grass.FinalRoughness"), -20, 210);

    UMaterialExpressionLinearInterpolate* AoBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Grass.AODetailBlend"), -700, 540);

    UMaterialExpressionCustom* RotatedNormal = AddCustomExpression(
        Material,
        TEXT("Grass.ReorientRotatedNormal37Degrees"),
        TEXT("TRIAD_IPV5_ReorientRotatedNormal37Degrees"),
        GrassRotatedNormalCode,
        CMOT_Float3,
        {TEXT("RotatedNormal")},
        -700,
        -30);
    UMaterialExpressionLinearInterpolate* NormalBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Grass.NormalDetailBlend"), -470, -30);
    UMaterialExpressionConstant3Vector* FlatNormal =
        AddExpression<UMaterialExpressionConstant3Vector>(
            Material, TEXT("Grass.FlatTangentNormal"), -250, -80);
    UMaterialExpressionScalarParameter* MicroNormalStrength =
        AddScalarParameter(
            Material, TEXT("Grass.MicroNormalStrength"),
            TEXT("MicroNormalStrength"), GrassMicroNormalStrength,
            -250, 30);
    UMaterialExpressionLinearInterpolate* MicroNormalAlpha =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Grass.DistanceFadedMicroNormalAlpha"), -20, 0);
    UMaterialExpressionLinearInterpolate* MicroNormalBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("Grass.DistanceFadedMicroNormal"), 210, -30);
    UMaterialExpressionNormalize* FinalNormal =
        AddExpression<UMaterialExpressionNormalize>(
            Material, TEXT("Grass.FinalNormalizedNormal"), 450, -30);

    if (!Uv0 || !PrimaryMeters || !PrimaryUv || !SecondaryMeters ||
        !SecondaryUnrotated || !RotationRadians || !RotatedSecondary ||
        !SecondaryOffset || !SecondaryUv || !MacroMeters || !MacroUv ||
        !BasePrimary || !BaseSecondary || !NormalPrimary || !NormalSecondary ||
        !RoughPrimary || !RoughSecondary || !AoPrimary || !AoSecondary ||
        !MacroField || !WorldPosition || !CameraPosition || !CameraDelta ||
        !CameraDistance || !FadeStart || !DistancePastStart || !FadeRange ||
        !FadeRatio || !FarFade || !NearFade || !SecondaryStrength ||
        !SecondaryAlpha || !BaseBlend || !Desaturation || !DesaturatedBase ||
        !LawnTint || !TintedColor || !MacroTintLow || !MacroTintHigh ||
        !MacroTint || !FinalBaseColor || !RoughnessBlend || !RoughnessBias ||
        !RoughnessAdd || !FinalRoughness || !AoBlend || !RotatedNormal ||
        !NormalBlend || !FlatNormal || !MicroNormalStrength ||
        !MicroNormalAlpha || !MicroNormalBlend || !FinalNormal)
    {
        OutError = TEXT("Could not allocate the exact Explore V5 lawn graph.");
        return nullptr;
    }

    Uv0->CoordinateIndex = 0;
    Uv0->UTiling = 1.0f;
    Uv0->VTiling = 1.0f;
    PrimaryUv->A.Connect(0, Uv0);
    PrimaryUv->B.Connect(0, PrimaryMeters);
    SecondaryUnrotated->A.Connect(0, Uv0);
    SecondaryUnrotated->B.Connect(0, SecondaryMeters);
    RotationRadians->R = SecondaryRotationRadians;
    RotatedSecondary->Coordinate.Connect(0, SecondaryUnrotated);
    RotatedSecondary->Time.Connect(0, RotationRadians);
    RotatedSecondary->CenterX = 0.0f;
    RotatedSecondary->CenterY = 0.0f;
    RotatedSecondary->Speed = 1.0f;
    SecondaryOffset->R = SecondaryOffsetU;
    SecondaryOffset->G = SecondaryOffsetV;
    SecondaryUv->A.Connect(0, RotatedSecondary);
    SecondaryUv->B.Connect(0, SecondaryOffset);
    MacroUv->A.Connect(0, Uv0);
    MacroUv->B.Connect(0, MacroMeters);

    BasePrimary->Coordinates.Connect(0, PrimaryUv);
    BaseSecondary->Coordinates.Connect(0, SecondaryUv);
    NormalPrimary->Coordinates.Connect(0, PrimaryUv);
    NormalSecondary->Coordinates.Connect(0, SecondaryUv);
    RoughPrimary->Coordinates.Connect(0, PrimaryUv);
    RoughSecondary->Coordinates.Connect(0, SecondaryUv);
    AoPrimary->Coordinates.Connect(0, PrimaryUv);
    AoSecondary->Coordinates.Connect(0, SecondaryUv);
    MacroField->Inputs[0].Input.Connect(0, MacroUv);

    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    CameraDelta->A.Connect(0, WorldPosition);
    CameraDelta->B.Connect(0, CameraPosition);
    CameraDistance->Input.Connect(0, CameraDelta);
    DistancePastStart->A.Connect(0, CameraDistance);
    DistancePastStart->B.Connect(0, FadeStart);
    FadeRatio->A.Connect(0, DistancePastStart);
    FadeRatio->B.Connect(0, FadeRange);
    FarFade->Input.Connect(0, FadeRatio);
    NearFade->Input.Connect(0, FarFade);
    SecondaryAlpha->A.Connect(0, NearFade);
    SecondaryAlpha->B.Connect(0, SecondaryStrength);

    BaseBlend->A.Connect(0, BasePrimary);
    BaseBlend->B.Connect(0, BaseSecondary);
    BaseBlend->Alpha.Connect(0, SecondaryAlpha);
    DesaturatedBase->LuminanceFactors = GrassLuminanceFactors;
    DesaturatedBase->Input.Connect(0, BaseBlend);
    DesaturatedBase->Fraction.Connect(0, Desaturation);
    TintedColor->A.Connect(0, DesaturatedBase);
    TintedColor->B.Connect(0, LawnTint);
    MacroTint->A.Connect(0, MacroTintLow);
    MacroTint->B.Connect(0, MacroTintHigh);
    MacroTint->Alpha.Connect(0, MacroField);
    FinalBaseColor->A.Connect(0, TintedColor);
    FinalBaseColor->B.Connect(0, MacroTint);

    RoughnessBlend->A.Connect(1, RoughPrimary);
    RoughnessBlend->B.Connect(1, RoughSecondary);
    RoughnessBlend->Alpha.Connect(0, SecondaryAlpha);
    RoughnessAdd->A.Connect(0, RoughnessBlend);
    RoughnessAdd->B.Connect(0, RoughnessBias);
    FinalRoughness->Input.Connect(0, RoughnessAdd);

    AoBlend->A.Connect(1, AoPrimary);
    AoBlend->B.Connect(1, AoSecondary);
    AoBlend->Alpha.Connect(0, SecondaryAlpha);

    RotatedNormal->Inputs[0].Input.Connect(0, NormalSecondary);
    NormalBlend->A.Connect(0, NormalPrimary);
    NormalBlend->B.Connect(0, RotatedNormal);
    NormalBlend->Alpha.Connect(0, SecondaryAlpha);
    FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
    MicroNormalAlpha->ConstA = GrassFarMicroNormalStrength;
    MicroNormalAlpha->B.Connect(0, MicroNormalStrength);
    MicroNormalAlpha->Alpha.Connect(0, NearFade);
    MicroNormalBlend->A.Connect(0, FlatNormal);
    MicroNormalBlend->B.Connect(0, NormalBlend);
    MicroNormalBlend->Alpha.Connect(0, MicroNormalAlpha);
    FinalNormal->VectorInput.Connect(0, MicroNormalBlend);

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    EditorOnly->BaseColor.Connect(0, FinalBaseColor);
    EditorOnly->Normal.Connect(0, FinalNormal);
    EditorOnly->Roughness.Connect(0, FinalRoughness);
    EditorOnly->AmbientOcclusion.Connect(0, AoBlend);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return Material;
}

UMaterial* CreateWaterMaterial(
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
            FountainWaterName,
            MaterialRoot,
            UMaterial::StaticClass(),
            Factory,
            AssetCreateContext))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the fresh Explore V5 fountain-water material.");
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* Uv0 =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("Water.UV0"), -1050, -220);
    UMaterialExpressionTime* Time = AddExpression<UMaterialExpressionTime>(
        Material, TEXT("Water.TimeSeconds"), -1050, -100);
    UMaterialExpressionConstant2Vector* VelocityA =
        AddExpression<UMaterialExpressionConstant2Vector>(
            Material, TEXT("Water.VelocityA_UVPerSecond"), -1050, 20);
    UMaterialExpressionConstant2Vector* VelocityB =
        AddExpression<UMaterialExpressionConstant2Vector>(
            Material, TEXT("Water.VelocityB_UVPerSecond"), -1050, 130);
    UMaterialExpressionConstant* FieldScaleA =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("Water.FieldScaleA"), -1050, 240);
    UMaterialExpressionConstant* FieldScaleB =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("Water.FieldScaleB"), -1050, 350);
    UMaterialExpressionCustom* AnalyticNormal = AddCustomExpression(
        Material,
        TEXT("Water.AnalyticTwoFieldNormal"),
        TEXT("TRIAD_IPV5_AnalyticTwoNonparallelMovingNormals"),
        WaterAnalyticNormalCode,
        CMOT_Float3,
        {TEXT("UV0"), TEXT("TimeSeconds"), TEXT("VelocityA"),
         TEXT("VelocityB"), TEXT("FieldScaleA"), TEXT("FieldScaleB")},
        -720,
        -40);
    UMaterialExpressionVectorParameter* Tint = AddVectorParameter(
        Material, TEXT("Water.Tint"), TEXT("WaterTint"),
        WaterTint, -400, -300);
    UMaterialExpressionScalarParameter* Roughness = AddScalarParameter(
        Material, TEXT("Water.Roughness"), TEXT("Roughness"),
        WaterRoughness, -400, -180);
    UMaterialExpressionScalarParameter* Specular = AddScalarParameter(
        Material, TEXT("Water.Specular"), TEXT("Specular"),
        WaterSpecular, -400, -60);
    UMaterialExpressionScalarParameter* Opacity = AddScalarParameter(
        Material, TEXT("Water.Opacity"), TEXT("Opacity"),
        WaterOpacity, -400, 80);
    UMaterialExpressionClamp* ClampedOpacity =
        AddExpression<UMaterialExpressionClamp>(
            Material, TEXT("Water.ClampOpacity35To65"), -150, 40);
    UMaterialExpressionScalarParameter* DepthFadeDistance = AddScalarParameter(
        Material, TEXT("Water.DepthFadeDistanceCm"), TEXT("DepthFadeDistanceCm"),
        WaterDepthFadeCm, -400, 200);
    UMaterialExpressionClamp* ClampedDepthFadeDistance =
        AddExpression<UMaterialExpressionClamp>(
            Material, TEXT("Water.ClampDepthFadeDistance25To150"), -150, 180);
    UMaterialExpressionDepthFade* DepthFade = AddDepthFadeExpression(
        Material, TEXT("Water.DepthFade80Cm"), 100, 80);
    UMaterialExpressionScalarParameter* Ior = AddScalarParameter(
        Material, TEXT("Water.IOR"), TEXT("IOR"),
        WaterIor, 100, 260);
    if (!Uv0 || !Time || !VelocityA || !VelocityB || !FieldScaleA ||
        !FieldScaleB || !AnalyticNormal || !Tint || !Roughness || !Specular ||
        !Opacity || !ClampedOpacity || !DepthFadeDistance ||
        !ClampedDepthFadeDistance || !DepthFade || !Ior)
    {
        OutError = TEXT("Could not allocate the exact analytic fountain-water graph.");
        return nullptr;
    }

    Uv0->CoordinateIndex = 0;
    Uv0->UTiling = 1.0f;
    Uv0->VTiling = 1.0f;
    Time->bIgnorePause = false;
    Time->bOverride_Period = false;
    Time->Period = 0.0f;
    VelocityA->R = WaterVelocityAx;
    VelocityA->G = WaterVelocityAy;
    VelocityB->R = WaterVelocityBx;
    VelocityB->G = WaterVelocityBy;
    FieldScaleA->R = WaterFieldScaleA;
    FieldScaleB->R = WaterFieldScaleB;
    AnalyticNormal->Inputs[0].Input.Connect(0, Uv0);
    AnalyticNormal->Inputs[1].Input.Connect(0, Time);
    AnalyticNormal->Inputs[2].Input.Connect(0, VelocityA);
    AnalyticNormal->Inputs[3].Input.Connect(0, VelocityB);
    AnalyticNormal->Inputs[4].Input.Connect(0, FieldScaleA);
    AnalyticNormal->Inputs[5].Input.Connect(0, FieldScaleB);
    Opacity->SliderMin = WaterOpacityMin;
    Opacity->SliderMax = WaterOpacityMax;
    ClampedOpacity->Input.Connect(0, Opacity);
    ClampedOpacity->ClampMode = CMODE_Clamp;
    ClampedOpacity->MinDefault = WaterOpacityMin;
    ClampedOpacity->MaxDefault = WaterOpacityMax;
    DepthFadeDistance->SliderMin = WaterDepthFadeMinCm;
    DepthFadeDistance->SliderMax = WaterDepthFadeMaxCm;
    ClampedDepthFadeDistance->Input.Connect(0, DepthFadeDistance);
    ClampedDepthFadeDistance->ClampMode = CMODE_Clamp;
    ClampedDepthFadeDistance->MinDefault = WaterDepthFadeMinCm;
    ClampedDepthFadeDistance->MaxDefault = WaterDepthFadeMaxCm;
    DepthFade->InOpacity.Connect(0, ClampedOpacity);
    DepthFade->FadeDistance.Connect(0, ClampedDepthFadeDistance);
    DepthFade->OpacityDefault = WaterOpacity;
    DepthFade->FadeDistanceDefault = WaterDepthFadeCm;

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Translucent;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;
    Material->RefractionMethod = RM_IndexOfRefraction;
    Material->bScreenSpaceReflections = true;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    EditorOnly->BaseColor.Connect(0, Tint);
    EditorOnly->Roughness.Connect(0, Roughness);
    EditorOnly->Specular.Connect(0, Specular);
    EditorOnly->Normal.Connect(0, AnalyticNormal);
    EditorOnly->Opacity.Connect(0, DepthFade);
    EditorOnly->Refraction.Connect(0, Ior);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return Material;
}

UMaterialInstanceConstant* CreateMaterialInstance(
    IAssetTools& AssetTools,
    const FMaterialInstanceSpec& Spec,
    UMaterialInterface* Parent,
    FString& OutError)
{
    if (!ParentSupportsSpec(Parent, Spec, OutError))
    {
        return nullptr;
    }
    UMaterialInstanceConstantFactoryNew* Factory =
        NewObject<UMaterialInstanceConstantFactoryNew>();
    if (Factory)
    {
        Factory->InitialParent = Parent;
    }
    UMaterialInstanceConstant* Instance = Factory
        ? Cast<UMaterialInstanceConstant>(AssetTools.CreateAsset(
            Spec.AssetName,
            MaterialRoot,
            UMaterialInstanceConstant::StaticClass(),
            Factory,
            AssetCreateContext))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create Explore V5 material instance '%s'."),
            *Spec.AssetName);
        return nullptr;
    }
    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    Instance->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Instance->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    for (const TPair<FName, float>& Pair : Spec.Scalars)
    {
        Instance->SetScalarParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    for (const TPair<FName, FLinearColor>& Pair : Spec.Vectors)
    {
        Instance->SetVectorParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    OutError.Reset();
    return Instance;
}

bool InputIs(
    const FExpressionInput& Input,
    const UMaterialExpression* Expression,
    int32 OutputIndex = 0)
{
    return Input.Expression == Expression && Input.OutputIndex == OutputIndex;
}

bool BuildExactNodeMap(
    const UMaterialEditorOnlyData* EditorOnly,
    int32 ExpectedCount,
    TMap<FString, const UMaterialExpression*>& OutNodes,
    FString& OutError)
{
    OutNodes.Reset();
    if (!EditorOnly ||
        EditorOnly->ExpressionCollection.Expressions.Num() != ExpectedCount)
    {
        OutError = FString::Printf(
            TEXT("Material graph has %d expressions; expected exactly %d."),
            EditorOnly
                ? EditorOnly->ExpressionCollection.Expressions.Num()
                : -1,
            ExpectedCount);
        return false;
    }
    for (const UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        if (!Expression || Expression->Desc.IsEmpty() ||
            OutNodes.Contains(Expression->Desc))
        {
            OutError =
                TEXT("Material graph has a null, unnamed, or duplicate sealed expression.");
            return false;
        }
        const FString ClassName = Expression->GetClass()->GetName();
        if (ClassName.Contains(TEXT("RuntimeVirtualTexture")) ||
            ClassName.Contains(TEXT("VirtualTextureOutput")) ||
            ClassName.Contains(TEXT("Displacement")))
        {
            OutError = FString::Printf(
                TEXT("Material graph contains forbidden expression class '%s'."),
                *ClassName);
            return false;
        }
        OutNodes.Add(Expression->Desc, Expression);
    }
    OutError.Reset();
    return true;
}

template <typename T>
const T* ExactNode(
    const TMap<FString, const UMaterialExpression*>& Nodes,
    const TCHAR* NodeId)
{
    const UMaterialExpression* const* Found = Nodes.Find(NodeId);
    return Found ? Cast<T>(*Found) : nullptr;
}

const UMaterialExpressionDepthFade* ExactDepthFadeNode(
    const TMap<FString, const UMaterialExpression*>& Nodes,
    const TCHAR* NodeId)
{
    const UMaterialExpression* const* Found = Nodes.Find(NodeId);
    const UMaterialExpression* Expression = Found ? *Found : nullptr;
    return Expression &&
            Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionDepthFade")
        ? static_cast<const UMaterialExpressionDepthFade*>(Expression)
        : nullptr;
}

bool ExactScalar(
    const UMaterialExpressionScalarParameter* Parameter,
    const TCHAR* Name,
    float Value)
{
    return Parameter && Parameter->ParameterName == FName(Name) &&
        Parameter->Group == ParameterGroup &&
        !Parameter->bUseCustomPrimitiveData &&
        Parameter->PrimitiveDataIndex == 0 &&
        FMath::IsNearlyEqual(Parameter->DefaultValue, Value, 0.000001f);
}

bool ExactVector(
    const UMaterialExpressionVectorParameter* Parameter,
    const TCHAR* Name,
    const FLinearColor& Value)
{
    return Parameter && Parameter->ParameterName == FName(Name) &&
        Parameter->Group == ParameterGroup &&
        !Parameter->bUseCustomPrimitiveData &&
        Parameter->PrimitiveDataIndex == 0 &&
        Parameter->DefaultValue.Equals(Value, 0.000001f);
}

bool ExactClamp(
    const UMaterialExpressionClamp* Clamp,
    const UMaterialExpression* Input,
    float MinValue,
    float MaxValue)
{
    return Clamp && InputIs(Clamp->Input, Input) &&
        !Clamp->Min.Expression && !Clamp->Max.Expression &&
        Clamp->ClampMode == CMODE_Clamp &&
        FMath::IsNearlyEqual(Clamp->MinDefault, MinValue, 0.000001f) &&
        FMath::IsNearlyEqual(Clamp->MaxDefault, MaxValue, 0.000001f);
}

bool ExactTexture(
    const UMaterialExpressionTextureSampleParameter2D* Parameter,
    const TCHAR* Name,
    const FString& TexturePath,
    EMaterialSamplerType SamplerType)
{
    return Parameter && Parameter->ParameterName == FName(Name) &&
        Parameter->Group == ParameterGroup && Parameter->Texture &&
        Parameter->Texture->GetPathName() == TexturePath &&
        Parameter->SamplerType == SamplerType &&
        Parameter->SamplerSource == SSM_FromTextureAsset &&
        Parameter->MipValueMode == TMVM_None &&
        Parameter->AutomaticViewMipBias;
}

bool HasNoCustomizedUvConnections(const UMaterialEditorOnlyData* EditorOnly)
{
    if (!EditorOnly)
    {
        return false;
    }
    for (const FVector2MaterialInput& CustomizedUv : EditorOnly->CustomizedUVs)
    {
        if (CustomizedUv.Expression)
        {
            return false;
        }
    }
    return true;
}

bool HasNoDisplacementOrMaterialAttributePath(
    const UMaterial* Material,
    const UMaterialEditorOnlyData* EditorOnly)
{
    return Material && EditorOnly &&
        !Material->bEnableTessellation &&
        !Material->bEnableDisplacementFade &&
        FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) &&
        !EditorOnly->WorldPositionOffset.Expression &&
        !EditorOnly->Displacement.Expression &&
        !EditorOnly->PixelDepthOffset.Expression &&
        !EditorOnly->MaterialAttributes.Expression &&
        !EditorOnly->FrontMaterial.Expression &&
        HasNoCustomizedUvConnections(EditorOnly);
}

bool ValidateGrassGraph(
    UMaterial* Material,
    UTexture2D* BaseColorTexture,
    UTexture2D* NormalDxTexture,
    UTexture2D* RoughnessTexture,
    UTexture2D* AoTexture,
    FString& OutError)
{
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    TMap<FString, const UMaterialExpression*> Nodes;
    if (!BuildExactNodeMap(EditorOnly, 53, Nodes, OutError))
    {
        return false;
    }

    const auto* Uv0 = ExactNode<UMaterialExpressionTextureCoordinate>(
        Nodes, TEXT("Grass.UV0_Metres"));
    const auto* PrimaryMeters = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.PrimaryTileMeters"));
    const auto* PrimaryUv = ExactNode<UMaterialExpressionDivide>(
        Nodes, TEXT("Grass.PrimaryUV"));
    const auto* SecondaryMeters = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.SecondaryTileMeters"));
    const auto* SecondaryUnrotated = ExactNode<UMaterialExpressionDivide>(
        Nodes, TEXT("Grass.SecondaryUnrotatedUV"));
    const auto* RotationRadians = ExactNode<UMaterialExpressionConstant>(
        Nodes, TEXT("Grass.StaticRotation37Degrees"));
    const auto* RotatedSecondary = ExactNode<UMaterialExpressionRotator>(
        Nodes, TEXT("Grass.RotateSecondaryUV"));
    const auto* SecondaryOffset = ExactNode<UMaterialExpressionConstant2Vector>(
        Nodes, TEXT("Grass.SecondaryStaticOffset"));
    const auto* SecondaryUv = ExactNode<UMaterialExpressionAdd>(
        Nodes, TEXT("Grass.SecondaryUV"));
    const auto* MacroMeters = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.MacroTileMeters"));
    const auto* MacroUv = ExactNode<UMaterialExpressionDivide>(
        Nodes, TEXT("Grass.MacroUV"));

    const auto* BasePrimary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.BaseColorPrimary"));
    const auto* BaseSecondary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.BaseColorSecondary"));
    const auto* NormalPrimary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.NormalDXPrimary"));
    const auto* NormalSecondary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.NormalDXSecondary"));
    const auto* RoughPrimary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.RoughnessPrimary"));
    const auto* RoughSecondary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.RoughnessSecondary"));
    const auto* AoPrimary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.AOPrimary"));
    const auto* AoSecondary =
        ExactNode<UMaterialExpressionTextureSampleParameter2D>(
            Nodes, TEXT("Grass.AOSecondary"));
    const auto* MacroField = ExactNode<UMaterialExpressionCustom>(
        Nodes, TEXT("Grass.ProceduralMacroField"));

    const auto* WorldPosition = ExactNode<UMaterialExpressionWorldPosition>(
        Nodes, TEXT("Grass.WorldPositionForDistanceOnly"));
    const auto* CameraPosition = ExactNode<UMaterialExpressionCameraPositionWS>(
        Nodes, TEXT("Grass.CameraPositionForDistanceOnly"));
    const auto* CameraDelta = ExactNode<UMaterialExpressionSubtract>(
        Nodes, TEXT("Grass.CameraDelta"));
    const auto* CameraDistance = ExactNode<UMaterialExpressionLength>(
        Nodes, TEXT("Grass.CameraDistanceCm"));
    const auto* FadeStart = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.MicroFadeStartCm"));
    const auto* DistancePastStart = ExactNode<UMaterialExpressionSubtract>(
        Nodes, TEXT("Grass.DistancePastFadeStart"));
    const auto* FadeRange = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.MicroFadeRangeCm"));
    const auto* FadeRatio = ExactNode<UMaterialExpressionDivide>(
        Nodes, TEXT("Grass.DistanceFadeRatio"));
    const auto* FarFade = ExactNode<UMaterialExpressionSaturate>(
        Nodes, TEXT("Grass.SaturatedFarFade"));
    const auto* NearFade = ExactNode<UMaterialExpressionOneMinus>(
        Nodes, TEXT("Grass.NearMicroFade"));
    const auto* SecondaryStrength = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.SecondaryBlendStrength"));
    const auto* SecondaryAlpha = ExactNode<UMaterialExpressionMultiply>(
        Nodes, TEXT("Grass.SecondaryBlendAlpha"));

    const auto* BaseBlend = ExactNode<UMaterialExpressionLinearInterpolate>(
        Nodes, TEXT("Grass.BaseColorDetailBlend"));
    const auto* Desaturation = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.Desaturation"));
    const auto* DesaturatedBase = ExactNode<UMaterialExpressionDesaturation>(
        Nodes, TEXT("Grass.DesaturateBaseColor"));
    const auto* LawnTint = ExactNode<UMaterialExpressionVectorParameter>(
        Nodes, TEXT("Grass.LawnTint"));
    const auto* TintedColor = ExactNode<UMaterialExpressionMultiply>(
        Nodes, TEXT("Grass.DarkerTintedColor"));
    const auto* MacroTintLow = ExactNode<UMaterialExpressionVectorParameter>(
        Nodes, TEXT("Grass.MacroTintLow"));
    const auto* MacroTintHigh = ExactNode<UMaterialExpressionVectorParameter>(
        Nodes, TEXT("Grass.MacroTintHigh"));
    const auto* MacroTint = ExactNode<UMaterialExpressionLinearInterpolate>(
        Nodes, TEXT("Grass.LowAmplitudeMacroTint"));
    const auto* FinalBaseColor = ExactNode<UMaterialExpressionMultiply>(
        Nodes, TEXT("Grass.FinalBaseColor"));

    const auto* RoughnessBlend =
        ExactNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Grass.RoughnessDetailBlend"));
    const auto* RoughnessBias = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Grass.RoughnessBias"));
    const auto* RoughnessAdd = ExactNode<UMaterialExpressionAdd>(
        Nodes, TEXT("Grass.RoughnessWithBias"));
    const auto* FinalRoughness = ExactNode<UMaterialExpressionSaturate>(
        Nodes, TEXT("Grass.FinalRoughness"));
    const auto* AoBlend = ExactNode<UMaterialExpressionLinearInterpolate>(
        Nodes, TEXT("Grass.AODetailBlend"));

    const auto* RotatedNormal = ExactNode<UMaterialExpressionCustom>(
        Nodes, TEXT("Grass.ReorientRotatedNormal37Degrees"));
    const auto* NormalBlend = ExactNode<UMaterialExpressionLinearInterpolate>(
        Nodes, TEXT("Grass.NormalDetailBlend"));
    const auto* FlatNormal = ExactNode<UMaterialExpressionConstant3Vector>(
        Nodes, TEXT("Grass.FlatTangentNormal"));
    const auto* MicroNormalStrength =
        ExactNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Grass.MicroNormalStrength"));
    const auto* MicroNormalAlpha =
        ExactNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Grass.DistanceFadedMicroNormalAlpha"));
    const auto* MicroNormalBlend =
        ExactNode<UMaterialExpressionLinearInterpolate>(
            Nodes, TEXT("Grass.DistanceFadedMicroNormal"));
    const auto* FinalNormal = ExactNode<UMaterialExpressionNormalize>(
        Nodes, TEXT("Grass.FinalNormalizedNormal"));

    int32 TextureSampleCount = 0;
    int32 RotatorCount = 0;
    int32 CustomCount = 0;
    for (const UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        TextureSampleCount +=
            Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
        RotatorCount += Cast<UMaterialExpressionRotator>(Expression) ? 1 : 0;
        CustomCount += Cast<UMaterialExpressionCustom>(Expression) ? 1 : 0;
    }

    const float ScaleRatio = SecondaryTileMeters / PrimaryTileMeters;
    const bool bNonHarmonicScales =
        FMath::Abs(ScaleRatio - FMath::RoundToFloat(ScaleRatio)) > 0.25f;
    if (!Material || Material->GetPathName() != GrassBaseObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !HasNoDisplacementOrMaterialAttributePath(Material, EditorOnly) ||
        EditorOnly->Metallic.Expression || EditorOnly->Specular.Expression ||
        EditorOnly->Anisotropy.Expression || EditorOnly->Tangent.Expression ||
        EditorOnly->EmissiveColor.Expression || EditorOnly->Opacity.Expression ||
        EditorOnly->OpacityMask.Expression || EditorOnly->Refraction.Expression ||
        EditorOnly->SubsurfaceColor.Expression || EditorOnly->ClearCoat.Expression ||
        EditorOnly->ClearCoatRoughness.Expression ||
        EditorOnly->ShadingModelFromMaterialExpression.Expression ||
        EditorOnly->SurfaceThickness.Expression ||
        TextureSampleCount != 8 || RotatorCount != 1 || CustomCount != 2 ||
        !Uv0 || Uv0->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(Uv0->UTiling, 1.0f) ||
        !FMath::IsNearlyEqual(Uv0->VTiling, 1.0f) ||
        !ExactScalar(PrimaryMeters, TEXT("PrimaryTileMeters"),
            PrimaryTileMeters) ||
        !ExactScalar(SecondaryMeters, TEXT("SecondaryTileMeters"),
            SecondaryTileMeters) ||
        !ExactScalar(MacroMeters, TEXT("MacroTileMeters"), MacroTileMeters) ||
        !bNonHarmonicScales ||
        !PrimaryUv || !InputIs(PrimaryUv->A, Uv0) ||
        !InputIs(PrimaryUv->B, PrimaryMeters) ||
        !SecondaryUnrotated || !InputIs(SecondaryUnrotated->A, Uv0) ||
        !InputIs(SecondaryUnrotated->B, SecondaryMeters) ||
        !RotationRadians ||
        !FMath::IsNearlyEqual(
            RotationRadians->R, SecondaryRotationRadians, 0.000001f) ||
        !RotatedSecondary ||
        !InputIs(RotatedSecondary->Coordinate, SecondaryUnrotated) ||
        !InputIs(RotatedSecondary->Time, RotationRadians) ||
        !FMath::IsNearlyZero(RotatedSecondary->CenterX) ||
        !FMath::IsNearlyZero(RotatedSecondary->CenterY) ||
        !FMath::IsNearlyEqual(RotatedSecondary->Speed, 1.0f) ||
        !SecondaryOffset ||
        !FMath::IsNearlyEqual(SecondaryOffset->R, SecondaryOffsetU) ||
        !FMath::IsNearlyEqual(SecondaryOffset->G, SecondaryOffsetV) ||
        !SecondaryUv || !InputIs(SecondaryUv->A, RotatedSecondary) ||
        !InputIs(SecondaryUv->B, SecondaryOffset) ||
        !MacroUv || !InputIs(MacroUv->A, Uv0) ||
        !InputIs(MacroUv->B, MacroMeters))
    {
        OutError =
            TEXT("Explore V5 lawn lost its exact continuous-metre UV0, two non-harmonic scales, single 37-degree rotation, or no-displacement surface contract.");
        return false;
    }

    if (!ExactTexture(BasePrimary, TEXT("Grass001BaseColorPrimary"),
            GrassBaseColorTexturePath, SAMPLERTYPE_Color) ||
        !ExactTexture(BaseSecondary, TEXT("Grass001BaseColorSecondary"),
            GrassBaseColorTexturePath, SAMPLERTYPE_Color) ||
        !ExactTexture(NormalPrimary, TEXT("Grass001NormalDXPrimary"),
            GrassNormalDxTexturePath, SAMPLERTYPE_Normal) ||
        !ExactTexture(NormalSecondary, TEXT("Grass001NormalDXSecondary"),
            GrassNormalDxTexturePath, SAMPLERTYPE_Normal) ||
        !ExactTexture(RoughPrimary, TEXT("Grass001RoughnessPrimary"),
            GrassRoughnessTexturePath, SAMPLERTYPE_Masks) ||
        !ExactTexture(RoughSecondary, TEXT("Grass001RoughnessSecondary"),
            GrassRoughnessTexturePath, SAMPLERTYPE_Masks) ||
        !ExactTexture(AoPrimary, TEXT("Grass001AOPrimary"),
            GrassAoTexturePath, SAMPLERTYPE_Masks) ||
        !ExactTexture(AoSecondary, TEXT("Grass001AOSecondary"),
            GrassAoTexturePath, SAMPLERTYPE_Masks) ||
        BasePrimary->Texture != BaseColorTexture ||
        BaseSecondary->Texture != BaseColorTexture ||
        NormalPrimary->Texture != NormalDxTexture ||
        NormalSecondary->Texture != NormalDxTexture ||
        RoughPrimary->Texture != RoughnessTexture ||
        RoughSecondary->Texture != RoughnessTexture ||
        AoPrimary->Texture != AoTexture || AoSecondary->Texture != AoTexture ||
        !InputIs(BasePrimary->Coordinates, PrimaryUv) ||
        !InputIs(BaseSecondary->Coordinates, SecondaryUv) ||
        !InputIs(NormalPrimary->Coordinates, PrimaryUv) ||
        !InputIs(NormalSecondary->Coordinates, SecondaryUv) ||
        !InputIs(RoughPrimary->Coordinates, PrimaryUv) ||
        !InputIs(RoughSecondary->Coordinates, SecondaryUv) ||
        !InputIs(AoPrimary->Coordinates, PrimaryUv) ||
        !InputIs(AoSecondary->Coordinates, SecondaryUv))
    {
        OutError =
            TEXT("Explore V5 lawn no longer depends only on the four exact V4 Grass001 texture objects with the sealed UV routing.");
        return false;
    }

    if (!MacroField ||
        MacroField->Description != GrassMacroFieldDescription ||
        MacroField->Code != GrassMacroFieldCode ||
        MacroField->OutputType != CMOT_Float1 ||
        MacroField->Inputs.Num() != 1 ||
        MacroField->Inputs[0].InputName != TEXT("MacroUV") ||
        !InputIs(MacroField->Inputs[0].Input, MacroUv) ||
        MacroField->AdditionalOutputs.Num() != 0 ||
        MacroField->AdditionalDefines.Num() != 0 ||
        MacroField->IncludeFilePaths.Num() != 0)
    {
        OutError =
            TEXT("Explore V5 lawn lost its exact deterministic low-frequency health and mowing field.");
        return false;
    }

    if (!WorldPosition ||
        WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        !CameraPosition || !CameraDelta ||
        !InputIs(CameraDelta->A, WorldPosition) ||
        !InputIs(CameraDelta->B, CameraPosition) ||
        !CameraDistance || !InputIs(CameraDistance->Input, CameraDelta) ||
        !ExactScalar(FadeStart, TEXT("MicroFadeStartCm"), GrassFadeStartCm) ||
        !DistancePastStart ||
        !InputIs(DistancePastStart->A, CameraDistance) ||
        !InputIs(DistancePastStart->B, FadeStart) ||
        !ExactScalar(FadeRange, TEXT("MicroFadeRangeCm"), GrassFadeRangeCm) ||
        !FadeRatio || !InputIs(FadeRatio->A, DistancePastStart) ||
        !InputIs(FadeRatio->B, FadeRange) || !FarFade ||
        !InputIs(FarFade->Input, FadeRatio) || !NearFade ||
        !InputIs(NearFade->Input, FarFade) ||
        !ExactScalar(SecondaryStrength, TEXT("SecondaryBlendStrength"),
            GrassSecondaryBlend) ||
        !SecondaryAlpha || !InputIs(SecondaryAlpha->A, NearFade) ||
        !InputIs(SecondaryAlpha->B, SecondaryStrength))
    {
        OutError =
            TEXT("Explore V5 lawn lost its exact 12-54 m camera-distance detail fade.");
        return false;
    }

    if (!BaseBlend || !InputIs(BaseBlend->A, BasePrimary) ||
        !InputIs(BaseBlend->B, BaseSecondary) ||
        !InputIs(BaseBlend->Alpha, SecondaryAlpha) ||
        !ExactScalar(Desaturation, TEXT("Desaturation"), GrassDesaturation) ||
        !DesaturatedBase || !InputIs(DesaturatedBase->Input, BaseBlend) ||
        !InputIs(DesaturatedBase->Fraction, Desaturation) ||
        !DesaturatedBase->LuminanceFactors.Equals(
            GrassLuminanceFactors, 0.000001f) ||
        !ExactVector(LawnTint, TEXT("LawnTint"), GrassLawnTint) ||
        !TintedColor || !InputIs(TintedColor->A, DesaturatedBase) ||
        !InputIs(TintedColor->B, LawnTint) ||
        !ExactVector(MacroTintLow, TEXT("MacroTintLow"), GrassMacroTintLow) ||
        !ExactVector(MacroTintHigh, TEXT("MacroTintHigh"),
            GrassMacroTintHigh) ||
        !MacroTint || !InputIs(MacroTint->A, MacroTintLow) ||
        !InputIs(MacroTint->B, MacroTintHigh) ||
        !InputIs(MacroTint->Alpha, MacroField) ||
        !FinalBaseColor || !InputIs(FinalBaseColor->A, TintedColor) ||
        !InputIs(FinalBaseColor->B, MacroTint) ||
        GrassDesaturation < 0.20f || GrassLawnTint.GetMax() > 1.0f ||
        (GrassMacroTintHigh - GrassMacroTintLow).GetMax() > 0.06f ||
        !RoughnessBlend || !InputIs(RoughnessBlend->A, RoughPrimary, 1) ||
        !InputIs(RoughnessBlend->B, RoughSecondary, 1) ||
        !InputIs(RoughnessBlend->Alpha, SecondaryAlpha) ||
        !ExactScalar(RoughnessBias, TEXT("RoughnessBias"),
            GrassRoughnessBias) ||
        !RoughnessAdd || !InputIs(RoughnessAdd->A, RoughnessBlend) ||
        !InputIs(RoughnessAdd->B, RoughnessBias) ||
        !FinalRoughness || !InputIs(FinalRoughness->Input, RoughnessAdd) ||
        !AoBlend || !InputIs(AoBlend->A, AoPrimary, 1) ||
        !InputIs(AoBlend->B, AoSecondary, 1) ||
        !InputIs(AoBlend->Alpha, SecondaryAlpha))
    {
        OutError =
            TEXT("Explore V5 lawn lost its darker/desaturated response or restrained macro/roughness/AO breakup.");
        return false;
    }

    if (!RotatedNormal ||
        RotatedNormal->Description !=
            TEXT("TRIAD_IPV5_ReorientRotatedNormal37Degrees") ||
        RotatedNormal->Code != GrassRotatedNormalCode ||
        RotatedNormal->OutputType != CMOT_Float3 ||
        RotatedNormal->Inputs.Num() != 1 ||
        RotatedNormal->Inputs[0].InputName != TEXT("RotatedNormal") ||
        !InputIs(RotatedNormal->Inputs[0].Input, NormalSecondary) ||
        RotatedNormal->AdditionalOutputs.Num() != 0 ||
        RotatedNormal->AdditionalDefines.Num() != 0 ||
        RotatedNormal->IncludeFilePaths.Num() != 0 ||
        !NormalBlend || !InputIs(NormalBlend->A, NormalPrimary) ||
        !InputIs(NormalBlend->B, RotatedNormal) ||
        !InputIs(NormalBlend->Alpha, SecondaryAlpha) || !FlatNormal ||
        !FlatNormal->Constant.Equals(
            FLinearColor(0.0f, 0.0f, 1.0f, 1.0f), 0.000001f) ||
        !ExactScalar(MicroNormalStrength, TEXT("MicroNormalStrength"),
            GrassMicroNormalStrength) ||
        !MicroNormalAlpha || MicroNormalAlpha->A.Expression ||
        !FMath::IsNearlyEqual(
            MicroNormalAlpha->ConstA,
            GrassFarMicroNormalStrength,
            0.000001f) ||
        !InputIs(MicroNormalAlpha->B, MicroNormalStrength) ||
        !InputIs(MicroNormalAlpha->Alpha, NearFade) ||
        !MicroNormalBlend || !InputIs(MicroNormalBlend->A, FlatNormal) ||
        !InputIs(MicroNormalBlend->B, NormalBlend) ||
        !InputIs(MicroNormalBlend->Alpha, MicroNormalAlpha) ||
        !FinalNormal || !InputIs(FinalNormal->VectorInput, MicroNormalBlend) ||
        !InputIs(EditorOnly->BaseColor, FinalBaseColor) ||
        !InputIs(EditorOnly->Normal, FinalNormal) ||
        !InputIs(EditorOnly->Roughness, FinalRoughness) ||
        !InputIs(EditorOnly->AmbientOcclusion, AoBlend))
    {
        OutError =
            TEXT("Explore V5 lawn lost its exact tangent reorientation, 0.06 far micro-normal floor, distance-faded near response, or material-output bindings.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateWaterGraph(UMaterial* Material, FString& OutError)
{
    const UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    TMap<FString, const UMaterialExpression*> Nodes;
    if (!BuildExactNodeMap(EditorOnly, 16, Nodes, OutError))
    {
        return false;
    }
    const auto* Uv0 = ExactNode<UMaterialExpressionTextureCoordinate>(
        Nodes, TEXT("Water.UV0"));
    const auto* Time = ExactNode<UMaterialExpressionTime>(
        Nodes, TEXT("Water.TimeSeconds"));
    const auto* VelocityA = ExactNode<UMaterialExpressionConstant2Vector>(
        Nodes, TEXT("Water.VelocityA_UVPerSecond"));
    const auto* VelocityB = ExactNode<UMaterialExpressionConstant2Vector>(
        Nodes, TEXT("Water.VelocityB_UVPerSecond"));
    const auto* FieldScaleA = ExactNode<UMaterialExpressionConstant>(
        Nodes, TEXT("Water.FieldScaleA"));
    const auto* FieldScaleB = ExactNode<UMaterialExpressionConstant>(
        Nodes, TEXT("Water.FieldScaleB"));
    const auto* AnalyticNormal = ExactNode<UMaterialExpressionCustom>(
        Nodes, TEXT("Water.AnalyticTwoFieldNormal"));
    const auto* Tint = ExactNode<UMaterialExpressionVectorParameter>(
        Nodes, TEXT("Water.Tint"));
    const auto* Roughness = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Water.Roughness"));
    const auto* Specular = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Water.Specular"));
    const auto* Opacity = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Water.Opacity"));
    const auto* ClampedOpacity = ExactNode<UMaterialExpressionClamp>(
        Nodes, TEXT("Water.ClampOpacity35To65"));
    const auto* DepthFadeDistance =
        ExactNode<UMaterialExpressionScalarParameter>(
            Nodes, TEXT("Water.DepthFadeDistanceCm"));
    const auto* ClampedDepthFadeDistance =
        ExactNode<UMaterialExpressionClamp>(
            Nodes, TEXT("Water.ClampDepthFadeDistance25To150"));
    const auto* DepthFade = ExactDepthFadeNode(
        Nodes, TEXT("Water.DepthFade80Cm"));
    const auto* Ior = ExactNode<UMaterialExpressionScalarParameter>(
        Nodes, TEXT("Water.IOR"));

    int32 TextureSampleCount = 0;
    int32 CustomCount = 0;
    for (const UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        TextureSampleCount +=
            Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
        CustomCount += Cast<UMaterialExpressionCustom>(Expression) ? 1 : 0;
    }
    const FVector2D Va(WaterVelocityAx, WaterVelocityAy);
    const FVector2D Vb(WaterVelocityBx, WaterVelocityBy);
    const float SpeedA = Va.Size();
    const float SpeedB = Vb.Size();
    const float SpeedRatio = FMath::Max(SpeedA, SpeedB) /
        FMath::Min(SpeedA, SpeedB);
    const float Cross = Va.X * Vb.Y - Va.Y * Vb.X;
    if (!Material || Material->GetPathName() != FountainWaterObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Translucent || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->bScreenSpaceReflections ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        Material->TranslucencyLightingMode != TLM_SurfacePerPixelLighting ||
        Material->RefractionMethod != RM_IndexOfRefraction ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !HasNoDisplacementOrMaterialAttributePath(Material, EditorOnly) ||
        EditorOnly->Metallic.Expression || EditorOnly->Anisotropy.Expression ||
        EditorOnly->Tangent.Expression || EditorOnly->EmissiveColor.Expression ||
        EditorOnly->OpacityMask.Expression ||
        EditorOnly->AmbientOcclusion.Expression ||
        EditorOnly->SubsurfaceColor.Expression || EditorOnly->ClearCoat.Expression ||
        EditorOnly->ClearCoatRoughness.Expression ||
        EditorOnly->ShadingModelFromMaterialExpression.Expression ||
        EditorOnly->SurfaceThickness.Expression ||
        TextureSampleCount != 0 || CustomCount != 1 ||
        !Uv0 || Uv0->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(Uv0->UTiling, 1.0f) ||
        !FMath::IsNearlyEqual(Uv0->VTiling, 1.0f) || !Time ||
        Time->bIgnorePause || Time->bOverride_Period ||
        !FMath::IsNearlyZero(Time->Period, 0.000001f) ||
        !VelocityA || !VelocityB ||
        !FMath::IsNearlyEqual(VelocityA->R, WaterVelocityAx) ||
        !FMath::IsNearlyEqual(VelocityA->G, WaterVelocityAy) ||
        !FMath::IsNearlyEqual(VelocityB->R, WaterVelocityBx) ||
        !FMath::IsNearlyEqual(VelocityB->G, WaterVelocityBy) ||
        SpeedA > 0.03f || SpeedB > 0.03f || SpeedRatio < 1.25f ||
        FMath::Abs(Cross) < 0.00001f || !FieldScaleA || !FieldScaleB ||
        !FMath::IsNearlyEqual(FieldScaleA->R, WaterFieldScaleA) ||
        !FMath::IsNearlyEqual(FieldScaleB->R, WaterFieldScaleB))
    {
        OutError =
            TEXT("Explore V5 fountain water lost its texture-free Surface/DefaultLit/Translucent/SurfacePerPixel/local-SSR or two nonparallel <=0.03 UV/s velocity contract.");
        return false;
    }

    const FName ExpectedInputNames[] = {
        TEXT("UV0"), TEXT("TimeSeconds"), TEXT("VelocityA"),
        TEXT("VelocityB"), TEXT("FieldScaleA"), TEXT("FieldScaleB")};
    const UMaterialExpression* ExpectedInputExpressions[] = {
        Uv0, Time, VelocityA, VelocityB, FieldScaleA, FieldScaleB};
    if (!AnalyticNormal ||
        AnalyticNormal->Description !=
            TEXT("TRIAD_IPV5_AnalyticTwoNonparallelMovingNormals") ||
        AnalyticNormal->Code != WaterAnalyticNormalCode ||
        AnalyticNormal->OutputType != CMOT_Float3 ||
        AnalyticNormal->Inputs.Num() != UE_ARRAY_COUNT(ExpectedInputNames) ||
        AnalyticNormal->AdditionalOutputs.Num() != 0 ||
        AnalyticNormal->AdditionalDefines.Num() != 0 ||
        AnalyticNormal->IncludeFilePaths.Num() != 0)
    {
        OutError = TEXT("Explore V5 fountain water analytic normal code changed.");
        return false;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedInputNames); ++Index)
    {
        if (AnalyticNormal->Inputs[Index].InputName !=
                ExpectedInputNames[Index] ||
            !InputIs(
                AnalyticNormal->Inputs[Index].Input,
                ExpectedInputExpressions[Index]))
        {
            OutError =
                TEXT("Explore V5 fountain water analytic normal inputs changed.");
            return false;
        }
    }
    if (!ExactVector(Tint, TEXT("WaterTint"), WaterTint) ||
        !ExactScalar(Roughness, TEXT("Roughness"), WaterRoughness) ||
        !ExactScalar(Specular, TEXT("Specular"), WaterSpecular) ||
        !ExactScalar(Opacity, TEXT("Opacity"), WaterOpacity) ||
        !FMath::IsNearlyEqual(
            Opacity->SliderMin, WaterOpacityMin, 0.000001f) ||
        !FMath::IsNearlyEqual(
            Opacity->SliderMax, WaterOpacityMax, 0.000001f) ||
        !ExactClamp(
            ClampedOpacity, Opacity, WaterOpacityMin, WaterOpacityMax) ||
        !ExactScalar(
            DepthFadeDistance, TEXT("DepthFadeDistanceCm"), WaterDepthFadeCm) ||
        !FMath::IsNearlyEqual(
            DepthFadeDistance->SliderMin,
            WaterDepthFadeMinCm,
            0.000001f) ||
        !FMath::IsNearlyEqual(
            DepthFadeDistance->SliderMax,
            WaterDepthFadeMaxCm,
            0.000001f) ||
        !ExactClamp(
            ClampedDepthFadeDistance,
            DepthFadeDistance,
            WaterDepthFadeMinCm,
            WaterDepthFadeMaxCm) ||
        !DepthFade || !InputIs(DepthFade->InOpacity, ClampedOpacity) ||
        !InputIs(DepthFade->FadeDistance, ClampedDepthFadeDistance) ||
        !FMath::IsNearlyEqual(
            DepthFade->OpacityDefault, WaterOpacity, 0.000001f) ||
        !FMath::IsNearlyEqual(
            DepthFade->FadeDistanceDefault, WaterDepthFadeCm, 0.000001f) ||
        !ExactScalar(Ior, TEXT("IOR"), WaterIor) ||
        !InputIs(EditorOnly->BaseColor, Tint) ||
        !InputIs(EditorOnly->Roughness, Roughness) ||
        !InputIs(EditorOnly->Specular, Specular) ||
        !InputIs(EditorOnly->Normal, AnalyticNormal) ||
        !InputIs(EditorOnly->Opacity, DepthFade) ||
        !InputIs(EditorOnly->Refraction, Ior))
    {
        OutError =
            TEXT("Explore V5 fountain water lost its exact tint/roughness/specular, clamped opacity [0.35,0.65], clamped depth-fade [25,150] cm (default 80 cm), or IOR 1.333 bindings.");
        return false;
    }
    OutError.Reset();
    return true;
}

FString EnabledBasePropertyOverrideNames(
    const FMaterialInstanceBasePropertyOverrides& Overrides)
{
    TArray<FString> Names;
    if (Overrides.bOverride_OpacityMaskClipValue)
        Names.Add(TEXT("OpacityMaskClipValue"));
    if (Overrides.bOverride_BlendMode)
        Names.Add(TEXT("BlendMode"));
    if (Overrides.bOverride_ShadingModel)
        Names.Add(TEXT("ShadingModel"));
    if (Overrides.bOverride_DitheredLODTransition)
        Names.Add(TEXT("DitheredLODTransition"));
    if (Overrides.bOverride_CastDynamicShadowAsMasked)
        Names.Add(TEXT("CastDynamicShadowAsMasked"));
    if (Overrides.bOverride_TwoSided)
        Names.Add(TEXT("TwoSided"));
    if (Overrides.bOverride_bIsThinSurface)
        Names.Add(TEXT("ThinSurface"));
    if (Overrides.bOverride_OutputTranslucentVelocity)
        Names.Add(TEXT("OutputTranslucentVelocity"));
    if (Overrides.bOverride_bHasPixelAnimation)
        Names.Add(TEXT("HasPixelAnimation"));
    if (Overrides.bOverride_bEnableTessellation)
        Names.Add(TEXT("EnableTessellation"));
    if (Overrides.bOverride_DisplacementScaling)
        Names.Add(TEXT("DisplacementScaling"));
    if (Overrides.bOverride_bEnableDisplacementFade)
        Names.Add(TEXT("EnableDisplacementFade"));
    if (Overrides.bOverride_DisplacementFadeRange)
        Names.Add(TEXT("DisplacementFadeRange"));
    if (Overrides.bOverride_MaxWorldPositionOffsetDisplacement)
        Names.Add(TEXT("MaximumWorldPositionOffsetDisplacement"));
    Names.Sort();
    return FString::Join(Names, TEXT(","));
}

bool SameNames(const TSet<FName>& Expected, const TSet<FName>& Actual)
{
    if (Expected.Num() != Actual.Num())
    {
        return false;
    }
    for (const FName& Name : Expected)
    {
        if (!Actual.Contains(Name))
        {
            return false;
        }
    }
    return true;
}

bool ValidateMaterialInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialInstanceSpec& Spec,
    FString& OutError)
{
    if (!Instance || Instance->GetPathName() != Spec.ObjectPath ||
        !Instance->Parent ||
        Instance->Parent->GetPathName() != Spec.ParentObjectPath ||
        !Instance->NaniteOverrideMaterial.bEnableOverride ||
        Instance->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Instance->GetNaniteOverride())
    {
        OutError = FString::Printf(
            TEXT("Explore V5 instance '%s' has the wrong exact path, parent, or Nanite-override state."),
            *Spec.AssetName);
        return false;
    }
    TSet<FName> ExpectedScalarNames;
    TSet<FName> ActualScalarNames;
    for (const TPair<FName, float>& Pair : Spec.Scalars)
    {
        ExpectedScalarNames.Add(Pair.Key);
    }
    for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualScalarNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("Explore V5 instance '%s' has an invalid or duplicate scalar override."),
                *Spec.AssetName);
            return false;
        }
        ActualScalarNames.Add(Value.ParameterInfo.Name);
    }
    TSet<FName> ExpectedVectorNames;
    TSet<FName> ActualVectorNames;
    for (const TPair<FName, FLinearColor>& Pair : Spec.Vectors)
    {
        ExpectedVectorNames.Add(Pair.Key);
    }
    for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualVectorNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("Explore V5 instance '%s' has an invalid or duplicate vector override."),
                *Spec.AssetName);
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    if (!SameNames(ExpectedScalarNames, ActualScalarNames) ||
        !SameNames(ExpectedVectorNames, ActualVectorNames))
    {
        OutError = FString::Printf(
            TEXT("Explore V5 instance '%s' override roster changed."),
            *Spec.AssetName);
        return false;
    }
    const FStaticParameterSet StaticParameters = Instance->GetStaticParameters();
    if (Instance->TextureParameterValues.Num() != 0 ||
        Instance->DoubleVectorParameterValues.Num() != 0 ||
        Instance->TextureCollectionParameterValues.Num() != 0 ||
        Instance->RuntimeVirtualTextureParameterValues.Num() != 0 ||
        Instance->SparseVolumeTextureParameterValues.Num() != 0 ||
        Instance->FontParameterValues.Num() != 0 ||
        Instance->UserSceneTextureOverrides.Num() != 0 ||
        StaticParameters.StaticSwitchParameters.Num() != 0 ||
        StaticParameters.EditorOnly.StaticComponentMaskParameters.Num() != 0 ||
        StaticParameters.EditorOnly.TerrainLayerWeightParameters.Num() != 0 ||
        StaticParameters.bHasMaterialLayers)
    {
        OutError = FString::Printf(
            TEXT("Explore V5 instance '%s' gained a forbidden texture, RVT, static, layer, font, or scene override."),
            *Spec.AssetName);
        return false;
    }
    const FString BaseOverrides =
        EnabledBasePropertyOverrideNames(Instance->BasePropertyOverrides);
    if (!BaseOverrides.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("Explore V5 instance '%s' enables forbidden base-property overrides: %s."),
            *Spec.AssetName,
            *BaseOverrides);
        return false;
    }
    for (const TPair<FName, float>& Pair : Spec.Scalars)
    {
        float ActualValue = 0.0f;
        if (!Instance->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Pair.Key),
                ActualValue,
                true) ||
            !FMath::IsNearlyEqual(ActualValue, Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("Explore V5 instance '%s' scalar '%s' changed."),
                *Spec.AssetName,
                *Pair.Key.ToString());
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : Spec.Vectors)
    {
        FLinearColor ActualValue;
        if (!Instance->GetVectorParameterValue(
                FHashedMaterialParameterInfo(Pair.Key),
                ActualValue,
                true) ||
            !ActualValue.Equals(Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("Explore V5 instance '%s' vector '%s' changed."),
                *Spec.AssetName,
                *Pair.Key.ToString());
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateCompiledMaterial(UMaterial* Material, FString& OutError)
{
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(ERHIFeatureLevel::SM5)
        : nullptr;
    if (Resource)
    {
        Resource->FinishCompilation();
    }
    const TArray<FString> CompileErrors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Material || !Resource ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        !Resource->IsCompilationFinished() || CompileErrors.Num() != 0)
    {
        OutError = FString::Printf(
            TEXT("Explore V5 material '%s' has no valid compiled SM5 resource: [%s]."),
            Material ? *Material->GetPathName() : TEXT("<null>"),
            *FString::Join(CompileErrors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompiledInstance(
    UMaterialInstanceConstant* Instance,
    FString& OutError)
{
    if (Instance)
    {
        Instance->EnsureIsComplete();
    }
    FMaterialResource* Resource = Instance
        ? Instance->GetMaterialResource(ERHIFeatureLevel::SM5)
        : nullptr;
    const TArray<FString> CompileErrors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Instance || !Resource || Instance->IsCompiling() ||
        !Instance->IsComplete() || !Resource->IsCompilationFinished() ||
        !Resource->GetGameThreadShaderMap() ||
        !Resource->IsGameThreadShaderMapComplete() ||
        CompileErrors.Num() != 0)
    {
        OutError = FString::Printf(
            TEXT("Explore V5 instance '%s' has no complete SM5 shader map: [%s]."),
            Instance ? *Instance->GetPathName() : TEXT("<null>"),
            *FString::Join(CompileErrors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateContextHasNoSiteSpecificResponse(
    UMaterialInstanceConstant* Instance,
    FString& OutError)
{
    const FName ForbiddenStrengths[] = {
        TEXT("WeatheringStrength"),
        TEXT("SillDirtStrength"),
        TEXT("CorniceRunoffStrength"),
        TEXT("GroundContactDampStrength"),
        TEXT("CavityDirtStrength"),
        TEXT("BumpOffsetStrength"),
        TEXT("ExposedMetalMaskStrength")};
    for (const FName& Name : ForbiddenStrengths)
    {
        float Value = 1.0f;
        if (!Instance ||
            !Instance->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Name), Value, true) ||
            !FMath::IsNearlyZero(Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("Explore V5 context material '%s' enables site-specific '%s'."),
                Instance ? *Instance->GetPathName() : TEXT("<null>"),
                *Name.ToString());
            return false;
        }
    }
    for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
    {
        const FString Name = Value.ParameterInfo.Name.ToString();
        if (Name.Contains(TEXT("Joint"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Window"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Stain"), ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(
                TEXT("Explore V5 context material '%s' contains forbidden site-specific override '%s'."),
                *Instance->GetPathName(),
                *Name);
            return false;
        }
    }
    OutError.Reset();
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5MaterialFactory
{
const FString& GetMaterialRootPath()
{
    return MaterialRoot;
}

const FString& GetGrassBaseMaterialObjectPath()
{
    return GrassBaseObjectPath;
}

const FString& GetGrassMaterialInstanceObjectPath()
{
    return GrassInstanceObjectPath;
}

const FString& GetFountainWaterMaterialObjectPath()
{
    return FountainWaterObjectPath;
}

const FString& GetHardscapeStoneMaterialInstanceObjectPath()
{
    return HardscapeStoneObjectPath;
}

const FString& GetContextRenderMaterialInstanceObjectPath()
{
    return ContextRenderObjectPath;
}

const FString& GetContextRoofMaterialInstanceObjectPath()
{
    return ContextRoofObjectPath;
}

bool CreateFreshExploreV5Materials(
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    OutAssets.Reset();
    OutError.Reset();
    if (!ValidateEmptyMaterialRoot(OutError))
    {
        return false;
    }
    for (const FString& TargetPath : ExactMaterialObjectPaths())
    {
        if (LoadObject<UObject>(nullptr, *TargetPath))
        {
            OutError = FString::Printf(
                TEXT("CreateFreshExploreV5Materials refuses to overwrite existing asset '%s'."),
                *TargetPath);
            return false;
        }
    }

    UTexture2D* BaseColorTexture = nullptr;
    UTexture2D* NormalDxTexture = nullptr;
    UTexture2D* RoughnessTexture = nullptr;
    UTexture2D* AoTexture = nullptr;
    if (!LoadGrassTextures(
            BaseColorTexture,
            NormalDxTexture,
            RoughnessTexture,
            AoTexture,
            OutError))
    {
        return false;
    }

    if (!ValidateHeroV5Dependencies(OutError))
    {
        return false;
    }

    TMap<FString, UMaterialInstanceConstant*> ContextParents;
    for (const FMaterialInstanceSpec& Spec : ContextInstanceSpecs())
    {
        UMaterialInstanceConstant* Parent =
            LoadExact<UMaterialInstanceConstant>(Spec.ParentObjectPath);
        if (!Parent || !ParentSupportsSpec(Parent, Spec, OutError))
        {
            return false;
        }
        ContextParents.Add(Spec.ParentObjectPath, Parent);
    }

    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    FScopedFreshMaterialRollback Rollback(OutAssets, OutError);
    UMaterial* GrassBase = CreateGrassBase(
        AssetTools,
        BaseColorTexture,
        NormalDxTexture,
        RoughnessTexture,
        AoTexture,
        OutError);
    if (!GrassBase)
    {
        return false;
    }
    OutAssets.Add(GrassBase);

    const FMaterialInstanceSpec GrassSpec = MakeGrassInstanceSpec();
    UMaterialInstanceConstant* GrassInstance = CreateMaterialInstance(
        AssetTools, GrassSpec, GrassBase, OutError);
    if (!GrassInstance)
    {
        return false;
    }
    OutAssets.Add(GrassInstance);

    UMaterial* Water = CreateWaterMaterial(AssetTools, OutError);
    if (!Water)
    {
        return false;
    }
    OutAssets.Add(Water);

    for (const FMaterialInstanceSpec& Spec : ContextInstanceSpecs())
    {
        UMaterialInstanceConstant* Parent =
            ContextParents.FindRef(Spec.ParentObjectPath);
        UMaterialInstanceConstant* Instance = CreateMaterialInstance(
            AssetTools, Spec, Parent, OutError);
        if (!Instance)
        {
            return false;
        }
        OutAssets.Add(Instance);
    }
    if (OutAssets.Num() != 6)
    {
        OutError =
            TEXT("Explore V5 material factory did not create its exact six-asset roster.");
        return false;
    }
    if (!ValidateExactMaterialRootRoster(OutError))
    {
        return false;
    }

    FString ValidationReport;
    if (!ValidateExploreV5Materials(ValidationReport))
    {
        OutError = TEXT("Fresh Explore V5 material validation failed: ") +
            ValidationReport;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateExploreV5Materials(FString& OutReport)
{
    OutReport.Reset();
    if (!ValidateHeroV5Dependencies(OutReport) ||
        !ValidateExactMaterialRootRoster(OutReport))
    {
        return false;
    }
    UTexture2D* BaseColorTexture = nullptr;
    UTexture2D* NormalDxTexture = nullptr;
    UTexture2D* RoughnessTexture = nullptr;
    UTexture2D* AoTexture = nullptr;
    if (!LoadGrassTextures(
            BaseColorTexture,
            NormalDxTexture,
            RoughnessTexture,
            AoTexture,
            OutReport))
    {
        return false;
    }

    UMaterial* GrassBase = LoadExact<UMaterial>(GrassBaseObjectPath);
    UMaterialInstanceConstant* GrassInstance =
        LoadExact<UMaterialInstanceConstant>(GrassInstanceObjectPath);
    UMaterial* Water = LoadExact<UMaterial>(FountainWaterObjectPath);
    if (!GrassBase || !GrassInstance || !Water)
    {
        OutReport =
            TEXT("Explore V5 material namespace lacks one or more exact grass/water assets.");
        return false;
    }
    if (!ValidateGrassGraph(
            GrassBase,
            BaseColorTexture,
            NormalDxTexture,
            RoughnessTexture,
            AoTexture,
            OutReport) ||
        !ValidateMaterialInstance(
            GrassInstance, MakeGrassInstanceSpec(), OutReport) ||
        !ValidateWaterGraph(Water, OutReport))
    {
        return false;
    }

    TArray<UMaterialInstanceConstant*> ContextInstances;
    for (const FMaterialInstanceSpec& Spec : ContextInstanceSpecs())
    {
        UMaterialInstanceConstant* Parent =
            LoadExact<UMaterialInstanceConstant>(Spec.ParentObjectPath);
        UMaterialInstanceConstant* Instance =
            LoadExact<UMaterialInstanceConstant>(Spec.ObjectPath);
        if (!Parent || !ParentSupportsSpec(Parent, Spec, OutReport) ||
            !ValidateMaterialInstance(Instance, Spec, OutReport) ||
            !ValidateContextHasNoSiteSpecificResponse(Instance, OutReport))
        {
            return false;
        }
        ContextInstances.Add(Instance);
    }
    if (ContextInstances.Num() != 3)
    {
        OutReport =
            TEXT("Explore V5 material namespace lacks its exact three context instances.");
        return false;
    }

    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateCompiledMaterial(GrassBase, OutReport) ||
        !ValidateCompiledMaterial(Water, OutReport) ||
        !ValidateCompiledInstance(GrassInstance, OutReport))
    {
        return false;
    }
    for (UMaterialInstanceConstant* ContextInstance : ContextInstances)
    {
        if (!ValidateCompiledInstance(ContextInstance, OutReport))
        {
            return false;
        }
    }
    OutReport =
        TEXT("EXPLORE_V5_MATERIALS_VALID: exact six-asset grass/water/context roster, dependencies, topology, overrides, and SM5 shader maps passed.");
    return true;
}
} // namespace TRIADIstanaExploreV5MaterialFactory
