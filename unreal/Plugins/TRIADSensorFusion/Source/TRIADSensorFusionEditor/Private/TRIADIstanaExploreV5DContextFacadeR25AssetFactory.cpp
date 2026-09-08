#include "TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "RHIFeatureLevel.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <initializer_list>

namespace
{
const FString AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/ContextFacadeR25"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_ContextFacadeR25_Master"));
const FString MasterObjectPath(
    MaterialRoot + TEXT("/") + MasterName + TEXT(".") + MasterName);
constexpr int32 ExpectedAssetCount = 5;
constexpr int32 ExpectedMaterialCount = 4;

const FName MaterialParameterGroup(
    TEXT("Istana Explore V5D Context Facade R25"));
const FString SurfaceResponseDescription(
    TEXT("TRIAD_IPV5D_NONAUTHORITATIVE_CONTEXT_FACADE_R25_DISTANCE_READABLE_RECESSED_GLAZING"));

// UV0 is authored in local facade metres. This successor deliberately adds
// presentation response only: it does not displace the mesh, add collision,
// or create sensor/RF geometry authority.
const FString SurfaceResponseCode(
    TEXT("float cellMetres = max(VariationCellMeters, 1.0);\n")
    TEXT("float2 p = WorldPositionCm.xy / (cellMetres * 100.0);\n")
    TEXT("float2 i = floor(p);\n")
    TEXT("float2 f = frac(p);\n")
    TEXT("f = f * f * (3.0 - 2.0 * f);\n")
    TEXT("float4 h = frac(sin(float4(\n")
    TEXT("    dot(i, float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(1.0, 0.0), float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(0.0, 1.0), float2(127.1, 311.7)),\n")
    TEXT("    dot(i + float2(1.0, 1.0), float2(127.1, 311.7)))) * 43758.5453);\n")
    TEXT("float variation = lerp(lerp(h.x, h.y, f.x), lerp(h.z, h.w, f.x), f.y);\n")
    TEXT("float2 buildingSeed = floor(WorldPositionCm.xy / 6400.0);\n")
    TEXT("float buildingHash = frac(sin(dot(buildingSeed, float2(19.19, 83.17))) * 24634.6345);\n")
    TEXT("float3 surface = lerp(SurfaceTintLow.rgb, SurfaceTintHigh.rgb, variation);\n")
    TEXT("surface *= lerp(0.92, 1.08, buildingHash);\n")
    TEXT("float bay = max(BayMeters, 0.5);\n")
    TEXT("float storey = max(StoreyMeters, 2.0);\n")
    TEXT("float2 q = float2(UV0.x / bay, UV0.y / storey);\n")
    TEXT("float2 phase = frac(q);\n")
    TEXT("float2 aa = max(fwidth(q), float2(0.002, 0.002));\n")
    TEXT("float cellFootprint = max(aa.x, aa.y);\n")
    TEXT("float microReadability = 1.0 - smoothstep(0.35, 0.90, cellFootprint);\n")
    TEXT("float widthFraction = saturate(ApertureWidthFraction);\n")
    TEXT("float heightFraction = saturate(ApertureHeightFraction);\n")
    TEXT("float sillFraction = saturate(ApertureSillFraction);\n")
    TEXT("float minU = 0.5 - widthFraction * 0.5;\n")
    TEXT("float maxU = 0.5 + widthFraction * 0.5;\n")
    TEXT("float minV = sillFraction;\n")
    TEXT("float maxV = min(sillFraction + heightFraction, 0.98);\n")
    TEXT("float outerU = smoothstep(minU - aa.x, minU + aa.x, phase.x) *\n")
    TEXT("    (1.0 - smoothstep(maxU - aa.x, maxU + aa.x, phase.x));\n")
    TEXT("float outerV = smoothstep(minV - aa.y, minV + aa.y, phase.y) *\n")
    TEXT("    (1.0 - smoothstep(maxV - aa.y, maxV + aa.y, phase.y));\n")
    TEXT("float outerAperture = saturate(outerU * outerV);\n")
    TEXT("float frameU = min(0.065, widthFraction * 0.18);\n")
    TEXT("float frameV = min(0.065, heightFraction * 0.18);\n")
    TEXT("float innerMinU = minU + frameU;\n")
    TEXT("float innerMaxU = maxU - frameU;\n")
    TEXT("float innerMinV = minV + frameV;\n")
    TEXT("float innerMaxV = maxV - frameV;\n")
    TEXT("float innerU = smoothstep(innerMinU - aa.x, innerMinU + aa.x, phase.x) *\n")
    TEXT("    (1.0 - smoothstep(innerMaxU - aa.x, innerMaxU + aa.x, phase.x));\n")
    TEXT("float innerV = smoothstep(innerMinV - aa.y, innerMinV + aa.y, phase.y) *\n")
    TEXT("    (1.0 - smoothstep(innerMaxV - aa.y, innerMaxV + aa.y, phase.y));\n")
    TEXT("float insetGlass = saturate(innerU * innerV);\n")
    TEXT("float frameMask = saturate(outerAperture - insetGlass);\n")
    TEXT("float spanU = max(innerMaxU - innerMinU, 0.001);\n")
    TEXT("float spanV = max(innerMaxV - innerMinV, 0.001);\n")
    TEXT("float localU = saturate((phase.x - innerMinU) / spanU);\n")
    TEXT("float localV = saturate((phase.y - innerMinV) / spanV);\n")
    TEXT("float mullionAa = max(aa.x / spanU, 0.003);\n")
    TEXT("float transomAa = max(aa.y / spanV, 0.003);\n")
    TEXT("float mullion = 1.0 - smoothstep(0.025, 0.025 + mullionAa, abs(localU - 0.5));\n")
    TEXT("float transom = 1.0 - smoothstep(0.022, 0.022 + transomAa, abs(localV - 0.58));\n")
    TEXT("float dividerMask = insetGlass * saturate(max(mullion, transom));\n")
    TEXT("float edgeDistance = min(min(phase.x - minU, maxU - phase.x),\n")
    TEXT("    min(phase.y - minV, maxV - phase.y));\n")
    TEXT("float revealWidth = max(min(frameU, frameV) * 1.25, 0.025);\n")
    TEXT("float revealMask = outerAperture * (1.0 - smoothstep(0.0, revealWidth, edgeDistance));\n")
    TEXT("float distanceCm = length(WorldPositionCm - CameraPositionCm);\n")
    TEXT("float apertureRange = max(ApertureFadeEndCm - ApertureFadeStartCm, 1.0);\n")
    TEXT("float apertureFade = 1.0 - saturate((distanceCm - ApertureFadeStartCm) / apertureRange);\n")
    TEXT("float facadeStrength = max(ApertureHintStrength, 0.0) * apertureFade * microReadability;\n")
    TEXT("float cellHash = frac(sin(dot(floor(q) + buildingSeed, float2(41.37, 289.11))) * 15731.743);\n")
    TEXT("float cellOccupancy = lerp(0.72, 1.0, step(0.16, cellHash));\n")
    TEXT("float cellTone = lerp(0.70, 1.12, cellHash);\n")
    TEXT("float2 macroQ = float2(UV0.x / (bay * 4.0), UV0.y / (storey * 4.0));\n")
    TEXT("float macroHash = frac(sin(dot(floor(macroQ) + buildingSeed * 0.25, float2(73.19, 211.73))) * 31821.631);\n")
    TEXT("float macroTone = lerp(0.90, 1.07, macroHash);\n")
    TEXT("float3 dPdx = ddx(WorldPositionCm);\n")
    TEXT("float3 dPdy = ddy(WorldPositionCm);\n")
    TEXT("float3 geometricNormal = normalize(cross(dPdy, dPdx));\n")
    TEXT("float3 viewDirection = normalize(CameraPositionCm - WorldPositionCm);\n")
    TEXT("float grazing = 1.0 - saturate(abs(dot(geometricNormal, viewDirection)));\n")
    TEXT("float glassFresnel = pow(grazing, 4.0);\n")
    TEXT("float3 glassTint = saturate(ApertureHintTint.rgb * cellTone);\n")
    TEXT("glassTint = lerp(glassTint, AtmosphereTint.rgb, glassFresnel * 0.32);\n")
    TEXT("float wallRole = step(0.0001, ApertureHintStrength);\n")
    TEXT("float plinthMask = wallRole * (1.0 - smoothstep(0.18, 0.88, UV0.y));\n")
    TEXT("float3 facadeSurface = surface * lerp(1.0, macroTone, wallRole);\n")
    TEXT("facadeSurface = lerp(facadeSurface, facadeSurface * 0.48, plinthMask);\n")
    TEXT("float visibleFrame = saturate(frameMask * facadeStrength);\n")
    TEXT("float visibleReveal = saturate(revealMask * facadeStrength * cellOccupancy);\n")
    TEXT("float visibleGlass = saturate(insetGlass * facadeStrength * cellOccupancy);\n")
    TEXT("float visibleDivider = saturate(dividerMask * facadeStrength * cellOccupancy);\n")
    TEXT("float3 frameTint = lerp(surface, SurfaceTintHigh.rgb, 0.34);\n")
    TEXT("float3 framedSurface = lerp(facadeSurface, frameTint, visibleFrame * 0.78);\n")
    TEXT("float3 revealTint = glassTint * lerp(0.46, 0.64, grazing);\n")
    TEXT("float3 recessedSurface = lerp(framedSurface, revealTint, visibleReveal * 0.88);\n")
    TEXT("float3 glazedSurface = lerp(recessedSurface, glassTint, visibleGlass);\n")
    TEXT("float3 brokenSurface = lerp(glazedSurface, frameTint * 0.72, visibleDivider * 0.90);\n")
    TEXT("float atmosphereRange = max(AtmosphereEndCm - AtmosphereStartCm, 1.0);\n")
    TEXT("float atmosphere = saturate((distanceCm - AtmosphereStartCm) / atmosphereRange) *\n")
    TEXT("    saturate(AtmosphereStrength);\n")
    TEXT("float luminance = dot(brokenSurface, float3(0.212639, 0.715169, 0.072192));\n")
    TEXT("float3 mutedSurface = lerp(brokenSurface, luminance.xxx, atmosphere * 0.26);\n")
    TEXT("float3 finalColor = saturate(lerp(mutedSurface, AtmosphereTint.rgb, atmosphere));\n")
    TEXT("float responseRoughness = lerp(saturate(SurfaceRoughness), 0.22, visibleGlass);\n")
    TEXT("responseRoughness = lerp(responseRoughness, 0.46, visibleDivider);\n")
    TEXT("float finalRoughness = lerp(responseRoughness, 1.0, atmosphere * 0.28);\n")
    TEXT("return float4(finalColor, saturate(finalRoughness));"));

struct FScalarSpec
{
    const TCHAR* NodeId;
    const TCHAR* ParameterName;
    float DefaultValue;
};

const FScalarSpec ScalarSpecs[] = {
    {TEXT("R25.VariationCellMeters"), TEXT("VariationCellMeters"), 44.0f},
    {TEXT("R25.BayMeters"), TEXT("BayMeters"), 3.1f},
    {TEXT("R25.StoreyMeters"), TEXT("StoreyMeters"), 3.2f},
    {TEXT("R25.ApertureWidthFraction"), TEXT("ApertureWidthFraction"), 0.61f},
    {TEXT("R25.ApertureHeightFraction"), TEXT("ApertureHeightFraction"), 0.49f},
    {TEXT("R25.ApertureSillFraction"), TEXT("ApertureSillFraction"), 0.20f},
    {TEXT("R25.ApertureHintStrength"), TEXT("ApertureHintStrength"), 0.0f},
    {TEXT("R25.ApertureFadeStartCm"), TEXT("ApertureFadeStartCm"), 55000.0f},
    {TEXT("R25.ApertureFadeEndCm"), TEXT("ApertureFadeEndCm"), 100000.0f},
    {TEXT("R25.AtmosphereStartCm"), TEXT("AtmosphereStartCm"), 60000.0f},
    {TEXT("R25.AtmosphereEndCm"), TEXT("AtmosphereEndCm"), 130000.0f},
    {TEXT("R25.AtmosphereStrength"), TEXT("AtmosphereStrength"), 0.22f},
    {TEXT("R25.SurfaceRoughness"), TEXT("SurfaceRoughness"), 0.70f},
    {TEXT("R25.SurfaceSpecular"), TEXT("SurfaceSpecular"), 0.25f}};
static_assert(UE_ARRAY_COUNT(ScalarSpecs) == 14);

struct FVectorSpec
{
    const TCHAR* NodeId;
    const TCHAR* ParameterName;
    FLinearColor DefaultValue;
};

const FVectorSpec VectorSpecs[] = {
    {TEXT("R25.SurfaceTintLow"), TEXT("SurfaceTintLow"),
     FLinearColor(0.27f, 0.27f, 0.25f, 1.0f)},
    {TEXT("R25.SurfaceTintHigh"), TEXT("SurfaceTintHigh"),
     FLinearColor(0.49f, 0.46f, 0.40f, 1.0f)},
    {TEXT("R25.ApertureHintTint"), TEXT("ApertureHintTint"),
     FLinearColor(0.020f, 0.050f, 0.075f, 1.0f)},
    {TEXT("R25.AtmosphereTint"), TEXT("AtmosphereTint"),
     FLinearColor(0.48f, 0.54f, 0.55f, 1.0f)}};
static_assert(UE_ARRAY_COUNT(VectorSpecs) == 4);

struct FMaterialSpec
{
    const TCHAR* Name;
    FLinearColor SurfaceTintLow;
    FLinearColor SurfaceTintHigh;
    FLinearColor ApertureHintTint;
    FLinearColor AtmosphereTint;
    float VariationCellMeters;
    float BayMeters;
    float StoreyMeters;
    float ApertureWidthFraction;
    float ApertureHeightFraction;
    float ApertureSillFraction;
    float ApertureHintStrength;
    float ApertureFadeStartCm;
    float ApertureFadeEndCm;
    float AtmosphereStartCm;
    float AtmosphereEndCm;
    float AtmosphereStrength;
    float SurfaceRoughness;
    float SurfaceSpecular;
};

const FMaterialSpec MaterialSpecs[] = {
    {
        TEXT("MI_IPV5D_ContextFacadeR25_OfficialWall"),
        FLinearColor(0.31f, 0.28f, 0.23f, 1.0f),
        FLinearColor(0.55f, 0.50f, 0.40f, 1.0f),
        FLinearColor(0.025f, 0.055f, 0.078f, 1.0f),
        FLinearColor(0.48f, 0.54f, 0.55f, 1.0f),
        42.0f, 3.0f, 3.2f, 0.62f, 0.50f, 0.18f, 0.78f,
        55000.0f, 100000.0f, 60000.0f, 130000.0f, 0.22f, 0.69f, 0.27f},
    {
        TEXT("MI_IPV5D_ContextFacadeR25_OfficialRoof"),
        FLinearColor(0.09f, 0.10f, 0.105f, 1.0f),
        FLinearColor(0.18f, 0.19f, 0.20f, 1.0f),
        FLinearColor(0.025f, 0.055f, 0.078f, 1.0f),
        FLinearColor(0.48f, 0.54f, 0.55f, 1.0f),
        42.0f, 3.0f, 3.2f, 0.62f, 0.50f, 0.18f, 0.0f,
        55000.0f, 100000.0f, 60000.0f, 130000.0f, 0.18f, 0.84f, 0.17f},
    {
        TEXT("MI_IPV5D_ContextFacadeR25_FallbackWall"),
        FLinearColor(0.22f, 0.24f, 0.25f, 1.0f),
        FLinearColor(0.46f, 0.44f, 0.40f, 1.0f),
        FLinearColor(0.018f, 0.045f, 0.068f, 1.0f),
        FLinearColor(0.47f, 0.53f, 0.55f, 1.0f),
        44.0f, 3.15f, 3.15f, 0.60f, 0.48f, 0.20f, 0.74f,
        55000.0f, 95000.0f, 55000.0f, 125000.0f, 0.25f, 0.72f, 0.24f},
    {
        TEXT("MI_IPV5D_ContextFacadeR25_FallbackRoof"),
        FLinearColor(0.07f, 0.08f, 0.09f, 1.0f),
        FLinearColor(0.15f, 0.16f, 0.17f, 1.0f),
        FLinearColor(0.018f, 0.045f, 0.068f, 1.0f),
        FLinearColor(0.47f, 0.53f, 0.55f, 1.0f),
        44.0f, 3.15f, 3.15f, 0.60f, 0.48f, 0.20f, 0.0f,
        55000.0f, 95000.0f, 55000.0f, 125000.0f, 0.20f, 0.87f, 0.16f}};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == ExpectedMaterialCount);

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const TArray<FString>& OrderedMaterialPaths()
{
    static TArray<FString> Paths;
    if (Paths.IsEmpty())
    {
        Paths.Reserve(ExpectedMaterialCount);
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            Paths.Add(ObjectPath(MaterialRoot, Spec.Name));
        }
    }
    return Paths;
}

TArray<FString> ExpectedObjectPaths()
{
    TArray<FString> Paths = OrderedMaterialPaths();
    Paths.Add(MasterObjectPath);
    Paths.Sort();
    return Paths;
}

template <typename T>
T* LoadExact(const FString& Path)
{
    T* Object = LoadObject<T>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets, FString& OutError)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets())
    {
        Registry.Get().WaitForCompletion();
    }
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false);
    if (Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("R25 context-facade Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateExactRootRoster(bool bRequireSaved, FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherRootAssets(Assets, OutError))
    {
        return false;
    }
    const TArray<FString> Expected = ExpectedObjectPaths();
    TArray<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        Actual.Add(Asset.GetObjectPathString());
    }
    Actual.Sort();
    if (Actual != Expected)
    {
        OutError = FString::Printf(
            TEXT("R25 context-facade root must contain exactly five assets; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        for (const FString& Path : Expected)
        {
            UObject* Object = LoadObject<UObject>(nullptr, *Path);
            if (!Object || !FPackageName::DoesPackageExist(
                    Object->GetOutermost()->GetName()))
            {
                OutError = TEXT("An R25 context-facade asset is not persisted: ") +
                    Path;
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

class FScopedFreshRollback final
{
public:
    FScopedFreshRollback(TArray<UObject*>& InAssets, FString& InError)
        : Assets(InAssets), Error(InError)
    {
    }

    ~FScopedFreshRollback()
    {
        if (bCommitted)
        {
            return;
        }
        TArray<FAssetData> Data;
        FString Ignored;
        GatherRootAssets(Data, Ignored);
        TArray<UObject*> Disposable;
        for (const FAssetData& Row : Data)
        {
            UObject* Object = Row.GetAsset();
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (Object && Package &&
                Package->HasAnyPackageFlags(PKG_NewlyCreated) &&
                !FPackageName::DoesPackageExist(Package->GetName()))
            {
                Disposable.AddUnique(Object);
            }
        }
        if (!Disposable.IsEmpty() &&
            ObjectTools::DeleteObjectsUnchecked(Disposable) != Disposable.Num())
        {
            Error += TEXT(" R25_CONTEXT_FACADE_ROLLBACK_INCOMPLETE");
        }
        Assets.Reset();
    }

    void Commit() { bCommitted = true; }

private:
    TArray<UObject*>& Assets;
    FString& Error;
    bool bCommitted = false;
};

template <typename T>
T* AddMaterialExpression(
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

UMaterialExpressionScalarParameter* AddScalarParameter(
    UMaterial* Material,
    const FScalarSpec& Spec,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(
            Material, Spec.NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = Spec.ParameterName;
        Parameter->Group = MaterialParameterGroup;
        Parameter->DefaultValue = Spec.DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionVectorParameter* AddVectorParameter(
    UMaterial* Material,
    const FVectorSpec& Spec,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionVectorParameter* Parameter =
        AddMaterialExpression<UMaterialExpressionVectorParameter>(
            Material, Spec.NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = Spec.ParameterName;
        Parameter->Group = MaterialParameterGroup;
        Parameter->DefaultValue = Spec.DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

void GatherSpecScalars(
    const FMaterialSpec& Spec,
    TArray<TPair<FName, float>>& OutValues)
{
    OutValues = {
        {TEXT("VariationCellMeters"), Spec.VariationCellMeters},
        {TEXT("BayMeters"), Spec.BayMeters},
        {TEXT("StoreyMeters"), Spec.StoreyMeters},
        {TEXT("ApertureWidthFraction"), Spec.ApertureWidthFraction},
        {TEXT("ApertureHeightFraction"), Spec.ApertureHeightFraction},
        {TEXT("ApertureSillFraction"), Spec.ApertureSillFraction},
        {TEXT("ApertureHintStrength"), Spec.ApertureHintStrength},
        {TEXT("ApertureFadeStartCm"), Spec.ApertureFadeStartCm},
        {TEXT("ApertureFadeEndCm"), Spec.ApertureFadeEndCm},
        {TEXT("AtmosphereStartCm"), Spec.AtmosphereStartCm},
        {TEXT("AtmosphereEndCm"), Spec.AtmosphereEndCm},
        {TEXT("AtmosphereStrength"), Spec.AtmosphereStrength},
        {TEXT("SurfaceRoughness"), Spec.SurfaceRoughness},
        {TEXT("SurfaceSpecular"), Spec.SurfaceSpecular}};
}

void GatherSpecVectors(
    const FMaterialSpec& Spec,
    TArray<TPair<FName, FLinearColor>>& OutValues)
{
    OutValues = {
        {TEXT("SurfaceTintLow"), Spec.SurfaceTintLow},
        {TEXT("SurfaceTintHigh"), Spec.SurfaceTintHigh},
        {TEXT("ApertureHintTint"), Spec.ApertureHintTint},
        {TEXT("AtmosphereTint"), Spec.AtmosphereTint}};
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
              FName(TEXT("TRIAD.CreateIstanaExploreV5DContextFacadeR25"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the fresh R25 context-facade master material.");
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* Uv0 =
        AddMaterialExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("R25.UV0_SourceMetres"), -1800, -450);
    UMaterialExpressionWorldPosition* WorldPosition =
        AddMaterialExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("R25.WorldPositionCentimetres"), -1800, -300);
    UMaterialExpressionCameraPositionWS* CameraPosition =
        AddMaterialExpression<UMaterialExpressionCameraPositionWS>(
            Material, TEXT("R25.CameraPositionCentimetres"), -1800, -150);

    TArray<UMaterialExpressionVectorParameter*> Vectors;
    Vectors.Reserve(UE_ARRAY_COUNT(VectorSpecs));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(VectorSpecs); ++Index)
    {
        Vectors.Add(AddVectorParameter(
            Material, VectorSpecs[Index], -1800, 50 + Index * 130));
    }

    TArray<UMaterialExpressionScalarParameter*> Scalars;
    Scalars.Reserve(UE_ARRAY_COUNT(ScalarSpecs));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ScalarSpecs); ++Index)
    {
        Scalars.Add(AddScalarParameter(
            Material, ScalarSpecs[Index], -1350, -650 + Index * 110));
    }

    UMaterialExpressionCustom* SurfaceResponse =
        AddMaterialExpression<UMaterialExpressionCustom>(
            Material, TEXT("R25.ContextFacadeSurfaceResponse"), -550, -400);
    UMaterialExpressionComponentMask* RoughnessOutput =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("R25.ContextFacadeRoughnessA"), -250, -20);
    if (!Uv0 || !WorldPosition || !CameraPosition ||
        Vectors.Contains(nullptr) || Scalars.Contains(nullptr) ||
        !SurfaceResponse || !RoughnessOutput)
    {
        OutError = TEXT("Could not allocate the exact R25 context-facade material graph.");
        return nullptr;
    }

    SurfaceResponse->Description = SurfaceResponseDescription;
    SurfaceResponse->Code = SurfaceResponseCode;
    SurfaceResponse->OutputType = CMOT_Float4;
    const TCHAR* InputNames[] = {
        TEXT("UV0"), TEXT("WorldPositionCm"), TEXT("CameraPositionCm"),
        TEXT("SurfaceTintLow"), TEXT("SurfaceTintHigh"),
        TEXT("ApertureHintTint"), TEXT("AtmosphereTint"),
        TEXT("VariationCellMeters"), TEXT("BayMeters"),
        TEXT("StoreyMeters"), TEXT("ApertureWidthFraction"),
        TEXT("ApertureHeightFraction"), TEXT("ApertureSillFraction"),
        TEXT("ApertureHintStrength"), TEXT("ApertureFadeStartCm"),
        TEXT("ApertureFadeEndCm"), TEXT("AtmosphereStartCm"),
        TEXT("AtmosphereEndCm"), TEXT("AtmosphereStrength"),
        TEXT("SurfaceRoughness")};
    TArray<UMaterialExpression*> Inputs = {
        Uv0, WorldPosition, CameraPosition,
        Vectors[0], Vectors[1], Vectors[2], Vectors[3]};
    for (UMaterialExpressionScalarParameter* Scalar : Scalars)
    {
        if (Scalar->ParameterName != TEXT("SurfaceSpecular"))
        {
            Inputs.Add(Scalar);
        }
    }
    if (Inputs.Num() != UE_ARRAY_COUNT(InputNames))
    {
        OutError = TEXT("R25 custom input construction lost its exact 20-input roster.");
        return nullptr;
    }
    SurfaceResponse->Inputs.Reset(Inputs.Num());
    for (int32 Index = 0; Index < Inputs.Num(); ++Index)
    {
        FCustomInput& Input = SurfaceResponse->Inputs.AddDefaulted_GetRef();
        Input.InputName = InputNames[Index];
        Input.Input.Connect(0, Inputs[Index]);
    }

    Uv0->CoordinateIndex = 0;
    Uv0->UTiling = 1.0f;
    Uv0->VTiling = 1.0f;
    Uv0->UnMirrorU = false;
    Uv0->UnMirrorV = false;
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    RoughnessOutput->R = false;
    RoughnessOutput->G = false;
    RoughnessOutput->B = false;
    RoughnessOutput->A = true;
    RoughnessOutput->Input.Connect(0, SurfaceResponse);

    UMaterialExpressionScalarParameter* Specular = Scalars.Last();
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    EditorOnly->BaseColor.Connect(0, SurfaceResponse);
    EditorOnly->Roughness.Connect(0, RoughnessOutput);
    EditorOnly->Specular.Connect(0, Specular);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return Material;
}

UMaterialInstanceConstant* CreateInstance(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    FString& OutError)
{
    if (!Parent || Parent->GetPathName() != MasterObjectPath)
    {
        OutError = TEXT("R25 context-facade master is absent: ") +
            MasterObjectPath;
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
              Spec.Name,
              MaterialRoot,
              UMaterialInstanceConstant::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DContextFacadeR25"))))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create exact R25 context-facade material '%s'."),
            Spec.Name);
        return nullptr;
    }

    Instance->Modify();
    Instance->SetParentEditorOnly(Parent, false);
    Instance->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Instance->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    TArray<TPair<FName, float>> Scalars;
    TArray<TPair<FName, FLinearColor>> Vectors;
    GatherSpecScalars(Spec, Scalars);
    GatherSpecVectors(Spec, Vectors);
    for (const TPair<FName, float>& Pair : Scalars)
    {
        Instance->SetScalarParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    for (const TPair<FName, FLinearColor>& Pair : Vectors)
    {
        Instance->SetVectorParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    Instance->PostEditChange();
    Instance->MarkPackageDirty();
    OutError.Reset();
    return Instance;
}

bool ValidateMaster(UMaterial* Material, FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != MasterObjectPath || !EditorOnly ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        Material->TwoSided || !Material->bTangentSpaceNormal ||
        Material->bUseMaterialAttributes || Material->bScreenSpaceReflections ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        EditorOnly->ExpressionCollection.Expressions.Num() != 23)
    {
        OutError = TEXT("R25 context-facade master lost its exact opaque, one-sided, displacement-free 23-node policy.");
        return false;
    }

    TSet<FName> ScalarNames;
    TSet<FName> VectorNames;
    UMaterialExpressionCustom* Response = nullptr;
    UMaterialExpressionComponentMask* RoughnessOutput = nullptr;
    UMaterialExpressionScalarParameter* Specular = nullptr;
    int32 UvCount = 0;
    int32 WorldPositionCount = 0;
    int32 CameraPositionCount = 0;
    for (UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        if (UMaterialExpressionScalarParameter* Scalar =
                Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            ScalarNames.Add(Scalar->ParameterName);
            if (Scalar->ParameterName == TEXT("SurfaceSpecular"))
            {
                Specular = Scalar;
            }
        }
        else if (UMaterialExpressionVectorParameter* Vector =
                     Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            VectorNames.Add(Vector->ParameterName);
        }
        else if (Cast<UMaterialExpressionTextureCoordinate>(Expression))
        {
            ++UvCount;
        }
        else if (Cast<UMaterialExpressionWorldPosition>(Expression))
        {
            ++WorldPositionCount;
        }
        else if (Cast<UMaterialExpressionCameraPositionWS>(Expression))
        {
            ++CameraPositionCount;
        }
        else if (UMaterialExpressionCustom* Custom =
                     Cast<UMaterialExpressionCustom>(Expression))
        {
            Response = Custom;
        }
        else if (UMaterialExpressionComponentMask* Mask =
                     Cast<UMaterialExpressionComponentMask>(Expression))
        {
            RoughnessOutput = Mask;
        }
        else
        {
            OutError = TEXT("R25 context-facade master contains a forbidden expression class.");
            return false;
        }
    }
    if (ScalarNames.Num() != UE_ARRAY_COUNT(ScalarSpecs) ||
        VectorNames.Num() != UE_ARRAY_COUNT(VectorSpecs) ||
        UvCount != 1 || WorldPositionCount != 1 || CameraPositionCount != 1 ||
        !Response || !RoughnessOutput || !Specular ||
        Response->Desc != TEXT("R25.ContextFacadeSurfaceResponse") ||
        Response->Description != SurfaceResponseDescription ||
        Response->Code != SurfaceResponseCode ||
        Response->OutputType != CMOT_Float4 ||
        Response->Inputs.Num() != 20 ||
        !Response->AdditionalOutputs.IsEmpty() ||
        !Response->AdditionalDefines.IsEmpty() ||
        !Response->IncludeFilePaths.IsEmpty() ||
        EditorOnly->BaseColor.Expression != Response ||
        EditorOnly->Roughness.Expression != RoughnessOutput ||
        RoughnessOutput->Input.Expression != Response ||
        !RoughnessOutput->A || RoughnessOutput->R || RoughnessOutput->G ||
        RoughnessOutput->B || EditorOnly->Specular.Expression != Specular ||
        EditorOnly->Normal.Expression || EditorOnly->Metallic.Expression ||
        EditorOnly->AmbientOcclusion.Expression ||
        EditorOnly->EmissiveColor.Expression || EditorOnly->Opacity.Expression ||
        EditorOnly->OpacityMask.Expression ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->Displacement.Expression ||
        EditorOnly->PixelDepthOffset.Expression)
    {
        OutError = TEXT("R25 context-facade graph topology, parameter roster, code, or render-only bindings drifted.");
        return false;
    }
    for (const FScalarSpec& Expected : ScalarSpecs)
    {
        bool bFound = false;
        for (UMaterialExpression* Expression :
             EditorOnly->ExpressionCollection.Expressions)
        {
            const UMaterialExpressionScalarParameter* Scalar =
                Cast<UMaterialExpressionScalarParameter>(Expression);
            if (Scalar && Scalar->Desc == Expected.NodeId &&
                Scalar->ParameterName == Expected.ParameterName &&
                Scalar->Group == MaterialParameterGroup &&
                !Scalar->bUseCustomPrimitiveData &&
                FMath::IsNearlyEqual(
                    Scalar->DefaultValue, Expected.DefaultValue, 0.000001f))
            {
                bFound = true;
                break;
            }
        }
        if (!bFound)
        {
            OutError = FString::Printf(
                TEXT("R25 context-facade scalar node '%s' drifted."),
                Expected.NodeId);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialSpec& Spec,
    FString& OutError)
{
    if (!Instance ||
        Instance->GetClass() != UMaterialInstanceConstant::StaticClass() ||
        Instance->GetPathName() != ObjectPath(MaterialRoot, Spec.Name) ||
        !Instance->Parent ||
        Instance->Parent->GetPathName() != MasterObjectPath ||
        !Instance->NaniteOverrideMaterial.bEnableOverride ||
        Instance->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Instance->GetNaniteOverride() ||
        Instance->ScalarParameterValues.Num() != 14 ||
        Instance->VectorParameterValues.Num() != 4 ||
        !Instance->TextureParameterValues.IsEmpty() ||
        !Instance->RuntimeVirtualTextureParameterValues.IsEmpty() ||
        !Instance->SparseVolumeTextureParameterValues.IsEmpty() ||
        !Instance->FontParameterValues.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("R25 context-facade instance '%s' lost its exact texture-free policy."),
            Spec.Name);
        return false;
    }

    TArray<TPair<FName, float>> Scalars;
    TArray<TPair<FName, FLinearColor>> Vectors;
    GatherSpecScalars(Spec, Scalars);
    GatherSpecVectors(Spec, Vectors);
    for (const TPair<FName, float>& Pair : Scalars)
    {
        float Actual = 0.0f;
        if (!Instance->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !FMath::IsNearlyEqual(Actual, Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("R25 context-facade instance '%s' scalar '%s' drifted."),
                Spec.Name,
                *Pair.Key.ToString());
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : Vectors)
    {
        FLinearColor Actual;
        if (!Instance->GetVectorParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !Actual.Equals(Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("R25 context-facade instance '%s' vector '%s' drifted."),
                Spec.Name,
                *Pair.Key.ToString());
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateCompiledMaterial(
    UMaterialInterface* Material,
    FString& OutError)
{
    // The unattended R25 transaction runs the UE 5.5 D3D12 SM6 feature level.
    // A freshly compiled material therefore need not allocate a legacy SM5
    // resource. Validate the resource that the running editor will render.
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
    const TArray<FString> Errors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Material || !Resource || !ShaderMap ||
        !Resource->IsCompilationFinished() ||
        !Resource->IsGameThreadShaderMapComplete() ||
        !ShaderMap->IsValidForRendering() || Resource->IsDefaultMaterial() ||
        Errors.Num() != 0)
    {
        OutError = FString::Printf(
            TEXT("R25 context-facade material '%s' has no valid compiled active-feature-level resource (%d): resource=%d shaderMap=%d finished=%d gameThreadComplete=%d validForRendering=%d defaultMaterial=%d errors=[%s]."),
            Material ? *Material->GetPathName() : TEXT("<null>"),
            static_cast<int32>(FeatureLevel),
            Resource ? 1 : 0,
            ShaderMap ? 1 : 0,
            Resource && Resource->IsCompilationFinished() ? 1 : 0,
            Resource && Resource->IsGameThreadShaderMapComplete() ? 1 : 0,
            ShaderMap && ShaderMap->IsValidForRendering() ? 1 : 0,
            Resource && Resource->IsDefaultMaterial() ? 1 : 0,
            *FString::Join(Errors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    if (!ValidateExactRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UMaterial* Master = LoadExact<UMaterial>(MasterObjectPath);
    if (!ValidateMaster(Master, OutReport) ||
        !ValidateCompiledMaterial(Master, OutReport))
    {
        return false;
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance =
            LoadExact<UMaterialInstanceConstant>(
                ObjectPath(MaterialRoot, Spec.Name));
        if (!ValidateInstance(Instance, Spec, OutReport) ||
            !ValidateCompiledMaterial(Instance, OutReport))
        {
            return false;
        }
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_ASSETS_VALID assets=5 master=1 instances=4 exactOrder=officialWall+officialRoof+fallbackWall+fallbackRoof textureFree=true opaque=true defaultLit=true oneSided=true nanite=true apertureStrengthWalls=0.78+0.74 apertureStrengthRoofs=0.0 apertureFadeEndCm=100000+95000 atmosphereEndCm=130000+125000 recessedGlazingCue=true mullionTransomCue=true plinthContactCue=true materialOnlyNoGeometryChange=true collisionNavigationSensorRfAuthority=false surveyAsBuiltHyperreal=false sharedV5CAssetsMutated=false.");
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DContextFacadeR25AssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }
const FString& GetMasterMaterialObjectPath() { return MasterObjectPath; }
const TArray<FString>& GetOrderedMaterialObjectPaths()
{
    return OrderedMaterialPaths();
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing, OutError) || !Existing.IsEmpty())
    {
        OutError = TEXT("CreateFreshAssets requires an empty exact R25 context-facade asset root.");
        return false;
    }

    FScopedFreshRollback Rollback(OutAssets, OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterial* Master = CreateMaster(AssetTools, OutError);
    if (!Master)
    {
        return false;
    }
    OutAssets.Add(Master);
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance =
            CreateInstance(AssetTools, Spec, Master, OutError);
        if (!Instance)
        {
            return false;
        }
        OutAssets.Add(Instance);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (OutAssets.Num() != ExpectedAssetCount ||
        !ValidateInternal(false, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Fresh R25 context-facade asset validation failed.");
        }
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}
} // namespace TRIADIstanaExploreV5DContextFacadeR25AssetFactory
