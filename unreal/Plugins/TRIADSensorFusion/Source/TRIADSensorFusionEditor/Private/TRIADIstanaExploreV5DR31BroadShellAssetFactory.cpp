#include "TRIADIstanaExploreV5DR31BroadShellAssetFactory.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "IAssetTools.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceBasePropertyOverrides.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "RHIFeatureLevel.h"
#include "ShaderCompiler.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Ssl.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"
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
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_R31_BroadShellPBR_Master"));
const FString MasterObjectPath(
    MaterialRoot + TEXT("/") + MasterName + TEXT(".") + MasterName);
const FString PublicViewTextureRoot(
    TEXT("/Game/TRIAD/IstanaPublicView/Textures"));

const FName ParameterGroup(TEXT("Istana Explore V5D R31 Broad Shell"));
const FName BaseColorTextureParameter(TEXT("BaseColorTexture"));
const FName NormalTextureParameter(TEXT("NormalTexture"));
const FName PackedOrmTextureParameter(TEXT("PackedORMTexture"));
const FName UvScaleParameter(TEXT("UvScale"));
const FName TextureInfluenceParameter(TEXT("TextureInfluence"));
const FName NormalStrengthParameter(TEXT("NormalStrength"));
const FName SurfaceRoughnessParameter(TEXT("SurfaceRoughness"));
const FName RoughnessTextureWeightParameter(TEXT("RoughnessTextureWeight"));
const FName MetallicParameter(TEXT("Metallic"));
const FName SpecularParameter(TEXT("Specular"));
const FName AoTextureWeightParameter(TEXT("AoTextureWeight"));
const FName VariationCellMetersParameter(TEXT("VariationCellMeters"));
const FName WeatheringStrengthParameter(TEXT("WeatheringStrength"));
const FName WallVerticalWeatherMaskParameter(TEXT("WallVerticalWeatherMask"));
const FName BayMetersParameter(TEXT("BayMeters"));
const FName StoreyMetersParameter(TEXT("StoreyMeters"));
const FName ApertureWidthFractionParameter(TEXT("ApertureWidthFraction"));
const FName ApertureHeightFractionParameter(TEXT("ApertureHeightFraction"));
const FName ApertureSillFractionParameter(TEXT("ApertureSillFraction"));
const FName ApertureHintStrengthParameter(TEXT("ApertureHintStrength"));
const FName ApertureFadeStartCmParameter(TEXT("ApertureFadeStartCm"));
const FName ApertureFadeEndCmParameter(TEXT("ApertureFadeEndCm"));
const FName AtmosphereStartCmParameter(TEXT("AtmosphereStartCm"));
const FName AtmosphereEndCmParameter(TEXT("AtmosphereEndCm"));
const FName AtmosphereStrengthParameter(TEXT("AtmosphereStrength"));
const FName TintParameter(TEXT("Tint"));
const FName ApertureTintParameter(TEXT("ApertureTint"));
const FName AtmosphereTintParameter(TEXT("AtmosphereTint"));

constexpr int32 ExpectedAssetCount = 5;
constexpr int32 ExpectedMaterialCount = 4;
constexpr int32 ExpectedTextureParameterCount = 3;
constexpr int32 ExpectedScalarParameterCount = 22;
constexpr int32 ExpectedVectorParameterCount = 3;
constexpr int32 ExpectedMasterExpressionCount = 42;

enum class ETextureUsage : uint8
{
    BaseColor,
    Normal,
    PackedOrm
};

struct FMaterialSpec
{
    const TCHAR* Role;
    const TCHAR* Name;
    const TCHAR* TextureSet;
    float MetresPerTile;
    FLinearColor Tint;
    FLinearColor ApertureTint;
    FLinearColor AtmosphereTint;
    float TextureInfluence;
    float NormalStrength;
    float SurfaceRoughness;
    float RoughnessTextureWeight;
    float Metallic;
    float Specular;
    float AoTextureWeight;
    float VariationCellMeters;
    float WeatheringStrength;
    float WallVerticalWeatherMask;
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
};

// Exact public order: official wall, official roof, fallback wall, fallback roof.
const FMaterialSpec MaterialSpecs[] = {
    {TEXT("OfficialWall"), TEXT("MI_IPV5D_R31_OfficialWall"), TEXT("Stone"), 1.8f,
     FLinearColor(0.86f, 0.82f, 0.72f),
     FLinearColor(0.020f, 0.050f, 0.074f),
     FLinearColor(0.48f, 0.54f, 0.55f),
     0.78f, 0.48f, 0.68f, 0.72f, 0.0f, 0.29f, 0.72f,
     46.0f, 0.24f, 1.0f, 3.0f, 3.2f, 0.62f, 0.50f, 0.18f, 0.72f,
     55000.0f, 100000.0f, 60000.0f, 130000.0f, 0.22f},
    {TEXT("OfficialRoof"), TEXT("MI_IPV5D_R31_OfficialRoof"), TEXT("Slate"), 1.3f,
     FLinearColor(0.47f, 0.51f, 0.53f),
     FLinearColor(0.020f, 0.050f, 0.074f),
     FLinearColor(0.48f, 0.54f, 0.55f),
     0.90f, 0.62f, 0.76f, 0.82f, 0.0f, 0.24f, 0.80f,
     52.0f, 0.20f, 0.0f, 3.0f, 3.2f, 0.62f, 0.50f, 0.18f, 0.0f,
     55000.0f, 100000.0f, 60000.0f, 130000.0f, 0.18f},
    {TEXT("FallbackWall"), TEXT("MI_IPV5D_R31_FallbackWall"), TEXT("Plaster"), 2.2f,
     FLinearColor(0.78f, 0.78f, 0.72f),
     FLinearColor(0.018f, 0.044f, 0.066f),
     FLinearColor(0.47f, 0.53f, 0.55f),
     0.72f, 0.42f, 0.72f, 0.68f, 0.0f, 0.27f, 0.68f,
     48.0f, 0.30f, 1.0f, 3.15f, 3.15f, 0.60f, 0.48f, 0.20f, 0.68f,
     55000.0f, 95000.0f, 55000.0f, 125000.0f, 0.25f},
    {TEXT("FallbackRoof"), TEXT("MI_IPV5D_R31_FallbackRoof"), TEXT("Slate"), 1.6f,
     FLinearColor(0.39f, 0.43f, 0.45f),
     FLinearColor(0.018f, 0.044f, 0.066f),
     FLinearColor(0.47f, 0.53f, 0.55f),
     0.82f, 0.50f, 0.82f, 0.76f, 0.0f, 0.22f, 0.74f,
     54.0f, 0.27f, 0.0f, 3.15f, 3.15f, 0.60f, 0.48f, 0.20f, 0.0f,
     55000.0f, 95000.0f, 55000.0f, 125000.0f, 0.20f}};
static_assert(UE_ARRAY_COUNT(MaterialSpecs) == ExpectedMaterialCount);

const TCHAR* const TextureSets[] = {
    TEXT("Plaster"), TEXT("Stone"), TEXT("Slate")};

const FString SeamSafeMetricUvDescription(
    TEXT("TRIAD_IPV5D_R31_SEAM_SAFE_METRIC_TEXTURE_UV_WALL_VERTEX_TANGENT_QUANTIZED_SCALE_WORLD_U_PLAN_SOURCE_XY"));
const FString SeamSafeMetricUvCode(
    TEXT("float2 wallTangentRaw = VertexTangentWS.xy;\n")
    TEXT("float2 wallTangent = wallTangentRaw / max(length(wallTangentRaw), 0.0001);\n")
    TEXT("float useWorldX = step(abs(wallTangent.y), abs(wallTangent.x));\n")
    TEXT("float signedDominantTangent = lerp(wallTangent.y, wallTangent.x, useWorldX);\n")
    TEXT("float tangentQuantizationLevels = 4096.0;\n")
    TEXT("float tangentQuantizationPhase = 89.0 / 1048576.0;\n")
    TEXT("float quantizedDominantMagnitude = floor((abs(signedDominantTangent) - tangentQuantizationPhase) * tangentQuantizationLevels + 0.5) / tangentQuantizationLevels + tangentQuantizationPhase;\n")
    TEXT("float signedWorldAxisMetres = lerp(WorldPositionCm.y, WorldPositionCm.x, useWorldX) * 0.01 * (signedDominantTangent < 0.0 ? -1.0 : 1.0);\n")
    TEXT("float wallUMetres = signedWorldAxisMetres / max(quantizedDominantMagnitude, 0.0001);\n")
    TEXT("return float2(lerp(UV0.x, wallUMetres, saturate(WallRoleMask)), UV0.y);"));

// The texture samples are already linearized by Unreal according to their
// admitted source settings. The immutable combined shell exposes no OSM group
// identifier to the material. A world-XY cell floor is not safe here because a
// cell boundary can cut straight through a building or even one facade face.
// Instead, the shader canonicalizes the geometric normal and combines its
// direction with signed plane distance through continuous sin/cos signals.
// Four normalized archetype weights avoid hard plane-distance/family cuts.
// A source-oriented VertexTangentWS world projection also avoids the source
// OBJ's per-segment U resets. Quantizing only the dominant tangent component
// used to restore metric scale removes kilometre-offset amplification of tiny
// source-float tangent differences while keeping worst-case scale error below
// the source-audited 0.018 percent. Natural changes remain at corners;
// geometry and pixel depth do not.
const FString SurfaceResponseDescription(
    TEXT("TRIAD_IPV5D_R31_SEAM_SAFE_FACADE_PLANE_FAMILIES_OCCUPANCY_WEATHERED_RECESSED_RENDER_ONLY"));
const FString SurfaceResponseCode(
    TEXT("float variationMeters = max(VariationCellMeters, 1.0);\n")
    TEXT("float3 dPdx = ddx(WorldPositionCm);\n")
    TEXT("float3 dPdy = ddy(WorldPositionCm);\n")
    TEXT("float3 geometricNormalRaw = cross(dPdy, dPdx);\n")
    TEXT("float3 geometricNormal = geometricNormalRaw / max(length(geometricNormalRaw), 0.0001);\n")
    TEXT("float3 normalMagnitude = abs(geometricNormal);\n")
    TEXT("float dominantNormalComponent = (normalMagnitude.x >= normalMagnitude.y && normalMagnitude.x >= normalMagnitude.z) ? geometricNormal.x : ((normalMagnitude.y >= normalMagnitude.z) ? geometricNormal.y : geometricNormal.z);\n")
    TEXT("float canonicalNormalSign = dominantNormalComponent < 0.0 ? -1.0 : 1.0;\n")
    TEXT("float3 canonicalPlaneNormal = geometricNormal * canonicalNormalSign;\n")
    TEXT("float facadePlaneDistanceMetres = dot(WorldPositionCm, canonicalPlaneNormal) * 0.01;\n")
    TEXT("float facadeNormalSignal = dot(canonicalPlaneNormal, float3(0.7548777,1.3247180,2.4142136));\n")
    TEXT("float facadePlaneCoordinate = facadePlaneDistanceMetres / variationMeters;\n")
    TEXT("float facadeSignalA = 0.5 + 0.5 * sin(facadeNormalSignal * 11.173 + facadePlaneCoordinate * 2.173);\n")
    TEXT("float facadeSignalB = 0.5 + 0.5 * cos(facadeNormalSignal * 17.271 - facadePlaneCoordinate * 1.337);\n")
    TEXT("float facadeSignalC = 0.5 + 0.5 * sin(facadeNormalSignal * 23.417 + facadePlaneCoordinate * 0.917 + 1.0471976);\n")
    TEXT("float familyAngle = 6.2831853 * facadeSignalA;\n")
    TEXT("float4 familyLobes = 0.5 + 0.5 * cos(float4(familyAngle,familyAngle,familyAngle,familyAngle) - float4(0.0,1.5707963,3.1415927,4.7123890));\n")
    TEXT("float4 familyWeights = familyLobes * familyLobes;\n")
    TEXT("familyWeights /= max(dot(familyWeights, float4(1.0,1.0,1.0,1.0)), 0.0001);\n")
    TEXT("float facadeNoise = facadeSignalA;\n")
    TEXT("float familyBayScale = dot(familyWeights, float4(0.82,1.02,1.18,0.92));\n")
    TEXT("float familyStoreyScale = dot(familyWeights, float4(0.96,1.06,0.92,1.02));\n")
    TEXT("float familyWidthScale = dot(familyWeights, float4(0.86,1.04,1.16,0.94));\n")
    TEXT("float familyHeightScale = dot(familyWeights, float4(1.05,0.91,1.12,0.97));\n")
    TEXT("float familyWarmth = dot(familyWeights, float4(-0.035,0.045,0.018,-0.012));\n")
    TEXT("float3 familyPalette = float3(dot(familyWeights,float4(0.96,1.04,1.02,0.98)),dot(familyWeights,float4(0.99,1.01,0.97,1.03)),dot(familyWeights,float4(1.04,0.96,0.94,1.01)));\n")
    TEXT("float familySpandrelStrength = dot(familyWeights,float4(0.34,0.16,0.52,0.26));\n")
    TEXT("float familyPierStrength = dot(familyWeights,float4(0.18,0.42,0.22,0.56));\n")
    TEXT("float familySlabStrength = dot(familyWeights,float4(0.10,0.30,0.58,0.18));\n")
    TEXT("float familyPhaseU = facadeSignalB * 0.18 + dot(familyWeights,float4(0.08,0.31,0.57,0.79));\n")
    TEXT("float familyPhaseV = facadeSignalC * 0.12 + dot(familyWeights,float4(0.00,0.11,0.19,0.07));\n")
    TEXT("float wallMask = saturate(WallVerticalWeatherMask);\n")
    TEXT("float sourceAbsoluteZMetres = 1.0 - UV0.y; // Wall OBJ V is absolute source Z, not height above a building-local base.\n")
    TEXT("float facadeUMetres = UV0.x; // Input is the shared seam-safe metric UV used by all three textures.\n")
    TEXT("float runoffPhase = frac(facadeUMetres * 0.137 + facadeSignalB);\n")
    TEXT("float runoff = wallMask * pow(saturate(1.0 - abs(runoffPhase - 0.5) * 2.0), 7.0);\n")
    TEXT("float roofMottle = (1.0-wallMask) * (0.28 + 0.72 * (0.5 + 0.5 * sin(facadeSignalB * 7.13 + facadeSignalC * 3.17)));\n")
    TEXT("float weather = saturate(WeatheringStrength * (0.26 + 0.42 * facadeNoise + 0.18 * runoff + 0.34 * roofMottle));\n")
    TEXT("float3 surface = saturate(BaseSurface.rgb * lerp(1.0, 0.70, weather));\n")
    TEXT("float3 familyTint = float3(1.0 + familyWarmth, 1.0 + familyWarmth * 0.38, 1.0 - familyWarmth * 0.52);\n")
    TEXT("surface = saturate(surface * familyTint * lerp(float3(1.0,1.0,1.0),familyPalette,wallMask));\n")
    TEXT("float bay = max(BayMeters * familyBayScale, 0.5);\n")
    TEXT("float storey = max(StoreyMeters * familyStoreyScale, 2.0);\n")
    TEXT("float2 q = float2(facadeUMetres / bay + familyPhaseU, sourceAbsoluteZMetres / storey + familyPhaseV);\n")
    TEXT("float2 phase = frac(q);\n")
    TEXT("float2 aa = max(fwidth(q), float2(0.002, 0.002));\n")
    TEXT("float cellFootprint = max(aa.x, aa.y);\n")
    TEXT("float microReadability = 1.0 - smoothstep(0.35, 0.90, cellFootprint);\n")
    TEXT("float familyWidth = saturate(ApertureWidthFraction * familyWidthScale);\n")
    TEXT("float familyHeight = saturate(ApertureHeightFraction * familyHeightScale);\n")
    TEXT("float familySill = saturate(ApertureSillFraction + (facadeSignalC-0.5) * 0.07);\n")
    TEXT("float minU = 0.5 - familyWidth * 0.5;\n")
    TEXT("float maxU = 0.5 + familyWidth * 0.5;\n")
    TEXT("float minV = familySill;\n")
    TEXT("float maxV = min(minV + familyHeight, 0.98);\n")
    TEXT("float facadeEdgeU = min(phase.x,1.0-phase.x);\n")
    TEXT("float facadeEdgeV = min(phase.y,1.0-phase.y);\n")
    TEXT("float pierMask = 1.0-smoothstep(0.035-aa.x,0.105+aa.x,facadeEdgeU);\n")
    TEXT("float slabFaceMask = 1.0-smoothstep(0.018-aa.y,0.072+aa.y,facadeEdgeV);\n")
    TEXT("float spandrelMask = smoothstep(maxV+0.025-aa.y,maxV+0.085+aa.y,phase.y) * (1.0-smoothstep(0.91-aa.y,0.98+aa.y,phase.y));\n")
    TEXT("float twoStoreyWave = 0.5+0.5*cos(3.1415927*q.y);\n")
    TEXT("float outerU = smoothstep(minU-aa.x,minU+aa.x,phase.x) * (1.0-smoothstep(maxU-aa.x,maxU+aa.x,phase.x));\n")
    TEXT("float outerV = smoothstep(minV-aa.y,minV+aa.y,phase.y) * (1.0-smoothstep(maxV-aa.y,maxV+aa.y,phase.y));\n")
    TEXT("float outerAperture = saturate(outerU * outerV);\n")
    TEXT("float frameU = min(0.065, saturate(ApertureWidthFraction) * 0.18);\n")
    TEXT("float frameV = min(0.065, saturate(ApertureHeightFraction) * 0.18);\n")
    TEXT("float innerMinU = minU + frameU;\n")
    TEXT("float innerMaxU = maxU - frameU;\n")
    TEXT("float innerMinV = minV + frameV;\n")
    TEXT("float innerMaxV = maxV - frameV;\n")
    TEXT("float innerU = smoothstep(innerMinU-aa.x,innerMinU+aa.x,phase.x) * (1.0-smoothstep(innerMaxU-aa.x,innerMaxU+aa.x,phase.x));\n")
    TEXT("float innerV = smoothstep(innerMinV-aa.y,innerMinV+aa.y,phase.y) * (1.0-smoothstep(innerMaxV-aa.y,innerMaxV+aa.y,phase.y));\n")
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
    TEXT("float edgeDistance = min(min(phase.x-minU,maxU-phase.x),min(phase.y-minV,maxV-phase.y));\n")
    TEXT("float revealWidth = max(min(frameU, frameV) * 1.25, 0.025);\n")
    TEXT("float revealMask = outerAperture * (1.0 - smoothstep(0.0, revealWidth, edgeDistance));\n")
    TEXT("float distanceCm = length(WorldPositionCm - CameraPositionCm);\n")
    TEXT("float rangeCm = max(ApertureFadeEndCm - ApertureFadeStartCm, 1.0);\n")
    TEXT("float fade = 1.0 - saturate((distanceCm - ApertureFadeStartCm) / rangeCm);\n")
    TEXT("float facadeStrength = max(ApertureHintStrength, 0.0) * wallMask * fade * microReadability;\n")
    TEXT("float architecturalRange = wallMask * fade * microReadability;\n")
    TEXT("float visibleSpandrelRhythm = architecturalRange * familySpandrelStrength * spandrelMask;\n")
    TEXT("float visiblePierRhythm = architecturalRange * familyPierStrength * pierMask;\n")
    TEXT("float visibleSlabRhythm = architecturalRange * familySlabStrength * twoStoreyWave * slabFaceMask;\n")
    TEXT("float2 cellIndex = floor(q);\n")
    TEXT("float cellSignal = 0.5 + 0.5 * sin(dot(cellIndex, float2(41.37,289.11)) + facadeSignalB * 17.31 + facadeSignalC * 31.73);\n")
    TEXT("float occupancyThreshold = lerp(0.13,0.37,facadeSignalC);\n")
    TEXT("float occupied = smoothstep(occupancyThreshold - 0.07, occupancyThreshold + 0.07, cellSignal);\n")
    TEXT("float cellOccupancy = lerp(0.38, 1.0, occupied);\n")
    TEXT("float cellToneSignal = 0.5 + 0.5 * cos(dot(cellIndex, float2(19.19,73.71)) + facadeSignalA * 9.31);\n")
    TEXT("float cellTone = lerp(0.64, 1.15, cellToneSignal);\n")
    TEXT("float blindSignal = 0.5 + 0.5 * sin(dot(cellIndex, float2(113.17,47.73)) + facadeSignalB * 13.11);\n")
    TEXT("float blindMask = smoothstep(0.79, 0.89, blindSignal);\n")
    TEXT("float2 macroQ = float2(facadeUMetres / (bay * 4.0), sourceAbsoluteZMetres / (storey * 4.0));\n")
    TEXT("float macroWaveU = 0.5 + 0.5 * cos(6.2831853 * (macroQ.x + facadeSignalB));\n")
    TEXT("float macroWaveV = 0.5 + 0.5 * cos(6.2831853 * (macroQ.y + facadeSignalC));\n")
    TEXT("float macroTone = lerp(0.92, 1.05, saturate(0.55 * macroWaveU + 0.45 * macroWaveV));\n")
    TEXT("float3 viewDirectionRaw = CameraPositionCm - WorldPositionCm;\n")
    TEXT("float3 viewDirection = viewDirectionRaw / max(length(viewDirectionRaw), 0.0001);\n")
    TEXT("float2 dUvdx = ddx(UV0);\n")
    TEXT("float2 dUvdy = ddy(UV0);\n")
    TEXT("float uvDeterminant = dUvdx.x*dUvdy.y - dUvdx.y*dUvdy.x;\n")
    TEXT("float safeUvDeterminant = (uvDeterminant < 0.0 ? -1.0 : 1.0) * max(abs(uvDeterminant), 0.0000001);\n")
    TEXT("float3 tangentURaw = (dPdx*dUvdy.y - dPdy*dUvdx.y) / safeUvDeterminant;\n")
    TEXT("float3 tangentVRaw = (-dPdx*dUvdy.x + dPdy*dUvdx.x) / safeUvDeterminant;\n")
    TEXT("float3 tangentU = tangentURaw / max(length(tangentURaw), 0.0001);\n")
    TEXT("float3 tangentV = tangentVRaw / max(length(tangentVRaw), 0.0001);\n")
    TEXT("float validUvBasis = step(0.0000001, abs(uvDeterminant));\n")
    TEXT("float2 interiorShift = 0.115 * validUvBasis * wallMask * float2(dot(viewDirection,tangentU)/bay,dot(viewDirection,tangentV)/storey);\n")
    TEXT("float2 interiorPhase = frac(phase + interiorShift);\n")
    TEXT("float interiorBandV = lerp(0.72,1.0,smoothstep(0.12,0.88,interiorPhase.y));\n")
    TEXT("float interiorBandU = lerp(0.86,1.0,smoothstep(0.08,0.92,interiorPhase.x));\n")
    TEXT("float interiorBand = interiorBandU * interiorBandV;\n")
    TEXT("float grazing = 1.0 - saturate(abs(dot(geometricNormal, viewDirection)));\n")
    TEXT("float glassFresnel = pow(grazing, 4.0);\n")
    TEXT("float3 unoccupiedGlass = ApertureTint.rgb * lerp(0.52,0.76,cellTone);\n")
    TEXT("float3 occupiedGlass = ApertureTint.rgb * lerp(1.02,1.32,cellTone) + float3(0.012,0.010,0.006);\n")
    TEXT("float3 glassTint = saturate(lerp(unoccupiedGlass,occupiedGlass,occupied) * interiorBand);\n")
    TEXT("glassTint = lerp(glassTint, glassTint * float3(0.76,0.83,0.88), blindMask * 0.72);\n")
    TEXT("glassTint = lerp(glassTint, AtmosphereTint.rgb, glassFresnel * 0.32);\n")
    TEXT("float3 facadeSurface = surface * lerp(1.0, macroTone, wallMask);\n")
    TEXT("facadeSurface = lerp(facadeSurface,facadeSurface*float3(0.78,0.82,0.84),saturate(visibleSpandrelRhythm*0.58));\n")
    TEXT("facadeSurface = lerp(facadeSurface,saturate(surface*1.12+0.018),saturate(visiblePierRhythm*0.54));\n")
    TEXT("facadeSurface = lerp(facadeSurface,saturate(surface*1.16+0.025),saturate(visibleSlabRhythm*0.68));\n")
    TEXT("float visibleFrame = saturate(frameMask * facadeStrength);\n")
    TEXT("float visibleReveal = saturate(revealMask * facadeStrength * cellOccupancy);\n")
    TEXT("float visibleGlass = saturate(insetGlass * facadeStrength * cellOccupancy);\n")
    TEXT("float visibleDivider = saturate(dividerMask * facadeStrength * cellOccupancy);\n")
    TEXT("float3 frameTint = saturate(surface * 1.18 + 0.035);\n")
    TEXT("float3 framedSurface = lerp(facadeSurface, frameTint, visibleFrame * 0.78);\n")
    TEXT("float3 revealTint = glassTint * lerp(0.46, 0.64, grazing);\n")
    TEXT("float3 recessedSurface = lerp(framedSurface, revealTint, visibleReveal * 0.88);\n")
    TEXT("float3 glazedSurface = lerp(recessedSurface, glassTint, visibleGlass);\n")
    TEXT("float3 brokenSurface = lerp(glazedSurface, frameTint * 0.72, visibleDivider * 0.90);\n")
    TEXT("float atmosphereRange = max(AtmosphereEndCm - AtmosphereStartCm, 1.0);\n")
    TEXT("float atmosphere = saturate((distanceCm - AtmosphereStartCm) / atmosphereRange) * saturate(AtmosphereStrength);\n")
    TEXT("float luminance = dot(brokenSurface, float3(0.212639,0.715169,0.072192));\n")
    TEXT("float3 mutedSurface = lerp(brokenSurface, luminance.xxx, atmosphere * 0.26);\n")
    TEXT("float3 finalColor = saturate(lerp(mutedSurface, AtmosphereTint.rgb, atmosphere));\n")
    TEXT("float responseRoughness = lerp(saturate(SurfaceRoughness), saturate(PackedOrm.g), saturate(RoughnessTextureWeight));\n")
    TEXT("responseRoughness = saturate(responseRoughness + (facadeSignalC-0.5)*0.10 + roofMottle*0.08);\n")
    TEXT("responseRoughness = lerp(responseRoughness, lerp(0.19,0.31,blindMask), visibleGlass);\n")
    TEXT("responseRoughness = lerp(responseRoughness, 0.46, visibleDivider);\n")
    TEXT("float finalRoughness = lerp(responseRoughness, 1.0, atmosphere * 0.28);\n")
    TEXT("return float4(finalColor, saturate(finalRoughness));"));

FString ObjectPath(const FString& Root, const FString& Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

FString MaterialObjectPath(const FMaterialSpec& Spec)
{
    return ObjectPath(MaterialRoot, Spec.Name);
}

const TArray<FString>& OrderedMaterialPaths()
{
    static TArray<FString> Paths;
    if (Paths.IsEmpty())
    {
        Paths.Reserve(ExpectedMaterialCount);
        for (const FMaterialSpec& Spec : MaterialSpecs)
        {
            Paths.Add(MaterialObjectPath(Spec));
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

const TCHAR* TextureSuffix(ETextureUsage Usage)
{
    if (Usage == ETextureUsage::Normal)
    {
        return TEXT("Normal");
    }
    if (Usage == ETextureUsage::PackedOrm)
    {
        return TEXT("ORM");
    }
    return TEXT("BaseColor");
}

FString TextureObjectPath(const TCHAR* TextureSet, ETextureUsage Usage)
{
    const FString Name = FString::Printf(
        TEXT("T_IPV_%s_%s"), TextureSet, TextureSuffix(Usage));
    return ObjectPath(PublicViewTextureRoot, Name);
}

template <typename TObjectType>
TObjectType* LoadExact(const FString& Path)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *Path);
    return IsValid(Object) && Object->GetPathName() == Path ? Object : nullptr;
}

template <typename ElementType>
bool SameSet(const TSet<ElementType>& A, const TSet<ElementType>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (const ElementType& Value : A)
    {
        if (!B.Contains(Value))
        {
            return false;
        }
    }
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

FString SourceContractPath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
             "R31BroadShellLookdev/r31_broad_shell_lookdev.contract.json")));
}

FString BytesToHex(const uint8* Bytes, int32 Count)
{
    FString Result;
    Result.Reserve(Count * 2);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result += FString::Printf(TEXT("%02X"), Bytes[Index]);
    }
    return Result;
}

bool HasExactJsonString(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const FString& Expected)
{
    FString Actual;
    return Object.IsValid() && Object->TryGetStringField(Field, Actual) &&
        Actual == Expected;
}

bool HasExactJsonNumber(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    double Expected)
{
    double Actual = 0.0;
    return Object.IsValid() && Object->TryGetNumberField(Field, Actual) &&
        FMath::IsNearlyEqual(Actual, Expected, 0.000001);
}

bool HasExactJsonBool(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    bool Expected)
{
    bool Actual = false;
    return Object.IsValid() && Object->TryGetBoolField(Field, Actual) &&
        Actual == Expected;
}

bool HasExactJsonColor(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    const FLinearColor& Expected)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != 4)
    {
        return false;
    }
    const double ExpectedValues[] = {
        Expected.R, Expected.G, Expected.B, Expected.A};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (!(*Values)[Index].IsValid() ||
            (*Values)[Index]->Type != EJson::Number ||
            !FMath::IsNearlyEqual(
                (*Values)[Index]->AsNumber(), ExpectedValues[Index],
                0.000001))
        {
            return false;
        }
    }
    return true;
}

bool HasExactJsonNumberQuad(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    double A,
    double B,
    double C,
    double D)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) ||
        !Values || Values->Num() != 4)
    {
        return false;
    }
    const double Expected[] = {A, B, C, D};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        if (!(*Values)[Index].IsValid() ||
            (*Values)[Index]->Type != EJson::Number ||
            !FMath::IsNearlyEqual(
                (*Values)[Index]->AsNumber(), Expected[Index], 0.000001))
        {
            return false;
        }
    }
    return true;
}

bool IsUpperSha256(const FString& Value)
{
    if (Value.Len() != 64 || Value != Value.ToUpper())
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsHexDigit(Character))
        {
            return false;
        }
    }
    return true;
}

bool ResolvePinnedRepositoryFile(
    const FString& ContractPath,
    FString& OutFilename)
{
    FString Normalized = ContractPath;
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (!Normalized.StartsWith(TEXT("unreal/")) ||
        Normalized.Contains(TEXT("../")) ||
        Normalized.Contains(TEXT("/..")) ||
        FPaths::IsRelative(Normalized) == false)
    {
        return false;
    }
    FString ProjectRoot = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ProjectRoot);
    OutFilename = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        ProjectRoot, Normalized.RightChop(7)));
    FPaths::NormalizeFilename(OutFilename);
    return FPaths::IsUnderDirectory(OutFilename, ProjectRoot);
}

bool ValidatePinnedRepositoryFile(
    const TSharedPtr<FJsonObject>& Row,
    const FString& Label,
    FString* OutContractPath,
    FString& OutError)
{
    FString ContractPath;
    FString ExpectedSha256;
    double ExpectedBytesNumber = 0.0;
    if (!Row.IsValid() ||
        !Row->TryGetStringField(TEXT("file"), ContractPath) ||
        !Row->TryGetNumberField(TEXT("bytes"), ExpectedBytesNumber) ||
        !Row->TryGetStringField(TEXT("sha256"), ExpectedSha256) ||
        ExpectedBytesNumber <= 0.0 ||
        ExpectedBytesNumber !=
            static_cast<double>(static_cast<int64>(ExpectedBytesNumber)) ||
        !IsUpperSha256(ExpectedSha256))
    {
        OutError = TEXT("R31 broad-shell contract contains an invalid ") +
            Label + TEXT(" pin row.");
        return false;
    }
    FString Filename;
    if (!ResolvePinnedRepositoryFile(ContractPath, Filename))
    {
        OutError = TEXT("R31 broad-shell contract pin escaped the Unreal project: ") +
            ContractPath;
        return false;
    }
    TArray<uint8> FileBytes;
    const int64 ExpectedBytes = static_cast<int64>(ExpectedBytesNumber);
    if (!FFileHelper::LoadFileToArray(FileBytes, *Filename) ||
        FileBytes.Num() != ExpectedBytes)
    {
        OutError = FString::Printf(
            TEXT("R31 broad-shell %s byte guard failed: expected=%lld actual=%d file='%s'."),
            *Label, ExpectedBytes, FileBytes.Num(), *Filename);
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            FileBytes.GetData(), static_cast<size_t>(FileBytes.Num()), Digest) ==
            nullptr ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH) != ExpectedSha256)
    {
        OutError = TEXT("R31 broad-shell ") + Label +
            TEXT(" SHA-256 guard failed for '") + Filename + TEXT("'.");
        return false;
    }
#else
    OutError = TEXT("R31 broad-shell pin admission requires WITH_SSL SHA-256 support.");
    return false;
#endif
    if (OutContractPath)
    {
        *OutContractPath = ContractPath;
    }
    OutError.Reset();
    return true;
}

bool ValidateSourceContract(FString& OutError)
{
    // Updated only when the reviewed source contract changes.
    constexpr int64 ExpectedBytes = 30110;
    const FString ExpectedSha256(TEXT(
        "5ED127F1072A127A9ECEE51D1BD9496CE05E43E57FCB90F92A560840D6E97F27"));
    const FString Filename = SourceContractPath();
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename) ||
        Bytes.Num() != ExpectedBytes)
    {
        OutError = FString::Printf(
            TEXT("R31 broad-shell source-contract byte guard failed: expected=%lld actual=%d file='%s'."),
            ExpectedBytes, Bytes.Num(), *Filename);
        return false;
    }
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) == nullptr ||
        BytesToHex(Digest, SHA256_DIGEST_LENGTH) != ExpectedSha256)
    {
        OutError = TEXT("R31 broad-shell source-contract SHA-256 guard failed for '") +
            Filename + TEXT("'.");
        return false;
    }
#else
    OutError = TEXT("R31 broad-shell source admission requires WITH_SSL SHA-256 support.");
    return false;
#endif

    FString ContractText;
    TSharedPtr<FJsonObject> Root;
    if (!FFileHelper::LoadFileToString(ContractText, *Filename) ||
        !FJsonSerializer::Deserialize(
            TJsonReaderFactory<>::Create(ContractText), Root) ||
        !Root.IsValid() ||
        !HasExactJsonString(
            Root, TEXT("schema"),
            TEXT("triad.istana_explore_v5d.r31_broad_shell_lookdev.v1")))
    {
        OutError = TEXT("R31 broad-shell source contract could not be parsed as the exact schema.");
        return false;
    }

    const TSharedPtr<FJsonObject>* SourceMesh = nullptr;
    const TSharedPtr<FJsonObject>* SourceObj = nullptr;
    const TSharedPtr<FJsonObject>* ImportedAsset = nullptr;
    if (!Root->TryGetObjectField(TEXT("sourceMesh"), SourceMesh) ||
        !SourceMesh || !(*SourceMesh)->TryGetObjectField(
            TEXT("sourceObj"), SourceObj) ||
        !SourceObj || !(*SourceMesh)->TryGetObjectField(
            TEXT("importedAsset"), ImportedAsset) ||
        !ImportedAsset ||
        !HasExactJsonNumber(*SourceObj, TEXT("vertexRecordCount"), 24522.0) ||
        !HasExactJsonNumber(
            *SourceObj, TEXT("textureCoordinateRecordCount"), 130632.0) ||
        !HasExactJsonNumber(*SourceObj, TEXT("faceCount"), 43448.0) ||
        !HasExactJsonString(
            *ImportedAsset, TEXT("assetObjectPath"),
            TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV2/SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2")) ||
        !HasExactJsonNumber(*ImportedAsset, TEXT("triangleCount"), 43448.0) ||
        !HasExactJsonNumber(*ImportedAsset, TEXT("materialSlotCount"), 17.0) ||
        !HasExactJsonNumber(*ImportedAsset, TEXT("renderVertexCount"), 24468.0) ||
        !HasExactJsonNumber(
            *ImportedAsset, TEXT("meshDescriptionVertexInstanceCount"),
            130344.0) ||
        !HasExactJsonNumber(
            *SourceMesh, TEXT("canonicalSourceGroupCount"), 1391.0) ||
        !HasExactJsonNumber(
            *SourceMesh, TEXT("retainedSuppressionV2GroupCount"), 1388.0) ||
        !HasExactJsonBool(
            *SourceMesh, TEXT("groupIdentifiersAndOrderPreserved"), true))
    {
        OutError = TEXT("R31 broad-shell raw/imported V2 source census drifted.");
        return false;
    }
    FString SourceObjPath;
    if (!ValidatePinnedRepositoryFile(
            *SourceObj, TEXT("source OBJ"), &SourceObjPath, OutError) ||
        SourceObjPath !=
            TEXT("unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.obj"))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* SourcePins = nullptr;
    if (!Root->TryGetArrayField(TEXT("sourcePins"), SourcePins) ||
        !SourcePins || SourcePins->Num() != 5)
    {
        OutError = TEXT("R31 broad-shell contract must contain exactly five retained source pins.");
        return false;
    }
    const TSet<FString> ExpectedSourcePinPaths = {
        TEXT("unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/SM_IPV5D_OSMCurrentSurroundings_Render.mtl"),
        TEXT("unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/IstanaPublicViewV5DLocalFallbackSuppression.v2.metadata.json"),
        TEXT("unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/IstanaPublicViewV5DLocalFallbackSuppression.v2.manifest.json"),
        TEXT("unreal/Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/local_fallback_suppression_v2.contract.json"),
        TEXT("unreal/SourceAssets/IstanaPublicView/Textures/Generated/manifest.json")};
    TSet<FString> ActualSourcePinPaths;
    for (const TSharedPtr<FJsonValue>& Value : *SourcePins)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString ContractPath;
        if (!ValidatePinnedRepositoryFile(
                Row, TEXT("retained source"), &ContractPath, OutError) ||
            ActualSourcePinPaths.Contains(ContractPath))
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("R31 broad-shell contract contains a duplicate retained source pin.");
            }
            return false;
        }
        ActualSourcePinPaths.Add(ContractPath);
    }
    if (!SameSet(ExpectedSourcePinPaths, ActualSourcePinPaths))
    {
        OutError = TEXT("R31 broad-shell retained source-pin roster drifted.");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* TexturePins = nullptr;
    if (!Root->TryGetArrayField(TEXT("texturePins"), TexturePins) ||
        !TexturePins || TexturePins->Num() != 9)
    {
        OutError = TEXT("R31 broad-shell contract must contain exactly nine texture pins.");
        return false;
    }
    TSet<FString> TextureKeys;
    for (const TSharedPtr<FJsonValue>& Value : *TexturePins)
    {
        const TSharedPtr<FJsonObject> Row = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        FString TextureSet;
        FString Usage;
        FString ContractPath;
        if (!Row.IsValid() ||
            !Row->TryGetStringField(TEXT("set"), TextureSet) ||
            !Row->TryGetStringField(TEXT("usage"), Usage) ||
            !ValidatePinnedRepositoryFile(
                Row, TEXT("texture source"), &ContractPath, OutError))
        {
            return false;
        }
        const FString Key = TextureSet + TEXT("/") + Usage;
        const FString ExpectedPath = FString::Printf(
            TEXT("unreal/SourceAssets/IstanaPublicView/Textures/Generated/T_IPV_%s_%s.png"),
            *TextureSet, *Usage);
        if ((TextureSet != TEXT("Plaster") && TextureSet != TEXT("Stone") &&
             TextureSet != TEXT("Slate")) ||
            (Usage != TEXT("BaseColor") && Usage != TEXT("Normal") &&
             Usage != TEXT("ORM")) ||
            ContractPath != ExpectedPath || TextureKeys.Contains(Key))
        {
            OutError = TEXT("R31 broad-shell texture pin set/usage/path roster drifted: ") +
                Key;
            return false;
        }
        TextureKeys.Add(Key);
    }
    if (TextureKeys.Num() != 9)
    {
        OutError = TEXT("R31 broad-shell texture pin matrix is incomplete.");
        return false;
    }

    const TSharedPtr<FJsonObject>* AssetPackage = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Instances = nullptr;
    if (!Root->TryGetObjectField(TEXT("assetPackage"), AssetPackage) ||
        !AssetPackage ||
        !HasExactJsonString(*AssetPackage, TEXT("root"), AssetRoot) ||
        !HasExactJsonString(*AssetPackage, TEXT("master"), MasterName) ||
        !(*AssetPackage)->TryGetArrayField(TEXT("instances"), Instances) ||
        !Instances || Instances->Num() != ExpectedMaterialCount)
    {
        OutError = TEXT("R31 broad-shell asset-package contract drifted.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedMaterialCount; ++Index)
    {
        const TSharedPtr<FJsonObject> Row = (*Instances)[Index].IsValid()
            ? (*Instances)[Index]->AsObject()
            : nullptr;
        const FMaterialSpec& Spec = MaterialSpecs[Index];
        if (!HasExactJsonString(Row, TEXT("role"), Spec.Role) ||
            !HasExactJsonString(Row, TEXT("name"), Spec.Name) ||
            !HasExactJsonString(Row, TEXT("textureSet"), Spec.TextureSet) ||
            !HasExactJsonColor(Row, TEXT("tint"), Spec.Tint) ||
            !HasExactJsonColor(
                Row, TEXT("apertureTint"), Spec.ApertureTint) ||
            !HasExactJsonColor(
                Row, TEXT("atmosphereTint"), Spec.AtmosphereTint) ||
            !HasExactJsonNumber(Row, TEXT("metresPerTile"), Spec.MetresPerTile) ||
            !HasExactJsonNumber(
                Row, TEXT("textureInfluence"), Spec.TextureInfluence) ||
            !HasExactJsonNumber(
                Row, TEXT("normalStrength"), Spec.NormalStrength) ||
            !HasExactJsonNumber(
                Row, TEXT("surfaceRoughness"), Spec.SurfaceRoughness) ||
            !HasExactJsonNumber(
                Row, TEXT("roughnessTextureWeight"),
                Spec.RoughnessTextureWeight) ||
            !HasExactJsonNumber(Row, TEXT("metallic"), Spec.Metallic) ||
            !HasExactJsonNumber(Row, TEXT("specular"), Spec.Specular) ||
            !HasExactJsonNumber(
                Row, TEXT("aoTextureWeight"), Spec.AoTextureWeight) ||
            !HasExactJsonNumber(
                Row, TEXT("variationCellMeters"), Spec.VariationCellMeters) ||
            !HasExactJsonNumber(
                Row, TEXT("weatheringStrength"), Spec.WeatheringStrength) ||
            !HasExactJsonNumber(
                Row, TEXT("wallVerticalWeatherMask"),
                Spec.WallVerticalWeatherMask) ||
            !HasExactJsonNumber(Row, TEXT("bayMeters"), Spec.BayMeters) ||
            !HasExactJsonNumber(
                Row, TEXT("storeyMeters"), Spec.StoreyMeters) ||
            !HasExactJsonNumber(
                Row, TEXT("apertureWidthFraction"),
                Spec.ApertureWidthFraction) ||
            !HasExactJsonNumber(
                Row, TEXT("apertureHeightFraction"),
                Spec.ApertureHeightFraction) ||
            !HasExactJsonNumber(
                Row, TEXT("apertureSillFraction"),
                Spec.ApertureSillFraction) ||
            !HasExactJsonNumber(
                Row, TEXT("apertureHintStrength"),
                Spec.ApertureHintStrength) ||
            !HasExactJsonNumber(
                Row, TEXT("apertureFadeStartCm"),
                Spec.ApertureFadeStartCm) ||
            !HasExactJsonNumber(
                Row, TEXT("apertureFadeEndCm"),
                Spec.ApertureFadeEndCm) ||
            !HasExactJsonNumber(
                Row, TEXT("atmosphereStartCm"), Spec.AtmosphereStartCm) ||
            !HasExactJsonNumber(
                Row, TEXT("atmosphereEndCm"), Spec.AtmosphereEndCm) ||
            !HasExactJsonNumber(
                Row, TEXT("atmosphereStrength"), Spec.AtmosphereStrength))
        {
            OutError = FString::Printf(
                TEXT("R31 broad-shell JSON instance spec drifted at index %d."),
                Index);
            return false;
        }
    }

    const TSharedPtr<FJsonObject>* UvSemantics = nullptr;
    const TSharedPtr<FJsonObject>* SourceUvProbe = nullptr;
    if (!Root->TryGetObjectField(TEXT("uvSemantics"), UvSemantics) ||
        !UvSemantics ||
        !HasExactJsonString(
            *UvSemantics, TEXT("sourceObj"),
            TEXT("walls=(per-segment U from 0 to segment length using lexicographically sorted XY endpoints; V=absolute source Z); roofs and hidden bottoms=(hero-local X,Y)")) ||
        !HasExactJsonString(
            *UvSemantics, TEXT("unrealImport"),
            TEXT("UV0.y = 1 - source OBJ V")) ||
        !HasExactJsonString(
            *UvSemantics, TEXT("shaderRecovery"),
            TEXT("sourceAbsoluteZMetres = 1 - UV0.y; wall storey cadence uses this absolute source-Z phase")) ||
        !HasExactJsonBool(
            *UvSemantics,
            TEXT("buildingLocalAboveGradeCoordinateAvailable"), false) ||
        !HasExactJsonBool(
            *UvSemantics, TEXT("groundContactOrPlinthFromUvAllowed"),
            false) ||
        !(*UvSemantics)->TryGetObjectField(
            TEXT("sourceUvProbe"), SourceUvProbe) ||
        !SourceUvProbe ||
        !HasExactJsonBool(*SourceUvProbe, TEXT("required"), true) ||
        !HasExactJsonNumber(
            *SourceUvProbe, TEXT("wallTriangleCount"), 24360.0) ||
        !HasExactJsonNumber(
            *SourceUvProbe, TEXT("roofOrHiddenBottomTriangleCount"),
            19088.0) ||
        !HasExactJsonNumber(
            *SourceUvProbe, TEXT("maximumWallUMetresError"),
            0.000001) ||
        !HasExactJsonNumber(
            *SourceUvProbe, TEXT("maximumWallVAbsoluteZMetresError"),
            0.0) ||
        !HasExactJsonNumber(
            *SourceUvProbe, TEXT("maximumPlanUvHeroLocalXyMetresError"),
            0.0) ||
        !HasExactJsonNumber(
            *SourceUvProbe, TEXT("maximumMismatchCornerCount"), 0.0))
    {
        OutError = TEXT("R31 broad-shell exact source-UV semantics contract drifted.");
        return false;
    }

    const TSharedPtr<FJsonObject>* AppearanceVariation = nullptr;
    const TSharedPtr<FJsonObject>* IdentitySource = nullptr;
    const TSharedPtr<FJsonObject>* CadencePhase = nullptr;
    const TSharedPtr<FJsonObject>* Glazing = nullptr;
    const TSharedPtr<FJsonObject>* Weathering = nullptr;
    const TSharedPtr<FJsonObject>* ShallowInterior = nullptr;
    const TSharedPtr<FJsonObject>* FamilyColour = nullptr;
    const TSharedPtr<FJsonObject>* ArchitecturalRhythm = nullptr;
    if (!Root->TryGetObjectField(
            TEXT("appearanceVariation"), AppearanceVariation) ||
        !AppearanceVariation ||
        !HasExactJsonString(
            *AppearanceVariation, TEXT("presentationRange"),
            TEXT("mid/far context only")) ||
        !HasExactJsonNumber(
            *AppearanceVariation, TEXT("facadeFamilyCount"), 4.0) ||
        !(*AppearanceVariation)->TryGetObjectField(
            TEXT("identitySource"), IdentitySource) ||
        !IdentitySource ||
        !HasExactJsonString(
            *IdentitySource, TEXT("method"),
            TEXT("continuous canonical-normal and signed-plane-distance signals with normalized four-archetype blending")) ||
        !HasExactJsonString(
            *IdentitySource, TEXT("parallelPlaneDistanceScaleMetres"),
            TEXT("per-instance variationCellMeters")) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("arrayPositionUsed"), false) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("shortRepeatingCycleUsed"), false) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("exactOsmGroupIdVisibleToPixelShader"),
            false) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("worldXyGridCellUsed"), false) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("hardNormalQuantizationUsed"), false) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("hardPlaneDistanceQuantizationUsed"),
            false) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("hardFacadeFamilyThresholdUsed"),
            false) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("normalizedFourArchetypeBlend"), true) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("constantWithinPlanarFace"), true) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("naturalCornerBreaksAllowed"), true) ||
        !HasExactJsonBool(
            *IdentitySource, TEXT("cadenceWarpingUsed"), false) ||
        !(*AppearanceVariation)->TryGetObjectField(
            TEXT("familyColourMultipliers"), FamilyColour) ||
        !FamilyColour ||
        !HasExactJsonNumberQuad(
            *FamilyColour, TEXT("red"), 0.96, 1.04, 1.02, 0.98) ||
        !HasExactJsonNumberQuad(
            *FamilyColour, TEXT("green"), 0.99, 1.01, 0.97, 1.03) ||
        !HasExactJsonNumberQuad(
            *FamilyColour, TEXT("blue"), 1.04, 0.96, 0.94, 1.01) ||
        !(*AppearanceVariation)->TryGetObjectField(
            TEXT("architecturalRhythm"), ArchitecturalRhythm) ||
        !ArchitecturalRhythm ||
        !HasExactJsonNumberQuad(
            *ArchitecturalRhythm, TEXT("familySpandrelStrengths"),
            0.34, 0.16, 0.52, 0.26) ||
        !HasExactJsonNumberQuad(
            *ArchitecturalRhythm, TEXT("familyPierStrengths"),
            0.18, 0.42, 0.22, 0.56) ||
        !HasExactJsonNumberQuad(
            *ArchitecturalRhythm, TEXT("familySlabStrengths"),
            0.10, 0.30, 0.58, 0.18) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm, TEXT("continuousTwoStoreyWave"), true) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm,
            TEXT("hardFloorParitySelectorUsed"), false) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm,
            TEXT("antialiasedFacadeEdgeMasks"), true) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm, TEXT("wallRoleGated"), true) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm, TEXT("distanceFadeGated"), true) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm, TEXT("microReadabilityGated"), true) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm,
            TEXT("geometryOrSilhouetteChanged"), false) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm, TEXT("balconyGeometryClaimed"), false) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm,
            TEXT("exactPerBuildingStyleClaimed"), false) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm,
            TEXT("renderOnlyDepthImpression"), true) ||
        !HasExactJsonNumber(
            *ArchitecturalRhythm,
            TEXT("minimumPairwiseFamilySignatureDistance"), 0.05) ||
        !HasExactJsonNumber(
            *ArchitecturalRhythm,
            TEXT("maximumPhaseWrapCueDelta"), 0.001) ||
        !HasExactJsonBool(
            *ArchitecturalRhythm,
            TEXT("sourceGrammarProbeRequired"), true) ||
        !(*AppearanceVariation)->TryGetObjectField(
            TEXT("cadencePhase"), CadencePhase) ||
        !CadencePhase ||
        !HasExactJsonBool(
            *CadencePhase, TEXT("vertexTangentWorldPlaneFacadeU"), true) ||
        !HasExactJsonBool(
            *CadencePhase,
            TEXT("vertexTangentDominantAxisWorldCoordinate"), true) ||
        !HasExactJsonBool(
            *CadencePhase,
            TEXT("dominantComponentQuantizedMetricScaleRestoration"), true) ||
        !HasExactJsonBool(
            *CadencePhase,
            TEXT("orientationDependentFacadeStretchIntroduced"), false) ||
        !HasExactJsonBool(
            *CadencePhase, TEXT("sourcePerSegmentUUsedForWallCadence"),
            false) ||
        !HasExactJsonBool(
            *CadencePhase, TEXT("coplanarSegmentResetSeamsAllowed"),
            false) ||
        !(*AppearanceVariation)->TryGetObjectField(
            TEXT("glazing"), Glazing) ||
        !Glazing ||
        !HasExactJsonNumber(
            *Glazing, TEXT("occupancyThresholdTransitionHalfWidth"),
            0.07) ||
        !HasExactJsonNumber(
            *Glazing, TEXT("unoccupiedCueStrength"), 0.38) ||
        !HasExactJsonNumber(
            *Glazing, TEXT("occupiedCueStrength"), 1.0) ||
        !HasExactJsonNumber(
            *Glazing, TEXT("blindThresholdCentre"), 0.84) ||
        !HasExactJsonNumber(
            *Glazing, TEXT("blindThresholdTransitionHalfWidth"), 0.05) ||
        !HasExactJsonBool(*Glazing, TEXT("emissive"), false) ||
        !(*AppearanceVariation)->TryGetObjectField(
            TEXT("weathering"), Weathering) ||
        !Weathering ||
        !HasExactJsonBool(*Weathering, TEXT("wallVerticalDirt"), false) ||
        !HasExactJsonBool(
            *Weathering, TEXT("plinthContactDarkening"), false) ||
        !HasExactJsonBool(*Weathering, TEXT("wallRunoff"), true) ||
        !HasExactJsonBool(*Weathering, TEXT("roofMottling"), true) ||
        !HasExactJsonBool(
            *Weathering, TEXT("buildingLocalGradeVisibleToPixelShader"),
            false) ||
        !(*AppearanceVariation)->TryGetObjectField(
            TEXT("shallowInteriorCue"), ShallowInterior) ||
        !ShallowInterior ||
        !HasExactJsonNumber(
            *ShallowInterior, TEXT("nominalDepthMetres"), 0.115) ||
        !HasExactJsonBool(
            *ShallowInterior, TEXT("viewShiftedInFacadeUvBasis"), true) ||
        !HasExactJsonBool(
            *ShallowInterior, TEXT("geometryDisplacement"), false) ||
        !HasExactJsonBool(
            *ShallowInterior, TEXT("worldPositionOffset"), false) ||
        !HasExactJsonBool(
            *ShallowInterior, TEXT("pixelDepthOffset"), false))
    {
        OutError = TEXT("R31 broad-shell seam-safe facade-plane appearance-variation contract drifted.");
        return false;
    }

    const TSharedPtr<FJsonObject>* SeamSafety = nullptr;
    if (!Root->TryGetObjectField(TEXT("seamSafety"), SeamSafety) ||
        !SeamSafety ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("legacyWorldXyGridDiagnosticRetainedForComparisonOnly"),
            true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("legacyWorldXyGridMayDriveRuntimeAppearance"), false) ||
        !HasExactJsonBool(
            *SeamSafety, TEXT("sourceObjGroupAndTriangleProbeRequired"),
            true) ||
        !HasExactJsonBool(
            *SeamSafety, TEXT("probeIncludesWallRoofAndHiddenBottom"),
            true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("coordinateMatchedCoplanarWallEdgeProbeRequired"), true) ||
        !HasExactJsonBool(
            *SeamSafety, TEXT("projectionSelectorEqualityRequired"), true) ||
        !HasExactJsonString(
            *SeamSafety, TEXT("probeIdentity"),
            TEXT("continuous canonical-normal and signed-plane-distance signals plus VertexTangentWS dominant-axis world U with 4096-level quantized metric-scale restoration")) ||
        !HasExactJsonNumber(
            *SeamSafety, TEXT("maximumWithinTriangleFacadeSignalDelta"),
            0.001) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("maximumCoplanarSharedEdgeFacadeSignalDelta"), 0.001) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("maximumCoplanarWallCadenceUJumpMetres"), 0.002) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("maximumCoplanarMetricTextureUvJumpMetres"), 0.002) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("vertexTangentDominantComponentQuantizationLevels"),
            4096.0) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("vertexTangentDominantComponentQuantizationPhase"),
            89.0 / 1048576.0) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("maximumWallMetricScaleRelativeError"), 0.00018) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("minimumWallCanonicalTangentSourceUAlignment"),
            0.999999) ||
        !HasExactJsonNumber(
            *SeamSafety,
            TEXT("maximumToleranceExceededCount"),
            0.0) ||
        !HasExactJsonBool(
            *SeamSafety, TEXT("hardDiscontinuousPlaneSelectorAllowed"),
            false) ||
        !HasExactJsonBool(
            *SeamSafety, TEXT("hardFacadeArchetypeThresholdAllowed"),
            false) ||
        !HasExactJsonBool(
            *SeamSafety, TEXT("naturalCornerBreaksAreNotSeams"), true) ||
        !HasExactJsonBool(
            *SeamSafety, TEXT("nativeReviewMustRejectWorldGridSeams"),
            true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("nativeReviewMustRejectCheckerboardCutsThroughContinuousPlanes"),
            true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("nativeReviewMustRejectVisibleCoplanarCadenceOrTexturePhaseSeams"),
            true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("sourceProbeProvesWithinRenderPathContinuity"), true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("sourceProbeProvesNaniteRasterAppearanceParity"), false) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("nativeNaniteRasterHighOccupancyPairRequired"), true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("nativeReviewMustRejectNaniteRasterCadenceFamilyTextureOrNormalPop"),
            true) ||
        !HasExactJsonBool(
            *SeamSafety,
            TEXT("sourceOrOfflineProbeMayAuthorizeNativeAcceptance"), false))
    {
        OutError = TEXT("R31 broad-shell facade-plane seam-safety contract drifted.");
        return false;
    }
    const TSharedPtr<FJsonObject>* TextureAssetBinding = nullptr;
    if (!Root->TryGetObjectField(
            TEXT("textureAssetBinding"), TextureAssetBinding) ||
        !TextureAssetBinding ||
        !HasExactJsonBool(
            *TextureAssetBinding, TEXT("sourceFileSha256Pinned"), true) ||
        !HasExactJsonBool(
            *TextureAssetBinding,
            TEXT("singleSourceImportPathRequired"), true) ||
        !HasExactJsonBool(
            *TextureAssetBinding,
            TEXT("storedSourceMd5MustMatchCurrentFile"), true) ||
        !HasExactJsonBool(
            *TextureAssetBinding, TEXT("sourceIdRequiredNonEmpty"), true) ||
        !HasExactJsonBool(
            *TextureAssetBinding,
            TEXT("sourceIdClaimedAsPayloadDigest"), false) ||
        !HasExactJsonBool(
            *TextureAssetBinding,
            TEXT("embeddedPixelByteEqualityClaim"), false) ||
        !HasExactJsonBool(
            *TextureAssetBinding,
            TEXT("nativeTextureTreeImmutableDuringTransaction"), true))
    {
        OutError = TEXT("R31 broad-shell practical texture-asset binding contract drifted.");
        return false;
    }

    const TSharedPtr<FJsonObject>* NativeTransaction = nullptr;
    if (!Root->TryGetObjectField(
            TEXT("nativeTransaction"), NativeTransaction) ||
        !NativeTransaction ||
        !HasExactJsonNumber(
            *NativeTransaction, TEXT("contractReferencedFileCount"), 15.0) ||
        !HasExactJsonNumber(
            *NativeTransaction,
            TEXT("promotedContractReferencedFileCount"), 4.0) ||
        !HasExactJsonNumber(
            *NativeTransaction,
            TEXT("retainedContractReferencedFileCount"), 11.0))
    {
        OutError = TEXT("R31 broad-shell native referenced-file closure drifted.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateSingleTextureSourceProvenance(
    UTexture2D* Texture,
    const FString& ExpectedFilename,
    FString& OutError)
{
    FString ExpectedSource = FPaths::ConvertRelativePathToFull(ExpectedFilename);
    FPaths::NormalizeFilename(ExpectedSource);
    const UAssetImportData* ImportData = Texture ? Texture->AssetImportData : nullptr;
    const TArray<FString> ImportedFilenames = ImportData
        ? ImportData->ExtractFilenames()
        : TArray<FString>();
    if (!ImportData || ImportedFilenames.Num() != 1 ||
        ImportData->GetSourceData().SourceFiles.Num() != 1)
    {
        OutError = TEXT("R31 shell texture must retain exactly one source provenance record.");
        return false;
    }
    FString ActualSource = FPaths::ConvertRelativePathToFull(
        ImportedFilenames[0]);
    FPaths::NormalizeFilename(ActualSource);
    const FMD5Hash CurrentHash = FMD5Hash::HashFile(*ExpectedSource);
    const FMD5Hash& ImportedHash =
        ImportData->GetSourceData().SourceFiles[0].FileHash;
    if (!FPaths::IsSamePath(ActualSource, ExpectedSource) ||
        !CurrentHash.IsValid() || !ImportedHash.IsValid() ||
        CurrentHash != ImportedHash)
    {
        OutError = TEXT("R31 shell texture import path or stored source-content MD5 is stale.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateTexture(
    UTexture2D* Texture,
    const TCHAR* TextureSet,
    ETextureUsage Usage,
    FString& OutError)
{
    const FString ExpectedSource = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceAssets/IstanaPublicView/Textures/Generated"),
        FString::Printf(
            TEXT("T_IPV_%s_%s.png"), TextureSet, TextureSuffix(Usage)));
    if (!Texture ||
        Texture->GetClass() != UTexture2D::StaticClass() ||
        Texture->GetPathName() != TextureObjectPath(TextureSet, Usage) ||
        Texture->GetPathName().Contains(TEXT("/HeroMaterials")) ||
        !Texture->Source.IsValid() ||
        Texture->Source.GetSizeX() != 2048 ||
        Texture->Source.GetSizeY() != 2048 ||
        Texture->Source.GetNumSlices() != 1 ||
        Texture->Source.GetNumLayers() != 1 ||
        Texture->Source.GetNumMips() != 1 ||
        Texture->Source.GetNumBlocks() != 1 ||
        Texture->Source.GetFormat() != TSF_BGRA8 ||
        Texture->Source.GetIdString().IsEmpty() ||
        Texture->MipGenSettings != TMGS_FromTextureGroup ||
        Texture->AddressX != TA_Wrap || Texture->AddressY != TA_Wrap ||
        Texture->Filter != TF_Default ||
        Texture->bFlipGreenChannel ||
        Texture->VirtualTextureStreaming || Texture->NeverStream ||
        Texture->LODBias != 0 || Texture->MaxTextureSize != 0 ||
        Texture->PowerOfTwoMode != ETexturePowerOfTwoSetting::None ||
        Texture->CompressionNoAlpha ||
        static_cast<bool>(Texture->SRGB) !=
            (Usage == ETextureUsage::BaseColor) ||
        Texture->CompressionSettings !=
            (Usage == ETextureUsage::Normal
                 ? TC_Normalmap
                 : Usage == ETextureUsage::PackedOrm ? TC_Masks : TC_Default) ||
        Texture->LODGroup !=
            (Usage == ETextureUsage::Normal
                 ? TEXTUREGROUP_WorldNormalMap
                 : TEXTUREGROUP_World))
    {
        OutError = TEXT("An R31 shell texture lost its exact 2048px source/color-space/compression/mip/wrap/filter/streaming/alpha contract.");
        return false;
    }
    return ValidateSingleTextureSourceProvenance(
        Texture, ExpectedSource, OutError);
}

bool LoadTextureSet(
    const TCHAR* TextureSet,
    TMap<FName, UTexture2D*>& OutTextures,
    FString& OutError)
{
    for (ETextureUsage Usage : {
             ETextureUsage::BaseColor,
             ETextureUsage::Normal,
             ETextureUsage::PackedOrm})
    {
        const FString Path = TextureObjectPath(TextureSet, Usage);
        UTexture2D* Texture = LoadExact<UTexture2D>(Path);
        if (!ValidateTexture(Texture, TextureSet, Usage, OutError))
        {
            OutError = TEXT("R31 refused shell texture '") + Path +
                TEXT("': ") + OutError;
            return false;
        }
        OutTextures.Add(
            FName(*FString::Printf(
                TEXT("%s_%s"), TextureSet, TextureSuffix(Usage))),
            Texture);
    }
    return true;
}

bool LoadAllTextures(
    TMap<FName, UTexture2D*>& OutTextures,
    FString& OutError)
{
    OutTextures.Reset();
    for (const TCHAR* TextureSet : TextureSets)
    {
        if (!LoadTextureSet(TextureSet, OutTextures, OutError))
        {
            return false;
        }
    }
    return OutTextures.Num() == UE_ARRAY_COUNT(TextureSets) * 3;
}

UTexture2D* FindTexture(
    const TMap<FName, UTexture2D*>& Textures,
    const TCHAR* TextureSet,
    ETextureUsage Usage)
{
    return Textures.FindRef(FName(*FString::Printf(
        TEXT("%s_%s"), TextureSet, TextureSuffix(Usage))));
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
    const bool bQuerySucceeded = Registry.Get().GetAssetsByPath(
        FName(*AssetRoot), OutAssets, true, false);
    if (!bQuerySucceeded || Registry.Get().IsLoadingAssets())
    {
        OutError = TEXT("R31 broad-shell Asset Registry discovery failed or did not complete.");
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
            TEXT("R31 broad-shell root must contain exactly five assets; actual=[%s]."),
            *FString::Join(Actual, TEXT(", ")));
        return false;
    }
    if (bRequireSaved)
    {
        for (const FString& Path : Expected)
        {
            UObject* Object = LoadObject<UObject>(nullptr, *Path);
            if (!Object || !FPackageName::DoesPackageExist(
                    Object->GetOutermost()->GetName()) ||
                Object->GetOutermost()->IsDirty())
            {
                OutError = TEXT("An R31 broad-shell asset is not persisted and clean: ") +
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
            Error += TEXT(" R31_BROAD_SHELL_ASSET_ROLLBACK_INCOMPLETE");
        }
        Assets.Reset();
    }

    void Commit() { bCommitted = true; }

private:
    TArray<UObject*>& Assets;
    FString& Error;
    bool bCommitted = false;
};

template <typename TExpression>
TExpression* AddExpression(
    UMaterial* Material,
    const TCHAR* Description,
    int32 X,
    int32 Y)
{
    TExpression* Expression = Cast<TExpression>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            Material, TExpression::StaticClass(), X, Y));
    if (Expression)
    {
        Expression->Desc = Description;
    }
    return Expression;
}

UMaterialExpressionScalarParameter* AddScalar(
    UMaterial* Material,
    const TCHAR* Description,
    const FName& Name,
    float DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionScalarParameter* Parameter =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, Description, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionVectorParameter* AddVector(
    UMaterial* Material,
    const TCHAR* Description,
    const FName& Name,
    const FLinearColor& DefaultValue,
    int32 X,
    int32 Y)
{
    UMaterialExpressionVectorParameter* Parameter =
        AddExpression<UMaterialExpressionVectorParameter>(
            Material, Description, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->Group = ParameterGroup;
        Parameter->DefaultValue = DefaultValue;
        Parameter->bUseCustomPrimitiveData = false;
        Parameter->PrimitiveDataIndex = 0;
    }
    return Parameter;
}

UMaterialExpressionTextureSampleParameter2D* AddTextureParameter(
    UMaterial* Material,
    const TCHAR* Description,
    const FName& Name,
    UTexture2D* Texture,
    EMaterialSamplerType SamplerType,
    int32 X,
    int32 Y)
{
    UMaterialExpressionTextureSampleParameter2D* Parameter =
        AddExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, Description, X, Y);
    if (Parameter)
    {
        Parameter->ParameterName = Name;
        Parameter->Group = ParameterGroup;
        Parameter->Texture = Texture;
        Parameter->SamplerType = SamplerType;
        Parameter->SamplerSource = SSM_FromTextureAsset;
        Parameter->MipValueMode = TMVM_None;
        Parameter->ConstCoordinate = 0;
        Parameter->ConstMipValue = INDEX_NONE;
        Parameter->AutomaticViewMipBias = true;
        Parameter->AutoSetSampleType();
    }
    return Parameter;
}

bool Connect(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    UMaterialExpression* To,
    const TCHAR* ToInput,
    const TCHAR* Label,
    FString& OutError)
{
    if (!From || !To ||
        !UMaterialEditingLibrary::ConnectMaterialExpressions(
            From, FString(FromOutput), To, FString(ToInput)))
    {
        OutError = TEXT("Could not connect R31 broad-shell graph edge '") +
            FString(Label) + TEXT("'.");
        return false;
    }
    return true;
}

bool ConnectProperty(
    UMaterialExpression* From,
    const TCHAR* FromOutput,
    EMaterialProperty Property,
    const TCHAR* Label,
    FString& OutError)
{
    if (!From || !UMaterialEditingLibrary::ConnectMaterialProperty(
                     From, FString(FromOutput), Property))
    {
        OutError = TEXT("Could not connect R31 broad-shell property '") +
            FString(Label) + TEXT("'.");
        return false;
    }
    return true;
}

bool InputMatchesExactOutput(
    const FExpressionInput& Input,
    const UMaterialExpression* Expected,
    int32 ExpectedOutputIndex = 0)
{
    if (!Expected || !Expected->Outputs.IsValidIndex(ExpectedOutputIndex))
    {
        return false;
    }
    const FExpressionOutput& Output = Expected->Outputs[ExpectedOutputIndex];
    return Input.Expression == Expected &&
        Input.OutputIndex == ExpectedOutputIndex &&
        Input.Mask == Output.Mask && Input.MaskR == Output.MaskR &&
        Input.MaskG == Output.MaskG && Input.MaskB == Output.MaskB &&
        Input.MaskA == Output.MaskA;
}

bool InputIsExactlyDisconnected(const FExpressionInput& Input)
{
    return !Input.Expression && Input.OutputIndex == 0 && Input.Mask == 0 &&
        Input.MaskR == 0 && Input.MaskG == 0 && Input.MaskB == 0 &&
        Input.MaskA == 0;
}

bool OutputMatchesExplicitDescriptor(
    const FExpressionOutput& Actual,
    const TCHAR* ExpectedName,
    int32 ExpectedMask,
    int32 ExpectedMaskR,
    int32 ExpectedMaskG,
    int32 ExpectedMaskB,
    int32 ExpectedMaskA)
{
    return Actual.OutputName == FName(ExpectedName) &&
        Actual.Mask == ExpectedMask && Actual.MaskR == ExpectedMaskR &&
        Actual.MaskG == ExpectedMaskG && Actual.MaskB == ExpectedMaskB &&
        Actual.MaskA == ExpectedMaskA;
}

bool OutputsMatchExplicitR31Contract(const UMaterialExpression* Expression)
{
    if (!Expression)
    {
        return false;
    }

    const auto OutputMatches = [Expression](
        int32 Index,
        const TCHAR* Name,
        int32 Mask,
        int32 MaskR,
        int32 MaskG,
        int32 MaskB,
        int32 MaskA)
    {
        return Expression->Outputs.IsValidIndex(Index) &&
            OutputMatchesExplicitDescriptor(
                Expression->Outputs[Index], Name, Mask, MaskR, MaskG, MaskB,
                MaskA);
    };

    if (Expression->GetClass() ==
        UMaterialExpressionTextureSampleParameter2D::StaticClass())
    {
        return Expression->Outputs.Num() == 6 &&
            OutputMatches(0, TEXT("RGB"), 1, 1, 1, 1, 0) &&
            OutputMatches(1, TEXT("R"), 1, 1, 0, 0, 0) &&
            OutputMatches(2, TEXT("G"), 1, 0, 1, 0, 0) &&
            OutputMatches(3, TEXT("B"), 1, 0, 0, 1, 0) &&
            OutputMatches(4, TEXT("A"), 1, 0, 0, 0, 1) &&
            OutputMatches(5, TEXT("RGBA"), 1, 1, 1, 1, 1);
    }
    if (Expression->GetClass() ==
        UMaterialExpressionVectorParameter::StaticClass())
    {
        return Expression->Outputs.Num() == 5 &&
            OutputMatches(0, TEXT(""), 1, 1, 1, 1, 0) &&
            OutputMatches(1, TEXT(""), 1, 1, 0, 0, 0) &&
            OutputMatches(2, TEXT(""), 1, 0, 1, 0, 0) &&
            OutputMatches(3, TEXT(""), 1, 0, 0, 1, 0) &&
            OutputMatches(4, TEXT(""), 1, 0, 0, 0, 1);
    }
    if (Expression->GetClass() ==
        UMaterialExpressionConstant3Vector::StaticClass())
    {
        return Expression->Outputs.Num() == 4 &&
            OutputMatches(0, TEXT(""), 1, 1, 1, 1, 0) &&
            OutputMatches(1, TEXT(""), 1, 1, 0, 0, 0) &&
            OutputMatches(2, TEXT(""), 1, 0, 1, 0, 0) &&
            OutputMatches(3, TEXT(""), 1, 0, 0, 1, 0);
    }
    if (Expression->GetClass() == UMaterialExpressionWorldPosition::StaticClass())
    {
        return Expression->Outputs.Num() == 3 &&
            OutputMatches(0, TEXT("XYZ"), 1, 1, 1, 1, 0) &&
            OutputMatches(1, TEXT("XY"), 1, 1, 1, 0, 0) &&
            OutputMatches(2, TEXT("Z"), 1, 0, 0, 1, 0);
    }

    const UClass* ExpressionClass = Expression->GetClass();
    const bool bExactSingleOutputClass =
        ExpressionClass == UMaterialExpressionScalarParameter::StaticClass() ||
        ExpressionClass == UMaterialExpressionTextureCoordinate::StaticClass() ||
        ExpressionClass == UMaterialExpressionVertexTangentWS::StaticClass() ||
        ExpressionClass == UMaterialExpressionCameraPositionWS::StaticClass() ||
        ExpressionClass == UMaterialExpressionMultiply::StaticClass() ||
        ExpressionClass == UMaterialExpressionLinearInterpolate::StaticClass() ||
        ExpressionClass == UMaterialExpressionCustom::StaticClass() ||
        ExpressionClass == UMaterialExpressionComponentMask::StaticClass() ||
        ExpressionClass == UMaterialExpressionConstant::StaticClass();
    return bExactSingleOutputClass && Expression->Outputs.Num() == 1 &&
        OutputMatches(0, TEXT(""), 0, 0, 0, 0, 0);
}

UMaterial* CreateMaster(
    IAssetTools& AssetTools,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    UTexture2D* DefaultBase = FindTexture(
        Textures, TEXT("Plaster"), ETextureUsage::BaseColor);
    UTexture2D* DefaultNormal = FindTexture(
        Textures, TEXT("Plaster"), ETextureUsage::Normal);
    UTexture2D* DefaultOrm = FindTexture(
        Textures, TEXT("Plaster"), ETextureUsage::PackedOrm);
    UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
    UMaterial* Material = Factory
        ? Cast<UMaterial>(AssetTools.CreateAsset(
              MasterName,
              MaterialRoot,
              UMaterial::StaticClass(),
              Factory,
              FName(TEXT("TRIAD.CreateIstanaExploreV5DR31BroadShell"))))
        : nullptr;
    UMaterialEditorOnlyData* EditorOnly = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !EditorOnly || !DefaultBase || !DefaultNormal || !DefaultOrm ||
        !EditorOnly->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("Could not allocate the isolated R31 broad-shell master or its admitted defaults.");
        return nullptr;
    }

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
    Material->bEnableExecWire = false;
    // UE 5.5 interprets zero here as an unlimited sentinel. R31's no-
    // displacement invariant is instead proven by disconnected, nonconstant
    // WPO/displacement/PDO roots and by the exact expression-class roster.
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
#if WITH_EDITORONLY_DATA
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
#endif
    Material->bUsedWithNanite = true;
    EditorOnly->ExpressionCollection.EditorComments.Reset();
    EditorOnly->ExpressionCollection.ExpressionExecBegin = nullptr;
    EditorOnly->ExpressionCollection.ExpressionExecEnd = nullptr;
    EditorOnly->BaseColor.UseConstant = false;
    EditorOnly->Normal.UseConstant = false;
    EditorOnly->Roughness.UseConstant = false;
    EditorOnly->Anisotropy.UseConstant = false;
    EditorOnly->Metallic.UseConstant = false;
    EditorOnly->Specular.UseConstant = false;
    EditorOnly->AmbientOcclusion.UseConstant = false;
    EditorOnly->Anisotropy.Expression = nullptr;
    EditorOnly->Tangent.UseConstant = false;
    EditorOnly->Tangent.Expression = nullptr;
    EditorOnly->EmissiveColor.UseConstant = false;
    EditorOnly->EmissiveColor.Expression = nullptr;
    EditorOnly->Opacity.UseConstant = false;
    EditorOnly->Opacity.Expression = nullptr;
    EditorOnly->OpacityMask.UseConstant = false;
    EditorOnly->OpacityMask.Expression = nullptr;
    EditorOnly->WorldPositionOffset.UseConstant = false;
    EditorOnly->WorldPositionOffset.Expression = nullptr;
    EditorOnly->Displacement.UseConstant = false;
    EditorOnly->Displacement.Expression = nullptr;
    EditorOnly->SubsurfaceColor.UseConstant = false;
    EditorOnly->SubsurfaceColor.Expression = nullptr;
    EditorOnly->ClearCoat.UseConstant = false;
    EditorOnly->ClearCoat.Expression = nullptr;
    EditorOnly->ClearCoatRoughness.UseConstant = false;
    EditorOnly->ClearCoatRoughness.Expression = nullptr;
    EditorOnly->Refraction.UseConstant = false;
    EditorOnly->Refraction.Expression = nullptr;
    EditorOnly->PixelDepthOffset.UseConstant = false;
    EditorOnly->PixelDepthOffset.Expression = nullptr;
    EditorOnly->SurfaceThickness.UseConstant = false;
    EditorOnly->SurfaceThickness.Expression = nullptr;
    EditorOnly->MaterialAttributes.Expression = nullptr;
    EditorOnly->MaterialAttributes.PropertyConnectedMask = 0;
    EditorOnly->ShadingModelFromMaterialExpression.Expression = nullptr;
    EditorOnly->FrontMaterial.Expression = nullptr;
    for (FVector2MaterialInput& CustomizedUv : EditorOnly->CustomizedUVs)
    {
        CustomizedUv.UseConstant = false;
    }

    UMaterialExpressionTextureCoordinate* Uv0 =
        AddExpression<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("R31.UV0.SourceFacadeMetres"), -1800, -520);
    UMaterialExpressionScalarParameter* UvScale = AddScalar(
        Material, TEXT("R31.UvScale"), UvScaleParameter, 0.5f,
        -1800, -390);
    UMaterialExpressionMultiply* ScaledUv =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R31.ScaleSourceMetreUV"), -1550, -500);
    UMaterialExpressionTextureSampleParameter2D* Base = AddTextureParameter(
        Material, TEXT("R31.BaseColorTexture"), BaseColorTextureParameter,
        DefaultBase, SAMPLERTYPE_Color, -1300, -720);
    UMaterialExpressionTextureSampleParameter2D* Normal = AddTextureParameter(
        Material, TEXT("R31.NormalTexture"), NormalTextureParameter,
        DefaultNormal, SAMPLERTYPE_Normal, -1300, -390);
    UMaterialExpressionTextureSampleParameter2D* Orm = AddTextureParameter(
        Material, TEXT("R31.PackedORMTexture"), PackedOrmTextureParameter,
        DefaultOrm, SAMPLERTYPE_Masks, -1300, -50);
    UMaterialExpressionVectorParameter* Tint = AddVector(
        Material, TEXT("R31.Tint"), TintParameter, FLinearColor::White,
        -1010, -800);
    UMaterialExpressionMultiply* TexturedTint =
        AddExpression<UMaterialExpressionMultiply>(
            Material, TEXT("R31.TexturedTint"), -760, -690);
    UMaterialExpressionScalarParameter* TextureInfluence = AddScalar(
        Material, TEXT("R31.TextureInfluence"), TextureInfluenceParameter,
        0.75f, -760, -840);
    UMaterialExpressionLinearInterpolate* BaseBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendTintAndTexture"), -500, -680);
    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("R31.WorldPositionCentimetres"), -500, -520);
    UMaterialExpressionVertexTangentWS* VertexTangent =
        AddExpression<UMaterialExpressionVertexTangentWS>(
            Material, TEXT("R31.VertexTangentWS"), -500, -470);
    UMaterialExpressionCameraPositionWS* CameraPosition =
        AddExpression<UMaterialExpressionCameraPositionWS>(
            Material, TEXT("R31.CameraPositionCentimetres"), -500, -400);

    UMaterialExpressionScalarParameter* NormalStrength = AddScalar(
        Material, TEXT("R31.NormalStrength"), NormalStrengthParameter,
        0.5f, -760, -230);
    UMaterialExpressionScalarParameter* SurfaceRoughness = AddScalar(
        Material, TEXT("R31.SurfaceRoughness"), SurfaceRoughnessParameter,
        0.72f, -500, 20);
    UMaterialExpressionScalarParameter* RoughnessTextureWeight = AddScalar(
        Material, TEXT("R31.RoughnessTextureWeight"),
        RoughnessTextureWeightParameter, 0.7f, -500, 120);
    UMaterialExpressionScalarParameter* Metallic = AddScalar(
        Material, TEXT("R31.Metallic"), MetallicParameter,
        0.0f, -40, 340);
    UMaterialExpressionScalarParameter* Specular = AddScalar(
        Material, TEXT("R31.Specular"), SpecularParameter,
        0.28f, -40, 450);
    UMaterialExpressionScalarParameter* AoTextureWeight = AddScalar(
        Material, TEXT("R31.AoTextureWeight"), AoTextureWeightParameter,
        0.7f, -500, 600);
    UMaterialExpressionScalarParameter* VariationCellMeters = AddScalar(
        Material, TEXT("R31.VariationCellMeters"), VariationCellMetersParameter,
        48.0f, -500, 230);
    UMaterialExpressionScalarParameter* WeatheringStrength = AddScalar(
        Material, TEXT("R31.WeatheringStrength"), WeatheringStrengthParameter,
        0.25f, -500, 330);
    UMaterialExpressionScalarParameter* WallVerticalWeatherMask = AddScalar(
        Material, TEXT("R31.WallVerticalWeatherMask"),
        WallVerticalWeatherMaskParameter, 1.0f, -500, 380);
    UMaterialExpressionCustom* SeamSafeMetricUv =
        AddExpression<UMaterialExpressionCustom>(
            Material, TEXT("R31.SeamSafeMetricTextureUV"),
            -1550, -610);
    UMaterialExpressionScalarParameter* BayMeters = AddScalar(
        Material, TEXT("R31.BayMeters"), BayMetersParameter,
        3.1f, -500, 430);
    UMaterialExpressionScalarParameter* StoreyMeters = AddScalar(
        Material, TEXT("R31.StoreyMeters"), StoreyMetersParameter,
        3.2f, -500, 530);
    UMaterialExpressionScalarParameter* ApertureWidth = AddScalar(
        Material, TEXT("R31.ApertureWidthFraction"),
        ApertureWidthFractionParameter, 0.61f, -250, 20);
    UMaterialExpressionScalarParameter* ApertureHeight = AddScalar(
        Material, TEXT("R31.ApertureHeightFraction"),
        ApertureHeightFractionParameter, 0.49f, -250, 120);
    UMaterialExpressionScalarParameter* ApertureSill = AddScalar(
        Material, TEXT("R31.ApertureSillFraction"),
        ApertureSillFractionParameter, 0.20f, -250, 220);
    UMaterialExpressionScalarParameter* ApertureStrength = AddScalar(
        Material, TEXT("R31.ApertureHintStrength"),
        ApertureHintStrengthParameter, 0.7f, -250, 320);
    UMaterialExpressionScalarParameter* ApertureFadeStart = AddScalar(
        Material, TEXT("R31.ApertureFadeStartCm"),
        ApertureFadeStartCmParameter, 55000.0f, -250, 420);
    UMaterialExpressionScalarParameter* ApertureFadeEnd = AddScalar(
        Material, TEXT("R31.ApertureFadeEndCm"),
        ApertureFadeEndCmParameter, 100000.0f, -250, 520);
    UMaterialExpressionScalarParameter* AtmosphereStart = AddScalar(
        Material, TEXT("R31.AtmosphereStartCm"),
        AtmosphereStartCmParameter, 60000.0f, -250, 570);
    UMaterialExpressionScalarParameter* AtmosphereEnd = AddScalar(
        Material, TEXT("R31.AtmosphereEndCm"),
        AtmosphereEndCmParameter, 130000.0f, -250, 620);
    UMaterialExpressionScalarParameter* AtmosphereStrength = AddScalar(
        Material, TEXT("R31.AtmosphereStrength"),
        AtmosphereStrengthParameter, 0.22f, -250, 670);
    UMaterialExpressionVectorParameter* ApertureTint = AddVector(
        Material, TEXT("R31.ApertureTint"), ApertureTintParameter,
        FLinearColor(0.02f, 0.05f, 0.074f), -250, 740);
    UMaterialExpressionVectorParameter* AtmosphereTint = AddVector(
        Material, TEXT("R31.AtmosphereTint"), AtmosphereTintParameter,
        FLinearColor(0.48f, 0.54f, 0.55f), -250, 820);

    UMaterialExpressionCustom* Surface =
        AddExpression<UMaterialExpressionCustom>(
            Material, TEXT("R31.TexturedWeatheredR25DepthSurface"),
            80, -360);
    UMaterialExpressionComponentMask* RoughnessOutput =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("R31.SurfaceRoughnessA"), 350, 10);
    UMaterialExpressionConstant3Vector* FlatNormal =
        AddExpression<UMaterialExpressionConstant3Vector>(
            Material, TEXT("R31.FlatNormal"), -500, -250);
    UMaterialExpressionLinearInterpolate* NormalBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendFlatAndTextureNormal"), -40, -180);
    UMaterialExpressionConstant* AoOne =
        AddExpression<UMaterialExpressionConstant>(
            Material, TEXT("R31.AmbientOcclusionOne"), -500, 710);
    UMaterialExpressionLinearInterpolate* AoBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("R31.BlendAmbientOcclusion"), -40, 650);

    const TArray<UMaterialExpression*> Required = {
        Uv0, UvScale, SeamSafeMetricUv, ScaledUv, Base, Normal, Orm, Tint, TexturedTint,
        TextureInfluence, BaseBlend, WorldPosition, CameraPosition,
        VertexTangent,
        NormalStrength, SurfaceRoughness, RoughnessTextureWeight, Metallic,
        Specular, AoTextureWeight, VariationCellMeters, WeatheringStrength,
        WallVerticalWeatherMask,
        BayMeters, StoreyMeters, ApertureWidth, ApertureHeight, ApertureSill,
        ApertureStrength, ApertureFadeStart, ApertureFadeEnd, ApertureTint,
        AtmosphereStart, AtmosphereEnd, AtmosphereStrength, AtmosphereTint,
        Surface, RoughnessOutput, FlatNormal, NormalBlend, AoOne, AoBlend};
    if (Required.Contains(nullptr) ||
        Required.Num() != ExpectedMasterExpressionCount)
    {
        OutError = TEXT("Could not allocate the exact 42-node R31 broad-shell graph.");
        return nullptr;
    }

    Uv0->CoordinateIndex = 0;
    Uv0->UTiling = 1.0f;
    Uv0->VTiling = 1.0f;
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    ScaledUv->ConstA = 0.0f;
    ScaledUv->ConstB = 1.0f;
    TexturedTint->ConstA = 0.0f;
    TexturedTint->ConstB = 1.0f;
    BaseBlend->ConstA = 0.0f;
    BaseBlend->ConstB = 1.0f;
    BaseBlend->ConstAlpha = 0.5f;
    FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
    NormalBlend->ConstA = 0.0f;
    NormalBlend->ConstB = 1.0f;
    NormalBlend->ConstAlpha = 0.5f;
    AoOne->R = 1.0f;
    AoBlend->ConstA = 0.0f;
    AoBlend->ConstB = 1.0f;
    AoBlend->ConstAlpha = 0.5f;
    RoughnessOutput->R = false;
    RoughnessOutput->G = false;
    RoughnessOutput->B = false;
    RoughnessOutput->A = true;

    SeamSafeMetricUv->Description = SeamSafeMetricUvDescription;
    SeamSafeMetricUv->Code = SeamSafeMetricUvCode;
    SeamSafeMetricUv->OutputType = CMOT_Float2;
    const TCHAR* SeamSafeUvInputNames[] = {
        TEXT("UV0"), TEXT("WorldPositionCm"), TEXT("VertexTangentWS"),
        TEXT("WallRoleMask")};
    const TArray<UMaterialExpression*> SeamSafeUvInputs = {
        Uv0, WorldPosition, VertexTangent, WallVerticalWeatherMask};
    SeamSafeMetricUv->Inputs.Reset(SeamSafeUvInputs.Num());
    for (int32 Index = 0; Index < SeamSafeUvInputs.Num(); ++Index)
    {
        FCustomInput& Input = SeamSafeMetricUv->Inputs.AddDefaulted_GetRef();
        Input.InputName = SeamSafeUvInputNames[Index];
        Input.Input.Connect(0, SeamSafeUvInputs[Index]);
    }

    if (!Connect(SeamSafeMetricUv, TEXT(""), ScaledUv, TEXT("A"), TEXT("seam-safe metric UV to source-metre scale"), OutError) ||
        !Connect(UvScale, TEXT(""), ScaledUv, TEXT("B"), TEXT("reciprocal metres-per-tile"), OutError) ||
        !Connect(ScaledUv, TEXT(""), Base, TEXT("UVs"), TEXT("metric UV to base"), OutError) ||
        !Connect(ScaledUv, TEXT(""), Normal, TEXT("UVs"), TEXT("metric UV to normal"), OutError) ||
        !Connect(ScaledUv, TEXT(""), Orm, TEXT("UVs"), TEXT("metric UV to ORM"), OutError) ||
        !Connect(Base, TEXT("RGB"), TexturedTint, TEXT("A"), TEXT("base texture to tint"), OutError) ||
        !Connect(Tint, TEXT(""), TexturedTint, TEXT("B"), TEXT("tint multiply"), OutError) ||
        !Connect(Tint, TEXT(""), BaseBlend, TEXT("A"), TEXT("plain tint branch"), OutError) ||
        !Connect(TexturedTint, TEXT(""), BaseBlend, TEXT("B"), TEXT("textured tint branch"), OutError) ||
        !Connect(TextureInfluence, TEXT(""), BaseBlend, TEXT("Alpha"), TEXT("texture influence"), OutError) ||
        !Connect(FlatNormal, TEXT(""), NormalBlend, TEXT("A"), TEXT("flat normal"), OutError) ||
        !Connect(Normal, TEXT("RGB"), NormalBlend, TEXT("B"), TEXT("texture normal"), OutError) ||
        !Connect(NormalStrength, TEXT(""), NormalBlend, TEXT("Alpha"), TEXT("normal strength"), OutError) ||
        !Connect(AoOne, TEXT(""), AoBlend, TEXT("A"), TEXT("neutral AO"), OutError) ||
        !Connect(Orm, TEXT("R"), AoBlend, TEXT("B"), TEXT("packed AO"), OutError) ||
        !Connect(AoTextureWeight, TEXT(""), AoBlend, TEXT("Alpha"), TEXT("AO weight"), OutError))
    {
        return nullptr;
    }

    Surface->Description = SurfaceResponseDescription;
    Surface->Desc = TEXT("R31.TexturedWeatheredR25DepthSurface");
    Surface->Code = SurfaceResponseCode;
    Surface->OutputType = CMOT_Float4;
    const TCHAR* InputNames[] = {
        TEXT("BaseSurface"), TEXT("PackedOrm"), TEXT("UV0"),
        TEXT("WorldPositionCm"), TEXT("CameraPositionCm"),
        TEXT("SurfaceRoughness"), TEXT("RoughnessTextureWeight"),
        TEXT("VariationCellMeters"), TEXT("WeatheringStrength"),
        TEXT("WallVerticalWeatherMask"),
        TEXT("BayMeters"), TEXT("StoreyMeters"),
        TEXT("ApertureWidthFraction"), TEXT("ApertureHeightFraction"),
        TEXT("ApertureSillFraction"), TEXT("ApertureHintStrength"),
        TEXT("ApertureFadeStartCm"), TEXT("ApertureFadeEndCm"),
        TEXT("ApertureTint"), TEXT("AtmosphereStartCm"),
        TEXT("AtmosphereEndCm"), TEXT("AtmosphereStrength"),
        TEXT("AtmosphereTint")};
    const TArray<UMaterialExpression*> Inputs = {
        BaseBlend, Orm, SeamSafeMetricUv, WorldPosition, CameraPosition,
        SurfaceRoughness, RoughnessTextureWeight, VariationCellMeters,
        WeatheringStrength, WallVerticalWeatherMask, BayMeters, StoreyMeters, ApertureWidth,
        ApertureHeight, ApertureSill, ApertureStrength,
        ApertureFadeStart, ApertureFadeEnd, ApertureTint,
        AtmosphereStart, AtmosphereEnd, AtmosphereStrength, AtmosphereTint};
    Surface->Inputs.Reset(Inputs.Num());
    for (int32 Index = 0; Index < Inputs.Num(); ++Index)
    {
        FCustomInput& Input = Surface->Inputs.AddDefaulted_GetRef();
        Input.InputName = InputNames[Index];
        Input.Input.Connect(0, Inputs[Index]);
    }
    RoughnessOutput->Input.Connect(0, Surface);

    if (!ConnectProperty(Surface, TEXT(""), MP_BaseColor, TEXT("Base Color"), OutError) ||
        !ConnectProperty(NormalBlend, TEXT(""), MP_Normal, TEXT("Normal"), OutError) ||
        !ConnectProperty(RoughnessOutput, TEXT(""), MP_Roughness, TEXT("Roughness"), OutError) ||
        !ConnectProperty(Metallic, TEXT(""), MP_Metallic, TEXT("Metallic"), OutError) ||
        !ConnectProperty(Specular, TEXT(""), MP_Specular, TEXT("Specular"), OutError) ||
        !ConnectProperty(AoBlend, TEXT(""), MP_AmbientOcclusion, TEXT("Ambient Occlusion"), OutError))
    {
        return nullptr;
    }
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    OutError.Reset();
    return Material;
}

void GatherSpecScalars(
    const FMaterialSpec& Spec,
    TArray<TPair<FName, float>>& OutValues)
{
    OutValues = {
        {UvScaleParameter, 1.0f / Spec.MetresPerTile},
        {TextureInfluenceParameter, Spec.TextureInfluence},
        {NormalStrengthParameter, Spec.NormalStrength},
        {SurfaceRoughnessParameter, Spec.SurfaceRoughness},
        {RoughnessTextureWeightParameter, Spec.RoughnessTextureWeight},
        {MetallicParameter, Spec.Metallic},
        {SpecularParameter, Spec.Specular},
        {AoTextureWeightParameter, Spec.AoTextureWeight},
        {VariationCellMetersParameter, Spec.VariationCellMeters},
        {WeatheringStrengthParameter, Spec.WeatheringStrength},
        {WallVerticalWeatherMaskParameter, Spec.WallVerticalWeatherMask},
        {BayMetersParameter, Spec.BayMeters},
        {StoreyMetersParameter, Spec.StoreyMeters},
        {ApertureWidthFractionParameter, Spec.ApertureWidthFraction},
        {ApertureHeightFractionParameter, Spec.ApertureHeightFraction},
        {ApertureSillFractionParameter, Spec.ApertureSillFraction},
        {ApertureHintStrengthParameter, Spec.ApertureHintStrength},
        {ApertureFadeStartCmParameter, Spec.ApertureFadeStartCm},
        {ApertureFadeEndCmParameter, Spec.ApertureFadeEndCm},
        {AtmosphereStartCmParameter, Spec.AtmosphereStartCm},
        {AtmosphereEndCmParameter, Spec.AtmosphereEndCm},
        {AtmosphereStrengthParameter, Spec.AtmosphereStrength}};
}

UMaterialInstanceConstant* CreateInstance(
    IAssetTools& AssetTools,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    const TMap<FName, UTexture2D*>& Textures,
    FString& OutError)
{
    if (!Parent || Parent->GetPathName() != MasterObjectPath)
    {
        OutError = TEXT("R31 broad-shell master is absent: ") + MasterObjectPath;
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
              FName(TEXT("TRIAD.CreateIstanaExploreV5DR31BroadShell"))))
        : nullptr;
    if (!Instance)
    {
        OutError = FString::Printf(
            TEXT("Could not create exact R31 broad-shell material '%s'."),
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
    GatherSpecScalars(Spec, Scalars);
    for (const TPair<FName, float>& Pair : Scalars)
    {
        Instance->SetScalarParameterValueEditorOnly(
            FMaterialParameterInfo(Pair.Key), Pair.Value);
    }
    Instance->SetVectorParameterValueEditorOnly(
        FMaterialParameterInfo(TintParameter), Spec.Tint);
    Instance->SetVectorParameterValueEditorOnly(
        FMaterialParameterInfo(ApertureTintParameter), Spec.ApertureTint);
    Instance->SetVectorParameterValueEditorOnly(
        FMaterialParameterInfo(AtmosphereTintParameter), Spec.AtmosphereTint);
    Instance->SetTextureParameterValueEditorOnly(
        FMaterialParameterInfo(BaseColorTextureParameter),
        FindTexture(Textures, Spec.TextureSet, ETextureUsage::BaseColor));
    Instance->SetTextureParameterValueEditorOnly(
        FMaterialParameterInfo(NormalTextureParameter),
        FindTexture(Textures, Spec.TextureSet, ETextureUsage::Normal));
    Instance->SetTextureParameterValueEditorOnly(
        FMaterialParameterInfo(PackedOrmTextureParameter),
        FindTexture(Textures, Spec.TextureSet, ETextureUsage::PackedOrm));
    Instance->PostEditChange();
    Instance->EnsureIsComplete();
    Instance->MarkPackageDirty();
    OutError.Reset();
    return Instance;
}

bool ValidateCompiledMaterial(UMaterialInterface* Material, FString& OutError)
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
    const TArray<FString> Errors = Resource
        ? Resource->GetCompileErrors()
        : TArray<FString>{TEXT("resource-null")};
    if (!Material || !Resource || !ShaderMap ||
        !Resource->IsCompilationFinished() ||
        !Resource->IsGameThreadShaderMapComplete() ||
        !ShaderMap->IsCompilationFinalized() ||
        !ShaderMap->CompiledSuccessfully() ||
        !ShaderMap->IsValidForRendering() || Resource->IsDefaultMaterial() ||
        !Errors.IsEmpty())
    {
        OutError = FString::Printf(
            TEXT("R31 broad-shell material '%s' has no valid compiled active-feature-level resource (%d): errors=[%s]."),
            Material ? *Material->GetPathName() : TEXT("<null>"),
            static_cast<int32>(FeatureLevel),
            *FString::Join(Errors, TEXT(" | ")));
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateMaster(
    UMaterial* Material,
    bool bRequireSaved,
    FString& OutError)
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
        Material->bEnableExecWire ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        EditorOnly->ExpressionCollection.Expressions.Num() !=
            ExpectedMasterExpressionCount ||
        !EditorOnly->ExpressionCollection.EditorComments.IsEmpty() ||
        EditorOnly->ExpressionCollection.ExpressionExecBegin ||
        EditorOnly->ExpressionCollection.ExpressionExecEnd ||
        !EditorOnly->BaseColor.Expression ||
        !EditorOnly->Normal.Expression ||
        !EditorOnly->Roughness.Expression ||
        !EditorOnly->Metallic.Expression ||
        !EditorOnly->Specular.Expression ||
        !EditorOnly->AmbientOcclusion.Expression ||
        EditorOnly->BaseColor.UseConstant ||
        EditorOnly->Normal.UseConstant ||
        EditorOnly->Roughness.UseConstant ||
        EditorOnly->Metallic.UseConstant ||
        EditorOnly->Specular.UseConstant ||
        EditorOnly->AmbientOcclusion.UseConstant ||
        !InputIsExactlyDisconnected(EditorOnly->Anisotropy) ||
        !InputIsExactlyDisconnected(EditorOnly->Tangent) ||
        !InputIsExactlyDisconnected(EditorOnly->EmissiveColor) ||
        !InputIsExactlyDisconnected(EditorOnly->Opacity) ||
        !InputIsExactlyDisconnected(EditorOnly->OpacityMask) ||
        !InputIsExactlyDisconnected(EditorOnly->WorldPositionOffset) ||
        !InputIsExactlyDisconnected(EditorOnly->Displacement) ||
        !InputIsExactlyDisconnected(EditorOnly->SubsurfaceColor) ||
        !InputIsExactlyDisconnected(EditorOnly->ClearCoat) ||
        !InputIsExactlyDisconnected(EditorOnly->ClearCoatRoughness) ||
        !InputIsExactlyDisconnected(EditorOnly->Refraction) ||
        !InputIsExactlyDisconnected(EditorOnly->MaterialAttributes) ||
        EditorOnly->MaterialAttributes.PropertyConnectedMask != 0 ||
        !InputIsExactlyDisconnected(EditorOnly->PixelDepthOffset) ||
        !InputIsExactlyDisconnected(
            EditorOnly->ShadingModelFromMaterialExpression) ||
        !InputIsExactlyDisconnected(EditorOnly->SurfaceThickness) ||
        !InputIsExactlyDisconnected(EditorOnly->FrontMaterial) ||
        EditorOnly->Anisotropy.UseConstant ||
        EditorOnly->Tangent.UseConstant ||
        EditorOnly->EmissiveColor.UseConstant ||
        EditorOnly->Opacity.UseConstant ||
        EditorOnly->OpacityMask.UseConstant ||
        EditorOnly->WorldPositionOffset.UseConstant ||
        EditorOnly->Displacement.UseConstant ||
        EditorOnly->SubsurfaceColor.UseConstant ||
        EditorOnly->ClearCoat.UseConstant ||
        EditorOnly->ClearCoatRoughness.UseConstant ||
        EditorOnly->Refraction.UseConstant ||
        EditorOnly->PixelDepthOffset.UseConstant ||
        EditorOnly->SurfaceThickness.UseConstant ||
        UE_ARRAY_COUNT(EditorOnly->CustomizedUVs) != 8 ||
        (bRequireSaved &&
         (!Material->GetOutermost() || Material->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(Material->GetOutermost()->GetName()))))
    {
        OutError = TEXT("R31 broad-shell master lost its exact opaque, DefaultLit, one-sided, Nanite, displacement-free contract.");
        return false;
    }
    for (const FVector2MaterialInput& CustomizedUv : EditorOnly->CustomizedUVs)
    {
        if (!InputIsExactlyDisconnected(CustomizedUv) ||
            CustomizedUv.UseConstant)
        {
            OutError = TEXT("R31 broad-shell master has a connected or constant customized UV channel.");
            return false;
        }
    }

    TSet<FName> TextureNames;
    TSet<FName> ScalarNames;
    TSet<FName> VectorNames;
    TMap<FName, UMaterialExpressionTextureSampleParameter2D*>
        TextureExpressions;
    TMap<FName, UMaterialExpressionScalarParameter*> ScalarExpressions;
    TMap<FName, UMaterialExpressionVectorParameter*> VectorExpressions;
    TMap<FString, UMaterialExpression*> Nodes;
    TSet<FString> ActualNodeDescriptions;
    UMaterialExpressionCustom* Surface = nullptr;
    UMaterialExpressionCustom* SeamSafeMetricUv = nullptr;
    UMaterialExpression* BaseBlendExpression = nullptr;
    UMaterialExpression* UvExpression = nullptr;
    UMaterialExpression* WorldPositionExpression = nullptr;
    UMaterialExpression* VertexTangentExpression = nullptr;
    UMaterialExpression* CameraPositionExpression = nullptr;
    int32 UvCount = 0;
    int32 WorldPositionCount = 0;
    int32 VertexTangentCount = 0;
    int32 CameraPositionCount = 0;
    int32 CustomCount = 0;
    int32 ComponentMaskCount = 0;
    int32 Constant3Count = 0;
    int32 ConstantCount = 0;
    int32 LerpCount = 0;
    int32 MultiplyCount = 0;
    for (UMaterialExpression* Expression :
         EditorOnly->ExpressionCollection.Expressions)
    {
        if (!Expression)
        {
            OutError = TEXT("R31 broad-shell master contains a null expression.");
            return false;
        }
        if (Expression->GetOuter() != Material || Expression->Material != Material ||
            Expression->Function || Expression->SubgraphExpression ||
            Expression->Desc.IsEmpty() ||
            !OutputsMatchExplicitR31Contract(Expression) ||
            Nodes.Contains(Expression->Desc))
        {
            OutError = TEXT("R31 broad-shell master has invalid expression ownership, an empty/duplicate description, or a mutated explicit output descriptor.");
            return false;
        }
        Nodes.Add(Expression->Desc, Expression);
        ActualNodeDescriptions.Add(Expression->Desc);
        if (Expression->GetClass() ==
            UMaterialExpressionTextureSampleParameter2D::StaticClass())
        {
            UMaterialExpressionTextureSampleParameter2D* Texture =
                CastChecked<UMaterialExpressionTextureSampleParameter2D>(Expression);
            if (TextureNames.Contains(Texture->ParameterName) ||
                !Texture->Texture ||
                !Texture->Texture->GetPathName().StartsWith(
                    PublicViewTextureRoot + TEXT("/")))
            {
                OutError = TEXT("R31 broad-shell master has a duplicate or forbidden texture parameter.");
                return false;
            }
            TextureNames.Add(Texture->ParameterName);
            TextureExpressions.Add(Texture->ParameterName, Texture);
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionScalarParameter::StaticClass())
        {
            UMaterialExpressionScalarParameter* Scalar =
                CastChecked<UMaterialExpressionScalarParameter>(Expression);
            if (ScalarNames.Contains(Scalar->ParameterName))
            {
                OutError = TEXT("R31 broad-shell master has a duplicate scalar parameter.");
                return false;
            }
            ScalarNames.Add(Scalar->ParameterName);
            ScalarExpressions.Add(Scalar->ParameterName, Scalar);
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionVectorParameter::StaticClass())
        {
            UMaterialExpressionVectorParameter* Vector =
                CastChecked<UMaterialExpressionVectorParameter>(Expression);
            if (VectorNames.Contains(Vector->ParameterName))
            {
                OutError = TEXT("R31 broad-shell master has a duplicate vector parameter.");
                return false;
            }
            VectorNames.Add(Vector->ParameterName);
            VectorExpressions.Add(Vector->ParameterName, Vector);
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionTextureCoordinate::StaticClass())
        {
            ++UvCount;
            UvExpression = Expression;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionWorldPosition::StaticClass())
        {
            ++WorldPositionCount;
            WorldPositionExpression = Expression;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionVertexTangentWS::StaticClass())
        {
            ++VertexTangentCount;
            VertexTangentExpression = Expression;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionCameraPositionWS::StaticClass())
        {
            ++CameraPositionCount;
            CameraPositionExpression = Expression;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionCustom::StaticClass())
        {
            UMaterialExpressionCustom* Custom =
                CastChecked<UMaterialExpressionCustom>(Expression);
            ++CustomCount;
            if (Custom->Desc == TEXT("R31.TexturedWeatheredR25DepthSurface"))
            {
                Surface = Custom;
            }
            else if (Custom->Desc == TEXT("R31.SeamSafeMetricTextureUV"))
            {
                SeamSafeMetricUv = Custom;
            }
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionComponentMask::StaticClass())
        {
            ++ComponentMaskCount;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionConstant3Vector::StaticClass())
        {
            ++Constant3Count;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionConstant::StaticClass())
        {
            ++ConstantCount;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionLinearInterpolate::StaticClass())
        {
            ++LerpCount;
        }
        else if (Expression->GetClass() ==
                 UMaterialExpressionMultiply::StaticClass())
        {
            ++MultiplyCount;
        }
        else
        {
            OutError = TEXT("R31 broad-shell master contains forbidden expression class: ") +
                Expression->GetClass()->GetName();
            return false;
        }
    }
    BaseBlendExpression = Nodes.FindRef(TEXT("R31.BlendTintAndTexture"));
    const TSet<FName> ExpectedTextures = {
        BaseColorTextureParameter, NormalTextureParameter,
        PackedOrmTextureParameter};
    const TSet<FName> ExpectedScalars = {
        UvScaleParameter, TextureInfluenceParameter, NormalStrengthParameter,
        SurfaceRoughnessParameter, RoughnessTextureWeightParameter,
        MetallicParameter, SpecularParameter, AoTextureWeightParameter,
        VariationCellMetersParameter, WeatheringStrengthParameter,
        WallVerticalWeatherMaskParameter,
        BayMetersParameter, StoreyMetersParameter,
        ApertureWidthFractionParameter, ApertureHeightFractionParameter,
        ApertureSillFractionParameter, ApertureHintStrengthParameter,
        ApertureFadeStartCmParameter, ApertureFadeEndCmParameter,
        AtmosphereStartCmParameter, AtmosphereEndCmParameter,
        AtmosphereStrengthParameter};
    const TSet<FName> ExpectedVectors = {
        TintParameter, ApertureTintParameter, AtmosphereTintParameter};
    const TSet<FString> ExpectedNodeDescriptions = {
        TEXT("R31.UV0.SourceFacadeMetres"),
        TEXT("R31.UvScale"),
        TEXT("R31.SeamSafeMetricTextureUV"),
        TEXT("R31.ScaleSourceMetreUV"),
        TEXT("R31.BaseColorTexture"),
        TEXT("R31.NormalTexture"),
        TEXT("R31.PackedORMTexture"),
        TEXT("R31.Tint"),
        TEXT("R31.TexturedTint"),
        TEXT("R31.TextureInfluence"),
        TEXT("R31.BlendTintAndTexture"),
        TEXT("R31.WorldPositionCentimetres"),
        TEXT("R31.VertexTangentWS"),
        TEXT("R31.CameraPositionCentimetres"),
        TEXT("R31.NormalStrength"),
        TEXT("R31.SurfaceRoughness"),
        TEXT("R31.RoughnessTextureWeight"),
        TEXT("R31.Metallic"),
        TEXT("R31.Specular"),
        TEXT("R31.AoTextureWeight"),
        TEXT("R31.VariationCellMeters"),
        TEXT("R31.WeatheringStrength"),
        TEXT("R31.WallVerticalWeatherMask"),
        TEXT("R31.BayMeters"),
        TEXT("R31.StoreyMeters"),
        TEXT("R31.ApertureWidthFraction"),
        TEXT("R31.ApertureHeightFraction"),
        TEXT("R31.ApertureSillFraction"),
        TEXT("R31.ApertureHintStrength"),
        TEXT("R31.ApertureFadeStartCm"),
        TEXT("R31.ApertureFadeEndCm"),
        TEXT("R31.AtmosphereStartCm"),
        TEXT("R31.AtmosphereEndCm"),
        TEXT("R31.AtmosphereStrength"),
        TEXT("R31.ApertureTint"),
        TEXT("R31.AtmosphereTint"),
        TEXT("R31.TexturedWeatheredR25DepthSurface"),
        TEXT("R31.SurfaceRoughnessA"),
        TEXT("R31.FlatNormal"),
        TEXT("R31.BlendFlatAndTextureNormal"),
        TEXT("R31.AmbientOcclusionOne"),
        TEXT("R31.BlendAmbientOcclusion")};
    const TMap<FString, const UClass*> ExpectedNodeClasses = {
        {TEXT("R31.UV0.SourceFacadeMetres"),
         UMaterialExpressionTextureCoordinate::StaticClass()},
        {TEXT("R31.UvScale"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.SeamSafeMetricTextureUV"),
         UMaterialExpressionCustom::StaticClass()},
        {TEXT("R31.ScaleSourceMetreUV"),
         UMaterialExpressionMultiply::StaticClass()},
        {TEXT("R31.BaseColorTexture"),
         UMaterialExpressionTextureSampleParameter2D::StaticClass()},
        {TEXT("R31.NormalTexture"),
         UMaterialExpressionTextureSampleParameter2D::StaticClass()},
        {TEXT("R31.PackedORMTexture"),
         UMaterialExpressionTextureSampleParameter2D::StaticClass()},
        {TEXT("R31.Tint"), UMaterialExpressionVectorParameter::StaticClass()},
        {TEXT("R31.TexturedTint"),
         UMaterialExpressionMultiply::StaticClass()},
        {TEXT("R31.TextureInfluence"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.BlendTintAndTexture"),
         UMaterialExpressionLinearInterpolate::StaticClass()},
        {TEXT("R31.WorldPositionCentimetres"),
         UMaterialExpressionWorldPosition::StaticClass()},
        {TEXT("R31.VertexTangentWS"),
         UMaterialExpressionVertexTangentWS::StaticClass()},
        {TEXT("R31.CameraPositionCentimetres"),
         UMaterialExpressionCameraPositionWS::StaticClass()},
        {TEXT("R31.NormalStrength"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.SurfaceRoughness"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.RoughnessTextureWeight"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.Metallic"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.Specular"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.AoTextureWeight"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.VariationCellMeters"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.WeatheringStrength"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.WallVerticalWeatherMask"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.BayMeters"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.StoreyMeters"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.ApertureWidthFraction"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.ApertureHeightFraction"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.ApertureSillFraction"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.ApertureHintStrength"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.ApertureFadeStartCm"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.ApertureFadeEndCm"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.AtmosphereStartCm"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.AtmosphereEndCm"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.AtmosphereStrength"),
         UMaterialExpressionScalarParameter::StaticClass()},
        {TEXT("R31.ApertureTint"),
         UMaterialExpressionVectorParameter::StaticClass()},
        {TEXT("R31.AtmosphereTint"),
         UMaterialExpressionVectorParameter::StaticClass()},
        {TEXT("R31.TexturedWeatheredR25DepthSurface"),
         UMaterialExpressionCustom::StaticClass()},
        {TEXT("R31.SurfaceRoughnessA"),
         UMaterialExpressionComponentMask::StaticClass()},
        {TEXT("R31.FlatNormal"),
         UMaterialExpressionConstant3Vector::StaticClass()},
        {TEXT("R31.BlendFlatAndTextureNormal"),
         UMaterialExpressionLinearInterpolate::StaticClass()},
        {TEXT("R31.AmbientOcclusionOne"),
         UMaterialExpressionConstant::StaticClass()},
        {TEXT("R31.BlendAmbientOcclusion"),
         UMaterialExpressionLinearInterpolate::StaticClass()}};
    const TMap<FName, float> ExpectedScalarDefaults = {
        {UvScaleParameter, 0.5f},
        {TextureInfluenceParameter, 0.75f},
        {NormalStrengthParameter, 0.5f},
        {SurfaceRoughnessParameter, 0.72f},
        {RoughnessTextureWeightParameter, 0.7f},
        {MetallicParameter, 0.0f},
        {SpecularParameter, 0.28f},
        {AoTextureWeightParameter, 0.7f},
        {VariationCellMetersParameter, 48.0f},
        {WeatheringStrengthParameter, 0.25f},
        {WallVerticalWeatherMaskParameter, 1.0f},
        {BayMetersParameter, 3.1f},
        {StoreyMetersParameter, 3.2f},
        {ApertureWidthFractionParameter, 0.61f},
        {ApertureHeightFractionParameter, 0.49f},
        {ApertureSillFractionParameter, 0.20f},
        {ApertureHintStrengthParameter, 0.7f},
        {ApertureFadeStartCmParameter, 55000.0f},
        {ApertureFadeEndCmParameter, 100000.0f},
        {AtmosphereStartCmParameter, 60000.0f},
        {AtmosphereEndCmParameter, 130000.0f},
        {AtmosphereStrengthParameter, 0.22f}};
    const TMap<FName, FLinearColor> ExpectedVectorDefaults = {
        {TintParameter, FLinearColor::White},
        {ApertureTintParameter, FLinearColor(0.02f, 0.05f, 0.074f)},
        {AtmosphereTintParameter, FLinearColor(0.48f, 0.54f, 0.55f)}};
    if (TextureNames.Num() != ExpectedTextureParameterCount ||
        ScalarNames.Num() != ExpectedScalarParameterCount ||
        VectorNames.Num() != ExpectedVectorParameterCount ||
        !SameSet(TextureNames, ExpectedTextures) ||
        !SameSet(ScalarNames, ExpectedScalars) ||
        !SameSet(VectorNames, ExpectedVectors) ||
        !SameSet(ActualNodeDescriptions, ExpectedNodeDescriptions) ||
        UvCount != 1 || WorldPositionCount != 1 || VertexTangentCount != 1 ||
        CameraPositionCount != 1 ||
        CustomCount != 2 || ComponentMaskCount != 1 ||
        Constant3Count != 1 || ConstantCount != 1 || LerpCount != 3 ||
        MultiplyCount != 2 || Nodes.Num() != ExpectedMasterExpressionCount ||
        !SeamSafeMetricUv ||
        SeamSafeMetricUv->Description != SeamSafeMetricUvDescription ||
        SeamSafeMetricUv->Code != SeamSafeMetricUvCode ||
        SeamSafeMetricUv->OutputType != CMOT_Float2 ||
        SeamSafeMetricUv->Inputs.Num() != 4 ||
        !Surface || Surface->Desc != TEXT("R31.TexturedWeatheredR25DepthSurface") ||
        Surface->Description != SurfaceResponseDescription ||
        Surface->Code != SurfaceResponseCode ||
        Surface->OutputType != CMOT_Float4 || Surface->Inputs.Num() != 23 ||
        !Surface->AdditionalOutputs.IsEmpty() ||
        !Surface->AdditionalDefines.IsEmpty() ||
        !Surface->IncludeFilePaths.IsEmpty())
    {
        OutError = TEXT("R31 broad-shell graph topology, source-metre UV, parameter roster, or weather/aperture code drifted.");
        return false;
    }
    if (ExpectedNodeClasses.Num() != ExpectedMasterExpressionCount)
    {
        OutError = TEXT("R31 broad-shell expected node-label/class roster is internally incomplete.");
        return false;
    }
    for (const TPair<FString, const UClass*>& Pair : ExpectedNodeClasses)
    {
        UMaterialExpression* Actual = Nodes.FindRef(Pair.Key);
        if (!Actual || Actual->GetClass() != Pair.Value)
        {
            OutError = TEXT("R31 broad-shell node-label/class binding drifted: ") +
                Pair.Key;
            return false;
        }
    }

    for (const TPair<FName, float>& Pair : ExpectedScalarDefaults)
    {
        UMaterialExpressionScalarParameter* const* Found =
            ScalarExpressions.Find(Pair.Key);
        if (!Found || !*Found || (*Found)->Group != ParameterGroup ||
            (*Found)->bUseCustomPrimitiveData ||
            (*Found)->PrimitiveDataIndex != 0 ||
            !FMath::IsNearlyEqual(
                (*Found)->DefaultValue, Pair.Value, 0.000001f))
        {
            OutError = TEXT("R31 broad-shell master scalar default/metadata drifted: ") +
                Pair.Key.ToString();
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : ExpectedVectorDefaults)
    {
        UMaterialExpressionVectorParameter* const* Found =
            VectorExpressions.Find(Pair.Key);
        if (!Found || !*Found || (*Found)->Group != ParameterGroup ||
            (*Found)->bUseCustomPrimitiveData ||
            (*Found)->PrimitiveDataIndex != 0 ||
            !(*Found)->DefaultValue.Equals(Pair.Value, 0.000001f))
        {
            OutError = TEXT("R31 broad-shell master vector default/metadata drifted: ") +
                Pair.Key.ToString();
            return false;
        }
    }
    const FString ExpectedDefaultTexturePaths[] = {
        TextureObjectPath(TEXT("Plaster"), ETextureUsage::BaseColor),
        TextureObjectPath(TEXT("Plaster"), ETextureUsage::Normal),
        TextureObjectPath(TEXT("Plaster"), ETextureUsage::PackedOrm)};
    const FName ExpectedDefaultTextureNames[] = {
        BaseColorTextureParameter, NormalTextureParameter,
        PackedOrmTextureParameter};
    for (int32 Index = 0; Index < 3; ++Index)
    {
        UMaterialExpressionTextureSampleParameter2D* const* Found =
            TextureExpressions.Find(ExpectedDefaultTextureNames[Index]);
        if (!Found || !*Found || (*Found)->Group != ParameterGroup ||
            !(*Found)->Texture || (*Found)->Texture->GetPathName() !=
                ExpectedDefaultTexturePaths[Index] ||
            (*Found)->SamplerSource != SSM_FromTextureAsset ||
            (*Found)->MipValueMode != TMVM_None ||
            (*Found)->ConstCoordinate != 0 ||
            (*Found)->ConstMipValue != INDEX_NONE ||
            !(*Found)->AutomaticViewMipBias ||
            !InputIsExactlyDisconnected((*Found)->TextureObject) ||
            !InputIsExactlyDisconnected((*Found)->MipValue) ||
            !InputIsExactlyDisconnected((*Found)->CoordinatesDX) ||
            !InputIsExactlyDisconnected((*Found)->CoordinatesDY) ||
            !InputIsExactlyDisconnected((*Found)->AutomaticViewMipBiasValue))
        {
            OutError = TEXT("R31 broad-shell master texture default/metadata drifted: ") +
                ExpectedDefaultTextureNames[Index].ToString();
            return false;
        }
    }
    const FName ExpectedInputNames[] = {
        TEXT("BaseSurface"), TEXT("PackedOrm"), TEXT("UV0"),
        TEXT("WorldPositionCm"), TEXT("CameraPositionCm"),
        TEXT("SurfaceRoughness"), TEXT("RoughnessTextureWeight"),
        TEXT("VariationCellMeters"), TEXT("WeatheringStrength"),
        TEXT("WallVerticalWeatherMask"), TEXT("BayMeters"),
        TEXT("StoreyMeters"), TEXT("ApertureWidthFraction"),
        TEXT("ApertureHeightFraction"), TEXT("ApertureSillFraction"),
        TEXT("ApertureHintStrength"), TEXT("ApertureFadeStartCm"),
        TEXT("ApertureFadeEndCm"), TEXT("ApertureTint"),
        TEXT("AtmosphereStartCm"), TEXT("AtmosphereEndCm"),
        TEXT("AtmosphereStrength"), TEXT("AtmosphereTint")};
    UMaterialExpression* const ExpectedInputExpressions[] = {
        BaseBlendExpression, TextureExpressions.FindRef(PackedOrmTextureParameter),
        SeamSafeMetricUv, WorldPositionExpression, CameraPositionExpression,
        ScalarExpressions.FindRef(SurfaceRoughnessParameter),
        ScalarExpressions.FindRef(RoughnessTextureWeightParameter),
        ScalarExpressions.FindRef(VariationCellMetersParameter),
        ScalarExpressions.FindRef(WeatheringStrengthParameter),
        ScalarExpressions.FindRef(WallVerticalWeatherMaskParameter),
        ScalarExpressions.FindRef(BayMetersParameter),
        ScalarExpressions.FindRef(StoreyMetersParameter),
        ScalarExpressions.FindRef(ApertureWidthFractionParameter),
        ScalarExpressions.FindRef(ApertureHeightFractionParameter),
        ScalarExpressions.FindRef(ApertureSillFractionParameter),
        ScalarExpressions.FindRef(ApertureHintStrengthParameter),
        ScalarExpressions.FindRef(ApertureFadeStartCmParameter),
        ScalarExpressions.FindRef(ApertureFadeEndCmParameter),
        VectorExpressions.FindRef(ApertureTintParameter),
        ScalarExpressions.FindRef(AtmosphereStartCmParameter),
        ScalarExpressions.FindRef(AtmosphereEndCmParameter),
        ScalarExpressions.FindRef(AtmosphereStrengthParameter),
        VectorExpressions.FindRef(AtmosphereTintParameter)};
    static_assert(
        UE_ARRAY_COUNT(ExpectedInputNames) ==
        UE_ARRAY_COUNT(ExpectedInputExpressions));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedInputNames); ++Index)
    {
        if (!ExpectedInputExpressions[Index] ||
            Surface->Inputs[Index].InputName != ExpectedInputNames[Index] ||
            !InputMatchesExactOutput(
                Surface->Inputs[Index].Input,
                ExpectedInputExpressions[Index], 0))
        {
            OutError = FString::Printf(
                TEXT("R31 broad-shell custom-input topology drifted at index %d."),
                Index);
            return false;
        }
    }
    const FName ExpectedSeamSafeUvInputNames[] = {
        TEXT("UV0"), TEXT("WorldPositionCm"), TEXT("VertexTangentWS"),
        TEXT("WallRoleMask")};
    UMaterialExpression* const ExpectedSeamSafeUvInputs[] = {
        UvExpression, WorldPositionExpression, VertexTangentExpression,
        ScalarExpressions.FindRef(WallVerticalWeatherMaskParameter)};
    static_assert(
        UE_ARRAY_COUNT(ExpectedSeamSafeUvInputNames) ==
        UE_ARRAY_COUNT(ExpectedSeamSafeUvInputs));
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(ExpectedSeamSafeUvInputNames); ++Index)
    {
        if (!ExpectedSeamSafeUvInputs[Index] ||
            SeamSafeMetricUv->Inputs[Index].InputName !=
                ExpectedSeamSafeUvInputNames[Index] ||
            !InputMatchesExactOutput(
                SeamSafeMetricUv->Inputs[Index].Input,
                ExpectedSeamSafeUvInputs[Index], 0))
        {
            OutError = FString::Printf(
                TEXT("R31 seam-safe metric-UV custom-input topology drifted at index %d."),
                Index);
            return false;
        }
    }
    UMaterialExpressionTextureCoordinate* ExactUv =
        Cast<UMaterialExpressionTextureCoordinate>(
            Nodes.FindRef(TEXT("R31.UV0.SourceFacadeMetres")));
    UMaterialExpressionMultiply* ExactScaledUv =
        Cast<UMaterialExpressionMultiply>(
            Nodes.FindRef(TEXT("R31.ScaleSourceMetreUV")));
    UMaterialExpressionMultiply* ExactTexturedTint =
        Cast<UMaterialExpressionMultiply>(
            Nodes.FindRef(TEXT("R31.TexturedTint")));
    UMaterialExpressionLinearInterpolate* ExactBaseBlend =
        Cast<UMaterialExpressionLinearInterpolate>(BaseBlendExpression);
    UMaterialExpressionConstant3Vector* ExactFlatNormal =
        Cast<UMaterialExpressionConstant3Vector>(
            Nodes.FindRef(TEXT("R31.FlatNormal")));
    UMaterialExpressionLinearInterpolate* ExactNormalBlend =
        Cast<UMaterialExpressionLinearInterpolate>(
            Nodes.FindRef(TEXT("R31.BlendFlatAndTextureNormal")));
    UMaterialExpressionConstant* ExactAoOne =
        Cast<UMaterialExpressionConstant>(
            Nodes.FindRef(TEXT("R31.AmbientOcclusionOne")));
    UMaterialExpressionLinearInterpolate* ExactAoBlend =
        Cast<UMaterialExpressionLinearInterpolate>(
            Nodes.FindRef(TEXT("R31.BlendAmbientOcclusion")));
    UMaterialExpressionComponentMask* ExactRoughnessOutput =
        Cast<UMaterialExpressionComponentMask>(
            Nodes.FindRef(TEXT("R31.SurfaceRoughnessA")));
    UMaterialExpressionTextureSampleParameter2D* ExactBase =
        TextureExpressions.FindRef(BaseColorTextureParameter);
    UMaterialExpressionTextureSampleParameter2D* ExactNormal =
        TextureExpressions.FindRef(NormalTextureParameter);
    UMaterialExpressionTextureSampleParameter2D* ExactOrm =
        TextureExpressions.FindRef(PackedOrmTextureParameter);
    if (!ExactUv || ExactUv != UvExpression || ExactUv->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(ExactUv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(ExactUv->VTiling, 1.0f, 0.000001f) ||
        ExactUv->UnMirrorU || ExactUv->UnMirrorV ||
        !ExactScaledUv ||
        !FMath::IsNearlyEqual(ExactScaledUv->ConstA, 0.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(ExactScaledUv->ConstB, 1.0f, 0.000001f) ||
        !InputMatchesExactOutput(ExactScaledUv->A, SeamSafeMetricUv, 0) ||
        !InputMatchesExactOutput(ExactScaledUv->B,
                     ScalarExpressions.FindRef(UvScaleParameter), 0) ||
        !ExactBase || ExactBase->SamplerType != SAMPLERTYPE_Color ||
        !InputMatchesExactOutput(ExactBase->Coordinates, ExactScaledUv, 0) ||
        !ExactNormal || ExactNormal->SamplerType != SAMPLERTYPE_Normal ||
        !InputMatchesExactOutput(ExactNormal->Coordinates, ExactScaledUv, 0) ||
        !ExactOrm || ExactOrm->SamplerType != SAMPLERTYPE_Masks ||
        !InputMatchesExactOutput(ExactOrm->Coordinates, ExactScaledUv, 0) ||
        !ExactTexturedTint ||
        !FMath::IsNearlyEqual(ExactTexturedTint->ConstA, 0.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(ExactTexturedTint->ConstB, 1.0f, 0.000001f) ||
        !InputMatchesExactOutput(ExactTexturedTint->A, ExactBase, 0) ||
        !InputMatchesExactOutput(ExactTexturedTint->B,
                     VectorExpressions.FindRef(TintParameter), 0) ||
        !ExactBaseBlend ||
        !FMath::IsNearlyEqual(ExactBaseBlend->ConstA, 0.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(ExactBaseBlend->ConstB, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            ExactBaseBlend->ConstAlpha, 0.5f, 0.000001f) ||
        !InputMatchesExactOutput(ExactBaseBlend->A,
                     VectorExpressions.FindRef(TintParameter), 0) ||
        !InputMatchesExactOutput(ExactBaseBlend->B, ExactTexturedTint, 0) ||
        !InputMatchesExactOutput(ExactBaseBlend->Alpha,
                     ScalarExpressions.FindRef(TextureInfluenceParameter), 0) ||
        !ExactFlatNormal ||
        !ExactFlatNormal->Constant.Equals(
            FLinearColor(0.0f, 0.0f, 1.0f, 1.0f), 0.000001f) ||
        !ExactNormalBlend ||
        !FMath::IsNearlyEqual(ExactNormalBlend->ConstA, 0.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(ExactNormalBlend->ConstB, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            ExactNormalBlend->ConstAlpha, 0.5f, 0.000001f) ||
        !InputMatchesExactOutput(ExactNormalBlend->A, ExactFlatNormal, 0) ||
        !InputMatchesExactOutput(ExactNormalBlend->B, ExactNormal, 0) ||
        !InputMatchesExactOutput(ExactNormalBlend->Alpha,
                     ScalarExpressions.FindRef(NormalStrengthParameter), 0) ||
        !ExactAoOne || !FMath::IsNearlyEqual(ExactAoOne->R, 1.0f, 0.000001f) ||
        !ExactAoBlend ||
        !FMath::IsNearlyEqual(ExactAoBlend->ConstA, 0.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(ExactAoBlend->ConstB, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            ExactAoBlend->ConstAlpha, 0.5f, 0.000001f) ||
        !InputMatchesExactOutput(ExactAoBlend->A, ExactAoOne, 0) ||
        !InputMatchesExactOutput(ExactAoBlend->B, ExactOrm, 1) ||
        !InputMatchesExactOutput(ExactAoBlend->Alpha,
                     ScalarExpressions.FindRef(AoTextureWeightParameter), 0) ||
        !ExactRoughnessOutput || ExactRoughnessOutput->R ||
        ExactRoughnessOutput->G || ExactRoughnessOutput->B ||
        !ExactRoughnessOutput->A ||
        !InputMatchesExactOutput(ExactRoughnessOutput->Input, Surface, 0) ||
        !InputMatchesExactOutput(EditorOnly->BaseColor, Surface, 0) ||
        !InputMatchesExactOutput(EditorOnly->Normal, ExactNormalBlend, 0) ||
        !InputMatchesExactOutput(
            EditorOnly->Roughness, ExactRoughnessOutput, 0) ||
        !InputMatchesExactOutput(
            EditorOnly->Metallic,
            ScalarExpressions.FindRef(MetallicParameter), 0) ||
        !InputMatchesExactOutput(
            EditorOnly->Specular,
            ScalarExpressions.FindRef(SpecularParameter), 0) ||
        !InputMatchesExactOutput(
            EditorOnly->AmbientOcclusion, ExactAoBlend, 0) ||
        Cast<UMaterialExpressionWorldPosition>(WorldPositionExpression)->
            WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets)
    {
        OutError = TEXT("R31 broad-shell graph edge, sampler, UV0, or material-output topology drifted.");
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateInstance(
    UMaterialInstanceConstant* Instance,
    const FMaterialSpec& Spec,
    UMaterial* Parent,
    bool bRequireSaved,
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
        Instance->GetPathName() != MaterialObjectPath(Spec) ||
        Instance->Parent != Parent ||
        Instance->ScalarParameterValues.Num() != ExpectedScalarParameterCount ||
        Instance->VectorParameterValues.Num() != ExpectedVectorParameterCount ||
        Instance->TextureParameterValues.Num() != ExpectedTextureParameterCount ||
        !Instance->DoubleVectorParameterValues.IsEmpty() ||
        !Instance->TextureCollectionParameterValues.IsEmpty() ||
        !Instance->RuntimeVirtualTextureParameterValues.IsEmpty() ||
        !Instance->SparseVolumeTextureParameterValues.IsEmpty() ||
        !Instance->FontParameterValues.IsEmpty() ||
        !Instance->UserSceneTextureOverrides.IsEmpty() ||
        !StaticParameters.StaticSwitchParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.StaticComponentMaskParameters.IsEmpty() ||
        !StaticParameters.EditorOnly.TerrainLayerWeightParameters.IsEmpty() ||
        StaticParameters.bHasMaterialLayers || !BaseOverrides.IsEmpty() ||
        !Instance->NaniteOverrideMaterial.bEnableOverride ||
        Instance->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Instance->GetNaniteOverride() ||
        (bRequireSaved &&
         (!Instance->GetOutermost() || Instance->GetOutermost()->IsDirty() ||
          !FPackageName::DoesPackageExist(Instance->GetOutermost()->GetName()))))
    {
        OutError = FString::Printf(
            TEXT("R31 broad-shell material '%s' lost its exact parent/parameter/base-property/static/save contract; baseOverrides=[%s]."),
            Spec.Name, *BaseOverrides);
        return false;
    }

    TArray<TPair<FName, float>> ExpectedScalars;
    GatherSpecScalars(Spec, ExpectedScalars);
    TSet<FName> ExpectedScalarNames;
    TSet<FName> ActualScalarNames;
    for (const TPair<FName, float>& Pair : ExpectedScalars)
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
                TEXT("R31 material '%s' has an invalid or duplicate global scalar override."),
                Spec.Name);
            return false;
        }
        ActualScalarNames.Add(Value.ParameterInfo.Name);
    }
    if (!SameSet(ExpectedScalarNames, ActualScalarNames))
    {
        OutError = FString::Printf(
            TEXT("R31 material '%s' scalar override roster drifted."),
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
                TEXT("R31 material '%s' scalar '%s' drifted."),
                Spec.Name, *Pair.Key.ToString());
            return false;
        }
    }
    FLinearColor ActualTint;
    FLinearColor ActualApertureTint;
    FLinearColor ActualAtmosphereTint;
    const TSet<FName> ExpectedVectorNames = {
        TintParameter, ApertureTintParameter, AtmosphereTintParameter};
    TSet<FName> ActualVectorNames;
    for (const FVectorParameterValue& Value : Instance->VectorParameterValues)
    {
        if (Value.ParameterInfo.Name.IsNone() ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            ActualVectorNames.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("R31 material '%s' has an invalid or duplicate global vector override."),
                Spec.Name);
            return false;
        }
        ActualVectorNames.Add(Value.ParameterInfo.Name);
    }
    if (!SameSet(ExpectedVectorNames, ActualVectorNames))
    {
        OutError = FString::Printf(
            TEXT("R31 material '%s' vector override roster drifted."),
            Spec.Name);
        return false;
    }
    if (!Instance->GetVectorParameterValue(
            FHashedMaterialParameterInfo(TintParameter), ActualTint, true) ||
        !ActualTint.Equals(Spec.Tint, 0.000001f) ||
        !Instance->GetVectorParameterValue(
            FHashedMaterialParameterInfo(ApertureTintParameter),
            ActualApertureTint, true) ||
        !ActualApertureTint.Equals(Spec.ApertureTint, 0.000001f) ||
        !Instance->GetVectorParameterValue(
            FHashedMaterialParameterInfo(AtmosphereTintParameter),
            ActualAtmosphereTint, true) ||
        !ActualAtmosphereTint.Equals(Spec.AtmosphereTint, 0.000001f))
    {
        OutError = FString::Printf(
            TEXT("R31 material '%s' vector roster drifted."), Spec.Name);
        return false;
    }

    const TMap<FName, FString> ExpectedTexturePaths = {
        {BaseColorTextureParameter,
         TextureObjectPath(Spec.TextureSet, ETextureUsage::BaseColor)},
        {NormalTextureParameter,
         TextureObjectPath(Spec.TextureSet, ETextureUsage::Normal)},
        {PackedOrmTextureParameter,
         TextureObjectPath(Spec.TextureSet, ETextureUsage::PackedOrm)}};
    TSet<FName> SeenTextures;
    for (const FTextureParameterValue& Value : Instance->TextureParameterValues)
    {
        const FString* Expected = ExpectedTexturePaths.Find(
            Value.ParameterInfo.Name);
        if (!Expected || !Value.ParameterValue ||
            Value.ParameterValue->GetPathName() != *Expected ||
            Value.ParameterValue->GetPathName().Contains(TEXT("HeroMaterials")) ||
            Value.ParameterInfo.Association !=
                EMaterialParameterAssociation::GlobalParameter ||
            Value.ParameterInfo.Index != INDEX_NONE ||
            SeenTextures.Contains(Value.ParameterInfo.Name))
        {
            OutError = FString::Printf(
                TEXT("R31 material '%s' has a missing, duplicate, or forbidden texture override."),
                Spec.Name);
            return false;
        }
        SeenTextures.Add(Value.ParameterInfo.Name);
    }
    if (SeenTextures.Num() != ExpectedTextureParameterCount)
    {
        OutError = FString::Printf(
            TEXT("R31 material '%s' does not bind exactly BaseColor, Normal and packed ORM."),
            Spec.Name);
        return false;
    }
    return ValidateCompiledMaterial(Instance, OutError);
}

bool ValidateInternal(bool bRequireSaved, FString& OutReport)
{
    if (!ValidateSourceContract(OutReport))
    {
        return false;
    }
    FString V2Report;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedV2Asset(V2Report))
    {
        OutReport = TEXT("R31 refused an invalid immutable 43,448-triangle V2 shell: ") +
            V2Report;
        return false;
    }
    TMap<FName, UTexture2D*> Textures;
    if (!LoadAllTextures(Textures, OutReport))
    {
        return false;
    }
    if (!ValidateExactRootRoster(bRequireSaved, OutReport))
    {
        return false;
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    UMaterial* Master = LoadExact<UMaterial>(MasterObjectPath);
    if (!ValidateMaster(Master, bRequireSaved, OutReport))
    {
        return false;
    }
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        if (!ValidateInstance(
                LoadExact<UMaterialInstanceConstant>(MaterialObjectPath(Spec)),
                Spec, Master, bRequireSaved, OutReport))
        {
            return false;
        }
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R31_BROAD_SHELL_ASSETS_VALID assets=5 master=1 instances=4 materialOverrides=17 uniqueTextureSets=3 textureBindings=12 uniqueTextureDependencies=9 texturePayloadBinding=source-provenance-admitted-existing-payload sourceFileSha256Pinned=true storedImportMd5Matches=true sourceIdNonempty=true sourceIdClaimedAsPayloadDigest=false embeddedPixelByteEqualityClaim=false immutableTextureTreeRequired=true baseColor=true tangentNormal=true packedOrm=true exactV2Mesh=true canonicalSourceGroups=1391 retainedSuppressionV2Groups=1388 retainedTriangles=43448 retainedMaterialSlots=17 retainedVertexInstances=130344 sourceMetreUv0=true wallUPerSegment=true wallVAbsoluteSourceZ=true roofBottomUvHeroLocalXy=true sourceObjVRecovered=true buildingLocalAboveGradeCoordinate=false fullPrecisionUv=true generatedLightmapUv=false identityTransform=true facadePlaneArchetypes=4 facadePlaneSignals=true normalizedFourArchetypeBlend=true hardPlaneDistanceKey=false hardFamilyThreshold=false worldXyGridCellHash=false constantWithinPlanarFace=true vertexTangentWorldPlaneFacadeU=true vertexTangentDominantComponentQuantizationLevels=4096 maximumWallMetricScaleRelativeError=0.00018 seamSafeMetricTextureUv=true sourcePerSegmentUUsedForWallCadence=false cadenceWarping=false naturalCornerBreaks=true arrayPositionHash=false shortRepeatingCycle=false exactOsmGroupIdVisibleToPixelShader=false deterministicCadencePhase=true storeyCadenceAbsoluteSourceZ=true deterministicGlazingOccupancy=true deterministicBlindVariation=true wallOnlyFamilyColour=true architecturalSpandrelPierTwoStoreySlabEdge=true antialiasedArchitecturalEdges=true hardFloorParitySelector=false exactPerBuildingStyleClaimed=false balconyGeometryClaimed=false wallVerticalDirt=false plinthContactDarkening=false wallRunoff=true roofMottling=true smoothMacroTone=true shallowInteriorCueMetres=0.115 worldPositionOffset=false pixelDepthOffset=false r25FrameInsetGlassRetained=true r25MullionTransomRetained=true r25RevealRetained=true r25CellOccupancyToneRetained=true r25FresnelRetained=true r25PlinthContactRetained=false r25AtmosphericDistanceRetained=true wallDepthCuesRoleGated=true deterministicWeathering=true opaque=true defaultLit=true oneSided=true nanite=true materialOnly=true meshPackageMutated=false topologyModified=false transformsModified=false geographyModified=false providerHandoffModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false cesiumModified=false vegetationModified=false terrainModified=false sourceWithinRenderPathContinuityProven=true naniteRasterAppearanceParityProven=false nativeNaniteRasterPairRequired=true surveyAsBuiltCurrentCompletePhysicalMaterialHyperreal=false midFarOnly=true visualCaptureAccepted=false captureRevalidationRequired=true.");
    return true;
}
} // namespace

namespace TRIADIstanaExploreV5DR31BroadShellAssetFactory
{
const FString& GetAssetRootPath()
{
    return AssetRoot;
}

const FString& GetMasterMaterialObjectPath()
{
    return MasterObjectPath;
}

const TArray<FString>& GetOrderedMaterialObjectPaths()
{
    return OrderedMaterialPaths();
}

bool CreateFreshAssets(TArray<UObject*>& OutAssets, FString& OutError)
{
    OutAssets.Reset();
    FString ExistingReport;
    if (ValidateInternal(true, ExistingReport))
    {
        for (const FString& Path : ExpectedObjectPaths())
        {
            OutAssets.Add(LoadObject<UObject>(nullptr, *Path));
        }
        OutError = ExistingReport;
        return true;
    }
    if (!ValidateSourceContract(OutError))
    {
        return false;
    }
    FString V2Report;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedV2Asset(V2Report))
    {
        OutError = TEXT("R31 creation refused invalid immutable V2 shell: ") +
            V2Report;
        return false;
    }
    TArray<FAssetData> Existing;
    if (!GatherRootAssets(Existing, OutError) || !Existing.IsEmpty())
    {
        OutError = TEXT("R31 creation refuses an invalid or partially populated isolated asset root.");
        return false;
    }
    TMap<FName, UTexture2D*> Textures;
    if (!LoadAllTextures(Textures, OutError))
    {
        return false;
    }
    FScopedFreshRollback Rollback(OutAssets, OutError);
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>(
            TEXT("AssetTools")).Get();
    UMaterial* Master = CreateMaster(AssetTools, Textures, OutError);
    if (!Master)
    {
        return false;
    }
    OutAssets.Add(Master);
    for (const FMaterialSpec& Spec : MaterialSpecs)
    {
        UMaterialInstanceConstant* Instance = CreateInstance(
            AssetTools, Spec, Master, Textures, OutError);
        if (!Instance)
        {
            return false;
        }
        OutAssets.Add(Instance);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    if (OutAssets.Num() != ExpectedAssetCount ||
        !ValidateMaster(Master, false, OutError))
    {
        return false;
    }
    for (int32 Index = 0; Index < ExpectedMaterialCount; ++Index)
    {
        if (!ValidateInstance(
                Cast<UMaterialInstanceConstant>(OutAssets[Index + 1]),
                MaterialSpecs[Index], Master, false, OutError))
        {
            return false;
        }
    }
    Rollback.Commit();
    OutError.Reset();
    return true;
}

bool ValidateAssets(FString& OutReport)
{
    return ValidateInternal(true, OutReport);
}
} // namespace TRIADIstanaExploreV5DR31BroadShellAssetFactory
