#include "TRIADIstanaExploreV5BAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/FbxFactory.h"
#include "Factories/FbxImportUI.h"
#include "Factories/FbxStaticMeshImportData.h"
#include "Factories/MaterialFactoryNew.h"
#include "FbxMeshUtils.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "ShaderCompiler.h"
#include "Ssl.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5AppearanceActor.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
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
const FString AssetRoot(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5B"));
const FString BuildingPath(AssetRoot + TEXT("/Building"));
const FString PropMeshPath(AssetRoot + TEXT("/Props/Meshes"));
const FString VegetationMeshPath(AssetRoot + TEXT("/Vegetation/Meshes"));
const FString TexturePath(AssetRoot + TEXT("/Vegetation/Textures"));
const FString MaterialPath(AssetRoot + TEXT("/Materials"));

const FString PorticoName(TEXT("SM_IPV5B_BuildingHero_CrossTrimmed"));
const FString HardscapeRenderSuccessorName(
    TEXT("SM_IPV5B_Hardscape_NoLegacyBeds"));
const FString SurfaceName(TEXT("SM_IPV5B_FountainSurface"));
const FString FoamName(TEXT("SM_IPV5B_FountainEdgeFoam"));
const FString ImpactName(TEXT("SM_IPV5B_FountainImpactRing"));
const FString PlumeName(TEXT("SM_IPV5B_FountainPlume"));
const FString CentralPlumeName(TEXT("SM_IPV5B_FountainCentralPlume"));
const FString PaverInnerName(TEXT("SM_IPV5B_PaverWedgeInner"));
const FString PaverOuterName(TEXT("SM_IPV5B_PaverWedgeOuter"));
const FString AccentTurfName(TEXT("SM_IPV5B_BermudaTurfCluster"));
// UE 5.5's FbxMeshUtils::ImportStaticMeshLOD can register this exact
// in-memory scratch mesh while importing an LOD into an existing asset. It
// is not one of the 37 V5B assets and must never become a persisted package.
const FString AccentTurfLodImportScratchPackageName(
    VegetationMeshPath + TEXT("/None"));
const FString AccentTurfLodImportScratchObjectPath(
    AccentTurfLodImportScratchPackageName + TEXT(".StaticMesh_0"));
constexpr int64 AccentTurfLod0ObjBytes = 515342;
constexpr int64 AccentTurfLod1ObjBytes = 238493;
constexpr int64 AccentTurfLod2ObjBytes = 70572;
constexpr int64 AccentTurfMtlBytes = 119;
constexpr int64 AccentTurfManifestBytes = 17943;
const FString AccentTurfLod0ObjSha256(
    TEXT("294032971A4F9049A474A0226248184106509001B07C5E7EE0073FEC2331E7B6"));
const FString AccentTurfLod1ObjSha256(
    TEXT("2AEAE5F3358B8385D48CAB46FB8AED8AB69ACDB4A94475301AB1DAED996A8475"));
const FString AccentTurfLod2ObjSha256(
    TEXT("8EEA89426433072677F99C2DB3EAF9F68D0DE9A2FFEB7F3B971FAED14FC07743"));
const FString AccentTurfMtlSha256(
    TEXT("B5D4EEB2CE7FDE84C8DD1673796855017A6080960699C90CE45B81AE048BD8AE"));
const FString AccentTurfManifestSha256(
    TEXT("E30A1FA46F559933F6AB209F7018EE590DFD0546C389294ABFC4784F69B10D80"));
constexpr int32 AccentTurfLod0Triangles = 2550;
constexpr int32 AccentTurfLod1Triangles = 680;
constexpr int32 AccentTurfLod2Triangles = 204;
constexpr float AccentTurfLod1ScreenSize = 0.10f;
constexpr float AccentTurfLod2ScreenSize = 0.040f;
// This is a frozen one-material compatibility value, not R11 source-height
// evidence. The scoped populated-namespace upgrade is intentionally allowed
// to save only the mesh; changing this value requires a separately authorized
// two-asset material migration. R11's exact source maximum is 4.4 cm.
constexpr float AccentFrozenMaterialWindHeightNormalizerCm = 4.8f;
const FString FormalBedName(TEXT("SM_IPV5B_FormalBedSoilVeneer"));
const FString TreeBaseMulchName(TEXT("SM_IPV5B_TreeBaseMulchMound"));
constexpr int64 TreeBaseMulchObjBytes = 17630;
constexpr int64 TreeBaseMulchMtlBytes = 280;
constexpr int64 TreeBaseMulchManifestBytes = 1456;
const FString TreeBaseMulchObjSha256(
    TEXT("93F07CB64B37AB46696ACDAC9433E16071BB231AE865EF0BB0E67684C49F0887"));
const FString TreeBaseMulchMtlSha256(
    TEXT("CC5403A5A9B6535403FE7F4DFD2E194E196CD3093F7D59AC084F59BE926D4277"));
const FString TreeBaseMulchManifestSha256(
    TEXT("DF1352077E0DA19F7619D8E0A4C43E2807952BB77EC79BC87091C48BBD87722C"));

const FString AccentMaterialName(TEXT("M_IPV5B_AccentTurf"));
const FString FormalBedMaterialName(TEXT("M_IPV5B_FormalBedSoil"));
const FString SurfaceMaterialName(TEXT("M_IPV5B_FountainSurface"));
const FString FoamMaterialName(TEXT("M_IPV5B_FountainFoam"));
const FString SprayMaterialName(TEXT("M_IPV5B_FountainSpray"));
const FString PaverMaterialName(TEXT("MI_IPV5B_PaverStone"));
const FString BarkMaterialName(TEXT("M_IPV5B_PachiraBark"));
const FString LeavesMaterialName(TEXT("M_IPV5B_PachiraLeaves"));
const FString PachiraLeavesOpacityTextureName(
    TEXT("T_IPV5B_Pachira_Leaves_Opacity"));

constexpr float PachiraOpacityClipValue = 0.34f;
constexpr float PachiraWindHeightCm = 190.0f;
constexpr float PachiraWindStrengthCm = 6.0f;
constexpr float PachiraWindSpeed = 0.42f;
constexpr float PachiraBarkWindResponse = 0.08f;
constexpr float PachiraLeavesWindResponse = 0.60f;
constexpr float PachiraBarkMaximumWpoCm = 2.0f;
constexpr float PachiraLeavesMaximumWpoCm = 8.0f;
constexpr float PachiraBarkSpecular = 0.18f;
constexpr float PachiraLeavesSpecular = 0.22f;
const FLinearColor PachiraLeavesSubsurfaceAttenuation(
    0.22f, 0.34f, 0.14f, 1.0f);

const FString SourceHeroPath(
    TEXT("/Game/TRIAD/IstanaPublicViewV5/Building/SM_IstanaPublicViewV5_Building_Hero.SM_IstanaPublicViewV5_Building_Hero"));
const FString SourceHardscapePath(
    TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Hardscape.SM_IstanaPublicView_Hardscape"));
FString ObjectPath(const FString& PackagePath, const FString& Name)
{
    return PackagePath + TEXT("/") + Name + TEXT(".") + Name;
}

const FString PorticoObjectPath = ObjectPath(BuildingPath, PorticoName);
const FString HardscapeRenderSuccessorObjectPath =
    ObjectPath(PropMeshPath, HardscapeRenderSuccessorName);
const FString SurfaceObjectPath = ObjectPath(PropMeshPath, SurfaceName);
const FString FoamObjectPath = ObjectPath(PropMeshPath, FoamName);
const FString ImpactObjectPath = ObjectPath(PropMeshPath, ImpactName);
const FString PlumeObjectPath = ObjectPath(PropMeshPath, PlumeName);
const FString CentralPlumeObjectPath = ObjectPath(PropMeshPath, CentralPlumeName);
const FString PaverInnerObjectPath = ObjectPath(PropMeshPath, PaverInnerName);
const FString PaverOuterObjectPath = ObjectPath(PropMeshPath, PaverOuterName);
const FString AccentTurfObjectPath = ObjectPath(VegetationMeshPath, AccentTurfName);
const FString FormalBedObjectPath = ObjectPath(PropMeshPath, FormalBedName);
const FString TreeBaseMulchObjectPath =
    ObjectPath(PropMeshPath, TreeBaseMulchName);
const FString AccentMaterialObjectPath = ObjectPath(MaterialPath, AccentMaterialName);
const FString FormalBedMaterialObjectPath =
    ObjectPath(MaterialPath, FormalBedMaterialName);
const FString SurfaceMaterialObjectPath = ObjectPath(MaterialPath, SurfaceMaterialName);
const FString FoamMaterialObjectPath = ObjectPath(MaterialPath, FoamMaterialName);
const FString SprayMaterialObjectPath = ObjectPath(MaterialPath, SprayMaterialName);
const FString PaverMaterialObjectPath = ObjectPath(MaterialPath, PaverMaterialName);
const FString BarkMaterialObjectPath = ObjectPath(MaterialPath, BarkMaterialName);
const FString LeavesMaterialObjectPath = ObjectPath(MaterialPath, LeavesMaterialName);
const FString DitherTemporalAaFunctionPath(
    TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA"));

const FString AccentWindDescription(
    TEXT("TRIAD_EXPLORE_V5B_INSTANCE_LOCAL_BERMUDA_WPO_V3_FADE_MATCHED"));
const FString AccentWindCode(
    TEXT("float instanceSeed = saturate(Random01);\n")
    TEXT("float bladeSeed = saturate(BladeUV.x);\n")
    TEXT("float localHeight = max(InstanceLocalPosition.z, 0.0);\n")
    TEXT("float h = saturate(localHeight / max(HeightCm, 1.0));\n")
    TEXT("float bladePhase = (bladeSeed - 0.5) * 1.10;\n")
    TEXT("float phase = TimeSeconds * WindSpeed * 6.28318530718 + instanceSeed * 6.28318530718 + dot(WorldPosition.xy, float2(0.0017, 0.0023)) + bladePhase;\n")
    TEXT("float wave = sin(phase) + 0.27 * sin(phase * 1.73 + 1.2 + bladeSeed * 2.10);\n")
    TEXT("float bladeResponse = lerp(0.88, 1.10, frac(bladeSeed * 13.173));\n")
    TEXT("float2 direction = normalize(WindDirection.xy + float2(0.0001, 0.0));\n")
    TEXT("float bend = clamp(WindStrengthCm * ResponseScale * bladeResponse * wave * h * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("float lift = clamp(abs(WindStrengthCm) * ResponseScale * 0.020 * sin(phase * 0.71 + bladePhase) * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("float wpoFade = smoothstep(0.9090909, 1.0, saturate(InstanceFade));\n")
    TEXT("return float3(direction * bend, lift) * wpoFade;"));
const FString AccentBladeColorDescription(
    TEXT("TRIAD_EXPLORE_V5B_MODELED_BLADE_COLOR_V7_PHYSICAL_NORMAL_READABILITY"));
const FString AccentBladeColorCode(
    TEXT("float h = saturate(BladeUV.y);\n")
    TEXT("float bladeSeed = saturate(BladeUV.x);\n")
    TEXT("float instanceSeed = saturate(Random01);\n")
    TEXT("float rise = smoothstep(0.05, 0.68, h);\n")
    TEXT("float cutTip = smoothstep(0.72, 1.00, h);\n")
    TEXT("float tonal = lerp(0.96, 1.04, frac(bladeSeed * 7.173 + instanceSeed * 0.381966));\n")
    TEXT("float warmth = frac(bladeSeed * 13.371 + instanceSeed * 0.618034);\n")
    TEXT("float3 root = float3(0.070, 0.155, 0.048);\n")
    TEXT("float3 body = float3(0.082, 0.185, 0.058);\n")
    TEXT("float3 cutColor = float3(0.076, 0.174, 0.054);\n")
    TEXT("float3 hue = lerp(float3(1.010, 1.000, 0.980), float3(0.990, 1.005, 1.020), warmth);\n")
    TEXT("float3 result = lerp(root, body, rise);\n")
    TEXT("result = lerp(result, cutColor, cutTip);\n")
    TEXT("return saturate(result * tonal * hue);"));
const FString PachiraLeafColorDescription(
    TEXT("TRIAD_EXPLORE_V5B_PACHIRA_PER_INSTANCE_LEAF_COLOR_V1"));
const FString PachiraLeafColorCode(
    TEXT("float seed = saturate(Random01);\n")
    TEXT("float3 shadeTint = float3(0.900, 0.950, 0.860);\n")
    TEXT("float3 sunTint = float3(1.020, 1.000, 0.960);\n")
    TEXT("float tonal = lerp(0.965, 1.025, frac(seed * 1.61803398875));\n")
    TEXT("return saturate(BaseColor * lerp(shadeTint, sunTint, seed) * tonal);"));
const FString PachiraWindDescription(
    TEXT("TRIAD_EXPLORE_V5B_INSTANCE_LOCAL_PACHIRA_WPO_V1"));
const FString PachiraWindCode(
    TEXT("float seed = saturate(Random01);\n")
    TEXT("float h = saturate(max(InstanceLocalPosition.z, 0.0) / max(HeightCm, 1.0));\n")
    TEXT("float phase = TimeSeconds * WindSpeed * 6.28318530718 + seed * 6.28318530718 + dot(WorldPosition.xy, float2(0.0017, 0.0023));\n")
    TEXT("float wave = sin(phase) + 0.23 * sin(phase * 1.73 + 1.2);\n")
    TEXT("float stiffness = lerp(0.90, 1.10, seed);\n")
    TEXT("float2 direction = normalize(WindDirection.xy + float2(0.0001, 0.0));\n")
    TEXT("float bend = clamp(WindStrengthCm * ResponseScale * stiffness * wave * h * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("float lift = clamp(abs(WindStrengthCm) * ResponseScale * 0.018 * sin(phase * 0.71) * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("return float3(direction * bend, lift);"));
const FString FormalBedBaseDescription(
    TEXT("TRIAD_EXPLORE_V5B_FORMAL_BED_APRON_AND_TREE_MULCH_IMPORTED_V_SENTINEL_COLOR_V12"));
const FString FormalBedBaseCode(
    TEXT("float2 p = WorldPosition.xy * 0.01;\n")
    TEXT("float apronFraction = saturate(ApronUV.x);\n")
    TEXT("float treeMulchSentinel = 1.0 - step(0.5, ApronUV.y);\n")
    TEXT("float2 q0 = p / 2.40;\n")
    TEXT("float2 i0 = floor(q0);\n")
    TEXT("float2 f0 = frac(q0);\n")
    TEXT("f0 = f0 * f0 * (3.0 - 2.0 * f0);\n")
    TEXT("float m00 = frac(sin(dot(i0, float2(127.1, 311.7))) * 43758.5453);\n")
    TEXT("float m10 = frac(sin(dot(i0 + float2(1.0, 0.0), float2(127.1, 311.7))) * 43758.5453);\n")
    TEXT("float m01 = frac(sin(dot(i0 + float2(0.0, 1.0), float2(127.1, 311.7))) * 43758.5453);\n")
    TEXT("float m11 = frac(sin(dot(i0 + float2(1.0, 1.0), float2(127.1, 311.7))) * 43758.5453);\n")
    TEXT("float macro = lerp(lerp(m00, m10, f0.x), lerp(m01, m11, f0.x), f0.y);\n")
    TEXT("float2 q1 = (p + float2(13.7, -9.1)) / 0.62;\n")
    TEXT("float2 i1 = floor(q1);\n")
    TEXT("float2 f1 = frac(q1);\n")
    TEXT("f1 = f1 * f1 * (3.0 - 2.0 * f1);\n")
    TEXT("float d00 = frac(sin(dot(i1, float2(269.5, 183.3))) * 43758.5453);\n")
    TEXT("float d10 = frac(sin(dot(i1 + float2(1.0, 0.0), float2(269.5, 183.3))) * 43758.5453);\n")
    TEXT("float d01 = frac(sin(dot(i1 + float2(0.0, 1.0), float2(269.5, 183.3))) * 43758.5453);\n")
    TEXT("float d11 = frac(sin(dot(i1 + float2(1.0, 1.0), float2(269.5, 183.3))) * 43758.5453);\n")
    TEXT("float detail = lerp(lerp(d00, d10, f1.x), lerp(d01, d11, f1.x), f1.y);\n")
    TEXT("float breakup = saturate(0.72 * macro + 0.28 * detail);\n")
    TEXT("float edgeBreak = (detail - 0.5) * 0.018;\n")
    TEXT("float edgeStart = 0.740 + edgeBreak;\n")
    TEXT("float edgeEnd = 0.990 + 0.20 * edgeBreak;\n")
    TEXT("float soilMask = 1.0 - smoothstep(edgeStart, edgeEnd, apronFraction);\n")
    TEXT("float3 moist = float3(0.030, 0.027, 0.022);\n")
    TEXT("float3 loam = float3(0.066, 0.057, 0.042);\n")
    TEXT("float3 dry = float3(0.090, 0.074, 0.050);\n")
    TEXT("float3 soil = lerp(moist, loam, saturate(0.25 + 0.62 * breakup));\n")
    TEXT("soil = lerp(soil, dry, 0.03 + 0.05 * detail);\n")
    TEXT("float3 rimDark = float3(0.055, 0.070, 0.022);\n")
    TEXT("float3 rimWarm = float3(0.080, 0.095, 0.030);\n")
    TEXT("float3 rim = lerp(rimDark, rimWarm, saturate(0.28 + 0.55 * macro));\n")
    TEXT("float3 formalBed = lerp(rim, soil, soilMask);\n")
    TEXT("float edgeNoise = (detail - 0.5) * 0.060;\n")
    TEXT("float mulchMask = 1.0 - smoothstep(0.82 + edgeNoise, 0.995, apronFraction);\n")
    TEXT("float3 mulchDark = float3(0.040, 0.030, 0.021);\n")
    TEXT("float3 mulchWarm = float3(0.090, 0.060, 0.035);\n")
    TEXT("float3 mulchFleck = float3(0.135, 0.092, 0.050);\n")
    TEXT("float3 mulch = lerp(mulchDark, mulchWarm, saturate(0.22 + 0.70 * breakup));\n")
    TEXT("mulch = lerp(mulch, mulchFleck, 0.04 + 0.08 * detail);\n")
    TEXT("float3 mulchEdgeDark = float3(0.045, 0.055, 0.022);\n")
    TEXT("float3 mulchEdgeWarm = float3(0.065, 0.078, 0.028);\n")
    TEXT("float3 mulchEdge = lerp(mulchEdgeDark, mulchEdgeWarm, saturate(0.28 + 0.55 * macro));\n")
    TEXT("float3 treeMulch = lerp(mulchEdge, mulch, mulchMask);\n")
    TEXT("return lerp(formalBed, treeMulch, treeMulchSentinel);"));
const FString FormalBedRoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5B_FORMAL_BED_SOIL_ROUGHNESS_V4"));
const FString FormalBedRoughnessCode(
    TEXT("float2 p = WorldPosition.xy * 0.01;\n")
    TEXT("float2 q = (p + float2(-4.3, 17.9)) / 0.72;\n")
    TEXT("float2 i = floor(q);\n")
    TEXT("float2 f = frac(q);\n")
    TEXT("f = f * f * (3.0 - 2.0 * f);\n")
    TEXT("float n00 = frac(sin(dot(i, float2(419.2, 371.9))) * 43758.5453);\n")
    TEXT("float n10 = frac(sin(dot(i + float2(1.0, 0.0), float2(419.2, 371.9))) * 43758.5453);\n")
    TEXT("float n01 = frac(sin(dot(i + float2(0.0, 1.0), float2(419.2, 371.9))) * 43758.5453);\n")
    TEXT("float n11 = frac(sin(dot(i + float2(1.0, 1.0), float2(419.2, 371.9))) * 43758.5453);\n")
    TEXT("float roughNoise = lerp(lerp(n00, n10, f.x), lerp(n01, n11, f.x), f.y);\n")
    TEXT("return lerp(0.83, 0.93, roughNoise);"));
const FString FormalBedNormalDescription(
    TEXT("TRIAD_EXPLORE_V5B_FORMAL_BED_MULCH_FINE_MICRO_NORMAL_V2"));
const FString FormalBedNormalCode(
    TEXT("float2 p = WorldPosition.xy * 0.01;\n")
    TEXT("float2 grain = float2(cos(p.x * 92.0 + 1.7 * sin(p.y * 61.0)), cos(p.y * 103.0 + 1.5 * sin(p.x * 67.0)));\n")
    TEXT("float2 fleck = float2(cos((p.x + p.y) * 171.0 + sin(p.x * 39.0)), cos((p.y - p.x) * 157.0 + sin(p.y * 43.0)));\n")
    TEXT("float2 slope = 0.060 * grain + 0.025 * fleck;\n")
    TEXT("return normalize(float3(slope, 1.0));"));
const FString PaverUvDescription(TEXT("V5B_PAVER_STONE_UV0"));
const FString PaverRandomDescription(TEXT("V5B_PAVER_PER_INSTANCE_RANDOM"));
const FString PaverBaseDescription(
    TEXT("TRIAD_EXPLORE_V5B_PAVER_PROCEDURAL_STONE_COLOR_V1"));
const FString PaverBaseCode(
    TEXT("float2 p = UV * float2(6.0,5.0);\n")
    TEXT("float seed = saturate(Random01);\n")
    TEXT("float macro = 0.5+0.5*sin(p.x*1.31+p.y*1.73+seed*6.2831853);\n")
    TEXT("float vein = 0.5+0.5*sin(p.x*3.7-p.y*2.9+0.9*sin(p.y*1.6+seed*4.1));\n")
    TEXT("float grain = 0.5+0.5*sin(p.x*11.3+p.y*9.7+sin(p.x*2.3-p.y*1.9));\n")
    TEXT("float fleckSource = 0.5+0.5*sin(p.x*19.7+p.y*23.3+2.0*sin(p.x*5.1-p.y*3.7));\n")
    TEXT("float paleFleck = pow(saturate(fleckSource),18.0);\n")
    TEXT("float darkFleck = pow(saturate(1.0-fleckSource),20.0);\n")
    TEXT("float3 cool = float3(0.205,0.185,0.151);\n")
    TEXT("float3 warm = float3(0.355,0.294,0.208);\n")
    TEXT("float3 stone = lerp(cool,warm,saturate(0.30+0.32*seed+0.18*macro));\n")
    TEXT("stone *= lerp(0.82,1.09,saturate(0.48*macro+0.34*vein+0.18*grain));\n")
    TEXT("stone += paleFleck*float3(0.090,0.078,0.058);\n")
    TEXT("stone -= darkFleck*float3(0.060,0.050,0.037);\n")
    TEXT("return saturate(stone);"));
const FString PaverRoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5B_PAVER_PROCEDURAL_STONE_ROUGHNESS_V1"));
const FString PaverRoughnessCode(
    TEXT("float2 p = UV * float2(7.0,6.0);\n")
    TEXT("float seed = saturate(Random01);\n")
    TEXT("float macro = 0.5+0.5*sin(p.x*1.17-p.y*1.43+seed*5.7);\n")
    TEXT("float micro = 0.5+0.5*sin(p.x*8.9+p.y*11.1+sin(p.x*3.2));\n")
    TEXT("float pores = pow(saturate(0.5+0.5*sin(p.x*21.7-p.y*18.3+seed*9.1)),8.0);\n")
    TEXT("return clamp(0.74+0.10*micro+0.055*(1.0-macro)+0.045*pores+0.02*(seed-0.5),0.70,0.94);"));
const FString PaverNormalDescription(
    TEXT("TRIAD_EXPLORE_V5B_PAVER_PROCEDURAL_STONE_NORMAL_V1"));
const FString PaverNormalCode(
    TEXT("float2 p = UV * 6.2831853;\n")
    TEXT("float phase = saturate(Random01)*6.2831853;\n")
    TEXT("float dx = 0.042*cos(p.x*4.1+p.y*1.7+phase)+0.019*cos(p.x*13.7-p.y*9.1);\n")
    TEXT("float dy = 0.034*cos(p.y*3.7-p.x*1.3+phase*0.73)-0.017*sin(p.y*12.1+p.x*7.9);\n")
    TEXT("return normalize(float3(-dx,-dy,1.0));"));
const FString PaverSpecularDescription(TEXT("V5B_PAVER_STONE_SPECULAR"));

const TArray<FString>& PachiraPartNames()
{
    static const TArray<FString> Names = {
        TEXT("pachira_aquatica_01_bark_a"),
        TEXT("pachira_aquatica_01_leaves_a"),
        TEXT("pachira_aquatica_01_bark_b"),
        TEXT("pachira_aquatica_01_leaves_b"),
        TEXT("pachira_aquatica_01_bark_c"),
        TEXT("pachira_aquatica_01_leaves_c"),
        TEXT("pachira_aquatica_01_bark_d"),
        TEXT("pachira_aquatica_01_leaves_d")};
    return Names;
}

const TArray<int32>& PachiraTriangleCounts()
{
    // Exact UE 5.5 LOD0 census from the pinned 2,481,564-byte Poly Haven FBX.
    // Keep this per-node contract so an incomplete or differently split import
    // cannot hide behind an approximate aggregate triangle range.
    static const TArray<int32> Counts = {
        82,
        4392,
        231,
        1984,
        1142,
        6792,
        1918,
        12633};
    return Counts;
}

const TArray<FString>& PachiraObjectPaths()
{
    static TArray<FString> Paths;
    if (Paths.IsEmpty())
    {
        for (const FString& Name : PachiraPartNames())
        {
            Paths.Add(ObjectPath(VegetationMeshPath, Name));
        }
    }
    return Paths;
}

struct FTextureSpec
{
    FString SourceName;
    FString AssetName;
    int64 SourceBytes = 0;
    bool bSrgb = false;
    TextureCompressionSettings Compression = TC_Default;
    bool bFlipGreen = false;
};

const TArray<FTextureSpec>& TextureSpecs()
{
    static const TArray<FTextureSpec> Specs = {
        {TEXT("pachira_aquatica_01_bark_diff_2k.png"), TEXT("T_IPV5B_Pachira_Bark_BaseColor"), 11564938, true, TC_Default, false},
        {TEXT("pachira_aquatica_01_bark_nor_gl_2k.png"), TEXT("T_IPV5B_Pachira_Bark_NormalDX"), 11940638, false, TC_Normalmap, true},
        {TEXT("pachira_aquatica_01_bark_rough_2k.png"), TEXT("T_IPV5B_Pachira_Bark_Roughness"), 3486282, false, TC_Masks, false},
        {TEXT("pachira_aquatica_01_leaves_diff_2k.png"), TEXT("T_IPV5B_Pachira_Leaves_BaseColor"), 8481246, true, TC_Default, false},
        {TEXT("pachira_aquatica_01_leaves_nor_gl_2k.png"), TEXT("T_IPV5B_Pachira_Leaves_NormalDX"), 11274169, false, TC_Normalmap, true},
        {TEXT("pachira_aquatica_01_leaves_rough_2k.png"), TEXT("T_IPV5B_Pachira_Leaves_Roughness"), 2251537, false, TC_Masks, false},
        {TEXT("pachira_aquatica_01_leaves_alpha_2k.png"), PachiraLeavesOpacityTextureName, 162415, false, TC_Masks, false},
        {TEXT("T_IPV5B_Bermuda_BaseColorPadded.png"), TEXT("T_IPV5B_Bermuda_BaseColorPadded"), 500787, true, TC_Default, false},
        {TEXT("grass_bermuda_01_alpha_1k.png"), TEXT("T_IPV5B_Bermuda_Opacity"), 59564, false, TC_Masks, false}};
    return Specs;
}

FString ProjectSourcePath(const FString& Relative)
{
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceAssets"), Relative));
}

FString PachiraSourceRoot()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5/Sources/Vegetation/polyhaven/pachira_aquatica_01"));
}

FString PachiraFbxSource()
{
    return FPaths::Combine(PachiraSourceRoot(), TEXT("pachira_aquatica_01_2k.fbx"));
}

FString TextureSource(const FTextureSpec& Spec)
{
    if (Spec.AssetName == TEXT("T_IPV5B_Bermuda_BaseColorPadded"))
    {
        return ProjectSourcePath(TEXT(
            "IstanaPublicViewExploreV5B/Grass/Generated/T_IPV5B_Bermuda_BaseColorPadded.png"));
    }
    if (Spec.AssetName == TEXT("T_IPV5B_Bermuda_Opacity"))
    {
        return ProjectSourcePath(TEXT(
            "IstanaPublicViewExploreV5B/Grass/Sources/PolyHaven/grass_bermuda_01_alpha_1k.png"));
    }
    return FPaths::Combine(PachiraSourceRoot(), TEXT("textures"), Spec.SourceName);
}

FString PropSource(const FString& Name)
{
    return ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV5B/Props/Generated/") + Name + TEXT(".obj"));
}

FString PorticoSource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Portico/Generated/SM_IPV5B_BuildingHero_CrossTrimmed.obj"));
}

FString HardscapeRenderSuccessorSource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Hardscape/Generated/SM_IPV5B_HardscapeRenderSuccessor.obj"));
}

FString HardscapeRenderSuccessorMaterialLibrarySource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Hardscape/Generated/SM_IPV5B_HardscapeRenderSuccessor.mtl"));
}

FString AccentTurfSource(int32 LodIndex = 0)
{
    const FString Suffix = LodIndex > 0
        ? FString::Printf(TEXT("_LOD%d"), LodIndex)
        : FString();
    return ProjectSourcePath(
        TEXT("IstanaPublicViewExploreV5B/Grass/Generated/") +
        AccentTurfName + Suffix + TEXT(".obj"));
}

FString AccentTurfMaterialLibrarySource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Grass/Generated/SM_IPV5B_BermudaTurfCluster.mtl"));
}

FString AccentTurfManifestSource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Grass/Generated/bermuda_turf_cluster.manifest.json"));
}

FString FormalBedSource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Beds/Generated/SM_IPV5B_FormalBedSoilVeneer.obj"));
}

FString TreeBaseMulchSource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Beds/Generated/SM_IPV5B_TreeBaseMulchMound.obj"));
}

FString TreeBaseMulchMaterialLibrarySource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Beds/Generated/SM_IPV5B_TreeBaseMulchMound.mtl"));
}

FString TreeBaseMulchManifestSource()
{
    return ProjectSourcePath(TEXT(
        "IstanaPublicViewExploreV5B/Beds/Generated/tree_base_mulch_mound_manifest.json"));
}

template <typename T>
T* LoadExact(const FString& Path)
{
    T* Object = LoadObject<T>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

bool ValidateSourceBytes(
    const FString& Filename,
    int64 ExpectedBytes,
    FString& OutError)
{
    const int64 ActualBytes = IFileManager::Get().FileSize(*Filename);
    if (ActualBytes != ExpectedBytes)
    {
        OutError = FString::Printf(
            TEXT("V5B source byte contract failed for '%s': expected %lld, actual %lld."),
            *Filename,
            ExpectedBytes,
            ActualBytes);
        return false;
    }
    OutError.Reset();
    return true;
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
            TEXT("V5B source hash admission failed for '%s': expected bytes=%lld actual=%d."),
            *Filename,
            ExpectedBytes,
            Bytes.Num());
        return false;
    }

    FString ActualSha256;
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr)
    {
        OutError = TEXT("V5B source SHA-256 computation failed for '") +
            Filename + TEXT("'.");
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    ActualSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("V5B source SHA-256 admission requires WITH_SSL for '") +
        Filename + TEXT("'.");
    return false;
#endif
    if (ActualSha256 != ExpectedSha256)
    {
        OutError = FString::Printf(
            TEXT("V5B source SHA-256 guard failed for '%s': expected=%s actual=%s."),
            *Filename,
            *ExpectedSha256,
            *ActualSha256);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateTreeBaseMulchSourceHashes(FString& OutError)
{
    return ValidateSourceHash(
               TreeBaseMulchSource(),
               TreeBaseMulchObjBytes,
               TreeBaseMulchObjSha256,
               OutError) &&
        ValidateSourceHash(
               TreeBaseMulchMaterialLibrarySource(),
               TreeBaseMulchMtlBytes,
               TreeBaseMulchMtlSha256,
               OutError) &&
        ValidateSourceHash(
               TreeBaseMulchManifestSource(),
               TreeBaseMulchManifestBytes,
               TreeBaseMulchManifestSha256,
               OutError);
}

bool ValidateAccentTurfSourceHashes(FString& OutError)
{
    return ValidateSourceHash(
               AccentTurfSource(0),
               AccentTurfLod0ObjBytes,
               AccentTurfLod0ObjSha256,
               OutError) &&
        ValidateSourceHash(
               AccentTurfSource(1),
               AccentTurfLod1ObjBytes,
               AccentTurfLod1ObjSha256,
               OutError) &&
        ValidateSourceHash(
               AccentTurfSource(2),
               AccentTurfLod2ObjBytes,
               AccentTurfLod2ObjSha256,
               OutError) &&
        ValidateSourceHash(
               AccentTurfMaterialLibrarySource(),
               AccentTurfMtlBytes,
               AccentTurfMtlSha256,
               OutError) &&
        ValidateSourceHash(
               AccentTurfManifestSource(),
               AccentTurfManifestBytes,
               AccentTurfManifestSha256,
               OutError);
}

TArray<FString> AccentTurfLodImportScratchArtifactCandidates()
{
    const FString PackageFilename = FPackageName::LongPackageNameToFilename(
        AccentTurfLodImportScratchPackageName,
        FPackageName::GetAssetPackageExtension());
    const FString Base = FPaths::Combine(
        FPaths::GetPath(PackageFilename),
        FPaths::GetBaseFilename(PackageFilename));
    return {
        Base + TEXT(".uasset"),
        Base + TEXT(".uexp"),
        Base + TEXT(".ubulk"),
        Base + TEXT(".uptnl"),
        Base + TEXT(".m.ubulk"),
        Base + TEXT(".upayload")};
}

bool IsExactUnpersistedAccentTurfLodImportScratch(const FAssetData& Row)
{
    if (Row.GetObjectPathString() != AccentTurfLodImportScratchObjectPath ||
        Row.PackageName != FName(*AccentTurfLodImportScratchPackageName) ||
        Row.PackagePath != FName(*VegetationMeshPath) ||
        Row.AssetName != FName(TEXT("StaticMesh_0")) ||
        Row.AssetClassPath != UStaticMesh::StaticClass()->GetClassPathName())
    {
        return false;
    }

    const TArray<FString> Artifacts =
        AccentTurfLodImportScratchArtifactCandidates();
    if (Artifacts.Num() != 6)
    {
        return false;
    }
    for (const FString& Artifact : Artifacts)
    {
        if (IFileManager::Get().FileExists(*Artifact))
        {
            // A scratch-named package on disk is namespace drift, not the
            // engine's disposable in-memory LOD-import registry row.
            return false;
        }
    }
    return true;
}

bool GatherRootAssetsUnfiltered(TArray<FAssetData>& OutAssets)
{
    FAssetRegistryModule& Registry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if (Registry.Get().IsLoadingAssets())
    {
        Registry.Get().WaitForCompletion();
    }
    OutAssets.Reset();
    Registry.Get().GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false);
    return !Registry.Get().IsLoadingAssets();
}

bool GatherRootAssets(TArray<FAssetData>& OutAssets)
{
    if (!GatherRootAssetsUnfiltered(OutAssets))
    {
        return false;
    }
    OutAssets.RemoveAll([](const FAssetData& Row)
    {
        return IsExactUnpersistedAccentTurfLodImportScratch(Row);
    });
    return true;
}

class FScopedFreshRollback final
{
public:
    FScopedFreshRollback(TArray<UObject*>& InOutAssets, FString& InOutError)
        : Assets(InOutAssets), Error(InOutError)
    {
    }

    ~FScopedFreshRollback()
    {
        if (bCommitted)
        {
            return;
        }
        TArray<FAssetData> Data;
        // Rollback sees the raw registry so it can dispose of the UE 5.5
        // LOD-import scratch object instead of merely hiding it from the
        // exact persisted-roster validator.
        GatherRootAssetsUnfiltered(Data);
        TArray<UObject*> Disposable;
        for (const FAssetData& Row : Data)
        {
            UObject* Object = Row.GetAsset();
            UPackage* Package = Object ? Object->GetOutermost() : nullptr;
            if (Object && Package && Package->HasAnyPackageFlags(PKG_NewlyCreated) &&
                !FPackageName::DoesPackageExist(Package->GetName()))
            {
                Disposable.AddUnique(Object);
            }
        }
        if (Disposable.Num() > 0 &&
            ObjectTools::DeleteObjectsUnchecked(Disposable) != Disposable.Num())
        {
            if (!Error.IsEmpty())
            {
                Error += TEXT(" ");
            }
            Error += TEXT("V5B_ROLLBACK_INCOMPLETE");
        }
        Assets.Reset();
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
T* AddExpression(UMaterial* Material, const TCHAR* Description, int32 X, int32 Y)
{
    UMaterialEditorOnlyData* EditorOnly = Material ? Material->GetEditorOnlyData() : nullptr;
    T* Expression = EditorOnly
        ? NewObject<T>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
        EditorOnly->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpression* AddExpressionByClassPath(
    UMaterial* Material,
    const TCHAR* ClassPath,
    const TCHAR* Description,
    int32 X,
    int32 Y)
{
    UMaterialEditorOnlyData* EditorOnly = Material ? Material->GetEditorOnlyData() : nullptr;
    UClass* ExpressionClass = EditorOnly
        ? FindObject<UClass>(nullptr, ClassPath)
        : nullptr;
    if (!ExpressionClass && EditorOnly)
    {
        ExpressionClass = LoadObject<UClass>(nullptr, ClassPath);
    }
    UMaterialExpression* Expression =
        ExpressionClass && ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())
        ? NewObject<UMaterialExpression>(
              Material, ExpressionClass, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = X;
        Expression->MaterialExpressionEditorY = Y;
        EditorOnly->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpressionConstant3Vector* AddColor(
    UMaterial* Material,
    const TCHAR* Description,
    const FLinearColor& Color,
    int32 X,
    int32 Y)
{
    UMaterialExpressionConstant3Vector* Node =
        AddExpression<UMaterialExpressionConstant3Vector>(Material, Description, X, Y);
    if (Node)
    {
        Node->Constant = Color;
    }
    return Node;
}

UMaterialExpressionScalarParameter* AddScalar(
    UMaterial* Material,
    const TCHAR* Description,
    const TCHAR* Name,
    float Value,
    int32 X,
    int32 Y)
{
    UMaterialExpressionScalarParameter* Node =
        AddExpression<UMaterialExpressionScalarParameter>(Material, Description, X, Y);
    if (Node)
    {
        Node->ParameterName = Name;
        Node->Group = TEXT("Istana Explore V5B");
        Node->DefaultValue = Value;
    }
    return Node;
}

UMaterialExpressionTextureSampleParameter2D* AddTexture(
    UMaterial* Material,
    const TCHAR* Description,
    const TCHAR* Name,
    UTexture2D* Texture,
    EMaterialSamplerType Sampler,
    int32 X,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Node =
        AddExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, Description, X, Y);
    if (Node)
    {
        Node->ParameterName = Name;
        Node->Group = TEXT("Istana Explore V5B");
        Node->Texture = Texture;
        Node->SamplerType = Sampler;
        Node->SamplerSource = SSM_FromTextureAsset;
    }
    return Node;
}

UMaterial* CreateEmptyMaterial(IAssetTools& AssetTools, const FString& Name, FString& OutError)
{
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              Name,
              MaterialPath,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5BAssets"))))
        : nullptr;
    if (!Material || !Material->GetEditorOnlyData())
    {
        OutError = TEXT("Could not create a V5B material: ") + Name;
        return nullptr;
    }
    Material->Modify();
    Material->MaterialDomain = MD_Surface;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->bUsedWithInstancedStaticMeshes = true;
    return Material;
}

void FinalizeMaterial(UMaterial* Material)
{
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
}

UMaterial* CreateAccentTurfMaterial(
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(AssetTools, AccentMaterialName, OutError);
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    UMaterialExpressionTextureCoordinate* BladeUv =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"), -1320, -340);
    UMaterialExpression* InstanceRandom = AddExpressionByClassPath(
        Material,
        TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"),
        TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"),
        -1320,
        -180);
    UMaterialExpressionCustom* BladeColor =
        AddExpression<UMaterialExpressionCustom>(
            Material, *AccentBladeColorDescription, -1040, -340);
    UMaterialExpressionConstant3Vector* WorldUpNormal = AddColor(
        Material,
        TEXT("V5B_BERMUDA_WORLD_UP_NORMAL"),
        FLinearColor(0.0f, 0.0f, 1.0f),
        -760,
        -180);
    UMaterialExpressionConstant3Vector* Subsurface = AddColor(
        Material,
        TEXT("V5B_BERMUDA_LIT_SUBSURFACE"),
        FLinearColor(0.058f, 0.145f, 0.038f),
        -760,
        -420);
    UMaterialExpressionScalarParameter* Roughness = AddScalar(
        Material, TEXT("V5B_BERMUDA_HIGH_ROUGHNESS"), TEXT("Roughness"),
        0.80f, -760, -60);
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material, TEXT("V5B_BERMUDA_LOW_SPECULAR"), TEXT("Specular"),
        0.30f, -760, 40);
    UMaterialExpression* InstanceFade = AddExpressionByClassPath(
        Material,
        TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"),
        TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"),
        -760,
        260);
    UMaterialExpressionVertexNormalWS* ImportedCurvatureNormal =
        AddExpression<UMaterialExpressionVertexNormalWS>(
            Material,
            TEXT("V5B_BERMUDA_IMPORTED_CURVATURE_NORMAL_WS"),
            -1040,
            -100);
    UMaterialExpressionTwoSidedSign* TwoSidedSign =
        AddExpression<UMaterialExpressionTwoSidedSign>(
            Material,
            TEXT("V5B_BERMUDA_TWO_SIDED_SIGN"),
            -1040,
            0);
    UMaterialExpressionMultiply* FacingCorrectedNormal =
        AddExpression<UMaterialExpressionMultiply>(
            Material,
            TEXT("V5B_BERMUDA_FACING_CORRECTED_NORMAL"),
            -840,
            -100);
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material,
            TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"),
            -560,
            -180);
    UMaterialFunctionInterface* DitherTemporalAaFunction =
        LoadExact<UMaterialFunctionInterface>(DitherTemporalAaFunctionPath);
    UMaterialExpressionMaterialFunctionCall* DitheredInstanceFade =
        AddExpression<UMaterialExpressionMaterialFunctionCall>(
            Material,
            TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"),
            -600,
            260);
    int32 DitherAlphaInputIndex = INDEX_NONE;
    if (DitheredInstanceFade && DitherTemporalAaFunction &&
        DitheredInstanceFade->SetMaterialFunction(DitherTemporalAaFunction))
    {
        for (int32 InputIndex = 0;
             InputIndex < DitheredInstanceFade->FunctionInputs.Num();
             ++InputIndex)
        {
            if (DitheredInstanceFade->GetInputName(InputIndex)
                    .ToString()
                    .StartsWith(TEXT("Alpha Threshold")))
            {
                DitherAlphaInputIndex = InputIndex;
                break;
            }
        }
    }
    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("V5B_BERMUDA_WORLD_POSITION"), -1320, 440);
    UMaterialExpressionTransformPosition* InstanceLocalPosition =
        AddExpression<UMaterialExpressionTransformPosition>(
            Material, TEXT("V5B_BERMUDA_INSTANCE_LOCAL_POSITION"), -1040, 440);
    UMaterialExpressionTime* Time = AddExpression<UMaterialExpressionTime>(
        Material, TEXT("V5B_BERMUDA_TIME"), -1040, 600);
    UMaterialExpressionScalarParameter* Strength = AddScalar(
        Material, TEXT("V5B_BERMUDA_WIND_STRENGTH"),
        TEXT("TRIAD_WindStrengthCm"), 0.48f, -780, 400);
    UMaterialExpressionScalarParameter* Speed = AddScalar(
        Material, TEXT("V5B_BERMUDA_WIND_SPEED"),
        TEXT("TRIAD_WindSpeed"), 0.12f, -780, 500);
    UMaterialExpressionScalarParameter* Height = AddScalar(
        Material, TEXT("V5B_BERMUDA_WIND_HEIGHT"),
        TEXT("TRIAD_WindHeightCm"),
        AccentFrozenMaterialWindHeightNormalizerCm,
        -780,
        600);
    UMaterialExpressionScalarParameter* Response = AddScalar(
        Material, TEXT("V5B_BERMUDA_WIND_RESPONSE"),
        TEXT("TRIAD_WindResponseScale"), 0.28f, -780, 700);
    UMaterialExpressionScalarParameter* MaximumWpo = AddScalar(
        Material, TEXT("V5B_BERMUDA_MAX_WPO"),
        TEXT("TRIAD_MaxWpoCm"), 0.55f, -780, 800);
    UMaterialExpressionVectorParameter* Direction =
        AddExpression<UMaterialExpressionVectorParameter>(
            Material, TEXT("V5B_BERMUDA_WIND_DIRECTION"), -780, 900);
    UMaterialExpressionCustom* Wind =
        AddExpression<UMaterialExpressionCustom>(
            Material, *AccentWindDescription, -400, 590);

    if (!Data || !BladeUv || !InstanceRandom || !BladeColor ||
        !WorldUpNormal || !Subsurface || !Roughness || !Specular ||
        !InstanceFade || !ImportedCurvatureNormal || !TwoSidedSign ||
        !FacingCorrectedNormal || !DistanceMatchedNormal ||
        !DitheredInstanceFade ||
        DitherAlphaInputIndex == INDEX_NONE ||
        !WorldPosition || !InstanceLocalPosition || !Time || !Strength ||
        !Speed || !Height || !Response || !MaximumWpo || !Direction || !Wind)
    {
        OutError = TEXT("Could not allocate the complete V5B modeled-blade turf material graph.");
        return nullptr;
    }

    BladeUv->CoordinateIndex = 0;
    BladeUv->UTiling = 1.0f;
    BladeUv->VTiling = 1.0f;
    BladeColor->Description = AccentBladeColorDescription;
    BladeColor->Code = AccentBladeColorCode;
    BladeColor->OutputType = CMOT_Float3;
    BladeColor->Inputs.SetNum(2);
    BladeColor->Inputs[0].InputName = TEXT("BladeUV");
    BladeColor->Inputs[0].Input.Connect(0, BladeUv);
    BladeColor->Inputs[1].InputName = TEXT("Random01");
    BladeColor->Inputs[1].Input.Connect(0, InstanceRandom);
    FacingCorrectedNormal->A.Connect(0, ImportedCurvatureNormal);
    FacingCorrectedNormal->B.Connect(0, TwoSidedSign);
    DistanceMatchedNormal->A.Connect(0, WorldUpNormal);
    DistanceMatchedNormal->B.Connect(0, FacingCorrectedNormal);
    DistanceMatchedNormal->Alpha.Connect(0, InstanceFade);
    DitheredInstanceFade->FunctionInputs[DitherAlphaInputIndex].Input.Connect(
        0,
        InstanceFade);

    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    InstanceLocalPosition->TransformSourceType = TRANSFORMPOSSOURCE_World;
    InstanceLocalPosition->TransformType = TRANSFORMPOSSOURCE_Instance;
    InstanceLocalPosition->Input.Connect(0, WorldPosition);
    Direction->ParameterName = TEXT("TRIAD_WindDirection");
    Direction->Group = TEXT("Istana Explore V5B");
    Direction->DefaultValue = FLinearColor(0.93f, 0.37f, 0.0f, 0.0f);
    Wind->Description = AccentWindDescription;
    Wind->OutputType = CMOT_Float3;
    Wind->Code = AccentWindCode;
    Wind->Inputs.SetNum(9);
    Wind->Inputs[0].InputName = TEXT("WorldPosition");
    Wind->Inputs[0].Input.Connect(0, WorldPosition);
    Wind->Inputs[1].InputName = TEXT("InstanceLocalPosition");
    Wind->Inputs[1].Input.Connect(0, InstanceLocalPosition);
    Wind->Inputs[2].InputName = TEXT("BladeUV");
    Wind->Inputs[2].Input.Connect(0, BladeUv);
    Wind->Inputs[3].InputName = TEXT("TimeSeconds");
    Wind->Inputs[3].Input.Connect(0, Time);
    Wind->Inputs[4].InputName = TEXT("WindStrengthCm");
    Wind->Inputs[4].Input.Connect(0, Strength);
    Wind->Inputs[5].InputName = TEXT("WindSpeed");
    Wind->Inputs[5].Input.Connect(0, Speed);
    Wind->Inputs[6].InputName = TEXT("WindDirection");
    Wind->Inputs[6].Input.Connect(0, Direction);
    Wind->Inputs[7].InputName = TEXT("HeightCm");
    Wind->Inputs[7].Input.Connect(0, Height);
    Wind->Inputs[8].InputName = TEXT("ResponseScale");
    Wind->Inputs[8].Input.Connect(0, Response);
    FCustomInput& MaxWpoInput = Wind->Inputs.AddDefaulted_GetRef();
    MaxWpoInput.InputName = TEXT("MaxWpoCm");
    MaxWpoInput.Input.Connect(0, MaximumWpo);
    FCustomInput& RandomInput = Wind->Inputs.AddDefaulted_GetRef();
    RandomInput.InputName = TEXT("Random01");
    RandomInput.Input.Connect(0, InstanceRandom);
    FCustomInput& InstanceFadeInput = Wind->Inputs.AddDefaulted_GetRef();
    InstanceFadeInput.InputName = TEXT("InstanceFade");
    InstanceFadeInput.Input.Connect(0, InstanceFade);

    Data->BaseColor.Connect(0, BladeColor);
    Data->Normal.Connect(0, DistanceMatchedNormal);
    Data->Roughness.Connect(0, Roughness);
    Data->Specular.Connect(0, Specular);
    Data->OpacityMask.Connect(0, DitheredInstanceFade);
    Data->SubsurfaceColor.Connect(0, Subsurface);
    Data->WorldPositionOffset.Connect(0, Wind);
    Material->BlendMode = BLEND_Masked;
    Material->OpacityMaskClipValue = 0.50f;
    Material->TwoSided = true;
    Material->SetShadingModel(MSM_TwoSidedFoliage);
    Material->bTangentSpaceNormal = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.55f;
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

UMaterial* CreateFormalBedSoilMaterial(
    IAssetTools& AssetTools,
    FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(
        AssetTools, FormalBedMaterialName, OutError);
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("V5B_FORMAL_BED_WORLD_POSITION"), -900, 0);
    UMaterialExpressionTextureCoordinate* ApronUv =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_FORMAL_BED_APRON_FRACTION_UV0"), -900, -120);
    UMaterialExpressionCustom* Base = AddExpression<UMaterialExpressionCustom>(
        Material, *FormalBedBaseDescription, -600, -180);
    UMaterialExpressionCustom* Roughness =
        AddExpression<UMaterialExpressionCustom>(
            Material, *FormalBedRoughnessDescription, -600, 20);
    UMaterialExpressionCustom* Normal =
        AddExpression<UMaterialExpressionCustom>(
            Material, *FormalBedNormalDescription, -600, 120);
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material,
        TEXT("V5B_FORMAL_BED_SOIL_SPECULAR"),
        TEXT("Specular"),
        0.08f,
        -320,
        220);
    if (!Data || !WorldPosition || !ApronUv || !Base || !Roughness || !Normal ||
        !Specular)
    {
        OutError = TEXT("Could not allocate the complete V5B procedural formal-bed soil graph.");
        return nullptr;
    }
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    ApronUv->CoordinateIndex = 0;
    ApronUv->UTiling = 1.0f;
    ApronUv->VTiling = 1.0f;
    Base->Description = FormalBedBaseDescription;
    Base->Code = FormalBedBaseCode;
    Base->OutputType = CMOT_Float3;
    Base->Inputs.SetNum(2);
    Base->Inputs[0].InputName = TEXT("WorldPosition");
    Base->Inputs[0].Input.Connect(0, WorldPosition);
    Base->Inputs[1].InputName = TEXT("ApronUV");
    Base->Inputs[1].Input.Connect(0, ApronUv);
    const auto ConfigureWorldCustom = [WorldPosition](
        UMaterialExpressionCustom* Custom,
        const FString& Description,
        const FString& Code,
        ECustomMaterialOutputType OutputType)
    {
        Custom->Description = Description;
        Custom->Code = Code;
        Custom->OutputType = OutputType;
        Custom->Inputs.SetNum(1);
        Custom->Inputs[0].InputName = TEXT("WorldPosition");
        Custom->Inputs[0].Input.Connect(0, WorldPosition);
    };
    ConfigureWorldCustom(
        Roughness,
        FormalBedRoughnessDescription,
        FormalBedRoughnessCode,
        CMOT_Float1);
    ConfigureWorldCustom(
        Normal,
        FormalBedNormalDescription,
        FormalBedNormalCode,
        CMOT_Float3);
    Data->BaseColor.Connect(0, Base);
    Data->Normal.Connect(0, Normal);
    Data->Roughness.Connect(0, Roughness);
    Data->Specular.Connect(0, Specular);
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

UMaterial* CreateWaterMaterial(IAssetTools& AssetTools, FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(AssetTools, SurfaceMaterialName, OutError);
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    UMaterialExpressionScalarParameter* Roughness = AddScalar(
        Material, TEXT("V5B_WATER_ROUGHNESS"), TEXT("Roughness"), 0.26f, -680, 20);
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material, TEXT("V5B_WATER_SPECULAR"), TEXT("Specular"), 0.40f, -680, 100);
    UMaterialExpressionTextureCoordinate* Uv =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, TEXT("V5B_WATER_UV"), -900, 260);
    UMaterialExpressionTime* Time =
        AddExpression<UMaterialExpressionTime>(Material, TEXT("V5B_WATER_TIME"), -900, 340);
    UMaterialExpressionCustom* Absorption =
        AddExpression<UMaterialExpressionCustom>(Material, TEXT("V5B_WATER_RADIAL_ABSORPTION"), -650, -260);
    UMaterialExpressionConstant3Vector* SkyTint = AddColor(
        Material, TEXT("V5B_WATER_GRAZING_SKY_TINT"), FLinearColor(0.16f, 0.26f, 0.28f), -320, -180);
    UMaterialExpressionFresnel* Fresnel =
        AddExpression<UMaterialExpressionFresnel>(Material, TEXT("V5B_WATER_VIEW_FRESNEL"), -360, -60);
    UMaterialExpressionConstant* FresnelStrength =
        AddExpression<UMaterialExpressionConstant>(Material, TEXT("V5B_WATER_FRESNEL_STRENGTH"), -360, 30);
    UMaterialExpressionMultiply* FresnelAlpha =
        AddExpression<UMaterialExpressionMultiply>(Material, TEXT("V5B_WATER_BOUNDED_FRESNEL"), -120, -50);
    UMaterialExpressionLinearInterpolate* ViewColor =
        AddExpression<UMaterialExpressionLinearInterpolate>(Material, TEXT("V5B_WATER_ABSORPTION_PLUS_SKY"), 100, -180);
    UMaterialExpressionCustom* Normal =
        AddExpression<UMaterialExpressionCustom>(Material, TEXT("V5B_WATER_MICRO_NORMAL_NO_REFRACTION"), -560, 300);
    if (!Data || !Roughness || !Specular || !Uv || !Time || !Absorption ||
        !SkyTint || !Fresnel || !FresnelStrength || !FresnelAlpha ||
        !ViewColor || !Normal)
    {
        OutError = TEXT("Could not allocate the V5B opaque-water graph.");
        return nullptr;
    }
    Absorption->Description = TEXT("Radial shallow/deep absorption with low-amplitude moving color breakup");
    Absorption->OutputType = CMOT_Float3;
    Absorption->Code = TEXT(
        "float edge = smoothstep(0.68, 1.0, UV.y);\n"
        "float island = 1.0-smoothstep(0.0, 0.20, UV.y);\n"
        "float macro = 0.5+0.5*sin((UV.x*5.0+UV.y*3.0+TimeSeconds*0.035)*6.28318);\n"
        "float3 deep = float3(0.010,0.038,0.043);\n"
        "float3 shallow = float3(0.028,0.078,0.076);\n"
        "float shallowWeight = saturate(max(edge,island)*0.70 + macro*0.08);\n"
        "return lerp(deep,shallow,shallowWeight);");
    Absorption->Inputs.SetNum(2);
    Absorption->Inputs[0].InputName = TEXT("UV");
    Absorption->Inputs[0].Input.Connect(0, Uv);
    Absorption->Inputs[1].InputName = TEXT("TimeSeconds");
    Absorption->Inputs[1].Input.Connect(0, Time);
    Fresnel->Exponent = 4.8f;
    Fresnel->BaseReflectFraction = 0.035f;
    FresnelStrength->R = 0.46f;
    FresnelAlpha->A.Connect(0, Fresnel);
    FresnelAlpha->B.Connect(0, FresnelStrength);
    ViewColor->A.Connect(0, Absorption);
    ViewColor->B.Connect(0, SkyTint);
    ViewColor->Alpha.Connect(0, FresnelAlpha);
    Normal->Description = TEXT("Bounded dual-scale fountain ripples; deliberately no refraction input");
    Normal->OutputType = CMOT_Float3;
    Normal->Code = TEXT(
        "float2 p = UV * float2(16.0, 16.0);\n"
        "float2 q = UV * float2(27.0, 21.0);\n"
        "float2 s1 = float2(sin((p.x+p.y*0.37)+TimeSeconds*1.35), cos((p.y-p.x*0.29)-TimeSeconds*1.05));\n"
        "float2 s2 = float2(cos((q.x-q.y*0.41)-TimeSeconds*0.72), sin((q.y+q.x*0.33)+TimeSeconds*0.84));\n"
        "return normalize(float3((s1*0.030+s2*0.018), 1.0));");
    Normal->Inputs.SetNum(2);
    Normal->Inputs[0].InputName = TEXT("UV");
    Normal->Inputs[0].Input.Connect(0, Uv);
    Normal->Inputs[1].InputName = TEXT("TimeSeconds");
    Normal->Inputs[1].Input.Connect(0, Time);
    Data->BaseColor.Connect(0, ViewColor);
    Data->Roughness.Connect(0, Roughness);
    Data->Specular.Connect(0, Specular);
    Data->Normal.Connect(0, Normal);
    Material->BlendMode = BLEND_Opaque;
    Material->bTangentSpaceNormal = true;
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

UMaterial* CreateTranslucentWaterDetailMaterial(
    IAssetTools& AssetTools,
    const FString& Name,
    bool bSpray,
    FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(AssetTools, Name, OutError);
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    UMaterialExpressionConstant3Vector* Color = AddColor(
        Material,
        bSpray ? TEXT("V5B_SPRAY_COLOR") : TEXT("V5B_FOAM_COLOR"),
        bSpray ? FLinearColor(0.70f, 0.80f, 0.82f) : FLinearColor(0.79f, 0.88f, 0.86f),
        -600,
        -160);
    UMaterialExpressionTextureCoordinate* Uv =
        AddExpression<UMaterialExpressionTextureCoordinate>(Material, TEXT("V5B_DETAIL_UV"), -900, 120);
    UMaterialExpressionTime* Time =
        AddExpression<UMaterialExpressionTime>(Material, TEXT("V5B_DETAIL_TIME"), -900, 220);
    UMaterialExpressionCustom* Opacity =
        AddExpression<UMaterialExpressionCustom>(Material, TEXT("V5B_DETAIL_BREAKUP"), -580, 160);
    UMaterialExpressionConstant* Roughness =
        AddExpression<UMaterialExpressionConstant>(Material, TEXT("V5B_DETAIL_ROUGHNESS"), -500, 340);
    if (!Data || !Color || !Uv || !Time || !Opacity || !Roughness)
    {
        OutError = TEXT("Could not allocate a V5B foam/spray graph.");
        return nullptr;
    }
    Roughness->R = bSpray ? 0.28f : 0.35f;
    Opacity->Description = TEXT("Deterministic moving analytic breakup; no refraction or scene-color distortion");
    Opacity->OutputType = CMOT_Float1;
    Opacity->Code = bSpray
        ? TEXT(
              "float streak = 0.5 + 0.5*sin((UV.y*18.0-TimeSeconds*1.7)*6.28318 + sin(UV.x*6.28318)*2.2);\n"
              "float breakup = 0.5 + 0.5*sin((UV.x*11.0+UV.y*7.0+TimeSeconds*0.9)*6.28318);\n"
              "float tip = 1.0-smoothstep(0.80,1.0,UV.y);\n"
              "return saturate((0.035+0.24*streak*breakup)*tip);")
        : TEXT(
              "float a = 0.5+0.5*sin((UV.x*23.0+TimeSeconds*0.28)*6.28318);\n"
              "float b = 0.5+0.5*sin((UV.x*41.0-TimeSeconds*0.17)*6.28318);\n"
              "return saturate(0.10+0.30*a*b);");
    Opacity->Inputs.SetNum(2);
    Opacity->Inputs[0].InputName = TEXT("UV");
    Opacity->Inputs[0].Input.Connect(0, Uv);
    Opacity->Inputs[1].InputName = TEXT("TimeSeconds");
    Opacity->Inputs[1].Input.Connect(0, Time);
    Data->BaseColor.Connect(0, Color);
    Data->Roughness.Connect(0, Roughness);
    Data->Opacity.Connect(0, Opacity);
    Material->BlendMode = BLEND_Translucent;
    Material->TwoSided = true;
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

bool ValidatePachiraMaterialGraph(
    UMaterial* Material,
    bool bLeaves,
    FString& OutError);

UMaterial* CreatePachiraMaterial(
    IAssetTools& AssetTools,
    bool bLeaves,
    const TMap<FString, UTexture2D*>& Textures,
    FString& OutError)
{
    const FString Name = bLeaves ? LeavesMaterialName : BarkMaterialName;
    UMaterial* Material = CreateEmptyMaterial(AssetTools, Name, OutError);
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    const FString Prefix = bLeaves ? TEXT("T_IPV5B_Pachira_Leaves_") : TEXT("T_IPV5B_Pachira_Bark_");
    UTexture2D* BaseTexture = Textures.FindRef(Prefix + TEXT("BaseColor"));
    UTexture2D* NormalTexture = Textures.FindRef(Prefix + TEXT("NormalDX"));
    UTexture2D* RoughTexture = Textures.FindRef(Prefix + TEXT("Roughness"));
    UTexture2D* OpacityTexture = bLeaves
        ? Textures.FindRef(PachiraLeavesOpacityTextureName)
        : nullptr;
    UMaterialExpressionTextureSampleParameter2D* Base = AddTexture(
        Material, TEXT("V5B_PACHIRA_BASE"), TEXT("BaseColorTexture"), BaseTexture, SAMPLERTYPE_Color, -700, -260);
    UMaterialExpressionTextureSampleParameter2D* Normal = AddTexture(
        Material, TEXT("V5B_PACHIRA_NORMAL_DX"), TEXT("NormalTexture"), NormalTexture, SAMPLERTYPE_Normal, -700, -80);
    UMaterialExpressionTextureSampleParameter2D* Roughness = AddTexture(
        Material, TEXT("V5B_PACHIRA_ROUGHNESS"), TEXT("RoughnessTexture"), RoughTexture, SAMPLERTYPE_Masks, -700, 100);
    UMaterialExpressionTextureSampleParameter2D* Opacity = bLeaves
        ? AddTexture(Material, TEXT("V5B_PACHIRA_OPACITY"), TEXT("OpacityTexture"), OpacityTexture, SAMPLERTYPE_Masks, -700, 280)
        : nullptr;
    UMaterialExpression* InstanceRandom = AddExpressionByClassPath(
        Material,
        TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"),
        TEXT("V5B_PACHIRA_PER_INSTANCE_RANDOM"),
        -700,
        460);
    UMaterialExpressionCustom* VariedLeafColor = bLeaves
        ? AddExpression<UMaterialExpressionCustom>(
              Material, *PachiraLeafColorDescription, -420, -300)
        : nullptr;
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material,
        TEXT("V5B_PACHIRA_LOW_SPECULAR"),
        TEXT("Specular"),
        bLeaves ? PachiraLeavesSpecular : PachiraBarkSpecular,
        -360,
        100);

    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("V5B_PACHIRA_WORLD_POSITION"), -700, 640);
    UMaterialExpressionTransformPosition* InstanceLocalPosition =
        AddExpression<UMaterialExpressionTransformPosition>(
            Material, TEXT("V5B_PACHIRA_INSTANCE_LOCAL_POSITION"), -440, 640);
    UMaterialExpressionTime* Time = AddExpression<UMaterialExpressionTime>(
        Material, TEXT("V5B_PACHIRA_TIME"), -440, 800);
    UMaterialExpressionScalarParameter* Strength = AddScalar(
        Material, TEXT("V5B_PACHIRA_WIND_STRENGTH"),
        TEXT("TRIAD_WindStrengthCm"), PachiraWindStrengthCm, -180, 520);
    UMaterialExpressionScalarParameter* Speed = AddScalar(
        Material, TEXT("V5B_PACHIRA_WIND_SPEED"),
        TEXT("TRIAD_WindSpeed"), PachiraWindSpeed, -180, 620);
    UMaterialExpressionScalarParameter* Height = AddScalar(
        Material, TEXT("V5B_PACHIRA_WIND_HEIGHT"),
        TEXT("TRIAD_WindHeightCm"), PachiraWindHeightCm, -180, 720);
    UMaterialExpressionScalarParameter* Response = AddScalar(
        Material, TEXT("V5B_PACHIRA_WIND_RESPONSE"),
        TEXT("TRIAD_WindResponseScale"),
        bLeaves ? PachiraLeavesWindResponse : PachiraBarkWindResponse,
        -180,
        820);
    UMaterialExpressionScalarParameter* MaximumWpo = AddScalar(
        Material, TEXT("V5B_PACHIRA_MAX_WPO"),
        TEXT("TRIAD_MaxWpoCm"),
        bLeaves ? PachiraLeavesMaximumWpoCm : PachiraBarkMaximumWpoCm,
        -180,
        920);
    UMaterialExpressionVectorParameter* Direction =
        AddExpression<UMaterialExpressionVectorParameter>(
            Material, TEXT("V5B_PACHIRA_WIND_DIRECTION"), -180, 1020);
    UMaterialExpressionCustom* Wind =
        AddExpression<UMaterialExpressionCustom>(
            Material, *PachiraWindDescription, 120, 760);

    UMaterialExpressionConstant2Vector* TangentXY = bLeaves
        ? AddExpression<UMaterialExpressionConstant2Vector>(
              Material, TEXT("V5B_PACHIRA_TWO_SIDED_TANGENT_XY"), -420, -80)
        : nullptr;
    UMaterialExpressionTwoSidedSign* TwoSidedSign = bLeaves
        ? AddExpression<UMaterialExpressionTwoSidedSign>(
              Material, TEXT("V5B_PACHIRA_TWO_SIDED_SIGN"), -420, 10)
        : nullptr;
    UMaterialExpressionAppendVector* Facing = bLeaves
        ? AddExpression<UMaterialExpressionAppendVector>(
              Material, TEXT("V5B_PACHIRA_TWO_SIDED_FACING"), -200, -40)
        : nullptr;
    UMaterialExpressionMultiply* CorrectedNormal = bLeaves
        ? AddExpression<UMaterialExpressionMultiply>(
              Material, TEXT("V5B_PACHIRA_TWO_SIDED_NORMAL"), 20, -80)
        : nullptr;
    UMaterialExpressionConstant3Vector* SubsurfaceAttenuation = bLeaves
        ? AddColor(
              Material,
              TEXT("V5B_PACHIRA_SUBSURFACE_ATTENUATION"),
              PachiraLeavesSubsurfaceAttenuation,
              -180,
              180)
        : nullptr;
    UMaterialExpressionMultiply* SubsurfaceColor = bLeaves
        ? AddExpression<UMaterialExpressionMultiply>(
              Material, TEXT("V5B_PACHIRA_ATTENUATED_SUBSURFACE"), 40, 180)
        : nullptr;

    if (!Data || !Base || !Normal || !Roughness || !InstanceRandom ||
        !Specular || !WorldPosition || !InstanceLocalPosition || !Time ||
        !Strength || !Speed || !Height || !Response || !MaximumWpo ||
        !Direction || !Wind ||
        (bLeaves &&
         (!Opacity || !VariedLeafColor || !TangentXY || !TwoSidedSign ||
          !Facing || !CorrectedNormal || !SubsurfaceAttenuation ||
          !SubsurfaceColor)))
    {
        OutError = TEXT("A V5B Pachira PBR, two-sided foliage, variation, or wind node is absent.");
        return nullptr;
    }

    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    InstanceLocalPosition->TransformSourceType = TRANSFORMPOSSOURCE_World;
    InstanceLocalPosition->TransformType = TRANSFORMPOSSOURCE_Instance;
    InstanceLocalPosition->Input.Connect(0, WorldPosition);
    Direction->ParameterName = TEXT("TRIAD_WindDirection");
    Direction->Group = TEXT("Istana Explore V5B");
    Direction->DefaultValue = FLinearColor(0.93f, 0.37f, 0.0f, 0.0f);
    Wind->Description = PachiraWindDescription;
    Wind->Code = PachiraWindCode;
    Wind->OutputType = CMOT_Float3;
    Wind->Inputs.SetNum(10);
    const FName WindInputNames[] = {
        TEXT("WorldPosition"),
        TEXT("InstanceLocalPosition"),
        TEXT("TimeSeconds"),
        TEXT("WindStrengthCm"),
        TEXT("WindSpeed"),
        TEXT("WindDirection"),
        TEXT("HeightCm"),
        TEXT("ResponseScale"),
        TEXT("MaxWpoCm"),
        TEXT("Random01")};
    UMaterialExpression* WindInputExpressions[] = {
        WorldPosition,
        InstanceLocalPosition,
        Time,
        Strength,
        Speed,
        Direction,
        Height,
        Response,
        MaximumWpo,
        InstanceRandom};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(WindInputNames); ++Index)
    {
        Wind->Inputs[Index].InputName = WindInputNames[Index];
        Wind->Inputs[Index].Input.Connect(0, WindInputExpressions[Index]);
    }

    if (bLeaves)
    {
        Data->BaseColor.Connect(0, VariedLeafColor);
        Data->Normal.Connect(0, CorrectedNormal);
    }
    else
    {
        Data->BaseColor.Connect(0, Base);
        Data->Normal.Connect(0, Normal);
    }
    Data->Roughness.Connect(1, Roughness);
    Data->Specular.Connect(0, Specular);
    Data->WorldPositionOffset.Connect(0, Wind);
    Material->MaxWorldPositionOffsetDisplacement =
        bLeaves ? PachiraLeavesMaximumWpoCm : PachiraBarkMaximumWpoCm;
    if (bLeaves)
    {
        VariedLeafColor->Description = PachiraLeafColorDescription;
        VariedLeafColor->Code = PachiraLeafColorCode;
        VariedLeafColor->OutputType = CMOT_Float3;
        VariedLeafColor->Inputs.SetNum(2);
        VariedLeafColor->Inputs[0].InputName = TEXT("BaseColor");
        VariedLeafColor->Inputs[0].Input.Connect(0, Base);
        VariedLeafColor->Inputs[1].InputName = TEXT("Random01");
        VariedLeafColor->Inputs[1].Input.Connect(0, InstanceRandom);

        TangentXY->R = 1.0f;
        TangentXY->G = 1.0f;
        Facing->A.Connect(0, TangentXY);
        Facing->B.Connect(0, TwoSidedSign);
        CorrectedNormal->A.Connect(0, Normal);
        CorrectedNormal->B.Connect(0, Facing);
        SubsurfaceColor->A.Connect(0, VariedLeafColor);
        SubsurfaceColor->B.Connect(0, SubsurfaceAttenuation);

        Data->OpacityMask.Connect(1, Opacity);
        Data->SubsurfaceColor.Connect(0, SubsurfaceColor);
        Material->BlendMode = BLEND_Masked;
        Material->OpacityMaskClipValue = PachiraOpacityClipValue;
        Material->TwoSided = true;
        Material->SetShadingModel(MSM_TwoSidedFoliage);
    }
    FinalizeMaterial(Material);
    if (!ValidatePachiraMaterialGraph(Material, bLeaves, OutError))
    {
        return nullptr;
    }
    OutError.Reset();
    return Material;
}

bool ValidatePachiraMaterialGraph(
    UMaterial* Material,
    bool bLeaves,
    FString& OutError)
{
    const UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    const FString ExpectedMaterialPath =
        bLeaves ? LeavesMaterialObjectPath : BarkMaterialObjectPath;
    const EBlendMode ExpectedBlendMode =
        bLeaves ? BLEND_Masked : BLEND_Opaque;
    const EMaterialShadingModel ExpectedShadingModel =
        bLeaves ? MSM_TwoSidedFoliage : MSM_DefaultLit;
    const float ExpectedSpecular =
        bLeaves ? PachiraLeavesSpecular : PachiraBarkSpecular;
    const float ExpectedResponse =
        bLeaves ? PachiraLeavesWindResponse : PachiraBarkWindResponse;
    const float ExpectedMaximumWpo =
        bLeaves ? PachiraLeavesMaximumWpoCm : PachiraBarkMaximumWpoCm;
    const int32 ExpectedExpressionCount = bLeaves ? 23 : 15;

    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != ExpectedMaterialPath || !Data ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != ExpectedBlendMode ||
        Material->TwoSided != bLeaves || !Material->bTangentSpaceNormal ||
        Material->bUseMaterialAttributes ||
        !Material->bUsedWithInstancedStaticMeshes ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            ExpectedShadingModel) ||
        !FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            ExpectedMaximumWpo,
            0.000001f) ||
        (bLeaves && !FMath::IsNearlyEqual(
            Material->OpacityMaskClipValue,
            PachiraOpacityClipValue,
            0.000001f)) ||
        Data->ExpressionCollection.Expressions.Num() !=
            ExpectedExpressionCount)
    {
        OutError = FString::Printf(
            TEXT("V5B Pachira %s lost its exact surface/instancing/foliage/WPO material policy."),
            bLeaves ? TEXT("leaves") : TEXT("bark"));
        return false;
    }

    const UMaterialExpressionTextureSampleParameter2D* Base = nullptr;
    const UMaterialExpressionTextureSampleParameter2D* Normal = nullptr;
    const UMaterialExpressionTextureSampleParameter2D* Roughness = nullptr;
    const UMaterialExpressionTextureSampleParameter2D* Opacity = nullptr;
    const UMaterialExpression* InstanceRandom = nullptr;
    const UMaterialExpressionScalarParameter* Specular = nullptr;
    const UMaterialExpressionWorldPosition* WorldPosition = nullptr;
    const UMaterialExpressionTransformPosition* InstanceLocalPosition = nullptr;
    const UMaterialExpressionTime* Time = nullptr;
    const UMaterialExpressionScalarParameter* Strength = nullptr;
    const UMaterialExpressionScalarParameter* Speed = nullptr;
    const UMaterialExpressionScalarParameter* Height = nullptr;
    const UMaterialExpressionScalarParameter* Response = nullptr;
    const UMaterialExpressionScalarParameter* MaximumWpo = nullptr;
    const UMaterialExpressionVectorParameter* Direction = nullptr;
    const UMaterialExpressionCustom* Wind = nullptr;
    const UMaterialExpressionCustom* VariedLeafColor = nullptr;
    const UMaterialExpressionConstant2Vector* TangentXY = nullptr;
    const UMaterialExpressionTwoSidedSign* TwoSidedSign = nullptr;
    const UMaterialExpressionAppendVector* Facing = nullptr;
    const UMaterialExpressionMultiply* CorrectedNormal = nullptr;
    const UMaterialExpressionConstant3Vector* SubsurfaceAttenuation = nullptr;
    const UMaterialExpressionMultiply* SubsurfaceColor = nullptr;

    int32 TextureSampleNodes = 0;
    int32 InstanceRandomNodes = 0;
    int32 ScalarNodes = 0;
    int32 VectorNodes = 0;
    int32 WorldPositionNodes = 0;
    int32 TransformNodes = 0;
    int32 TimeNodes = 0;
    int32 WindNodes = 0;
    int32 LeafColorNodes = 0;
    int32 Constant2Nodes = 0;
    int32 TwoSidedSignNodes = 0;
    int32 AppendNodes = 0;
    int32 MultiplyNodes = 0;
    int32 Constant3Nodes = 0;

    for (const UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        if (const UMaterialExpressionTextureSampleParameter2D* Texture =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            ++TextureSampleNodes;
            if (Texture->ParameterName == TEXT("BaseColorTexture") &&
                Texture->Desc == TEXT("V5B_PACHIRA_BASE"))
            {
                Base = Texture;
            }
            else if (Texture->ParameterName == TEXT("NormalTexture") &&
                Texture->Desc == TEXT("V5B_PACHIRA_NORMAL_DX"))
            {
                Normal = Texture;
            }
            else if (Texture->ParameterName == TEXT("RoughnessTexture") &&
                Texture->Desc == TEXT("V5B_PACHIRA_ROUGHNESS"))
            {
                Roughness = Texture;
            }
            else if (Texture->ParameterName == TEXT("OpacityTexture") &&
                Texture->Desc == TEXT("V5B_PACHIRA_OPACITY"))
            {
                Opacity = Texture;
            }
        }
        if (Expression && Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
        {
            InstanceRandom = Expression;
            ++InstanceRandomNodes;
        }
        if (const UMaterialExpressionScalarParameter* Scalar =
                Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            ++ScalarNodes;
            if (Scalar->ParameterName == TEXT("Specular") &&
                Scalar->Desc == TEXT("V5B_PACHIRA_LOW_SPECULAR"))
            {
                Specular = Scalar;
            }
            else if (Scalar->ParameterName == TEXT("TRIAD_WindStrengthCm") &&
                Scalar->Desc == TEXT("V5B_PACHIRA_WIND_STRENGTH"))
            {
                Strength = Scalar;
            }
            else if (Scalar->ParameterName == TEXT("TRIAD_WindSpeed") &&
                Scalar->Desc == TEXT("V5B_PACHIRA_WIND_SPEED"))
            {
                Speed = Scalar;
            }
            else if (Scalar->ParameterName == TEXT("TRIAD_WindHeightCm") &&
                Scalar->Desc == TEXT("V5B_PACHIRA_WIND_HEIGHT"))
            {
                Height = Scalar;
            }
            else if (Scalar->ParameterName == TEXT("TRIAD_WindResponseScale") &&
                Scalar->Desc == TEXT("V5B_PACHIRA_WIND_RESPONSE"))
            {
                Response = Scalar;
            }
            else if (Scalar->ParameterName == TEXT("TRIAD_MaxWpoCm") &&
                Scalar->Desc == TEXT("V5B_PACHIRA_MAX_WPO"))
            {
                MaximumWpo = Scalar;
            }
        }
        if (const UMaterialExpressionVectorParameter* Vector =
                Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            ++VectorNodes;
            if (Vector->ParameterName == TEXT("TRIAD_WindDirection") &&
                Vector->Desc == TEXT("V5B_PACHIRA_WIND_DIRECTION"))
            {
                Direction = Vector;
            }
        }
        if (const UMaterialExpressionWorldPosition* Position =
                Cast<UMaterialExpressionWorldPosition>(Expression))
        {
            ++WorldPositionNodes;
            if (Position->Desc == TEXT("V5B_PACHIRA_WORLD_POSITION"))
            {
                WorldPosition = Position;
            }
        }
        if (const UMaterialExpressionTransformPosition* Transform =
                Cast<UMaterialExpressionTransformPosition>(Expression))
        {
            ++TransformNodes;
            if (Transform->Desc == TEXT("V5B_PACHIRA_INSTANCE_LOCAL_POSITION"))
            {
                InstanceLocalPosition = Transform;
            }
        }
        if (const UMaterialExpressionTime* TimeExpression =
                Cast<UMaterialExpressionTime>(Expression))
        {
            ++TimeNodes;
            if (TimeExpression->Desc == TEXT("V5B_PACHIRA_TIME"))
            {
                Time = TimeExpression;
            }
        }
        if (const UMaterialExpressionCustom* Custom =
                Cast<UMaterialExpressionCustom>(Expression))
        {
            if (Custom->Description == PachiraWindDescription &&
                Custom->Code == PachiraWindCode)
            {
                Wind = Custom;
                ++WindNodes;
            }
            else if (Custom->Description == PachiraLeafColorDescription &&
                Custom->Code == PachiraLeafColorCode)
            {
                VariedLeafColor = Custom;
                ++LeafColorNodes;
            }
        }
        if (const UMaterialExpressionConstant2Vector* Constant2 =
                Cast<UMaterialExpressionConstant2Vector>(Expression))
        {
            ++Constant2Nodes;
            if (Constant2->Desc == TEXT("V5B_PACHIRA_TWO_SIDED_TANGENT_XY"))
            {
                TangentXY = Constant2;
            }
        }
        if (const UMaterialExpressionTwoSidedSign* Sign =
                Cast<UMaterialExpressionTwoSidedSign>(Expression))
        {
            ++TwoSidedSignNodes;
            if (Sign->Desc == TEXT("V5B_PACHIRA_TWO_SIDED_SIGN"))
            {
                TwoSidedSign = Sign;
            }
        }
        if (const UMaterialExpressionAppendVector* Append =
                Cast<UMaterialExpressionAppendVector>(Expression))
        {
            ++AppendNodes;
            if (Append->Desc == TEXT("V5B_PACHIRA_TWO_SIDED_FACING"))
            {
                Facing = Append;
            }
        }
        if (const UMaterialExpressionMultiply* Multiply =
                Cast<UMaterialExpressionMultiply>(Expression))
        {
            ++MultiplyNodes;
            if (Multiply->Desc == TEXT("V5B_PACHIRA_TWO_SIDED_NORMAL"))
            {
                CorrectedNormal = Multiply;
            }
            else if (Multiply->Desc ==
                TEXT("V5B_PACHIRA_ATTENUATED_SUBSURFACE"))
            {
                SubsurfaceColor = Multiply;
            }
        }
        if (const UMaterialExpressionConstant3Vector* Constant3 =
                Cast<UMaterialExpressionConstant3Vector>(Expression))
        {
            ++Constant3Nodes;
            if (Constant3->Desc ==
                TEXT("V5B_PACHIRA_SUBSURFACE_ATTENUATION"))
            {
                SubsurfaceAttenuation = Constant3;
            }
        }
    }

    const auto InputMatches = [](
        const FExpressionInput& Input,
        const UMaterialExpression* ExpectedExpression,
        int32 ExpectedOutputIndex)
    {
        return Input.Expression == ExpectedExpression &&
            Input.OutputIndex == ExpectedOutputIndex;
    };
    const auto TextureMatches = [](
        const UMaterialExpressionTextureSampleParameter2D* Texture,
        const FString& ExpectedTexturePath,
        EMaterialSamplerType ExpectedSampler)
    {
        return Texture && Texture->Texture &&
            Texture->Texture->GetPathName() == ExpectedTexturePath &&
            Texture->SamplerType == ExpectedSampler &&
            Texture->SamplerSource == SSM_FromTextureAsset &&
            !Texture->Coordinates.Expression;
    };
    const FString Prefix = bLeaves
        ? TEXT("T_IPV5B_Pachira_Leaves_")
        : TEXT("T_IPV5B_Pachira_Bark_");
    const auto ScalarMatches = [](const UMaterialExpressionScalarParameter* Scalar,
                                  float ExpectedValue)
    {
        return Scalar &&
            Scalar->Group == FName(TEXT("Istana Explore V5B")) &&
            FMath::IsNearlyEqual(
                Scalar->DefaultValue, ExpectedValue, 0.000001f);
    };

    if (TextureSampleNodes != (bLeaves ? 4 : 3) ||
        InstanceRandomNodes != 1 || ScalarNodes != 6 || VectorNodes != 1 ||
        WorldPositionNodes != 1 || TransformNodes != 1 || TimeNodes != 1 ||
        WindNodes != 1 || LeafColorNodes != (bLeaves ? 1 : 0) ||
        Constant2Nodes != (bLeaves ? 1 : 0) ||
        TwoSidedSignNodes != (bLeaves ? 1 : 0) ||
        AppendNodes != (bLeaves ? 1 : 0) ||
        MultiplyNodes != (bLeaves ? 2 : 0) ||
        Constant3Nodes != (bLeaves ? 1 : 0) ||
        !Base || !Normal || !Roughness || (bLeaves && !Opacity) ||
        (!bLeaves && Opacity) || !InstanceRandom ||
        InstanceRandom->Desc != TEXT("V5B_PACHIRA_PER_INSTANCE_RANDOM") ||
        !TextureMatches(
            Base, ObjectPath(TexturePath, Prefix + TEXT("BaseColor")),
            SAMPLERTYPE_Color) ||
        !TextureMatches(
            Normal, ObjectPath(TexturePath, Prefix + TEXT("NormalDX")),
            SAMPLERTYPE_Normal) ||
        !TextureMatches(
            Roughness, ObjectPath(TexturePath, Prefix + TEXT("Roughness")),
            SAMPLERTYPE_Masks) ||
        (bLeaves && !TextureMatches(
            Opacity,
            ObjectPath(TexturePath, PachiraLeavesOpacityTextureName),
            SAMPLERTYPE_Masks)) ||
        !ScalarMatches(Specular, ExpectedSpecular) ||
        !ScalarMatches(Strength, PachiraWindStrengthCm) ||
        !ScalarMatches(Speed, PachiraWindSpeed) ||
        !ScalarMatches(Height, PachiraWindHeightCm) ||
        !ScalarMatches(Response, ExpectedResponse) ||
        !ScalarMatches(MaximumWpo, ExpectedMaximumWpo) ||
        !Direction ||
        Direction->Group != FName(TEXT("Istana Explore V5B")) ||
        !Direction->DefaultValue.Equals(
            FLinearColor(0.93f, 0.37f, 0.0f, 0.0f), 0.000001f) ||
        !WorldPosition ||
        WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        !InstanceLocalPosition ||
        InstanceLocalPosition->TransformSourceType !=
            TRANSFORMPOSSOURCE_World ||
        InstanceLocalPosition->TransformType != TRANSFORMPOSSOURCE_Instance ||
        !InputMatches(InstanceLocalPosition->Input, WorldPosition, 0) ||
        !Time || Time->bIgnorePause || Time->bOverride_Period ||
        !FMath::IsNearlyZero(Time->Period, 0.000001f) ||
        !Wind || Wind->OutputType != CMOT_Float3)
    {
        OutError = FString::Printf(
            TEXT("V5B Pachira %s lost an exact PBR texture, scalar, instance-random, or instance-local wind node."),
            bLeaves ? TEXT("leaves") : TEXT("bark"));
        return false;
    }

    const FName ExpectedWindInputNames[] = {
        TEXT("WorldPosition"),
        TEXT("InstanceLocalPosition"),
        TEXT("TimeSeconds"),
        TEXT("WindStrengthCm"),
        TEXT("WindSpeed"),
        TEXT("WindDirection"),
        TEXT("HeightCm"),
        TEXT("ResponseScale"),
        TEXT("MaxWpoCm"),
        TEXT("Random01")};
    const UMaterialExpression* ExpectedWindInputNodes[] = {
        WorldPosition,
        InstanceLocalPosition,
        Time,
        Strength,
        Speed,
        Direction,
        Height,
        Response,
        MaximumWpo,
        InstanceRandom};
    bool bExactWindInputs = Wind->Inputs.Num() == 10;
    if (bExactWindInputs)
    {
        for (int32 Index = 0; Index < 10; ++Index)
        {
            if (Wind->Inputs[Index].InputName != ExpectedWindInputNames[Index] ||
                !InputMatches(
                    Wind->Inputs[Index].Input,
                    ExpectedWindInputNodes[Index],
                    0))
            {
                bExactWindInputs = false;
                break;
            }
        }
    }
    if (!bExactWindInputs ||
        !InputMatches(Data->Roughness, Roughness, 1) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->WorldPositionOffset, Wind, 0) ||
        Data->Metallic.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->AmbientOcclusion.Expression ||
        Data->Refraction.Expression || Data->PixelDepthOffset.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5B Pachira %s lost its exact instance-local wind inputs or direct material-output connections."),
            bLeaves ? TEXT("leaves") : TEXT("bark"));
        return false;
    }

    const bool bExactLeafColor = VariedLeafColor &&
        VariedLeafColor->OutputType == CMOT_Float3 &&
        VariedLeafColor->Inputs.Num() == 2 &&
        VariedLeafColor->Inputs[0].InputName == FName(TEXT("BaseColor")) &&
        InputMatches(VariedLeafColor->Inputs[0].Input, Base, 0) &&
        VariedLeafColor->Inputs[1].InputName == FName(TEXT("Random01")) &&
        InputMatches(VariedLeafColor->Inputs[1].Input, InstanceRandom, 0);
    const bool bExactLeafFacing = TangentXY &&
        FMath::IsNearlyEqual(TangentXY->R, 1.0f, 0.000001f) &&
        FMath::IsNearlyEqual(TangentXY->G, 1.0f, 0.000001f) &&
        TwoSidedSign && Facing &&
        InputMatches(Facing->A, TangentXY, 0) &&
        InputMatches(Facing->B, TwoSidedSign, 0) && CorrectedNormal &&
        InputMatches(CorrectedNormal->A, Normal, 0) &&
        InputMatches(CorrectedNormal->B, Facing, 0);
    const bool bExactSubsurface = SubsurfaceAttenuation &&
        SubsurfaceAttenuation->Constant.Equals(
            PachiraLeavesSubsurfaceAttenuation, 0.000001f) &&
        SubsurfaceColor &&
        InputMatches(SubsurfaceColor->A, VariedLeafColor, 0) &&
        InputMatches(SubsurfaceColor->B, SubsurfaceAttenuation, 0);
    const bool bExactLeafOutputs =
        bExactLeafColor && bExactLeafFacing && bExactSubsurface &&
        InputMatches(Data->BaseColor, VariedLeafColor, 0) &&
        InputMatches(Data->Normal, CorrectedNormal, 0) &&
        InputMatches(Data->OpacityMask, Opacity, 1) &&
        InputMatches(Data->SubsurfaceColor, SubsurfaceColor, 0);
    const bool bExactBarkOutputs =
        InputMatches(Data->BaseColor, Base, 0) &&
        InputMatches(Data->Normal, Normal, 0) &&
        !Data->OpacityMask.Expression && !Data->SubsurfaceColor.Expression;
    if ((bLeaves && !bExactLeafOutputs) ||
        (!bLeaves && !bExactBarkOutputs))
    {
        OutError = FString::Printf(
            TEXT("V5B Pachira %s lost its exact per-instance color, TwoSidedSign normal, attenuated subsurface, alpha, or direct bark connection."),
            bLeaves ? TEXT("leaves") : TEXT("bark"));
        return false;
    }

    OutError.Reset();
    return true;
}

UMaterial* CreatePaverStoneMaterial(IAssetTools& AssetTools, FString& OutError)
{
    UMaterial* Material = CreateEmptyMaterial(
        AssetTools, PaverMaterialName, OutError);
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    UMaterialExpressionTextureCoordinate* Uv =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material, *PaverUvDescription, -900, -100);
    UMaterialExpression* InstanceRandom = AddExpressionByClassPath(
        Material,
        TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"),
        *PaverRandomDescription,
        -900,
        80);
    UMaterialExpressionCustom* Base = AddExpression<UMaterialExpressionCustom>(
        Material, *PaverBaseDescription, -600, -220);
    UMaterialExpressionCustom* Roughness =
        AddExpression<UMaterialExpressionCustom>(
            Material, *PaverRoughnessDescription, -600, 20);
    UMaterialExpressionCustom* Normal = AddExpression<UMaterialExpressionCustom>(
        Material, *PaverNormalDescription, -600, 240);
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material,
        *PaverSpecularDescription,
        TEXT("Specular"),
        0.16f,
        -320,
        400);
    if (!Data || !Uv || !InstanceRandom || !Base || !Roughness || !Normal ||
        !Specular)
    {
        OutError = TEXT("Could not allocate the complete V5B procedural paver-stone graph.");
        return nullptr;
    }

    Uv->CoordinateIndex = 0;
    Uv->UTiling = 1.0f;
    Uv->VTiling = 1.0f;
    const auto ConfigureStoneCustom = [Uv, InstanceRandom](
        UMaterialExpressionCustom* Custom,
        const FString& Description,
        const FString& Code,
        ECustomMaterialOutputType OutputType)
    {
        Custom->Description = Description;
        Custom->Code = Code;
        Custom->OutputType = OutputType;
        Custom->Inputs.SetNum(2);
        Custom->Inputs[0].InputName = TEXT("UV");
        Custom->Inputs[0].Input.Connect(0, Uv);
        Custom->Inputs[1].InputName = TEXT("Random01");
        Custom->Inputs[1].Input.Connect(0, InstanceRandom);
    };
    ConfigureStoneCustom(
        Base, PaverBaseDescription, PaverBaseCode, CMOT_Float3);
    ConfigureStoneCustom(
        Roughness,
        PaverRoughnessDescription,
        PaverRoughnessCode,
        CMOT_Float1);
    ConfigureStoneCustom(
        Normal, PaverNormalDescription, PaverNormalCode, CMOT_Float3);

    Data->BaseColor.Connect(0, Base);
    Data->Roughness.Connect(0, Roughness);
    Data->Normal.Connect(0, Normal);
    Data->Specular.Connect(0, Specular);
    Material->BlendMode = BLEND_Opaque;
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUsedWithInstancedStaticMeshes = true;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    FinalizeMaterial(Material);
    OutError.Reset();
    return Material;
}

bool CreateMaterials(
    IAssetTools& AssetTools,
    const TMap<FString, UTexture2D*>& Textures,
    TMap<FString, UMaterialInterface*>& OutMaterials,
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    UMaterial* Accent = CreateAccentTurfMaterial(AssetTools, OutError);
    if (!Accent)
    {
        return false;
    }
    UMaterial* FormalBed = CreateFormalBedSoilMaterial(AssetTools, OutError);
    if (!FormalBed)
    {
        return false;
    }

    UMaterial* Paver = CreatePaverStoneMaterial(AssetTools, OutError);
    if (!Paver)
    {
        return false;
    }

    UMaterial* Surface = CreateWaterMaterial(AssetTools, OutError);
    UMaterial* Foam = Surface
        ? CreateTranslucentWaterDetailMaterial(AssetTools, FoamMaterialName, false, OutError)
        : nullptr;
    UMaterial* Spray = Foam
        ? CreateTranslucentWaterDetailMaterial(AssetTools, SprayMaterialName, true, OutError)
        : nullptr;
    UMaterial* Bark = Spray
        ? CreatePachiraMaterial(AssetTools, false, Textures, OutError)
        : nullptr;
    UMaterial* Leaves = Bark
        ? CreatePachiraMaterial(AssetTools, true, Textures, OutError)
        : nullptr;
    if (!Surface || !Foam || !Spray || !Bark || !Leaves)
    {
        return false;
    }
    OutMaterials = {
        {AccentMaterialName, Accent},
        {FormalBedMaterialName, FormalBed},
        {PaverMaterialName, Paver},
        {SurfaceMaterialName, Surface},
        {FoamMaterialName, Foam},
        {SprayMaterialName, Spray},
        {BarkMaterialName, Bark},
        {LeavesMaterialName, Leaves}};
    for (const TPair<FString, UMaterialInterface*>& Pair : OutMaterials)
    {
        OutAssets.Add(Pair.Value);
    }
    OutError.Reset();
    return OutMaterials.Num() == 8;
}

bool ImportTextures(
    IAssetTools& AssetTools,
    TMap<FString, UTexture2D*>& OutTextures,
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    TArray<UAssetImportTask*> Tasks;
    TArray<TStrongObjectPtr<UAssetImportTask>> TaskGuards;
    TaskGuards.Reserve(TextureSpecs().Num());
    TMap<FString, UAssetImportTask*> ByName;
    for (const FTextureSpec& Spec : TextureSpecs())
    {
        const FString Source = TextureSource(Spec);
        if (!ValidateSourceBytes(Source, Spec.SourceBytes, OutError))
        {
            return false;
        }
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        if (!Task)
        {
            OutError = TEXT("Could not allocate a V5B Pachira texture import task.");
            return false;
        }
        Task->Filename = Source;
        Task->DestinationPath = TexturePath;
        Task->DestinationName = Spec.AssetName;
        Task->bReplaceExisting = false;
        Task->bReplaceExistingSettings = false;
        Task->bAutomated = true;
        Task->bSave = false;
        Task->bAsync = false;
        Tasks.Add(Task);
        TaskGuards.Add(TStrongObjectPtr<UAssetImportTask>(Task));
        ByName.Add(Spec.AssetName, Task);
    }
    AssetTools.ImportAssetTasks(Tasks);
    for (const FTextureSpec& Spec : TextureSpecs())
    {
        const FString ExpectedPath = ObjectPath(TexturePath, Spec.AssetName);
        UTexture2D* Texture = nullptr;
        for (UObject* Object : ByName.FindRef(Spec.AssetName)->GetObjects())
        {
            if (Object && Object->GetPathName() == ExpectedPath)
            {
                Texture = Cast<UTexture2D>(Object);
            }
        }
        if (!Texture)
        {
            Texture = LoadExact<UTexture2D>(ExpectedPath);
        }
        if (!Texture)
        {
            OutError = TEXT("Texture import produced no exact V5B object: ") + ExpectedPath;
            return false;
        }
        Texture->Modify();
        Texture->SRGB = Spec.bSrgb;
        Texture->CompressionSettings = Spec.Compression;
        Texture->bFlipGreenChannel = Spec.bFlipGreen;
        const bool bBermudaOpacity =
            Spec.AssetName == TEXT("T_IPV5B_Bermuda_Opacity");
        const bool bPachiraLeavesOpacity =
            Spec.AssetName == PachiraLeavesOpacityTextureName;
        const bool bPreserveMaskedAlphaCoverage =
            bBermudaOpacity || bPachiraLeavesOpacity;
        const bool bBermudaBladeTexture = bBermudaOpacity ||
            Spec.AssetName == TEXT("T_IPV5B_Bermuda_BaseColorPadded");
        Texture->AddressX = bBermudaBladeTexture ? TA_Clamp : TA_Wrap;
        Texture->AddressY = bBermudaBladeTexture ? TA_Clamp : TA_Wrap;
        Texture->MipGenSettings = bPreserveMaskedAlphaCoverage
            ? TMGS_Sharpen1
            : TMGS_FromTextureGroup;
        Texture->bDoScaleMipsForAlphaCoverage =
            bPreserveMaskedAlphaCoverage;
        Texture->AlphaCoverageThresholds = bBermudaOpacity
            ? FVector4(0.28, 0.0, 0.0, 0.0)
            : bPachiraLeavesOpacity
            ? FVector4(PachiraOpacityClipValue, 0.0, 0.0, 0.0)
            : FVector4(0.0, 0.0, 0.0, 0.0);
        Texture->PostEditChange();
        Texture->MarkPackageDirty();
        OutTextures.Add(Spec.AssetName, Texture);
        OutAssets.Add(Texture);
    }
    OutError.Reset();
    return OutTextures.Num() == 9;
}

UAssetImportTask* MakeMeshTask(
    const FString& Source,
    const FString& Destination,
    const FString& Name,
    bool bCombine,
    bool bConvertScene,
    bool bTransformAbsolute,
    float UniformScale)
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
    Options->StaticMeshImportData->bConvertScene = bConvertScene;
    Options->StaticMeshImportData->bConvertSceneUnit = bConvertScene;
    Options->StaticMeshImportData->bForceFrontXAxis = false;
    Options->StaticMeshImportData->ImportUniformScale = UniformScale;
    Options->StaticMeshImportData->bCombineMeshes = bCombine;
    Options->StaticMeshImportData->bReorderMaterialToFbxOrder = true;
    Options->StaticMeshImportData->bImportMeshLODs = false;
    Options->StaticMeshImportData->bTransformVertexToAbsolute = bTransformAbsolute;
    Options->StaticMeshImportData->bBakePivotInVertex = false;
    Options->StaticMeshImportData->bAutoGenerateCollision = false;
    Options->StaticMeshImportData->bGenerateLightmapUVs = true;
    Options->StaticMeshImportData->NormalImportMethod = EFBXNormalImportMethod::FBXNIM_ImportNormals;
    Options->StaticMeshImportData->NormalGenerationMethod = EFBXNormalGenerationMethod::MikkTSpace;
    Options->StaticMeshImportData->bBuildNanite = false;
    Options->StaticMeshImportData->bRemoveDegenerates = true;
    Factory->ImportUI = Options;
    Factory->SetDetectImportTypeOnImport(false);
    Task->Filename = Source;
    Task->DestinationPath = Destination;
    Task->DestinationName = Name;
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
    Mesh->NaniteSettings.bEnabled = false;
    Mesh->CreateBodySetup();
    if (UBodySetup* Body = Mesh->GetBodySetup())
    {
        Body->Modify();
        Body->RemoveSimpleCollision();
        Body->CollisionTraceFlag = CTF_UseDefault;
    }
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
}

bool NormalizePachiraCentimeterGeometry(UStaticMesh* Mesh, FString& OutError)
{
    if (!Mesh || Mesh->GetNumSourceModels() != 1)
    {
        OutError = TEXT("A V5B Pachira part lost its single source-model contract.");
        return false;
    }
    Mesh->Modify();
    FMeshDescription* Description = Mesh->GetMeshDescription(0);
    if (!Description || Description->Vertices().Num() <= 0)
    {
        OutError = TEXT("A V5B Pachira part has no editable LOD0 mesh description.");
        return false;
    }
    FStaticMeshAttributes Attributes(*Description);
    TVertexAttributesRef<FVector3f> Positions = Attributes.GetVertexPositions();
    float SourceMinZ = TNumericLimits<float>::Max();
    float SourceMaxZ = -TNumericLimits<float>::Max();
    for (const FVertexID VertexId : Description->Vertices().GetElementIDs())
    {
        const FVector3f Position = Positions[VertexId];
        if (!FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y) ||
            !FMath::IsFinite(Position.Z))
        {
            OutError = TEXT("A V5B Pachira source vertex is non-finite.");
            return false;
        }
        SourceMinZ = FMath::Min(SourceMinZ, Position.Z);
        SourceMaxZ = FMath::Max(SourceMaxZ, Position.Z);
    }
    if (!FMath::IsWithinInclusive(SourceMinZ, -0.05f, 0.60f) ||
        SourceMaxZ < 0.20f || SourceMaxZ > 2.00f)
    {
        OutError = FString::Printf(
            TEXT("A V5B Pachira source node lost its pinned metre-scale FBX geometry: minZ=%.6f maxZ=%.6f."),
            SourceMinZ,
            SourceMaxZ);
        return false;
    }
    for (const FVertexID VertexId : Description->Vertices().GetElementIDs())
    {
        Positions[VertexId] *= 100.0f;
    }
    Mesh->CommitMeshDescription(0);
    Mesh->GetSourceModel(0).BuildSettings.BuildScale3D = FVector::OneVector;
    Mesh->Build(false);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    OutError.Reset();
    return true;
}

int32 TriangleCount(const UStaticMesh* Mesh, int32 LodIndex = 0)
{
    const FStaticMeshRenderData* Data = Mesh ? Mesh->GetRenderData() : nullptr;
    return Data && Data->LODResources.IsValidIndex(LodIndex)
        ? Data->LODResources[LodIndex].GetNumTriangles()
        : INDEX_NONE;
}

UStaticMesh* ResolveMesh(UAssetImportTask* Task, const FString& ExactPath)
{
    if (UStaticMesh* Exact = LoadExact<UStaticMesh>(ExactPath))
    {
        return Exact;
    }
    if (Task)
    {
        for (UObject* Object : Task->GetObjects())
        {
            if (IsValid(Object) && Object->GetPathName() == ExactPath)
            {
                return Cast<UStaticMesh>(Object);
            }
        }
    }
    return nullptr;
}

bool NormalizeAndBindPorticoMaterials(
    UStaticMesh* Portico,
    const UStaticMesh* SourceHero,
    FString& OutError)
{
    if (!Portico || !SourceHero ||
        Portico->GetStaticMaterials().Num() != 11 ||
        SourceHero->GetStaticMaterials().Num() != 11 ||
        !Portico->GetRenderData() ||
        Portico->GetRenderData()->LODResources.Num() != 1 ||
        Portico->GetRenderData()->LODResources[0].Sections.Num() != 11)
    {
        OutError = TEXT("The trimmed V5B/source V5 building mesh lacks the exact eleven-slot regular surface.");
        return false;
    }

    const TArray<FStaticMaterial> ImportedMaterials = Portico->GetStaticMaterials();
    const TArray<FStaticMaterial>& SourceMaterials = SourceHero->GetStaticMaterials();
    TMap<FName, int32> SourceOrder;
    for (int32 SourceIndex = 0; SourceIndex < SourceMaterials.Num(); ++SourceIndex)
    {
        const FStaticMaterial& Source = SourceMaterials[SourceIndex];
        if (Source.MaterialSlotName.IsNone() ||
            Source.MaterialSlotName != Source.ImportedMaterialSlotName ||
            !Source.MaterialInterface || SourceOrder.Contains(Source.MaterialSlotName))
        {
            OutError = FString::Printf(
                TEXT("The source V5 hero material roster is not exact at slot %d."),
                SourceIndex);
            return false;
        }
        SourceOrder.Add(Source.MaterialSlotName, SourceIndex);
    }

    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(SourceMaterials.Num());
    TArray<int32> ImportedToOrdered;
    ImportedToOrdered.Init(INDEX_NONE, ImportedMaterials.Num());
    TArray<bool> Seen;
    Seen.Init(false, SourceMaterials.Num());
    for (int32 ImportedIndex = 0; ImportedIndex < ImportedMaterials.Num(); ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        const int32* OrderedIndex = SourceOrder.Find(Imported.MaterialSlotName);
        if (!OrderedIndex || Imported.MaterialSlotName.IsNone() ||
            Imported.MaterialSlotName != Imported.ImportedMaterialSlotName ||
            Seen[*OrderedIndex])
        {
            OutError = FString::Printf(
                TEXT("Trimmed V5B OBJ material slots are not an exact unique V5 permutation at imported index %d: slot='%s' imported='%s'."),
                ImportedIndex,
                *Imported.MaterialSlotName.ToString(),
                *Imported.ImportedMaterialSlotName.ToString());
            return false;
        }
        Seen[*OrderedIndex] = true;
        ImportedToOrdered[ImportedIndex] = *OrderedIndex;
        OrderedMaterials[*OrderedIndex] = Imported;
        OrderedMaterials[*OrderedIndex].MaterialSlotName =
            SourceMaterials[*OrderedIndex].MaterialSlotName;
        OrderedMaterials[*OrderedIndex].ImportedMaterialSlotName =
            SourceMaterials[*OrderedIndex].ImportedMaterialSlotName;
        OrderedMaterials[*OrderedIndex].MaterialInterface =
            SourceMaterials[*OrderedIndex].MaterialInterface;
    }
    for (bool bSeen : Seen)
    {
        if (!bSeen)
        {
            OutError = TEXT("Trimmed V5B OBJ omitted one or more required V5 material slots.");
            return false;
        }
    }

    const FStaticMeshLODResources& Lod = Portico->GetRenderData()->LODResources[0];
    for (int32 SectionIndex = 0; SectionIndex < Lod.Sections.Num(); ++SectionIndex)
    {
        const int32 RenderMaterialIndex = Lod.Sections[SectionIndex].MaterialIndex;
        FMeshSectionInfo Section = Portico->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Portico->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToOrdered.IsValidIndex(RenderMaterialIndex) ||
            !ImportedToOrdered.IsValidIndex(Section.MaterialIndex) ||
            !ImportedToOrdered.IsValidIndex(Original.MaterialIndex) ||
            RenderMaterialIndex != Section.MaterialIndex ||
            RenderMaterialIndex != Original.MaterialIndex ||
            ImportedToOrdered[RenderMaterialIndex] == INDEX_NONE)
        {
            OutError = FString::Printf(
                TEXT("Trimmed V5B OBJ section %d references an invalid imported material index."),
                SectionIndex);
            return false;
        }
        Section.MaterialIndex = ImportedToOrdered[Section.MaterialIndex];
        Original.MaterialIndex = ImportedToOrdered[Original.MaterialIndex];
        Portico->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Portico->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    Portico->SetStaticMaterials(OrderedMaterials);
    OutError.Reset();
    return true;
}

bool NormalizeAndBindHardscapeSuccessorMaterials(
    UStaticMesh* Successor,
    const UStaticMesh* SourceHardscape,
    FString& OutError)
{
    const FName ExpectedNames[] = {
        FName(TEXT("M_IPV_Stone")),
        FName(TEXT("M_IPV_Water")),
        FName(TEXT("M_IPV_Metal"))};
    if (!Successor || !SourceHardscape ||
        Successor->GetStaticMaterials().Num() != UE_ARRAY_COUNT(ExpectedNames) ||
        SourceHardscape->GetStaticMaterials().Num() != 4 ||
        !Successor->GetRenderData() ||
        Successor->GetRenderData()->LODResources.Num() != 1 ||
        Successor->GetRenderData()->LODResources[0].Sections.Num() !=
            UE_ARRAY_COUNT(ExpectedNames))
    {
        OutError = TEXT("The V5B hardscape render successor lacks its exact three-slot/three-section surface.");
        return false;
    }

    TArray<const FStaticMaterial*> SourceByExpectedIndex;
    SourceByExpectedIndex.Init(nullptr, UE_ARRAY_COUNT(ExpectedNames));
    for (const FStaticMaterial& Source : SourceHardscape->GetStaticMaterials())
    {
        for (int32 ExpectedIndex = 0;
             ExpectedIndex < UE_ARRAY_COUNT(ExpectedNames);
             ++ExpectedIndex)
        {
            if (Source.MaterialSlotName == ExpectedNames[ExpectedIndex] &&
                Source.ImportedMaterialSlotName == ExpectedNames[ExpectedIndex])
            {
                if (SourceByExpectedIndex[ExpectedIndex] ||
                    !Source.MaterialInterface)
                {
                    OutError = TEXT("The frozen source hardscape material roster is ambiguous or unbound.");
                    return false;
                }
                SourceByExpectedIndex[ExpectedIndex] = &Source;
            }
        }
    }
    for (const FStaticMaterial* Source : SourceByExpectedIndex)
    {
        if (!Source)
        {
            OutError = TEXT("The frozen source hardscape omitted a required non-planting material slot.");
            return false;
        }
    }

    const TArray<FStaticMaterial> ImportedMaterials =
        Successor->GetStaticMaterials();
    TArray<FStaticMaterial> OrderedMaterials;
    OrderedMaterials.SetNum(UE_ARRAY_COUNT(ExpectedNames));
    TArray<int32> ImportedToOrdered;
    ImportedToOrdered.Init(INDEX_NONE, ImportedMaterials.Num());
    TArray<bool> Seen;
    Seen.Init(false, UE_ARRAY_COUNT(ExpectedNames));
    for (int32 ImportedIndex = 0;
         ImportedIndex < ImportedMaterials.Num();
         ++ImportedIndex)
    {
        const FStaticMaterial& Imported = ImportedMaterials[ImportedIndex];
        int32 ExpectedIndex = INDEX_NONE;
        for (int32 Candidate = 0;
             Candidate < UE_ARRAY_COUNT(ExpectedNames);
             ++Candidate)
        {
            if (Imported.MaterialSlotName == ExpectedNames[Candidate] &&
                Imported.ImportedMaterialSlotName == ExpectedNames[Candidate])
            {
                ExpectedIndex = Candidate;
                break;
            }
        }
        if (ExpectedIndex == INDEX_NONE || Seen[ExpectedIndex])
        {
            OutError = FString::Printf(
                TEXT("The V5B hardscape successor has an unexpected or duplicate imported material at index %d."),
                ImportedIndex);
            return false;
        }
        Seen[ExpectedIndex] = true;
        ImportedToOrdered[ImportedIndex] = ExpectedIndex;
        OrderedMaterials[ExpectedIndex] = Imported;
        OrderedMaterials[ExpectedIndex].MaterialSlotName =
            ExpectedNames[ExpectedIndex];
        OrderedMaterials[ExpectedIndex].ImportedMaterialSlotName =
            ExpectedNames[ExpectedIndex];
        OrderedMaterials[ExpectedIndex].MaterialInterface =
            SourceByExpectedIndex[ExpectedIndex]->MaterialInterface;
    }

    const FStaticMeshLODResources& Lod =
        Successor->GetRenderData()->LODResources[0];
    for (int32 SectionIndex = 0;
         SectionIndex < Lod.Sections.Num();
         ++SectionIndex)
    {
        const int32 RenderMaterialIndex =
            Lod.Sections[SectionIndex].MaterialIndex;
        FMeshSectionInfo Section =
            Successor->GetSectionInfoMap().Get(0, SectionIndex);
        FMeshSectionInfo Original =
            Successor->GetOriginalSectionInfoMap().Get(0, SectionIndex);
        if (!ImportedToOrdered.IsValidIndex(RenderMaterialIndex) ||
            !ImportedToOrdered.IsValidIndex(Section.MaterialIndex) ||
            !ImportedToOrdered.IsValidIndex(Original.MaterialIndex) ||
            RenderMaterialIndex != Section.MaterialIndex ||
            RenderMaterialIndex != Original.MaterialIndex ||
            ImportedToOrdered[RenderMaterialIndex] == INDEX_NONE)
        {
            OutError = FString::Printf(
                TEXT("The V5B hardscape successor section %d references an invalid imported material index."),
                SectionIndex);
            return false;
        }
        Section.MaterialIndex = ImportedToOrdered[Section.MaterialIndex];
        Original.MaterialIndex = ImportedToOrdered[Original.MaterialIndex];
        Successor->GetSectionInfoMap().Set(0, SectionIndex, Section);
        Successor->GetOriginalSectionInfoMap().Set(0, SectionIndex, Original);
    }
    Successor->Modify();
    Successor->SetStaticMaterials(OrderedMaterials);
    Successor->PostEditChange();
    Successor->MarkPackageDirty();
    OutError.Reset();
    return true;
}

bool BindSingleMaterial(
    UStaticMesh* Mesh,
    UMaterialInterface* Material,
    FString& OutError)
{
    if (!Mesh || !Material || Mesh->GetStaticMaterials().Num() != 1)
    {
        OutError = TEXT("A V5B prop/vegetation mesh lost its one-slot contract.");
        return false;
    }
    Mesh->Modify();
    Mesh->GetStaticMaterials()[0].MaterialInterface = Material;
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    OutError.Reset();
    return true;
}

bool ConfigureAccentTurfMesh(
    UStaticMesh* Mesh,
    UMaterialInterface* Material,
    bool bImportLod0InPlace,
    FString& OutError)
{
    if (!Mesh || Mesh->GetPathName() != AccentTurfObjectPath || !Material ||
        Material->GetPathName() != AccentMaterialObjectPath ||
        !ValidateAccentTurfSourceHashes(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact V5B accent-turf mesh/material/source roster is unavailable.");
        }
        return false;
    }
    if (bImportLod0InPlace)
    {
        if (!FbxMeshUtils::ImportStaticMeshLOD(
                Mesh, AccentTurfSource(0), 0, false))
        {
            OutError = TEXT("Could not import the exact V5B accent-turf LOD0 scratch mesh.");
            return false;
        }
        FAssetCompilingManager::Get().FinishAllCompilation();
        UStaticMesh* ImportedLodZero = FindObject<UStaticMesh>(
            nullptr, *AccentTurfLodImportScratchObjectPath);
        const FMeshDescription* ImportedDescription =
            ImportedLodZero ? ImportedLodZero->GetMeshDescription(0) : nullptr;
        if (!ImportedLodZero || ImportedLodZero == Mesh ||
            ImportedLodZero->GetPathName() !=
                AccentTurfLodImportScratchObjectPath ||
            ImportedLodZero->GetOutermost()->GetName() !=
                AccentTurfLodImportScratchPackageName ||
            !ImportedDescription || ImportedDescription->Vertices().Num() != 3910 ||
            ImportedDescription->Triangles().Num() != AccentTurfLod0Triangles)
        {
            OutError = TEXT("UE5.5 did not expose the exact unpersisted 3910-vertex/2550-triangle LOD0 scratch mesh.");
            return false;
        }
        for (const FString& Artifact :
             AccentTurfLodImportScratchArtifactCandidates())
        {
            if (IFileManager::Get().FileExists(*Artifact))
            {
                OutError = TEXT("The LOD0 scratch package acquired a persisted artifact; in-place replacement is refused: ") +
                    Artifact;
                return false;
            }
        }
        Mesh->Modify();
        if (!Mesh->CreateMeshDescription(0, *ImportedDescription))
        {
            OutError = TEXT("Could not copy the exact imported LOD0 mesh description into the persisted V5B carrier.");
            return false;
        }
        Mesh->CommitMeshDescription(0);
    }
    for (int32 LodIndex = 1; LodIndex <= 2; ++LodIndex)
    {
        if (!FbxMeshUtils::ImportStaticMeshLOD(
                Mesh, AccentTurfSource(LodIndex), LodIndex, false))
        {
            OutError = FString::Printf(
                TEXT("Could not import the exact V5B accent-turf LOD%d in place."),
                LodIndex);
            return false;
        }
    }
    TArray<UStaticMesh*> AccentMeshes = {Mesh};
    FStaticMeshCompilingManager::Get().FinishCompilation(AccentMeshes);
    if (Mesh->GetNumSourceModels() != 3 ||
        !BindSingleMaterial(Mesh, Material, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5B curved-blade turf carrier lost its exact three-source-LOD contract.");
        }
        return false;
    }

    Mesh->Modify();
    Mesh->bAutoComputeLODScreenSize = false;
    const float ScreenSizes[] = {
        1.0f, AccentTurfLod1ScreenSize, AccentTurfLod2ScreenSize};
    for (int32 LodIndex = 0; LodIndex < 3; ++LodIndex)
    {
        FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(LodIndex);
        SourceModel.BuildSettings.bUseHighPrecisionTangentBasis = true;
        SourceModel.BuildSettings.bUseFullPrecisionUVs = true;
        SourceModel.BuildSettings.bGenerateLightmapUVs = false;
        SourceModel.BuildSettings.bRecomputeNormals = false;
        SourceModel.BuildSettings.BuildScale3D = FVector::OneVector;
        SourceModel.ScreenSize.Default = ScreenSizes[LodIndex];
    }
    MakeRenderOnly(Mesh);
    Mesh->Build(false);
    Mesh->PostEditChange();
    Mesh->MarkPackageDirty();
    FStaticMeshCompilingManager::Get().FinishCompilation(AccentMeshes);
    if (TriangleCount(Mesh, 0) != AccentTurfLod0Triangles ||
        TriangleCount(Mesh, 1) != AccentTurfLod1Triangles ||
        TriangleCount(Mesh, 2) != AccentTurfLod2Triangles)
    {
        OutError = FString::Printf(
            TEXT("The V5B curved-blade LOD topology failed after import: actual=%d/%d/%d expected=%d/%d/%d."),
            TriangleCount(Mesh, 0),
            TriangleCount(Mesh, 1),
            TriangleCount(Mesh, 2),
            AccentTurfLod0Triangles,
            AccentTurfLod1Triangles,
            AccentTurfLod2Triangles);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ImportMeshes(
    IAssetTools& AssetTools,
    const TMap<FString, UMaterialInterface*>& Materials,
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    struct FPropSpec
    {
        FString Name;
        FString Object;
        int64 Bytes;
        int32 Triangles;
        FString Material;
    };
    const FPropSpec Props[] = {
        {SurfaceName, SurfaceObjectPath, 736198, 2048, SurfaceMaterialName},
        {FoamName, FoamObjectPath, 90183, 256, FoamMaterialName},
        {ImpactName, ImpactObjectPath, 178039, 512, FoamMaterialName},
        {PlumeName, PlumeObjectPath, 169152, 484, SprayMaterialName},
        {CentralPlumeName, CentralPlumeObjectPath, 322921, 912, SprayMaterialName},
        {PaverInnerName, PaverInnerObjectPath, 4328, 12, PaverMaterialName},
        {PaverOuterName, PaverOuterObjectPath, 4328, 12, PaverMaterialName}};

    TArray<UAssetImportTask*> Tasks;
    TArray<TStrongObjectPtr<UAssetImportTask>> TaskGuards;
    TaskGuards.Reserve(UE_ARRAY_COUNT(Props) + 6);
    TMap<FString, UAssetImportTask*> ByName;
    for (const FPropSpec& Spec : Props)
    {
        const FString Source = PropSource(Spec.Name);
        if (!ValidateSourceBytes(Source, Spec.Bytes, OutError))
        {
            return false;
        }
        UAssetImportTask* Task = MakeMeshTask(
            Source, PropMeshPath, Spec.Name, true, false, true, 1.0f);
        if (!Task)
        {
            OutError = TEXT("Could not allocate a V5B prop import task.");
            return false;
        }
        Tasks.Add(Task);
        TaskGuards.Add(TStrongObjectPtr<UAssetImportTask>(Task));
        ByName.Add(Spec.Name, Task);
    }

    if (!ValidateSourceBytes(PorticoSource(), 330419440, OutError))
    {
        return false;
    }
    UAssetImportTask* PorticoTask = MakeMeshTask(
        PorticoSource(), BuildingPath, PorticoName, true, false, true, 1.0f);
    if (!PorticoTask)
    {
        OutError = TEXT("Could not allocate the V5B trimmed-portico import task.");
        return false;
    }
    Tasks.Add(PorticoTask);
    TaskGuards.Add(TStrongObjectPtr<UAssetImportTask>(PorticoTask));

    if (!ValidateSourceBytes(
            HardscapeRenderSuccessorSource(), 587239, OutError) ||
        !ValidateSourceBytes(
            HardscapeRenderSuccessorMaterialLibrarySource(), 453, OutError))
    {
        return false;
    }
    UAssetImportTask* HardscapeRenderSuccessorTask = MakeMeshTask(
        HardscapeRenderSuccessorSource(),
        PropMeshPath,
        HardscapeRenderSuccessorName,
        true,
        false,
        true,
        1.0f);
    if (!HardscapeRenderSuccessorTask)
    {
        OutError = TEXT("Could not allocate the V5B no-legacy-bed hardscape successor import task.");
        return false;
    }
    Tasks.Add(HardscapeRenderSuccessorTask);
    TaskGuards.Add(
        TStrongObjectPtr<UAssetImportTask>(HardscapeRenderSuccessorTask));

    if (!ValidateAccentTurfSourceHashes(OutError))
    {
        return false;
    }
    UAssetImportTask* AccentTurfTask = MakeMeshTask(
        AccentTurfSource(0),
        VegetationMeshPath,
        AccentTurfName,
        true,
        false,
        true,
        1.0f);
    if (!AccentTurfTask)
    {
        OutError = TEXT("Could not allocate the V5B Bermuda turf-cluster import task.");
        return false;
    }
    Tasks.Add(AccentTurfTask);
    TaskGuards.Add(TStrongObjectPtr<UAssetImportTask>(AccentTurfTask));

    if (!ValidateSourceBytes(FormalBedSource(), 2142473, OutError))
    {
        return false;
    }
    UAssetImportTask* FormalBedTask = MakeMeshTask(
        FormalBedSource(),
        PropMeshPath,
        FormalBedName,
        true,
        false,
        true,
        1.0f);
    if (!FormalBedTask)
    {
        OutError = TEXT("Could not allocate the V5B raised formal-bed mound import task.");
        return false;
    }
    Tasks.Add(FormalBedTask);
    TaskGuards.Add(TStrongObjectPtr<UAssetImportTask>(FormalBedTask));

    if (!ValidateTreeBaseMulchSourceHashes(OutError))
    {
        return false;
    }
    UAssetImportTask* TreeBaseMulchTask = MakeMeshTask(
        TreeBaseMulchSource(),
        PropMeshPath,
        TreeBaseMulchName,
        true,
        false,
        true,
        1.0f);
    if (!TreeBaseMulchTask)
    {
        OutError = TEXT("Could not allocate the V5B tree-base mulch import task.");
        return false;
    }
    Tasks.Add(TreeBaseMulchTask);
    TaskGuards.Add(TStrongObjectPtr<UAssetImportTask>(TreeBaseMulchTask));

    if (!ValidateSourceBytes(PachiraFbxSource(), 2481564, OutError))
    {
        return false;
    }
    UAssetImportTask* PachiraTask = MakeMeshTask(
        PachiraFbxSource(), VegetationMeshPath, FString(), false, true, false, 1.0f);
    if (!PachiraTask)
    {
        OutError = TEXT("Could not allocate the uncombined V5B Pachira import task.");
        return false;
    }
    Tasks.Add(PachiraTask);
    TaskGuards.Add(TStrongObjectPtr<UAssetImportTask>(PachiraTask));
    AssetTools.ImportAssetTasks(Tasks);

    // UE 5.5 may prefix uncombined FBX node assets with the source filename.
    // Resolve by exact node-name suffix, then rename all eight still-unsaved
    // assets into the frozen final package/name roster before any binding.
    TArray<FAssetRenameData> PachiraRenames;
    TSet<UStaticMesh*> DiscoveredPachiraMeshes;
    TArray<UStaticMesh*> OrderedPachiraMeshes;
    OrderedPachiraMeshes.SetNum(PachiraPartNames().Num());
    for (int32 Index = 0; Index < PachiraPartNames().Num(); ++Index)
    {
        const FString& PartName = PachiraPartNames()[Index];
        UStaticMesh* Match = nullptr;
        int32 MatchCount = 0;
        for (UObject* Object : PachiraTask->GetObjects())
        {
            UStaticMesh* Candidate = Cast<UStaticMesh>(Object);
            if (Candidate &&
                (Candidate->GetName().Equals(PartName, ESearchCase::IgnoreCase) ||
                 Candidate->GetName().EndsWith(PartName, ESearchCase::IgnoreCase)))
            {
                Match = Candidate;
                ++MatchCount;
            }
        }
        if (MatchCount != 1 || !Match || DiscoveredPachiraMeshes.Contains(Match))
        {
            OutError = FString::Printf(
                TEXT("Uncombined Pachira import could not uniquely resolve node '%s' (matches=%d)."),
                *PartName,
                MatchCount);
            return false;
        }
        DiscoveredPachiraMeshes.Add(Match);
        OrderedPachiraMeshes[Index] = Match;
        if (Match->GetPathName() != PachiraObjectPaths()[Index])
        {
            PachiraRenames.Emplace(Match, VegetationMeshPath, PartName);
        }
    }
    if (DiscoveredPachiraMeshes.Num() != 8 ||
        (PachiraRenames.Num() > 0 && !AssetTools.RenameAssets(PachiraRenames)))
    {
        OutError = TEXT("The exact eight-part Pachira in-memory rename transaction failed.");
        return false;
    }
    for (UStaticMesh* Mesh : OrderedPachiraMeshes)
    {
        OutAssets.Add(Mesh);
    }

    TArray<UStaticMesh*> Compiled;
    for (const FPropSpec& Spec : Props)
    {
        UStaticMesh* Mesh = ResolveMesh(ByName.FindRef(Spec.Name), Spec.Object);
        if (!Mesh || !BindSingleMaterial(Mesh, Materials.FindRef(Spec.Material), OutError))
        {
            return false;
        }
        MakeRenderOnly(Mesh);
        Compiled.Add(Mesh);
        OutAssets.Add(Mesh);
    }

    UStaticMesh* AccentTurf = ResolveMesh(AccentTurfTask, AccentTurfObjectPath);
    if (!AccentTurf || !ConfigureAccentTurfMesh(
            AccentTurf,
            Materials.FindRef(AccentMaterialName),
            false,
            OutError))
    {
        OutError = TEXT("The derived V5B Bermuda turf cluster lost its exact mesh/material contract. ") + OutError;
        return false;
    }
    Compiled.Add(AccentTurf);
    OutAssets.Add(AccentTurf);

    UStaticMesh* FormalBed = ResolveMesh(FormalBedTask, FormalBedObjectPath);
    if (!FormalBed ||
        !BindSingleMaterial(
            FormalBed,
            Materials.FindRef(FormalBedMaterialName),
            OutError))
    {
        OutError = TEXT("The V5B raised formal-bed organic mound lost its exact mesh/material contract. ") + OutError;
        return false;
    }
    MakeRenderOnly(FormalBed);
    Compiled.Add(FormalBed);
    OutAssets.Add(FormalBed);

    UStaticMesh* TreeBaseMulch = ResolveMesh(
        TreeBaseMulchTask,
        TreeBaseMulchObjectPath);
    if (!TreeBaseMulch ||
        !BindSingleMaterial(
            TreeBaseMulch,
            Materials.FindRef(FormalBedMaterialName),
            OutError))
    {
        OutError = TEXT("The V5B tree-base mulch mound lost its exact mesh/material contract. ") + OutError;
        return false;
    }
    MakeRenderOnly(TreeBaseMulch);
    Compiled.Add(TreeBaseMulch);
    OutAssets.Add(TreeBaseMulch);

    UStaticMesh* Portico = ResolveMesh(PorticoTask, PorticoObjectPath);
    UStaticMesh* SourceHero = LoadExact<UStaticMesh>(SourceHeroPath);
    if (!Portico || !SourceHero ||
        !NormalizeAndBindPorticoMaterials(Portico, SourceHero, OutError))
    {
        OutError = TEXT("The trimmed V5B/source V5 building material contract failed. ") + OutError;
        return false;
    }
    Portico->Modify();
    MakeRenderOnly(Portico);
    Compiled.Add(Portico);
    OutAssets.Add(Portico);

    UStaticMesh* HardscapeRenderSuccessor = ResolveMesh(
        HardscapeRenderSuccessorTask,
        HardscapeRenderSuccessorObjectPath);
    UStaticMesh* SourceHardscape =
        LoadExact<UStaticMesh>(SourceHardscapePath);
    if (!HardscapeRenderSuccessor || !SourceHardscape ||
        !NormalizeAndBindHardscapeSuccessorMaterials(
            HardscapeRenderSuccessor,
            SourceHardscape,
            OutError))
    {
        OutError = TEXT("The V5B/source hardscape render-successor contract failed. ") +
            OutError;
        return false;
    }
    MakeRenderOnly(HardscapeRenderSuccessor);
    Compiled.Add(HardscapeRenderSuccessor);
    OutAssets.Add(HardscapeRenderSuccessor);

    for (int32 Index = 0; Index < PachiraPartNames().Num(); ++Index)
    {
        UStaticMesh* Mesh = ResolveMesh(PachiraTask, PachiraObjectPaths()[Index]);
        const bool bLeaves = PachiraPartNames()[Index].Contains(TEXT("_leaves_"));
        if (!Mesh || !BindSingleMaterial(
                Mesh,
                Materials.FindRef(bLeaves ? LeavesMaterialName : BarkMaterialName),
                OutError) ||
            !NormalizePachiraCentimeterGeometry(Mesh, OutError))
        {
            OutError = TEXT("Uncombined Pachira import did not produce the exact eight part names. ") + OutError;
            return false;
        }
        MakeRenderOnly(Mesh);
        Compiled.Add(Mesh);
    }
    FStaticMeshCompilingManager::Get().FinishCompilation(Compiled);

    for (const FPropSpec& Spec : Props)
    {
        UStaticMesh* Mesh = LoadExact<UStaticMesh>(Spec.Object);
        if (TriangleCount(Mesh) != Spec.Triangles)
        {
            OutError = FString::Printf(
                TEXT("V5B prop triangle contract failed for %s: expected %d, actual %d."),
                *Spec.Name,
                Spec.Triangles,
                TriangleCount(Mesh));
            return false;
        }
    }
    if (TriangleCount(AccentTurf, 0) != AccentTurfLod0Triangles ||
        TriangleCount(AccentTurf, 1) != AccentTurfLod1Triangles ||
        TriangleCount(AccentTurf, 2) != AccentTurfLod2Triangles)
    {
        OutError = FString::Printf(
            TEXT("V5B curved-blade turf carrier must contain exact LOD triangles %d/%d/%d; actual %d/%d/%d."),
            AccentTurfLod0Triangles,
            AccentTurfLod1Triangles,
            AccentTurfLod2Triangles,
            TriangleCount(AccentTurf, 0),
            TriangleCount(AccentTurf, 1),
            TriangleCount(AccentTurf, 2));
        return false;
    }
    if (TriangleCount(FormalBed) != 5696)
    {
        OutError = FString::Printf(
            TEXT("V5B raised formal-bed organic mound must contain exactly 5696 triangles; actual %d."),
            TriangleCount(FormalBed));
        return false;
    }
    if (TriangleCount(TreeBaseMulch) != 224)
    {
        OutError = FString::Printf(
            TEXT("V5B tree-base mulch mound must contain exactly 224 triangles; actual %d."),
            TriangleCount(TreeBaseMulch));
        return false;
    }
    if (TriangleCount(Portico) != 773158)
    {
        OutError = FString::Printf(
            TEXT("V5B trimmed building must contain exactly 773158 triangles; actual %d."),
            TriangleCount(Portico));
        return false;
    }
    if (TriangleCount(HardscapeRenderSuccessor) != 1568)
    {
        OutError = FString::Printf(
            TEXT("V5B hardscape render successor must contain exactly 1568 non-planting triangles; actual %d."),
            TriangleCount(HardscapeRenderSuccessor));
        return false;
    }
    OutError.Reset();
    return true;
}

TArray<FString> ExactObjectPaths()
{
    TArray<FString> Paths = {
        PorticoObjectPath,
        HardscapeRenderSuccessorObjectPath,
        SurfaceObjectPath,
        FoamObjectPath,
        ImpactObjectPath,
        PlumeObjectPath,
        CentralPlumeObjectPath,
        PaverInnerObjectPath,
        PaverOuterObjectPath,
        AccentTurfObjectPath,
        FormalBedObjectPath,
        TreeBaseMulchObjectPath,
        AccentMaterialObjectPath,
        FormalBedMaterialObjectPath,
        SurfaceMaterialObjectPath,
        FoamMaterialObjectPath,
        SprayMaterialObjectPath,
        PaverMaterialObjectPath,
        BarkMaterialObjectPath,
        LeavesMaterialObjectPath};
    Paths.Append(PachiraObjectPaths());
    for (const FTextureSpec& Spec : TextureSpecs())
    {
        Paths.Add(ObjectPath(TexturePath, Spec.AssetName));
    }
    Paths.Sort();
    return Paths;
}

bool MeshIsRenderOnly(const UStaticMesh* Mesh)
{
    if (!Mesh || Mesh->NaniteSettings.bEnabled)
    {
        return false;
    }
    const UBodySetup* Body = Mesh->GetBodySetup();
    return !Body || (Body->AggGeom.GetElementCount() == 0 &&
                     Body->CollisionTraceFlag != CTF_UseComplexAsSimple);
}

bool ValidateCompiledMaterial(UMaterialInterface* Material, FString& OutReport)
{
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(ERHIFeatureLevel::SM5)
        : nullptr;
    if (!Resource && Material)
    {
        // A cold unattended editor can load a valid persisted material before
        // it has allocated the active SM5 rendering resource. Cache shaders
        // without regenerating or dirtying the material, then validate the
        // same strict shader-map/default-fallback contract below.
        Material->ForceRecompileForRendering(
            EMaterialShaderPrecompileMode::Default);
        Material->EnsureIsComplete();
        Resource = Material->GetMaterialResource(ERHIFeatureLevel::SM5);
    }
    if (!Resource && Material && Material->GetMaterial())
    {
        Material->GetMaterial()->EnsureIsComplete();
        Resource = Material->GetMaterial()->GetMaterialResource(ERHIFeatureLevel::SM5);
    }
    if (Resource && !Resource->IsGameThreadShaderMapComplete())
    {
        Resource->SubmitCompileJobs_GameThread(EShaderCompileJobPriority::High);
        Resource->FinishCompilation();
    }
    FMaterialShaderMap* ShaderMap = Resource
        ? Resource->GetGameThreadShaderMap()
        : nullptr;
    const bool bMaterialMapDdcEnabled = IsMaterialMapDDCEnabled();
    const bool bShaderJobCacheDdcEnabled = IsShaderJobCacheDDCEnabled();
    // UE 5.5 intentionally leaves bCompiledSuccessfully unset on the
    // renderable finalized clone when per-shader DDC is active and full
    // material-map DDC is disabled (ShaderCompiler.cpp).  In that mode the
    // complete-map, finished-resource, no-errors checks below are the engine's
    // authoritative success state.
    const bool bCompileStateAccepted = ShaderMap &&
        (ShaderMap->CompiledSuccessfully() ||
         (!bMaterialMapDdcEnabled && bShaderJobCacheDdcEnabled));
    const bool bValid =
        Material && Resource && Resource->IsCompilationFinished() &&
        Resource->IsGameThreadShaderMapComplete() &&
        bCompileStateAccepted &&
        ShaderMap->IsValidForRendering() &&
        !Resource->IsDefaultMaterial() && Resource->GetCompileErrors().IsEmpty();
    if (!bValid)
    {
        const FString Errors = Resource
            ? FString::Join(Resource->GetCompileErrors(), TEXT(" | "))
            : TEXT("material resource absent");
        OutReport = FString::Printf(
            TEXT("A V5B material failed its compiled-SM5/default-fallback gate: %s resource=%d compilationFinished=%d gameThreadComplete=%d shaderMap=%d compiledSuccessfully=%d materialMapDDC=%d shaderJobCacheDDC=%d acceptedCompileState=%d validForRendering=%d defaultMaterial=%d errors=%s"),
            Material ? *Material->GetPathName() : TEXT("<null>"),
            Resource ? 1 : 0,
            Resource && Resource->IsCompilationFinished() ? 1 : 0,
            Resource && Resource->IsGameThreadShaderMapComplete() ? 1 : 0,
            ShaderMap ? 1 : 0,
            ShaderMap && ShaderMap->CompiledSuccessfully() ? 1 : 0,
            bMaterialMapDdcEnabled ? 1 : 0,
            bShaderJobCacheDdcEnabled ? 1 : 0,
            bCompileStateAccepted ? 1 : 0,
            ShaderMap && ShaderMap->IsValidForRendering() ? 1 : 0,
            Resource && Resource->IsDefaultMaterial() ? 1 : 0,
            *Errors);
        return false;
    }
    return true;
}

bool ValidateInternal(bool bRequireCleanPackages, FString& OutReport)
{
    TArray<FAssetData> Data;
    if (!GatherRootAssets(Data))
    {
        OutReport = TEXT("The Asset Registry did not complete V5B discovery.");
        return false;
    }
    const TArray<FString> ExpectedPaths = ExactObjectPaths();
    TSet<FString> Expected;
    for (const FString& Path : ExpectedPaths)
    {
        Expected.Add(Path);
    }
    TSet<FString> Actual;
    for (const FAssetData& Row : Data)
    {
        Actual.Add(Row.GetObjectPathString());
    }
	bool bRosterExact = Actual.Num() == Expected.Num();
	for (const FString& ExpectedPath : Expected)
	{
		bRosterExact = bRosterExact && Actual.Contains(ExpectedPath);
	}

	if (Data.Num() != 37 || Actual.Num() != 37 || !bRosterExact)
    {
        OutReport = FString::Printf(
            TEXT("V5B exact namespace roster failed: expected 37 assets, registry=%d unique=%d."),
            Data.Num(),
            Actual.Num());
        return false;
    }
    for (const FString& Path : ExpectedPaths)
    {
        UObject* Object = LoadObject<UObject>(nullptr, *Path);
        UPackage* Package = Object ? Object->GetOutermost() : nullptr;
        if (!Object || Object->GetPathName() != Path || !Package ||
            (bRequireCleanPackages && Package->IsDirty()))
        {
            OutReport = TEXT("A V5B object is absent, path-ambiguous, or dirty: ") + Path;
            return false;
        }
    }

    struct FExpectedMesh
    {
        FString Path;
        int32 Triangles;
        FString Material;
    };
    const FExpectedMesh Meshes[] = {
        {SurfaceObjectPath, 2048, SurfaceMaterialObjectPath},
        {FoamObjectPath, 256, FoamMaterialObjectPath},
        {ImpactObjectPath, 512, FoamMaterialObjectPath},
        {PlumeObjectPath, 484, SprayMaterialObjectPath},
        {CentralPlumeObjectPath, 912, SprayMaterialObjectPath},
        {PaverInnerObjectPath, 12, PaverMaterialObjectPath},
        {PaverOuterObjectPath, 12, PaverMaterialObjectPath},
        {TreeBaseMulchObjectPath, 224, FormalBedMaterialObjectPath}};
    for (const FExpectedMesh& Spec : Meshes)
    {
        UStaticMesh* Mesh = LoadExact<UStaticMesh>(Spec.Path);
        if (!MeshIsRenderOnly(Mesh) || TriangleCount(Mesh) != Spec.Triangles ||
            Mesh->GetStaticMaterials().Num() != 1 || !Mesh->GetMaterial(0) ||
            Mesh->GetMaterial(0)->GetPathName() != Spec.Material)
        {
            OutReport = TEXT("A V5B prop mesh lost topology/material/render-only state: ") + Spec.Path;
            return false;
        }
    }
    UStaticMesh* HardscapeRenderSuccessor =
        LoadExact<UStaticMesh>(HardscapeRenderSuccessorObjectPath);
    UStaticMesh* SourceHardscape = LoadExact<UStaticMesh>(SourceHardscapePath);
    const FName HardscapeNames[] = {
        FName(TEXT("M_IPV_Stone")),
        FName(TEXT("M_IPV_Water")),
        FName(TEXT("M_IPV_Metal"))};
    const int32 HardscapeTriangles[] = {640, 736, 192};
    const FStaticMeshRenderData* HardscapeRenderData =
        HardscapeRenderSuccessor
        ? HardscapeRenderSuccessor->GetRenderData()
        : nullptr;
    if (!MeshIsRenderOnly(HardscapeRenderSuccessor) || !SourceHardscape ||
        TriangleCount(HardscapeRenderSuccessor) != 1568 ||
        HardscapeRenderSuccessor->GetNumSourceModels() != 1 ||
        HardscapeRenderSuccessor->GetSourceModel(0).BuildSettings.BuildScale3D !=
            FVector::OneVector ||
        HardscapeRenderSuccessor->GetStaticMaterials().Num() != 3 ||
        !HardscapeRenderData ||
        HardscapeRenderData->LODResources.Num() != 1 ||
        HardscapeRenderData->LODResources[0].Sections.Num() != 3)
    {
        OutReport = TEXT("The V5B hardscape render successor lost its exact 1568-triangle/three-slot/render-only state.");
        return false;
    }
    TSet<int32> SeenHardscapeMaterialIndices;
    for (int32 ExpectedIndex = 0; ExpectedIndex < 3; ++ExpectedIndex)
    {
        const FStaticMaterial& SuccessorMaterial =
            HardscapeRenderSuccessor->GetStaticMaterials()[ExpectedIndex];
        const int32 SourceIndex =
            SourceHardscape->GetMaterialIndexFromImportedMaterialSlotName(
                HardscapeNames[ExpectedIndex]);
        if (SuccessorMaterial.MaterialSlotName != HardscapeNames[ExpectedIndex] ||
            SuccessorMaterial.ImportedMaterialSlotName !=
                HardscapeNames[ExpectedIndex] ||
            SourceIndex == INDEX_NONE ||
            SuccessorMaterial.MaterialInterface !=
                SourceHardscape->GetMaterial(SourceIndex))
        {
            OutReport = TEXT("The V5B hardscape successor material names or frozen source bindings drifted.");
            return false;
        }
    }
    for (const FStaticMeshSection& Section :
         HardscapeRenderData->LODResources[0].Sections)
    {
        if (Section.MaterialIndex < 0 || Section.MaterialIndex >= 3 ||
            SeenHardscapeMaterialIndices.Contains(Section.MaterialIndex) ||
            Section.NumTriangles !=
                HardscapeTriangles[Section.MaterialIndex])
        {
            OutReport = TEXT("The V5B hardscape successor section/material triangle census drifted.");
            return false;
        }
        SeenHardscapeMaterialIndices.Add(Section.MaterialIndex);
    }
    const FBoxSphereBounds HardscapeBounds =
        HardscapeRenderSuccessor->GetBounds();
    const FVector HardscapeMin =
        HardscapeBounds.Origin - HardscapeBounds.BoxExtent;
    const FVector HardscapeMax =
        HardscapeBounds.Origin + HardscapeBounds.BoxExtent;
    if (SeenHardscapeMaterialIndices.Num() != 3 ||
        !FMath::IsWithinInclusive(HardscapeMin.X, -1807.1f, -1806.9f) ||
        !FMath::IsWithinInclusive(HardscapeMin.Y, 7792.9f, 7793.1f) ||
        !FMath::IsWithinInclusive(HardscapeMin.Z, 15.1f, 15.35f) ||
        !FMath::IsWithinInclusive(HardscapeMax.X, 1806.9f, 1807.1f) ||
        !FMath::IsWithinInclusive(HardscapeMax.Y, 10849.9f, 10850.1f) ||
        !FMath::IsWithinInclusive(HardscapeMax.Z, 1415.1f, 1415.35f))
    {
        OutReport = FString::Printf(
            TEXT("The V5B hardscape successor lost its pinned non-planting bounds: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            HardscapeMin.X,
            HardscapeMin.Y,
            HardscapeMin.Z,
            HardscapeMax.X,
            HardscapeMax.Y,
            HardscapeMax.Z);
        return false;
    }
    UStaticMesh* AccentTurf = LoadExact<UStaticMesh>(AccentTurfObjectPath);
    if (!MeshIsRenderOnly(AccentTurf) ||
        TriangleCount(AccentTurf, 0) != AccentTurfLod0Triangles ||
        TriangleCount(AccentTurf, 1) != AccentTurfLod1Triangles ||
        TriangleCount(AccentTurf, 2) != AccentTurfLod2Triangles ||
        AccentTurf->GetNumSourceModels() != 3 ||
        AccentTurf->bAutoComputeLODScreenSize ||
        AccentTurf->GetStaticMaterials().Num() != 1 || !AccentTurf->GetMaterial(0) ||
        AccentTurf->GetMaterial(0)->GetPathName() != AccentMaterialObjectPath)
    {
        OutReport = TEXT("The V5B curved-blade turf carrier lost its exact 2550/680/204-triangle three-LOD/material/render-only state.");
        return false;
    }
    const float ExpectedAccentScreenSizes[] = {
        1.0f, AccentTurfLod1ScreenSize, AccentTurfLod2ScreenSize};
    for (int32 LodIndex = 0; LodIndex < 3; ++LodIndex)
    {
        const FStaticMeshSourceModel& SourceModel =
            AccentTurf->GetSourceModel(LodIndex);
        if (!SourceModel.BuildSettings.bUseHighPrecisionTangentBasis ||
            !SourceModel.BuildSettings.bUseFullPrecisionUVs ||
            SourceModel.BuildSettings.bGenerateLightmapUVs ||
            SourceModel.BuildSettings.bRecomputeNormals ||
            SourceModel.BuildSettings.BuildScale3D != FVector::OneVector ||
            !FMath::IsNearlyEqual(
                SourceModel.ScreenSize.Default,
                ExpectedAccentScreenSizes[LodIndex],
                0.000001f))
        {
            OutReport = FString::Printf(
                TEXT("The V5B curved-blade turf LOD%d lost its imported-normal, UV, scale, precision, or screen-size contract."),
                LodIndex);
            return false;
        }
    }
    const FBoxSphereBounds AccentBounds = AccentTurf->GetBounds();
    const FVector AccentMin = AccentBounds.Origin - AccentBounds.BoxExtent;
    const FVector AccentMax = AccentBounds.Origin + AccentBounds.BoxExtent;
    if (!FMath::IsWithinInclusive(AccentMin.X, -78.05f, -77.72f) ||
        !FMath::IsWithinInclusive(AccentMin.Y, -68.62f, -68.29f) ||
        !FMath::IsWithinInclusive(AccentMin.Z, -0.05f, 0.05f) ||
        !FMath::IsWithinInclusive(AccentMax.X, 79.02f, 79.35f) ||
        !FMath::IsWithinInclusive(AccentMax.Y, 68.22f, 68.55f) ||
        !FMath::IsWithinInclusive(AccentMax.Z, 4.33f, 4.47f))
    {
        OutReport = FString::Printf(
            TEXT("The V5B Bermuda turf cluster lost its pinned centimetre bounds: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            AccentMin.X,
            AccentMin.Y,
            AccentMin.Z,
            AccentMax.X,
            AccentMax.Y,
            AccentMax.Z);
        return false;
    }
    const FStaticMeshRenderData* AccentRenderData = AccentTurf->GetRenderData();
    if (!AccentRenderData || AccentRenderData->LODResources.Num() != 3)
    {
        OutReport = TEXT("The V5B curved-blade turf carrier lost its exact three render LODs.");
        return false;
    }
    const int32 ExpectedAccentVertices[] = {3910, 2040, 612};
    const int32 ExpectedAccentIndices[] = {7650, 2040, 612};
    const int32 ExpectedAccentSeeds[] = {680, 680, 204};
    const float MinimumNormalZ[] = {0.03f, 0.06f, 0.06f};
    const float MaximumNormalZ[] = {0.93f, 0.83f, 0.83f};
    for (int32 LodIndex = 0; LodIndex < 3; ++LodIndex)
    {
        const FStaticMeshLODResources& Lod =
            AccentRenderData->LODResources[LodIndex];
        const FStaticMeshVertexBuffer& VertexBuffer =
            Lod.VertexBuffers.StaticMeshVertexBuffer;
        const FPositionVertexBuffer& PositionBuffer =
            Lod.VertexBuffers.PositionVertexBuffer;
        if (!VertexBuffer.GetUseHighPrecisionTangentBasis() ||
            !VertexBuffer.GetUseFullPrecisionUVs() ||
            VertexBuffer.GetNumTexCoords() != 1 ||
            VertexBuffer.GetNumVertices() != ExpectedAccentVertices[LodIndex] ||
            PositionBuffer.GetNumVertices() != ExpectedAccentVertices[LodIndex] ||
            Lod.IndexBuffer.GetNumIndices() != ExpectedAccentIndices[LodIndex] ||
            Lod.Sections.Num() != 1 || Lod.Sections[0].MaterialIndex != 0)
        {
            OutReport = FString::Printf(
                TEXT("The V5B curved-blade turf LOD%d lost its exact high-precision vertex/index/UV0/material topology: highPrecision=%d fullPrecisionUv=%d texCoords=%d vertices=%u positionVertices=%u indices=%u sections=%d materialIndex=%d."),
                LodIndex,
                VertexBuffer.GetUseHighPrecisionTangentBasis() ? 1 : 0,
                VertexBuffer.GetUseFullPrecisionUVs() ? 1 : 0,
                VertexBuffer.GetNumTexCoords(),
                VertexBuffer.GetNumVertices(),
                PositionBuffer.GetNumVertices(),
                Lod.IndexBuffer.GetNumIndices(),
                Lod.Sections.Num(),
                Lod.Sections.Num() == 1 ? Lod.Sections[0].MaterialIndex : -1);
            return false;
        }
        float MinimumBladeSeed = 1.0f;
        float MaximumBladeSeed = 0.0f;
        TMap<int32, FIntVector> SeedHeightCounts;
        for (uint32 VertexIndex = 0;
             VertexIndex < VertexBuffer.GetNumVertices();
             ++VertexIndex)
        {
            const FVector2f Uv = VertexBuffer.GetVertexUV(VertexIndex, 0);
            const FVector3f Position = PositionBuffer.VertexPosition(VertexIndex);
            const FVector3f Tangent = VertexBuffer.VertexTangentX(VertexIndex);
            const FVector3f ImportedNormal =
                VertexBuffer.VertexTangentZ(VertexIndex);
            if (Uv.ContainsNaN() || Uv.X < -0.00025f || Uv.X > 1.00025f ||
                Uv.Y < -0.00025f || Uv.Y > 1.00025f ||
                Position.ContainsNaN() || Tangent.ContainsNaN() ||
                ImportedNormal.ContainsNaN() ||
                !FMath::IsNearlyEqual(Tangent.SizeSquared(), 1.0f, 0.002f) ||
                !FMath::IsNearlyEqual(
                    ImportedNormal.SizeSquared(), 1.0f, 0.002f) ||
                !FMath::IsWithinInclusive(
                    ImportedNormal.Z,
                    MinimumNormalZ[LodIndex],
                    MaximumNormalZ[LodIndex]))
            {
                OutReport = FString::Printf(
                    TEXT("The V5B curved-blade turf LOD%d lost finite unit UVs, positions, tangents, or its imported-normal envelope."),
                    LodIndex);
                return false;
            }
            const int32 SeedKey = FMath::RoundToInt(Uv.X * 100000000.0f);
            FIntVector& Counts = SeedHeightCounts.FindOrAdd(SeedKey);
            if (FMath::IsNearlyZero(Uv.Y, 0.00025f))
            {
                ++Counts.X;
            }
            else if (FMath::IsNearlyEqual(Uv.Y, 1.0f, 0.00025f))
            {
                ++Counts.Z;
            }
            else
            {
                ++Counts.Y;
            }
            MinimumBladeSeed = FMath::Min(MinimumBladeSeed, Uv.X);
            MaximumBladeSeed = FMath::Max(MaximumBladeSeed, Uv.X);
        }
        const bool bRequireFullSeedRange = LodIndex < 2;
        if (SeedHeightCounts.Num() != ExpectedAccentSeeds[LodIndex] ||
            (bRequireFullSeedRange &&
             (MinimumBladeSeed > 0.00001f || MaximumBladeSeed < 0.99999f)))
        {
            OutReport = FString::Printf(
                TEXT("The V5B curved-blade LOD%d roster/seed range drifted: blades=%d seed=%.6f..%.6f."),
                LodIndex,
                SeedHeightCounts.Num(),
                MinimumBladeSeed,
                MaximumBladeSeed);
            return false;
        }
        int32 ClippedSeedCount = 0;
        int32 JuvenileSeedCount = 0;
        for (const TPair<int32, FIntVector>& Pair : SeedHeightCounts)
        {
            const bool bExactRoster = LodIndex == 0
                ? Pair.Value.X == 2 && Pair.Value.Y == 2 &&
                    (Pair.Value.Z == 1 || Pair.Value.Z == 2)
                : Pair.Value == FIntVector(2, 0, 1);
            if (!bExactRoster)
            {
                OutReport = FString::Printf(
                    TEXT("The V5B curved-blade LOD%d seed %d lost its exact root/mid/tip UV roster."),
                    LodIndex,
                    Pair.Key);
                return false;
            }
            if (LodIndex == 0)
            {
                ClippedSeedCount += Pair.Value.Z == 2 ? 1 : 0;
                JuvenileSeedCount += Pair.Value.Z == 1 ? 1 : 0;
            }
        }
        if (LodIndex == 0 &&
            (ClippedSeedCount != 510 || JuvenileSeedCount != 170))
        {
            OutReport = FString::Printf(
                TEXT("The V5B curved-blade turf LOD0 lost its exact 510 clipped / 170 juvenile tip roster: actual=%d/%d."),
                ClippedSeedCount,
                JuvenileSeedCount);
            return false;
        }
    }
    UStaticMesh* FormalBed = LoadExact<UStaticMesh>(FormalBedObjectPath);
    if (!MeshIsRenderOnly(FormalBed) || TriangleCount(FormalBed) != 5696 ||
        FormalBed->GetNumSourceModels() != 1 ||
        FormalBed->GetSourceModel(0).BuildSettings.BuildScale3D != FVector::OneVector ||
        FormalBed->GetStaticMaterials().Num() != 1 || !FormalBed->GetMaterial(0) ||
        FormalBed->GetMaterial(0)->GetPathName() != FormalBedMaterialObjectPath)
    {
        OutReport = TEXT("The V5B raised formal-bed organic mound lost its exact 5696-triangle/material/render-only state.");
        return false;
    }
    const FStaticMeshRenderData* FormalBedRenderData =
        FormalBed->GetRenderData();
    if (!FormalBedRenderData || FormalBedRenderData->LODResources.Num() != 1 ||
        FormalBedRenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer
                .GetNumTexCoords() < 1)
    {
        OutReport = TEXT("The V5B formal-bed mound lost its apron-fraction UV0 channel.");
        return false;
    }
    bool SeenApronFractions[11] = {};
    const FStaticMeshVertexBuffer& FormalBedVertexBuffer =
        FormalBedRenderData->LODResources[0].VertexBuffers.StaticMeshVertexBuffer;
    for (uint32 VertexIndex = 0;
         VertexIndex < FormalBedVertexBuffer.GetNumVertices();
         ++VertexIndex)
    {
        const FVector2f Uv = FormalBedVertexBuffer.GetVertexUV(VertexIndex, 0);
        const int32 Tenth = FMath::RoundToInt(Uv.X * 10.0f);
        if (Tenth < 0 || Tenth > 10 ||
            !FMath::IsNearlyEqual(Uv.X, Tenth / 10.0f, 0.00025f) ||
            !FMath::IsNearlyEqual(Uv.Y, 1.0f, 0.00025f))
        {
            OutReport = FString::Printf(
                TEXT("The V5B formal-bed apron UV0 escaped its exact 0.1 ring lattice/imported formal-bed sentinel at vertex %u: (%.6f,%.6f)."),
                VertexIndex,
                Uv.X,
                Uv.Y);
            return false;
        }
        SeenApronFractions[Tenth] = true;
    }
    for (int32 Tenth = 0; Tenth <= 10; ++Tenth)
    {
        if (!SeenApronFractions[Tenth])
        {
            OutReport = FString::Printf(
                TEXT("The V5B formal-bed apron UV0.x is missing exact ring fraction %.1f."),
                Tenth / 10.0f);
            return false;
        }
    }
    const FBoxSphereBounds FormalBedBounds = FormalBed->GetBounds();
    const FVector FormalBedMin =
        FormalBedBounds.Origin - FormalBedBounds.BoxExtent;
    const FVector FormalBedMax =
        FormalBedBounds.Origin + FormalBedBounds.BoxExtent;
    if (!FMath::IsWithinInclusive(FormalBedMin.X, -5229.1f, -5228.5f) ||
        !FMath::IsWithinInclusive(FormalBedMin.Y, 7495.3f, 7496.0f) ||
        !FMath::IsWithinInclusive(FormalBedMin.Z, -15.7f, -15.0f) ||
        !FMath::IsWithinInclusive(FormalBedMax.X, 5228.5f, 5229.1f) ||
        !FMath::IsWithinInclusive(FormalBedMax.Y, 10097.8f, 10098.5f) ||
        !FMath::IsWithinInclusive(FormalBedMax.Z, 43.7f, 44.5f))
    {
        OutReport = FString::Printf(
            TEXT("The V5B raised formal-bed mound lost its pinned collision-envelope bounds: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            FormalBedMin.X,
            FormalBedMin.Y,
            FormalBedMin.Z,
            FormalBedMax.X,
            FormalBedMax.Y,
            FormalBedMax.Z);
        return false;
    }
    UStaticMesh* TreeBaseMulch =
        LoadExact<UStaticMesh>(TreeBaseMulchObjectPath);
    const FBoxSphereBounds TreeBaseMulchBounds = TreeBaseMulch
        ? TreeBaseMulch->GetBounds()
        : FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0);
    const FVector TreeBaseMulchMin =
        TreeBaseMulchBounds.Origin - TreeBaseMulchBounds.BoxExtent;
    const FVector TreeBaseMulchMax =
        TreeBaseMulchBounds.Origin + TreeBaseMulchBounds.BoxExtent;
    const FStaticMaterial* TreeBaseMulchSlot =
        TreeBaseMulch && TreeBaseMulch->GetStaticMaterials().Num() == 1
        ? &TreeBaseMulch->GetStaticMaterials()[0]
        : nullptr;
    if (!MeshIsRenderOnly(TreeBaseMulch) ||
        TriangleCount(TreeBaseMulch) != 224 ||
        !TreeBaseMulchSlot ||
        TreeBaseMulchSlot->MaterialSlotName != FName(FormalBedMaterialName) ||
        TreeBaseMulchSlot->ImportedMaterialSlotName !=
            FName(FormalBedMaterialName) ||
        !TreeBaseMulch->GetMaterial(0) ||
        TreeBaseMulch->GetMaterial(0)->GetPathName() !=
            FormalBedMaterialObjectPath ||
        !TreeBaseMulchMin.Equals(
            FVector(-102.8296f, -97.5176f, -0.45f), 0.05f) ||
        !TreeBaseMulchMax.Equals(
            FVector(109.1395f, 108.0829f, 1.8f), 0.05f))
    {
        OutReport = FString::Printf(
            TEXT("The V5B tree-base mulch mound lost its exact 224-triangle/material/render-only/bounds contract: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)."),
            TreeBaseMulchMin.X,
            TreeBaseMulchMin.Y,
            TreeBaseMulchMin.Z,
            TreeBaseMulchMax.X,
            TreeBaseMulchMax.Y,
            TreeBaseMulchMax.Z);
        return false;
    }
    const FStaticMeshRenderData* TreeBaseMulchRenderData =
        TreeBaseMulch->GetRenderData();
    if (!TreeBaseMulchRenderData ||
        TreeBaseMulchRenderData->LODResources.Num() != 1 ||
        TreeBaseMulchRenderData->LODResources[0]
                .VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords() < 1)
    {
        OutReport = TEXT("The V5B tree-base mulch mound lost its radial/sentinel UV0 channel.");
        return false;
    }
    constexpr float ExpectedTreeMulchRadialFractions[] = {
        0.0f,
        0.30f,
        0.65f,
        0.88f,
        1.0f};
    bool SeenTreeMulchRadialFractions[
        UE_ARRAY_COUNT(ExpectedTreeMulchRadialFractions)] = {};
    const FStaticMeshVertexBuffer& TreeBaseMulchVertexBuffer =
        TreeBaseMulchRenderData->LODResources[0]
            .VertexBuffers.StaticMeshVertexBuffer;
    for (uint32 VertexIndex = 0;
         VertexIndex < TreeBaseMulchVertexBuffer.GetNumVertices();
         ++VertexIndex)
    {
        const FVector2f Uv =
            TreeBaseMulchVertexBuffer.GetVertexUV(VertexIndex, 0);
        int32 MatchingRadialIndex = INDEX_NONE;
        for (int32 RadialIndex = 0;
             RadialIndex < UE_ARRAY_COUNT(ExpectedTreeMulchRadialFractions);
             ++RadialIndex)
        {
            if (FMath::IsNearlyEqual(
                    Uv.X,
                    ExpectedTreeMulchRadialFractions[RadialIndex],
                    0.00025f))
            {
                MatchingRadialIndex = RadialIndex;
                break;
            }
        }
        if (Uv.ContainsNaN() || MatchingRadialIndex == INDEX_NONE ||
            !FMath::IsNearlyZero(Uv.Y, 0.00025f))
        {
            OutReport = FString::Printf(
                TEXT("The V5B tree-base mulch UV0 lost its exact radial fraction/imported tree-mulch sentinel at vertex %u: (%.6f,%.6f)."),
                VertexIndex,
                Uv.X,
                Uv.Y);
            return false;
        }
        SeenTreeMulchRadialFractions[MatchingRadialIndex] = true;
    }
    for (int32 RadialIndex = 0;
         RadialIndex < UE_ARRAY_COUNT(ExpectedTreeMulchRadialFractions);
         ++RadialIndex)
    {
        if (!SeenTreeMulchRadialFractions[RadialIndex])
        {
            OutReport = FString::Printf(
                TEXT("The V5B tree-base mulch UV0 is missing exact radial fraction %.2f."),
                ExpectedTreeMulchRadialFractions[RadialIndex]);
            return false;
        }
    }
    UStaticMesh* Portico = LoadExact<UStaticMesh>(PorticoObjectPath);
    UStaticMesh* SourceHero = LoadExact<UStaticMesh>(SourceHeroPath);
    if (!MeshIsRenderOnly(Portico) || TriangleCount(Portico) != 773158 ||
        !SourceHero || Portico->GetStaticMaterials().Num() != 11 ||
        SourceHero->GetStaticMaterials().Num() != 11)
    {
        OutReport = TEXT("The V5B trimmed building lost its exact render-only 773158-triangle/11-slot state.");
        return false;
    }
    for (int32 Slot = 0; Slot < 11; ++Slot)
    {
        const FStaticMaterial& Target = Portico->GetStaticMaterials()[Slot];
        const FStaticMaterial& Source = SourceHero->GetStaticMaterials()[Slot];
        if (Target.MaterialSlotName != Source.MaterialSlotName ||
            Target.ImportedMaterialSlotName != Source.ImportedMaterialSlotName ||
            Target.MaterialInterface != Source.MaterialInterface)
        {
            OutReport = TEXT("The V5B trimmed building no longer mirrors the exact V5 material order/bindings.");
            return false;
        }
    }
    int64 PachiraTriangles = 0;
    float PachiraMinZ = TNumericLimits<float>::Max();
    float PachiraMaxZ = TNumericLimits<float>::Lowest();
    for (int32 Index = 0; Index < PachiraObjectPaths().Num(); ++Index)
    {
        UStaticMesh* Mesh = LoadExact<UStaticMesh>(PachiraObjectPaths()[Index]);
        const bool bLeaves = PachiraPartNames()[Index].Contains(TEXT("_leaves_"));
        const FString& ExpectedMaterial = bLeaves
            ? LeavesMaterialObjectPath
            : BarkMaterialObjectPath;
        const int32 Count = TriangleCount(Mesh);
        if (!MeshIsRenderOnly(Mesh) || Mesh->GetNumSourceModels() != 1 ||
            Mesh->GetSourceModel(0).BuildSettings.BuildScale3D != FVector::OneVector ||
            Count != PachiraTriangleCounts()[Index] ||
            Mesh->GetStaticMaterials().Num() != 1 || !Mesh->GetMaterial(0) ||
            Mesh->GetMaterial(0)->GetPathName() != ExpectedMaterial)
        {
            OutReport = FString::Printf(
                TEXT("A V5B Pachira part lost exact naming/topology/material/render-only state: %s expectedTriangles=%d actualTriangles=%d."),
                *PachiraObjectPaths()[Index],
                PachiraTriangleCounts()[Index],
                Count);
            return false;
        }
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        PachiraMinZ = FMath::Min(PachiraMinZ, Bounds.Origin.Z - Bounds.BoxExtent.Z);
        PachiraMaxZ = FMath::Max(PachiraMaxZ, Bounds.Origin.Z + Bounds.BoxExtent.Z);
        PachiraTriangles += Count;
    }
    if (PachiraTriangles != 29174 ||
        !FMath::IsWithinInclusive(PachiraMinZ, -5.0f, 5.0f) ||
        !FMath::IsWithinInclusive(PachiraMaxZ, 180.0f, 200.0f))
    {
        OutReport = FString::Printf(
            TEXT("The eight-part Pachira census/centimeter-scale contract failed: triangles=%lld minZ=%.3f maxZ=%.3f."),
            PachiraTriangles,
            PachiraMinZ,
            PachiraMaxZ);
        return false;
    }
    for (const FTextureSpec& Spec : TextureSpecs())
    {
        UTexture2D* Texture = LoadExact<UTexture2D>(ObjectPath(TexturePath, Spec.AssetName));
        const bool bBermudaOpacity =
            Spec.AssetName == TEXT("T_IPV5B_Bermuda_Opacity");
        const bool bPachiraLeavesOpacity =
            Spec.AssetName == PachiraLeavesOpacityTextureName;
        const bool bPreserveMaskedAlphaCoverage =
            bBermudaOpacity || bPachiraLeavesOpacity;
        const bool bBermudaBladeTexture = bBermudaOpacity ||
            Spec.AssetName == TEXT("T_IPV5B_Bermuda_BaseColorPadded");
        if (!Texture || Texture->SRGB != Spec.bSrgb ||
            Texture->CompressionSettings != Spec.Compression ||
            Texture->bFlipGreenChannel != Spec.bFlipGreen ||
            Texture->AddressX != (bBermudaBladeTexture ? TA_Clamp : TA_Wrap) ||
            Texture->AddressY != (bBermudaBladeTexture ? TA_Clamp : TA_Wrap) ||
            Texture->MipGenSettings !=
                (bPreserveMaskedAlphaCoverage
                    ? TMGS_Sharpen1
                    : TMGS_FromTextureGroup) ||
            Texture->bDoScaleMipsForAlphaCoverage !=
                bPreserveMaskedAlphaCoverage ||
            !Texture->AlphaCoverageThresholds.Equals(
                bBermudaOpacity
                    ? FVector4(0.28, 0.0, 0.0, 0.0)
                    : bPachiraLeavesOpacity
                    ? FVector4(
                          PachiraOpacityClipValue, 0.0, 0.0, 0.0)
                    : FVector4(0.0, 0.0, 0.0, 0.0),
                0.000001))
        {
            OutReport = TEXT("A V5B source texture lost color/mask/NormalGL-to-DX/addressing/alpha-coverage state: ") + Spec.AssetName;
            return false;
        }
    }
    UMaterial* Surface = LoadExact<UMaterial>(SurfaceMaterialObjectPath);
    UMaterial* Foam = LoadExact<UMaterial>(FoamMaterialObjectPath);
    UMaterial* Spray = LoadExact<UMaterial>(SprayMaterialObjectPath);
    UMaterial* Bark = LoadExact<UMaterial>(BarkMaterialObjectPath);
    UMaterial* Leaves = LoadExact<UMaterial>(LeavesMaterialObjectPath);
    UMaterial* FormalBedMaterial =
        LoadExact<UMaterial>(FormalBedMaterialObjectPath);
    if (!Surface || Surface->BlendMode != BLEND_Opaque ||
        !Foam || Foam->BlendMode != BLEND_Translucent ||
        !Spray || Spray->BlendMode != BLEND_Translucent ||
        !Bark || Bark->BlendMode != BLEND_Opaque ||
        !Leaves || Leaves->BlendMode != BLEND_Masked || !Leaves->TwoSided ||
        !FormalBedMaterial || FormalBedMaterial->BlendMode != BLEND_Opaque ||
        FormalBedMaterial->TwoSided || !FormalBedMaterial->bTangentSpaceNormal ||
        !FMath::IsNearlyZero(
            FormalBedMaterial->MaxWorldPositionOffsetDisplacement, 0.000001f) ||
        !Surface->bUsedWithInstancedStaticMeshes ||
        !Foam->bUsedWithInstancedStaticMeshes ||
        !Spray->bUsedWithInstancedStaticMeshes ||
        !Bark->bUsedWithInstancedStaticMeshes ||
        !Leaves->bUsedWithInstancedStaticMeshes ||
        !FormalBedMaterial->bUsedWithInstancedStaticMeshes)
    {
        OutReport = TEXT("A V5B material lost its opaque/no-refraction, translucent-detail, or masked foliage policy.");
        return false;
    }
    if (!ValidatePachiraMaterialGraph(Bark, false, OutReport) ||
        !ValidatePachiraMaterialGraph(Leaves, true, OutReport))
    {
        return false;
    }
    const UMaterialEditorOnlyData* FormalBedData =
        FormalBedMaterial->GetEditorOnlyData();
    const UMaterialExpressionCustom* FormalBedBase = nullptr;
    const UMaterialExpressionCustom* FormalBedRoughness = nullptr;
    const UMaterialExpressionCustom* FormalBedNormal = nullptr;
    const UMaterialExpressionScalarParameter* FormalBedSpecular = nullptr;
    const UMaterialExpressionWorldPosition* FormalBedWorldPosition = nullptr;
    const UMaterialExpressionTextureCoordinate* FormalBedApronUv = nullptr;
    if (FormalBedData)
    {
        for (const UMaterialExpression* Expression :
             FormalBedData->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionCustom* Custom =
                    Cast<UMaterialExpressionCustom>(Expression))
            {
                if (Custom->Description == FormalBedBaseDescription &&
                    Custom->Code == FormalBedBaseCode)
                {
                    FormalBedBase = Custom;
                }
                else if (Custom->Description == FormalBedRoughnessDescription &&
                    Custom->Code == FormalBedRoughnessCode)
                {
                    FormalBedRoughness = Custom;
                }
                else if (Custom->Description == FormalBedNormalDescription &&
                    Custom->Code == FormalBedNormalCode)
                {
                    FormalBedNormal = Custom;
                }
            }
            if (const UMaterialExpressionScalarParameter* Scalar =
                    Cast<UMaterialExpressionScalarParameter>(Expression))
            {
                if (Scalar->ParameterName == TEXT("Specular"))
                {
                    FormalBedSpecular = Scalar;
                }
            }
            if (const UMaterialExpressionWorldPosition* Position =
                    Cast<UMaterialExpressionWorldPosition>(Expression))
            {
                if (Position->Desc == TEXT("V5B_FORMAL_BED_WORLD_POSITION"))
                {
                    FormalBedWorldPosition = Position;
                }
            }
            if (const UMaterialExpressionTextureCoordinate* TextureCoordinate =
                    Cast<UMaterialExpressionTextureCoordinate>(Expression))
            {
                if (TextureCoordinate->Desc ==
                    TEXT("V5B_FORMAL_BED_APRON_FRACTION_UV0"))
                {
                    FormalBedApronUv = TextureCoordinate;
                }
            }
        }
    }
    if (!FormalBedData ||
        FormalBedData->ExpressionCollection.Expressions.Num() != 6 ||
        !FormalBedBase || !FormalBedRoughness || !FormalBedNormal ||
        !FormalBedSpecular ||
        !FormalBedWorldPosition || !FormalBedApronUv ||
        FormalBedWorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        FormalBedApronUv->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(FormalBedApronUv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(FormalBedApronUv->VTiling, 1.0f, 0.000001f) ||
        FormalBedBase->Inputs.Num() != 2 ||
        FormalBedBase->Inputs[0].InputName != FName(TEXT("WorldPosition")) ||
        FormalBedBase->Inputs[0].Input.Expression != FormalBedWorldPosition ||
        FormalBedBase->Inputs[0].Input.OutputIndex != 0 ||
        FormalBedBase->Inputs[1].InputName != FName(TEXT("ApronUV")) ||
        FormalBedBase->Inputs[1].Input.Expression != FormalBedApronUv ||
        FormalBedBase->Inputs[1].Input.OutputIndex != 0 ||
        FormalBedBase->OutputType != CMOT_Float3 ||
        FormalBedRoughness->Inputs.Num() != 1 ||
        FormalBedRoughness->Inputs[0].InputName !=
            FName(TEXT("WorldPosition")) ||
        FormalBedRoughness->Inputs[0].Input.Expression !=
            FormalBedWorldPosition ||
        FormalBedRoughness->Inputs[0].Input.OutputIndex != 0 ||
        FormalBedRoughness->OutputType != CMOT_Float1 ||
        FormalBedNormal->Inputs.Num() != 1 ||
        FormalBedNormal->Inputs[0].InputName !=
            FName(TEXT("WorldPosition")) ||
        FormalBedNormal->Inputs[0].Input.Expression !=
            FormalBedWorldPosition ||
        FormalBedNormal->Inputs[0].Input.OutputIndex != 0 ||
        FormalBedNormal->OutputType != CMOT_Float3 ||
        !FMath::IsNearlyEqual(
            FormalBedSpecular->DefaultValue, 0.08f, 0.000001f) ||
        FormalBedData->BaseColor.Expression != FormalBedBase ||
        FormalBedData->BaseColor.OutputIndex != 0 ||
        FormalBedData->Roughness.Expression != FormalBedRoughness ||
        FormalBedData->Roughness.OutputIndex != 0 ||
        FormalBedData->Specular.Expression != FormalBedSpecular ||
        FormalBedData->Specular.OutputIndex != 0 ||
        FormalBedData->Normal.Expression != FormalBedNormal ||
        FormalBedData->Normal.OutputIndex != 0 ||
        FormalBedData->WorldPositionOffset.Expression)
    {
        OutReport = TEXT("V5B formal-bed soil lost its exact apron-UV/value-noise color/roughness/micro-normal contract.");
        return false;
    }
    UMaterial* Accent = LoadExact<UMaterial>(AccentMaterialObjectPath);
    UMaterial* Paver = LoadExact<UMaterial>(PaverMaterialObjectPath);
    const UMaterialEditorOnlyData* AccentData =
        Accent ? Accent->GetEditorOnlyData() : nullptr;
    if (!Accent || !AccentData || Accent->BlendMode != BLEND_Masked ||
        !Accent->TwoSided || !Accent->bUsedWithInstancedStaticMeshes ||
        Accent->bTangentSpaceNormal ||
        !Accent->GetShadingModels().HasOnlyShadingModel(MSM_TwoSidedFoliage) ||
        !FMath::IsNearlyEqual(Accent->OpacityMaskClipValue, 0.50f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            Accent->MaxWorldPositionOffsetDisplacement, 0.55f, 0.000001f))
    {
        OutReport = TEXT("V5B masked/two-sided turf lost its exact policy.");
        return false;
    }

    const UMaterialEditorOnlyData* PaverData =
        Paver ? Paver->GetEditorOnlyData() : nullptr;
    const UMaterialExpressionTextureCoordinate* PaverUv = nullptr;
    const UMaterialExpression* PaverRandom = nullptr;
    const UMaterialExpressionCustom* PaverBase = nullptr;
    const UMaterialExpressionCustom* PaverRoughness = nullptr;
    const UMaterialExpressionCustom* PaverNormal = nullptr;
    const UMaterialExpressionScalarParameter* PaverSpecular = nullptr;
    int32 PaverUvNodes = 0;
    int32 PaverRandomNodes = 0;
    int32 PaverBaseNodes = 0;
    int32 PaverRoughnessNodes = 0;
    int32 PaverNormalNodes = 0;
    int32 PaverSpecularNodes = 0;
    if (PaverData)
    {
        for (const UMaterialExpression* Expression :
             PaverData->ExpressionCollection.Expressions)
        {
            if (const UMaterialExpressionTextureCoordinate* TextureCoordinate =
                    Cast<UMaterialExpressionTextureCoordinate>(Expression))
            {
                if (TextureCoordinate->Desc == PaverUvDescription)
                {
                    PaverUv = TextureCoordinate;
                    ++PaverUvNodes;
                }
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom") &&
                Expression->Desc == PaverRandomDescription)
            {
                PaverRandom = Expression;
                ++PaverRandomNodes;
            }
            if (const UMaterialExpressionCustom* Custom =
                    Cast<UMaterialExpressionCustom>(Expression))
            {
                if (Custom->Description == PaverBaseDescription &&
                    Custom->Code == PaverBaseCode)
                {
                    PaverBase = Custom;
                    ++PaverBaseNodes;
                }
                else if (Custom->Description == PaverRoughnessDescription &&
                    Custom->Code == PaverRoughnessCode)
                {
                    PaverRoughness = Custom;
                    ++PaverRoughnessNodes;
                }
                else if (Custom->Description == PaverNormalDescription &&
                    Custom->Code == PaverNormalCode)
                {
                    PaverNormal = Custom;
                    ++PaverNormalNodes;
                }
            }
            if (const UMaterialExpressionScalarParameter* Scalar =
                    Cast<UMaterialExpressionScalarParameter>(Expression))
            {
                if (Scalar->Desc == PaverSpecularDescription &&
                    Scalar->ParameterName == TEXT("Specular") &&
                    FMath::IsNearlyEqual(
                        Scalar->DefaultValue, 0.16f, 0.000001f))
                {
                    PaverSpecular = Scalar;
                    ++PaverSpecularNodes;
                }
            }
        }
    }
    const auto PaverInputMatches = [](
        const FExpressionInput& Input,
        const UMaterialExpression* ExpectedExpression,
        int32 ExpectedOutputIndex)
    {
        return Input.Expression == ExpectedExpression &&
            Input.OutputIndex == ExpectedOutputIndex;
    };
    const auto PaverCustomMatches = [&PaverInputMatches, PaverUv, PaverRandom](
        const UMaterialExpressionCustom* Custom,
        ECustomMaterialOutputType ExpectedOutputType)
    {
        return Custom && Custom->OutputType == ExpectedOutputType &&
            Custom->Inputs.Num() == 2 &&
            Custom->Inputs[0].InputName == FName(TEXT("UV")) &&
            PaverInputMatches(Custom->Inputs[0].Input, PaverUv, 0) &&
            Custom->Inputs[1].InputName == FName(TEXT("Random01")) &&
            PaverInputMatches(Custom->Inputs[1].Input, PaverRandom, 0);
    };
    if (!Paver || Paver->GetClass() != UMaterial::StaticClass() ||
        Paver->MaterialDomain != MD_Surface || Paver->BlendMode != BLEND_Opaque ||
        Paver->TwoSided || !Paver->bTangentSpaceNormal ||
        !Paver->bUsedWithInstancedStaticMeshes ||
        !Paver->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !FMath::IsNearlyZero(
            Paver->MaxWorldPositionOffsetDisplacement, 0.000001f) ||
        !PaverData || PaverData->ExpressionCollection.Expressions.Num() != 6 ||
        PaverUvNodes != 1 || PaverRandomNodes != 1 || PaverBaseNodes != 1 ||
        PaverRoughnessNodes != 1 || PaverNormalNodes != 1 ||
        PaverSpecularNodes != 1 || !PaverUv ||
        PaverUv->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(PaverUv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(PaverUv->VTiling, 1.0f, 0.000001f) ||
        !PaverCustomMatches(PaverBase, CMOT_Float3) ||
        !PaverCustomMatches(PaverRoughness, CMOT_Float1) ||
        !PaverCustomMatches(PaverNormal, CMOT_Float3) ||
        !PaverInputMatches(PaverData->BaseColor, PaverBase, 0) ||
        !PaverInputMatches(PaverData->Roughness, PaverRoughness, 0) ||
        !PaverInputMatches(PaverData->Normal, PaverNormal, 0) ||
        !PaverInputMatches(PaverData->Specular, PaverSpecular, 0) ||
        PaverData->EmissiveColor.Expression || PaverData->Opacity.Expression ||
        PaverData->OpacityMask.Expression ||
        PaverData->WorldPositionOffset.Expression ||
        PaverData->SubsurfaceColor.Expression || PaverData->Refraction.Expression)
    {
        OutReport = TEXT("V5B paver lost its exact cook-safe instanced procedural-stone material graph.");
        return false;
    }
    const TMap<FName, float> ExpectedAccentScalars = {
        {TEXT("Roughness"), 0.80f},
        {TEXT("Specular"), 0.30f},
        {TEXT("TRIAD_WindStrengthCm"), 0.48f},
        {TEXT("TRIAD_WindSpeed"), 0.12f},
        {TEXT("TRIAD_WindResponseScale"), 0.28f},
        {TEXT("TRIAD_MaxWpoCm"), 0.55f},
        {TEXT("TRIAD_WindHeightCm"),
         AccentFrozenMaterialWindHeightNormalizerCm}};
    TSet<FName> SeenAccentScalars;
    TMap<FName, const UMaterialExpressionScalarParameter*> ExactScalarNodes;
    const UMaterialExpressionCustom* ExactBladeColor = nullptr;
    const UMaterialExpressionCustom* ExactWind = nullptr;
    const UMaterialExpressionTextureCoordinate* BladeUv = nullptr;
    const UMaterialExpressionConstant3Vector* WorldUpNormal = nullptr;
    const UMaterialExpressionVertexNormalWS* ImportedCurvatureNormal = nullptr;
    const UMaterialExpressionTwoSidedSign* TwoSidedSign = nullptr;
    const UMaterialExpressionMultiply* FacingCorrectedNormal = nullptr;
    const UMaterialExpressionLinearInterpolate* DistanceMatchedNormal = nullptr;
    const UMaterialExpressionConstant3Vector* SubsurfaceTint = nullptr;
    const UMaterialExpressionMaterialFunctionCall* DitheredInstanceFade = nullptr;
    const UMaterialExpressionWorldPosition* WorldPosition = nullptr;
    const UMaterialExpressionTransformPosition* InstanceLocalPosition = nullptr;
    const UMaterialExpressionTime* Time = nullptr;
    const UMaterialExpressionVectorParameter* WindDirection = nullptr;
    const UMaterialExpression* InstanceFade = nullptr;
    const UMaterialExpression* InstanceRandom = nullptr;
    int32 ExactWindNodes = 0;
    int32 ExactBladeColorNodes = 0;
    int32 BladeUvNodes = 0;
    int32 WorldUpNormalNodes = 0;
    int32 ImportedCurvatureNormalNodes = 0;
    int32 TwoSidedSignNodes = 0;
    int32 FacingCorrectedNormalNodes = 0;
    int32 DistanceMatchedNormalNodes = 0;
    int32 SubsurfaceNodes = 0;
    int32 TextureSampleNodes = 0;
    int32 InstanceFadeNodes = 0;
    int32 InstanceRandomNodes = 0;
    for (const UMaterialExpression* Expression :
         AccentData->ExpressionCollection.Expressions)
    {
        if (const UMaterialExpressionScalarParameter* Scalar =
                Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            const float* ExpectedValue =
                ExpectedAccentScalars.Find(Scalar->ParameterName);
            if (ExpectedValue &&
                FMath::IsNearlyEqual(
                    Scalar->DefaultValue, *ExpectedValue, 0.000001f))
            {
                SeenAccentScalars.Add(Scalar->ParameterName);
                ExactScalarNodes.Add(Scalar->ParameterName, Scalar);
            }
        }
        if (Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            ++TextureSampleNodes;
        }
        if (const UMaterialExpressionTextureCoordinate* TextureCoordinate =
                Cast<UMaterialExpressionTextureCoordinate>(Expression))
        {
            if (TextureCoordinate->Desc ==
                TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"))
            {
                BladeUv = TextureCoordinate;
                ++BladeUvNodes;
            }
        }
        if (const UMaterialExpressionConstant3Vector* Color =
                Cast<UMaterialExpressionConstant3Vector>(Expression))
        {
            if (Color->Desc == TEXT("V5B_BERMUDA_WORLD_UP_NORMAL"))
            {
                WorldUpNormal = Color;
                ++WorldUpNormalNodes;
            }
            else if (Color->Desc == TEXT("V5B_BERMUDA_LIT_SUBSURFACE"))
            {
                SubsurfaceTint = Color;
                ++SubsurfaceNodes;
            }
        }
        if (const UMaterialExpressionVertexNormalWS* VertexNormal =
                Cast<UMaterialExpressionVertexNormalWS>(Expression))
        {
            if (VertexNormal->Desc ==
                TEXT("V5B_BERMUDA_IMPORTED_CURVATURE_NORMAL_WS"))
            {
                ImportedCurvatureNormal = VertexNormal;
                ++ImportedCurvatureNormalNodes;
            }
        }
        if (const UMaterialExpressionTwoSidedSign* Sign =
                Cast<UMaterialExpressionTwoSidedSign>(Expression))
        {
            if (Sign->Desc == TEXT("V5B_BERMUDA_TWO_SIDED_SIGN"))
            {
                TwoSidedSign = Sign;
                ++TwoSidedSignNodes;
            }
        }
        if (const UMaterialExpressionMultiply* Multiply =
                Cast<UMaterialExpressionMultiply>(Expression))
        {
            if (Multiply->Desc ==
                TEXT("V5B_BERMUDA_FACING_CORRECTED_NORMAL"))
            {
                FacingCorrectedNormal = Multiply;
                ++FacingCorrectedNormalNodes;
            }
        }
        if (const UMaterialExpressionLinearInterpolate* Lerp =
                Cast<UMaterialExpressionLinearInterpolate>(Expression))
        {
            if (Lerp->Desc ==
                TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"))
            {
                DistanceMatchedNormal = Lerp;
                ++DistanceMatchedNormalNodes;
            }
        }
        if (const UMaterialExpressionWorldPosition* Position =
                Cast<UMaterialExpressionWorldPosition>(Expression))
        {
            if (Position->Desc == TEXT("V5B_BERMUDA_WORLD_POSITION"))
            {
                WorldPosition = Position;
            }
        }
        if (const UMaterialExpressionTransformPosition* Transform =
                Cast<UMaterialExpressionTransformPosition>(Expression))
        {
            if (Transform->Desc == TEXT("V5B_BERMUDA_INSTANCE_LOCAL_POSITION"))
            {
                InstanceLocalPosition = Transform;
            }
        }
        if (const UMaterialExpressionTime* TimeExpression =
                Cast<UMaterialExpressionTime>(Expression))
        {
            if (TimeExpression->Desc == TEXT("V5B_BERMUDA_TIME"))
            {
                Time = TimeExpression;
            }
        }
        if (const UMaterialExpressionMaterialFunctionCall* FunctionCall =
                Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            if (FunctionCall->Desc ==
                TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"))
            {
                DitheredInstanceFade = FunctionCall;
            }
        }
        if (const UMaterialExpressionCustom* Custom =
                Cast<UMaterialExpressionCustom>(Expression))
        {
            if (Custom->Description == AccentBladeColorDescription &&
                Custom->Code == AccentBladeColorCode)
            {
                ExactBladeColor = Custom;
                ++ExactBladeColorNodes;
            }
            else if (Custom->Description == AccentWindDescription &&
                Custom->Code == AccentWindCode)
            {
                ExactWind = Custom;
                ++ExactWindNodes;
            }
        }
        if (const UMaterialExpressionVectorParameter* Vector =
                Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            if (Vector->ParameterName == TEXT("TRIAD_WindDirection"))
            {
                WindDirection = Vector;
            }
        }
        if (Expression && Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
        {
            InstanceFade = Expression;
            ++InstanceFadeNodes;
        }
        if (Expression && Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
        {
            InstanceRandom = Expression;
            ++InstanceRandomNodes;
        }
    }
    const auto InputMatches = [](
        const FExpressionInput& Input,
        const UMaterialExpression* ExpectedExpression,
        int32 ExpectedOutputIndex)
    {
        return Input.Expression == ExpectedExpression &&
            Input.OutputIndex == ExpectedOutputIndex;
    };
    bool bExactDitherInput = DitheredInstanceFade &&
        DitheredInstanceFade->MaterialFunction &&
        DitheredInstanceFade->MaterialFunction->GetPathName() ==
            DitherTemporalAaFunctionPath &&
        DitheredInstanceFade->FunctionOutputs.Num() > 0;
    int32 DitherAlphaInputIndex = INDEX_NONE;
    if (bExactDitherInput)
    {
        for (int32 InputIndex = 0;
             InputIndex < DitheredInstanceFade->FunctionInputs.Num();
             ++InputIndex)
        {
            if (DitheredInstanceFade->GetInputName(InputIndex)
                    .ToString()
                    .StartsWith(TEXT("Alpha Threshold")))
            {
                DitherAlphaInputIndex = InputIndex;
                break;
            }
        }
        bExactDitherInput = DitherAlphaInputIndex != INDEX_NONE &&
            InputMatches(
                DitheredInstanceFade->FunctionInputs[DitherAlphaInputIndex].Input,
                InstanceFade,
                0);
    }
    const UMaterialExpressionScalarParameter* Roughness =
        ExactScalarNodes.FindRef(FName(TEXT("Roughness")));
    const UMaterialExpressionScalarParameter* Specular =
        ExactScalarNodes.FindRef(FName(TEXT("Specular")));
    const UMaterialExpressionScalarParameter* Strength =
        ExactScalarNodes.FindRef(FName(TEXT("TRIAD_WindStrengthCm")));
    const UMaterialExpressionScalarParameter* Speed =
        ExactScalarNodes.FindRef(FName(TEXT("TRIAD_WindSpeed")));
    const UMaterialExpressionScalarParameter* DirectionalResponse =
        ExactScalarNodes.FindRef(FName(TEXT("TRIAD_WindResponseScale")));
    const UMaterialExpressionScalarParameter* MaximumWpo =
        ExactScalarNodes.FindRef(FName(TEXT("TRIAD_MaxWpoCm")));
    const UMaterialExpressionScalarParameter* Height =
        ExactScalarNodes.FindRef(FName(TEXT("TRIAD_WindHeightCm")));
    const FName ExpectedWindInputNames[] = {
        TEXT("WorldPosition"),
        TEXT("InstanceLocalPosition"),
        TEXT("BladeUV"),
        TEXT("TimeSeconds"),
        TEXT("WindStrengthCm"),
        TEXT("WindSpeed"),
        TEXT("WindDirection"),
        TEXT("HeightCm"),
        TEXT("ResponseScale"),
        TEXT("MaxWpoCm"),
        TEXT("Random01"),
        TEXT("InstanceFade")};
    const UMaterialExpression* ExpectedWindInputNodes[] = {
        WorldPosition,
        InstanceLocalPosition,
        BladeUv,
        Time,
        Strength,
        Speed,
        WindDirection,
        Height,
        DirectionalResponse,
        MaximumWpo,
        InstanceRandom,
        InstanceFade};
    bool bExactWindInputs = ExactWind && ExactWind->Inputs.Num() == 12;
    if (bExactWindInputs)
    {
        for (int32 Index = 0; Index < 12; ++Index)
        {
            if (ExactWind->Inputs[Index].InputName != ExpectedWindInputNames[Index] ||
                !InputMatches(
                    ExactWind->Inputs[Index].Input,
                    ExpectedWindInputNodes[Index],
                    0))
            {
                bExactWindInputs = false;
                break;
            }
        }
    }
    const bool bExactBladeColorInputs = ExactBladeColor &&
        ExactBladeColor->OutputType == CMOT_Float3 &&
        ExactBladeColor->Inputs.Num() == 2 &&
        ExactBladeColor->Inputs[0].InputName == FName(TEXT("BladeUV")) &&
        InputMatches(ExactBladeColor->Inputs[0].Input, BladeUv, 0) &&
        ExactBladeColor->Inputs[1].InputName == FName(TEXT("Random01")) &&
        InputMatches(ExactBladeColor->Inputs[1].Input, InstanceRandom, 0);
    const bool bExactDistanceMatchedNormal =
        ImportedCurvatureNormal && ImportedCurvatureNormalNodes == 1 &&
        TwoSidedSign && TwoSidedSignNodes == 1 &&
        FacingCorrectedNormal && FacingCorrectedNormalNodes == 1 &&
        DistanceMatchedNormal && DistanceMatchedNormalNodes == 1 &&
        InputMatches(
            FacingCorrectedNormal->A,
            ImportedCurvatureNormal,
            0) &&
        InputMatches(FacingCorrectedNormal->B, TwoSidedSign, 0) &&
        InputMatches(DistanceMatchedNormal->A, WorldUpNormal, 0) &&
        InputMatches(
            DistanceMatchedNormal->B,
            FacingCorrectedNormal,
            0) &&
        InputMatches(DistanceMatchedNormal->Alpha, InstanceFade, 0);
    if (AccentData->ExpressionCollection.Expressions.Num() != 23 ||
        SeenAccentScalars.Num() != ExpectedAccentScalars.Num() ||
        TextureSampleNodes != 0 ||
        !BladeUv || BladeUvNodes != 1 || BladeUv->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(BladeUv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(BladeUv->VTiling, 1.0f, 0.000001f) ||
        !ExactBladeColor || ExactBladeColorNodes != 1 ||
        !bExactBladeColorInputs ||
        !WorldUpNormal || WorldUpNormalNodes != 1 ||
        !WorldUpNormal->Constant.Equals(
            FLinearColor(0.0f, 0.0f, 1.0f), 0.000001f) ||
        !bExactDistanceMatchedNormal ||
        !SubsurfaceTint || SubsurfaceNodes != 1 ||
        !SubsurfaceTint->Constant.Equals(
            FLinearColor(0.058f, 0.145f, 0.038f), 0.000001f) ||
        !Roughness || Roughness->Desc != TEXT("V5B_BERMUDA_HIGH_ROUGHNESS") ||
        !Specular || Specular->Desc != TEXT("V5B_BERMUDA_LOW_SPECULAR") ||
        !bExactDitherInput || !ExactWind || ExactWindNodes != 1 ||
        ExactWind->OutputType != CMOT_Float3 || !bExactWindInputs ||
        InstanceFadeNodes != 1 || InstanceRandomNodes != 1 ||
        !WindDirection ||
        !WindDirection->DefaultValue.Equals(
            FLinearColor(0.93f, 0.37f, 0.0f, 0.0f), 0.000001f) ||
        !WorldPosition ||
        WorldPosition->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        !InstanceLocalPosition ||
        InstanceLocalPosition->TransformSourceType != TRANSFORMPOSSOURCE_World ||
        InstanceLocalPosition->TransformType != TRANSFORMPOSSOURCE_Instance ||
        !InputMatches(InstanceLocalPosition->Input, WorldPosition, 0) ||
        !InputMatches(AccentData->BaseColor, ExactBladeColor, 0) ||
        !InputMatches(AccentData->Normal, DistanceMatchedNormal, 0) ||
        !InputMatches(AccentData->Roughness, Roughness, 0) ||
        !InputMatches(AccentData->Specular, Specular, 0) ||
        !InputMatches(AccentData->OpacityMask, DitheredInstanceFade, 0) ||
        !InputMatches(AccentData->SubsurfaceColor, SubsurfaceTint, 0) ||
        !InputMatches(AccentData->WorldPositionOffset, ExactWind, 0) ||
        AccentData->EmissiveColor.Expression || AccentData->Opacity.Expression ||
        AccentData->Refraction.Expression)
    {
        OutReport = TEXT("V5B accent turf lost its zero-texture modeled-blade/procedural-color/imported-curvature/two-sided/fade-matched-normal/direct-dither/fade-matched-wind graph.");
        return false;
    }
    UMaterialInterface* CompiledMaterials[] = {
        Surface,
        Foam,
        Spray,
        Bark,
        Leaves,
        FormalBedMaterial,
        Accent,
        Paver};
    for (UMaterialInterface* Material : CompiledMaterials)
    {
        if (!ValidateCompiledMaterial(Material, OutReport))
        {
            return false;
        }
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5B_ASSETS_VALID assets=37 meshes=20 trimmedBuildingTriangles=773158 hardscapeSuccessorTriangles=1568 fineTurfAppearanceProxy=true botanicalSpeciesClaim=false frozenCompatibilityAssetName=BermudaTurfCluster r11TuftCenters=272 r11PairTufts=136 r11TriadTufts=136 r11BladeRoots=680 r11ClippedJuvenileBlades=510/170 r11PostureMix=306/306/68 r11CShapeCounterBend=612/68 r11HeightMix=462/184/34 r11TotalTriangleSurfaceAreaCm2=505.174458 r11TotalTriangleSurfaceAreaBoundsCm2=500/535 r11ProjectedAreaRatioToR10Bounds=0.92/1.08 r11ProjectedAreaMaxRatiosAtDownwardElevation0/10/20/30=1.064873/1.070234/1.076868/1.072940 r11ProjectedAreaApplicability=orthographicDownwardElevation0/10/20/30Azimuth0..359NotOverhead sourceHeightCm=1.6..4.4 turfLodTriangles=2550/680/204 turfLodScreenSizes=1.0/0.10/0.040 turfWindHeightNormalizerCm=4.8 frozenOneMaterialCompatibility=true formalBedMoundTriangles=5696 treeBaseMulchMoundTriangles=224 pachiraParts=8 pachiraTriangles=%lld props=7 materials=8 textures=9 claim=RENDER_ONLY_FINE_TURF_APPEARANCE_PROXY_NOT_BOTANICAL_OR_SPECIES_IDENTIFICATION_NOT_SURVEY_NOT_FOUNTAIN_OPERATING_STATE_NOT_SENSOR_NOT_RF_TRUTH"),
        PachiraTriangles);
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5BAssetFactory
{
bool IsExactUnpersistedAccentTurfLodImportScratch(const FAssetData& Row)
{
    return ::IsExactUnpersistedAccentTurfLodImportScratch(Row);
}

const FString& GetAssetRootPath() { return AssetRoot; }
const FString& GetPorticoMeshObjectPath() { return PorticoObjectPath; }
const FString& GetHardscapeRenderSuccessorMeshObjectPath()
{
    return HardscapeRenderSuccessorObjectPath;
}
const FString& GetAccentTurfMeshObjectPath() { return AccentTurfObjectPath; }
const FString& GetAccentTurfMaterialObjectPath() { return AccentMaterialObjectPath; }
const FString& GetFormalBedVeneerMeshObjectPath() { return FormalBedObjectPath; }
const FString& GetTreeBaseMulchMeshObjectPath() { return TreeBaseMulchObjectPath; }
const FString& GetFormalBedVeneerMaterialObjectPath() { return FormalBedMaterialObjectPath; }
const FString& GetFountainSurfaceMeshObjectPath() { return SurfaceObjectPath; }
const FString& GetFountainFoamMeshObjectPath() { return FoamObjectPath; }
const FString& GetFountainImpactRingMeshObjectPath() { return ImpactObjectPath; }
const FString& GetFountainPlumeMeshObjectPath() { return PlumeObjectPath; }
const FString& GetFountainCentralPlumeMeshObjectPath() { return CentralPlumeObjectPath; }
const FString& GetPaverInnerMeshObjectPath() { return PaverInnerObjectPath; }
const FString& GetPaverOuterMeshObjectPath() { return PaverOuterObjectPath; }
const FString& GetFountainSurfaceMaterialObjectPath() { return SurfaceMaterialObjectPath; }
const FString& GetFountainFoamMaterialObjectPath() { return FoamMaterialObjectPath; }
const FString& GetFountainSprayMaterialObjectPath() { return SprayMaterialObjectPath; }
const FString& GetPaverMaterialObjectPath() { return PaverMaterialObjectPath; }
const FString& GetPachiraBarkMaterialObjectPath() { return BarkMaterialObjectPath; }
const FString& GetPachiraLeavesMaterialObjectPath() { return LeavesMaterialObjectPath; }
const TArray<FString>& GetPachiraMeshObjectPaths() { return PachiraObjectPaths(); }

bool CreateFreshExploreV5BAssets(
    TArray<UObject*>& OutAssets,
    FString& OutError)
{
    OutAssets.Reset();
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing) || !Existing.IsEmpty())
    {
        OutError = TEXT("CreateFreshExploreV5BAssets requires an empty exact V5B asset root.");
        return false;
    }
    FScopedFreshRollback Rollback(OutAssets, OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
    TMap<FString, UTexture2D*> Textures;
    TMap<FString, UMaterialInterface*> Materials;
    if (!ImportTextures(AssetTools, Textures, OutAssets, OutError) ||
        !CreateMaterials(AssetTools, Textures, Materials, OutAssets, OutError) ||
        !ImportMeshes(AssetTools, Materials, OutAssets, OutError))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    FString Validation;
    if (OutAssets.Num() != 37 || !ValidateInternal(false, Validation))
    {
        OutError = TEXT("Fresh V5B asset validation failed before save: ") + Validation;
        return false;
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateAccentTurfAssetForR11Upgrade(
    bool& bOutAlreadyR11,
    FString& OutError)
{
    bOutAlreadyR11 = false;
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing) || Existing.Num() != 37)
    {
        OutError = TEXT("The R11 carrier upgrade requires the exact persisted 37-asset V5B namespace.");
        return false;
    }
    TArray<FString> ActualPaths;
    for (const FAssetData& Row : Existing)
    {
        ActualPaths.Add(Row.GetObjectPathString());
        UObject* Object = Row.GetAsset();
        if (!Object || !Object->GetOutermost() ||
            Object->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R11 carrier upgrade refuses a missing or dirty V5B package: ") +
                Row.GetObjectPathString();
            return false;
        }
    }
    ActualPaths.Sort();
    if (ActualPaths != ExactObjectPaths())
    {
        OutError = TEXT("The R11 carrier upgrade refuses V5B namespace path drift.");
        return false;
    }

    FString CurrentReport;
    if (ValidateInternal(false, CurrentReport))
    {
        bOutAlreadyR11 = true;
        OutError.Reset();
        return true;
    }

    UStaticMesh* Mesh = LoadExact<UStaticMesh>(AccentTurfObjectPath);
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    const int32 R10Vertices[] = {4250, 2550, 768};
    const int32 R10Indices[] = {7650, 2550, 768};
    const int32 R10Triangles[] = {2550, 850, 256};
    const float ScreenSizes[] = {1.0f, 0.10f, 0.040f};
    bool bExactR10 = Mesh && MeshIsRenderOnly(Mesh) &&
        Mesh->GetNumSourceModels() == 3 && !Mesh->bAutoComputeLODScreenSize &&
        Mesh->GetStaticMaterials().Num() == 1 && Mesh->GetMaterial(0) &&
        Mesh->GetMaterial(0)->GetPathName() == AccentMaterialObjectPath &&
        RenderData && RenderData->LODResources.Num() == 3;
    for (int32 LodIndex = 0; bExactR10 && LodIndex < 3; ++LodIndex)
    {
        const FStaticMeshSourceModel& SourceModel = Mesh->GetSourceModel(LodIndex);
        const FStaticMeshLODResources& Lod = RenderData->LODResources[LodIndex];
        bExactR10 &= TriangleCount(Mesh, LodIndex) == R10Triangles[LodIndex] &&
            Lod.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices() ==
                R10Vertices[LodIndex] &&
            Lod.VertexBuffers.PositionVertexBuffer.GetNumVertices() ==
                R10Vertices[LodIndex] &&
            Lod.IndexBuffer.GetNumIndices() == R10Indices[LodIndex] &&
            Lod.Sections.Num() == 1 && Lod.Sections[0].MaterialIndex == 0 &&
            SourceModel.BuildSettings.bUseHighPrecisionTangentBasis &&
            SourceModel.BuildSettings.bUseFullPrecisionUVs &&
            !SourceModel.BuildSettings.bGenerateLightmapUVs &&
            !SourceModel.BuildSettings.bRecomputeNormals &&
            SourceModel.BuildSettings.BuildScale3D == FVector::OneVector &&
            FMath::IsNearlyEqual(
                SourceModel.ScreenSize.Default, ScreenSizes[LodIndex], 0.000001f);
    }
    if (bExactR10)
    {
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FVector Minimum = Bounds.Origin - Bounds.BoxExtent;
        const FVector Maximum = Bounds.Origin + Bounds.BoxExtent;
        bExactR10 =
            FMath::IsWithinInclusive(Minimum.X, -79.49f, -79.17f) &&
            FMath::IsWithinInclusive(Minimum.Y, -69.78f, -69.46f) &&
            FMath::IsWithinInclusive(Minimum.Z, -0.05f, 0.05f) &&
            FMath::IsWithinInclusive(Maximum.X, 80.49f, 80.81f) &&
            FMath::IsWithinInclusive(Maximum.Y, 70.50f, 70.83f) &&
            FMath::IsWithinInclusive(Maximum.Z, 4.33f, 4.47f);
    }
    if (!bExactR10)
    {
        OutError = TEXT("The accent carrier is neither the exact admitted R10 topology nor an idempotent exact R11 asset; partial repair is refused. Current validation: ") +
            CurrentReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool RebuildExistingAccentTurfAsset(
    UStaticMesh*& OutAccentTurf,
    FString& OutError)
{
    OutAccentTurf = nullptr;
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing) || Existing.Num() != 37)
    {
        OutError = TEXT("The accent-turf upgrade requires the exact persisted 37-asset V5B namespace.");
        return false;
    }
    TArray<FString> ActualPaths;
    for (const FAssetData& Row : Existing)
    {
        ActualPaths.Add(Row.GetObjectPathString());
        UObject* Object = Row.GetAsset();
        if (!Object || !Object->GetOutermost() || Object->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The accent-turf upgrade refuses a missing or dirty V5B package: ") +
                Row.GetObjectPathString();
            return false;
        }
    }
    ActualPaths.Sort();
    if (ActualPaths != ExactObjectPaths())
    {
        OutError = TEXT("The accent-turf upgrade refuses V5B namespace path drift.");
        return false;
    }
    UStaticMesh* AccentTurf = LoadExact<UStaticMesh>(AccentTurfObjectPath);
    UMaterialInterface* AccentMaterial =
        LoadExact<UMaterialInterface>(AccentMaterialObjectPath);
    if (!AccentTurf || !AccentMaterial ||
        !ConfigureAccentTurfMesh(
            AccentTurf, AccentMaterial, true, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact in-place V5B accent-turf rebuild failed.");
        }
        return false;
    }
    FString PreSaveValidation;
    if (!ValidateInternal(false, PreSaveValidation))
    {
        OutError = TEXT("The in-place accent-turf rebuild failed complete pre-save validation: ") +
            PreSaveValidation;
        return false;
    }
    OutAccentTurf = AccentTurf;
    OutError.Reset();
    return true;
}

bool ValidateExploreV5BAssets(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}
} // namespace TRIADIstanaExploreV5BAssetFactory
