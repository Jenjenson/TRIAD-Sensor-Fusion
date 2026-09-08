#include "TRIADIstanaExploreV5CSurroundingsAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
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
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "Ssl.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshResources.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <initializer_list>

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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/Surroundings"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MeshName(
    TEXT("SM_IPV5C_OfficialPreferredSurroundingContext"));
const FString MeshObjectPath(
    AssetRoot + TEXT("/") + MeshName + TEXT(".") + MeshName);
const FString MassingMasterName(TEXT("M_IPV5C_ContextMassing_Master"));
const FString MassingMasterObjectPath(
    MaterialRoot + TEXT("/") + MassingMasterName + TEXT(".") +
    MassingMasterName);

constexpr int32 ExpectedAssetCount = 6;
constexpr int32 ExpectedTriangleCount = 35424;
constexpr int32 ExpectedMaterialCount = 4;
constexpr int64 ExpectedObjBytes = 13977769;
constexpr int64 ExpectedMtlBytes = 778;
constexpr int64 ExpectedFeaturesBytes = 1960959;
constexpr int64 ExpectedManifestBytes = 19852;
constexpr int64 ExpectedContractBytes = 11730;
const FString ExpectedObjSha256(
    TEXT("774F7E30456B989D0BF9EEB013C10D2688A2B3DE87936529BBB154E83D344C74"));
const FString ExpectedMtlSha256(
    TEXT("6BBDA30E125F92EEF36D060404CA6EBB3F7DFC9DF95D7895766A57A99A737CA7"));
const FString ExpectedFeaturesSha256(
    TEXT("8825DDC93E7C6465B01AD5A5AD23B2E8368F2C825B6F8F8EB1FBBD2DA8138793"));
const FString ExpectedManifestSha256(
    TEXT("CEBFA56EC84E697305A35DA9CCD06B29619AB4500701B4A806B958B415F04F20"));
const FString ExpectedContractSha256(
    TEXT("52587014FC80459732B1C943056D1724684287B67CA0DE8556AF7E1E2F8C7FBD"));

const FName MaterialParameterGroup(TEXT("Istana Explore V5C Surroundings"));
const FString SurfaceResponseDescription(
    TEXT("TRIAD_IPV5C_NONAUTHORITATIVE_CONTEXT_MASSING_V3_SCREEN_ADAPTIVE_DUAL_SCALE_FACADE"));
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
    TEXT("float3 surface = lerp(SurfaceTintLow.rgb, SurfaceTintHigh.rgb, variation);\n")
    TEXT("float bay = max(BayMeters, 0.5);\n")
    TEXT("float storey = max(StoreyMeters, 2.0);\n")
    TEXT("float2 q = float2(UV0.x / bay, UV0.y / storey);\n")
    TEXT("float2 phase = frac(q);\n")
    TEXT("float2 aa = max(fwidth(q), float2(0.002, 0.002));\n")
    TEXT("float cellFootprint = max(aa.x, aa.y);\n")
    TEXT("float microReadability = 1.0 - smoothstep(0.30, 0.75, cellFootprint);\n")
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
    TEXT("float frameU = min(0.10, widthFraction * 0.28);\n")
    TEXT("float frameV = min(0.10, heightFraction * 0.28);\n")
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
    TEXT("float distanceCm = length(WorldPositionCm - CameraPositionCm);\n")
    TEXT("float apertureRange = max(ApertureFadeEndCm - ApertureFadeStartCm, 1.0);\n")
    TEXT("float apertureFade = 1.0 - saturate((distanceCm - ApertureFadeStartCm) / apertureRange);\n")
    TEXT("float facadeStrength = max(ApertureHintStrength, 0.0) * apertureFade * microReadability;\n")
    TEXT("float2 buildingSeed = floor(WorldPositionCm.xy / 6400.0);\n")
    TEXT("float cellHash = frac(sin(dot(floor(q) + buildingSeed, float2(41.37, 289.11))) * 15731.743);\n")
    TEXT("float cellOccupancy = lerp(0.66, 1.0, step(0.18, cellHash));\n")
    TEXT("float cellTone = lerp(0.72, 1.10, cellHash);\n")
    TEXT("float2 macroQ = float2(UV0.x / (bay * 4.0), UV0.y / (storey * 4.0));\n")
    TEXT("float macroHash = frac(sin(dot(floor(macroQ) + buildingSeed * 0.25, float2(73.19, 211.73))) * 31821.631);\n")
    TEXT("float macroTone = lerp(0.90, 1.06, macroHash);\n")
    TEXT("float3 dPdx = ddx(WorldPositionCm);\n")
    TEXT("float3 dPdy = ddy(WorldPositionCm);\n")
    TEXT("float3 geometricNormal = normalize(cross(dPdy, dPdx));\n")
    TEXT("float3 viewDirection = normalize(CameraPositionCm - WorldPositionCm);\n")
    TEXT("float glassFresnel = pow(1.0 - saturate(abs(dot(geometricNormal, viewDirection))), 4.0);\n")
    TEXT("float3 glassTint = saturate(ApertureHintTint.rgb * cellTone);\n")
    TEXT("glassTint = lerp(glassTint, AtmosphereTint.rgb, glassFresnel * 0.24);\n")
    TEXT("float wallRole = step(0.0001, ApertureHintStrength);\n")
    TEXT("float plinthMask = wallRole * (1.0 - smoothstep(0.18, 0.62, UV0.y));\n")
    TEXT("float3 facadeSurface = surface * lerp(1.0, macroTone, wallRole);\n")
    TEXT("facadeSurface = lerp(facadeSurface, facadeSurface * 0.58, plinthMask);\n")
    TEXT("float visibleFrame = saturate(frameMask * facadeStrength);\n")
    TEXT("float visibleGlass = saturate(insetGlass * facadeStrength * cellOccupancy);\n")
    TEXT("float3 frameTint = lerp(surface, SurfaceTintHigh.rgb, 0.24);\n")
    TEXT("facadeSurface = lerp(facadeSurface, frameTint, visibleFrame * 0.62);\n")
    TEXT("float3 brokenSurface = lerp(facadeSurface, glassTint, visibleGlass);\n")
    TEXT("float atmosphereRange = max(AtmosphereEndCm - AtmosphereStartCm, 1.0);\n")
    TEXT("float atmosphere = saturate((distanceCm - AtmosphereStartCm) / atmosphereRange) *\n")
    TEXT("    saturate(AtmosphereStrength);\n")
    TEXT("float luminance = dot(brokenSurface, float3(0.212639, 0.715169, 0.072192));\n")
    TEXT("float3 mutedSurface = lerp(brokenSurface, luminance.xxx, atmosphere * 0.35);\n")
    TEXT("float3 finalColor = saturate(lerp(mutedSurface, AtmosphereTint.rgb, atmosphere));\n")
    TEXT("float responseRoughness = lerp(saturate(SurfaceRoughness), 0.28, visibleGlass);\n")
    TEXT("float finalRoughness = lerp(responseRoughness, 1.0, atmosphere * 0.35);\n")
    TEXT("return float4(finalColor, saturate(finalRoughness));"));

struct FMasterScalarNodeSpec
{
    const TCHAR* NodeId;
    const TCHAR* ParameterName;
    float DefaultValue;
};

const FMasterScalarNodeSpec MasterScalarNodeSpecs[] = {
    {TEXT("V5C.VariationCellMeters"), TEXT("VariationCellMeters"), 48.0f},
    {TEXT("V5C.BayMeters"), TEXT("BayMeters"), 3.0f},
    {TEXT("V5C.StoreyMeters"), TEXT("StoreyMeters"), 3.2f},
    {TEXT("V5C.ApertureWidthFraction"), TEXT("ApertureWidthFraction"), 0.54f},
    {TEXT("V5C.ApertureHeightFraction"), TEXT("ApertureHeightFraction"), 0.42f},
    {TEXT("V5C.ApertureSillFraction"), TEXT("ApertureSillFraction"), 0.23f},
    {TEXT("V5C.ApertureHintStrength"), TEXT("ApertureHintStrength"), 0.0f},
    {TEXT("V5C.ApertureFadeStartCm"), TEXT("ApertureFadeStartCm"), 35000.0f},
    {TEXT("V5C.ApertureFadeEndCm"), TEXT("ApertureFadeEndCm"), 70000.0f},
    {TEXT("V5C.AtmosphereStartCm"), TEXT("AtmosphereStartCm"), 40000.0f},
    {TEXT("V5C.AtmosphereEndCm"), TEXT("AtmosphereEndCm"), 110000.0f},
    {TEXT("V5C.AtmosphereStrength"), TEXT("AtmosphereStrength"), 0.34f},
    {TEXT("V5C.SurfaceRoughness"), TEXT("SurfaceRoughness"), 0.74f},
    {TEXT("V5C.SurfaceSpecular"), TEXT("SurfaceSpecular"), 0.22f}};
static_assert(UE_ARRAY_COUNT(MasterScalarNodeSpecs) == 14);

struct FMasterVectorNodeSpec
{
    const TCHAR* NodeId;
    const TCHAR* ParameterName;
    FLinearColor DefaultValue;
};

const FMasterVectorNodeSpec MasterVectorNodeSpecs[] = {
    {TEXT("V5C.SurfaceTintLow"), TEXT("SurfaceTintLow"),
     FLinearColor(0.24f, 0.23f, 0.20f, 1.0f)},
    {TEXT("V5C.SurfaceTintHigh"), TEXT("SurfaceTintHigh"),
     FLinearColor(0.36f, 0.34f, 0.29f, 1.0f)},
    {TEXT("V5C.ApertureHintTint"), TEXT("ApertureHintTint"),
     FLinearColor(0.020f, 0.035f, 0.050f, 1.0f)},
    {TEXT("V5C.AtmosphereTint"), TEXT("AtmosphereTint"),
     FLinearColor(0.45f, 0.50f, 0.46f, 1.0f)}};
static_assert(UE_ARRAY_COUNT(MasterVectorNodeSpecs) == 4);

struct FMasterCustomInputSpec
{
    const TCHAR* InputName;
    const TCHAR* NodeId;
};

const FMasterCustomInputSpec MasterCustomInputSpecs[] = {
    {TEXT("UV0"), TEXT("V5C.UV0_SourceMetres")},
    {TEXT("WorldPositionCm"), TEXT("V5C.WorldPositionCentimetres")},
    {TEXT("CameraPositionCm"), TEXT("V5C.CameraPositionCentimetres")},
    {TEXT("SurfaceTintLow"), TEXT("V5C.SurfaceTintLow")},
    {TEXT("SurfaceTintHigh"), TEXT("V5C.SurfaceTintHigh")},
    {TEXT("ApertureHintTint"), TEXT("V5C.ApertureHintTint")},
    {TEXT("AtmosphereTint"), TEXT("V5C.AtmosphereTint")},
    {TEXT("VariationCellMeters"), TEXT("V5C.VariationCellMeters")},
    {TEXT("BayMeters"), TEXT("V5C.BayMeters")},
    {TEXT("StoreyMeters"), TEXT("V5C.StoreyMeters")},
    {TEXT("ApertureWidthFraction"), TEXT("V5C.ApertureWidthFraction")},
    {TEXT("ApertureHeightFraction"), TEXT("V5C.ApertureHeightFraction")},
    {TEXT("ApertureSillFraction"), TEXT("V5C.ApertureSillFraction")},
    {TEXT("ApertureHintStrength"), TEXT("V5C.ApertureHintStrength")},
    {TEXT("ApertureFadeStartCm"), TEXT("V5C.ApertureFadeStartCm")},
    {TEXT("ApertureFadeEndCm"), TEXT("V5C.ApertureFadeEndCm")},
    {TEXT("AtmosphereStartCm"), TEXT("V5C.AtmosphereStartCm")},
    {TEXT("AtmosphereEndCm"), TEXT("V5C.AtmosphereEndCm")},
    {TEXT("AtmosphereStrength"), TEXT("V5C.AtmosphereStrength")},
    {TEXT("SurfaceRoughness"), TEXT("V5C.SurfaceRoughness")}};
static_assert(UE_ARRAY_COUNT(MasterCustomInputSpecs) == 20);

struct FMaterialSpec
{
    const TCHAR* Name;
    int32 Triangles;
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
        TEXT("M_IPV5C_OfficialContextRender"),
        7690,
        FLinearColor(0.24f, 0.23f, 0.20f, 1.0f),
        FLinearColor(0.36f, 0.34f, 0.29f, 1.0f),
        FLinearColor(0.020f, 0.035f, 0.050f, 1.0f),
        FLinearColor(0.45f, 0.50f, 0.46f, 1.0f),
        48.0f, 3.0f, 3.2f, 0.54f, 0.42f, 0.23f, 0.56f,
        35000.0f, 70000.0f, 40000.0f, 110000.0f, 0.34f, 0.74f, 0.22f},
    {
        TEXT("M_IPV5C_OfficialContextRoof"),
        2945,
        FLinearColor(0.075f, 0.085f, 0.095f, 1.0f),
        FLinearColor(0.13f, 0.14f, 0.15f, 1.0f),
        FLinearColor(0.020f, 0.035f, 0.050f, 1.0f),
        FLinearColor(0.45f, 0.50f, 0.46f, 1.0f),
        48.0f, 3.0f, 3.2f, 0.54f, 0.42f, 0.23f, 0.0f,
        35000.0f, 70000.0f, 40000.0f, 110000.0f, 0.30f, 0.86f, 0.16f},
    {
        TEXT("M_IPV5C_OsmFallbackContextRender"),
        17690,
        FLinearColor(0.18f, 0.20f, 0.22f, 1.0f),
        FLinearColor(0.30f, 0.32f, 0.34f, 1.0f),
        FLinearColor(0.018f, 0.030f, 0.045f, 1.0f),
        FLinearColor(0.45f, 0.50f, 0.46f, 1.0f),
        56.0f, 3.2f, 3.2f, 0.56f, 0.44f, 0.22f, 0.54f,
        35000.0f, 70000.0f, 40000.0f, 110000.0f, 0.38f, 0.78f, 0.20f},
    {
        TEXT("M_IPV5C_OsmFallbackContextRoof"),
        7099,
        FLinearColor(0.055f, 0.065f, 0.075f, 1.0f),
        FLinearColor(0.11f, 0.12f, 0.13f, 1.0f),
        FLinearColor(0.018f, 0.030f, 0.045f, 1.0f),
        FLinearColor(0.45f, 0.50f, 0.46f, 1.0f),
        56.0f, 3.2f, 3.2f, 0.56f, 0.44f, 0.22f, 0.0f,
        35000.0f, 70000.0f, 40000.0f, 110000.0f, 0.34f, 0.88f, 0.16f}};
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

FString ProjectSourcePath(const TCHAR* Relative)
{
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceAssets"), Relative));
}

FString ObjSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Surroundings/Generated/"
        "SM_IPV5C_OfficialPreferredSurroundingContext.obj"));
}

FString MtlSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Surroundings/Generated/"
        "SM_IPV5C_OfficialPreferredSurroundingContext.mtl"));
}

FString FeaturesSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Surroundings/Generated/"
        "IstanaPublicViewV5CSurroundings.features.json"));
}

FString ManifestSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Surroundings/Generated/"
        "IstanaPublicViewV5CSurroundings.manifest.json"));
}

FString ContractSourcePath()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5C/Surroundings/"
        "istana_public_view_v5c_surroundings.contract.json"));
}

template <typename T>
T* LoadExact(const FString& Path)
{
    T* Object = LoadObject<T>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

bool ValidateSourceHash(
    const FString& Filename,
    int64 ExpectedBytes,
    const FString& ExpectedSha256,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != ExpectedBytes)
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings source byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            ExpectedBytes,
            Bytes.Num());
        return false;
    }
    FString ActualSha256;
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) ==
        nullptr)
    {
        OutError = TEXT("V5C surroundings source SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    ActualSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5C surroundings source SHA-256 admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (ActualSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings source SHA-256 guard failed for '%s': expected=%s actual=%s."),
            *Filename,
            *ExpectedSha256,
            *ActualSha256);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAllSourceHashes(FString& OutError)
{
    return ValidateSourceHash(
               ObjSourcePath(), ExpectedObjBytes, ExpectedObjSha256, OutError) &&
        ValidateSourceHash(
               MtlSourcePath(), ExpectedMtlBytes, ExpectedMtlSha256, OutError) &&
        ValidateSourceHash(
               FeaturesSourcePath(), ExpectedFeaturesBytes,
               ExpectedFeaturesSha256, OutError) &&
        ValidateSourceHash(
               ManifestSourcePath(), ExpectedManifestBytes,
               ExpectedManifestSha256, OutError) &&
        ValidateSourceHash(
               ContractSourcePath(), ExpectedContractBytes,
               ExpectedContractSha256, OutError);
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
        OutError = TEXT("V5C surroundings Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

TArray<FString> ExpectedObjectPaths()
{
    TArray<FString> Paths = OrderedMaterialPaths();
    Paths.Add(MassingMasterObjectPath);
    Paths.Add(MeshObjectPath);
    Paths.Sort();
    return Paths;
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
            TEXT("V5C surroundings root must contain exactly six assets; actual=[%s]."),
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
                OutError = TEXT("A V5C surroundings asset is not persisted: ") +
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
            Error += TEXT(" V5C_SURROUNDINGS_ROLLBACK_INCOMPLETE");
        }
        Assets.Reset();
    }

    void Commit() { bCommitted = true; }

private:
    TArray<UObject*>& Assets;
    FString& Error;
    bool bCommitted = false;
};

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
    const TCHAR* NodeId,
    const TCHAR* ParameterName,
    float DefaultValue,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddMaterialExpression<UMaterialExpressionScalarParameter>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = MaterialParameterGroup;
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
        AddMaterialExpression<UMaterialExpressionVectorParameter>(
            Material, NodeId, EditorX, EditorY);
    if (Parameter)
    {
        Parameter->ParameterName = ParameterName;
        Parameter->Group = MaterialParameterGroup;
        Parameter->DefaultValue = DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionCustom* AddCustomExpression(
    UMaterial* Material,
    const TCHAR* NodeId,
    const FString& Description,
    const FString& Code,
    std::initializer_list<const TCHAR*> InputNames,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialExpressionCustom* Expression =
        AddMaterialExpression<UMaterialExpressionCustom>(
            Material, NodeId, EditorX, EditorY);
    if (!Expression)
    {
        return nullptr;
    }
    Expression->Description = Description;
    Expression->Code = Code;
    Expression->OutputType = CMOT_Float4;
    Expression->Inputs.Reset(static_cast<int32>(InputNames.size()));
    for (const TCHAR* InputName : InputNames)
    {
        FCustomInput& Input = Expression->Inputs.AddDefaulted_GetRef();
        Input.InputName = InputName;
    }
    return Expression;
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

UMaterial* CreateMassingMaster(IAssetTools& AssetTools, FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MassingMasterName,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5CSurroundingsAssets"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create the fresh V5C context-massing master material.");
        return nullptr;
    }

    UMaterialExpressionTextureCoordinate* Uv0 =
        AddMaterialExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5C.UV0_SourceMetres"), -1800, -450);
    UMaterialExpressionWorldPosition* WorldPosition =
        AddMaterialExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("V5C.WorldPositionCentimetres"), -1800, -300);
    UMaterialExpressionCameraPositionWS* CameraPosition =
        AddMaterialExpression<UMaterialExpressionCameraPositionWS>(
            Material, TEXT("V5C.CameraPositionCentimetres"), -1800, -150);

    UMaterialExpressionVectorParameter* SurfaceTintLow = AddVectorParameter(
        Material, TEXT("V5C.SurfaceTintLow"), TEXT("SurfaceTintLow"),
        FLinearColor(0.24f, 0.23f, 0.20f, 1.0f), -1800, 50);
    UMaterialExpressionVectorParameter* SurfaceTintHigh = AddVectorParameter(
        Material, TEXT("V5C.SurfaceTintHigh"), TEXT("SurfaceTintHigh"),
        FLinearColor(0.36f, 0.34f, 0.29f, 1.0f), -1800, 180);
    UMaterialExpressionVectorParameter* ApertureHintTint = AddVectorParameter(
        Material, TEXT("V5C.ApertureHintTint"), TEXT("ApertureHintTint"),
        FLinearColor(0.020f, 0.035f, 0.050f, 1.0f), -1800, 310);
    UMaterialExpressionVectorParameter* AtmosphereTint = AddVectorParameter(
        Material, TEXT("V5C.AtmosphereTint"), TEXT("AtmosphereTint"),
        FLinearColor(0.45f, 0.50f, 0.46f, 1.0f), -1800, 440);

    UMaterialExpressionScalarParameter* VariationCellMeters = AddScalarParameter(
        Material, TEXT("V5C.VariationCellMeters"), TEXT("VariationCellMeters"),
        48.0f, -1350, -650);
    UMaterialExpressionScalarParameter* BayMeters = AddScalarParameter(
        Material, TEXT("V5C.BayMeters"), TEXT("BayMeters"),
        3.0f, -1350, -540);
    UMaterialExpressionScalarParameter* StoreyMeters = AddScalarParameter(
        Material, TEXT("V5C.StoreyMeters"), TEXT("StoreyMeters"),
        3.2f, -1350, -430);
    UMaterialExpressionScalarParameter* ApertureWidth = AddScalarParameter(
        Material, TEXT("V5C.ApertureWidthFraction"),
        TEXT("ApertureWidthFraction"), 0.54f, -1350, -320);
    UMaterialExpressionScalarParameter* ApertureHeight = AddScalarParameter(
        Material, TEXT("V5C.ApertureHeightFraction"),
        TEXT("ApertureHeightFraction"), 0.42f, -1350, -210);
    UMaterialExpressionScalarParameter* ApertureSill = AddScalarParameter(
        Material, TEXT("V5C.ApertureSillFraction"),
        TEXT("ApertureSillFraction"), 0.23f, -1350, -100);
    UMaterialExpressionScalarParameter* ApertureStrength = AddScalarParameter(
        Material, TEXT("V5C.ApertureHintStrength"),
        TEXT("ApertureHintStrength"), 0.0f, -1350, 10);
    UMaterialExpressionScalarParameter* ApertureFadeStart = AddScalarParameter(
        Material, TEXT("V5C.ApertureFadeStartCm"),
        TEXT("ApertureFadeStartCm"), 35000.0f, -1350, 120);
    UMaterialExpressionScalarParameter* ApertureFadeEnd = AddScalarParameter(
        Material, TEXT("V5C.ApertureFadeEndCm"),
        TEXT("ApertureFadeEndCm"), 70000.0f, -1350, 230);
    UMaterialExpressionScalarParameter* AtmosphereStart = AddScalarParameter(
        Material, TEXT("V5C.AtmosphereStartCm"),
        TEXT("AtmosphereStartCm"), 40000.0f, -1350, 340);
    UMaterialExpressionScalarParameter* AtmosphereEnd = AddScalarParameter(
        Material, TEXT("V5C.AtmosphereEndCm"),
        TEXT("AtmosphereEndCm"), 110000.0f, -1350, 450);
    UMaterialExpressionScalarParameter* AtmosphereStrength = AddScalarParameter(
        Material, TEXT("V5C.AtmosphereStrength"),
        TEXT("AtmosphereStrength"), 0.34f, -1350, 560);
    UMaterialExpressionScalarParameter* SurfaceRoughness = AddScalarParameter(
        Material, TEXT("V5C.SurfaceRoughness"),
        TEXT("SurfaceRoughness"), 0.74f, -250, 100);
    UMaterialExpressionScalarParameter* SurfaceSpecular = AddScalarParameter(
        Material, TEXT("V5C.SurfaceSpecular"),
        TEXT("SurfaceSpecular"), 0.22f, -250, 230);

    UMaterialExpressionCustom* SurfaceResponse = AddCustomExpression(
        Material,
        TEXT("V5C.ContextMassingSurfaceResponse"),
        SurfaceResponseDescription,
        SurfaceResponseCode,
        {
            TEXT("UV0"), TEXT("WorldPositionCm"), TEXT("CameraPositionCm"),
            TEXT("SurfaceTintLow"), TEXT("SurfaceTintHigh"),
            TEXT("ApertureHintTint"), TEXT("AtmosphereTint"),
            TEXT("VariationCellMeters"), TEXT("BayMeters"),
            TEXT("StoreyMeters"), TEXT("ApertureWidthFraction"),
            TEXT("ApertureHeightFraction"), TEXT("ApertureSillFraction"),
            TEXT("ApertureHintStrength"), TEXT("ApertureFadeStartCm"),
            TEXT("ApertureFadeEndCm"), TEXT("AtmosphereStartCm"),
            TEXT("AtmosphereEndCm"), TEXT("AtmosphereStrength"),
            TEXT("SurfaceRoughness")
        },
        -550,
        -400);
    UMaterialExpressionComponentMask* RoughnessOutput =
        AddMaterialExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("V5C.ContextMassingRoughnessA"), -250, -20);

    const TArray<UMaterialExpression*> Inputs = {
        Uv0, WorldPosition, CameraPosition,
        SurfaceTintLow, SurfaceTintHigh, ApertureHintTint, AtmosphereTint,
        VariationCellMeters, BayMeters, StoreyMeters, ApertureWidth,
        ApertureHeight, ApertureSill, ApertureStrength, ApertureFadeStart,
        ApertureFadeEnd, AtmosphereStart, AtmosphereEnd, AtmosphereStrength,
        SurfaceRoughness};
    if (!Uv0 || !WorldPosition || !CameraPosition || !SurfaceTintLow ||
        !SurfaceTintHigh || !ApertureHintTint || !AtmosphereTint ||
        !VariationCellMeters || !BayMeters || !StoreyMeters ||
        !ApertureWidth || !ApertureHeight || !ApertureSill ||
        !ApertureStrength || !ApertureFadeStart || !ApertureFadeEnd ||
        !AtmosphereStart || !AtmosphereEnd || !AtmosphereStrength ||
        !SurfaceRoughness || !SurfaceSpecular || !SurfaceResponse ||
        !RoughnessOutput ||
        Inputs.Num() != SurfaceResponse->Inputs.Num())
    {
        OutError = TEXT("Could not allocate the exact V5C context-massing material graph.");
        return nullptr;
    }
    Uv0->CoordinateIndex = 0;
    Uv0->UTiling = 1.0f;
    Uv0->VTiling = 1.0f;
    Uv0->UnMirrorU = false;
    Uv0->UnMirrorV = false;
    WorldPosition->WorldPositionShaderOffset =
        WPT_ExcludeAllShaderOffsets;
    for (int32 Index = 0; Index < Inputs.Num(); ++Index)
    {
        SurfaceResponse->Inputs[Index].Input.Connect(0, Inputs[Index]);
    }
    RoughnessOutput->R = false;
    RoughnessOutput->G = false;
    RoughnessOutput->B = false;
    RoughnessOutput->A = true;
    RoughnessOutput->Input.Connect(0, SurfaceResponse);

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
    EditorOnly->Specular.Connect(0, SurfaceSpecular);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return Material;
}

UMaterialInstanceConstant* CreateMaterialInstance(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    FString& OutError)
{
    if (!Parent || Parent->GetPathName() != MassingMasterObjectPath)
    {
        OutError = TEXT("V5C surroundings material parent is absent: ") +
            MassingMasterObjectPath;
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
              FName(TEXT("TRIAD.CreateIstanaExploreV5CSurroundingsAssets"))))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create exact V5C surroundings material '%s'."),
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

bool ValidateMassingMaster(UMaterial* Material, FString& OutError)
{
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != MassingMasterObjectPath || !EditorOnly ||
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
        OutError = TEXT("V5C context-massing master lost its exact opaque, one-sided, displacement-free material policy or 23-node graph.");
        return false;
    }

    TMap<FString, UMaterialExpression*> Nodes;
    TSet<FName> ScalarNames;
    TSet<FName> VectorNames;
    int32 UvCount = 0;
    int32 WorldPositionCount = 0;
    int32 CameraPositionCount = 0;
    int32 CustomCount = 0;
    int32 ComponentMaskCount = 0;
    UMaterialExpressionCustom* Response = nullptr;
    UMaterialExpressionComponentMask* RoughnessOutput = nullptr;
    UMaterialExpressionScalarParameter* Roughness = nullptr;
    UMaterialExpressionScalarParameter* Specular = nullptr;
    for (UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        if (!Expression || Expression->Desc.IsEmpty() ||
            Nodes.Contains(Expression->Desc))
        {
            OutError = TEXT("V5C context-massing graph has an absent, unnamed, or duplicate expression.");
            return false;
        }
        Nodes.Add(Expression->Desc, Expression);
        if (UMaterialExpressionScalarParameter* ScalarParameter =
                Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            if (ScalarParameter->ParameterName.IsNone() ||
                ScalarParameter->Group != MaterialParameterGroup ||
                ScalarParameter->bUseCustomPrimitiveData ||
                ScalarNames.Contains(ScalarParameter->ParameterName))
            {
                OutError = TEXT("V5C context-massing master has an invalid scalar parameter.");
                return false;
            }
            ScalarNames.Add(ScalarParameter->ParameterName);
            if (ScalarParameter->ParameterName == TEXT("SurfaceRoughness"))
            {
                Roughness = ScalarParameter;
            }
            else if (ScalarParameter->ParameterName == TEXT("SurfaceSpecular"))
            {
                Specular = ScalarParameter;
            }
        }
        else if (UMaterialExpressionVectorParameter* VectorParameter =
                     Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            if (VectorParameter->ParameterName.IsNone() ||
                VectorParameter->Group != MaterialParameterGroup ||
                VectorParameter->bUseCustomPrimitiveData ||
                VectorNames.Contains(VectorParameter->ParameterName))
            {
                OutError = TEXT("V5C context-massing master has an invalid vector parameter.");
                return false;
            }
            VectorNames.Add(VectorParameter->ParameterName);
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
            ++CustomCount;
            Response = Custom;
        }
        else if (UMaterialExpressionComponentMask* ComponentMask =
                     Cast<UMaterialExpressionComponentMask>(Expression))
        {
            ++ComponentMaskCount;
            RoughnessOutput = ComponentMask;
        }
        else
        {
            OutError = FString::Printf(
                TEXT("V5C context-massing graph contains forbidden expression class '%s'."),
                *Expression->GetClass()->GetName());
            return false;
        }
    }

    TSet<FName> ExpectedScalarNames;
    TSet<FName> ExpectedVectorNames;
    for (const FMasterScalarNodeSpec& Spec : MasterScalarNodeSpecs)
    {
        ExpectedScalarNames.Add(FName(Spec.ParameterName));
        const UMaterialExpressionScalarParameter* Parameter =
            Cast<UMaterialExpressionScalarParameter>(
                Nodes.FindRef(Spec.NodeId));
        if (!Parameter ||
            Parameter->ParameterName != FName(Spec.ParameterName) ||
            Parameter->Group != MaterialParameterGroup ||
            Parameter->bUseCustomPrimitiveData ||
            Parameter->PrimitiveDataIndex != 0 ||
            !FMath::IsNearlyEqual(
                Parameter->DefaultValue, Spec.DefaultValue, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V5C context-massing scalar node '%s' lost its exact Desc/parameter/default tuple."),
                Spec.NodeId);
            return false;
        }
    }
    for (const FMasterVectorNodeSpec& Spec : MasterVectorNodeSpecs)
    {
        ExpectedVectorNames.Add(FName(Spec.ParameterName));
        const UMaterialExpressionVectorParameter* Parameter =
            Cast<UMaterialExpressionVectorParameter>(
                Nodes.FindRef(Spec.NodeId));
        if (!Parameter ||
            Parameter->ParameterName != FName(Spec.ParameterName) ||
            Parameter->Group != MaterialParameterGroup ||
            Parameter->bUseCustomPrimitiveData ||
            Parameter->PrimitiveDataIndex != 0 ||
            !Parameter->DefaultValue.Equals(Spec.DefaultValue, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V5C context-massing vector node '%s' lost its exact Desc/parameter/default tuple."),
                Spec.NodeId);
            return false;
        }
    }
    const UMaterialExpressionTextureCoordinate* Uv0 =
        Cast<UMaterialExpressionTextureCoordinate>(
            Nodes.FindRef(TEXT("V5C.UV0_SourceMetres")));
    const UMaterialExpressionWorldPosition* WorldPosition =
        Cast<UMaterialExpressionWorldPosition>(
            Nodes.FindRef(TEXT("V5C.WorldPositionCentimetres")));
    const UMaterialExpressionCameraPositionWS* CameraPosition =
        Cast<UMaterialExpressionCameraPositionWS>(
            Nodes.FindRef(TEXT("V5C.CameraPositionCentimetres")));
    if (!SameNames(ExpectedScalarNames, ScalarNames) ||
        !SameNames(ExpectedVectorNames, VectorNames) || UvCount != 1 ||
        WorldPositionCount != 1 || CameraPositionCount != 1 ||
        CustomCount != 1 || ComponentMaskCount != 1 ||
        !Uv0 || !WorldPosition || !CameraPosition ||
        !Response || !Roughness || !Specular || !RoughnessOutput ||
        Response->Desc != TEXT("V5C.ContextMassingSurfaceResponse") ||
        Response->Description != SurfaceResponseDescription ||
        Response->Code != SurfaceResponseCode ||
        Response->OutputType != CMOT_Float4 ||
        Response->Inputs.Num() != UE_ARRAY_COUNT(MasterCustomInputSpecs) ||
        !Response->AdditionalOutputs.IsEmpty() ||
        !Response->AdditionalDefines.IsEmpty() ||
        !Response->IncludeFilePaths.IsEmpty() ||
        EditorOnly->BaseColor.Expression != Response ||
        EditorOnly->BaseColor.OutputIndex != 0 ||
        RoughnessOutput->Desc != TEXT("V5C.ContextMassingRoughnessA") ||
        RoughnessOutput->R || RoughnessOutput->G || RoughnessOutput->B ||
        !RoughnessOutput->A ||
        RoughnessOutput->Input.Expression != Response ||
        RoughnessOutput->Input.OutputIndex != 0 ||
        EditorOnly->Roughness.Expression != RoughnessOutput ||
        EditorOnly->Roughness.OutputIndex != 0 ||
        EditorOnly->Specular.Expression != Specular ||
        EditorOnly->Specular.OutputIndex != 0 ||
        EditorOnly->Normal.Expression || EditorOnly->Metallic.Expression ||
        EditorOnly->AmbientOcclusion.Expression ||
        EditorOnly->EmissiveColor.Expression || EditorOnly->Opacity.Expression ||
        EditorOnly->OpacityMask.Expression || EditorOnly->Refraction.Expression ||
        EditorOnly->WorldPositionOffset.Expression ||
        EditorOnly->Displacement.Expression ||
        EditorOnly->PixelDepthOffset.Expression)
    {
        OutError = TEXT("V5C context-massing master graph topology, parameter roster, or render-only output binding drifted.");
        return false;
    }
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(MasterCustomInputSpecs);
         ++Index)
    {
        const FMasterCustomInputSpec& Spec = MasterCustomInputSpecs[Index];
        UMaterialExpression* const* ExpectedNode =
            Nodes.Find(Spec.NodeId);
        if (!ExpectedNode ||
            Response->Inputs[Index].InputName != FName(Spec.InputName) ||
            Response->Inputs[Index].Input.Expression != *ExpectedNode ||
            Response->Inputs[Index].Input.OutputIndex != 0)
        {
            OutError = FString::Printf(
                TEXT("V5C context-massing custom input %d lost exact name '%s' or node '%s'."),
                Index,
                Spec.InputName,
                Spec.NodeId);
            return false;
        }
    }
    if (Uv0->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(Uv0->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(Uv0->VTiling, 1.0f, 0.000001f) ||
        Uv0->UnMirrorU || Uv0->UnMirrorV ||
        WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets)
    {
        OutError = TEXT("V5C context-massing source-metre UV0 or absolute world-position-no-offset tuple changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMaterialInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialSpec& Spec,
    FString& OutError)
{
    const FStaticParameterSet StaticParameters = Instance
        ? Instance->GetStaticParameters()
        : FStaticParameterSet();
    const FString BaseOverrides = Instance
        ? EnabledBasePropertyOverrideNames(Instance->BasePropertyOverrides)
        : TEXT("instance-null");
    if (!Instance ||
        Instance->GetClass() != UMaterialInstanceConstant::StaticClass() ||
        Instance->GetPathName() != ObjectPath(MaterialRoot, Spec.Name) ||
        !Instance->Parent ||
        Instance->Parent->GetPathName() != MassingMasterObjectPath ||
        !Instance->NaniteOverrideMaterial.bEnableOverride ||
        Instance->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Instance->GetNaniteOverride() ||
        !Instance->TextureParameterValues.IsEmpty() ||
        !Instance->DoubleVectorParameterValues.IsEmpty() ||
        !Instance->TextureCollectionParameterValues.IsEmpty() ||
        !Instance->RuntimeVirtualTextureParameterValues.IsEmpty() ||
        !Instance->SparseVolumeTextureParameterValues.IsEmpty() ||
        !Instance->FontParameterValues.IsEmpty() ||
        !Instance->UserSceneTextureOverrides.IsEmpty() ||
        !StaticParameters.StaticSwitchParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.StaticComponentMaskParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.TerrainLayerWeightParameters.IsEmpty() ||
        StaticParameters.bHasMaterialLayers || !BaseOverrides.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings procedural material instance drifted: %s overrides=[%s]."),
            Spec.Name,
            *BaseOverrides);
        return false;
    }

    TArray<TPair<FName, float>> ExpectedScalars;
    TArray<TPair<FName, FLinearColor>> ExpectedVectors;
    GatherSpecScalars(Spec, ExpectedScalars);
    GatherSpecVectors(Spec, ExpectedVectors);
    TSet<FName> ExpectedScalarNames;
    TSet<FName> ActualScalarNames;
    TSet<FName> ExpectedVectorNames;
    TSet<FName> ActualVectorNames;
    for (const TPair<FName, float>& Pair : ExpectedScalars)
    {
        ExpectedScalarNames.Add(Pair.Key);
    }
    for (const TPair<FName, FLinearColor>& Pair : ExpectedVectors)
    {
        ExpectedVectorNames.Add(Pair.Key);
    }
    for (const FScalarParameterValue& Value : Instance->ScalarParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualScalarNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("V5C surroundings instance has an invalid or duplicate scalar override.");
            return false;
        }
        ActualScalarNames.Add(Value.ParameterInfo.Name);
    }
    for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualVectorNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = TEXT("V5C surroundings instance has an invalid or duplicate vector override.");
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    if (!SameNames(ExpectedScalarNames, ActualScalarNames) ||
        !SameNames(ExpectedVectorNames, ActualVectorNames))
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings instance '%s' override roster changed."),
            Spec.Name);
        return false;
    }
    for (const TPair<FName, float>& Pair : ExpectedScalars)
    {
        float Actual = 0.0f;
        if (!Instance->GetScalarParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !FMath::IsNearlyEqual(Actual, Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V5C surroundings instance '%s' scalar '%s' changed."),
                Spec.Name,
                *Pair.Key.ToString());
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : ExpectedVectors)
    {
        FLinearColor Actual;
        if (!Instance->GetVectorParameterValue(
                FHashedMaterialParameterInfo(Pair.Key), Actual, true) ||
            !Actual.Equals(Pair.Value, 0.000001f))
        {
            OutError = FString::Printf(
                TEXT("V5C surroundings instance '%s' vector '%s' changed."),
                Spec.Name,
                *Pair.Key.ToString());
            return false;
        }
    }
    const bool bRoof = FString(Spec.Name).Contains(TEXT("Roof"));
    if (bRoof != FMath::IsNearlyZero(Spec.ApertureHintStrength, 0.000001f))
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings aperture-hint role changed for '%s'."),
            Spec.Name);
        return false;
    }
    OutError.Reset();
    return true;
}

UAssetImportTask* MakeImportTask()
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
    Options->StaticMeshImportData->bGenerateLightmapUVs = true;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod =
        EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = true;
    Options->StaticMeshImportData->bRemoveDegenerates = true;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = ObjSourcePath();
    Task->DestinationPath = AssetRoot;
    Task->DestinationName = MeshName;
    Task->bReplaceExisting = false;
    Task->bReplaceExistingSettings = false;
    Task->bAutomated = true;
    Task->bSave = false;
    Task->bAsync = false;
    Task->Factory = Factory;
    Task->Options = Options;
    return Task;
}

bool ValidateImportTask(const UAssetImportTask* Task, FString& OutError)
{
    const UFbxImportUI* Options =
        Task ? Cast<UFbxImportUI>(Task->Options) : nullptr;
    const UFbxStaticMeshImportData* Data =
        Options ? Options->StaticMeshImportData : nullptr;
    if (!Task || !Options || !Data ||
        !FPaths::IsSamePath(Task->Filename, ObjSourcePath()) ||
        Task->DestinationPath != AssetRoot ||
        Task->DestinationName != MeshName || Options->bImportAsSkeletal ||
        Options->MeshTypeToImport != FBXIT_StaticMesh ||
        Options->bAutomatedImportShouldDetectType || !Options->bImportMesh ||
        Options->bImportMaterials || Options->bImportTextures ||
        Data->bConvertScene || Data->bConvertSceneUnit ||
        Data->bForceFrontXAxis ||
        !FMath::IsNearlyEqual(Data->ImportUniformScale, 1.0f) ||
        !Data->bCombineMeshes || !Data->bReorderMaterialToFbxOrder ||
        Data->bImportMeshLODs || !Data->bTransformVertexToAbsolute ||
        Data->bBakePivotInVertex || Data->bAutoGenerateCollision ||
        !Data->bGenerateLightmapUVs ||
        Data->NormalImportMethod !=
            EFBXNormalImportMethod::FBXNIM_ImportNormals ||
        Data->NormalGenerationMethod !=
            EFBXNormalGenerationMethod::MikkTSpace ||
        !Data->bBuildNanite || !Data->bRemoveDegenerates ||
        Task->bReplaceExisting || Task->bReplaceExistingSettings ||
        !Task->bAutomated || Task->bSave || Task->bAsync)
    {
        OutError = TEXT("The exact V5C surroundings identity OBJ import policy changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 TriangleCount(const UStaticMesh* Mesh)
{
    const FStaticMeshRenderData* Data = Mesh ? Mesh->GetRenderData() : nullptr;
    return Data && Data->LODResources.Num() == 1
        ? Data->LODResources[0].GetNumTriangles()
        : INDEX_NONE;
}

void MakeRenderOnly(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return;
    }
    Mesh->Modify();
    Mesh->NaniteSettings.bEnabled = true;
    Mesh->NaniteSettings.KeepPercentTriangles = 1.0f;
    Mesh->NaniteSettings.TrimRelativeError = 0.0f;
    Mesh->NaniteSettings.FallbackTarget =
        ENaniteFallbackTarget::PercentTriangles;
    Mesh->NaniteSettings.FallbackPercentTriangles = 1.0f;
    Mesh->NaniteSettings.FallbackRelativeError = 0.0f;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    // PostEditChange starts a Nanite/static-mesh rebuild. The semantic
    // section census below must inspect the completed render data, never the
    // transient pre-build snapshot.
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
}

bool NormalizeAndBindMaterials(
    UStaticMesh* Mesh,
    const TArray<UMaterialInstanceConstant*>& Materials,
    FString& OutError)
{
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Materials.Num() != ExpectedMaterialCount ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("V5C surroundings material normalization requires the exact four-slot/four-section imported mesh.");
        return false;
    }

    TMap<FName, int32> CanonicalOrder;
    for (int32 CanonicalIndex = 0;
         CanonicalIndex < ExpectedMaterialCount;
         ++CanonicalIndex)
    {
        const FName Name(MaterialSpecs[CanonicalIndex].Name);
        if (!Materials[CanonicalIndex] || CanonicalOrder.Contains(Name))
        {
            OutError = TEXT("V5C surroundings canonical material roster is incomplete or ambiguous.");
            return false;
        }
        CanonicalOrder.Add(Name, CanonicalIndex);
    }

    const TArray<FStaticMaterial> ImportedMaterials =
        Mesh->GetStaticMaterials();
    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(ExpectedMaterialCount);
    TArray<int32> ImportedToCanonical;
    ImportedToCanonical.Init(INDEX_NONE, ExpectedMaterialCount);
    TArray<bool> SeenCanonical;
    SeenCanonical.Init(false, ExpectedMaterialCount);
    // The 2,678 OBJ usemtl transitions intentionally do not pin an unobserved
    // UE 5.5 imported-slot permutation. Accept any exact four-name
    // permutation, then normalize it; section consolidation and the semantic
    // triangle census remain hard gates.
    for (int32 ImportedIndex = 0;
         ImportedIndex < ExpectedMaterialCount;
         ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        const int32* CanonicalIndex =
            CanonicalOrder.Find(Imported.MaterialSlotName);
        if (!CanonicalIndex || Imported.MaterialSlotName.IsNone() ||
            Imported.MaterialSlotName != Imported.ImportedMaterialSlotName ||
            SeenCanonical[*CanonicalIndex])
        {
            OutError = FString::Printf(
                TEXT("V5C surroundings OBJ material slots are not an exact unique canonical permutation at imported index %d: slot='%s' imported='%s'."),
                ImportedIndex,
                *Imported.MaterialSlotName.ToString(),
                *Imported.ImportedMaterialSlotName.ToString());
            return false;
        }
        SeenCanonical[*CanonicalIndex] = true;
        ImportedToCanonical[ImportedIndex] = *CanonicalIndex;
        OrderedMaterials[*CanonicalIndex] = Imported;
        OrderedMaterials[*CanonicalIndex].MaterialSlotName =
            FName(MaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].ImportedMaterialSlotName =
            FName(MaterialSpecs[*CanonicalIndex].Name);
        OrderedMaterials[*CanonicalIndex].MaterialInterface =
            Materials[*CanonicalIndex];
    }

    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    TArray<FString> ImportedSectionDigest;
    ImportedSectionDigest.Reserve(Lod.Sections.Num());
    for (int32 SectionIndex = 0;
         SectionIndex < Lod.Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& ImportedSection = Lod.Sections[SectionIndex];
        const FString ImportedName = ImportedMaterials.IsValidIndex(
                ImportedSection.MaterialIndex)
            ? ImportedMaterials[ImportedSection.MaterialIndex]
                  .MaterialSlotName.ToString()
            : TEXT("INVALID");
        ImportedSectionDigest.Add(FString::Printf(
            TEXT("%d:%d:%s:%u"),
            SectionIndex,
            ImportedSection.MaterialIndex,
            *ImportedName,
            ImportedSection.NumTriangles));
    }
    TSet<int32> SeenSectionMaterials;
    for (int32 SectionIndex = 0;
         SectionIndex < Lod.Sections.Num();
         ++SectionIndex)
    {
        const FStaticMeshSection& RenderSection = Lod.Sections[SectionIndex];
        const int32 ImportedIndex = RenderSection.MaterialIndex;
        FMeshSectionInfo Section =
            Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToCanonical.IsValidIndex(ImportedIndex) ||
            ImportedToCanonical[ImportedIndex] == INDEX_NONE ||
            Section.MaterialIndex != ImportedIndex ||
            Original.MaterialIndex != ImportedIndex)
        {
            OutError = FString::Printf(
                TEXT("V5C surroundings section %d references an invalid imported material index."),
                SectionIndex);
            return false;
        }
        const int32 CanonicalIndex = ImportedToCanonical[ImportedIndex];
        if (SeenSectionMaterials.Contains(CanonicalIndex) ||
            RenderSection.NumTriangles !=
                MaterialSpecs[CanonicalIndex].Triangles)
        {
            OutError = FString::Printf(
                TEXT("V5C surroundings section %d semantic triangle count changed: material='%s' expected=%d actual=%u importedSections=index:materialIndex:name:triangles=[%s]."),
                SectionIndex,
                MaterialSpecs[CanonicalIndex].Name,
                MaterialSpecs[CanonicalIndex].Triangles,
                RenderSection.NumTriangles,
                *FString::Join(ImportedSectionDigest, TEXT(",")));
            return false;
        }
        SeenSectionMaterials.Add(CanonicalIndex);
        Section.MaterialIndex = CanonicalIndex;
        Original.MaterialIndex = CanonicalIndex;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    if (SeenSectionMaterials.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("V5C surroundings imported mesh omitted one or more canonical material sections.");
        return false;
    }

    Mesh->Modify();
    Mesh->SetStaticMaterials(OrderedMaterials);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    OutError.Reset();
    return true;
}

bool ValidateMesh(
    UStaticMesh* Mesh,
    bool bRequireMaterials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const UAssetImportData* ImportData =
        Mesh ? Mesh->GetAssetImportData() : nullptr;
    const TArray<FString> Sources =
        ImportData ? ImportData->ExtractFilenames() : TArray<FString>();
    FString ActualSource = Sources.Num() == 1
        ? FPaths::ConvertRelativePathToFull(Sources[0])
        : FString();
    FString ExpectedSource = ObjSourcePath();
    FPaths::NormalizeFilename(ActualSource);
    FPaths::NormalizeFilename(ExpectedSource);
    const FStaticMeshRenderData* RenderData =
        Mesh ? Mesh->GetRenderData() : nullptr;
    if (!Mesh || Mesh->GetPathName() != MeshObjectPath ||
        Sources.Num() != 1 ||
        !FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        Mesh->GetNumSourceModels() != 1 ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        TriangleCount(Mesh) != ExpectedTriangleCount ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount ||
        !Mesh->NaniteSettings.bEnabled ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget !=
            ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(
            Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(
            Mesh->NaniteSettings.FallbackRelativeError))
    {
        OutError = TEXT("V5C surroundings mesh lost exact source, scale, 35,424-triangle, one-LOD, four-slot or full-fidelity Nanite/fallback state.");
        return false;
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    if (Body && (Body->AggGeom.GetElementCount() != 0 ||
                 Body->CollisionTraceFlag == CTF_UseComplexAsSimple))
    {
        OutError = TEXT("V5C surroundings mesh must have no simple or complex-as-simple collision.");
        return false;
    }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector BoundsMin = Bounds.Origin - Bounds.BoxExtent;
    const FVector BoundsMax = Bounds.Origin + Bounds.BoxExtent;
    if (!BoundsMin.Equals(
            FVector(-98438.3058, -99831.9761, -233.2826), 1.0) ||
        !BoundsMax.Equals(
            FVector(99880.8394, 99339.8856, 11604.1346), 1.0))
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings imported centimetre bounds drifted: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            BoundsMin.X,
            BoundsMin.Y,
            BoundsMin.Z,
            BoundsMax.X,
            BoundsMax.Y,
            BoundsMax.Z);
        return false;
    }

    TSet<int32> SeenSections;
    TArray<FString> SectionDigest;
    bool bSectionCensusExact = true;
    const auto& Sections = RenderData->LODResources[0].Sections;
    SectionDigest.Reserve(Sections.Num());
    for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
    {
        const FStaticMeshSection& Section = Sections[SectionIndex];
        SectionDigest.Add(FString::Printf(
            TEXT("%d:%d:%u"),
            SectionIndex,
            Section.MaterialIndex,
            Section.NumTriangles));
        if (Section.MaterialIndex < 0 ||
            Section.MaterialIndex >= ExpectedMaterialCount ||
            SeenSections.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                MaterialSpecs[Section.MaterialIndex].Triangles)
        {
            bSectionCensusExact = false;
        }
        if (Section.MaterialIndex >= 0 &&
            Section.MaterialIndex < ExpectedMaterialCount)
        {
            SeenSections.Add(Section.MaterialIndex);
        }
    }
    if (!bSectionCensusExact || SeenSections.Num() != ExpectedMaterialCount)
    {
        OutError = FString::Printf(
            TEXT("V5C surroundings material-section triangle census drifted: section:material:triangles=[%s]."),
            *FString::Join(SectionDigest, TEXT(",")));
        return false;
    }
    for (int32 Slot = 0; Slot < ExpectedMaterialCount; ++Slot)
    {
        const FStaticMaterial& StaticMaterial =
            Mesh->GetStaticMaterials()[Slot];
        if (StaticMaterial.MaterialSlotName !=
                FName(MaterialSpecs[Slot].Name) ||
            StaticMaterial.ImportedMaterialSlotName !=
                FName(MaterialSpecs[Slot].Name) ||
            (bRequireMaterials &&
             (!Mesh->GetMaterial(Slot) ||
              Mesh->GetMaterial(Slot)->GetPathName() !=
                  OrderedMaterialPaths()[Slot])))
        {
            OutError = FString::Printf(
                TEXT("V5C surroundings imported material slot/order/binding drifted at %d."),
                Slot);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateCompiledMaster(UMaterial* Material, FString& OutError)
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
    const TArray<FString> Errors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Material || !Resource ||
        Material->IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5) ||
        !Resource->IsCompilationFinished() || Errors.Num() != 0)
    {
        OutError = FString::Printf(
            TEXT("V5C context-massing master has no valid compiled SM5 resource: [%s]."),
            *FString::Join(Errors, TEXT(" | ")));
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
    const TArray<FString> Errors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Instance || !Resource || Instance->IsCompiling() ||
        !Instance->IsComplete() || !Resource->IsCompilationFinished() ||
        !Resource->GetGameThreadShaderMap() ||
        !Resource->IsGameThreadShaderMapComplete() || Errors.Num() != 0)
    {
        OutError = FString::Printf(
            TEXT("V5C context-massing instance '%s' has no complete SM5 shader map: [%s]."),
            Instance ? *Instance->GetPathName() : TEXT("<null>"),
            *FString::Join(Errors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    if (!ValidateAllSourceHashes(OutReport) ||
        !ValidateExactRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UMaterial* Master = LoadExact<UMaterial>(MassingMasterObjectPath);
    if (!ValidateMassingMaster(Master, OutReport) ||
        !ValidateCompiledMaster(Master, OutReport))
    {
        return false;
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance =
            LoadExact<UMaterialInstanceConstant>(
                ObjectPath(MaterialRoot, Spec.Name));
        if (!ValidateMaterialInstance(Instance, Spec, OutReport) ||
            !ValidateCompiledInstance(Instance, OutReport))
        {
            return false;
        }
    }
    if (!ValidateMesh(
            LoadExact<UStaticMesh>(MeshObjectPath), true, OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5C_SURROUNDINGS_ASSETS_VALID assets=6 buildings=1338 official=449 osmFallback=889 meshTriangles=35424 materialSlots=4 holes=16 radiusMeters=304.025284..999.767652 terrainGroundedPolygonParts=1339 syntheticGradeMeters=-1.332826..1.774387 foundationSkirtMeters=1.0 localFacadeUvMetres=true objBytes=%lld objSha256=%s manifestSha256=%s contractSha256=%s proceduralTextureFreeMassing=true nonAuthoritativeFramedInsetGlazingCue=true screenAdaptiveMicroFacadeFwidth=0.30..0.75 macroFacadeGroups=4x4 macroTone=0.90..1.06 proceduralGlassRoughness=0.28 deterministicCellVariation=true plinthCue=true apertureFadeCm=35000..70000 atmosphereEndCm=110000 distanceMutedAtmosphereApproximation=true importedSlotPermutationNormalizedByName=true renderOnly=true materialOnlyNoGeometryChange=true noSurveyGradeFacadeOrRfAuthority=true."),
        ExpectedObjBytes,
        *ExpectedObjSha256,
        *ExpectedManifestSha256,
        *ExpectedContractSha256);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5CSurroundingsAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }
const FString& GetSurroundingsMeshObjectPath() { return MeshObjectPath; }
const TArray<FString>& GetOrderedMaterialObjectPaths()
{
    return OrderedMaterialPaths();
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    if (!ValidateAllSourceHashes(OutError))
    {
        return false;
    }
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing, OutError) || !Existing.IsEmpty())
    {
        OutError = TEXT("CreateFreshAssets requires an empty exact V5C surroundings asset root.");
        return false;
    }

    FScopedFreshRollback Rollback(OutAssets, OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterial* Master = CreateMassingMaster(AssetTools, OutError);
    if (!Master)
    {
        return false;
    }
    OutAssets.Add(Master);
    TArray<UMaterialInstanceConstant*> Materials;
    Materials.Reserve(ExpectedMaterialCount);
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Material =
            CreateMaterialInstance(AssetTools, Spec, Master, OutError);
        if (!Material)
        {
            return false;
        }
        Materials.Add(Material);
        OutAssets.Add(Material);
    }

    UAssetImportTask* Task = MakeImportTask();
    if (!Task || !ValidateImportTask(Task, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not allocate the exact V5C surroundings OBJ import task.");
        }
        return false;
    }
    AssetTools.ImportAssetTasks({Task});
    UStaticMesh* Mesh = nullptr;
    for (UObject* Object : Task->GetObjects())
    {
        if (Object && Object->GetPathName() == MeshObjectPath)
        {
            Mesh = Cast<UStaticMesh>(Object);
        }
    }
    if (!Mesh)
    {
        Mesh = LoadExact<UStaticMesh>(MeshObjectPath);
    }
    MakeRenderOnly(Mesh);
    if (!NormalizeAndBindMaterials(Mesh, Materials, OutError) ||
        !ValidateMesh(Mesh, true, OutError))
    {
        return false;
    }
    OutAssets.Add(Mesh);
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString Validation;
    if (OutAssets.Num() != ExpectedAssetCount ||
        !ValidateInternal(false, Validation))
    {
        OutError = TEXT("Fresh V5C surroundings validation failed before save: ") +
            Validation;
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
} // namespace TRIADIstanaExploreV5CSurroundingsAssetFactory
