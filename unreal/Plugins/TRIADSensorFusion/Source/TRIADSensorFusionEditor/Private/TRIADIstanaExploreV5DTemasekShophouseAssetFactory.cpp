#include "TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h"

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
#include "IAssetTools.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "Ssl.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshOperations.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5DTemasekShophouseActor.h"
#include "TRIADIstanaExploreV5DTemasekShophouseProvenance.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/"
         "R24TemasekShophouse"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MeshName(TEXT("SM_IPV5D_R24_TemasekShophouse_Render"));
const FString MeshObjectPath(
    AssetRoot + TEXT("/") + MeshName + TEXT(".") + MeshName);
const FString ObjSourceProjectRelativePath(
    TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
         "R24TemasekShophouse/Generated/"
         "SM_IPV5D_R24_TemasekShophouse_Render.obj"));
const FName AssetCreateContext(
    TEXT("TRIAD.CreateIstanaExploreV5DR24TemasekShophouseAssets"));

constexpr int32 ExpectedAssetCount = 16;
constexpr int32 ExpectedSourceVertexCount = 9536;
constexpr int32 ExpectedTriangleCount = 15760;
constexpr int32 ExpectedMaterialCount = 15;
constexpr int32 ExpectedComponentCount = 828;

// CreateFreshAssets admits one exact empty predecessor. The synchronous editor
// library must either commit after save/reload/cold validation or roll back the
// exact root diff; no pre-existing package can enter this ownership set.
bool bFreshAssetTransactionActive = false;
bool bFreshAssetTransactionReleasedToCaller = false;
TSet<FString> FreshAssetTransactionOwnedObjectPaths;

const FString BrickBaseCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01;\n")
    TEXT("float3 an = abs(normalize(WorldNormal));\n")
    TEXT("float2 uv = an.z > max(an.x, an.y) ? p.xy : (an.x > an.y ? p.yz : p.xz);\n")
    TEXT("float row = floor(uv.y / 0.075);\n")
    TEXT("float2 q = frac(float2((uv.x + fmod(abs(row), 2.0) * 0.12) / 0.24, uv.y / 0.075));\n")
    TEXT("float mortar = saturate(step(q.x, 0.028) + step(0.972, q.x) + step(q.y, 0.075) + step(0.925, q.y));\n")
    TEXT("float grain = frac(sin(dot(floor(p * 95.0), float3(12.9898, 78.233, 37.719))) * 43758.5453);\n")
    TEXT("float3 brick = float3(0.34, 0.058, 0.036) * (0.82 + 0.28 * grain);\n")
    TEXT("return lerp(brick, float3(0.19, 0.16, 0.13), mortar);"));
const FString BrickRoughnessCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01;\n")
    TEXT("float grain = frac(sin(dot(floor(p * 80.0), float3(17.17, 43.31, 91.73))) * 24634.6345);\n")
    TEXT("return saturate(0.72 + 0.18 * grain);"));
const FString BrickNormalCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01;\n")
    TEXT("float3 n = normalize(WorldNormal);\n")
    TEXT("float3 axis = abs(n.z) < 0.92 ? float3(0,0,1) : float3(0,1,0);\n")
    TEXT("float3 t = normalize(cross(axis, n)); float3 b = cross(n, t);\n")
    TEXT("float a = sin(dot(p, float3(73.1, 31.7, 19.3))); float c = sin(dot(p, float3(41.9, 89.7, 11.3)));\n")
    TEXT("return normalize(n + (t * a + b * c) * 0.055);"));

const FString PaintBaseCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01;\n")
    TEXT("float variation = 0.965 + 0.035 * sin(dot(p, float3(1.7, 2.3, 3.1)));\n")
    TEXT("return float3(0.78, 0.77, 0.70) * variation;"));
const FString PaintRoughnessCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; return 0.42 + 0.035 * (0.5 + 0.5 * sin(dot(p, float3(5.1, 3.7, 2.9))));"));
const FString PaintNormalCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; float3 n = normalize(WorldNormal); float3 axis = abs(n.z) < 0.92 ? float3(0,0,1) : float3(0,1,0); float3 t = normalize(cross(axis,n)); float3 b = cross(n,t); return normalize(n + (t*sin(dot(p,float3(19.1,13.7,7.3))) + b*sin(dot(p,float3(11.9,23.3,5.7))))*0.008);"));

const FString GlassBaseCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; float vertical = saturate(frac(p.z * 0.07)); float dust = 0.5 + 0.5 * sin(dot(p,float3(0.7,1.1,2.3))); return float3(0.018,0.038,0.052) * (0.82 + 0.12*vertical + 0.06*dust);"));
const FString GlassRoughnessCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; return 0.105 + 0.035 * (0.5 + 0.5 * sin(dot(p,float3(1.3,0.9,2.1))));"));
const FString GlassNormalCode(
    TEXT("return normalize(WorldNormal);"));

const FString RoofBaseCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; float fired = 0.5 + 0.5*sin(p.x*5.7 + p.y*3.1); float glaze = 0.5 + 0.5*sin(dot(p,float3(13.0,7.0,2.0))); return lerp(float3(0.028,0.115,0.055),float3(0.055,0.205,0.095),0.35*fired+0.20*glaze);"));
const FString RoofRoughnessCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; return 0.19 + 0.10 * (0.5 + 0.5*sin(dot(p,float3(3.1,5.3,1.7))));"));
const FString RoofNormalCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; float3 n=normalize(WorldNormal); float3 axis=abs(n.z)<0.92?float3(0,0,1):float3(0,1,0); float3 t=normalize(cross(axis,n)); float3 b=cross(n,t); return normalize(n+(t*sin(p.x*18.0)+b*sin(p.y*21.0))*0.018);"));

const FString MarbleBaseCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; float vein = pow(saturate(0.5 + 0.5*sin(p.z*3.7 + sin(p.x*1.9+p.y*2.3)*2.1)), 10.0); float cloud=0.5+0.5*sin(dot(p,float3(0.6,0.8,1.1))); return lerp(float3(0.67,0.63,0.55)*(0.92+0.08*cloud),float3(0.28,0.30,0.29),0.34*vein);"));
const FString MarbleRoughnessCode(
    TEXT("float3 p = AbsoluteWorldPosition * 0.01; return 0.27 + 0.08*(0.5+0.5*sin(dot(p,float3(1.7,2.1,0.9))));"));
const FString MarbleNormalCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; float3 n=normalize(WorldNormal); float3 axis=abs(n.z)<0.92?float3(0,0,1):float3(0,1,0); float3 t=normalize(cross(axis,n)); return normalize(n+t*sin(p.z*7.4+sin(p.x*2.1))*0.012);"));

const FString ConcreteBaseCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; float fine=frac(sin(dot(floor(p*55.0),float3(12.9898,78.233,45.164)))*43758.5453); float broad=0.5+0.5*sin(dot(p,float3(0.8,1.3,0.5))); return float3(0.49,0.47,0.42)*(0.90+0.12*fine+0.05*broad);"));
const FString ConcreteRoughnessCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; return 0.68+0.12*frac(sin(dot(floor(p*41.0),float3(31.7,17.3,53.1)))*24634.6345);"));
const FString ConcreteNormalCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; float3 n=normalize(WorldNormal); float3 axis=abs(n.z)<0.92?float3(0,0,1):float3(0,1,0); float3 t=normalize(cross(axis,n)); float3 b=cross(n,t); return normalize(n+(t*sin(dot(p,float3(37,19,11)))+b*sin(dot(p,float3(17,43,23))))*0.026);"));

const FString MetalBaseCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; float patina=0.5+0.5*sin(dot(p,float3(2.3,1.1,3.7))); return lerp(float3(0.012,0.015,0.018),float3(0.028,0.033,0.038),patina*0.35);"));
const FString MetalRoughnessCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; return 0.24+0.10*(0.5+0.5*sin(dot(p,float3(5.3,1.9,2.7))));"));
const FString MetalNormalCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; float3 n=normalize(WorldNormal); float3 axis=abs(n.z)<0.92?float3(0,0,1):float3(0,1,0); float3 t=normalize(cross(axis,n)); return normalize(n+t*sin(p.z*29.0+p.x*7.0)*0.009);"));

const FString PlaqueBaseCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; float age=0.5+0.5*sin(dot(p,float3(7.1,3.3,5.7))); return lerp(float3(0.17,0.09,0.025),float3(0.34,0.20,0.055),0.55+0.25*age);"));
const FString PlaqueRoughnessCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; return 0.29+0.11*(0.5+0.5*sin(dot(p,float3(3.3,5.9,2.1))));"));
const FString PlaqueNormalCode(
    TEXT("float3 p=AbsoluteWorldPosition*0.01; float3 n=normalize(WorldNormal); float3 axis=abs(n.z)<0.92?float3(0,0,1):float3(0,1,0); float3 t=normalize(cross(axis,n)); return normalize(n+t*sin(dot(p,float3(17,11,23)))*0.006);"));

const FString RecessBaseCode(TEXT("return float3(0.006,0.009,0.012);"));
const FString RecessRoughnessCode(TEXT("return 0.91;"));
const FString RecessNormalCode(TEXT("return normalize(WorldNormal);"));

// Texture-free, deterministic public-exterior appearance priors.  These are
// deliberately uncalibrated render materials, never RF/material truth.
const FString TShWhiteBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float v=0.965+0.035*sin(dot(p,float3(1.7,2.3,3.1))); return float3(0.79,0.775,0.72)*v;"));
const FString TShWhiteRough(TEXT("float3 p=AbsoluteWorldPosition*0.01; return 0.69+0.06*(0.5+0.5*sin(dot(p,float3(7.1,3.7,5.3))));"));
const FString TShCharcoalBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float v=0.92+0.08*frac(sin(dot(floor(p*18.0),float3(12.9,78.2,37.7)))*43758.5); return float3(0.03,0.034,0.037)*v;"));
const FString TShCharcoalRough(TEXT("return 0.58;"));
const FString TShGlassBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float v=0.88+0.12*(0.5+0.5*sin(p.z*2.1)); return float3(0.025,0.05,0.057)*v;"));
const FString TShGlassRough(TEXT("return 0.12;"));
const FString TShTimberBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float g=0.82+0.18*(0.5+0.5*sin(p.z*13.0+sin(p.x*2.1))); return float3(0.19,0.105,0.055)*g;"));
const FString TShTimberRough(TEXT("return 0.52;"));
const FString TShBrassBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float v=0.9+0.1*(0.5+0.5*sin(dot(p,float3(5.1,7.3,2.7)))); return float3(0.43,0.28,0.08)*v;"));
const FString TShBrassRough(TEXT("return 0.24;"));
const FString TShMosaicBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float2 q=floor(p.xy*8.0); float alt=fmod(abs(q.x+q.y),2.0); return lerp(float3(0.49,0.35,0.36),float3(0.36,0.37,0.39),alt);"));
const FString TShMosaicRough(TEXT("return 0.66;"));
const FString TShConcreteBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float n=frac(sin(dot(floor(p*45.0),float3(12.9,78.2,37.7)))*43758.5); return float3(0.58,0.565,0.525)*(0.9+0.15*n);"));
const FString TShConcreteRough(TEXT("return 0.82;"));
const FString TShTerracottaBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float n=frac(sin(dot(floor(p*30.0),float3(31.7,19.3,71.1)))*24634.6); return float3(0.445,0.15,0.078)*(0.85+0.22*n);"));
const FString TShTerracottaRough(TEXT("return 0.78;"));
const FString TShSolarBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float grid=step(0.94,frac(p.x*12.0))+step(0.94,frac(p.y*12.0)); return lerp(float3(0.018,0.07,0.12),float3(0.12,0.15,0.17),saturate(grid));"));
const FString TShSolarRough(TEXT("return 0.18;"));
const FString TShPaverLightBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float n=frac(sin(dot(floor(p*22.0),float3(13.1,47.7,91.3)))*17371.3); return float3(0.5,0.495,0.455)*(0.88+0.18*n);"));
const FString TShPaverLightRough(TEXT("return 0.86;"));
const FString TShPaverDarkBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float n=frac(sin(dot(floor(p*24.0),float3(53.1,17.7,29.3)))*35711.7); return float3(0.21,0.215,0.205)*(0.86+0.2*n);"));
const FString TShPaverDarkRough(TEXT("return 0.9;"));
const FString TShReusedTimberBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float g=0.78+0.22*(0.5+0.5*sin(p.x*11.0+sin(p.z*1.7))); return float3(0.315,0.17,0.075)*g;"));
const FString TShReusedTimberRough(TEXT("return 0.68;"));
const FString TShSoilBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float n=frac(sin(dot(floor(p*35.0),float3(23.1,61.7,11.9)))*21717.7); return float3(0.095,0.06,0.032)*(0.75+0.3*n);"));
const FString TShSoilRough(TEXT("return 0.97;"));
const FString TShWaterBase(TEXT("float3 p=AbsoluteWorldPosition*0.01; float r=0.5+0.5*sin(p.x*3.1+p.y*4.7); return float3(0.03,0.16,0.205)*(0.85+0.12*r);"));
const FString TShWaterRough(TEXT("return 0.19;"));
const FString TShShadowBase(TEXT("return float3(0.01,0.014,0.015);"));
const FString TShShadowRough(TEXT("return 0.96;"));

struct FMaterialSpec
{
    const TCHAR* Slot;
    const TCHAR* Asset;
    int32 Triangles;
    const FString* BaseCode;
    const FString* RoughnessCode;
    const FString* NormalCode;
    float Specular;
    float Metallic;
};

// Canonical lexical slot order; the static-mesh sections are remapped to it.
const FMaterialSpec MaterialSpecs[] = {
    {TEXT("M_TSH_Brass"), TEXT("M_TSH_Brass_PBR_R24"), 396, &TShBrassBase, &TShBrassRough, &MetalNormalCode, 0.55f, 0.78f},
    {TEXT("M_TSH_CharcoalTrim"), TEXT("M_TSH_CharcoalTrim_PBR_R24"), 3492, &TShCharcoalBase, &TShCharcoalRough, &PaintNormalCode, 0.3f, 0.0f},
    {TEXT("M_TSH_DarkWindowGlass"), TEXT("M_TSH_DarkWindowGlass_PBR_R24"), 612, &TShGlassBase, &TShGlassRough, &GlassNormalCode, 0.72f, 0.0f},
    {TEXT("M_TSH_PaverDark"), TEXT("M_TSH_PaverDark_PBR_R24"), 468, &TShPaverDarkBase, &TShPaverDarkRough, &ConcreteNormalCode, 0.18f, 0.0f},
    {TEXT("M_TSH_PaverLight"), TEXT("M_TSH_PaverLight_PBR_R24"), 480, &TShPaverLightBase, &TShPaverLightRough, &ConcreteNormalCode, 0.18f, 0.0f},
    {TEXT("M_TSH_PinkGreyMosaic"), TEXT("M_TSH_PinkGreyMosaic_PBR_R24"), 192, &TShMosaicBase, &TShMosaicRough, &MarbleNormalCode, 0.32f, 0.0f},
    {TEXT("M_TSH_PrecastConcrete"), TEXT("M_TSH_PrecastConcrete_PBR_R24"), 900, &TShConcreteBase, &TShConcreteRough, &ConcreteNormalCode, 0.2f, 0.0f},
    {TEXT("M_TSH_Rainwater"), TEXT("M_TSH_Rainwater_PBR_R24"), 36, &TShWaterBase, &TShWaterRough, &GlassNormalCode, 0.6f, 0.0f},
    {TEXT("M_TSH_ReusedTimber"), TEXT("M_TSH_ReusedTimber_PBR_R24"), 192, &TShReusedTimberBase, &TShReusedTimberRough, &PlaqueNormalCode, 0.25f, 0.0f},
    {TEXT("M_TSH_ShadowRecess"), TEXT("M_TSH_ShadowRecess_PBR_R24"), 468, &TShShadowBase, &TShShadowRough, &RecessNormalCode, 0.05f, 0.0f},
    {TEXT("M_TSH_SoilMulch"), TEXT("M_TSH_SoilMulch_PBR_R24"), 72, &TShSoilBase, &TShSoilRough, &ConcreteNormalCode, 0.12f, 0.0f},
    {TEXT("M_TSH_SolarPanel"), TEXT("M_TSH_SolarPanel_PBR_R24"), 216, &TShSolarBase, &TShSolarRough, &GlassNormalCode, 0.6f, 0.22f},
    {TEXT("M_TSH_Terracotta"), TEXT("M_TSH_Terracotta_PBR_R24"), 168, &TShTerracottaBase, &TShTerracottaRough, &BrickNormalCode, 0.18f, 0.0f},
    {TEXT("M_TSH_Timber"), TEXT("M_TSH_Timber_PBR_R24"), 2796, &TShTimberBase, &TShTimberRough, &PlaqueNormalCode, 0.25f, 0.0f},
    {TEXT("M_TSH_WhiteShanghaiPlaster"), TEXT("M_TSH_WhiteShanghaiPlaster_PBR_R24"), 5272, &TShWhiteBase, &TShWhiteRough, &PaintNormalCode, 0.3f, 0.0f},
};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == ExpectedMaterialCount);

struct FSourceSpec
{
    const TCHAR* RelativePath;
    int64 Bytes;
    const TCHAR* Sha256;
};

const FSourceSpec SourceSpecs[] = {
    {TEXT("Generated/SM_IPV5D_R24_TemasekShophouse_Render.obj"), 610314, TEXT("F5ED94C312A38A542D9C078F19F16FAF04CEA6ECAE5B4C2CF0AA7F3D651B9007")},
    {TEXT("Generated/SM_IPV5D_R24_TemasekShophouse_Render.mtl"), 2355, TEXT("41D50D10924A2AA0FECAB2ADC48E7BC071A20ED8A9F2D109BE81E0D2EF2CE4CB")},
    {TEXT("Generated/IstanaPublicViewV5DR24TemasekShophouse.geometry.json"), 2149277, TEXT("CC40E8C2C975D2A19124D74EAAC80B6C14C7121AD6BDDD616A740620737B05A7")},
    {TEXT("Generated/IstanaPublicViewV5DR24TemasekShophouse.features.json"), 7376, TEXT("900BED02B0C5CBFD8D2668C017B33F34B800B354F251B57767DE243F8126E4E0")},
    {TEXT("Generated/IstanaPublicViewV5DR24TemasekShophouse.materials.json"), 6610, TEXT("945404D4E6EC0E873FCDCBDA13977A67747961A3ADAAD413D4C1D20690EBE3FF")},
    {TEXT("Generated/IstanaPublicViewV5DR24TemasekShophouse.manifest.json"), 9131, TEXT("ED46D1B6367859A42BF1404FE1C0F5E457C16276777818B510FD8A8CA3BE8D87")},
    {TEXT("temasek_shophouse_r24.contract.json"), 10720, TEXT("F304EE5D090D41F2331F293E6CD797A9B27D5B3B74DE44DCAA6A3CC2A66FE082")},
    {TEXT("Sources/public_sources.json"), 5870, TEXT("E4D1B0F3648B87E577A004F158821EBD716E0288511C51A468E272D475DE009E")},
    {TEXT("Generated/temasek_shophouse_r24.unreal_placement.json"), 4517, TEXT("6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5")},
};

FString SourceRoot()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
             "R24TemasekShophouse")));
}

FString SourcePath(const TCHAR* RelativePath)
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        SourceRoot(), RelativePath));
}

FString ObjSourcePath()
{
    return SourcePath(SourceSpecs[0].RelativePath);
}

bool CanonicalProjectRelativeSourcePath(
    const FString& Source,
    FString& OutProjectRelative)
{
    OutProjectRelative.Reset();
    if (Source.IsEmpty())
    {
        return false;
    }
    FString ProjectRoot = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ProjectRoot);
    FString Absolute = FPaths::IsRelative(Source)
        ? FPaths::ConvertRelativePathToFull(
              FPaths::Combine(ProjectRoot, Source))
        : FPaths::ConvertRelativePathToFull(Source);
    FPaths::NormalizeFilename(Absolute);
    OutProjectRelative = Absolute;
    const FString ProjectRelativeAnchor = FPaths::Combine(
        ProjectRoot,
        TEXT("__TRIAD_PROJECT_RELATIVE_ANCHOR__"));
    if (!FPaths::MakePathRelativeTo(
            OutProjectRelative,
            *ProjectRelativeAnchor))
    {
        OutProjectRelative.Reset();
        return false;
    }
    FPaths::NormalizeFilename(OutProjectRelative);
    FPaths::CollapseRelativeDirectories(OutProjectRelative);
    return !OutProjectRelative.IsEmpty() &&
        OutProjectRelative != TEXT("..") &&
        !OutProjectRelative.StartsWith(TEXT("../")) &&
        !FPaths::IsRelative(ProjectRoot);
}

FString MaterialObjectPath(const FMaterialSpec& Spec)
{
    return MaterialRoot + TEXT("/") + Spec.Asset + TEXT(".") + Spec.Asset;
}

template <typename T>
T* LoadExact(const FString& ObjectPath)
{
    T* Object = LoadObject<T>(nullptr, *ObjectPath);
    return Object && Object->GetPathName() == ObjectPath ? Object : nullptr;
}

bool ValidateSourceHash(const FSourceSpec& Spec, FString& OutError)
{
    const FString Filename = SourcePath(Spec.RelativePath);
    TArray<uint8> Bytes;
    FString Actual;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != Spec.Bytes)
    {
        OutError = FString::Printf(
            TEXT("R24B Temasek Shophouse source-byte guard failed for '%s': expected=%lld actual=%d."),
            *Filename,
            Spec.Bytes,
            Bytes.Num());
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) == nullptr)
    {
        OutError = TEXT("R24B Temasek Shophouse SHA-256 computation failed for '") + Filename + TEXT("'.");
        return false;
    }
    Actual = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("R24B Temasek Shophouse source admission requires WITH_SSL.");
    return false;
#endif
    if (Actual != Spec.Sha256)
    {
        OutError = FString::Printf(
            TEXT("R24B Temasek Shophouse SHA-256 guard failed for '%s': expected=%s actual=%s."),
            *Filename,
            Spec.Sha256,
            *Actual);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateAllSourceHashes(FString& OutError)
{
    for (const FSourceSpec& Spec : SourceSpecs)
    {
        if (!ValidateSourceHash(Spec, OutError))
        {
            return false;
        }
    }
    return true;
}

bool GatherAssets(TArray<FAssetData>& OutAssets, FString& OutError)
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
        OutError = TEXT("R24B Temasek Shophouse Asset Registry discovery did not complete.");
        return false;
    }
    OutError.Reset();
    return true;
}

TArray<FString> ExpectedObjectPaths()
{
    TArray<FString> Paths = {MeshObjectPath};
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        Paths.Add(MaterialObjectPath(Spec));
    }
    Paths.Sort();
    return Paths;
}

TArray<FString> ExpectedPackageNames()
{
    TArray<FString> Packages;
    for (const FString& ObjectPath : ExpectedObjectPaths())
    {
        Packages.AddUnique(FPackageName::ObjectPathToPackageName(ObjectPath));
    }
    Packages.Sort();
    return Packages;
}

bool GetExactOwnedPackageArtifactPaths(
    const FString& PackageName,
    TArray<FString>& OutPaths,
    FString& OutError)
{
    OutPaths.Reset();
    const TArray<FString> ExactPackages = ExpectedPackageNames();
    if (!ExactPackages.Contains(PackageName))
    {
        OutError = TEXT("R24B rollback refused a package outside the exact sixteen-package roster: ") +
            PackageName;
        return false;
    }
    FString AssetDiskRoot = FPaths::ConvertRelativePathToFull(
        FPackageName::LongPackageNameToFilename(AssetRoot));
    FString PackageStem = FPaths::ConvertRelativePathToFull(
        FPackageName::LongPackageNameToFilename(PackageName));
    FPaths::NormalizeDirectoryName(AssetDiskRoot);
    FPaths::NormalizeFilename(PackageStem);
    if (!FPaths::IsUnderDirectory(PackageStem, AssetDiskRoot))
    {
        OutError = TEXT("R24B rollback refused a package stem outside its exact physical asset root: ") +
            PackageStem;
        return false;
    }
    static const TCHAR* ExactExtensions[] = {
        TEXT(".uasset"), TEXT(".uexp"), TEXT(".ubulk"), TEXT(".uptnl")};
    static_assert(UE_ARRAY_COUNT(ExactExtensions) == 4);
    for (const TCHAR* Extension : ExactExtensions)
    {
        FString Candidate = PackageStem + Extension;
        FPaths::NormalizeFilename(Candidate);
        if (!FPaths::IsUnderDirectory(Candidate, AssetDiskRoot) ||
            FPaths::GetBaseFilename(Candidate) !=
                FPaths::GetBaseFilename(PackageStem))
        {
            OutError = TEXT("R24B rollback artifact containment proof failed: ") +
                Candidate;
            OutPaths.Reset();
            return false;
        }
        OutPaths.Add(Candidate);
    }
    OutError.Reset();
    return true;
}

bool ValidateExactEmptyAssetRoot(FString& OutError)
{
    TArray<FAssetData> Existing;
    if (!GatherAssets(Existing, OutError))
    {
        return false;
    }
    TArray<FString> ExistingPaths;
    for (const FAssetData& Asset : Existing)
    {
        ExistingPaths.Add(Asset.GetObjectPathString());
    }
    TArray<FString> ExistingPackageFiles;
    FString AssetDiskRoot = FPaths::ConvertRelativePathToFull(
        FPackageName::LongPackageNameToFilename(AssetRoot));
    FPaths::NormalizeDirectoryName(AssetDiskRoot);
    IFileManager::Get().FindFilesRecursive(
        ExistingPackageFiles,
        *AssetDiskRoot,
        TEXT("*"),
        true,
        false,
        false);
    for (FString& ExistingPackageFile : ExistingPackageFiles)
    {
        ExistingPackageFile = FPaths::ConvertRelativePathToFull(
            ExistingPackageFile);
        FPaths::NormalizeFilename(ExistingPackageFile);
    }
    if (!ExistingPaths.IsEmpty() || !ExistingPackageFiles.IsEmpty())
    {
        ExistingPaths.Sort();
        ExistingPackageFiles.Sort();
        OutError = FString::Printf(
            TEXT("R24B exact asset root predecessor is not empty: registry=[%s] packageFiles=[%s]."),
            *FString::Join(ExistingPaths, TEXT(", ")),
            *FString::Join(ExistingPackageFiles, TEXT(", ")));
        return false;
    }
    OutError.Reset();
    return true;
}

void CaptureOwnedRootDiff()
{
    if (!bFreshAssetTransactionActive)
    {
        return;
    }
    FString Ignored;
    TArray<FAssetData> Assets;
    if (GatherAssets(Assets, Ignored))
    {
        for (const FAssetData& Asset : Assets)
        {
            FreshAssetTransactionOwnedObjectPaths.Add(
                Asset.GetObjectPathString());
        }
    }
}

bool RollbackActiveFreshAssetTransactionInternal(FString& OutReport)
{
    if (!bFreshAssetTransactionActive)
    {
        OutReport = TEXT("R24B fresh-asset rollback refused: no active exact-empty-predecessor transaction.");
        return false;
    }
    CaptureOwnedRootDiff();
    const TArray<FString> ExactObjectPaths = ExpectedObjectPaths();
    const TArray<FString> ExactPackageNames = ExpectedPackageNames();
    TArray<UObject*> Disposable;
    for (const FString& ObjectPath : FreshAssetTransactionOwnedObjectPaths)
    {
        if (!ExactObjectPaths.Contains(ObjectPath) ||
            !ObjectPath.StartsWith(AssetRoot + TEXT("/")))
        {
            OutReport = TEXT("R24B fresh-asset rollback ownership escaped the exact sixteen-object roster: ") +
                ObjectPath;
            return false;
        }
        if (UObject* Object = LoadObject<UObject>(nullptr, *ObjectPath))
        {
            Disposable.AddUnique(Object);
        }
    }

    const int32 ExpectedDeletes = Disposable.Num();
    const int32 OwnedPathCount = FreshAssetTransactionOwnedObjectPaths.Num();
    const int32 Deleted = Disposable.IsEmpty()
        ? 0
        : ObjectTools::DeleteObjectsUnchecked(Disposable);

    TArray<UPackage*> LoadedOwnedPackages;
    for (const FString& ObjectPath : FreshAssetTransactionOwnedObjectPaths)
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        if (!ExactPackageNames.Contains(PackageName))
        {
            OutReport = TEXT("R24B fresh-asset rollback package ownership proof failed: ") +
                PackageName;
            return false;
        }
        if (UPackage* LoadedPackage = FindPackage(
                nullptr,
                *PackageName))
        {
            LoadedPackage->SetDirtyFlag(false);
            LoadedOwnedPackages.AddUnique(LoadedPackage);
        }
    }
    FText UnloadError;
    bool bUnloadSucceeded = true;
    if (!LoadedOwnedPackages.IsEmpty())
    {
        UPackageTools::FUnloadPackageParams UnloadParams(
            LoadedOwnedPackages);
        UnloadParams.bUnloadDirtyPackages = false;
        UnloadParams.bResetTransBuffer = true;
        bUnloadSucceeded = UPackageTools::UnloadPackages(UnloadParams);
        UnloadError = UnloadParams.OutErrorMessage;
    }
    CollectGarbage(RF_NoFlags);

    int32 DeletedResidualArtifactCount = 0;
    bool bResidualArtifactCleanupSucceeded = true;
    FString ArtifactGuardError;
    for (const FString& ObjectPath : FreshAssetTransactionOwnedObjectPaths)
    {
        const FString PackageName =
            FPackageName::ObjectPathToPackageName(ObjectPath);
        TArray<FString> ExactArtifacts;
        if (!GetExactOwnedPackageArtifactPaths(
                PackageName,
                ExactArtifacts,
                ArtifactGuardError))
        {
            bResidualArtifactCleanupSucceeded = false;
            break;
        }
        for (const FString& Artifact : ExactArtifacts)
        {
            if (IFileManager::Get().FileExists(*Artifact))
            {
                const bool bDeleted = IFileManager::Get().Delete(
                    *Artifact,
                    false,
                    true,
                    true);
                bResidualArtifactCleanupSucceeded &= bDeleted;
                DeletedResidualArtifactCount += bDeleted ? 1 : 0;
            }
        }
    }

    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));
    Registry.Get().ScanPathsSynchronous({AssetRoot}, true);
    TArray<FString> ResidualLoadedPackageNames;
    for (const FString& PackageName : ExactPackageNames)
    {
        if (FindPackage(nullptr, *PackageName))
        {
            ResidualLoadedPackageNames.Add(PackageName);
        }
    }
    ResidualLoadedPackageNames.Sort();
    const bool bNoLoadedOwnedPackages =
        ResidualLoadedPackageNames.IsEmpty();
    FString EmptyError;
    const bool bRootEmpty = ValidateExactEmptyAssetRoot(EmptyError);
    const bool bDeletedExactly = Deleted == ExpectedDeletes;
    const bool bRollbackComplete = bRootEmpty && bDeletedExactly &&
        bUnloadSucceeded && bResidualArtifactCleanupSucceeded &&
        bNoLoadedOwnedPackages;
    if (bRollbackComplete)
    {
        bFreshAssetTransactionActive = false;
        bFreshAssetTransactionReleasedToCaller = false;
        FreshAssetTransactionOwnedObjectPaths.Reset();
    }
    OutReport = FString::Printf(
        TEXT("R24_TEMASEK_FRESH_ASSET_ROLLBACK rootEmpty=%s retryable=%s ownershipSubsetOfExactRoster=true ownedObjectPaths=%d loadedDeletes=%d/%d unloadSucceeded=%s noLoadedOwnedPackages=%s residualLoadedPackages=[%s] exactResidualPackageArtifactsDeleted=%d residualCleanupSucceeded=%s unloadError={%s} artifactGuard={%s} details={%s}"),
        bRootEmpty ? TEXT("true") : TEXT("false"),
        bRollbackComplete ? TEXT("true") : TEXT("false"),
        OwnedPathCount,
        Deleted,
        ExpectedDeletes,
        bUnloadSucceeded ? TEXT("true") : TEXT("false"),
        bNoLoadedOwnedPackages ? TEXT("true") : TEXT("false"),
        *FString::Join(ResidualLoadedPackageNames, TEXT(", ")),
        DeletedResidualArtifactCount,
        bResidualArtifactCleanupSucceeded ? TEXT("true") : TEXT("false"),
        *UnloadError.ToString(),
        *ArtifactGuardError,
        *EmptyError);
    return bRollbackComplete;
}

bool ValidateExactRoster(bool bRequireSaved, FString& OutError)
{
    TArray<FAssetData> Assets;
    if (!GatherAssets(Assets, OutError))
    {
        return false;
    }
    TArray<FString> Actual;
    for (const FAssetData& Asset : Assets)
    {
        Actual.Add(Asset.GetObjectPathString());
    }
    Actual.Sort();
    if (Actual != ExpectedObjectPaths())
    {
        OutError = FString::Printf(
            TEXT("R24B Temasek Shophouse root must contain exactly one mesh and fifteen non-foliage materials; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        for (const FAssetData& Asset : Assets)
        {
            UObject* Object = Asset.GetAsset();
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (!Object || !Package ||
                !FPackageName::DoesPackageExist(Package->GetName()) ||
                Package->IsDirty())
            {
                OutError = TEXT("An exact R24B Temasek Shophouse asset is not persisted and clean: ") +
                    Asset.GetObjectPathString();
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
        CaptureOwnedRootDiff();
        FString RollbackReport;
        if (!RollbackActiveFreshAssetTransactionInternal(RollbackReport))
        {
            Error += TEXT(" R24_TEMASEK_FRESH_ASSET_ROLLBACK_INCOMPLETE: ") +
                RollbackReport;
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
T* AddExpression(UMaterial* Material, const FString& Description, int32 X, int32 Y)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    T* Expression = Data
        ? NewObject<T>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
        Data->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpressionCustom* AddCustom(
    UMaterial* Material,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    int32 X,
    int32 Y)
{
    UMaterialExpressionCustom* Custom =
        AddExpression<UMaterialExpressionCustom>(Material, Description, X, Y);
    if (!Custom)
    {
        return nullptr;
    }
    Custom->Description = Description;
    Custom->Code = Code;
    Custom->OutputType = OutputType;
    Custom->Inputs.Reset(2);
    FCustomInput& World = Custom->Inputs.AddDefaulted_GetRef();
    World.InputName = TEXT("AbsoluteWorldPosition");
    FCustomInput& Normal = Custom->Inputs.AddDefaulted_GetRef();
    Normal.InputName = TEXT("WorldNormal");
    Custom->AdditionalOutputs.Reset();
    Custom->AdditionalDefines.Reset();
    Custom->IncludeFilePaths.Reset();
    return Custom;
}

bool InputIs(const FExpressionInput& Input, const UMaterialExpression* Expression)
{
    return Input.Expression == Expression;
}

bool ValidateCustom(
    const UMaterialExpressionCustom* Custom,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    const UMaterialExpressionWorldPosition* World,
    const UMaterialExpressionVertexNormalWS* Normal)
{
    return Custom && Custom->Desc == Description &&
        Custom->Description == Description && Custom->Code == Code &&
        Custom->OutputType == OutputType && Custom->Inputs.Num() == 2 &&
        Custom->Inputs[0].InputName == TEXT("AbsoluteWorldPosition") &&
        Custom->Inputs[1].InputName == TEXT("WorldNormal") &&
        InputIs(Custom->Inputs[0].Input, World) &&
        InputIs(Custom->Inputs[1].Input, Normal) &&
        Custom->AdditionalOutputs.IsEmpty() &&
        Custom->AdditionalDefines.IsEmpty() &&
        Custom->IncludeFilePaths.IsEmpty();
}

bool ValidateMaterial(
    const UMaterial* Material,
    const FMaterialSpec& Spec,
    bool bRequireSaved,
    FString& OutError)
{
    const UPackage* Package = Material ? Material->GetOutermost() : nullptr;
    const UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const FString Prefix = FString(TEXT("R24.")) + Spec.Asset;
    const UMaterialExpressionWorldPosition* World = nullptr;
    const UMaterialExpressionVertexNormalWS* Normal = nullptr;
    const UMaterialExpressionCustom* Base = nullptr;
    const UMaterialExpressionCustom* Roughness = nullptr;
    const UMaterialExpressionCustom* SurfaceNormal = nullptr;
    const UMaterialExpressionConstant* Specular = nullptr;
    const UMaterialExpressionConstant* Metallic = nullptr;
    int32 TextureSamples = 0;
    if (Data)
    {
        for (const UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (const auto* WorldCandidate =
                    Cast<UMaterialExpressionWorldPosition>(Expression))
            {
                World = WorldCandidate;
            }
            else if (const auto* NormalCandidate =
                         Cast<UMaterialExpressionVertexNormalWS>(Expression))
            {
                Normal = NormalCandidate;
            }
            else if (const auto* CustomCandidate =
                         Cast<UMaterialExpressionCustom>(Expression))
            {
                if (CustomCandidate->Desc == Prefix + TEXT(".BaseColor")) Base = CustomCandidate;
                else if (CustomCandidate->Desc == Prefix + TEXT(".Roughness")) Roughness = CustomCandidate;
                else if (CustomCandidate->Desc == Prefix + TEXT(".WorldNormal")) SurfaceNormal = CustomCandidate;
            }
            else if (const auto* ConstantCandidate =
                         Cast<UMaterialExpressionConstant>(Expression))
            {
                if (ConstantCandidate->Desc == Prefix + TEXT(".Specular")) Specular = ConstantCandidate;
                else if (ConstantCandidate->Desc == Prefix + TEXT(".Metallic")) Metallic = ConstantCandidate;
            }
            TextureSamples += Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
        }
    }
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != MaterialObjectPath(Spec) || !Package ||
        (bRequireSaved &&
         (!FPackageName::DoesPackageExist(Package->GetName()) || Package->IsDirty())) ||
        !Data || Data->ExpressionCollection.Expressions.Num() != 7 ||
        TextureSamples != 0 || !World ||
        World->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        !Normal ||
        !ValidateCustom(Base, Prefix + TEXT(".BaseColor"), *Spec.BaseCode,
            CMOT_Float3, World, Normal) ||
        !ValidateCustom(Roughness, Prefix + TEXT(".Roughness"),
            *Spec.RoughnessCode, CMOT_Float1, World, Normal) ||
        !ValidateCustom(SurfaceNormal, Prefix + TEXT(".WorldNormal"),
            *Spec.NormalCode, CMOT_Float3, World, Normal) ||
        !Specular || !FMath::IsNearlyEqual(Specular->R, Spec.Specular) ||
        !Metallic || !FMath::IsNearlyEqual(Metallic->R, Spec.Metallic) ||
        !InputIs(Data->BaseColor, Base) ||
        !InputIs(Data->Roughness, Roughness) ||
        !InputIs(Data->Normal, SurfaceNormal) ||
        !InputIs(Data->Specular, Specular) ||
        !InputIs(Data->Metallic, Metallic) ||
        Data->WorldPositionOffset.Expression || Data->Displacement.Expression ||
        Data->PixelDepthOffset.Expression || Data->Opacity.Expression ||
        Data->OpacityMask.Expression ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->GetNaniteOverride() ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement))
    {
        OutError = TEXT("R24B Temasek Shophouse procedural texture-free PBR graph drifted for '") +
            FString(Spec.Asset) + TEXT("'.");
        return false;
    }
    OutError.Reset();
    return true;
}

UMaterial* CreateMaterial(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              Spec.Asset,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              AssetCreateContext))
        : nullptr;
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data || !Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not create fresh R24B material '") +
            FString(Spec.Asset) + TEXT("'.");
        return nullptr;
    }

    const FString Prefix = FString(TEXT("R24.")) + Spec.Asset;
    UMaterialExpressionWorldPosition* World =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, Prefix + TEXT(".WorldPosition"), -900, -100);
    UMaterialExpressionVertexNormalWS* Normal =
        AddExpression<UMaterialExpressionVertexNormalWS>(
            Material, Prefix + TEXT(".VertexNormalWS"), -900, 40);
    UMaterialExpressionCustom* Base = AddCustom(
        Material, Prefix + TEXT(".BaseColor"), *Spec.BaseCode,
        CMOT_Float3, -620, -230);
    UMaterialExpressionCustom* Roughness = AddCustom(
        Material, Prefix + TEXT(".Roughness"), *Spec.RoughnessCode,
        CMOT_Float1, -620, 0);
    UMaterialExpressionCustom* SurfaceNormal = AddCustom(
        Material, Prefix + TEXT(".WorldNormal"), *Spec.NormalCode,
        CMOT_Float3, -620, 230);
    UMaterialExpressionConstant* Specular =
        AddExpression<UMaterialExpressionConstant>(
            Material, Prefix + TEXT(".Specular"), -340, 20);
    UMaterialExpressionConstant* Metallic =
        AddExpression<UMaterialExpressionConstant>(
            Material, Prefix + TEXT(".Metallic"), -340, 120);
    if (!World || !Normal || !Base || !Roughness || !SurfaceNormal ||
        !Specular || !Metallic)
    {
        OutError = TEXT("Could not allocate complete R24B PBR graph for '") +
            FString(Spec.Asset) + TEXT("'.");
        return nullptr;
    }
    World->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    for (UMaterialExpressionCustom* Custom : {Base, Roughness, SurfaceNormal})
    {
        Custom->Inputs[0].Input.Connect(0, World);
        Custom->Inputs[1].Input.Connect(0, Normal);
    }
    Specular->R = Spec.Specular;
    Metallic->R = Spec.Metallic;

    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Opaque;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = false;
    Material->bUseMaterialAttributes = false;
    Material->bScreenSpaceReflections = false;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    Data->BaseColor.Connect(0, Base);
    Data->Roughness.Connect(0, Roughness);
    Data->Normal.Connect(0, SurfaceNormal);
    Data->Specular.Connect(0, Specular);
    Data->Metallic.Connect(0, Metallic);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (!ValidateMaterial(Material, Spec, false, OutError))
    {
        return nullptr;
    }
    return Material;
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
    Options->StaticMeshImportData->ImportUniformScale = 100.0f;
    Options->StaticMeshImportData->bCombineMeshes = true;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = true;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = false;
    Options->StaticMeshImportData->NormalImportMethod =
        EFBXNormalImportMethod::FBXNIM_ComputeNormals;
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

void MakeRenderOnly(UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return;
    }
    Mesh->Modify();
    if (Mesh->GetNumSourceModels() == 1)
    {
        if (FMeshDescription* Description = Mesh->GetMeshDescription(0))
        {
            FStaticMeshOperations::ApplyTransform(
                *Description,
                FTransform(
                    FQuat::Identity,
                    FVector::ZeroVector,
                    FVector(1.0, -1.0, 1.0)),
                true);
            Mesh->CommitMeshDescription(0);
        }
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(0);
        SourceModel.BuildSettings.bGenerateLightmapUVs = false;
        SourceModel.BuildSettings.bUseFullPrecisionUVs = true;
    }
    Mesh->SetLightMapCoordinateIndex(0);
    // The small landmark keeps LOD0 CPU buffers so runtime can recompute the
    // provenance-stamped render-payload digest after cook/load.
    Mesh->bAllowCPUAccess = true;
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
        Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    }
    Mesh->MarkAsNotHavingNavigationData();
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
}

int32 CountProvenance(const UStaticMesh* Mesh)
{
    int32 Count = 0;
    const TArray<UAssetUserData*>* Data = Mesh
        ? Mesh->GetAssetUserDataArray()
        : nullptr;
    if (Data)
    {
        for (const UAssetUserData* Row : *Data)
        {
            Count += Row && Row->IsA(
                UTRIADIstanaExploreV5DTemasekShophouseProvenance::StaticClass());
        }
    }
    return Count;
}

const UTRIADIstanaExploreV5DTemasekShophouseProvenance* FindProvenance(
    const UStaticMesh* Mesh)
{
    const TArray<UAssetUserData*>* Data = Mesh
        ? Mesh->GetAssetUserDataArray()
        : nullptr;
    if (Data)
    {
        for (const UAssetUserData* Row : *Data)
        {
            if (const auto* Provenance = Cast<
                    UTRIADIstanaExploreV5DTemasekShophouseProvenance>(Row))
            {
                return Provenance;
            }
        }
    }
    return nullptr;
}

bool StampProvenance(UStaticMesh* Mesh, FString& OutError)
{
    if (!Mesh)
    {
        OutError = TEXT("Cannot stamp provenance on a null R24B mesh.");
        return false;
    }
    const int32 Existing = CountProvenance(Mesh);
    for (int32 Index = 0; Index < Existing; ++Index)
    {
        Mesh->RemoveUserDataOfClass(
            UTRIADIstanaExploreV5DTemasekShophouseProvenance::StaticClass());
    }
    if (CountProvenance(Mesh) != 0)
    {
        OutError = TEXT("Could not remove prior R24B provenance deterministically.");
        return false;
    }
    FString CookedRenderPayloadSha256;
    if (!UTRIADIstanaExploreV5DTemasekShophouseProvenance::
            ComputeCookedRenderPayloadSha256(
                Mesh,
                CookedRenderPayloadSha256,
                OutError))
    {
        return false;
    }
    auto* Provenance =
        NewObject<UTRIADIstanaExploreV5DTemasekShophouseProvenance>(
            Mesh, NAME_None, RF_Transactional);
    if (!Provenance)
    {
        OutError = TEXT("Could not allocate R24B cooked provenance.");
        return false;
    }
    Provenance->SetCanonicalContract(CookedRenderPayloadSha256);
    Mesh->AddAssetUserData(Provenance);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    OutError.Reset();
    return true;
}

bool NormalizeAndBind(
    UStaticMesh* Mesh,
    const TArray<UMaterial*>& Materials,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const FStaticMeshRenderData* RenderData = Mesh
        ? Mesh->GetRenderData()
        : nullptr;
    if (!Mesh || Materials.Num() != ExpectedMaterialCount ||
        Materials.Contains(nullptr) ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("R24B normalization requires the exact fifteen-slot/fifteen-section zero-baked-foliage mesh and fifteen procedural materials.");
        return false;
    }

    TMap<FName, int32> Canonical;
    for (int32 Index = 0; Index < ExpectedMaterialCount; ++Index)
    {
        Canonical.Add(FName(MaterialSpecs[Index].Slot), Index);
    }
    const TArray<FStaticMaterial> Imported = Mesh->GetStaticMaterials();
    TArray<FStaticMaterial> Ordered;
    Ordered.SetNum(ExpectedMaterialCount);
    TArray<int32> ImportedToCanonical;
    ImportedToCanonical.Init(INDEX_NONE, ExpectedMaterialCount);
    TArray<bool> Seen;
    Seen.Init(false, ExpectedMaterialCount);
    for (int32 ImportedIndex = 0;
         ImportedIndex < ExpectedMaterialCount;
         ++ImportedIndex)
    {
        const FStaticMaterial& Source = Imported[ImportedIndex];
        const int32* CanonicalIndex = Canonical.Find(Source.MaterialSlotName);
        if (!CanonicalIndex ||
            Source.MaterialSlotName != Source.ImportedMaterialSlotName ||
            Seen[*CanonicalIndex])
        {
            OutError = TEXT("R24B imported material names are not an exact unique semantic permutation.");
            return false;
        }
        Seen[*CanonicalIndex] = true;
        ImportedToCanonical[ImportedIndex] = *CanonicalIndex;
        Ordered[*CanonicalIndex] = Source;
        Ordered[*CanonicalIndex].MaterialSlotName =
            FName(MaterialSpecs[*CanonicalIndex].Slot);
        Ordered[*CanonicalIndex].ImportedMaterialSlotName =
            FName(MaterialSpecs[*CanonicalIndex].Slot);
        Ordered[*CanonicalIndex].MaterialInterface = Materials[*CanonicalIndex];
    }

    TSet<int32> SeenSections;
    const FStaticMeshLODResources& Lod = RenderData->LODResources[0];
    for (int32 SectionIndex = 0; SectionIndex < Lod.Sections.Num(); ++SectionIndex)
    {
        const FStaticMeshSection& RenderSection = Lod.Sections[SectionIndex];
        const int32 ImportedIndex = RenderSection.MaterialIndex;
        if (!ImportedToCanonical.IsValidIndex(ImportedIndex) ||
            ImportedToCanonical[ImportedIndex] == INDEX_NONE)
        {
            OutError = TEXT("An R24B section references an invalid imported material index.");
            return false;
        }
        const int32 CanonicalIndex = ImportedToCanonical[ImportedIndex];
        if (SeenSections.Contains(CanonicalIndex) ||
            RenderSection.NumTriangles != MaterialSpecs[CanonicalIndex].Triangles)
        {
            OutError = TEXT("R24B semantic section triangle census changed.");
            return false;
        }
        SeenSections.Add(CanonicalIndex);
        FMeshSectionInfo Section = Mesh->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Mesh->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        Section.MaterialIndex = CanonicalIndex;
        Section.bEnableCollision = false;
        Section.bCastShadow = true;
        Original.MaterialIndex = CanonicalIndex;
        Original.bEnableCollision = false;
        Original.bCastShadow = true;
        Mesh->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Mesh->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    if (SeenSections.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("R24B import omitted one or more semantic sections.");
        return false;
    }
    Mesh->Modify();
    Mesh->SetStaticMaterials(Ordered);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    OutError.Reset();
    return true;
}

bool ValidateMesh(
    UStaticMesh* Mesh,
    const TArray<UMaterial*>& Materials,
    bool bRequireSaved,
    FString& OutError)
{
    if (Mesh)
    {
        FStaticMeshCompilingManager::Get().FinishCompilation({Mesh});
    }
    const UAssetImportData* ImportData = Mesh ? Mesh->GetAssetImportData() : nullptr;
    const TArray<FString> Sources = ImportData
        ? ImportData->ExtractFilenames()
        : TArray<FString>();
    FString ActualProjectRelativeSource;
    const bool bSourceIdentityCanonical = Sources.Num() == 1 &&
        CanonicalProjectRelativeSourcePath(
            Sources[0],
            ActualProjectRelativeSource) &&
        ActualProjectRelativeSource == ObjSourceProjectRelativePath;
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    const FMeshDescription* Description = Mesh && Mesh->GetNumSourceModels() == 1
        ? Mesh->GetMeshDescription(0)
        : nullptr;
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    const UPackage* Package = Mesh ? Mesh->GetOutermost() : nullptr;
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != MeshObjectPath ||
        !bSourceIdentityCanonical ||
        (bRequireSaved &&
         (!Package || !FPackageName::DoesPackageExist(Package->GetName()) ||
          Package->IsDirty())) ||
        Mesh->GetNumSourceModels() != 1 || !Description ||
        Description->Vertices().Num() != ExpectedSourceVertexCount ||
        Description->Triangles().Num() != ExpectedTriangleCount ||
        Description->PolygonGroups().Num() != ExpectedMaterialCount ||
        Mesh->GetSourceModel(0).BuildSettings.BuildScale3D != FVector::OneVector ||
        Mesh->GetSourceModel(0).BuildSettings.bGenerateLightmapUVs ||
        !Mesh->GetSourceModel(0).BuildSettings.bUseFullPrecisionUVs ||
        Mesh->GetStaticMaterials().Num() != ExpectedMaterialCount ||
        !RenderData || RenderData->LODResources.Num() != 1 ||
        RenderData->LODResources[0].GetNumTriangles() != ExpectedTriangleCount ||
        RenderData->LODResources[0].Sections.Num() != ExpectedMaterialCount ||
        !Mesh->NaniteSettings.bEnabled || !Mesh->HasValidNaniteData() ||
        !FMath::IsNearlyEqual(Mesh->NaniteSettings.KeepPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.TrimRelativeError) ||
        Mesh->NaniteSettings.FallbackTarget != ENaniteFallbackTarget::PercentTriangles ||
        !FMath::IsNearlyEqual(Mesh->NaniteSettings.FallbackPercentTriangles, 1.0f) ||
        !FMath::IsNearlyZero(Mesh->NaniteSettings.FallbackRelativeError) ||
        !Body || Body->AggGeom.GetElementCount() != 0 ||
        Body->CollisionTraceFlag != CTF_UseSimpleAsComplex ||
        Mesh->bHasNavigationData || Mesh->GetNavCollision() ||
        !Mesh->bAllowCPUAccess ||
        CountProvenance(Mesh) != 1 || Materials.Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("R24B Temasek Shophouse mesh lost its exact source, 9,536-vertex/15,760-triangle/fifteen-section/zero-baked-foliage census, identity build scale, full-fidelity Nanite fallback, zero-collision, no-navigation, or cooked-provenance contract.");
        return false;
    }
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    if (!(Bounds.Origin - Bounds.BoxExtent).Equals(
            FVector(-2225.0, -2305.0, 0.0), 0.25) ||
        !(Bounds.Origin + Bounds.BoxExtent).Equals(
            FVector(2225.0, 1681.9333, 1463.0642), 0.25))
    {
        OutError = TEXT("R24B imported centimetre bounds changed.");
        return false;
    }
    TArray<bool> SeenSections;
    SeenSections.Init(false, ExpectedMaterialCount);
    for (const FStaticMeshSection& Section : RenderData->LODResources[0].Sections)
    {
        if (!SeenSections.IsValidIndex(Section.MaterialIndex) ||
            SeenSections[Section.MaterialIndex] ||
            Section.NumTriangles != MaterialSpecs[Section.MaterialIndex].Triangles)
        {
            OutError = TEXT("R24B mesh section mapping/census changed.");
            return false;
        }
        SeenSections[Section.MaterialIndex] = true;
    }
    for (int32 Index = 0; Index < ExpectedMaterialCount; ++Index)
    {
        const FStaticMaterial& Binding = Mesh->GetStaticMaterials()[Index];
        if (!SeenSections[Index] ||
            Binding.MaterialSlotName != FName(MaterialSpecs[Index].Slot) ||
            Binding.ImportedMaterialSlotName != FName(MaterialSpecs[Index].Slot) ||
            Binding.MaterialInterface != Materials[Index])
        {
            OutError = TEXT("R24B canonical semantic material binding changed.");
            return false;
        }
    }
    const UTRIADIstanaExploreV5DTemasekShophouseProvenance* Provenance =
        FindProvenance(Mesh);
    FString RecomputedPayloadSha256;
    FString PayloadError;
    if (!Provenance || !Provenance->IsCanonicalContract() ||
        Provenance->SourceObjProjectRelativePath !=
            ActualProjectRelativeSource ||
        !UTRIADIstanaExploreV5DTemasekShophouseProvenance::
            ComputeCookedRenderPayloadSha256(
                Mesh,
                RecomputedPayloadSha256,
                PayloadError) ||
        RecomputedPayloadSha256 != Provenance->CookedRenderPayloadSha256)
    {
        OutError = TEXT("R24B cooked render-payload digest or project-relative source identity changed. ") +
            PayloadError;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    if (!ValidateAllSourceHashes(OutReport) ||
        !ValidateExactRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UMaterial*> Materials;
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterial* Material = LoadExact<UMaterial>(MaterialObjectPath(Spec));
        if (!ValidateMaterial(Material, Spec, bRequireSaved, OutReport))
        {
            return false;
        }
        Materials.Add(Material);
    }
    if (!ValidateMesh(
            LoadExact<UStaticMesh>(MeshObjectPath),
            Materials,
            bRequireSaved,
            OutReport))
    {
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID assets=%d meshTriangles=%d sourceVertices=%d proceduralComponents=%d materialSlots=%d renderObjSha256=F5ED94C312A38A542D9C078F19F16FAF04CEA6ECAE5B4C2CF0AA7F3D651B9007 outputSetSha256=0ACA9941B8C60766EA4647A991DF80944B4A746E06CC2A5680C2D167C0FA4A4D placementReceiptSha256=6BEA0091D68FB0202A759733AB2838AA0A4405783D38EE1420A9E4CC090014A5 sourceIdentityProjectRelative=true cookedRenderPayloadSha256Recomputed=true cookedRenderPayloadScope=LOD0PositionsIndicesSectionsMaterialAssignments accidentalDriftGuard=true signedAuthority=false cpuReadableLod0=true proceduralTextureFreePbr=true whitePlasterMicrovariation=true charcoalTrim=true opaqueRoughnessControlledGlass=true timberGrain=true brass=true mosaic=true precastConcrete=true terracotta=true solarPanels=true reclaimedPavers=true soilAndRainwater=true foliageLayoutSchema=triad.istana_explore_v5d.r24_temasek_shophouse.foliage_layout.v1 foliageOwner=ATRIADIstanaExploreV5DLandmarkVegetationActor bakedFoliageRenderComponents=0 foliageTreeAnchors=3 materialCalibration=false naniteFullMesh=true rasterFallbackFullMesh=true zeroCollision=true noNavigation=true renderOnly=true surveyAsBuiltOneToOne=false sensorRfAuthority=false."),
        ExpectedAssetCount,
        ExpectedTriangleCount,
        ExpectedSourceVertexCount,
        ExpectedComponentCount,
        ExpectedMaterialCount);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DTemasekShophouseAssetFactory
{
const FString& GetAssetRootPath() { return AssetRoot; }
const FString& GetMeshObjectPath() { return MeshObjectPath; }

const TArray<FString>& GetOrderedMaterialObjectPaths()
{
    static const TArray<FString> Paths = []
    {
        TArray<FString> Result;
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            Result.Add(MaterialObjectPath(Spec));
        }
        return Result;
    }();
    return Paths;
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    if (bFreshAssetTransactionActive)
    {
        OutError = TEXT("R24B CreateFreshAssets refused: a prior fresh-asset transaction is still active.");
        return false;
    }
    if (MeshObjectPath !=
            ATRIADIstanaExploreV5DTemasekShophouseActor::ExpectedMeshObjectPath() ||
        !ValidateAllSourceHashes(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R24B factory/runtime mesh identity diverged.");
        }
        return false;
    }
    if (!ValidateExactEmptyAssetRoot(OutError))
    {
        OutError = TEXT("R24B CreateFreshAssets requires an exact empty registry-and-disk asset-root predecessor. ") +
            OutError;
        return false;
    }
    bFreshAssetTransactionActive = true;
    bFreshAssetTransactionReleasedToCaller = false;
    FreshAssetTransactionOwnedObjectPaths.Reset();
    // The exact-empty predecessor proof lets this transaction reserve the
    // entire sixteen-object namespace before creation. That receipt covers an
    // exact package artifact even if post-save corruption makes it unloadable
    // and therefore absent from both LoadObject and the Asset Registry.
    for (const FString& ReservedObjectPath : ExpectedObjectPaths())
    {
        FreshAssetTransactionOwnedObjectPaths.Add(ReservedObjectPath);
    }
    FScopedFreshRollback Rollback(OutAssets, OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    TArray<UMaterial*> Materials;
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterial* Material = CreateMaterial(AssetTools, Spec, OutError);
        if (!Material)
        {
            return false;
        }
        Materials.Add(Material);
        OutAssets.Add(Material);
        FreshAssetTransactionOwnedObjectPaths.Add(Material->GetPathName());
    }

    UAssetImportTask* Task = MakeImportTask();
    if (!Task)
    {
        OutError = TEXT("Could not allocate the exact R24B OBJ import task.");
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
    if (!Mesh)
    {
        OutError = TEXT("R24B OBJ import did not create the exact mesh.");
        return false;
    }
    OutAssets.Insert(Mesh, 0);
    FreshAssetTransactionOwnedObjectPaths.Add(Mesh->GetPathName());
    MakeRenderOnly(Mesh);
    if (!NormalizeAndBind(Mesh, Materials, OutError) ||
        !StampProvenance(Mesh, OutError) ||
        !ValidateMesh(Mesh, Materials, false, OutError))
    {
        return false;
    }
    FString PreSaveReport;
    if (!ValidateInternal(false, PreSaveReport))
    {
        OutError = TEXT("Fresh R24B validation failed before save: ") +
            PreSaveReport;
        return false;
    }
    TArray<FString> OwnedPaths =
        FreshAssetTransactionOwnedObjectPaths.Array();
    OwnedPaths.Sort();
    if (OwnedPaths != ExpectedObjectPaths())
    {
        OutError = TEXT("Fresh R24B ownership receipt is not the exact sixteen-object roster.");
        return false;
    }
    bFreshAssetTransactionReleasedToCaller = true;
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool RollbackActiveFreshAssetTransaction(FString& OutReport)
{
    if (!bFreshAssetTransactionReleasedToCaller)
    {
        OutReport = TEXT("R24B post-create rollback refused: no exact sixteen-object ownership receipt was released to the caller.");
        return false;
    }
    return RollbackActiveFreshAssetTransactionInternal(OutReport);
}

bool CommitActiveFreshAssetTransaction(FString& OutReport)
{
    if (!bFreshAssetTransactionActive ||
        !bFreshAssetTransactionReleasedToCaller)
    {
        OutReport = TEXT("R24B fresh-asset commit refused: no active exact-empty-predecessor transaction.");
        return false;
    }
    FString ColdReport;
    if (!ValidateInternal(true, ColdReport))
    {
        OutReport = TEXT("R24B fresh-asset commit refused before exact cold validation: ") +
            ColdReport;
        return false;
    }
    bFreshAssetTransactionActive = false;
    bFreshAssetTransactionReleasedToCaller = false;
    FreshAssetTransactionOwnedObjectPaths.Reset();
    OutReport = TEXT("R24_TEMASEK_FRESH_ASSET_TRANSACTION_COMMITTED saveReloadColdValidation=true exactEmptyPredecessor=true assets=16 bakedFoliageRenderComponents=0 foliageTreeAnchors=3");
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}

bool LoadValidatedRuntimeMesh(UStaticMesh*& OutMesh, FString& OutError)
{
    OutMesh = nullptr;
    if (!ValidateAssets(OutError))
    {
        return false;
    }
    OutMesh = LoadExact<UStaticMesh>(MeshObjectPath);
    if (!OutMesh)
    {
        OutError = TEXT("Validated R24B runtime mesh could not be loaded exactly.");
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace TRIADIstanaExploreV5DTemasekShophouseAssetFactory
