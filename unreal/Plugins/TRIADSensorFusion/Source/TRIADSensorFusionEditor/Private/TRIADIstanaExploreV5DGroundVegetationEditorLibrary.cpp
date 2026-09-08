#include "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Editor.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "PackageTools.h"
#include "RHIFeatureLevel.h"
#include "ShaderCompiler.h"
#include "Ssl.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5BVisualActor.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
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
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString R20UnloadMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5b"));
constexpr int64 R20PredecessorMapBytes = 37414604;
const FString R20PredecessorMapSha256(
    TEXT("E01C5ECB476E9723F3FB85AFFE101EE4CEBC719FD493E40C26E3BC85584FFAAB"));
// Exact first clean R20 transaction result. Idempotence is admitted only for
// this cold-validated serialized map artifact.
constexpr int64 R20FinalMapBytes = 32390049;
const FString R20FinalMapSha256(
    TEXT("4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33"));
// Exact first cold-validated R23 serialized-layout result. The R20 pin remains
// the sole mutation predecessor; this pin is the sole current layout result.
constexpr int64 R23FinalMapBytes = 34986401;
const FString R23FinalMapSha256(
    TEXT("49A2879BE33704DDE1C6C0EFBAAE2364300E36EB1F0466B775EDE4F7BB4870DC"));
const FString R23LayoutBackupRelativeRoot(
    TEXT("TRIAD/Backups/V5D_R23_GrassLayout"));
const FString R23LayoutBackupReceiptHeader(
    TEXT("TRIAD_V5D_R23_GRASS_LAYOUT_BACKUP_V1"));
struct FR21PredecessorArtifactPin
{
    int64 Bytes;
    const TCHAR* Sha256;
};

struct FR22PredecessorArtifactPin
{
    const TCHAR* AssetName;
    int64 Bytes;
    const TCHAR* Sha256;
};

// Exact cold-validated R19 material artifacts admitted by the R21 transaction,
// in GrassAssetNames order. The R20 map pin above remains an independent,
// immutable geometry/layout prerequisite.
const FR21PredecessorArtifactPin R21PredecessorGrassPins[] = {
    {37718, TEXT("42B3E4F81046415B3A65CE0F4F81A5D168F4FE270EF70E5872A21131645C6339")},
    {37645, TEXT("949324A4266C0A09684B213695295B0D2C9ADDDC310DB74F3FE34D12E10C9E51")},
    {37706, TEXT("AC8D77D34C4C9BCA5144949A1989C44E2F6F4DBD2702F9705977818763CF5109")},
    {37805, TEXT("197177AA26CF23A22E8E4297112441E75062EC643E43145A56065D7C7C0EB9DA")}};
const FString ContentNamespace(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation"));
const FString MaterialNamespace(ContentNamespace + TEXT("/Materials"));
const FString SourceGrassMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5B/Materials/M_IPV5B_AccentTurf.M_IPV5B_AccentTurf"));
const FString SourceSoilMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5B/Materials/M_IPV5B_FormalBedSoil.M_IPV5B_FormalBedSoil"));
const FString SourceEdgeGrassMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_GrassMedium_Wind.M_IPV4_GrassMedium_Wind"));
const FString GroundR12BaseColorTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_Grass001_BaseColor.T_IPV4_Grass001_BaseColor"));
const FString GroundR12NormalTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_Grass001_NormalDX.T_IPV4_Grass001_NormalDX"));
const FString GroundR12RoughnessTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_Grass001_Roughness.T_IPV4_Grass001_Roughness"));
const FString GroundR12AoTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_Grass001_AmbientOcclusion.T_IPV4_Grass001_AmbientOcclusion"));
const FString GroundBaseColorTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_Color.Grass004_Color"));
const FString GroundNormalTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_NormalGL.Grass004_NormalGL"));
const FString GroundRoughnessTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_Roughness.Grass004_Roughness"));
const FString GroundAoTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Textures/Grass004_AmbientOcclusion.Grass004_AmbientOcclusion"));
const FName GroundVegetationTag(
    TEXT("TRIADIstanaExploreV5DGroundVegetation"));

const FString GrassAssetNames[] = {
    TEXT("M_IPV5D_Turf_Manicured"),
    TEXT("M_IPV5D_Turf_Humid"),
    TEXT("M_IPV5D_Turf_Shade"),
    TEXT("M_IPV5D_Turf_DryEdge")};
const FString SoilAssetName(TEXT("M_IPV5D_SoilMulch_Layered"));
const FString GroundOverlayAssetName(TEXT("M_IPV5D_LawnMacroVariation"));
const FString EdgeGrassFadeAssetName(TEXT("M_IPV5D_GrassMedium_EdgeFade"));

// Exact cold-validated R21/R18/R11 six-package grass-system state admitted by
// the R22 transaction. The ordered roster is four fine-turf packages followed
// by the lawn overlay and the R11 edge-fade derivative.
const FR22PredecessorArtifactPin R22PredecessorPins[] = {
    {TEXT("M_IPV5D_Turf_Manicured"), 39262, TEXT("200F648C69E8ED88CE2FED8FD08EEB2FEA01D4E7933A193BAB42B830A66FE1AD")},
    {TEXT("M_IPV5D_Turf_Humid"), 39083, TEXT("937B006B01BD95E1F805ED9424A7F09AC7E8FE9475D768139428FA4CF4AEDAC5")},
    {TEXT("M_IPV5D_Turf_Shade"), 39649, TEXT("5239BBC6F3AC171CFFC0AE7A9BAD3413198A2D30C00A1C780D3AEAE1FA0008A5")},
    {TEXT("M_IPV5D_Turf_DryEdge"), 39659, TEXT("5AD7B4C89F7942FBC2840B9123C3FA4C8DF83FA484EFCD1D2F1F4B5555649C58")},
    {TEXT("M_IPV5D_LawnMacroVariation"), 32987, TEXT("C032A10C3803FD92FAFED7CB003E853A89207A4364383E65F845C30130ADD38B")},
    {TEXT("M_IPV5D_GrassMedium_EdgeFade"), 33972, TEXT("640360DF0CCB516A9861D4CB27E2A948A5F81792E3F822299ED765D8261732B4")}};
// Exact cold-validated R22 six-package result admitted by the R23
// photographic-depth transaction.  R22 remains a preserved visual candidate;
// only these exact canonical packages can advance to R23.
const FR22PredecessorArtifactPin R23PredecessorPins[] = {
    {TEXT("M_IPV5D_Turf_Manicured"), 41028, TEXT("0BA45DF9C7C826B0F6473C75621DEEFADD4FE3DC8BE4D51D226FEAF337DD76CA")},
    {TEXT("M_IPV5D_Turf_Humid"), 40858, TEXT("86E60FC8839C11A7939DD822C517E560D0BE656B38E5534067A30080A6728634")},
    {TEXT("M_IPV5D_Turf_Shade"), 41398, TEXT("FF09A4652EC84CCEF922A253263FF2CD979DBE3E0B1B5B4EEA396387713CE35A")},
    {TEXT("M_IPV5D_Turf_DryEdge"), 41412, TEXT("7DEF8E81069B8424B6E0F33B1910637B2B58288E96BC6EEBBE7977A872FD7AE1")},
    {TEXT("M_IPV5D_LawnMacroVariation"), 35780, TEXT("0C88CB096CCF567F55684CBE53CA3BB0B42E60FF494968E3CC2CC11703D0F011")},
    {TEXT("M_IPV5D_GrassMedium_EdgeFade"), 35623, TEXT("A6E3CD4502D8B1932EE8E8AF0D302A92B4F838BA7E97A369650C1EE5AE017CF2")}};
// Exact cold-validated R23 material outputs required by the subsequent
// map-only R20-to-R23 layout migration. The order remains four fine-turf
// packages, lawn overlay, then the owned edge-fade derivative.
const FR22PredecessorArtifactPin R23LayoutMaterialPins[] = {
    {TEXT("M_IPV5D_Turf_Manicured"), 41410, TEXT("ADC96F6308597891F641981939F831C4BA3E77C0DC3F8B290F636D2B9B63203A")},
    {TEXT("M_IPV5D_Turf_Humid"), 41082, TEXT("FD3354E36C9D30E16FDD2B8B0474E414B9775C76B1E53F10E9F0FEF29B2B71A0")},
    {TEXT("M_IPV5D_Turf_Shade"), 41123, TEXT("7974B40558A698B689ECC6158F3EA92BFD063E6B6AA6980E18C5C90AEA9B41E7")},
    {TEXT("M_IPV5D_Turf_DryEdge"), 41863, TEXT("5D1EDB35B52E779878C1B593373EA4C29B1AC64D625B80B4C4F8F1A7B4D530DE")},
    {TEXT("M_IPV5D_LawnMacroVariation"), 37007, TEXT("DF1C5C7DADF704FA5F70093F791066F8AB2BD601AEF95C4158BF1E2B936A0249")},
    {TEXT("M_IPV5D_GrassMedium_EdgeFade"), 35755, TEXT("FE68E9021BEDBA7A32DDEE7C1BF485287C6A0B98B26D684EEECFDEA774EE464E")}};
// R23B is an additive material-only successor to the exact persisted R23
// layout/material result above. Keep this table separate so R23 historical
// provenance and its map-only migration pins remain immutable.
const FR22PredecessorArtifactPin R23BPredecessorPins[] = {
    {TEXT("M_IPV5D_Turf_Manicured"), 41410, TEXT("ADC96F6308597891F641981939F831C4BA3E77C0DC3F8B290F636D2B9B63203A")},
    {TEXT("M_IPV5D_Turf_Humid"), 41082, TEXT("FD3354E36C9D30E16FDD2B8B0474E414B9775C76B1E53F10E9F0FEF29B2B71A0")},
    {TEXT("M_IPV5D_Turf_Shade"), 41123, TEXT("7974B40558A698B689ECC6158F3EA92BFD063E6B6AA6980E18C5C90AEA9B41E7")},
    {TEXT("M_IPV5D_Turf_DryEdge"), 41863, TEXT("5D1EDB35B52E779878C1B593373EA4C29B1AC64D625B80B4C4F8F1A7B4D530DE")},
    {TEXT("M_IPV5D_LawnMacroVariation"), 37007, TEXT("DF1C5C7DADF704FA5F70093F791066F8AB2BD601AEF95C4158BF1E2B936A0249")},
    {TEXT("M_IPV5D_GrassMedium_EdgeFade"), 35755, TEXT("FE68E9021BEDBA7A32DDEE7C1BF485287C6A0B98B26D684EEECFDEA774EE464E")}};
// Exact outputs measured by the first guarded R23B bootstrap save and then
// cold-read from disk. Publication/idempotence is admitted only for this sealed
// six-package canonical roster.
constexpr bool R23BFinalPinsAreSealed = true;
const FR22PredecessorArtifactPin R23BFinalMaterialPins[] = {
    {TEXT("M_IPV5D_Turf_Manicured"), 42053, TEXT("382B3F435190B79323DC0C91F2516889A24E93A072F6665BEB3AF14021F11324")},
    {TEXT("M_IPV5D_Turf_Humid"), 41699, TEXT("1BC6A33CFA62AEE015788F54E1EFA80BDB898F0E55FB1A84C3173A8193B567CD")},
    {TEXT("M_IPV5D_Turf_Shade"), 41665, TEXT("41F3EEA2A01D10EECFA20BF6F4927AA43F8B03F6AA45B5E16982B019F9810BFB")},
    {TEXT("M_IPV5D_Turf_DryEdge"), 42510, TEXT("6744C31747733108D53E5BCEF85BDE35BDE69F4C16BE7039DE54C3F2A8BE29B6")},
    {TEXT("M_IPV5D_LawnMacroVariation"), 37844, TEXT("4F976F96F28C57A3C9A98668460B8BF8F51824849D00F88BEDB8E10850FF6915")},
    {TEXT("M_IPV5D_GrassMedium_EdgeFade"), 36509, TEXT("5091EF91C6CCB8600F10507F6D46B6D780FC196DF63AB1754F53D49EA1141FBB")}};
constexpr int64 R22PreservedSoilBytes = 12219;
const FString R22PreservedSoilSha256(
    TEXT("1BA32CE17F611EA14C6D42C5D125D9562C15E1C8517BA845DEA3FC51B889B5AD"));
const FString EdgeGrassFadeNodeDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_PER_INSTANCE_FADE"));
const FString EdgeGrassFadeDitherDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_DITHER_TEMPORAL_AA"));
const FString EdgeGrassFadeMultiplyDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_RED_MASK_TIMES_DITHER"));
const FString GrassR22StableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R22_STABLE_SPATIAL_VISIBILITY_12M_18M"));
const FString EdgeGrassR22StableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_R22_STABLE_SPATIAL_VISIBILITY_12M_18M"));
const FString EdgeGrassR22CameraDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_R22_CAMERA_POSITION"));
const FString EdgeGrassR22RandomDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_R22_PER_INSTANCE_RANDOM"));
const FName R22RuntimeRevisionParameterName(
    TEXT("TRIAD_RuntimeMaterialRevision"));
const FString R22RuntimeRevisionDescription(
    TEXT("TRIAD_EXPLORE_V5D_R22_RUNTIME_MATERIAL_REVISION"));
constexpr float R22RuntimeRevisionValue = 22.0f;
const FString GrassR22StableVisibilityCode(
    TEXT("float instanceVisibility=saturate(InstanceFade);\n")
    TEXT("float instanceGate=step(saturate(Random01),instanceVisibility);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float distanceVisibility=1.0-smoothstep(1200.0,1800.0,distanceCm);\n")
    TEXT("float2 cell=floor(WorldPosition.xy*0.02);\n")
    TEXT("float distanceSeed=frac(saturate(Random01)*0.754877666+dot(cell,float2(0.1031,0.11369)));\n")
    TEXT("distanceSeed=frac(distanceSeed*(distanceSeed+33.33)*(distanceSeed+distanceSeed+17.17));\n")
    TEXT("float distanceGate=step(distanceSeed,distanceVisibility);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/22.0);\n")
    TEXT("return instanceGate*distanceGate*revisionGate;"));
const FString GrassR23StableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23_STABLE_SPATIAL_VISIBILITY_20M_28M"));
const FString EdgeGrassR23StableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_R23_STABLE_SPATIAL_VISIBILITY_20M_28M"));
const FString R23RuntimeRevisionDescription(
    TEXT("TRIAD_EXPLORE_V5D_R23_RUNTIME_MATERIAL_REVISION"));
constexpr float R23RuntimeRevisionValue = 23.0f;
const FName R23BRuntimeCalibrationRevisionParameterName(
    TEXT("TRIAD_RuntimeMaterialCalibrationRevision"));
const FString R23BRuntimeCalibrationRevisionDescription(
    TEXT("TRIAD_EXPLORE_V5D_R23B_RUNTIME_MATERIAL_CALIBRATION_REVISION"));
constexpr float R23BRuntimeCalibrationRevisionValue = 1.0f;
const FString GrassR23BStableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23B_STABLE_SPATIAL_VISIBILITY_20M_28M"));
const FString EdgeGrassR23BStableVisibilityDescription(
    TEXT("TRIAD_EXPLORE_V5D_EDGE_GRASS_R23B_STABLE_SPATIAL_VISIBILITY_20M_28M"));
const FString GrassR23StableVisibilityCode(
    TEXT("float instanceVisibility=saturate(InstanceFade);\n")
    TEXT("float instanceGate=step(saturate(Random01),instanceVisibility);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float distanceVisibility=1.0-smoothstep(2000.0,2800.0,distanceCm);\n")
    TEXT("float2 cell=floor(WorldPosition.xy*0.02);\n")
    TEXT("float distanceSeed=frac(saturate(Random01)*0.754877666+dot(cell,float2(0.1031,0.11369)));\n")
    TEXT("distanceSeed=frac(distanceSeed*(distanceSeed+33.33)*(distanceSeed+distanceSeed+17.17));\n")
    TEXT("float distanceGate=step(distanceSeed,distanceVisibility);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/23.0);\n")
    TEXT("return instanceGate*distanceGate*revisionGate;"));
const FString GrassR23BStableVisibilityCode(
    TEXT("float instanceVisibility=saturate(InstanceFade);\n")
    TEXT("float instanceGate=step(saturate(Random01),instanceVisibility);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float distanceVisibility=1.0-smoothstep(2000.0,2800.0,distanceCm);\n")
    TEXT("float2 cell=floor(WorldPosition.xy*0.02);\n")
    TEXT("float distanceSeed=frac(saturate(Random01)*0.754877666+dot(cell,float2(0.1031,0.11369)));\n")
    TEXT("distanceSeed=frac(distanceSeed*(distanceSeed+33.33)*(distanceSeed+distanceSeed+17.17));\n")
    TEXT("float distanceGate=step(distanceSeed,distanceVisibility);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/23.0);\n")
    TEXT("float calibrationGate=saturate(CalibrationRevision);\n")
    TEXT("return instanceGate*distanceGate*revisionGate*calibrationGate;"));
const FString EdgeGrassSourceWindDescription(
    TEXT("TRIAD_EXPLORE_V4_INSTANCE_LOCAL_PIVOT_UNDERDAMPED_WPO_V1"));
const FString EdgeGrassSourceWindCode(
    TEXT("float InstanceRandom = GetPerInstanceRandom(Parameters);\n")
    TEXT("float h = saturate(max(InstanceLocalPosition.z, 0.0) / max(HeightCm, 1.0));\n")
    TEXT("float phase = TimeSeconds * WindSpeed * 6.28318530718 + InstanceRandom * 6.28318530718 + dot(WorldPosition.xy, float2(0.0017, 0.0023));\n")
    TEXT("float wave = sin(phase) + 0.31 * sin(phase * 1.73 + 1.2);\n")
    TEXT("float2 direction = normalize(WindDirection.xy + float2(0.0001, 0.0));\n")
    TEXT("float bend = clamp(WindStrengthCm * ResponseScale * wave * h * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("float lift = clamp(abs(WindStrengthCm) * ResponseScale * 0.025 * sin(phase * 0.71) * h, -MaxWpoCm, MaxWpoCm);\n")
    TEXT("return float3(direction * bend, lift);"));
const FString EdgeGrassBaseColorTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_GrassMedium_BaseColor.T_IPV4_GrassMedium_BaseColor"));
const FString EdgeGrassNormalTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_GrassMedium_NormalDX.T_IPV4_GrassMedium_NormalDX"));
const FString EdgeGrassRoughnessTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_GrassMedium_Roughness.T_IPV4_GrassMedium_Roughness"));
const FString EdgeGrassOpacityTexturePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Textures/T_IPV4_GrassMedium_Opacity.T_IPV4_GrassMedium_Opacity"));
constexpr float EdgeGrassSourceHeightCm = 14.0426675f;

struct FGrassMaterialProfile
{
    const TCHAR* Id;
    FLinearColor Root;
    FLinearColor Body;
    FLinearColor Tip;
    FLinearColor Subsurface;
    float Roughness;
    float Specular;
    float WindResponse;
};

const FGrassMaterialProfile GrassProfiles[] = {
    {TEXT("MANICURED"), FLinearColor(0.105f, 0.180f, 0.083f), FLinearColor(0.145f, 0.226f, 0.111f), FLinearColor(0.153f, 0.235f, 0.117f), FLinearColor(0.118f, 0.195f, 0.092f), 0.70f, 0.30f, 0.14f},
    {TEXT("HUMID"), FLinearColor(0.100f, 0.175f, 0.084f), FLinearColor(0.137f, 0.219f, 0.111f), FLinearColor(0.145f, 0.228f, 0.117f), FLinearColor(0.112f, 0.189f, 0.093f), 0.66f, 0.32f, 0.18f},
    {TEXT("SHADE"), FLinearColor(0.091f, 0.154f, 0.087f), FLinearColor(0.124f, 0.197f, 0.113f), FLinearColor(0.131f, 0.205f, 0.118f), FLinearColor(0.104f, 0.174f, 0.099f), 0.72f, 0.28f, 0.11f},
    {TEXT("DRY_EDGE"), FLinearColor(0.126f, 0.165f, 0.086f), FLinearColor(0.171f, 0.207f, 0.112f), FLinearColor(0.179f, 0.216f, 0.118f), FLinearColor(0.129f, 0.164f, 0.088f), 0.76f, 0.26f, 0.14f}};
struct FR21GrassMaterialProfile
{
    const TCHAR* Id;
    FLinearColor Root;
    FLinearColor Body;
    FLinearColor Tip;
    FLinearColor SubsurfaceGain;
    float Roughness;
    float Specular;
    float WindResponse;
    float DryFraction;
    float ThatchFraction;
    float SoilRootFraction;
};

const FR21GrassMaterialProfile GrassR21Profiles[] = {
    {TEXT("MANICURED"), FLinearColor(0.095f, 0.115f, 0.030f), FLinearColor(0.185f, 0.210f, 0.055f), FLinearColor(0.200f, 0.220f, 0.060f), FLinearColor(1.30f, 0.84f, 0.56f), 0.68f, 0.30f, 0.14f, 0.10f, 0.18f, 0.035f},
    {TEXT("HUMID"), FLinearColor(0.087f, 0.108f, 0.032f), FLinearColor(0.172f, 0.200f, 0.057f), FLinearColor(0.187f, 0.210f, 0.062f), FLinearColor(1.28f, 0.86f, 0.58f), 0.66f, 0.31f, 0.18f, 0.07f, 0.13f, 0.025f},
    {TEXT("SHADE"), FLinearColor(0.075f, 0.092f, 0.032f), FLinearColor(0.150f, 0.175f, 0.058f), FLinearColor(0.163f, 0.185f, 0.063f), FLinearColor(1.34f, 0.88f, 0.62f), 0.72f, 0.28f, 0.11f, 0.13f, 0.22f, 0.045f},
    {TEXT("DRY_EDGE"), FLinearColor(0.115f, 0.120f, 0.027f), FLinearColor(0.205f, 0.195f, 0.048f), FLinearColor(0.224f, 0.207f, 0.052f), FLinearColor(1.24f, 0.80f, 0.52f), 0.75f, 0.26f, 0.14f, 0.26f, 0.34f, 0.075f}};
// Linear-light R23 diffuse/transmission profiles. These are bounded visual
// assumptions for managed tropical turf, not botanical or weather claims.
const FR21GrassMaterialProfile GrassR23Profiles[] = {
    {TEXT("MANICURED"), FLinearColor(0.060f, 0.105f, 0.030f), FLinearColor(0.110f, 0.195f, 0.050f), FLinearColor(0.135f, 0.220f, 0.065f), FLinearColor(1.12f, 1.06f, 0.92f), 0.68f, 0.30f, 0.14f, 0.050f, 0.090f, 0.025f},
    {TEXT("HUMID"), FLinearColor(0.050f, 0.095f, 0.030f), FLinearColor(0.095f, 0.180f, 0.052f), FLinearColor(0.120f, 0.205f, 0.068f), FLinearColor(1.10f, 1.07f, 0.94f), 0.66f, 0.31f, 0.18f, 0.035f, 0.070f, 0.018f},
    {TEXT("SHADE"), FLinearColor(0.045f, 0.075f, 0.030f), FLinearColor(0.080f, 0.145f, 0.052f), FLinearColor(0.105f, 0.175f, 0.068f), FLinearColor(1.16f, 1.10f, 1.00f), 0.72f, 0.28f, 0.11f, 0.065f, 0.120f, 0.032f},
    {TEXT("DRY_EDGE"), FLinearColor(0.080f, 0.095f, 0.025f), FLinearColor(0.135f, 0.160f, 0.045f), FLinearColor(0.170f, 0.185f, 0.055f), FLinearColor(1.08f, 1.01f, 0.86f), 0.75f, 0.26f, 0.14f, 0.180f, 0.240f, 0.060f}};
// R23B is a bounded visual-response calibration derived from rendered
// acceptance evidence. It is not a botanical, weather, or survey claim.
const FR21GrassMaterialProfile GrassR23BProfiles[] = {
    {TEXT("MANICURED"), FLinearColor(0.075f, 0.095f, 0.028f), FLinearColor(0.135f, 0.165f, 0.047f), FLinearColor(0.160f, 0.190f, 0.060f), FLinearColor(1.16f, 0.98f, 0.88f), 0.71f, 0.27f, 0.14f, 0.060f, 0.120f, 0.032f},
    {TEXT("HUMID"), FLinearColor(0.065f, 0.088f, 0.029f), FLinearColor(0.120f, 0.158f, 0.050f), FLinearColor(0.145f, 0.185f, 0.064f), FLinearColor(1.15f, 0.99f, 0.90f), 0.70f, 0.28f, 0.18f, 0.045f, 0.095f, 0.025f},
    {TEXT("SHADE"), FLinearColor(0.060f, 0.073f, 0.029f), FLinearColor(0.105f, 0.132f, 0.050f), FLinearColor(0.130f, 0.165f, 0.065f), FLinearColor(1.20f, 1.01f, 0.96f), 0.75f, 0.25f, 0.11f, 0.075f, 0.150f, 0.040f},
    {TEXT("DRY_EDGE"), FLinearColor(0.100f, 0.092f, 0.025f), FLinearColor(0.155f, 0.150f, 0.044f), FLinearColor(0.188f, 0.178f, 0.055f), FLinearColor(1.12f, 0.94f, 0.82f), 0.78f, 0.23f, 0.14f, 0.200f, 0.280f, 0.075f}};
const FLinearColor GrassR21DryBladeColor(0.285f, 0.205f, 0.060f);
const FLinearColor GrassR21ThatchColor(0.105f, 0.072f, 0.024f);
const FLinearColor GrassR21SoilRootColor(0.050f, 0.034f, 0.014f);
constexpr float GrassR21DryBlend = 0.90f;
constexpr float GrassR21ThatchBlend = 0.92f;
constexpr float GrassR21SoilRootBlend = 0.98f;
constexpr float GrassR21DryTransitionHalfWidth = 0.018f;
constexpr float GrassR21ThatchTransitionHalfWidth = 0.020f;
constexpr float GrassR21SoilTransitionHalfWidth = 0.015f;
const FLinearColor GrassR13SubsurfaceAttenuation[] = {
    FLinearColor(0.814f, 0.863f, 0.829f),
    FLinearColor(0.818f, 0.863f, 0.838f),
    FLinearColor(0.839f, 0.883f, 0.876f),
    FLinearColor(0.754f, 0.792f, 0.786f)};
const FLinearColor GrassR15SubsurfaceGain[] = {
    FLinearColor(0.96f, 1.04f, 0.98f),
    FLinearColor(0.97f, 1.06f, 1.00f),
    FLinearColor(1.02f, 1.10f, 1.05f),
    FLinearColor(0.90f, 0.97f, 0.92f)};
// R17 deliberately warms transmission only slightly: this removes the cold,
// waxy shade response without turning the short turf into neon foliage.
const FLinearColor GrassR17SubsurfaceGain[] = {
    FLinearColor(0.985f, 1.045f, 0.955f),
    FLinearColor(0.995f, 1.055f, 0.965f),
    FLinearColor(1.015f, 1.070f, 0.990f),
    FLinearColor(0.925f, 0.985f, 0.900f)};
// R19 keeps the reviewed blade tuples but warms and lifts transmission so
// tree shade reads as living turf rather than saturated emerald velvet.
const FLinearColor GrassR19SubsurfaceGain[] = {
    FLinearColor(1.20f, 1.08f, 0.90f),
    FLinearColor(1.18f, 1.09f, 0.92f),
    FLinearColor(1.23f, 1.11f, 0.98f),
    FLinearColor(1.15f, 1.03f, 0.86f)};
constexpr float GrassR19DryBladeFraction[] = {
    0.045f, 0.030f, 0.065f, 0.160f};
constexpr float GrassR19RootThatchMix[] = {
    0.10f, 0.08f, 0.14f, 0.22f};
const FGrassMaterialProfile R10GrassProfiles[] = {
    {TEXT("MANICURED"), FLinearColor(0.099f, 0.194f, 0.072f), FLinearColor(0.130f, 0.251f, 0.094f), FLinearColor(0.134f, 0.257f, 0.097f), FLinearColor(0.075f, 0.150f, 0.060f), 0.80f, 0.18f, 0.14f},
    {TEXT("HUMID"), FLinearColor(0.083f, 0.184f, 0.072f), FLinearColor(0.118f, 0.241f, 0.093f), FLinearColor(0.121f, 0.247f, 0.095f), FLinearColor(0.068f, 0.143f, 0.058f), 0.79f, 0.18f, 0.18f},
    {TEXT("SHADE"), FLinearColor(0.076f, 0.158f, 0.075f), FLinearColor(0.100f, 0.208f, 0.095f), FLinearColor(0.103f, 0.213f, 0.098f), FLinearColor(0.060f, 0.123f, 0.061f), 0.82f, 0.16f, 0.11f},
    {TEXT("DRY_EDGE"), FLinearColor(0.122f, 0.178f, 0.074f), FLinearColor(0.168f, 0.224f, 0.092f), FLinearColor(0.173f, 0.229f, 0.095f), FLinearColor(0.085f, 0.125f, 0.055f), 0.84f, 0.16f, 0.14f}};
const FGrassMaterialProfile LegacyGrassProfiles[] = {
    {TEXT("MANICURED"), FLinearColor(0.099f, 0.194f, 0.072f), FLinearColor(0.130f, 0.251f, 0.094f), FLinearColor(0.148f, 0.245f, 0.101f), FLinearColor(0.123f, 0.240f, 0.095f), 0.68f, 0.30f, 0.22f},
    {TEXT("HUMID"), FLinearColor(0.083f, 0.184f, 0.072f), FLinearColor(0.118f, 0.241f, 0.093f), FLinearColor(0.134f, 0.236f, 0.097f), FLinearColor(0.113f, 0.226f, 0.093f), 0.66f, 0.30f, 0.31f},
    {TEXT("SHADE"), FLinearColor(0.076f, 0.158f, 0.075f), FLinearColor(0.100f, 0.208f, 0.095f), FLinearColor(0.113f, 0.206f, 0.097f), FLinearColor(0.098f, 0.196f, 0.099f), 0.72f, 0.30f, 0.18f},
    {TEXT("DRY_EDGE"), FLinearColor(0.122f, 0.178f, 0.074f), FLinearColor(0.168f, 0.224f, 0.092f), FLinearColor(0.197f, 0.228f, 0.100f), FLinearColor(0.150f, 0.220f, 0.093f), 0.74f, 0.30f, 0.25f}};
static_assert(
    UE_ARRAY_COUNT(GrassProfiles) == UE_ARRAY_COUNT(GrassAssetNames),
    "V5D grass material names and profiles must remain one-to-one.");
static_assert(
    UE_ARRAY_COUNT(GrassR13SubsurfaceAttenuation) ==
        UE_ARRAY_COUNT(GrassAssetNames),
    "R13 grass subsurface attenuation must remain one-to-one with the four profiles.");
static_assert(
    UE_ARRAY_COUNT(GrassR15SubsurfaceGain) ==
        UE_ARRAY_COUNT(GrassAssetNames),
    "R15 grass subsurface gain must remain one-to-one with the four profiles.");
static_assert(
    UE_ARRAY_COUNT(GrassR17SubsurfaceGain) ==
        UE_ARRAY_COUNT(GrassAssetNames),
    "R17 grass subsurface gain must remain one-to-one with the four profiles.");
static_assert(
    UE_ARRAY_COUNT(GrassR19SubsurfaceGain) ==
        UE_ARRAY_COUNT(GrassAssetNames) &&
        UE_ARRAY_COUNT(GrassR19DryBladeFraction) ==
            UE_ARRAY_COUNT(GrassAssetNames) &&
        UE_ARRAY_COUNT(GrassR19RootThatchMix) ==
            UE_ARRAY_COUNT(GrassAssetNames),
    "R19 photographic turf constants must remain one-to-one with the four profiles.");
static_assert(
    UE_ARRAY_COUNT(GrassR21Profiles) == UE_ARRAY_COUNT(GrassAssetNames) &&
        UE_ARRAY_COUNT(GrassR23Profiles) == UE_ARRAY_COUNT(GrassAssetNames) &&
        UE_ARRAY_COUNT(GrassR23BProfiles) == UE_ARRAY_COUNT(GrassAssetNames) &&
        UE_ARRAY_COUNT(R21PredecessorGrassPins) ==
            UE_ARRAY_COUNT(GrassAssetNames),
    "R21/R23/R23B calibration profiles and exact predecessor pins must remain one-to-one with the four grass packages.");
static_assert(
    UE_ARRAY_COUNT(R22PredecessorPins) == 6 &&
        UE_ARRAY_COUNT(R23PredecessorPins) == 6 &&
        UE_ARRAY_COUNT(R23LayoutMaterialPins) == 6 &&
        UE_ARRAY_COUNT(R23BPredecessorPins) == 6 &&
        UE_ARRAY_COUNT(R23BFinalMaterialPins) == 6,
    "R22/R23/R23B exact predecessor and output pins must remain one-to-one with their six ordered grass-system targets.");
static_assert(
    UE_ARRAY_COUNT(R10GrassProfiles) == UE_ARRAY_COUNT(GrassAssetNames),
    "R10 V5D grass material profiles must remain one-to-one for atomic R12 migration admission.");
static_assert(
    UE_ARRAY_COUNT(LegacyGrassProfiles) == UE_ARRAY_COUNT(GrassAssetNames),
    "Legacy V5D grass material profiles must remain one-to-one for atomic migration admission.");

const FString GrassDescriptionPrefix(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_WORLD_STOCHASTIC_"));
const FString GrassDitherTemporalAaFunctionPath(
    TEXT("/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA.DitherTemporalAA"));
const FString GrassWindDescription(
    TEXT("TRIAD_EXPLORE_V5B_INSTANCE_LOCAL_BERMUDA_WPO_V3_FADE_MATCHED"));
const FString GrassWindCode(
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
constexpr float GrassWindStrengthCm = 0.48f;
constexpr float GrassWindSpeed = 0.12f;
constexpr float GrassWindHeightCm = 4.8f;
constexpr float GrassMaximumWpoCm = 0.55f;
const FLinearColor GrassWindDirection(0.93f, 0.37f, 0.0f, 0.0f);
const FString GrassR13RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R13_STOCHASTIC_ROUGHNESS"));
const FString GrassR13SpecularDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R13_STOCHASTIC_SPECULAR"));
const FString GrassR13NormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R13_DISTANCE_NORMAL_ALPHA"));
const FString GrassR13SubsurfaceAttenuationDescription(
    TEXT("V5B_BERMUDA_R13_SUBSURFACE_ATTENUATION"));
const FString GrassR13SubsurfaceDescription(
    TEXT("V5B_BERMUDA_R13_SUBSURFACE_FROM_BLADE_COLOR"));
const FString GrassR13NormalDescription(
    TEXT("V5B_BERMUDA_R13_NORMALIZED_DISTANCE_MATCHED_NORMAL"));
const FString GrassR13CameraDescription(
    TEXT("V5B_BERMUDA_R13_CAMERA_POSITION"));
const FString GrassR13RoughnessCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float micro=frac(bladeSeed*31.416+instanceSeed*0.7548777);\n")
    TEXT("float rootMask=1.0-smoothstep(0.06,0.28,h);\n")
    TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
    TEXT("return clamp(BaseRoughness+lerp(-0.035,0.040,micro)+0.020*rootMask+0.035*cutMask,0.63,0.84);"));
const FString GrassR13SpecularCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float micro=frac(bladeSeed*17.173+instanceSeed*0.5698403);\n")
    TEXT("return clamp(BaseSpecular*lerp(0.84,1.06,micro),0.20,0.33);"));
const FString GrassR13NormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float distanceWeight=lerp(0.28,0.72,1.0-smoothstep(800.0,2400.0,distanceCm));\n")
    TEXT("return distanceWeight*saturate(InstanceFade);"));
const FString GrassR15RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R15_SHADE_READABLE_ROUGHNESS"));
const FString GrassR15SpecularDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R15_BOUNDED_BLADE_SPECULAR"));
const FString GrassR15NormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R15_PER_BLADE_DISTANCE_NORMAL_ALPHA"));
const FString GrassR15SubsurfaceGainDescription(
    TEXT("V5B_BERMUDA_R15_SUBSURFACE_GAIN"));
const FString GrassR15SubsurfaceDescription(
    TEXT("V5B_BERMUDA_R15_SUBSURFACE_FROM_LIFTED_BLADE_COLOR"));
const FString GrassR15NormalDescription(
    TEXT("V5B_BERMUDA_R15_NORMALIZED_PER_BLADE_DISTANCE_NORMAL"));
const FString GrassR15CameraDescription(
    TEXT("V5B_BERMUDA_R15_CAMERA_POSITION"));
const FString GrassR15RoughnessCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float micro=frac(bladeSeed*31.416+instanceSeed*0.7548777);\n")
    TEXT("float rootMask=1.0-smoothstep(0.06,0.28,h);\n")
    TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
    TEXT("return clamp(BaseRoughness-0.060+lerp(-0.045,0.035,micro)+0.020*rootMask-0.030*cutMask,0.56,0.80);"));
const FString GrassR15SpecularCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float micro=frac(bladeSeed*17.173+instanceSeed*0.5698403);\n")
    TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
    TEXT("return clamp(BaseSpecular*lerp(0.92,1.18,micro)+0.045*cutMask,0.24,0.40);"));
const FString GrassR15NormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float nearWeight=1.0-smoothstep(800.0,2400.0,distanceCm);\n")
    TEXT("float baseWeight=lerp(0.32,0.82,nearWeight);\n")
    TEXT("float bladeWeight=lerp(0.86,1.14,micro);\n")
    TEXT("return saturate(baseWeight*bladeWeight*saturate(InstanceFade));"));
const FString GrassR17RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R17_MANAGED_TURF_ROUGHNESS"));
const FString GrassR17SpecularDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R17_RESTRAINED_BLADE_SPECULAR"));
const FString GrassR17NormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R17_PER_BLADE_DISTANCE_NORMAL_ALPHA"));
const FString GrassR17SubsurfaceGainDescription(
    TEXT("V5B_BERMUDA_R17_WARM_RESTRAINED_SUBSURFACE_GAIN"));
const FString GrassR17SubsurfaceDescription(
    TEXT("V5B_BERMUDA_R17_SUBSURFACE_FROM_PHOTOGRAPHIC_BLADE_COLOR"));
const FString GrassR17NormalDescription(
    TEXT("V5B_BERMUDA_R17_NORMALIZED_PER_BLADE_DISTANCE_NORMAL"));
const FString GrassR17CameraDescription(
    TEXT("V5B_BERMUDA_R17_CAMERA_POSITION"));
const FString GrassR17RoughnessCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float micro=frac(bladeSeed*31.416+instanceSeed*0.7548777);\n")
    TEXT("float rootMask=1.0-smoothstep(0.06,0.28,h);\n")
    TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
    TEXT("return clamp(BaseRoughness-0.020+lerp(-0.035,0.040,micro)+0.020*rootMask+0.015*cutMask,0.63,0.84);"));
const FString GrassR17SpecularCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float micro=frac(bladeSeed*17.173+instanceSeed*0.5698403);\n")
    TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
    TEXT("return clamp(BaseSpecular*lerp(0.86,1.08,micro)+0.015*cutMask,0.20,0.33);"));
const FString GrassR17NormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float nearWeight=1.0-smoothstep(800.0,2400.0,distanceCm);\n")
    TEXT("float baseWeight=lerp(0.20,0.66,nearWeight);\n")
    TEXT("float bladeWeight=lerp(0.90,1.10,micro);\n")
    TEXT("return saturate(baseWeight*bladeWeight*saturate(InstanceFade));"));
const FString GrassR19RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R19_BROAD_BLADE_ROUGHNESS"));
const FString GrassR19SpecularDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R19_RESTRAINED_LOW_GLINT_SPECULAR"));
const FString GrassR19NormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R19_DISTANCE_ANTI_MOIRE_NORMAL_ALPHA"));
const FString GrassR19SubsurfaceGainDescription(
    TEXT("V5B_BERMUDA_R19_WARM_DESATURATED_SUBSURFACE_GAIN"));
const FString GrassR19SubsurfaceDescription(
    TEXT("V5B_BERMUDA_R19_SUBSURFACE_FROM_PHOTOGRAPHIC_BLADE_COLOR"));
const FString GrassR19NormalDescription(
    TEXT("V5B_BERMUDA_R19_NORMALIZED_DISTANCE_ANTI_MOIRE_NORMAL"));
const FString GrassR19CameraDescription(
    TEXT("V5B_BERMUDA_R19_CAMERA_POSITION"));
const FString GrassR19RoughnessCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(800.0,2000.0,distanceCm);\n")
    TEXT("float micro=frac(bladeSeed*31.416+instanceSeed*0.7548777);\n")
    TEXT("float rootMask=1.0-smoothstep(0.06,0.30,h);\n")
    TEXT("float cutMask=smoothstep(0.82,1.0,h);\n")
    TEXT("return clamp(BaseRoughness+0.035+detail*lerp(-0.012,0.018,micro)+0.025*rootMask+0.030*cutMask,0.70,0.90);"));
const FString GrassR19SpecularCode(
    TEXT("float bladeSeed=saturate(BladeUV.x);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float instanceSeed=saturate(Random01);\n")
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(800.0,2000.0,distanceCm);\n")
    TEXT("float micro=frac(bladeSeed*17.173+instanceSeed*0.5698403);\n")
    TEXT("float filteredMicro=lerp(0.5,micro,detail);\n")
    TEXT("float cutMask=smoothstep(0.82,1.0,h);\n")
    TEXT("return clamp(BaseSpecular*lerp(0.62,0.72,filteredMicro)+0.003*cutMask,0.16,0.23);"));
const FString GrassR19NormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(800.0,2000.0,distanceCm);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float filteredMicro=lerp(0.5,micro,detail);\n")
    TEXT("float baseWeight=lerp(0.03,0.50,detail);\n")
    TEXT("float bladeWeight=lerp(0.96,1.04,filteredMicro);\n")
    TEXT("return saturate(baseWeight*bladeWeight*saturate(InstanceFade));"));
const FString GrassR21RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R21_DISTANCE_COLLAPSED_ROUGHNESS"));
const FString GrassR21SpecularDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R21_DISTANCE_COLLAPSED_SPECULAR"));
const FString GrassR21NormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R21_DISTANCE_COLLAPSED_NORMAL_ALPHA"));
const FString GrassR21SubsurfaceGainDescription(
    TEXT("V5B_BERMUDA_R21_WARM_CALIBRATED_SUBSURFACE_GAIN"));
const FString GrassR21SubsurfaceDescription(
    TEXT("V5B_BERMUDA_R21_SUBSURFACE_FROM_CALIBRATED_BLADE_COLOR"));
const FString GrassR21NormalDescription(
    TEXT("V5B_BERMUDA_R21_NORMALIZED_DISTANCE_COLLAPSED_NORMAL"));
const FString GrassR21CameraDescription(
    TEXT("V5B_BERMUDA_R21_CAMERA_POSITION"));
const FString GrassR21RoughnessCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(500.0,1400.0,distanceCm);\n")
    TEXT("return lerp(0.74,BaseRoughness,detail);"));
const FString GrassR21SpecularCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(500.0,1400.0,distanceCm);\n")
    TEXT("return lerp(0.19,BaseSpecular,detail);"));
const FString GrassR21NormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(400.0,1200.0,distanceCm);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float bladeWeight=lerp(0.95,1.05,micro);\n")
    TEXT("return saturate(0.42*detail*bladeWeight*saturate(InstanceFade));"));
const FString GrassR23RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23_PHOTOGRAPHIC_DEPTH_ROUGHNESS"));
const FString GrassR23SpecularDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23_FILTERED_LOW_GLINT_SPECULAR"));
const FString GrassR23NormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23_12M_24M_DEPTH_NORMAL_ALPHA"));
const FString GrassR23SubsurfaceGainDescription(
    TEXT("V5B_BERMUDA_R23_NATURAL_TURF_SUBSURFACE_GAIN"));
const FString GrassR23SubsurfaceDescription(
    TEXT("V5B_BERMUDA_R23_SUBSURFACE_FROM_NATURAL_BLADE_COLOR"));
const FString GrassR23NormalDescription(
    TEXT("V5B_BERMUDA_R23_NORMALIZED_DEPTH_PRESERVING_NORMAL"));
const FString GrassR23CameraDescription(
    TEXT("V5B_BERMUDA_R23_CAMERA_POSITION"));
const FString GrassR23RoughnessCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1000.0,2400.0,distanceCm);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*31.416+saturate(Random01)*0.7548777);\n")
    TEXT("float rootMask=1.0-smoothstep(0.05,0.34,h);\n")
    TEXT("float cutMask=smoothstep(0.82,1.0,h);\n")
    TEXT("float nearRough=BaseRoughness+lerp(-0.025,0.030,micro)+0.035*rootMask+0.018*cutMask;\n")
    TEXT("return clamp(lerp(0.78,nearRough,detail),0.65,0.86);"));
const FString GrassR23SpecularCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1000.0,2400.0,distanceCm);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float nearSpec=BaseSpecular*lerp(0.72,0.92,micro);\n")
    TEXT("return clamp(lerp(0.17,nearSpec,detail),0.15,0.29);"));
const FString GrassR23NormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1200.0,2400.0,distanceCm);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float bladeWeight=lerp(0.92,1.08,micro);\n")
    TEXT("float heightWeight=lerp(0.78,1.0,smoothstep(0.08,0.62,saturate(BladeUV.y)));\n")
    TEXT("return saturate((0.08+0.42*detail)*bladeWeight*heightWeight*saturate(InstanceFade));"));
const FString GrassR23BRoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23B_PHOTOGRAPHIC_DEPTH_ROUGHNESS"));
const FString GrassR23BSpecularDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23B_FILTERED_LOW_GLINT_SPECULAR"));
const FString GrassR23BNormalAlphaDescription(
    TEXT("TRIAD_EXPLORE_V5D_GRASS_R23B_12M_24M_DEPTH_NORMAL_ALPHA"));
const FString GrassR23BSubsurfaceGainDescription(
    TEXT("V5B_BERMUDA_R23B_NATURAL_TURF_SUBSURFACE_GAIN"));
const FString GrassR23BSubsurfaceDescription(
    TEXT("V5B_BERMUDA_R23B_SUBSURFACE_FROM_NATURAL_BLADE_COLOR"));
const FString GrassR23BNormalDescription(
    TEXT("V5B_BERMUDA_R23B_NORMALIZED_DEPTH_PRESERVING_NORMAL"));
const FString GrassR23BCameraDescription(
    TEXT("V5B_BERMUDA_R23B_CAMERA_POSITION"));
const FString GrassR23BRoughnessCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1000.0,2400.0,distanceCm);\n")
    TEXT("float h=saturate(BladeUV.y);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*31.416+saturate(Random01)*0.7548777);\n")
    TEXT("float rootMask=1.0-smoothstep(0.05,0.34,h);\n")
    TEXT("float cutMask=smoothstep(0.82,1.0,h);\n")
    TEXT("float nearRough=BaseRoughness+lerp(-0.018,0.022,micro)+0.035*rootMask+0.018*cutMask;\n")
    TEXT("return clamp(lerp(0.80,nearRough,detail),0.68,0.88);"));
const FString GrassR23BSpecularCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1000.0,2400.0,distanceCm);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float nearSpec=BaseSpecular*lerp(0.68,0.86,micro);\n")
    TEXT("return clamp(lerp(0.15,nearSpec,detail),0.12,0.25);"));
const FString GrassR23BNormalAlphaCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("float detail=1.0-smoothstep(1200.0,2400.0,distanceCm);\n")
    TEXT("float micro=frac(saturate(BladeUV.x)*17.173+saturate(Random01)*0.5698403);\n")
    TEXT("float bladeWeight=lerp(0.95,1.05,micro);\n")
    TEXT("float heightWeight=lerp(0.78,1.0,smoothstep(0.08,0.62,saturate(BladeUV.y)));\n")
    TEXT("return saturate((0.04+0.34*detail)*bladeWeight*heightWeight*saturate(InstanceFade));"));
const FString SoilBaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_SOIL_WORLD_STOCHASTIC_BASE_V1"));
const FString SoilRoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_SOIL_WORLD_STOCHASTIC_ROUGHNESS_V1"));
const FString SoilNormalDescription(
    TEXT("TRIAD_EXPLORE_V5D_SOIL_WORLD_STOCHASTIC_NORMAL_V1"));
const FString GroundMacroDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_WORLD_STOCHASTIC_CHROMA_ROUGHNESS_V1"));
const FString GroundR15CoreMaskDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_IRREGULAR_ELLIPSE_64_VERTEX_OUTWARD_DITHER_MASK_V4"));
const FString GroundCoreMaskDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_IRREGULAR_ELLIPSE_64_VERTEX_50M_OPAQUE_COLLAR_OUTWARD_DITHER_MASK_V5"));
const FString GroundMacroCenteredNode(
    TEXT("TRIAD.V5D.LawnMacroCentered"));
const FString GroundMacroAmplitudeNode(
    TEXT("TRIAD.V5D.LawnMacroRoughnessAmplitude"));
const FString GroundMacroVariationNode(
    TEXT("TRIAD.V5D.LawnMacroRoughnessVariation"));
const FString GroundRoughnessCombinedNode(
    TEXT("TRIAD.V5D.LawnRoughnessWithMacroVariation"));
const FLinearColor GroundLawnTint(1.100f, 1.220f, 1.140f, 1.0f);
const FLinearColor GroundMacroTintLow(0.950f, 1.010f, 1.030f, 1.0f);
const FLinearColor GroundMacroTintHigh(1.120f, 1.170f, 1.090f, 1.0f);
constexpr float GroundDesaturation = 0.30f;
constexpr float GroundFarMicroNormalStrength = 0.11f;
constexpr float GroundRoughnessBias = 0.0f;
constexpr float GroundMacroRoughnessAmplitude = 0.12f;

const FString GroundMacroCode(
    TEXT("float2 worldM = WorldPosition.xy * 0.01;\n")
    TEXT("float2 p = (worldM + float2(37.1,-21.7)) / 17.0;\n")
    TEXT("float2 i = floor(p); float2 f = frac(p); f=f*f*(3.0-2.0*f);\n")
    TEXT("float4 h=frac(sin(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+float2(1,1),float2(127.1,311.7))))*43758.5453);\n")
    TEXT("float broad=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);\n")
    TEXT("float2 warp=float2(sin(worldM.y*0.041+broad*2.3),cos(worldM.x*0.037-broad*1.9))*1.7;\n")
    TEXT("float2 q=(worldM+warp+float2(-8.3,15.9))/3.2;\n")
    TEXT("float2 j=floor(q); float2 g=frac(q); g=g*g*(3.0-2.0*g);\n")
    TEXT("float4 k=frac(sin(float4(dot(j,float2(269.5,183.3)),dot(j+float2(1,0),float2(269.5,183.3)),dot(j+float2(0,1),float2(269.5,183.3)),dot(j+float2(1,1),float2(269.5,183.3))))*43758.5453);\n")
    TEXT("float medium=lerp(lerp(k.x,k.y,g.x),lerp(k.z,k.w,g.x),g.y);\n")
    TEXT("float2 r=(worldM+float2(91.7,43.1))/43.0;\n")
    TEXT("float2 l=floor(r); float2 m=frac(r); m=m*m*(3.0-2.0*m);\n")
    TEXT("float4 n=frac(sin(float4(dot(l,float2(419.2,371.9)),dot(l+float2(1,0),float2(419.2,371.9)),dot(l+float2(0,1),float2(419.2,371.9)),dot(l+float2(1,1),float2(419.2,371.9))))*24634.6345);\n")
    TEXT("float veryBroad=lerp(lerp(n.x,n.y,m.x),lerp(n.z,n.w,m.x),m.y);\n")
    TEXT("float heroX=1.0-smoothstep(49.0,55.0,abs(worldM.x));\n")
    TEXT("float heroY=smoothstep(22.0,28.0,worldM.y)*(1.0-smoothstep(172.0,178.0,worldM.y));\n")
    TEXT("float axis=1.0-smoothstep(14.0,22.0,abs(worldM.x));\n")
    TEXT("float protection=saturate(max(0.86*heroX*heroY,0.72*axis));\n")
    TEXT("float amplitude=lerp(1.0,0.50,protection);\n")
    TEXT("float rawField=0.40*(veryBroad-0.5)+0.42*(broad-0.5)+0.18*(medium-0.5);\n")
    TEXT("float field=0.5+amplitude*1.32*rawField;\n")
    TEXT("return saturate(field + MacroUV.x*0.0);"));

const FString GroundR10FieldDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R10_COHERENT_11M_48M_MOW_WEAR_WET_FIELD_V2"));
const FString GroundR10SurfaceUvDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R10_WORLD_METER_SURFACE_UV_V2"));
const FString GroundR10BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R10_BOUNDED_BASE_V2"));
const FString GroundR10RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R10_BOUNDED_ROUGHNESS_V2"));
const FString GroundR10AoDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R10_DISTANCE_FADED_AO_V2"));
const FString GroundR10FarFadeDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R10_30M_55M_DETAIL_FADE_V2"));
const FString GroundR12BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R12_PHOTOGRAPHIC_BASE_V3"));
const FString GroundR12RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R12_PHOTOGRAPHIC_ROUGHNESS_V3"));
const FString GroundR13BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R13_GRASS004_PHOTOGRAPHIC_BASE_V4"));
const FString GroundR13RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R13_GRASS004_PHOTOGRAPHIC_ROUGHNESS_V4"));
const FString GroundR15BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R15_GRASS004_SHADE_READABLE_BASE_V1"));
const FString GroundR15RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R15_GRASS004_ROUGHNESS_V1"));
const FString GroundR17BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R17_WARM_MANAGED_GRASS004_BASE_V1"));
const FString GroundR17RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R17_MANAGED_GRASS004_ROUGHNESS_V1"));
const FString GroundR18BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R18_CALIBRATED_MANAGED_GRASS004_BASE_V1"));
const FString GroundR22BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R22_HYPERREAL_NEAR_WARM_FAR_FILTERED_BASE_V1"));
const FString GroundR22RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R22_DISTANCE_FILTERED_ROUGHNESS_V1"));
const FString GroundR22FarFadeDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R22_30M_55M_MIP_BIAS_FADE_V1"));
const FString GroundR22MipBiasDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R22_DISTANCE_MIP_BIAS_V1"));
const FString GroundR23BaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R23_NATURAL_MULTI_SCALE_SUBSTRATE_BASE_V1"));
const FString GroundR23RoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R23_PHOTOGRAPHIC_DEPTH_ROUGHNESS_V1"));
const FString GroundR23BBaseDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R23B_BALANCED_MULTI_SCALE_SUBSTRATE_BASE_V1"));
const FString GroundR23BRoughnessDescription(
    TEXT("TRIAD_EXPLORE_V5D_LAWN_R23B_PHOTOGRAPHIC_DEPTH_ROUGHNESS_V1"));
constexpr float GroundR10TextureTileMeters = 1.40f;
constexpr float GroundR10MowingBandMeters = 3.20f;
constexpr float GroundR10MowingAngleDegrees = 7.0f;
constexpr float GroundR10LowFrequencyMeters = 11.0f;
constexpr float GroundR10BroadFrequencyMeters = 48.0f;
constexpr float GroundR10DetailFadeStartCm = 3000.0f;
constexpr float GroundR10DetailFadeEndCm = 5500.0f;
constexpr float GroundR10NearNormalStrength = 0.32f;
constexpr float GroundR10FarNormalStrength = 0.03f;
constexpr float GroundR10Specular = 0.16f;
constexpr float GroundR10MaximumWear = 0.18f;
constexpr float GroundR10MaximumWetness = 0.12f;
constexpr float GroundR10MowingRoughnessAmplitude = 0.025f;
constexpr float GroundR12NearColorDetailPresence = 0.82f;
constexpr float GroundR12FarColorDetailPresence = 0.38f;
constexpr float GroundR12NearNormalStrength = 0.24f;
constexpr float GroundR12FarNormalStrength = 0.03f;
constexpr float GroundR12Specular = 0.28f;
constexpr float GroundR13NearColorDetailPresence = 0.82f;
constexpr float GroundR13FarColorDetailPresence = 0.38f;
constexpr float GroundR13NearNormalStrength = 0.14f;
constexpr float GroundR13FarNormalStrength = 0.01f;
constexpr float GroundR13Specular = 0.18f;
constexpr float GroundR15NearNormalStrength = 0.20f;
constexpr float GroundR15FarNormalStrength = 0.02f;
constexpr float GroundR15Specular = 0.22f;
constexpr float GroundR17NearNormalStrength = 0.15f;
constexpr float GroundR17FarNormalStrength = 0.01f;
constexpr float GroundR17Specular = 0.18f;
constexpr float GroundR22NearNormalStrength = 0.12f;
constexpr float GroundR22FarNormalStrength = 0.0f;
constexpr float GroundR22Specular = 0.14f;
constexpr float GroundR23NearNormalStrength = 0.22f;
constexpr float GroundR23FarNormalStrength = 0.02f;
constexpr float GroundR23Specular = 0.18f;
constexpr float GroundR23BNearNormalStrength = 0.17f;
constexpr float GroundR23BFarNormalStrength = 0.01f;
constexpr float GroundR23BSpecular = 0.15f;

const FString GroundR10SurfaceUvCode(
    TEXT("return WorldPosition.xy/(max(TileMeters,0.01)*100.0);"));

const FString GroundR10FieldCode(
    TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
    TEXT("float2 p11=(worldM+float2(13.7,-7.9))/11.0;\n")
    TEXT("float2 i11=floor(p11); float2 f11=frac(p11); f11=f11*f11*(3.0-2.0*f11);\n")
    TEXT("float4 h11=frac(float4(dot(i11,float2(127.1,311.7)),dot(i11+float2(1,0),float2(127.1,311.7)),dot(i11+float2(0,1),float2(127.1,311.7)),dot(i11+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
    TEXT("h11=frac(h11*(h11+33.33)*(h11+h11+17.17));\n")
    TEXT("float n11=lerp(lerp(h11.x,h11.y,f11.x),lerp(h11.z,h11.w,f11.x),f11.y);\n")
    TEXT("float2 p48=(worldM+float2(-31.3,19.1))/48.0;\n")
    TEXT("float2 i48=floor(p48); float2 f48=frac(p48); f48=f48*f48*(3.0-2.0*f48);\n")
    TEXT("float4 h48=frac(float4(dot(i48,float2(269.5,183.3)),dot(i48+float2(1,0),float2(269.5,183.3)),dot(i48+float2(0,1),float2(269.5,183.3)),dot(i48+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
    TEXT("h48=frac(h48*(h48+29.71)*(h48+h48+11.13));\n")
    TEXT("float n48=lerp(lerp(h48.x,h48.y,f48.x),lerp(h48.z,h48.w,f48.x),f48.y);\n")
    TEXT("float2 mowingAxis=float2(0.99254615164,0.12186934341);\n")
    TEXT("float mowing=0.5+0.5*sin(6.28318530718*dot(worldM,mowingAxis)/3.2);\n")
    TEXT("float macro=saturate(0.5+0.34*(n11-0.5)+0.24*(n48-0.5)+0.030*(mowing-0.5));\n")
    TEXT("float wear=clamp(0.04+0.10*(1.0-n11)+0.04*mowing,0.0,0.18);\n")
    TEXT("float wetness=clamp(0.02+0.08*n48+0.02*(1.0-mowing),0.0,0.12);\n")
    TEXT("return float4(macro,mowing,wear,wetness);"));

const FString GroundR10BaseCode(
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.38,1.0,saturate(NearDetail));\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(Macro));\n")
    TEXT("float mowingGain=lerp(0.985,1.015,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 wearSignature=lerp(resolved,dot(resolved,float3(0.212639,0.715169,0.072192)).xxx*float3(1.035,1.000,0.945),0.16*wearAmount);\n")
    TEXT("float3 wetCool=wearSignature*lerp(float3(1,1,1),float3(0.975,0.990,1.015),wetAmount);\n")
    TEXT("float wearGain=lerp(1.0,0.985,wearAmount);\n")
    TEXT("float wetGain=lerp(1.0,0.955,wetAmount);\n")
    TEXT("return saturate(wetCool*float3(1.050,1.100,1.060)*macroGain*mowingGain*wearGain*wetGain);"));

const FString GroundR10RoughnessCode(FString::Printf(
    TEXT("float nearRough=saturate(TextureRoughness+0.10);\n")
    TEXT("float base=lerp(0.86,nearRough,saturate(NearDetail));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("return clamp(base+0.05*(Macro-0.5)+%.6f*(0.5-Mowing)+0.05*wearAmount-0.06*wetAmount,0.76,0.96);"),
    2.0f * GroundR10MowingRoughnessAmplitude));

const FString GroundR12BaseCode(
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.38,0.82,saturate(NearDetail));\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(Macro));\n")
    TEXT("float mowingGain=lerp(0.985,1.015,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 wearSignature=lerp(resolved,dot(resolved,float3(0.212639,0.715169,0.072192)).xxx*float3(1.035,1.000,0.945),0.16*wearAmount);\n")
    TEXT("float3 wetCool=wearSignature*lerp(float3(1,1,1),float3(0.975,0.990,1.015),wetAmount);\n")
    TEXT("float wearGain=lerp(1.0,0.985,wearAmount);\n")
    TEXT("float wetGain=lerp(1.0,0.955,wetAmount);\n")
    TEXT("return saturate(wetCool*float3(1.085,1.060,1.075)*macroGain*mowingGain*wearGain*wetGain);"));

const FString GroundR12RoughnessCode(FString::Printf(
    TEXT("float nearRough=saturate(TextureRoughness+0.06);\n")
    TEXT("float base=lerp(0.84,nearRough,saturate(NearDetail));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("return clamp(base+0.05*(Macro-0.5)+%.6f*(0.5-Mowing)+0.05*wearAmount-0.06*wetAmount,0.70,0.93);"),
    2.0f * GroundR10MowingRoughnessAmplitude));

const FString GroundR13BaseCode(
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.38,0.82,saturate(NearDetail));\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(Macro));\n")
    TEXT("float mowingGain=lerp(0.985,1.015,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 wearSignature=lerp(resolved,dot(resolved,float3(0.212639,0.715169,0.072192)).xxx*float3(1.035,1.000,0.945),0.16*wearAmount);\n")
    TEXT("float3 wetCool=wearSignature*lerp(float3(1,1,1),float3(0.975,0.990,1.015),wetAmount);\n")
    TEXT("float wearGain=lerp(1.0,0.985,wearAmount);\n")
    TEXT("float wetGain=lerp(1.0,0.955,wetAmount);\n")
    TEXT("return saturate(wetCool*float3(0.561,0.722,0.711)*macroGain*mowingGain*wearGain*wetGain);"));

const FString GroundR13RoughnessCode(FString::Printf(
    TEXT("float nearRough=0.63+0.50*saturate(TextureRoughness);\n")
    TEXT("float base=lerp(0.84,nearRough,saturate(NearDetail));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("return clamp(base+0.05*(Macro-0.5)+%.6f*(0.5-Mowing)+0.05*wearAmount-0.06*wetAmount,0.70,0.91);"),
    2.0f * GroundR10MowingRoughnessAmplitude));

const FString GroundR15BaseCode(
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.38,0.82,saturate(NearDetail));\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(Macro));\n")
    TEXT("float mowingGain=lerp(0.985,1.015,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 wearSignature=lerp(resolved,dot(resolved,float3(0.212639,0.715169,0.072192)).xxx*float3(1.035,1.000,0.945),0.16*wearAmount);\n")
    TEXT("float3 wetCool=wearSignature*lerp(float3(1,1,1),float3(0.975,0.990,1.015),wetAmount);\n")
    TEXT("float wearGain=lerp(1.0,0.985,wearAmount);\n")
    TEXT("float wetGain=lerp(1.0,0.955,wetAmount);\n")
    TEXT("return saturate(wetCool*float3(0.640,0.805,0.765)*macroGain*mowingGain*wearGain*wetGain);"));

const FString GroundR15RoughnessCode(GroundR13RoughnessCode);
const FString GroundR17BaseCode(
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.38,0.82,saturate(NearDetail));\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(Macro));\n")
    TEXT("float mowingGain=lerp(0.985,1.015,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 wearSignature=lerp(resolved,dot(resolved,float3(0.212639,0.715169,0.072192)).xxx*float3(1.035,1.000,0.945),0.16*wearAmount);\n")
    TEXT("float3 wetCool=wearSignature*lerp(float3(1,1,1),float3(0.975,0.990,1.015),wetAmount);\n")
    TEXT("float wearGain=lerp(1.0,0.985,wearAmount);\n")
    TEXT("float wetGain=lerp(1.0,0.955,wetAmount);\n")
    TEXT("return saturate(wetCool*float3(0.700,0.900,0.620)*macroGain*mowingGain*wearGain*wetGain);"));
const FString GroundR17RoughnessCode(GroundR13RoughnessCode);
const FString GroundR18BaseCode(
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.38,0.82,saturate(NearDetail));\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(Macro));\n")
    TEXT("float mowingGain=lerp(0.985,1.015,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 wearSignature=lerp(resolved,dot(resolved,float3(0.212639,0.715169,0.072192)).xxx*float3(1.035,1.000,0.945),0.16*wearAmount);\n")
    TEXT("float3 wetCool=wearSignature*lerp(float3(1,1,1),float3(0.975,0.990,1.015),wetAmount);\n")
    TEXT("float wearGain=lerp(1.0,0.985,wearAmount);\n")
    TEXT("float wetGain=lerp(1.0,0.955,wetAmount);\n")
    TEXT("return saturate(wetCool*float3(0.715,0.965,0.595)*macroGain*mowingGain*wearGain*wetGain);"));

// R22 is calibrated against the exact R21C two-metre pixels. It warms only
// the ground-level response, returns to the proven R18 tint for elevated
// views, and leaves distance-frequency control to the explicit mip-bias node.
const FString GroundR22BaseCode(
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.38,0.82,saturate(NearDetail));\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(Macro));\n")
    TEXT("float mowingGain=lerp(0.985,1.015,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 wearSignature=lerp(resolved,dot(resolved,float3(0.212639,0.715169,0.072192)).xxx*float3(1.035,1.000,0.945),0.16*wearAmount);\n")
    TEXT("float3 wetCool=wearSignature*lerp(float3(1,1,1),float3(0.975,0.990,1.015),wetAmount);\n")
    TEXT("float wearGain=lerp(1.0,0.985,wearAmount);\n")
    TEXT("float wetGain=lerp(1.0,0.955,wetAmount);\n")
    TEXT("float cameraHeightDeltaCm=abs(CameraPosition.z-WorldPosition.z);\n")
    TEXT("float warm=1.0-smoothstep(5000.0,15000.0,cameraHeightDeltaCm);\n")
    TEXT("float3 calibratedTint=lerp(float3(0.715,0.965,0.595),float3(1.96625,0.91675,0.56525),warm);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/22.0);\n")
    TEXT("return saturate(wetCool*calibratedTint*macroGain*mowingGain*wearGain*wetGain*revisionGate);"));

const FString GroundR22RoughnessCode(
    TEXT("float nearDetail=saturate(NearDetail);\n")
    TEXT("float nearRough=0.63+0.50*saturate(TextureRoughness);\n")
    TEXT("float base=lerp(0.88,nearRough,nearDetail);\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float detailDelta=0.04*(Macro-0.5)+0.03*(0.5-Mowing)+0.04*wearAmount-0.05*wetAmount;\n")
    TEXT("return clamp(base+nearDetail*detailDelta,0.78,0.94);"));

// R23 removes the camera-height keyed orange multiplier and derives bounded,
// filtered turf/thatch/substrate breakup entirely from stable world space.
const FString GroundR23BaseCode(
    TEXT("float nearDetail=saturate(NearDetail);\n")
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.46,0.92,nearDetail);\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
    TEXT("float2 p=(worldM+float2(19.7,-11.3))/7.3;\n")
    TEXT("float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f);\n")
    TEXT("float4 h=frac(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
    TEXT("h=frac(h*(h+33.33)*(h+h+17.17));\n")
    TEXT("float broad=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);\n")
    TEXT("float2 q=(worldM+float2(-4.9,8.7)+float2(broad-0.5,0.5-broad)*0.8)/1.7;\n")
    TEXT("float2 j=floor(q); float2 g=frac(q); g=g*g*(3.0-2.0*g);\n")
    TEXT("float4 k=frac(float4(dot(j,float2(269.5,183.3)),dot(j+float2(1,0),float2(269.5,183.3)),dot(j+float2(0,1),float2(269.5,183.3)),dot(j+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
    TEXT("k=frac(k*(k+19.19)*(k+k+7.77));\n")
    TEXT("float meso=lerp(lerp(k.x,k.y,g.x),lerp(k.z,k.w,g.x),g.y);\n")
    TEXT("float2 r=(worldM+float2(2.3,-6.1))/0.48;\n")
    TEXT("float2 l=floor(r); float2 m=frac(r); m=m*m*(3.0-2.0*m);\n")
    TEXT("float4 n=frac(float4(dot(l,float2(419.2,371.9)),dot(l+float2(1,0),float2(419.2,371.9)),dot(l+float2(0,1),float2(419.2,371.9)),dot(l+float2(1,1),float2(419.2,371.9)))*0.0897);\n")
    TEXT("n=frac(n*(n+27.17)*(n+n+13.31));\n")
    TEXT("float micro=lerp(lerp(n.x,n.y,m.x),lerp(n.z,n.w,m.x),m.y);\n")
    TEXT("float organic=saturate(0.50*broad+0.34*meso+0.16*micro);\n")
    TEXT("float macroGain=lerp(0.94,1.06,saturate(0.62*Macro+0.38*organic));\n")
    TEXT("float mowingGain=lerp(0.992,1.008,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 naturalTint=lerp(float3(0.76,1.02,0.68),float3(0.78,1.10,0.70),nearDetail);\n")
    TEXT("float3 turf=resolved*naturalTint*macroGain*mowingGain;\n")
    TEXT("float thatchMask=smoothstep(0.78,0.94,saturate(0.58*meso+0.42*micro))*nearDetail;\n")
    TEXT("float soilMask=smoothstep(0.91,0.985,saturate(0.70*micro+0.30*(1.0-broad)))*nearDetail*(1.0-0.65*thatchMask);\n")
    TEXT("turf=lerp(turf,float3(0.100,0.075,0.035),0.12*thatchMask);\n")
    TEXT("turf=lerp(turf,float3(0.045,0.035,0.022),0.22*soilMask);\n")
    TEXT("float microGain=lerp(0.95,1.05,micro);\n")
    TEXT("turf*=lerp(1.0,microGain,nearDetail);\n")
    TEXT("turf*=lerp(1.0,0.985,wearAmount)*lerp(1.0,0.965,wetAmount);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/23.0);\n")
    TEXT("return saturate(turf*revisionGate);"));

const FString GroundR23RoughnessCode(
    TEXT("float nearDetail=saturate(NearDetail);\n")
    TEXT("float nearRough=0.58+0.46*saturate(TextureRoughness);\n")
    TEXT("float base=lerp(0.84,nearRough,nearDetail);\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float detailDelta=0.065*(Macro-0.5)+0.028*(0.5-Mowing)+0.035*wearAmount-0.055*wetAmount;\n")
    TEXT("return clamp(base+nearDetail*detailDelta,0.68,0.91);"));

const FString GroundR23BBaseCode(
    TEXT("float nearDetail=saturate(NearDetail);\n")
    TEXT("float luminance=dot(BaseColor,float3(0.212639,0.715169,0.072192));\n")
    TEXT("float detailPresence=lerp(0.46,0.92,nearDetail);\n")
    TEXT("float3 resolved=lerp(luminance.xxx,BaseColor,detailPresence);\n")
    TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
    TEXT("float2 p=(worldM+float2(19.7,-11.3))/7.3;\n")
    TEXT("float2 i=floor(p); float2 f=frac(p); f=f*f*(3.0-2.0*f);\n")
    TEXT("float4 h=frac(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
    TEXT("h=frac(h*(h+33.33)*(h+h+17.17));\n")
    TEXT("float broad=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);\n")
    TEXT("float2 q=(worldM+float2(-4.9,8.7)+float2(broad-0.5,0.5-broad)*0.8)/1.7;\n")
    TEXT("float2 j=floor(q); float2 g=frac(q); g=g*g*(3.0-2.0*g);\n")
    TEXT("float4 k=frac(float4(dot(j,float2(269.5,183.3)),dot(j+float2(1,0),float2(269.5,183.3)),dot(j+float2(0,1),float2(269.5,183.3)),dot(j+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
    TEXT("k=frac(k*(k+19.19)*(k+k+7.77));\n")
    TEXT("float meso=lerp(lerp(k.x,k.y,g.x),lerp(k.z,k.w,g.x),g.y);\n")
    TEXT("float2 r=(worldM+float2(2.3,-6.1))/0.48;\n")
    TEXT("float2 l=floor(r); float2 m=frac(r); m=m*m*(3.0-2.0*m);\n")
    TEXT("float4 n=frac(float4(dot(l,float2(419.2,371.9)),dot(l+float2(1,0),float2(419.2,371.9)),dot(l+float2(0,1),float2(419.2,371.9)),dot(l+float2(1,1),float2(419.2,371.9)))*0.0897);\n")
    TEXT("n=frac(n*(n+27.17)*(n+n+13.31));\n")
    TEXT("float micro=lerp(lerp(n.x,n.y,m.x),lerp(n.z,n.w,m.x),m.y);\n")
    TEXT("float organic=saturate(0.50*broad+0.34*meso+0.16*micro);\n")
    TEXT("float macroGain=lerp(0.965,1.035,saturate(0.62*Macro+0.38*organic));\n")
    TEXT("float mowingGain=lerp(0.996,1.004,saturate(Mowing));\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float3 naturalTint=lerp(float3(1.02,0.86,0.58),float3(1.06,0.90,0.60),nearDetail);\n")
    TEXT("float3 turf=resolved*naturalTint*macroGain*mowingGain;\n")
    TEXT("float thatchMask=smoothstep(0.72,0.90,saturate(0.58*meso+0.42*micro))*nearDetail;\n")
    TEXT("float soilMask=smoothstep(0.88,0.975,saturate(0.70*micro+0.30*(1.0-broad)))*nearDetail*(1.0-0.65*thatchMask);\n")
    TEXT("turf=lerp(turf,float3(0.115,0.078,0.032),0.18*thatchMask);\n")
    TEXT("turf=lerp(turf,float3(0.050,0.034,0.018),0.26*soilMask);\n")
    TEXT("float microGain=lerp(0.975,1.025,micro);\n")
    TEXT("turf*=lerp(1.0,microGain,nearDetail);\n")
    TEXT("turf*=lerp(1.0,0.985,wearAmount)*lerp(1.0,0.965,wetAmount);\n")
    TEXT("float revisionGate=saturate(MaterialRevision/23.0);\n")
    TEXT("float calibrationGate=saturate(CalibrationRevision);\n")
    TEXT("return saturate(turf*revisionGate*calibrationGate);"));

const FString GroundR23BRoughnessCode(
    TEXT("float nearDetail=saturate(NearDetail);\n")
    TEXT("float nearRough=0.62+0.42*saturate(TextureRoughness);\n")
    TEXT("float base=lerp(0.86,nearRough,nearDetail);\n")
    TEXT("float wearAmount=saturate(Wear/0.18);\n")
    TEXT("float wetAmount=saturate(Wetness/0.12);\n")
    TEXT("float detailDelta=0.035*(Macro-0.5)+0.015*(0.5-Mowing)+0.035*wearAmount-0.055*wetAmount;\n")
    TEXT("return clamp(base+nearDetail*detailDelta,0.72,0.92);"));

const FString GroundR10AoCode(
    TEXT("return lerp(1.0,clamp(TextureAo,0.72,1.0),0.55*saturate(NearDetail));"));

const FString GroundR10FarFadeCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("return 1.0-smoothstep(3000.0,5500.0,distanceCm);"));
const FString GroundR22FarFadeCode(
    TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
    TEXT("return 1.0-smoothstep(3000.0,5500.0,distanceCm);"));
const FString GroundR22MipBiasCode(
    TEXT("return 1.0-saturate(NearDetail);"));

FString BuildGroundCoreMaskCode(bool bIncludeOpaqueCollar)
{
    const FVector2D CenterCm =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipCenterCentimeters();
    const FVector2D SemiAxesCm =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSemiAxesCentimeters();
    const FVector RippleAmplitudes =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipRippleAmplitudes();
    const FVector RipplePhases =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipRipplePhasesRadians();
    const FString OpaqueCollarDeclaration = bIncludeOpaqueCollar
        ? FString::Printf(
            TEXT("const float opaqueCollarCm = %.9f;\n"),
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGroundOverlayOpaqueCollarMeters() * 100.0)
        : FString();
    const FString CoverageExpression = bIncludeOpaqueCollar
        ? TEXT("float coverage = saturate(1.0 + (signedInwardDistanceCm + opaqueCollarCm) / outwardFeatherCm);\n")
        : TEXT("float coverage = saturate(1.0 + signedInwardDistanceCm / outwardFeatherCm);\n");
    return FString::Printf(
        TEXT("const float2 coreCenterCm = float2(%.9f,%.9f);\n")
        TEXT("const float2 semiAxesCm = float2(%.9f,%.9f);\n")
        TEXT("const float outwardFeatherCm = %.9f;\n")
        TEXT("%s")
        TEXT("const float ditherCellCm = %.9f;\n")
        TEXT("const float segmentParameterEpsilon = %.9f;\n")
        TEXT("const float twoPi = 6.28318530717958647692;\n")
        TEXT("const int edgeCount = %d;\n")
        TEXT("const float sector = twoPi / (float)edgeCount;\n")
        TEXT("const float ripple3Amplitude = %.9f;\n")
        TEXT("const float ripple5Amplitude = %.9f;\n")
        TEXT("const float ripple7Amplitude = %.9f;\n")
        TEXT("const float ripple3Phase = %.9f;\n")
        TEXT("const float ripple5Phase = %.9f;\n")
        TEXT("const float ripple7Phase = %.9f;\n")
        TEXT("float2 worldCm = WorldPosition.xy;\n")
        TEXT("float2 centeredCm = worldCm - coreCenterCm;\n")
        TEXT("float radialDistanceCm = length(centeredCm);\n")
        TEXT("if (radialDistanceCm < 0.001) return 1.0;\n")
        TEXT("float parametricAngle = atan2(centeredCm.y/semiAxesCm.y, centeredCm.x/semiAxesCm.x);\n")
        TEXT("parametricAngle = parametricAngle < 0.0 ? parametricAngle + twoPi : parametricAngle;\n")
        TEXT("int sectorIndex = min(edgeCount - 1, (int)floor(parametricAngle / sector));\n")
        TEXT("float theta0 = (float)sectorIndex * sector;\n")
        TEXT("float theta1 = (float)(sectorIndex + 1) * sector;\n")
        TEXT("float scale0 = 1.0 + ripple3Amplitude*sin(3.0*theta0+ripple3Phase) + ripple5Amplitude*sin(5.0*theta0+ripple5Phase) + ripple7Amplitude*sin(7.0*theta0+ripple7Phase);\n")
        TEXT("float scale1 = 1.0 + ripple3Amplitude*sin(3.0*theta1+ripple3Phase) + ripple5Amplitude*sin(5.0*theta1+ripple5Phase) + ripple7Amplitude*sin(7.0*theta1+ripple7Phase);\n")
        TEXT("float2 p0 = scale0 * float2(semiAxesCm.x*cos(theta0),semiAxesCm.y*sin(theta0));\n")
        TEXT("float2 p1 = scale1 * float2(semiAxesCm.x*cos(theta1),semiAxesCm.y*sin(theta1));\n")
        TEXT("float2 edge = p1 - p0;\n")
        TEXT("float2 ray = centeredCm / radialDistanceCm;\n")
        TEXT("float denominator = ray.x*edge.y - ray.y*edge.x;\n")
        TEXT("if (abs(denominator) <= 0.0001) return 0.0;\n")
        TEXT("float rawSegmentParameter = (p0.x*ray.y - p0.y*ray.x) / denominator;\n")
        TEXT("float boundaryRayCm = (p0.x*edge.y - p0.y*edge.x) / denominator;\n")
        TEXT("if (rawSegmentParameter < -segmentParameterEpsilon || rawSegmentParameter > 1.0 + segmentParameterEpsilon || boundaryRayCm <= 0.0) return 0.0;\n")
        TEXT("float segmentParameter = clamp(rawSegmentParameter, 0.0, 1.0);\n")
        TEXT("if (segmentParameter < 0.0 || segmentParameter > 1.0) return 0.0;\n")
        TEXT("float signedInwardDistanceCm = boundaryRayCm - radialDistanceCm;\n")
        TEXT("%s")
        TEXT("float2 stableCell = floor(worldCm / ditherCellCm);\n")
        TEXT("float dither = frac(sin(dot(stableCell,float2(12.9898,78.233))) * 43758.5453);\n")
        TEXT("return coverage > 0.0 ? step(dither, coverage) : 0.0;"),
        CenterCm.X,
        CenterCm.Y,
        SemiAxesCm.X,
        SemiAxesCm.Y,
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayOutwardFeatherMeters() * 100.0,
        *OpaqueCollarDeclaration,
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayDitherCellMeters() * 100.0,
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSegmentParameterEpsilon(),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedProviderSiteClipSplinePoints(),
        RippleAmplitudes.X,
        RippleAmplitudes.Y,
        RippleAmplitudes.Z,
        RipplePhases.X,
        RipplePhases.Y,
        RipplePhases.Z,
        *CoverageExpression);
}

const FString GroundR15CoreMaskCode(BuildGroundCoreMaskCode(false));
const FString GroundCoreMaskCode(BuildGroundCoreMaskCode(true));

FString ObjectPath(const FString& AssetName)
{
    return MaterialNamespace + TEXT("/") + AssetName + TEXT(".") + AssetName;
}

FString PackagePath(const FString& AssetName)
{
    return MaterialNamespace + TEXT("/") + AssetName;
}

template <typename TObjectType>
TObjectType* LoadExact(const FString& Path)
{
    TObjectType* Object = LoadObject<TObjectType>(nullptr, *Path);
    return Object && Object->GetPathName() == Path ? Object : nullptr;
}

template <typename TActorType>
TActorType* FindExactlyOne(UWorld* World, int32& OutCount)
{
    OutCount = 0;
    TActorType* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<TActorType> It(World); It; ++It)
    {
        ++OutCount;
        Result = *It;
    }
    return OutCount == 1 ? Result : nullptr;
}

FString BuildR12GrassColorCodeForProfile(const FGrassMaterialProfile& Profile)
{
    return FString::Printf(
        TEXT("float h = saturate(BladeUV.y);\n")
        TEXT("float bladeSeed = saturate(BladeUV.x);\n")
        TEXT("float instanceSeed = saturate(Random01);\n")
        TEXT("float2 worldM = WorldPosition.xy * 0.01;\n")
        TEXT("float2 macroP = (worldM + float2(17.3,-9.1)) / 11.0;\n")
        TEXT("float2 macroI = floor(macroP);\n")
        TEXT("float2 macroF = frac(macroP);\n")
        TEXT("macroF = macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash = frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash = frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float ma=macroHash.x; float mb=macroHash.y; float mc=macroHash.z; float md=macroHash.w;\n")
        TEXT("float macroNoise = lerp(lerp(ma,mb,macroF.x),lerp(mc,md,macroF.x),macroF.y);\n")
        TEXT("float rise = smoothstep(0.04,0.69,h);\n")
        TEXT("float cutTip = smoothstep(0.90,1.0,h);\n")
        TEXT("float3 root = float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body = float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip = float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 result = lerp(root,body,rise);\n")
        TEXT("result = lerp(result,tip,cutTip);\n")
        TEXT("float coherentSeed = saturate(0.72*macroNoise+0.18*bladeSeed+0.10*instanceSeed);\n")
        TEXT("float localVariation = lerp(0.970,1.050,coherentSeed);\n")
        TEXT("float3 macroTint = lerp(float3(0.992,1.004,1.006),float3(1.006,0.998,0.988),macroNoise);\n")
        TEXT("return saturate(result*localVariation*macroTint);"),
        Profile.Root.R,
        Profile.Root.G,
        Profile.Root.B,
        Profile.Body.R,
        Profile.Body.G,
        Profile.Body.B,
        Profile.Tip.R,
        Profile.Tip.G,
        Profile.Tip.B);
}

FString BuildGrassColorCode(const FGrassMaterialProfile& Profile)
{
    return FString::Printf(
        TEXT("float h=saturate(BladeUV.y);\n")
        TEXT("float bladeSeed=saturate(BladeUV.x);\n")
        TEXT("float instanceSeed=saturate(Random01);\n")
        TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
        TEXT("float2 macroP=(worldM+float2(17.3,-9.1))/11.0;\n")
        TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP);\n")
        TEXT("macroF=macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash=frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash=frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float macroNoise=lerp(lerp(macroHash.x,macroHash.y,macroF.x),lerp(macroHash.z,macroHash.w,macroF.x),macroF.y);\n")
        TEXT("float rise=smoothstep(0.04,0.69,h);\n")
        TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
        TEXT("float3 root=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 result=lerp(root,body,rise);\n")
        TEXT("result=lerp(result,tip,cutMask);\n")
        TEXT("float coherentSeed=saturate(0.30*macroNoise+0.50*bladeSeed+0.20*instanceSeed);\n")
        TEXT("float localVariation=lerp(0.950,1.045,coherentSeed);\n")
        TEXT("float3 bladeTint=lerp(float3(1.015,0.995,0.972),float3(0.982,1.004,1.024),coherentSeed);\n")
        TEXT("float cutLuminance=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(result,cutLuminance.xxx,0.06*cutMask);\n")
        TEXT("return saturate(result*localVariation*bladeTint*lerp(1.0,0.985,cutMask));"),
        Profile.Root.R,
        Profile.Root.G,
        Profile.Root.B,
        Profile.Body.R,
        Profile.Body.G,
        Profile.Body.B,
        Profile.Tip.R,
        Profile.Tip.G,
        Profile.Tip.B);
}

FString BuildR15GrassColorCode(const FGrassMaterialProfile& Profile)
{
    return FString::Printf(
        TEXT("float h=saturate(BladeUV.y);\n")
        TEXT("float bladeSeed=saturate(BladeUV.x);\n")
        TEXT("float instanceSeed=saturate(Random01);\n")
        TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
        TEXT("float2 macroP=(worldM+float2(17.3,-9.1))/11.0;\n")
        TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP);\n")
        TEXT("macroF=macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash=frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash=frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float macroNoise=lerp(lerp(macroHash.x,macroHash.y,macroF.x),lerp(macroHash.z,macroHash.w,macroF.x),macroF.y);\n")
        TEXT("float rise=smoothstep(0.04,0.69,h);\n")
        TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
        TEXT("float3 root=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 result=lerp(root,body,rise);\n")
        TEXT("result=lerp(result,tip,cutMask);\n")
        TEXT("float coherentSeed=saturate(0.30*macroNoise+0.50*bladeSeed+0.20*instanceSeed);\n")
        TEXT("float localVariation=lerp(0.950,1.045,coherentSeed);\n")
        TEXT("float3 bladeTint=lerp(float3(1.015,0.995,0.972),float3(0.982,1.004,1.024),coherentSeed);\n")
        TEXT("float cutLuminance=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(result,cutLuminance.xxx,0.06*cutMask);\n")
        TEXT("return saturate(result*localVariation*bladeTint*lerp(1.0,0.985,cutMask)*float3(1.10,1.16,1.08));"),
        Profile.Root.R,
        Profile.Root.G,
        Profile.Root.B,
        Profile.Body.R,
        Profile.Body.G,
        Profile.Body.B,
        Profile.Tip.R,
        Profile.Tip.G,
        Profile.Tip.B);
}

FString BuildR17GrassColorCode(const FGrassMaterialProfile& Profile)
{
    // Keep the deterministic R15 blade field, cut topology, and local hue
    // variation; R17 only changes its final photographic gain.
    return FString::Printf(
        TEXT("float h=saturate(BladeUV.y);\n")
        TEXT("float bladeSeed=saturate(BladeUV.x);\n")
        TEXT("float instanceSeed=saturate(Random01);\n")
        TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
        TEXT("float2 macroP=(worldM+float2(17.3,-9.1))/11.0;\n")
        TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP);\n")
        TEXT("macroF=macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash=frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash=frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float macroNoise=lerp(lerp(macroHash.x,macroHash.y,macroF.x),lerp(macroHash.z,macroHash.w,macroF.x),macroF.y);\n")
        TEXT("float rise=smoothstep(0.04,0.69,h);\n")
        TEXT("float cutMask=smoothstep(0.86,1.0,h);\n")
        TEXT("float3 root=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 result=lerp(root,body,rise);\n")
        TEXT("result=lerp(result,tip,cutMask);\n")
        TEXT("float coherentSeed=saturate(0.30*macroNoise+0.50*bladeSeed+0.20*instanceSeed);\n")
        TEXT("float localVariation=lerp(0.950,1.045,coherentSeed);\n")
        TEXT("float3 bladeTint=lerp(float3(1.015,0.995,0.972),float3(0.982,1.004,1.024),coherentSeed);\n")
        TEXT("float cutLuminance=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(result,cutLuminance.xxx,0.06*cutMask);\n")
        TEXT("return saturate(result*localVariation*bladeTint*lerp(1.0,0.985,cutMask)*float3(1.16,1.18,1.02));"),
        Profile.Root.R, Profile.Root.G, Profile.Root.B,
        Profile.Body.R, Profile.Body.G, Profile.Body.B,
        Profile.Tip.R, Profile.Tip.G, Profile.Tip.B);
}

FString BuildR19GrassColorCode(
    const FGrassMaterialProfile& Profile,
    int32 ProfileIndex)
{
    return FString::Printf(
        TEXT("float h=saturate(BladeUV.y);\n")
        TEXT("float bladeSeed=saturate(BladeUV.x);\n")
        TEXT("float instanceSeed=saturate(Random01);\n")
        TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
        TEXT("float distanceDetail=1.0-smoothstep(800.0,2000.0,distanceCm);\n")
        TEXT("float visibilityDetail=smoothstep(0.90,1.0,saturate(InstanceFade));\n")
        TEXT("float detailWeight=distanceDetail*visibilityDetail;\n")
        TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
        TEXT("float2 macroP=(worldM+float2(17.3,-9.1))/11.0;\n")
        TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP);\n")
        TEXT("macroF=macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash=frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash=frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float macroNoise=lerp(lerp(macroHash.x,macroHash.y,macroF.x),lerp(macroHash.z,macroHash.w,macroF.x),macroF.y);\n")
        TEXT("float2 mesoP=(worldM+float2(-5.7,13.1))/3.6;\n")
        TEXT("float2 mesoI=floor(mesoP); float2 mesoF=frac(mesoP);\n")
        TEXT("mesoF=mesoF*mesoF*(3.0-2.0*mesoF);\n")
        TEXT("float4 mesoHash=frac(float4(dot(mesoI,float2(269.5,183.3)),dot(mesoI+float2(1,0),float2(269.5,183.3)),dot(mesoI+float2(0,1),float2(269.5,183.3)),dot(mesoI+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
        TEXT("mesoHash=frac(mesoHash*(mesoHash+19.19)*(mesoHash+mesoHash+7.77));\n")
        TEXT("float mesoNoise=lerp(lerp(mesoHash.x,mesoHash.y,mesoF.x),lerp(mesoHash.z,mesoHash.w,mesoF.x),mesoF.y);\n")
        TEXT("float rise=smoothstep(0.04,0.69,h);\n")
        TEXT("float cutMask=smoothstep(0.82,1.0,h);\n")
        TEXT("float3 root=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float dryFraction=%.6f;\n")
        TEXT("float rootThatchMix=%.6f;\n")
        TEXT("float3 result=lerp(root,body,rise);\n")
        TEXT("result=lerp(result,tip,cutMask);\n")
        TEXT("float broadSeed=saturate(0.70*macroNoise+0.30*mesoNoise);\n")
        TEXT("float fineSeed=saturate(0.60*bladeSeed+0.40*instanceSeed);\n")
        TEXT("float localVariation=lerp(0.970,1.035,broadSeed)*lerp(1.0,lerp(0.990,1.015,fineSeed),detailWeight);\n")
        TEXT("float3 bladeTint=lerp(float3(1.025,0.995,0.950),float3(0.985,1.005,0.990),broadSeed);\n")
        TEXT("float drySeed=frac(bladeSeed*13.173+instanceSeed*0.7548777+mesoNoise*0.618034);\n")
        TEXT("float dryNear=smoothstep(1.0-dryFraction-0.025,1.0-dryFraction+0.025,drySeed);\n")
        TEXT("float dryMix=lerp(dryFraction,dryNear,detailWeight)*lerp(0.70,1.0,mesoNoise);\n")
        TEXT("result=lerp(result,float3(0.230,0.165,0.070),0.68*dryMix);\n")
        TEXT("float rootMask=1.0-smoothstep(0.06,0.30,h);\n")
        TEXT("float thatchMix=rootMask*detailWeight*lerp(0.04,rootThatchMix,mesoNoise);\n")
        TEXT("result=lerp(result,float3(0.065,0.050,0.028),thatchMix);\n")
        TEXT("float cutLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(result,cutLuma.xxx,0.18*cutMask);\n")
        TEXT("result*=lerp(1.0,0.95,cutMask);\n")
        TEXT("result*=localVariation*bladeTint*float3(1.34,1.18,1.03);\n")
        TEXT("float finalLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(finalLuma.xxx,result,0.88);\n")
        TEXT("return saturate(result);"),
        Profile.Root.R, Profile.Root.G, Profile.Root.B,
        Profile.Body.R, Profile.Body.G, Profile.Body.B,
        Profile.Tip.R, Profile.Tip.G, Profile.Tip.B,
        GrassR19DryBladeFraction[ProfileIndex],
        GrassR19RootThatchMix[ProfileIndex]);
}

FString BuildR21GrassColorCode(const FR21GrassMaterialProfile& Profile)
{
    return FString::Printf(
        TEXT("float h=saturate(BladeUV.y);\n")
        TEXT("float bladeSeed=saturate(BladeUV.x);\n")
        TEXT("float instanceSeed=saturate(Random01);\n")
        TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
        TEXT("float distanceDetail=1.0-smoothstep(500.0,1400.0,distanceCm);\n")
        TEXT("float visibilityDetail=smoothstep(0.90,1.0,saturate(InstanceFade));\n")
        TEXT("float detailWeight=distanceDetail*visibilityDetail;\n")
        TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
        TEXT("float2 macroP=(worldM+float2(17.3,-9.1))/13.5;\n")
        TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP);\n")
        TEXT("macroF=macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash=frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash=frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float macroNoise=lerp(lerp(macroHash.x,macroHash.y,macroF.x),lerp(macroHash.z,macroHash.w,macroF.x),macroF.y);\n")
        TEXT("float2 mesoP=(worldM+float2(-5.7,13.1))/4.7;\n")
        TEXT("float2 mesoI=floor(mesoP); float2 mesoF=frac(mesoP);\n")
        TEXT("mesoF=mesoF*mesoF*(3.0-2.0*mesoF);\n")
        TEXT("float4 mesoHash=frac(float4(dot(mesoI,float2(269.5,183.3)),dot(mesoI+float2(1,0),float2(269.5,183.3)),dot(mesoI+float2(0,1),float2(269.5,183.3)),dot(mesoI+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
        TEXT("mesoHash=frac(mesoHash*(mesoHash+19.19)*(mesoHash+mesoHash+7.77));\n")
        TEXT("float mesoNoise=lerp(lerp(mesoHash.x,mesoHash.y,mesoF.x),lerp(mesoHash.z,mesoHash.w,mesoF.x),mesoF.y);\n")
        TEXT("float rise=smoothstep(0.08,0.62,h);\n")
        TEXT("float cutMask=smoothstep(0.84,1.0,h);\n")
        TEXT("float rootMask=1.0-smoothstep(0.06,0.34,h);\n")
        TEXT("float soilRootMask=1.0-smoothstep(0.02,0.18,h);\n")
        TEXT("float3 root=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float dryFraction=%.6f;\n")
        TEXT("float thatchFraction=%.6f;\n")
        TEXT("float soilRootFraction=%.6f;\n")
        TEXT("float3 dryColor=float3(0.285,0.205,0.060);\n")
        TEXT("float3 thatchColor=float3(0.105,0.072,0.024);\n")
        TEXT("float3 soilRootColor=float3(0.050,0.034,0.014);\n")
        TEXT("float drySeed=frac(bladeSeed*13.173+instanceSeed*0.7548777+mesoNoise*0.618034);\n")
        TEXT("float thatchSeed=frac(bladeSeed*23.417+instanceSeed*0.438579+macroNoise*0.381966);\n")
        TEXT("float soilSeed=frac(bladeSeed*37.719+instanceSeed*0.279621+mesoNoise*0.236068);\n")
        TEXT("float dryNear=smoothstep(1.0-dryFraction-0.018,1.0-dryFraction+0.018,drySeed);\n")
        TEXT("float thatchNear=smoothstep(1.0-thatchFraction-0.020,1.0-thatchFraction+0.020,thatchSeed);\n")
        TEXT("float soilNear=smoothstep(1.0-soilRootFraction-0.015,1.0-soilRootFraction+0.015,soilSeed);\n")
        TEXT("float3 nearDetailedColor=lerp(root,body,rise);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,tip,cutMask);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,dryColor,0.90*dryNear);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,thatchColor,0.92*rootMask*thatchNear);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,soilRootColor,0.98*soilRootMask*soilNear);\n")
        TEXT("float3 farColor=lerp(body,dryColor,0.90*dryFraction);\n")
        TEXT("farColor=lerp(farColor,thatchColor,0.08*thatchFraction);\n")
        TEXT("float3 result=lerp(farColor,nearDetailedColor,detailWeight);\n")
        TEXT("float broadSeed=saturate(0.70*macroNoise+0.30*mesoNoise);\n")
        TEXT("float fineSeed=saturate(0.60*bladeSeed+0.40*instanceSeed);\n")
        TEXT("float localVariation=lerp(0.975,1.025,broadSeed)*lerp(1.0,lerp(0.990,1.010,fineSeed),detailWeight);\n")
        TEXT("float3 bladeTint=lerp(float3(1.015,1.000,0.965),float3(0.990,1.005,0.990),broadSeed);\n")
        TEXT("float cutLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(result,cutLuma.xxx,0.20*cutMask*detailWeight);\n")
        TEXT("result*=lerp(1.0,0.97,cutMask*detailWeight);\n")
        TEXT("result*=localVariation*bladeTint*float3(1.08,1.00,0.92);\n")
        TEXT("float finalLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(finalLuma.xxx,result,0.90);\n")
        TEXT("return saturate(result);"),
        Profile.Root.R, Profile.Root.G, Profile.Root.B,
        Profile.Body.R, Profile.Body.G, Profile.Body.B,
        Profile.Tip.R, Profile.Tip.G, Profile.Tip.B,
        Profile.DryFraction,
        Profile.ThatchFraction,
        Profile.SoilRootFraction);
}

FString BuildR23GrassColorCode(const FR21GrassMaterialProfile& Profile)
{
    return FString::Printf(
        TEXT("float h=saturate(BladeUV.y);\n")
        TEXT("float bladeSeed=saturate(BladeUV.x);\n")
        TEXT("float instanceSeed=saturate(Random01);\n")
        TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
        TEXT("float distanceDetail=1.0-smoothstep(1000.0,2400.0,distanceCm);\n")
        TEXT("float visibilityDetail=smoothstep(0.82,1.0,saturate(InstanceFade));\n")
        TEXT("float detailWeight=distanceDetail*visibilityDetail;\n")
        TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
        TEXT("float2 macroP=(worldM+float2(17.3,-9.1))/9.5;\n")
        TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP);\n")
        TEXT("macroF=macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash=frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash=frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float macroNoise=lerp(lerp(macroHash.x,macroHash.y,macroF.x),lerp(macroHash.z,macroHash.w,macroF.x),macroF.y);\n")
        TEXT("float2 mesoP=(worldM+float2(-5.7,13.1))/2.1;\n")
        TEXT("float2 mesoI=floor(mesoP); float2 mesoF=frac(mesoP);\n")
        TEXT("mesoF=mesoF*mesoF*(3.0-2.0*mesoF);\n")
        TEXT("float4 mesoHash=frac(float4(dot(mesoI,float2(269.5,183.3)),dot(mesoI+float2(1,0),float2(269.5,183.3)),dot(mesoI+float2(0,1),float2(269.5,183.3)),dot(mesoI+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
        TEXT("mesoHash=frac(mesoHash*(mesoHash+19.19)*(mesoHash+mesoHash+7.77));\n")
        TEXT("float mesoNoise=lerp(lerp(mesoHash.x,mesoHash.y,mesoF.x),lerp(mesoHash.z,mesoHash.w,mesoF.x),mesoF.y);\n")
        TEXT("float rise=smoothstep(0.07,0.66,h);\n")
        TEXT("float cutMask=smoothstep(0.84,1.0,h);\n")
        TEXT("float soilRootMask=1.0-smoothstep(0.04,0.22,h);\n")
        TEXT("float thatchLower=smoothstep(0.08,0.18,h);\n")
        TEXT("float thatchUpper=1.0-smoothstep(0.34,0.54,h);\n")
        TEXT("float thatchBand=thatchLower*thatchUpper;\n")
        TEXT("float3 root=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float dryFraction=%.6f;\n")
        TEXT("float thatchFraction=%.6f;\n")
        TEXT("float soilRootFraction=%.6f;\n")
        TEXT("float3 dryColor=float3(0.220,0.160,0.050);\n")
        TEXT("float3 thatchColor=float3(0.085,0.060,0.025);\n")
        TEXT("float3 soilRootColor=float3(0.035,0.025,0.012);\n")
        TEXT("float drySeed=frac(bladeSeed*13.173+instanceSeed*0.7548777+mesoNoise*0.618034);\n")
        TEXT("float thatchSeed=frac(bladeSeed*23.417+instanceSeed*0.438579+macroNoise*0.381966);\n")
        TEXT("float soilSeed=frac(bladeSeed*37.719+instanceSeed*0.279621+mesoNoise*0.236068);\n")
        TEXT("float dryNear=smoothstep(1.0-dryFraction-0.030,1.0-dryFraction+0.030,drySeed);\n")
        TEXT("float thatchNear=smoothstep(1.0-thatchFraction-0.026,1.0-thatchFraction+0.026,thatchSeed);\n")
        TEXT("float soilNear=smoothstep(1.0-soilRootFraction-0.020,1.0-soilRootFraction+0.020,soilSeed);\n")
        TEXT("float3 nearDetailedColor=lerp(root,body,rise);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,tip,cutMask);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,dryColor,0.72*dryNear*smoothstep(0.18,0.82,h));\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,thatchColor,0.82*thatchBand*thatchNear);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,soilRootColor,0.92*soilRootMask*soilNear);\n")
        TEXT("float3 farColor=lerp(body,dryColor,0.42*dryFraction);\n")
        TEXT("farColor=lerp(farColor,thatchColor,0.035*thatchFraction);\n")
        TEXT("float3 result=lerp(farColor,nearDetailedColor,detailWeight);\n")
        TEXT("float broadSeed=saturate(0.62*macroNoise+0.38*mesoNoise);\n")
        TEXT("float fineSeed=saturate(0.58*bladeSeed+0.42*instanceSeed);\n")
        TEXT("float localVariation=lerp(0.91,1.09,broadSeed)*lerp(1.0,lerp(0.965,1.035,fineSeed),detailWeight);\n")
        TEXT("float3 bladeTint=lerp(float3(1.025,0.990,0.955),float3(0.965,1.025,0.985),broadSeed);\n")
        TEXT("float cutLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(result,cutLuma.xxx,0.11*cutMask*detailWeight);\n")
        TEXT("result*=lerp(1.0,0.965,cutMask*detailWeight);\n")
        TEXT("result*=localVariation*bladeTint*float3(0.98,1.02,0.96);\n")
        TEXT("float finalLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(finalLuma.xxx,result,0.94);\n")
        TEXT("return saturate(result);"),
        Profile.Root.R, Profile.Root.G, Profile.Root.B,
        Profile.Body.R, Profile.Body.G, Profile.Body.B,
        Profile.Tip.R, Profile.Tip.G, Profile.Tip.B,
        Profile.DryFraction,
        Profile.ThatchFraction,
        Profile.SoilRootFraction);
}

FString BuildR23BGrassColorCode(const FR21GrassMaterialProfile& Profile)
{
    return FString::Printf(
        TEXT("float h=saturate(BladeUV.y);\n")
        TEXT("float bladeSeed=saturate(BladeUV.x);\n")
        TEXT("float instanceSeed=saturate(Random01);\n")
        TEXT("float distanceCm=length(WorldPosition-CameraPosition);\n")
        TEXT("float distanceDetail=1.0-smoothstep(1000.0,2400.0,distanceCm);\n")
        TEXT("float visibilityDetail=smoothstep(0.82,1.0,saturate(InstanceFade));\n")
        TEXT("float detailWeight=distanceDetail*visibilityDetail;\n")
        TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
        TEXT("float2 macroP=(worldM+float2(17.3,-9.1))/9.5;\n")
        TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP);\n")
        TEXT("macroF=macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float4 macroHash=frac(float4(dot(macroI,float2(127.1,311.7)),dot(macroI+float2(1,0),float2(127.1,311.7)),dot(macroI+float2(0,1),float2(127.1,311.7)),dot(macroI+float2(1,1),float2(127.1,311.7)))*0.1031);\n")
        TEXT("macroHash=frac(macroHash*(macroHash+33.33)*(macroHash+macroHash+17.17));\n")
        TEXT("float macroNoise=lerp(lerp(macroHash.x,macroHash.y,macroF.x),lerp(macroHash.z,macroHash.w,macroF.x),macroF.y);\n")
        TEXT("float2 mesoP=(worldM+float2(-5.7,13.1))/2.1;\n")
        TEXT("float2 mesoI=floor(mesoP); float2 mesoF=frac(mesoP);\n")
        TEXT("mesoF=mesoF*mesoF*(3.0-2.0*mesoF);\n")
        TEXT("float4 mesoHash=frac(float4(dot(mesoI,float2(269.5,183.3)),dot(mesoI+float2(1,0),float2(269.5,183.3)),dot(mesoI+float2(0,1),float2(269.5,183.3)),dot(mesoI+float2(1,1),float2(269.5,183.3)))*0.0973);\n")
        TEXT("mesoHash=frac(mesoHash*(mesoHash+19.19)*(mesoHash+mesoHash+7.77));\n")
        TEXT("float mesoNoise=lerp(lerp(mesoHash.x,mesoHash.y,mesoF.x),lerp(mesoHash.z,mesoHash.w,mesoF.x),mesoF.y);\n")
        TEXT("float rise=smoothstep(0.07,0.66,h);\n")
        TEXT("float cutMask=smoothstep(0.84,1.0,h);\n")
        TEXT("float soilRootMask=1.0-smoothstep(0.04,0.22,h);\n")
        TEXT("float thatchLower=smoothstep(0.08,0.18,h);\n")
        TEXT("float thatchUpper=1.0-smoothstep(0.34,0.54,h);\n")
        TEXT("float thatchBand=thatchLower*thatchUpper;\n")
        TEXT("float3 root=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip=float3(%.6f,%.6f,%.6f);\n")
        TEXT("float dryFraction=%.6f;\n")
        TEXT("float thatchFraction=%.6f;\n")
        TEXT("float soilRootFraction=%.6f;\n")
        TEXT("float3 dryColor=float3(0.210,0.150,0.050);\n")
        TEXT("float3 thatchColor=float3(0.100,0.068,0.028);\n")
        TEXT("float3 soilRootColor=float3(0.045,0.030,0.014);\n")
        TEXT("float drySeed=frac(bladeSeed*13.173+instanceSeed*0.7548777+mesoNoise*0.618034);\n")
        TEXT("float thatchSeed=frac(bladeSeed*23.417+instanceSeed*0.438579+macroNoise*0.381966);\n")
        TEXT("float soilSeed=frac(bladeSeed*37.719+instanceSeed*0.279621+mesoNoise*0.236068);\n")
        TEXT("float dryNear=smoothstep(1.0-dryFraction-0.030,1.0-dryFraction+0.030,drySeed);\n")
        TEXT("float thatchNear=smoothstep(1.0-thatchFraction-0.026,1.0-thatchFraction+0.026,thatchSeed);\n")
        TEXT("float soilNear=smoothstep(1.0-soilRootFraction-0.020,1.0-soilRootFraction+0.020,soilSeed);\n")
        TEXT("float3 nearDetailedColor=lerp(root,body,rise);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,tip,cutMask);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,dryColor,0.76*dryNear*smoothstep(0.18,0.82,h));\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,thatchColor,0.88*thatchBand*thatchNear);\n")
        TEXT("nearDetailedColor=lerp(nearDetailedColor,soilRootColor,0.94*soilRootMask*soilNear);\n")
        TEXT("float3 farColor=lerp(body,dryColor,0.48*dryFraction);\n")
        TEXT("farColor=lerp(farColor,thatchColor,0.060*thatchFraction);\n")
        TEXT("float3 result=lerp(farColor,nearDetailedColor,detailWeight);\n")
        TEXT("float broadSeed=saturate(0.62*macroNoise+0.38*mesoNoise);\n")
        TEXT("float fineSeed=saturate(0.58*bladeSeed+0.42*instanceSeed);\n")
        TEXT("float localVariation=lerp(0.95,1.05,broadSeed)*lerp(1.0,lerp(0.98,1.02,fineSeed),detailWeight);\n")
        TEXT("float3 bladeTint=lerp(float3(1.045,0.975,0.950),float3(1.005,0.995,0.980),broadSeed);\n")
        TEXT("float cutLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(result,cutLuma.xxx,0.11*cutMask*detailWeight);\n")
        TEXT("result*=lerp(1.0,0.965,cutMask*detailWeight);\n")
        TEXT("result*=localVariation*bladeTint*float3(1.06,0.94,0.92);\n")
        TEXT("float finalLuma=dot(result,float3(0.212639,0.715169,0.072192));\n")
        TEXT("result=lerp(finalLuma.xxx,result,0.90);\n")
        TEXT("return saturate(result);"),
        Profile.Root.R, Profile.Root.G, Profile.Root.B,
        Profile.Body.R, Profile.Body.G, Profile.Body.B,
        Profile.Tip.R, Profile.Tip.G, Profile.Tip.B,
        Profile.DryFraction,
        Profile.ThatchFraction,
        Profile.SoilRootFraction);
}

FString BuildR12GrassColorCode(const FGrassMaterialProfile& Profile)
{
    return BuildR12GrassColorCodeForProfile(Profile);
}

FString BuildR10GrassColorCode(const FGrassMaterialProfile& Profile)
{
    return BuildR12GrassColorCodeForProfile(Profile);
}

FString BuildLegacyGrassColorCode(const FGrassMaterialProfile& Profile)
{
    return FString::Printf(
        TEXT("float h = saturate(BladeUV.y);\n")
        TEXT("float bladeSeed = saturate(BladeUV.x);\n")
        TEXT("float instanceSeed = saturate(Random01);\n")
        TEXT("float2 worldM = WorldPosition.xy * 0.01;\n")
        TEXT("float2 macroP = (worldM + float2(17.3,-9.1)) / 7.20;\n")
        TEXT("float2 macroI = floor(macroP);\n")
        TEXT("float2 macroF = frac(macroP);\n")
        TEXT("macroF = macroF*macroF*(3.0-2.0*macroF);\n")
        TEXT("float ma = frac(sin(dot(macroI,float2(127.1,311.7)))*43758.5453);\n")
        TEXT("float mb = frac(sin(dot(macroI+float2(1,0),float2(127.1,311.7)))*43758.5453);\n")
        TEXT("float mc = frac(sin(dot(macroI+float2(0,1),float2(127.1,311.7)))*43758.5453);\n")
        TEXT("float md = frac(sin(dot(macroI+float2(1,1),float2(127.1,311.7)))*43758.5453);\n")
        TEXT("float macroNoise = lerp(lerp(ma,mb,macroF.x),lerp(mc,md,macroF.x),macroF.y);\n")
        TEXT("float2 warp = float2(sin(worldM.y*0.083+macroNoise*2.1),cos(worldM.x*0.071-macroNoise*1.7))*0.31;\n")
        TEXT("float2 detailP = (worldM + warp + float2(-3.7,11.9)) / 0.58;\n")
        TEXT("float2 detailI = floor(detailP);\n")
        TEXT("float2 detailF = frac(detailP);\n")
        TEXT("detailF = detailF*detailF*(3.0-2.0*detailF);\n")
        TEXT("float da = frac(sin(dot(detailI,float2(269.5,183.3)))*43758.5453);\n")
        TEXT("float db = frac(sin(dot(detailI+float2(1,0),float2(269.5,183.3)))*43758.5453);\n")
        TEXT("float dc = frac(sin(dot(detailI+float2(0,1),float2(269.5,183.3)))*43758.5453);\n")
        TEXT("float dd = frac(sin(dot(detailI+float2(1,1),float2(269.5,183.3)))*43758.5453);\n")
        TEXT("float detailNoise = lerp(lerp(da,db,detailF.x),lerp(dc,dd,detailF.x),detailF.y);\n")
        TEXT("float rise = smoothstep(0.04,0.69,h);\n")
        TEXT("float cutTip = smoothstep(0.73,1.0,h);\n")
        TEXT("float3 root = float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 body = float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 tip = float3(%.6f,%.6f,%.6f);\n")
        TEXT("float3 result = lerp(root,body,rise);\n")
        TEXT("result = lerp(result,tip,cutTip);\n")
        TEXT("float localVariation = lerp(0.920,1.120,saturate(0.50*macroNoise+0.32*detailNoise+0.18*frac(bladeSeed*7.173+instanceSeed*0.618034)));\n")
        TEXT("float coolWarm = saturate(0.60*macroNoise+0.40*detailNoise);\n")
        TEXT("float3 hue = lerp(float3(0.972,1.016,1.025),float3(1.035,0.992,0.955),coolWarm);\n")
        TEXT("return saturate(result*localVariation*hue);"),
        Profile.Root.R,
        Profile.Root.G,
        Profile.Root.B,
        Profile.Body.R,
        Profile.Body.G,
        Profile.Body.B,
        Profile.Tip.R,
        Profile.Tip.G,
        Profile.Tip.B);
}

const FString SoilBaseCode(
    TEXT("float2 worldM = WorldPosition.xy * 0.01;\n")
    TEXT("float apronFraction = saturate(ApronUV.x);\n")
    TEXT("float treeMulchSentinel = 1.0-step(0.5,ApronUV.y);\n")
    TEXT("float2 macroP=(worldM+float2(13.7,-9.1))/3.80;\n")
    TEXT("float2 macroI=floor(macroP); float2 macroF=frac(macroP); macroF=macroF*macroF*(3.0-2.0*macroF);\n")
    TEXT("float ma=frac(sin(dot(macroI,float2(127.1,311.7)))*43758.5453);\n")
    TEXT("float mb=frac(sin(dot(macroI+float2(1,0),float2(127.1,311.7)))*43758.5453);\n")
    TEXT("float mc=frac(sin(dot(macroI+float2(0,1),float2(127.1,311.7)))*43758.5453);\n")
    TEXT("float md=frac(sin(dot(macroI+float2(1,1),float2(127.1,311.7)))*43758.5453);\n")
    TEXT("float macroNoise=lerp(lerp(ma,mb,macroF.x),lerp(mc,md,macroF.x),macroF.y);\n")
    TEXT("float2 detailP=(worldM+float2(sin(worldM.y*0.11),cos(worldM.x*0.09))*0.27)/0.54;\n")
    TEXT("float2 detailI=floor(detailP); float2 detailF=frac(detailP); detailF=detailF*detailF*(3.0-2.0*detailF);\n")
    TEXT("float da=frac(sin(dot(detailI,float2(269.5,183.3)))*43758.5453);\n")
    TEXT("float db=frac(sin(dot(detailI+float2(1,0),float2(269.5,183.3)))*43758.5453);\n")
    TEXT("float dc=frac(sin(dot(detailI+float2(0,1),float2(269.5,183.3)))*43758.5453);\n")
    TEXT("float dd=frac(sin(dot(detailI+float2(1,1),float2(269.5,183.3)))*43758.5453);\n")
    TEXT("float detailNoise=lerp(lerp(da,db,detailF.x),lerp(dc,dd,detailF.x),detailF.y);\n")
    TEXT("float poreNoise=0.5+0.5*sin(worldM.x*19.7+worldM.y*23.1+2.0*sin(worldM.x*5.3-worldM.y*3.9));\n")
    TEXT("float moisture=saturate(0.64*macroNoise+0.24*detailNoise+0.12*(1.0-poreNoise));\n")
    TEXT("float3 moist=float3(0.026,0.023,0.019); float3 loam=float3(0.068,0.055,0.038); float3 dry=float3(0.103,0.079,0.047);\n")
    TEXT("float3 soil=lerp(dry,moist,moisture); soil=lerp(soil,loam,0.24+0.22*detailNoise);\n")
    TEXT("float3 barkDark=float3(0.032,0.024,0.017); float3 barkWarm=float3(0.092,0.056,0.029);\n")
    TEXT("float fibre=pow(saturate(0.5+0.5*sin(worldM.x*12.7-worldM.y*8.9+detailNoise*5.0)),5.0);\n")
    TEXT("float3 mulch=lerp(barkDark,barkWarm,saturate(0.25+0.62*macroNoise+0.13*fibre));\n")
    TEXT("float edgeNoise=(detailNoise-0.5)*0.052; float coreMask=1.0-smoothstep(0.80+edgeNoise,0.995,apronFraction);\n")
    TEXT("float3 edgeTint=lerp(float3(0.046,0.055,0.022),float3(0.071,0.078,0.029),macroNoise);\n")
    TEXT("float3 formal=lerp(edgeTint,soil,coreMask); float3 tree=lerp(edgeTint,mulch,coreMask);\n")
    TEXT("return lerp(formal,tree,treeMulchSentinel);"));

const FString SoilRoughnessCode(
    TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
    TEXT("float macroNoise=0.5+0.5*sin(worldM.x*0.71+worldM.y*0.93+0.8*sin(worldM.y*0.19));\n")
    TEXT("float detailNoise=0.5+0.5*sin(worldM.x*11.3-worldM.y*9.7+sin(worldM.x*2.9));\n")
    TEXT("return clamp(0.80+0.11*detailNoise+0.055*(1.0-macroNoise),0.78,0.965);"));

const FString SoilNormalCode(
    TEXT("float2 worldM=WorldPosition.xy*0.01;\n")
    TEXT("float macroNoise=0.5+0.5*sin(worldM.x*1.3+worldM.y*1.7);\n")
    TEXT("float detailNoise=0.5+0.5*sin(worldM.x*29.0-worldM.y*23.0+macroNoise*2.1);\n")
    TEXT("float2 grain=float2(cos(worldM.x*91.0+1.7*sin(worldM.y*61.0)),cos(worldM.y*103.0+1.5*sin(worldM.x*67.0)));\n")
    TEXT("float2 fibre=float2(cos((worldM.x+worldM.y)*171.0),cos((worldM.y-worldM.x)*157.0));\n")
    TEXT("float2 slope=(0.052+0.016*detailNoise)*grain+0.023*fibre;\n")
    TEXT("return normalize(float3(slope,1.0));"));

UMaterialExpressionCustom* FindCustomByDescription(
    UMaterial* Material,
    const FString& Description)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    UMaterialExpressionCustom* Match = nullptr;
    for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
        if (Custom && Custom->Description == Description)
        {
            if (Match)
            {
                return nullptr;
            }
            Match = Custom;
        }
    }
    return Match;
}

UMaterialExpressionCustom* FindCustomByPrefix(
    UMaterial* Material,
    const FString& Prefix)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    UMaterialExpressionCustom* Match = nullptr;
    for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression);
        if (Custom && Custom->Description.StartsWith(Prefix))
        {
            if (Match)
            {
                return nullptr;
            }
            Match = Custom;
        }
    }
    return Match;
}

UMaterialExpressionWorldPosition* FindWorldPosition(
    UMaterial* Material,
    const FString& Description)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    UMaterialExpressionWorldPosition* Match = nullptr;
    for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        UMaterialExpressionWorldPosition* Position =
            Cast<UMaterialExpressionWorldPosition>(Expression);
        if (Position && Position->Desc == Description)
        {
            if (Match)
            {
                return nullptr;
            }
            Match = Position;
        }
    }
    return Match;
}

UMaterialExpressionScalarParameter* FindScalar(
    UMaterial* Material,
    FName ParameterName)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    UMaterialExpressionScalarParameter* Match = nullptr;
    for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        UMaterialExpressionScalarParameter* Scalar =
            Cast<UMaterialExpressionScalarParameter>(Expression);
        if (Scalar && Scalar->ParameterName == ParameterName)
        {
            if (Match)
            {
                return nullptr;
            }
            Match = Scalar;
        }
    }
    return Match;
}

UMaterialExpressionVectorParameter* FindVector(
    UMaterial* Material,
    FName ParameterName)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    UMaterialExpressionVectorParameter* Match = nullptr;
    for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        UMaterialExpressionVectorParameter* Vector =
            Cast<UMaterialExpressionVectorParameter>(Expression);
        if (Vector && Vector->ParameterName == ParameterName)
        {
            if (Match)
            {
                return nullptr;
            }
            Match = Vector;
        }
    }
    return Match;
}

template <typename TExpression>
TExpression* FindExpressionByDescription(
    UMaterial* Material,
    const FString& Description)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!Data)
    {
        return nullptr;
    }
    TExpression* Match = nullptr;
    for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        TExpression* Typed = Cast<TExpression>(Expression);
        if (Typed && Typed->Desc == Description)
        {
            if (Match)
            {
                return nullptr;
            }
            Match = Typed;
        }
    }
    return Match;
}

template <typename TExpression>
TExpression* AddExpression(
    UMaterial* Material,
    const FString& Description,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    TExpression* Expression = Data
        ? NewObject<TExpression>(Material, NAME_None, RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
        Data->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

UMaterialExpression* AddExpressionByClassPath(
    UMaterial* Material,
    const TCHAR* ClassPath,
    const FString& Description,
    int32 EditorX,
    int32 EditorY)
{
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UClass* ExpressionClass = Data
        ? FindObject<UClass>(nullptr, ClassPath)
        : nullptr;
    if (!ExpressionClass && Data)
    {
        ExpressionClass = LoadObject<UClass>(nullptr, ClassPath);
    }
    UMaterialExpression* Expression = ExpressionClass &&
        ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())
        ? NewObject<UMaterialExpression>(
              Material,
              ExpressionClass,
              NAME_None,
              RF_Transactional)
        : nullptr;
    if (Expression)
    {
        Expression->Desc = Description;
        Expression->MaterialExpressionEditorX = EditorX;
        Expression->MaterialExpressionEditorY = EditorY;
        Data->ExpressionCollection.Expressions.Add(Expression);
    }
    return Expression;
}

bool InputMatches(
    const FExpressionInput& Input,
    const UMaterialExpression* ExpectedExpression,
    int32 ExpectedOutputIndex = 0)
{
    return Input.Expression == ExpectedExpression &&
        Input.OutputIndex == ExpectedOutputIndex;
}

bool HasNoAuxiliaryCustomState(
    const UMaterialExpressionCustom* Custom)
{
    return Custom && Custom->AdditionalOutputs.IsEmpty() &&
        Custom->AdditionalDefines.IsEmpty() &&
        Custom->IncludeFilePaths.IsEmpty();
}

bool AllCustomExpressionsHaveNoAuxiliaryState(
    const UMaterialEditorOnlyData* Data)
{
    if (!Data)
    {
        return false;
    }
    for (const UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        const UMaterialExpressionCustom* Custom =
            Cast<UMaterialExpressionCustom>(Expression);
        if (Custom && !HasNoAuxiliaryCustomState(Custom))
        {
            return false;
        }
    }
    return true;
}

void ResetCustomAuxiliaryState(
    UMaterialExpressionCustom* Custom)
{
    if (Custom)
    {
        Custom->AdditionalOutputs.Reset();
        Custom->AdditionalDefines.Reset();
        Custom->IncludeFilePaths.Reset();
    }
}

bool ValidateCompiledMaterial(
    UMaterialInterface* Material,
    FString& OutError)
{
    const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
    if (Material)
    {
        Material->EnsureIsComplete();
    }
    FMaterialResource* Resource = Material
        ? Material->GetMaterialResource(FeatureLevel)
        : nullptr;
    if (!Resource && Material && Material->GetMaterial())
    {
        Material->GetMaterial()->EnsureIsComplete();
        Resource = Material->GetMaterial()->GetMaterialResource(FeatureLevel);
    }
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
    // UE 5.5 may leave CompiledSuccessfully unset on a renderable finalized
    // clone when per-shader DDC is active and full material-map DDC is off.
    // The complete map, finished resource, valid rendering state and empty
    // compile-error list remain the authoritative gate in that engine mode.
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
            TEXT("A V5D ground/vegetation material failed its compiled-active-feature-level/default-fallback gate: %s featureLevel=%d resource=%d compilationFinished=%d gameThreadComplete=%d shaderMap=%d compiledSuccessfully=%d materialMapDDC=%d shaderJobCacheDDC=%d acceptedCompileState=%d validForRendering=%d defaultMaterial=%d errors=%s"),
            Material ? *Material->GetPathName() : TEXT("<null>"),
            static_cast<int32>(FeatureLevel),
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
    OutError.Reset();
    return true;
}

bool ValidateGrassMaterialVersion(
    UMaterial* Material,
    int32 ProfileIndex,
    const FGrassMaterialProfile& Profile,
    const TCHAR* Version,
    const FString& ExpectedCode,
    FString& OutError)
{
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data || !Profile.Id || ProfileIndex < 0 ||
        ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames) ||
        Material->GetPathName() != ObjectPath(GrassAssetNames[ProfileIndex]) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || !Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            MSM_TwoSidedFoliage) ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        !FMath::IsNearlyEqual(
            Material->OpacityMaskClipValue, 0.50f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            GrassMaximumWpoCm,
            0.000001f))
    {
        OutError = FString::Printf(
            TEXT("V5D grass material %d lost its exact path or modeled-blade shading state."),
            ProfileIndex);
        return false;
    }
    const FString Description = GrassDescriptionPrefix +
        Profile.Id + Version;
    UMaterialExpressionCustom* Color =
        FindCustomByDescription(Material, Description);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material,
            TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition =
        FindWorldPosition(Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionTransformPosition* InstanceLocalPosition =
        FindExpressionByDescription<UMaterialExpressionTransformPosition>(
            Material,
            TEXT("V5B_BERMUDA_INSTANCE_LOCAL_POSITION"));
    UMaterialExpressionTime* Time =
        FindExpressionByDescription<UMaterialExpressionTime>(
            Material,
            TEXT("V5B_BERMUDA_TIME"));
    UMaterialExpressionScalarParameter* Roughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* WindStrength =
        FindScalar(Material, TEXT("TRIAD_WindStrengthCm"));
    UMaterialExpressionScalarParameter* WindSpeed =
        FindScalar(Material, TEXT("TRIAD_WindSpeed"));
    UMaterialExpressionScalarParameter* WindHeight =
        FindScalar(Material, TEXT("TRIAD_WindHeightCm"));
    UMaterialExpressionScalarParameter* WindResponse =
        FindScalar(Material, TEXT("TRIAD_WindResponseScale"));
    UMaterialExpressionScalarParameter* MaximumWpo =
        FindScalar(Material, TEXT("TRIAD_MaxWpoCm"));
    UMaterialExpressionScalarParameter* Specular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionVectorParameter* WindDirection =
        FindVector(Material, TEXT("TRIAD_WindDirection"));
    UMaterialExpressionConstant3Vector* WorldUpNormal =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material,
            TEXT("V5B_BERMUDA_WORLD_UP_NORMAL"));
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material,
            TEXT("V5B_BERMUDA_LIT_SUBSURFACE"));
    UMaterialExpressionVertexNormalWS* ImportedCurvatureNormal =
        FindExpressionByDescription<UMaterialExpressionVertexNormalWS>(
            Material,
            TEXT("V5B_BERMUDA_IMPORTED_CURVATURE_NORMAL_WS"));
    UMaterialExpressionTwoSidedSign* TwoSidedSign =
        FindExpressionByDescription<UMaterialExpressionTwoSidedSign>(
            Material,
            TEXT("V5B_BERMUDA_TWO_SIDED_SIGN"));
    UMaterialExpressionMultiply* FacingCorrectedNormal =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material,
            TEXT("V5B_BERMUDA_FACING_CORRECTED_NORMAL"));
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material,
            TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"));
    UMaterialExpressionMaterialFunctionCall* DitheredInstanceFade =
        FindExpressionByDescription<UMaterialExpressionMaterialFunctionCall>(
            Material,
            TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"));
    UMaterialExpressionCustom* Wind =
        FindCustomByDescription(Material, GrassWindDescription);

    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    int32 InstanceRandomNodes = 0;
    int32 InstanceFadeNodes = 0;
    int32 TextureSampleNodes = 0;
    bool bNoCustomizedUvConnections = true;
    for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
    {
        if (Cast<UMaterialExpressionTextureSample>(Expression))
        {
            ++TextureSampleNodes;
        }
        if (Expression && Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
        {
            ++InstanceRandomNodes;
            if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
            {
                InstanceRandom = Expression;
            }
        }
        if (Expression && Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
        {
            ++InstanceFadeNodes;
            if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
            {
                InstanceFade = Expression;
            }
        }
    }
    for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
    {
        bNoCustomizedUvConnections &= !CustomizedUv.Expression;
    }

    int32 DitherAlphaInputIndex = INDEX_NONE;
    int32 DitherAlphaInputCount = 0;
    bool bNoUnexpectedDitherInputs = true;
    const bool bExactDitherFunction = DitheredInstanceFade &&
        DitheredInstanceFade->MaterialFunction &&
        DitheredInstanceFade->MaterialFunction->GetPathName() ==
            GrassDitherTemporalAaFunctionPath &&
        DitheredInstanceFade->FunctionOutputs.Num() > 0;
    if (bExactDitherFunction)
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
                ++DitherAlphaInputCount;
            }
            else
            {
                bNoUnexpectedDitherInputs &=
                    !DitheredInstanceFade->FunctionInputs[InputIndex]
                         .Input.Expression;
            }
        }
    }
    const bool bExactDitherInput = bExactDitherFunction &&
        DitherAlphaInputCount == 1 && DitherAlphaInputIndex != INDEX_NONE &&
        bNoUnexpectedDitherInputs &&
        InputMatches(
            DitheredInstanceFade->FunctionInputs[DitherAlphaInputIndex].Input,
            InstanceFade,
            0);

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
        WindStrength,
        WindSpeed,
        WindDirection,
        WindHeight,
        WindResponse,
        MaximumWpo,
        InstanceRandom,
        InstanceFade};
    bool bExactWindInputs = Wind && Wind->Inputs.Num() == 12;
    if (bExactWindInputs)
    {
        for (int32 Index = 0; Index < 12; ++Index)
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

    if (!Color || !BladeUv || !InstanceRandom || !InstanceFade ||
        !WorldPosition || !InstanceLocalPosition || !Time || !Roughness ||
        !WindStrength || !WindSpeed || !WindHeight || !WindResponse ||
        !MaximumWpo || !Specular || !WindDirection || !WorldUpNormal ||
        !Subsurface || !ImportedCurvatureNormal || !TwoSidedSign ||
        !FacingCorrectedNormal || !DistanceMatchedNormal ||
        !DitheredInstanceFade || !Wind ||
        Data->ExpressionCollection.Expressions.Num() != 23 ||
        TextureSampleNodes != 0 || InstanceRandomNodes != 1 ||
        InstanceFadeNodes != 1 || !bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        BladeUv->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(BladeUv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(BladeUv->VTiling, 1.0f, 0.000001f) ||
        BladeUv->UnMirrorU || BladeUv->UnMirrorV ||
        Time->bOverride_Period || Time->bIgnorePause ||
        WorldPosition->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        InstanceLocalPosition->TransformSourceType != TRANSFORMPOSSOURCE_World ||
        InstanceLocalPosition->TransformType != TRANSFORMPOSSOURCE_Instance ||
        !InputMatches(InstanceLocalPosition->Input, WorldPosition, 0) ||
        Color->Code != ExpectedCode ||
        Color->OutputType != CMOT_Float3 || Color->Inputs.Num() != 3 ||
        Color->Inputs[0].InputName != TEXT("BladeUV") ||
        !InputMatches(Color->Inputs[0].Input, BladeUv, 0) ||
        Color->Inputs[1].InputName != TEXT("Random01") ||
        !InputMatches(Color->Inputs[1].Input, InstanceRandom, 0) ||
        Color->Inputs[2].InputName != TEXT("WorldPosition") ||
        !InputMatches(Color->Inputs[2].Input, WorldPosition, 0) ||
        !WorldUpNormal->Constant.Equals(
            FLinearColor(0.0f, 0.0f, 1.0f), 0.000001f) ||
        !InputMatches(
            FacingCorrectedNormal->A, ImportedCurvatureNormal, 0) ||
        !InputMatches(FacingCorrectedNormal->B, TwoSidedSign, 0) ||
        !InputMatches(DistanceMatchedNormal->A, WorldUpNormal, 0) ||
        !InputMatches(
            DistanceMatchedNormal->B, FacingCorrectedNormal, 0) ||
        !InputMatches(DistanceMatchedNormal->Alpha, InstanceFade, 0) ||
        !bExactDitherInput || Wind->Description != GrassWindDescription ||
        Wind->Code != GrassWindCode || Wind->OutputType != CMOT_Float3 ||
        !bExactWindInputs ||
        Roughness->Desc != TEXT("V5B_BERMUDA_HIGH_ROUGHNESS") ||
        Specular->Desc != TEXT("V5B_BERMUDA_LOW_SPECULAR") ||
        WindStrength->Desc != TEXT("V5B_BERMUDA_WIND_STRENGTH") ||
        WindSpeed->Desc != TEXT("V5B_BERMUDA_WIND_SPEED") ||
        WindHeight->Desc != TEXT("V5B_BERMUDA_WIND_HEIGHT") ||
        WindResponse->Desc != TEXT("V5B_BERMUDA_WIND_RESPONSE") ||
        MaximumWpo->Desc != TEXT("V5B_BERMUDA_MAX_WPO") ||
        WindDirection->Desc != TEXT("V5B_BERMUDA_WIND_DIRECTION") ||
        !FMath::IsNearlyEqual(
            Roughness->DefaultValue,
            Profile.Roughness,
            0.0001f) ||
        !FMath::IsNearlyEqual(
            Specular->DefaultValue,
            Profile.Specular,
            0.0001f) ||
        !FMath::IsNearlyEqual(
            WindStrength->DefaultValue, GrassWindStrengthCm, 0.0001f) ||
        !FMath::IsNearlyEqual(
            WindSpeed->DefaultValue, GrassWindSpeed, 0.0001f) ||
        !FMath::IsNearlyEqual(
            WindHeight->DefaultValue, GrassWindHeightCm, 0.0001f) ||
        !FMath::IsNearlyEqual(
            WindResponse->DefaultValue,
            Profile.WindResponse,
            0.0001f) ||
        !FMath::IsNearlyEqual(
            MaximumWpo->DefaultValue, GrassMaximumWpoCm, 0.0001f) ||
        !WindDirection->DefaultValue.Equals(
            GrassWindDirection, 0.0001f) ||
        !Subsurface->Constant.Equals(
            Profile.Subsurface,
            0.0001f) ||
        !InputMatches(Data->BaseColor, Color, 0) ||
        !InputMatches(Data->Normal, DistanceMatchedNormal, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->OpacityMask, DitheredInstanceFade, 0) ||
        !InputMatches(Data->SubsurfaceColor, Subsurface, 0) ||
        !InputMatches(Data->WorldPositionOffset, Wind, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->AmbientOcclusion.Expression ||
        Data->Displacement.Expression || Data->ClearCoat.Expression ||
        Data->ClearCoatRoughness.Expression || Data->Refraction.Expression ||
        Data->MaterialAttributes.Expression ||
        Data->PixelDepthOffset.Expression ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression || Data->FrontMaterial.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5D grass material %d lost its exact UV/random colour, fade-dither cutout, fade-matched normal, parameter or bounded-wind/WPO graph."),
            ProfileIndex);
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR12GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    const FString CurrentCode = BuildR12GrassColorCode(
        GrassProfiles[ProfileIndex]);
    if (CurrentCode.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        CurrentCode.Contains(TEXT("cos("), ESearchCase::CaseSensitive))
    {
        OutError = TEXT("The admitted R12 grass colour code must use arithmetic hashes with zero periodic operations.");
        return false;
    }
    return ValidateGrassMaterialVersion(
        Material,
        ProfileIndex,
        GrassProfiles[ProfileIndex],
        TEXT("_V3"),
        CurrentCode,
        OutError);
}

bool ValidateGrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (ProfileIndex < 0 ||
        ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames))
    {
        OutError = TEXT("R13 grass profile index is outside the exact four-profile roster.");
        return false;
    }
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const FGrassMaterialProfile& Profile = GrassProfiles[ProfileIndex];
    const FString ColorCode = BuildGrassColorCode(Profile);
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material,
        GrassDescriptionPrefix + Profile.Id + TEXT("_R13"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR13RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR13SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR13NormalAlphaDescription);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionCameraPositionWS* CameraPosition =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR13CameraDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionConstant3Vector* Attenuation =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR13SubsurfaceAttenuationDescription);
    UMaterialExpressionMultiply* Subsurface =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR13SubsurfaceDescription);
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"));
    UMaterialExpressionNormalize* FinalNormal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR13NormalDescription);
    UMaterialExpressionCustom* Wind =
        FindCustomByDescription(Material, GrassWindDescription);
    UMaterialExpressionMaterialFunctionCall* Dither =
        FindExpressionByDescription<UMaterialExpressionMaterialFunctionCall>(
            Material, TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"));
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    int32 TextureSamples = 0;
    int32 InstanceRandomNodes = 0;
    int32 InstanceFadeNodes = 0;
    bool bNoCustomizedUvConnections = Data != nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSamples +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
            {
                ++InstanceRandomNodes;
                if (Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
                {
                    InstanceRandom = Expression;
                }
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
            {
                ++InstanceFadeNodes;
                if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
                {
                    InstanceFade = Expression;
                }
            }
        }
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bNoCustomizedUvConnections &= !CustomizedUv.Expression;
        }
    }
    const bool bExactColorInputs = Color && Color->Inputs.Num() == 3 &&
        Color->Inputs[0].InputName == TEXT("BladeUV") &&
        InputMatches(Color->Inputs[0].Input, BladeUv, 0) &&
        Color->Inputs[1].InputName == TEXT("Random01") &&
        InputMatches(Color->Inputs[1].Input, InstanceRandom, 0) &&
        Color->Inputs[2].InputName == TEXT("WorldPosition") &&
        InputMatches(Color->Inputs[2].Input, WorldPosition, 0);
    const bool bExactSurfaceInputs = Roughness && Specular &&
        Roughness->Inputs.Num() == 3 && Specular->Inputs.Num() == 3 &&
        Roughness->Inputs[0].InputName == TEXT("BaseRoughness") &&
        InputMatches(Roughness->Inputs[0].Input, BaseRoughness, 0) &&
        Roughness->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Roughness->Inputs[1].Input, BladeUv, 0) &&
        Roughness->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Roughness->Inputs[2].Input, InstanceRandom, 0) &&
        Specular->Inputs[0].InputName == TEXT("BaseSpecular") &&
        InputMatches(Specular->Inputs[0].Input, BaseSpecular, 0) &&
        Specular->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Specular->Inputs[1].Input, BladeUv, 0) &&
        Specular->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Specular->Inputs[2].Input, InstanceRandom, 0);
    const bool bExactNormalAlphaInputs = NormalAlpha &&
        NormalAlpha->Inputs.Num() == 3 &&
        NormalAlpha->Inputs[0].InputName == TEXT("WorldPosition") &&
        InputMatches(NormalAlpha->Inputs[0].Input, WorldPosition, 0) &&
        NormalAlpha->Inputs[1].InputName == TEXT("CameraPosition") &&
        InputMatches(NormalAlpha->Inputs[1].Input, CameraPosition, 0) &&
        NormalAlpha->Inputs[2].InputName == TEXT("InstanceFade") &&
        InputMatches(NormalAlpha->Inputs[2].Input, InstanceFade, 0);
    if (!Material || !Data || !Profile.Id ||
        Material->GetPathName() != ObjectPath(GrassAssetNames[ProfileIndex]) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || !Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            MSM_TwoSidedFoliage) ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Data->ExpressionCollection.Expressions.Num() != 29 ||
        TextureSamples != 0 || InstanceRandomNodes != 1 ||
        InstanceFadeNodes != 1 || !bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        !Color || Color->Code != ColorCode ||
        Color->Code.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        Color->Code.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Color->OutputType != CMOT_Float3 || !bExactColorInputs ||
        !bExactSurfaceInputs || Roughness->Code != GrassR13RoughnessCode ||
        Roughness->OutputType != CMOT_Float1 ||
        Specular->Code != GrassR13SpecularCode ||
        Specular->OutputType != CMOT_Float1 ||
        !bExactNormalAlphaInputs ||
        NormalAlpha->Code != GrassR13NormalAlphaCode ||
        NormalAlpha->OutputType != CMOT_Float1 ||
        !BaseRoughness || !BaseSpecular ||
        !FMath::IsNearlyEqual(
            BaseRoughness->DefaultValue, Profile.Roughness, 0.0001f) ||
        !FMath::IsNearlyEqual(
            BaseSpecular->DefaultValue, Profile.Specular, 0.0001f) ||
        !Attenuation || !Attenuation->Constant.Equals(
            GrassR13SubsurfaceAttenuation[ProfileIndex], 0.0001f) ||
        !Subsurface || !InputMatches(Subsurface->A, Color, 0) ||
        !InputMatches(Subsurface->B, Attenuation, 0) ||
        !DistanceMatchedNormal ||
        !InputMatches(DistanceMatchedNormal->Alpha, NormalAlpha, 0) ||
        !FinalNormal ||
        !InputMatches(FinalNormal->VectorInput, DistanceMatchedNormal, 0) ||
        !Wind || Wind->Code != GrassWindCode ||
        Wind->OutputType != CMOT_Float3 || !Dither ||
        !InputMatches(Data->BaseColor, Color, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->Normal, FinalNormal, 0) ||
        !InputMatches(Data->SubsurfaceColor, Subsurface, 0) ||
        !InputMatches(Data->OpacityMask, Dither, 0) ||
        !InputMatches(Data->WorldPositionOffset, Wind, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->AmbientOcclusion.Expression ||
        Data->Displacement.Expression || Data->PixelDepthOffset.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5D R13 grass material %d lost its exact 29-node stochastic response, distance-normal or color-derived subsurface graph."),
            ProfileIndex);
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR15GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (ProfileIndex < 0 ||
        ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames))
    {
        OutError = TEXT("R15 grass profile index is outside the exact four-profile roster.");
        return false;
    }
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    const FGrassMaterialProfile& Profile = GrassProfiles[ProfileIndex];
    const FString ColorCode = BuildR15GrassColorCode(Profile);
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material,
        GrassDescriptionPrefix + Profile.Id + TEXT("_R15"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR15RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR15SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR15NormalAlphaDescription);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionCameraPositionWS* CameraPosition =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR15CameraDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionConstant3Vector* SubsurfaceGain =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR15SubsurfaceGainDescription);
    UMaterialExpressionMultiply* Subsurface =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR15SubsurfaceDescription);
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"));
    UMaterialExpressionNormalize* FinalNormal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR15NormalDescription);
    UMaterialExpressionCustom* Wind =
        FindCustomByDescription(Material, GrassWindDescription);
    UMaterialExpressionMaterialFunctionCall* Dither =
        FindExpressionByDescription<UMaterialExpressionMaterialFunctionCall>(
            Material, TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"));
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    int32 TextureSamples = 0;
    int32 InstanceRandomNodes = 0;
    int32 InstanceFadeNodes = 0;
    bool bNoCustomizedUvConnections = Data != nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSamples +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
            {
                ++InstanceRandomNodes;
                if (Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
                {
                    InstanceRandom = Expression;
                }
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
            {
                ++InstanceFadeNodes;
                if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
                {
                    InstanceFade = Expression;
                }
            }
        }
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bNoCustomizedUvConnections &= !CustomizedUv.Expression;
        }
    }
    const bool bExactColorInputs = Color && Color->Inputs.Num() == 3 &&
        Color->Inputs[0].InputName == TEXT("BladeUV") &&
        InputMatches(Color->Inputs[0].Input, BladeUv, 0) &&
        Color->Inputs[1].InputName == TEXT("Random01") &&
        InputMatches(Color->Inputs[1].Input, InstanceRandom, 0) &&
        Color->Inputs[2].InputName == TEXT("WorldPosition") &&
        InputMatches(Color->Inputs[2].Input, WorldPosition, 0);
    const bool bExactSurfaceInputs = Roughness && Specular &&
        Roughness->Inputs.Num() == 3 && Specular->Inputs.Num() == 3 &&
        Roughness->Inputs[0].InputName == TEXT("BaseRoughness") &&
        InputMatches(Roughness->Inputs[0].Input, BaseRoughness, 0) &&
        Roughness->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Roughness->Inputs[1].Input, BladeUv, 0) &&
        Roughness->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Roughness->Inputs[2].Input, InstanceRandom, 0) &&
        Specular->Inputs[0].InputName == TEXT("BaseSpecular") &&
        InputMatches(Specular->Inputs[0].Input, BaseSpecular, 0) &&
        Specular->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Specular->Inputs[1].Input, BladeUv, 0) &&
        Specular->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Specular->Inputs[2].Input, InstanceRandom, 0);
    const bool bExactNormalAlphaInputs = NormalAlpha &&
        NormalAlpha->Inputs.Num() == 5 &&
        NormalAlpha->Inputs[0].InputName == TEXT("WorldPosition") &&
        InputMatches(NormalAlpha->Inputs[0].Input, WorldPosition, 0) &&
        NormalAlpha->Inputs[1].InputName == TEXT("CameraPosition") &&
        InputMatches(NormalAlpha->Inputs[1].Input, CameraPosition, 0) &&
        NormalAlpha->Inputs[2].InputName == TEXT("InstanceFade") &&
        InputMatches(NormalAlpha->Inputs[2].Input, InstanceFade, 0) &&
        NormalAlpha->Inputs[3].InputName == TEXT("BladeUV") &&
        InputMatches(NormalAlpha->Inputs[3].Input, BladeUv, 0) &&
        NormalAlpha->Inputs[4].InputName == TEXT("Random01") &&
        InputMatches(NormalAlpha->Inputs[4].Input, InstanceRandom, 0);
    if (!Material || !Data || !Profile.Id ||
        Material->GetPathName() != ObjectPath(GrassAssetNames[ProfileIndex]) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || !Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            MSM_TwoSidedFoliage) ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Data->ExpressionCollection.Expressions.Num() != 29 ||
        TextureSamples != 0 || InstanceRandomNodes != 1 ||
        InstanceFadeNodes != 1 || !bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        !Color || Color->Code != ColorCode ||
        ColorCode.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        ColorCode.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Color->OutputType != CMOT_Float3 || !bExactColorInputs ||
        !bExactSurfaceInputs || Roughness->Code != GrassR15RoughnessCode ||
        Roughness->OutputType != CMOT_Float1 ||
        Specular->Code != GrassR15SpecularCode ||
        Specular->OutputType != CMOT_Float1 ||
        !bExactNormalAlphaInputs ||
        NormalAlpha->Code != GrassR15NormalAlphaCode ||
        NormalAlpha->OutputType != CMOT_Float1 ||
        !BaseRoughness || !BaseSpecular ||
        !FMath::IsNearlyEqual(
            BaseRoughness->DefaultValue, Profile.Roughness, 0.0001f) ||
        !FMath::IsNearlyEqual(
            BaseSpecular->DefaultValue, Profile.Specular, 0.0001f) ||
        !SubsurfaceGain || !SubsurfaceGain->Constant.Equals(
            GrassR15SubsurfaceGain[ProfileIndex], 0.0001f) ||
        !Subsurface || !InputMatches(Subsurface->A, Color, 0) ||
        !InputMatches(Subsurface->B, SubsurfaceGain, 0) ||
        !DistanceMatchedNormal ||
        !InputMatches(DistanceMatchedNormal->Alpha, NormalAlpha, 0) ||
        !FinalNormal ||
        !InputMatches(FinalNormal->VectorInput, DistanceMatchedNormal, 0) ||
        !Wind || Wind->Code != GrassWindCode ||
        Wind->OutputType != CMOT_Float3 || !Dither ||
        !InputMatches(Data->BaseColor, Color, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->Normal, FinalNormal, 0) ||
        !InputMatches(Data->SubsurfaceColor, Subsurface, 0) ||
        !InputMatches(Data->OpacityMask, Dither, 0) ||
        !InputMatches(Data->WorldPositionOffset, Wind, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->AmbientOcclusion.Expression ||
        Data->Displacement.Expression || Data->PixelDepthOffset.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5D R15 grass material %d lost its exact 29-node shade-readable response, five-input per-blade normal or color-derived subsurface graph."),
            ProfileIndex);
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR10GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    const FString R10Code = BuildR10GrassColorCode(
        R10GrassProfiles[ProfileIndex]);
    if (R10Code.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        R10Code.Contains(TEXT("cos("), ESearchCase::CaseSensitive))
    {
        OutError = TEXT("The admitted R10 grass colour code must use arithmetic hashes with zero periodic operations.");
        return false;
    }
    return ValidateGrassMaterialVersion(
        Material,
        ProfileIndex,
        R10GrassProfiles[ProfileIndex],
        TEXT("_V2"),
        R10Code,
        OutError);
}

bool ValidateLegacyGrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    return ValidateGrassMaterialVersion(
        Material,
        ProfileIndex,
        LegacyGrassProfiles[ProfileIndex],
        TEXT("_V1"),
        BuildLegacyGrassColorCode(LegacyGrassProfiles[ProfileIndex]),
        OutError);
}

bool ValidateSoilMaterial(UMaterial* Material, FString& OutError)
{
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionCustom* Base =
        FindCustomByDescription(Material, SoilBaseDescription);
    UMaterialExpressionCustom* Roughness =
        FindCustomByDescription(Material, SoilRoughnessDescription);
    UMaterialExpressionCustom* Normal =
        FindCustomByDescription(Material, SoilNormalDescription);
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material,
        TEXT("V5B_FORMAL_BED_WORLD_POSITION"));
    UMaterialExpressionTextureCoordinate* ApronUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material,
            TEXT("V5B_FORMAL_BED_APRON_FRACTION_UV0"));
    UMaterialExpressionScalarParameter* Specular =
        FindScalar(Material, TEXT("Specular"));
    int32 TextureSampleNodes = 0;
    bool bNoCustomizedUvConnections = Data != nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSampleNodes +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
        }
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bNoCustomizedUvConnections &= !CustomizedUv.Expression;
        }
    }
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != ObjectPath(SoilAssetName) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Opaque || Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !FMath::IsNearlyZero(
            Material->MaxWorldPositionOffsetDisplacement, 0.000001f) ||
        !Data || Data->ExpressionCollection.Expressions.Num() != 6 ||
        TextureSampleNodes != 0 || !bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        !Base || !Roughness || !Normal || !WorldPosition || !ApronUv ||
        !Specular ||
        Base->Code != SoilBaseCode || Roughness->Code != SoilRoughnessCode ||
        Normal->Code != SoilNormalCode ||
        WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        ApronUv->CoordinateIndex != 0 ||
        !FMath::IsNearlyEqual(ApronUv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(ApronUv->VTiling, 1.0f, 0.000001f) ||
        ApronUv->UnMirrorU || ApronUv->UnMirrorV ||
        Base->OutputType != CMOT_Float3 || Base->Inputs.Num() != 2 ||
        Roughness->Inputs.Num() != 1 || Normal->Inputs.Num() != 1 ||
        Base->Inputs[0].InputName != TEXT("WorldPosition") ||
        !InputMatches(Base->Inputs[0].Input, WorldPosition, 0) ||
        Base->Inputs[1].InputName != TEXT("ApronUV") ||
        !InputMatches(Base->Inputs[1].Input, ApronUv, 0) ||
        Roughness->OutputType != CMOT_Float1 ||
        Roughness->Inputs[0].InputName != TEXT("WorldPosition") ||
        !InputMatches(Roughness->Inputs[0].Input, WorldPosition, 0) ||
        Normal->OutputType != CMOT_Float3 ||
        Normal->Inputs[0].InputName != TEXT("WorldPosition") ||
        !InputMatches(Normal->Inputs[0].Input, WorldPosition, 0) ||
        Specular->Desc != TEXT("V5B_FORMAL_BED_SOIL_SPECULAR") ||
        !FMath::IsNearlyEqual(Specular->DefaultValue, 0.08f, 0.000001f) ||
        !InputMatches(Data->BaseColor, Base, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->Normal, Normal, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->OpacityMask.Expression ||
        Data->AmbientOcclusion.Expression ||
        Data->WorldPositionOffset.Expression || Data->Displacement.Expression ||
        Data->SubsurfaceColor.Expression || Data->ClearCoat.Expression ||
        Data->ClearCoatRoughness.Expression || Data->Refraction.Expression ||
        Data->MaterialAttributes.Expression ||
        Data->PixelDepthOffset.Expression ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression || Data->FrontMaterial.Expression)
    {
        OutError = TEXT("V5D soil material lost its exact six-node apron-UV/world-position base, roughness, specular or normal graph.");
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateLegacyGroundOverlayMaterial(UMaterial* Material, FString& OutError)
{
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionCustom* Macro =
        FindCustomByDescription(Material, GroundMacroDescription);
    UMaterialExpressionCustom* CoreMask =
        FindCustomByDescription(Material, GroundR15CoreMaskDescription);
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material,
        TEXT("Grass.WorldPositionForDistanceOnly"));
    UMaterialExpressionVectorParameter* LawnTint =
        FindVector(Material, TEXT("LawnTint"));
    UMaterialExpressionVectorParameter* MacroTintLow =
        FindVector(Material, TEXT("MacroTintLow"));
    UMaterialExpressionVectorParameter* MacroTintHigh =
        FindVector(Material, TEXT("MacroTintHigh"));
    UMaterialExpressionScalarParameter* Desaturation =
        FindScalar(Material, TEXT("Desaturation"));
    UMaterialExpressionScalarParameter* RoughnessBias =
        FindScalar(Material, TEXT("RoughnessBias"));
    UMaterialExpressionLinearInterpolate* MicroNormalAlpha =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material,
            TEXT("Grass.DistanceFadedMicroNormalAlpha"));
    UMaterialExpressionAdd* OriginalRoughness =
        FindExpressionByDescription<UMaterialExpressionAdd>(
            Material,
            TEXT("Grass.RoughnessWithBias"));
    UMaterialExpressionAdd* Centered =
        FindExpressionByDescription<UMaterialExpressionAdd>(
            Material,
            GroundMacroCenteredNode);
    UMaterialExpressionScalarParameter* Amplitude =
        FindScalar(Material, TEXT("V5D_MacroRoughnessAmplitude"));
    UMaterialExpressionMultiply* Variation =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material,
            GroundMacroVariationNode);
    UMaterialExpressionAdd* Combined =
        FindExpressionByDescription<UMaterialExpressionAdd>(
            Material,
            GroundRoughnessCombinedNode);
    UMaterialExpressionSaturate* FinalRoughness =
        FindExpressionByDescription<UMaterialExpressionSaturate>(
            Material,
            TEXT("Grass.FinalRoughness"));
    int32 TextureSampleCount = 0;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSampleCount +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
        }
    }
    if (!Material ||
        Material->GetPathName() != ObjectPath(GroundOverlayAssetName) ||
        Material->BlendMode != BLEND_Masked ||
        !FMath::IsNearlyEqual(
            Material->OpacityMaskClipValue,
            0.5f,
            0.0001f) ||
        Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Material->bTangentSpaceNormal || !Data || TextureSampleCount != 8 ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        !Macro || !CoreMask ||
        !WorldPosition ||
        !LawnTint || !MacroTintLow || !MacroTintHigh || !Desaturation ||
        !RoughnessBias || !MicroNormalAlpha || !OriginalRoughness ||
        !Centered || !Amplitude || !Variation || !Combined || !FinalRoughness ||
        Macro->Code != GroundMacroCode || Macro->OutputType != CMOT_Float1 ||
        Macro->Inputs.Num() != 2 ||
        Macro->Inputs[0].InputName != TEXT("MacroUV") ||
        !Macro->Inputs[0].Input.Expression ||
        Macro->Inputs[1].InputName != TEXT("WorldPosition") ||
        Macro->Inputs[1].Input.Expression != WorldPosition ||
        CoreMask->Code != GroundR15CoreMaskCode ||
        CoreMask->OutputType != CMOT_Float1 ||
        CoreMask->Inputs.Num() != 1 ||
        CoreMask->Inputs[0].InputName != TEXT("WorldPosition") ||
        CoreMask->Inputs[0].Input.Expression != WorldPosition ||
        Data->OpacityMask.Expression != CoreMask ||
        Data->OpacityMask.OutputIndex != 0 ||
        !LawnTint->DefaultValue.Equals(GroundLawnTint, 0.0001f) ||
        !MacroTintLow->DefaultValue.Equals(GroundMacroTintLow, 0.0001f) ||
        !MacroTintHigh->DefaultValue.Equals(GroundMacroTintHigh, 0.0001f) ||
        !FMath::IsNearlyEqual(
            Desaturation->DefaultValue,
            GroundDesaturation,
            0.0001f) ||
        !FMath::IsNearlyEqual(
            RoughnessBias->DefaultValue,
            GroundRoughnessBias,
            0.0001f) ||
        MicroNormalAlpha->A.Expression ||
        !FMath::IsNearlyEqual(
            MicroNormalAlpha->ConstA,
            GroundFarMicroNormalStrength,
            0.0001f) ||
        Centered->A.Expression != Macro || Centered->B.Expression ||
        !FMath::IsNearlyEqual(Centered->ConstB, -0.5f, 0.0001f) ||
        Amplitude->Desc != GroundMacroAmplitudeNode ||
        !FMath::IsNearlyEqual(
            Amplitude->DefaultValue,
            GroundMacroRoughnessAmplitude,
            0.0001f) ||
        Variation->A.Expression != Centered ||
        Variation->B.Expression != Amplitude ||
        Combined->A.Expression != OriginalRoughness ||
        Combined->B.Expression != Variation ||
        FinalRoughness->Input.Expression != Combined)
    {
        OutError = TEXT("V5D lawn overlay lost its exact world-stochastic chroma/roughness graph or its compact irregular-ellipse outward-feather mask.");
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

int32 CountCodeToken(const FString& Code, const FString& Token)
{
    int32 Count = 0;
    int32 SearchFrom = 0;
    while (SearchFrom < Code.Len())
    {
        const int32 Found = Code.Find(
            Token,
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            SearchFrom);
        if (Found == INDEX_NONE)
        {
            break;
        }
        ++Count;
        SearchFrom = Found + Token.Len();
    }
    return Count;
}

bool ValidateGroundOverlayMaterialVersion(
    UMaterial* Material,
    const FString& ExpectedBaseTexturePath,
    const FString& ExpectedNormalTexturePath,
    const FString& ExpectedRoughnessTexturePath,
    const FString& ExpectedAoTexturePath,
    const FString& ExpectedBaseDescription,
    const FString& ExpectedBaseCode,
    const FString& ExpectedRoughnessDescription,
    const FString& ExpectedRoughnessCode,
    float ExpectedNearNormalStrength,
    float ExpectedFarNormalStrength,
    float ExpectedSpecular,
    const FString& ExpectedCoreMaskDescription,
    const FString& ExpectedCoreMaskCode,
    const TCHAR* Revision,
    FString& OutError,
    bool bR22Topology = false,
    bool bR23BCalibration = false)
{
    const bool bR23Family =
        FCString::Strcmp(Revision, TEXT("R23")) == 0 ||
        FCString::Strcmp(Revision, TEXT("R23B")) == 0;
    const FString& ExpectedRevisionDescription = bR23Family
        ? R23RuntimeRevisionDescription
        : R22RuntimeRevisionDescription;
    const float ExpectedRevisionValue = bR23Family
        ? R23RuntimeRevisionValue
        : R22RuntimeRevisionValue;
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionScalarParameter* TileMeters =
        FindScalar(Material, TEXT("V5D_R10_TextureTileMeters"));
    UMaterialExpressionCustom* SurfaceUv =
        FindCustomByDescription(Material, GroundR10SurfaceUvDescription);
    UMaterialExpressionTextureSampleParameter2D* BaseSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.BaseColorTexture"));
    UMaterialExpressionTextureSampleParameter2D* NormalSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.NormalTexture"));
    UMaterialExpressionTextureSampleParameter2D* RoughnessSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.RoughnessTexture"));
    UMaterialExpressionTextureSampleParameter2D* AoSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.AoTexture"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5D.R10.WorldPositionNoOffsets"));
    UMaterialExpressionCameraPositionWS* CameraPosition =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, TEXT("V5D.R10.CameraPosition"));
    UMaterialExpressionCustom* Field =
        FindCustomByDescription(Material, GroundR10FieldDescription);
    UMaterialExpressionComponentMask* Macro =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.Macro11m48m"));
    UMaterialExpressionComponentMask* Mowing =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.Mowing3p2m7deg"));
    UMaterialExpressionComponentMask* Wear =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.BoundedWear"));
    UMaterialExpressionComponentMask* Wetness =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.BoundedWetness"));
    UMaterialExpressionCustom* NearDetail =
        FindCustomByDescription(
            Material,
            bR22Topology
                ? GroundR22FarFadeDescription
                : GroundR10FarFadeDescription);
    UMaterialExpressionCustom* MipBias = bR22Topology
        ? FindCustomByDescription(Material, GroundR22MipBiasDescription)
        : nullptr;
    UMaterialExpressionCustom* Base =
        FindCustomByDescription(Material, ExpectedBaseDescription);
    UMaterialExpressionCustom* Roughness =
        FindCustomByDescription(Material, ExpectedRoughnessDescription);
    UMaterialExpressionCustom* Ao =
        FindCustomByDescription(Material, GroundR10AoDescription);
    UMaterialExpressionConstant3Vector* FlatNormal =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, TEXT("V5D.R10.FlatNormal"));
    UMaterialExpressionLinearInterpolate* NormalStrength =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormalStrength"));
    UMaterialExpressionLinearInterpolate* NormalBlend =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormal"));
    UMaterialExpressionNormalize* FinalNormal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, TEXT("V5D.R10.FinalNormal"));
    UMaterialExpressionScalarParameter* Specular =
        FindScalar(Material, TEXT("V5D_R10_Specular"));
    UMaterialExpressionScalarParameter* RevisionMarker = bR22Topology
        ? FindScalar(Material, R22RuntimeRevisionParameterName)
        : nullptr;
    UMaterialExpressionScalarParameter* CalibrationRevisionMarker =
        bR23BCalibration
        ? FindScalar(Material, R23BRuntimeCalibrationRevisionParameterName)
        : nullptr;
    UMaterialExpressionCustom* CoreMask =
        FindCustomByDescription(Material, ExpectedCoreMaskDescription);

    int32 TextureSampleCount = 0;
    bool bNoCustomizedUvConnections = Data != nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSampleCount +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
        }
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bNoCustomizedUvConnections &= !CustomizedUv.Expression;
        }
    }
    const auto SampleMatches = [SurfaceUv, MipBias, bR22Topology](
        const UMaterialExpressionTextureSampleParameter2D* Sample,
        const FString& Path,
        EMaterialSamplerType SamplerType,
        FName ParameterName)
    {
        return Sample && Sample->Texture &&
            Sample->Texture->GetPathName() == Path &&
            Sample->ParameterName == ParameterName &&
            Sample->SamplerType == SamplerType &&
            Sample->SamplerSource == SSM_FromTextureAsset &&
            Sample->MipValueMode ==
                (bR22Topology ? TMVM_MipBias : TMVM_None) &&
            Sample->ConstCoordinate == 0 &&
            Sample->ConstMipValue == INDEX_NONE &&
            Sample->AutomaticViewMipBias &&
            !Sample->TextureObject.Expression &&
            (bR22Topology
                ? InputMatches(Sample->MipValue, MipBias, 0)
                : !Sample->MipValue.Expression) &&
            !Sample->CoordinatesDX.Expression &&
            !Sample->CoordinatesDY.Expression &&
            !Sample->AutomaticViewMipBiasValue.Expression &&
            InputMatches(Sample->Coordinates, SurfaceUv, 0);
    };
    const bool bMasksMatch = Macro && Mowing && Wear && Wetness && Field &&
        InputMatches(Macro->Input, Field, 0) && Macro->R && !Macro->G &&
        !Macro->B && !Macro->A && InputMatches(Mowing->Input, Field, 0) &&
        !Mowing->R && Mowing->G && !Mowing->B && !Mowing->A &&
        InputMatches(Wear->Input, Field, 0) && !Wear->R && !Wear->G && Wear->B &&
        !Wear->A && InputMatches(Wetness->Input, Field, 0) && !Wetness->R &&
        !Wetness->G && !Wetness->B && Wetness->A;
    if (!Material ||
        Material->GetPathName() != ObjectPath(GroundOverlayAssetName) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bScreenSpaceReflections || Material->bEnableTessellation ||
        Material->bEnableDisplacementFade ||
        !FMath::IsNearlyZero(Material->MaxWorldPositionOffsetDisplacement) ||
        !Material->GetUsageByFlag(MATUSAGE_Nanite) ||
        !Data || TextureSampleCount != 4 ||
        Data->ExpressionCollection.Expressions.Num() !=
            (bR22Topology ? (bR23BCalibration ? 26 : 25) : 23) ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        Material->bUsedWithInstancedStaticMeshes ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() || !bNoCustomizedUvConnections ||
        !TileMeters || !SurfaceUv ||
        !FMath::IsNearlyEqual(
            TileMeters->DefaultValue, GroundR10TextureTileMeters, 0.0001f) ||
        SurfaceUv->Code != GroundR10SurfaceUvCode ||
        SurfaceUv->OutputType != CMOT_Float2 || SurfaceUv->Inputs.Num() != 2 ||
        SurfaceUv->Inputs[0].InputName != TEXT("WorldPosition") ||
        !InputMatches(SurfaceUv->Inputs[0].Input, WorldPosition, 0) ||
        SurfaceUv->Inputs[1].InputName != TEXT("TileMeters") ||
        !InputMatches(SurfaceUv->Inputs[1].Input, TileMeters, 0) ||
        !SampleMatches(
            BaseSample,
            ExpectedBaseTexturePath,
            SAMPLERTYPE_Color,
            TEXT("V5D_R10_BaseColorTexture")) ||
        !SampleMatches(
            NormalSample,
            ExpectedNormalTexturePath,
            SAMPLERTYPE_Normal,
            TEXT("V5D_R10_NormalTexture")) ||
        !SampleMatches(
            RoughnessSample,
            ExpectedRoughnessTexturePath,
            SAMPLERTYPE_Masks,
            TEXT("V5D_R10_RoughnessTexture")) ||
        !SampleMatches(
            AoSample,
            ExpectedAoTexturePath,
            SAMPLERTYPE_Masks,
            TEXT("V5D_R10_AoTexture")) ||
        !WorldPosition ||
        WorldPosition->WorldPositionShaderOffset != WPT_ExcludeAllShaderOffsets ||
        !CameraPosition || !Field || Field->Code != GroundR10FieldCode ||
        CountCodeToken(Field->Code, TEXT("sin(")) != 1 ||
        Field->Code.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Field->OutputType != CMOT_Float4 || Field->Inputs.Num() != 1 ||
        Field->Inputs[0].InputName != TEXT("WorldPosition") ||
        !InputMatches(Field->Inputs[0].Input, WorldPosition, 0) || !bMasksMatch ||
        !NearDetail || NearDetail->Code !=
            (bR22Topology ? GroundR22FarFadeCode : GroundR10FarFadeCode) ||
        NearDetail->OutputType != CMOT_Float1 || NearDetail->Inputs.Num() != 2 ||
        NearDetail->Inputs[0].InputName != TEXT("WorldPosition") ||
        !InputMatches(NearDetail->Inputs[0].Input, WorldPosition, 0) ||
        NearDetail->Inputs[1].InputName != TEXT("CameraPosition") ||
        !InputMatches(NearDetail->Inputs[1].Input, CameraPosition, 0) ||
        (bR22Topology &&
            (!MipBias || MipBias->Code != GroundR22MipBiasCode ||
             MipBias->OutputType != CMOT_Float1 ||
             MipBias->Inputs.Num() != 1 ||
             MipBias->Inputs[0].InputName != TEXT("NearDetail") ||
             !InputMatches(MipBias->Inputs[0].Input, NearDetail, 0))) ||
        !Base || Base->Code != ExpectedBaseCode ||
        Base->OutputType != CMOT_Float3 ||
        Base->Inputs.Num() !=
            (bR22Topology ? (bR23BCalibration ? 10 : 9) : 6) ||
        Base->Inputs[0].InputName != TEXT("BaseColor") ||
        !InputMatches(Base->Inputs[0].Input, BaseSample, 0) ||
        Base->Inputs[1].InputName != TEXT("Macro") ||
        !InputMatches(Base->Inputs[1].Input, Macro, 0) ||
        Base->Inputs[2].InputName != TEXT("Mowing") ||
        !InputMatches(Base->Inputs[2].Input, Mowing, 0) ||
        Base->Inputs[3].InputName != TEXT("Wear") ||
        !InputMatches(Base->Inputs[3].Input, Wear, 0) ||
        Base->Inputs[4].InputName != TEXT("Wetness") ||
        !InputMatches(Base->Inputs[4].Input, Wetness, 0) ||
        Base->Inputs[5].InputName != TEXT("NearDetail") ||
        !InputMatches(Base->Inputs[5].Input, NearDetail, 0) ||
        (bR22Topology &&
            (Base->Inputs[6].InputName != TEXT("WorldPosition") ||
             !InputMatches(Base->Inputs[6].Input, WorldPosition, 0) ||
             Base->Inputs[7].InputName != TEXT("CameraPosition") ||
             !InputMatches(Base->Inputs[7].Input, CameraPosition, 0) ||
             Base->Inputs[8].InputName != TEXT("MaterialRevision") ||
             !InputMatches(Base->Inputs[8].Input, RevisionMarker, 0))) ||
        (bR23BCalibration &&
            (Base->Inputs[9].InputName != TEXT("CalibrationRevision") ||
             !InputMatches(
                 Base->Inputs[9].Input,
                 CalibrationRevisionMarker,
                 0))) ||
        !Roughness || Roughness->Code != ExpectedRoughnessCode ||
        Roughness->OutputType != CMOT_Float1 ||
        Roughness->Inputs.Num() != 6 ||
        Roughness->Inputs[0].InputName != TEXT("TextureRoughness") ||
        !InputMatches(Roughness->Inputs[0].Input, RoughnessSample, 1) ||
        Roughness->Inputs[1].InputName != TEXT("Macro") ||
        !InputMatches(Roughness->Inputs[1].Input, Macro, 0) ||
        Roughness->Inputs[2].InputName != TEXT("Mowing") ||
        !InputMatches(Roughness->Inputs[2].Input, Mowing, 0) ||
        Roughness->Inputs[3].InputName != TEXT("Wear") ||
        !InputMatches(Roughness->Inputs[3].Input, Wear, 0) ||
        Roughness->Inputs[4].InputName != TEXT("Wetness") ||
        !InputMatches(Roughness->Inputs[4].Input, Wetness, 0) ||
        Roughness->Inputs[5].InputName != TEXT("NearDetail") ||
        !InputMatches(Roughness->Inputs[5].Input, NearDetail, 0) ||
        !Ao || Ao->Code != GroundR10AoCode || Ao->OutputType != CMOT_Float1 ||
        Ao->Inputs.Num() != 2 ||
        Ao->Inputs[0].InputName != TEXT("TextureAo") ||
        !InputMatches(Ao->Inputs[0].Input, AoSample, 1) ||
        Ao->Inputs[1].InputName != TEXT("NearDetail") ||
        !InputMatches(Ao->Inputs[1].Input, NearDetail, 0) ||
        !FlatNormal || !FlatNormal->Constant.Equals(
            FLinearColor(0.0f, 0.0f, 1.0f), 0.0001f) ||
        !NormalStrength || NormalStrength->A.Expression ||
        NormalStrength->B.Expression ||
        !FMath::IsNearlyEqual(
            NormalStrength->ConstA, ExpectedFarNormalStrength, 0.0001f) ||
        !FMath::IsNearlyEqual(
            NormalStrength->ConstB, ExpectedNearNormalStrength, 0.0001f) ||
        !InputMatches(NormalStrength->Alpha, NearDetail, 0) || !NormalBlend ||
        !InputMatches(NormalBlend->A, FlatNormal, 0) ||
        !InputMatches(NormalBlend->B, NormalSample, 0) ||
        !InputMatches(NormalBlend->Alpha, NormalStrength, 0) || !FinalNormal ||
        !InputMatches(FinalNormal->VectorInput, NormalBlend, 0) || !Specular ||
        !FMath::IsNearlyEqual(
            Specular->DefaultValue, ExpectedSpecular, 0.0001f) ||
        (bR22Topology &&
            (!RevisionMarker ||
             RevisionMarker->Desc != ExpectedRevisionDescription ||
             !FMath::IsNearlyEqual(
                 RevisionMarker->DefaultValue,
                  ExpectedRevisionValue,
                  0.0001f))) ||
        (bR23BCalibration &&
            (!CalibrationRevisionMarker ||
             CalibrationRevisionMarker->Desc !=
                 R23BRuntimeCalibrationRevisionDescription ||
             !FMath::IsNearlyEqual(
                 CalibrationRevisionMarker->DefaultValue,
                 R23BRuntimeCalibrationRevisionValue,
                 0.0001f))) ||
        !CoreMask || CoreMask->Code != ExpectedCoreMaskCode ||
        CoreMask->OutputType != CMOT_Float1 || CoreMask->Inputs.Num() != 1 ||
        CoreMask->Inputs[0].InputName != TEXT("WorldPosition") ||
        !InputMatches(CoreMask->Inputs[0].Input, WorldPosition, 0) ||
        !InputMatches(Data->BaseColor, Base, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->Normal, FinalNormal, 0) ||
        !InputMatches(Data->AmbientOcclusion, Ao, 0) ||
        !InputMatches(Data->OpacityMask, CoreMask, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->WorldPositionOffset.Expression ||
        Data->Displacement.Expression || Data->SubsurfaceColor.Expression ||
        Data->ClearCoat.Expression || Data->ClearCoatRoughness.Expression ||
        Data->Refraction.Expression || Data->MaterialAttributes.Expression ||
        Data->PixelDepthOffset.Expression ||
        Data->ShadingModelFromMaterialExpression.Expression ||
        Data->SurfaceThickness.Expression || Data->FrontMaterial.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5D %s lawn overlay lost its exact four-sample, 3.2 m mowing, 11/48 m field, revision-specific distance filtering, or bounded wear/wetness graph."),
            Revision);
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR10GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundR12BaseColorTexturePath,
        GroundR12NormalTexturePath,
        GroundR12RoughnessTexturePath,
        GroundR12AoTexturePath,
        GroundR10BaseDescription,
        GroundR10BaseCode,
        GroundR10RoughnessDescription,
        GroundR10RoughnessCode,
        GroundR10NearNormalStrength,
        GroundR10FarNormalStrength,
        GroundR10Specular,
        GroundR15CoreMaskDescription,
        GroundR15CoreMaskCode,
        TEXT("R10"),
        OutError);
}

bool ValidateR12GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundR12BaseColorTexturePath,
        GroundR12NormalTexturePath,
        GroundR12RoughnessTexturePath,
        GroundR12AoTexturePath,
        GroundR12BaseDescription,
        GroundR12BaseCode,
        GroundR12RoughnessDescription,
        GroundR12RoughnessCode,
        GroundR12NearNormalStrength,
        GroundR12FarNormalStrength,
        GroundR12Specular,
        GroundR15CoreMaskDescription,
        GroundR15CoreMaskCode,
        TEXT("R12"),
        OutError);
}

bool ValidateGroundOverlayMaterial(UMaterial* Material, FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR13BaseDescription,
        GroundR13BaseCode,
        GroundR13RoughnessDescription,
        GroundR13RoughnessCode,
        GroundR13NearNormalStrength,
        GroundR13FarNormalStrength,
        GroundR13Specular,
        GroundR15CoreMaskDescription,
        GroundR15CoreMaskCode,
        TEXT("R13"),
        OutError);
}

bool ValidateR15GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR15BaseDescription,
        GroundR15BaseCode,
        GroundR15RoughnessDescription,
        GroundR15RoughnessCode,
        GroundR15NearNormalStrength,
        GroundR15FarNormalStrength,
        GroundR15Specular,
        GroundR15CoreMaskDescription,
        GroundR15CoreMaskCode,
        TEXT("R15"),
        OutError);
}

bool ValidateR16GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR15BaseDescription,
        GroundR15BaseCode,
        GroundR15RoughnessDescription,
        GroundR15RoughnessCode,
        GroundR15NearNormalStrength,
        GroundR15FarNormalStrength,
        GroundR15Specular,
        GroundCoreMaskDescription,
        GroundCoreMaskCode,
        TEXT("R16"),
        OutError);
}

bool ValidateR17GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    // R17 retains the R16 provider-seam mask byte-for-byte in source terms;
    // only photographic lawn response nodes are revised.
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR17BaseDescription,
        GroundR17BaseCode,
        GroundR17RoughnessDescription,
        GroundR17RoughnessCode,
        GroundR17NearNormalStrength,
        GroundR17FarNormalStrength,
        GroundR17Specular,
        GroundCoreMaskDescription,
        GroundCoreMaskCode,
        TEXT("R17"),
        OutError);
}

bool ValidateR18GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    // R18 is deliberately a one-node calibration: the R17 base response keeps
    // its exact wiring and changes only its revision tag and tint literal.
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR18BaseDescription,
        GroundR18BaseCode,
        GroundR17RoughnessDescription,
        GroundR17RoughnessCode,
        GroundR17NearNormalStrength,
        GroundR17FarNormalStrength,
        GroundR17Specular,
        GroundCoreMaskDescription,
        GroundCoreMaskCode,
        TEXT("R18"),
        OutError);
}

bool ValidateR22GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR22BaseDescription,
        GroundR22BaseCode,
        GroundR22RoughnessDescription,
        GroundR22RoughnessCode,
        GroundR22NearNormalStrength,
        GroundR22FarNormalStrength,
        GroundR22Specular,
        GroundCoreMaskDescription,
        GroundCoreMaskCode,
        TEXT("R22"),
        OutError,
        true);
}

bool ValidateR23GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR23BaseDescription,
        GroundR23BaseCode,
        GroundR23RoughnessDescription,
        GroundR23RoughnessCode,
        GroundR23NearNormalStrength,
        GroundR23FarNormalStrength,
        GroundR23Specular,
        GroundCoreMaskDescription,
        GroundCoreMaskCode,
        TEXT("R23"),
        OutError,
        true);
}

bool ValidateR23BGroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    return ValidateGroundOverlayMaterialVersion(
        Material,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath,
        GroundR23BBaseDescription,
        GroundR23BBaseCode,
        GroundR23BRoughnessDescription,
        GroundR23BRoughnessCode,
        GroundR23BNearNormalStrength,
        GroundR23BFarNormalStrength,
        GroundR23BSpecular,
        GroundCoreMaskDescription,
        GroundCoreMaskCode,
        TEXT("R23B"),
        OutError,
        true,
        true);
}

bool ValidateR17GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError);

bool ValidateR19GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError);

bool ValidateR21GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError);

bool ValidateR22GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError);

bool ValidateR23GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError);

bool ValidateR23BGrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError);

UMaterial* DuplicateMaterialFresh(
    UMaterial* Source,
    const FString& AssetName,
    FString& OutError);

struct FEdgeGrassMaterialGraph
{
    UMaterialEditorOnlyData* Data = nullptr;
    UMaterialExpressionTextureSampleParameter2D* Base = nullptr;
    UMaterialExpressionTextureSampleParameter2D* Normal = nullptr;
    UMaterialExpressionTextureSampleParameter2D* Roughness = nullptr;
    UMaterialExpressionTextureSampleParameter2D* Opacity = nullptr;
    UMaterialExpressionWorldPosition* WorldPosition = nullptr;
    UMaterialExpressionCameraPositionWS* CameraPosition = nullptr;
    UMaterialExpressionTransformPosition* InstanceLocalPosition = nullptr;
    UMaterialExpressionTime* Time = nullptr;
    UMaterialExpressionScalarParameter* Strength = nullptr;
    UMaterialExpressionScalarParameter* Speed = nullptr;
    UMaterialExpressionScalarParameter* Height = nullptr;
    UMaterialExpressionScalarParameter* Response = nullptr;
    UMaterialExpressionScalarParameter* MaximumWpo = nullptr;
    UMaterialExpressionVectorParameter* Direction = nullptr;
    UMaterialExpressionCustom* Wind = nullptr;
    UMaterialExpressionMultiply* CorrectedNormal = nullptr;
    UMaterialExpressionAppendVector* Facing = nullptr;
    UMaterialExpressionConstant2Vector* TangentXY = nullptr;
    UMaterialExpressionTwoSidedSign* FacingSign = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpressionMaterialFunctionCall* Dither = nullptr;
    UMaterialExpressionCustom* StableVisibility = nullptr;
    UMaterialExpressionScalarParameter* RevisionMarker = nullptr;
    UMaterialExpressionScalarParameter* CalibrationRevisionMarker = nullptr;
    UMaterialExpressionMultiply* FadeMultiply = nullptr;
    int32 TextureSamples = 0;
    int32 InstanceFadeNodes = 0;
    int32 InstanceRandomNodes = 0;
    int32 DitherNodes = 0;
    int32 MultiplyNodes = 0;
    bool bNoCustomizedUvConnections = false;
};

bool CollectEdgeGrassMaterialGraph(
    UMaterial* Material,
    FEdgeGrassMaterialGraph& OutGraph)
{
    OutGraph = FEdgeGrassMaterialGraph();
    OutGraph.Data = Material ? Material->GetEditorOnlyData() : nullptr;
    if (!OutGraph.Data)
    {
        return false;
    }
    OutGraph.CorrectedNormal = Cast<UMaterialExpressionMultiply>(
        OutGraph.Data->Normal.Expression);
    OutGraph.Facing = OutGraph.CorrectedNormal
        ? Cast<UMaterialExpressionAppendVector>(
              OutGraph.CorrectedNormal->B.Expression)
        : nullptr;
    OutGraph.TangentXY = OutGraph.Facing
        ? Cast<UMaterialExpressionConstant2Vector>(
              OutGraph.Facing->A.Expression)
        : nullptr;
    OutGraph.FacingSign = OutGraph.Facing
        ? Cast<UMaterialExpressionTwoSidedSign>(
              OutGraph.Facing->B.Expression)
        : nullptr;
    OutGraph.bNoCustomizedUvConnections = true;
    for (const FVector2MaterialInput& CustomizedUv :
         OutGraph.Data->CustomizedUVs)
    {
        OutGraph.bNoCustomizedUvConnections &=
            !CustomizedUv.Expression;
    }
    for (UMaterialExpression* Expression :
         OutGraph.Data->ExpressionCollection.Expressions)
    {
        if (UMaterialExpressionTextureSampleParameter2D* Sample =
                Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
        {
            ++OutGraph.TextureSamples;
            if (Sample->ParameterName == TEXT("BaseColorTexture"))
            {
                OutGraph.Base = OutGraph.Base ? nullptr : Sample;
            }
            else if (Sample->ParameterName == TEXT("NormalTexture"))
            {
                OutGraph.Normal = OutGraph.Normal ? nullptr : Sample;
            }
            else if (Sample->ParameterName == TEXT("RoughnessTexture"))
            {
                OutGraph.Roughness = OutGraph.Roughness ? nullptr : Sample;
            }
            else if (Sample->ParameterName == TEXT("OpacityTexture"))
            {
                OutGraph.Opacity = OutGraph.Opacity ? nullptr : Sample;
            }
        }
        if (UMaterialExpressionWorldPosition* Position =
                Cast<UMaterialExpressionWorldPosition>(Expression))
        {
            OutGraph.WorldPosition = OutGraph.WorldPosition
                ? nullptr : Position;
        }
        if (UMaterialExpressionCameraPositionWS* Camera =
                Cast<UMaterialExpressionCameraPositionWS>(Expression))
        {
            OutGraph.CameraPosition = OutGraph.CameraPosition
                ? nullptr : Camera;
        }
        if (UMaterialExpressionTransformPosition* Transform =
                Cast<UMaterialExpressionTransformPosition>(Expression))
        {
            OutGraph.InstanceLocalPosition = OutGraph.InstanceLocalPosition
                ? nullptr : Transform;
        }
        if (UMaterialExpressionTime* Time =
                Cast<UMaterialExpressionTime>(Expression))
        {
            OutGraph.Time = OutGraph.Time ? nullptr : Time;
        }
        if (UMaterialExpressionScalarParameter* Scalar =
                Cast<UMaterialExpressionScalarParameter>(Expression))
        {
            if (Scalar->ParameterName == TEXT("TRIAD_WindStrengthCm"))
                OutGraph.Strength = OutGraph.Strength ? nullptr : Scalar;
            if (Scalar->ParameterName == TEXT("TRIAD_WindSpeed"))
                OutGraph.Speed = OutGraph.Speed ? nullptr : Scalar;
            if (Scalar->ParameterName == TEXT("TRIAD_WindHeightCm"))
                OutGraph.Height = OutGraph.Height ? nullptr : Scalar;
            if (Scalar->ParameterName == TEXT("TRIAD_WindResponseScale"))
                OutGraph.Response = OutGraph.Response ? nullptr : Scalar;
            if (Scalar->ParameterName == TEXT("TRIAD_MaxWpoCm"))
                OutGraph.MaximumWpo = OutGraph.MaximumWpo ? nullptr : Scalar;
            if (Scalar->ParameterName == R22RuntimeRevisionParameterName)
                OutGraph.RevisionMarker = OutGraph.RevisionMarker
                    ? nullptr : Scalar;
            if (Scalar->ParameterName ==
                    R23BRuntimeCalibrationRevisionParameterName)
                OutGraph.CalibrationRevisionMarker =
                    OutGraph.CalibrationRevisionMarker ? nullptr : Scalar;
        }
        if (UMaterialExpressionVectorParameter* Vector =
                Cast<UMaterialExpressionVectorParameter>(Expression))
        {
            if (Vector->ParameterName == TEXT("TRIAD_WindDirection"))
            {
                OutGraph.Direction = OutGraph.Direction ? nullptr : Vector;
            }
        }
        if (UMaterialExpressionCustom* Custom =
                Cast<UMaterialExpressionCustom>(Expression))
        {
            if (Custom->Description == EdgeGrassSourceWindDescription &&
                Custom->Code == EdgeGrassSourceWindCode)
            {
                OutGraph.Wind = OutGraph.Wind ? nullptr : Custom;
            }
            if (Custom->Description ==
                    EdgeGrassR22StableVisibilityDescription &&
                Custom->Code == GrassR22StableVisibilityCode)
            {
                OutGraph.StableVisibility = OutGraph.StableVisibility
                    ? nullptr : Custom;
            }
            if (Custom->Description ==
                    EdgeGrassR23StableVisibilityDescription &&
                Custom->Code == GrassR23StableVisibilityCode)
            {
                OutGraph.StableVisibility = OutGraph.StableVisibility
                    ? nullptr : Custom;
            }
            if (Custom->Description ==
                    EdgeGrassR23BStableVisibilityDescription &&
                Custom->Code == GrassR23BStableVisibilityCode)
            {
                OutGraph.StableVisibility = OutGraph.StableVisibility
                    ? nullptr : Custom;
            }
        }
        if (Expression && Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
        {
            ++OutGraph.InstanceFadeNodes;
            if (Expression->Desc == EdgeGrassFadeNodeDescription)
            {
                OutGraph.InstanceFade = OutGraph.InstanceFade
                    ? nullptr : Expression;
            }
        }
        if (Expression && Expression->GetClass()->GetPathName() ==
                TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
        {
            ++OutGraph.InstanceRandomNodes;
            if (Expression->Desc == EdgeGrassR22RandomDescription)
            {
                OutGraph.InstanceRandom = OutGraph.InstanceRandom
                    ? nullptr : Expression;
            }
        }
        if (UMaterialExpressionMaterialFunctionCall* Function =
                Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
        {
            ++OutGraph.DitherNodes;
            if (Function->Desc == EdgeGrassFadeDitherDescription)
            {
                OutGraph.Dither = OutGraph.Dither ? nullptr : Function;
            }
        }
        if (UMaterialExpressionMultiply* Multiply =
                Cast<UMaterialExpressionMultiply>(Expression))
        {
            ++OutGraph.MultiplyNodes;
            if (Multiply->Desc == EdgeGrassFadeMultiplyDescription)
            {
                OutGraph.FadeMultiply = OutGraph.FadeMultiply
                    ? nullptr : Multiply;
            }
        }
    }
    return true;
}

bool ValidateEdgeGrassMaterialGraph(
    UMaterial* Material,
    const FString& ExpectedObjectPath,
    bool bRequireFadeDerivative,
    FEdgeGrassMaterialGraph& OutGraph,
    FString& OutError,
    bool bRequireR22StableDerivative = false,
    bool bRequireR23StableDerivative = false,
    bool bRequireR23BCalibratedDerivative = false)
{
    const bool bRequireStableDerivative =
        bRequireR22StableDerivative || bRequireR23StableDerivative ||
        bRequireR23BCalibratedDerivative;
    const FString& ExpectedStableCode = bRequireR23BCalibratedDerivative
        ? GrassR23BStableVisibilityCode
        : (bRequireR23StableDerivative
            ? GrassR23StableVisibilityCode : GrassR22StableVisibilityCode);
    const FString& ExpectedRevisionDescription =
        (bRequireR23StableDerivative || bRequireR23BCalibratedDerivative)
        ? R23RuntimeRevisionDescription : R22RuntimeRevisionDescription;
    const float ExpectedRevisionValue =
        (bRequireR23StableDerivative || bRequireR23BCalibratedDerivative)
        ? R23RuntimeRevisionValue : R22RuntimeRevisionValue;
    if (!CollectEdgeGrassMaterialGraph(Material, OutGraph))
    {
        OutError = TEXT("The edge-grass material has no editor graph.");
        return false;
    }
    const auto SampleMatches = [](
        const UMaterialExpressionTextureSampleParameter2D* Sample,
        const FString& TexturePath,
        FName ParameterName,
        EMaterialSamplerType SamplerType)
    {
        return Sample && Sample->Texture &&
            Sample->Texture->GetPathName() == TexturePath &&
            Sample->ParameterName == ParameterName &&
            Sample->SamplerType == SamplerType &&
            Sample->SamplerSource == SSM_FromTextureAsset &&
            Sample->MipValueMode == TMVM_None &&
            Sample->AutomaticViewMipBias &&
            Sample->ConstCoordinate == 0 &&
            Sample->ConstMipValue == INDEX_NONE &&
            !Sample->Coordinates.Expression &&
            !Sample->TextureObject.Expression &&
            !Sample->MipValue.Expression &&
            !Sample->CoordinatesDX.Expression &&
            !Sample->CoordinatesDY.Expression &&
            !Sample->AutomaticViewMipBiasValue.Expression;
    };
    const FName WindInputNames[] = {
        TEXT("WorldPosition"), TEXT("InstanceLocalPosition"),
        TEXT("TimeSeconds"), TEXT("WindStrengthCm"), TEXT("WindSpeed"),
        TEXT("WindDirection"), TEXT("HeightCm"), TEXT("ResponseScale"),
        TEXT("MaxWpoCm")};
    const UMaterialExpression* WindInputNodes[] = {
        OutGraph.WorldPosition, OutGraph.InstanceLocalPosition,
        OutGraph.Time, OutGraph.Strength, OutGraph.Speed,
        OutGraph.Direction, OutGraph.Height, OutGraph.Response,
        OutGraph.MaximumWpo};
    bool bExactWindInputs = OutGraph.Wind &&
        OutGraph.Wind->Inputs.Num() == UE_ARRAY_COUNT(WindInputNames);
    for (int32 Index = 0;
         bExactWindInputs && Index < UE_ARRAY_COUNT(WindInputNames);
         ++Index)
    {
        bExactWindInputs =
            OutGraph.Wind->Inputs[Index].InputName == WindInputNames[Index] &&
            InputMatches(
                OutGraph.Wind->Inputs[Index].Input,
                WindInputNodes[Index],
                0);
    }

    int32 DitherAlphaInputIndex = INDEX_NONE;
    int32 DitherAlphaInputCount = 0;
    bool bNoUnexpectedDitherInputs = true;
    const bool bExactDitherFunction = OutGraph.Dither &&
        OutGraph.Dither->MaterialFunction &&
        OutGraph.Dither->MaterialFunction->GetPathName() ==
            GrassDitherTemporalAaFunctionPath &&
        OutGraph.Dither->FunctionOutputs.Num() > 0;
    if (bExactDitherFunction)
    {
        for (int32 InputIndex = 0;
             InputIndex < OutGraph.Dither->FunctionInputs.Num();
             ++InputIndex)
        {
            if (OutGraph.Dither->GetInputName(InputIndex)
                    .ToString().StartsWith(TEXT("Alpha Threshold")))
            {
                DitherAlphaInputIndex = InputIndex;
                ++DitherAlphaInputCount;
            }
            else
            {
                bNoUnexpectedDitherInputs &=
                    !OutGraph.Dither->FunctionInputs[InputIndex]
                         .Input.Expression;
            }
        }
    }
    const bool bExactDitherInput = bExactDitherFunction &&
        DitherAlphaInputCount == 1 && DitherAlphaInputIndex != INDEX_NONE &&
        bNoUnexpectedDitherInputs &&
        InputMatches(
            OutGraph.Dither->FunctionInputs[DitherAlphaInputIndex].Input,
            OutGraph.InstanceFade,
            0);
    const bool bExactStableVisibility =
        bRequireStableDerivative && OutGraph.StableVisibility &&
        OutGraph.StableVisibility->Code == ExpectedStableCode &&
        OutGraph.StableVisibility->OutputType == CMOT_Float1 &&
        OutGraph.StableVisibility->Inputs.Num() ==
            (bRequireR23BCalibratedDerivative ? 6 : 5) &&
        OutGraph.StableVisibility->Inputs[0].InputName ==
            TEXT("InstanceFade") &&
        InputMatches(
            OutGraph.StableVisibility->Inputs[0].Input,
            OutGraph.InstanceFade,
            0) &&
        OutGraph.StableVisibility->Inputs[1].InputName == TEXT("Random01") &&
        InputMatches(
            OutGraph.StableVisibility->Inputs[1].Input,
            OutGraph.InstanceRandom,
            0) &&
        OutGraph.StableVisibility->Inputs[2].InputName ==
            TEXT("WorldPosition") &&
        InputMatches(
            OutGraph.StableVisibility->Inputs[2].Input,
            OutGraph.WorldPosition,
            0) &&
        OutGraph.StableVisibility->Inputs[3].InputName ==
            TEXT("CameraPosition") &&
        OutGraph.CameraPosition &&
        OutGraph.CameraPosition->Desc == EdgeGrassR22CameraDescription &&
        InputMatches(
            OutGraph.StableVisibility->Inputs[3].Input,
            OutGraph.CameraPosition,
            0) &&
        OutGraph.StableVisibility->Inputs[4].InputName ==
            TEXT("MaterialRevision") &&
        InputMatches(
            OutGraph.StableVisibility->Inputs[4].Input,
            OutGraph.RevisionMarker,
            0) &&
        OutGraph.RevisionMarker &&
        OutGraph.RevisionMarker->Desc == ExpectedRevisionDescription &&
        FMath::IsNearlyEqual(
            OutGraph.RevisionMarker->DefaultValue,
            ExpectedRevisionValue,
            0.0001f) &&
        (!bRequireR23BCalibratedDerivative ||
            (OutGraph.StableVisibility->Inputs[5].InputName ==
                 TEXT("CalibrationRevision") &&
             InputMatches(
                 OutGraph.StableVisibility->Inputs[5].Input,
                 OutGraph.CalibrationRevisionMarker,
                 0) &&
             OutGraph.CalibrationRevisionMarker &&
             OutGraph.CalibrationRevisionMarker->Desc ==
                 R23BRuntimeCalibrationRevisionDescription &&
             FMath::IsNearlyEqual(
                 OutGraph.CalibrationRevisionMarker->DefaultValue,
                 R23BRuntimeCalibrationRevisionValue,
                 0.0001f))) &&
        HasNoAuxiliaryCustomState(OutGraph.StableVisibility);
    const bool bExactFadeGraph = bRequireFadeDerivative
        ? OutGraph.InstanceFadeNodes == 1 && OutGraph.InstanceFade &&
            OutGraph.DitherNodes == 1 && bExactDitherInput &&
            OutGraph.FadeMultiply && OutGraph.MultiplyNodes == 2 &&
            InputMatches(
                OutGraph.FadeMultiply->A,
                OutGraph.Opacity,
                1) &&
            (bRequireStableDerivative
                ? OutGraph.InstanceRandomNodes == 1 &&
                    OutGraph.InstanceRandom && OutGraph.CameraPosition &&
                    bExactStableVisibility &&
                    InputMatches(
                        OutGraph.FadeMultiply->B,
                        OutGraph.StableVisibility,
                        0)
                : OutGraph.InstanceRandomNodes == 0 &&
                    !OutGraph.InstanceRandom && !OutGraph.CameraPosition &&
                    !OutGraph.StableVisibility &&
                    InputMatches(
                        OutGraph.FadeMultiply->B,
                        OutGraph.Dither,
                        0)) &&
            InputMatches(
                OutGraph.Data->OpacityMask,
                OutGraph.FadeMultiply,
                0)
        : OutGraph.InstanceFadeNodes == 0 && !OutGraph.InstanceFade &&
            OutGraph.InstanceRandomNodes == 0 && !OutGraph.InstanceRandom &&
            !OutGraph.CameraPosition && !OutGraph.StableVisibility &&
            OutGraph.DitherNodes == 0 && !OutGraph.Dither &&
            !OutGraph.FadeMultiply && OutGraph.MultiplyNodes == 1 &&
            InputMatches(OutGraph.Data->OpacityMask, OutGraph.Opacity, 1);

    if (!Material || Material->GetPathName() != ExpectedObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || !Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            MSM_TwoSidedFoliage) ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        !FMath::IsNearlyEqual(
            Material->OpacityMaskClipValue, 0.333f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            5.0f,
            0.000001f) ||
        OutGraph.Data->ExpressionCollection.Expressions.Num() !=
            (bRequireStableDerivative
                ? (bRequireR23BCalibratedDerivative ? 26 : 25)
                : (bRequireFadeDerivative ? 21 : 18)) ||
        OutGraph.TextureSamples != 4 ||
        !OutGraph.bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(OutGraph.Data) ||
        !SampleMatches(
            OutGraph.Base,
            EdgeGrassBaseColorTexturePath,
            TEXT("BaseColorTexture"),
            SAMPLERTYPE_Color) ||
        !SampleMatches(
            OutGraph.Normal,
            EdgeGrassNormalTexturePath,
            TEXT("NormalTexture"),
            SAMPLERTYPE_Normal) ||
        !SampleMatches(
            OutGraph.Roughness,
            EdgeGrassRoughnessTexturePath,
            TEXT("RoughnessTexture"),
            SAMPLERTYPE_Masks) ||
        !SampleMatches(
            OutGraph.Opacity,
            EdgeGrassOpacityTexturePath,
            TEXT("OpacityTexture"),
            SAMPLERTYPE_Masks) ||
        !OutGraph.WorldPosition ||
        OutGraph.WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        !OutGraph.InstanceLocalPosition ||
        OutGraph.InstanceLocalPosition->TransformSourceType !=
            TRANSFORMPOSSOURCE_World ||
        OutGraph.InstanceLocalPosition->TransformType !=
            TRANSFORMPOSSOURCE_Instance ||
        !InputMatches(
            OutGraph.InstanceLocalPosition->Input,
            OutGraph.WorldPosition,
            0) ||
        !OutGraph.Time || OutGraph.Time->bOverride_Period ||
        OutGraph.Time->bIgnorePause ||
        !FMath::IsNearlyEqual(OutGraph.Time->Period, 0.0f, 0.000001f) ||
        !OutGraph.Strength || !OutGraph.Speed ||
        !OutGraph.Height || !OutGraph.Response || !OutGraph.MaximumWpo ||
        !OutGraph.Direction || !OutGraph.Wind || !bExactWindInputs ||
        OutGraph.Wind->OutputType != CMOT_Float3 ||
        !HasNoAuxiliaryCustomState(OutGraph.Wind) ||
        !FMath::IsNearlyEqual(
            OutGraph.Strength->DefaultValue,
            bRequireStableDerivative ? 0.0f : 5.5f,
            0.000001f) ||
        !FMath::IsNearlyEqual(
            OutGraph.Speed->DefaultValue, 1.45f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            OutGraph.Height->DefaultValue,
            EdgeGrassSourceHeightCm,
            0.001f) ||
        !FMath::IsNearlyEqual(
            OutGraph.Response->DefaultValue, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            OutGraph.MaximumWpo->DefaultValue, 5.0f, 0.000001f) ||
        !OutGraph.Direction->DefaultValue.Equals(
            FLinearColor(0.93f, 0.37f, 0.0f, 0.0f),
            0.000001f) ||
        !OutGraph.CorrectedNormal || !OutGraph.Facing ||
        !OutGraph.TangentXY || !OutGraph.FacingSign ||
        !FMath::IsNearlyEqual(OutGraph.TangentXY->R, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(OutGraph.TangentXY->G, 1.0f, 0.000001f) ||
        !InputMatches(OutGraph.CorrectedNormal->A, OutGraph.Normal, 0) ||
        !InputMatches(OutGraph.CorrectedNormal->B, OutGraph.Facing, 0) ||
        !InputMatches(OutGraph.Facing->A, OutGraph.TangentXY, 0) ||
        !InputMatches(OutGraph.Facing->B, OutGraph.FacingSign, 0) ||
        !InputMatches(OutGraph.Data->BaseColor, OutGraph.Base, 0) ||
        !InputMatches(OutGraph.Data->Normal, OutGraph.CorrectedNormal, 0) ||
        !InputMatches(OutGraph.Data->Roughness, OutGraph.Roughness, 1) ||
        !InputMatches(OutGraph.Data->SubsurfaceColor, OutGraph.Base, 0) ||
        !InputMatches(OutGraph.Data->WorldPositionOffset, OutGraph.Wind, 0) ||
        !bExactFadeGraph || OutGraph.Data->Metallic.Expression ||
        OutGraph.Data->Specular.Expression ||
        OutGraph.Data->Specular.UseConstant ||
        !FMath::IsNearlyEqual(
            OutGraph.Data->Specular.Constant, 0.5f, 0.000001f) ||
        OutGraph.Data->Anisotropy.Expression ||
        OutGraph.Data->Tangent.Expression ||
        OutGraph.Data->EmissiveColor.Expression ||
        OutGraph.Data->Opacity.Expression ||
        OutGraph.Data->AmbientOcclusion.Expression ||
        OutGraph.Data->Displacement.Expression ||
        OutGraph.Data->ClearCoat.Expression ||
        OutGraph.Data->ClearCoatRoughness.Expression ||
        OutGraph.Data->Refraction.Expression ||
        OutGraph.Data->MaterialAttributes.Expression ||
        OutGraph.Data->PixelDepthOffset.Expression ||
        OutGraph.Data->ShadingModelFromMaterialExpression.Expression ||
        OutGraph.Data->SurfaceThickness.Expression ||
        OutGraph.Data->FrontMaterial.Expression)
    {
        OutError = bRequireStableDerivative
            ? (bRequireR23BCalibratedDerivative
                ? TEXT("The R23B edge material lost its exact 26-node stable spatial fade, base/runtime calibration revision markers, zero-default-wind, preserved red-mask or dormant predecessor dither graph.")
                : (bRequireR23StableDerivative
                    ? TEXT("The R23 edge material lost its exact 25-node stable spatial fade, runtime revision marker, zero-default-wind, preserved red-mask or dormant predecessor dither graph.")
                    : TEXT("The R22 edge material lost its exact 25-node stable spatial fade, runtime revision marker, zero-default-wind, preserved red-mask or dormant predecessor dither graph.")))
            : bRequireFadeDerivative
            ? TEXT("The R11 edge material lost its exact 21-node V4 clone plus PerInstanceFade/DitherTemporalAA/red-mask multiply graph.")
            : TEXT("The exact V4 edge source lost its 18-node four-texture/two-sided-normal/nine-input 5 cm WPO graph.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateEdgeGrassSourceMaterial(
    UMaterial* Material,
    FString& OutError)
{
    FEdgeGrassMaterialGraph Graph;
    return ValidateEdgeGrassMaterialGraph(
               Material,
               SourceEdgeGrassMaterialPath,
               false,
               Graph,
               OutError) &&
        ValidateCompiledMaterial(Material, OutError);
}

bool ValidateEdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    FEdgeGrassMaterialGraph SourceGraph;
    FEdgeGrassMaterialGraph FadeGraph;
    if (!ValidateEdgeGrassMaterialGraph(
            Source,
            SourceEdgeGrassMaterialPath,
            false,
            SourceGraph,
            OutError) ||
        !ValidateEdgeGrassMaterialGraph(
            Material,
            ObjectPath(EdgeGrassFadeAssetName),
            true,
            FadeGraph,
            OutError) ||
        !FMath::IsNearlyEqual(
            SourceGraph.Height->DefaultValue,
            FadeGraph.Height->DefaultValue,
            0.000001f))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R11 derivative no longer preserves the exact source V4 mesh-height wind calibration.");
        }
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR22EdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    FEdgeGrassMaterialGraph SourceGraph;
    FEdgeGrassMaterialGraph R22Graph;
    if (!ValidateEdgeGrassMaterialGraph(
            Source,
            SourceEdgeGrassMaterialPath,
            false,
            SourceGraph,
            OutError) ||
        !ValidateEdgeGrassMaterialGraph(
            Material,
            ObjectPath(EdgeGrassFadeAssetName),
            true,
            R22Graph,
            OutError,
            true) ||
        !FMath::IsNearlyEqual(
            SourceGraph.Height->DefaultValue,
            R22Graph.Height->DefaultValue,
            0.000001f))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R22 edge derivative no longer preserves the exact source V4 mesh-height calibration.");
        }
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR23EdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    FEdgeGrassMaterialGraph SourceGraph;
    FEdgeGrassMaterialGraph R23Graph;
    if (!ValidateEdgeGrassMaterialGraph(
            Source,
            SourceEdgeGrassMaterialPath,
            false,
            SourceGraph,
            OutError) ||
        !ValidateEdgeGrassMaterialGraph(
            Material,
            ObjectPath(EdgeGrassFadeAssetName),
            true,
            R23Graph,
            OutError,
            false,
            true) ||
        !FMath::IsNearlyEqual(
            SourceGraph.Height->DefaultValue,
            R23Graph.Height->DefaultValue,
            0.000001f))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R23 edge derivative no longer preserves the exact source V4 mesh-height calibration.");
        }
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR23BEdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    FEdgeGrassMaterialGraph SourceGraph;
    FEdgeGrassMaterialGraph R23BGraph;
    if (!ValidateEdgeGrassMaterialGraph(
            Source,
            SourceEdgeGrassMaterialPath,
            false,
            SourceGraph,
            OutError) ||
        !ValidateEdgeGrassMaterialGraph(
            Material,
            ObjectPath(EdgeGrassFadeAssetName),
            true,
            R23BGraph,
            OutError,
            false,
            false,
            true) ||
        !FMath::IsNearlyEqual(
            SourceGraph.Height->DefaultValue,
            R23BGraph.Height->DefaultValue,
            0.000001f))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R23B edge derivative no longer preserves the exact source V4 mesh-height calibration.");
        }
        return false;
    }
    const bool bR23BCompiled = ValidateCompiledMaterial(Material, OutError);
    return bR23BCompiled;
}

bool ConfigureEdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    FEdgeGrassMaterialGraph SourceGraph;
    if (!ValidateEdgeGrassMaterialGraph(
            Material,
            ObjectPath(EdgeGrassFadeAssetName),
            false,
            SourceGraph,
            OutError))
    {
        return false;
    }
    UMaterialFunctionInterface* DitherFunction =
        LoadExact<UMaterialFunctionInterface>(
            GrassDitherTemporalAaFunctionPath);
    Material->Modify();
    Material->PreEditChange(nullptr);
    UMaterialExpression* InstanceFade = AddExpressionByClassPath(
        Material,
        TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"),
        EdgeGrassFadeNodeDescription,
        -480,
        420);
    UMaterialExpressionMaterialFunctionCall* Dither =
        AddExpression<UMaterialExpressionMaterialFunctionCall>(
            Material,
            EdgeGrassFadeDitherDescription,
            -240,
            420);
    UMaterialExpressionMultiply* FadeMultiply =
        AddExpression<UMaterialExpressionMultiply>(
            Material,
            EdgeGrassFadeMultiplyDescription,
            20,
            380);
    int32 AlphaInputIndex = INDEX_NONE;
    int32 AlphaInputCount = 0;
    if (Dither && DitherFunction &&
        Dither->SetMaterialFunction(DitherFunction))
    {
        for (int32 InputIndex = 0;
             InputIndex < Dither->FunctionInputs.Num();
             ++InputIndex)
        {
            if (Dither->GetInputName(InputIndex)
                    .ToString().StartsWith(TEXT("Alpha Threshold")))
            {
                AlphaInputIndex = InputIndex;
                ++AlphaInputCount;
            }
        }
    }
    if (!InstanceFade || !Dither || !FadeMultiply ||
        AlphaInputCount != 1 || AlphaInputIndex == INDEX_NONE)
    {
        OutError = TEXT("Could not allocate the exact PerInstanceFade/DitherTemporalAA/red-mask multiply nodes for R11.");
        return false;
    }
    Dither->FunctionInputs[AlphaInputIndex].Input.Connect(0, InstanceFade);
    FadeMultiply->A.Connect(1, SourceGraph.Opacity);
    FadeMultiply->B.Connect(0, Dither);
    SourceGraph.Data->OpacityMask.Connect(0, FadeMultiply);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateEdgeGrassFadeMaterial(Material, OutError);
}

bool ConfigureR22EdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateEdgeGrassFadeMaterial(Material, OutError))
    {
        return false;
    }
    FEdgeGrassMaterialGraph Graph;
    if (!CollectEdgeGrassMaterialGraph(Material, Graph) || !Graph.Data ||
        !Graph.FadeMultiply || !Graph.Strength || !Graph.InstanceFade ||
        !Graph.WorldPosition)
    {
        OutError = TEXT("The exact R11 edge predecessor does not expose the nodes required by the R22 stable presentation migration.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    Graph.FadeMultiply->Modify();
    Graph.Strength->Modify();
    UMaterialExpressionCameraPositionWS* Camera =
        AddExpression<UMaterialExpressionCameraPositionWS>(
            Material, EdgeGrassR22CameraDescription, -700, 590);
    UMaterialExpression* Random = AddExpressionByClassPath(
        Material,
        TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"),
        EdgeGrassR22RandomDescription,
        -700,
        700);
    UMaterialExpressionCustom* Stable =
        AddExpression<UMaterialExpressionCustom>(
            Material,
            EdgeGrassR22StableVisibilityDescription,
            -430,
            610);
    UMaterialExpressionScalarParameter* RevisionMarker =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, R22RuntimeRevisionDescription, -700, 810);
    if (!Camera || !Random || !Stable || !RevisionMarker)
    {
        OutError = TEXT("Could not allocate the exact R22 edge stable-visibility and runtime-revision nodes.");
        return false;
    }
    RevisionMarker->ParameterName = R22RuntimeRevisionParameterName;
    RevisionMarker->DefaultValue = R22RuntimeRevisionValue;
    Stable->Description = EdgeGrassR22StableVisibilityDescription;
    Stable->Code = GrassR22StableVisibilityCode;
    Stable->OutputType = CMOT_Float1;
    Stable->Inputs.SetNum(5);
    Stable->Inputs[0].InputName = TEXT("InstanceFade");
    Stable->Inputs[0].Input.Connect(0, Graph.InstanceFade);
    Stable->Inputs[1].InputName = TEXT("Random01");
    Stable->Inputs[1].Input.Connect(0, Random);
    Stable->Inputs[2].InputName = TEXT("WorldPosition");
    Stable->Inputs[2].Input.Connect(0, Graph.WorldPosition);
    Stable->Inputs[3].InputName = TEXT("CameraPosition");
    Stable->Inputs[3].Input.Connect(0, Camera);
    Stable->Inputs[4].InputName = TEXT("MaterialRevision");
    Stable->Inputs[4].Input.Connect(0, RevisionMarker);
    ResetCustomAuxiliaryState(Stable);
    Graph.Strength->DefaultValue = 0.0f;
    Graph.FadeMultiply->B.Connect(0, Stable);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR22EdgeGrassFadeMaterial(Material, OutError);
}

bool ConfigureR23EdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR22EdgeGrassFadeMaterial(Material, OutError))
    {
        return false;
    }
    FEdgeGrassMaterialGraph Graph;
    if (!CollectEdgeGrassMaterialGraph(Material, Graph) ||
        !Graph.StableVisibility || !Graph.RevisionMarker)
    {
        OutError = TEXT("The exact R22 edge predecessor does not expose its stable visibility and revision nodes required by R23.");
        return false;
    }
    Material->Modify();
    Material->PreEditChange(nullptr);
    Graph.StableVisibility->Modify();
    Graph.RevisionMarker->Modify();
    Graph.StableVisibility->Description =
        EdgeGrassR23StableVisibilityDescription;
    Graph.StableVisibility->Code = GrassR23StableVisibilityCode;
    ResetCustomAuxiliaryState(Graph.StableVisibility);
    Graph.RevisionMarker->Desc = R23RuntimeRevisionDescription;
    Graph.RevisionMarker->DefaultValue = R23RuntimeRevisionValue;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR23EdgeGrassFadeMaterial(Material, OutError);
}

bool ConfigureR23BEdgeGrassFadeMaterial(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR23EdgeGrassFadeMaterial(Material, OutError))
    {
        return false;
    }
    FEdgeGrassMaterialGraph Graph;
    if (!CollectEdgeGrassMaterialGraph(Material, Graph) ||
        !Graph.StableVisibility || !Graph.RevisionMarker ||
        !Graph.InstanceFade || !Graph.InstanceRandom ||
        !Graph.WorldPosition || !Graph.CameraPosition ||
        Graph.CalibrationRevisionMarker)
    {
        OutError = TEXT("The exact R23 edge predecessor does not expose one uncalibrated stable-visibility graph required by R23B.");
        return false;
    }
    Material->Modify();
    Material->PreEditChange(nullptr);
    Graph.StableVisibility->Modify();
    UMaterialExpressionScalarParameter* CalibrationRevisionMarker =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material,
            R23BRuntimeCalibrationRevisionDescription,
            -700,
            900);
    if (!CalibrationRevisionMarker)
    {
        OutError = TEXT("Could not allocate the exact connected R23B edge calibration-revision node.");
        return false;
    }
    CalibrationRevisionMarker->ParameterName =
        R23BRuntimeCalibrationRevisionParameterName;
    CalibrationRevisionMarker->DefaultValue =
        R23BRuntimeCalibrationRevisionValue;
    Graph.StableVisibility->Description =
        EdgeGrassR23BStableVisibilityDescription;
    Graph.StableVisibility->Code = GrassR23BStableVisibilityCode;
    Graph.StableVisibility->OutputType = CMOT_Float1;
    Graph.StableVisibility->Inputs.SetNum(6);
    Graph.StableVisibility->Inputs[0].InputName = TEXT("InstanceFade");
    Graph.StableVisibility->Inputs[0].Input.Connect(0, Graph.InstanceFade);
    Graph.StableVisibility->Inputs[1].InputName = TEXT("Random01");
    Graph.StableVisibility->Inputs[1].Input.Connect(0, Graph.InstanceRandom);
    Graph.StableVisibility->Inputs[2].InputName = TEXT("WorldPosition");
    Graph.StableVisibility->Inputs[2].Input.Connect(0, Graph.WorldPosition);
    Graph.StableVisibility->Inputs[3].InputName = TEXT("CameraPosition");
    Graph.StableVisibility->Inputs[3].Input.Connect(0, Graph.CameraPosition);
    Graph.StableVisibility->Inputs[4].InputName = TEXT("MaterialRevision");
    Graph.StableVisibility->Inputs[4].Input.Connect(0, Graph.RevisionMarker);
    Graph.StableVisibility->Inputs[5].InputName = TEXT("CalibrationRevision");
    Graph.StableVisibility->Inputs[5].Input.Connect(
        0,
        CalibrationRevisionMarker);
    ResetCustomAuxiliaryState(Graph.StableVisibility);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR23BEdgeGrassFadeMaterial(Material, OutError);
}

bool CreateEdgeGrassFadeMaterial(
    UMaterial* Source,
    UMaterial*& OutMaterial,
    FString& OutError)
{
    OutMaterial = nullptr;
    if (!ValidateEdgeGrassSourceMaterial(Source, OutError))
    {
        return false;
    }
    OutMaterial = DuplicateMaterialFresh(
        Source,
        EdgeGrassFadeAssetName,
        OutError);
    return OutMaterial &&
        ConfigureEdgeGrassFadeMaterial(OutMaterial, OutError);
}

bool ValidateR17MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR17GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    if (!ValidateSoilMaterial(Soil, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    UMaterial* GroundOverlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateR17GroundOverlayMaterial(GroundOverlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(GroundOverlay);
    OutError.Reset();
    return true;
}

bool ValidateR18MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR17GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR18GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateMaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR19GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR18GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR21MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR21GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR18GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR22MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR22GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR22GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR23MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR23GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR23GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR23BMaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR23BGrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR23BGroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR16MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR15GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay = LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR16GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR15MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR15GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR15GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR13MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateGrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateGroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateUniformR13R15OrR16MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutRevision,
    FString& OutError)
{
    TArray<UMaterial*> R13Materials;
    TArray<UMaterial*> R15Materials;
    TArray<UMaterial*> R16Materials;
    FString R13Error;
    FString R15Error;
    FString R16Error;
    const bool bR13 =
        ValidateR13MaterialAssetsInternal(R13Materials, R13Error);
    const bool bR15 =
        ValidateR15MaterialAssetsInternal(R15Materials, R15Error);
    const bool bR16 =
        ValidateR16MaterialAssetsInternal(R16Materials, R16Error);
    const int32 ValidRevisionCount =
        (bR13 ? 1 : 0) + (bR15 ? 1 : 0) + (bR16 ? 1 : 0);
    if (ValidRevisionCount != 1)
    {
        OutMaterials.Reset();
        OutRevision.Reset();
        OutError = TEXT("The six V5D core packages are mixed, invalid, or ambiguously revisioned; an exact uniform R13, R15 or R16 roster is required. R13=") +
            R13Error + TEXT(" R15=") + R15Error +
            TEXT(" R16=") + R16Error;
        return false;
    }
    if (bR16)
    {
        OutRevision = TEXT("R16");
        OutMaterials = MoveTemp(R16Materials);
    }
    else if (bR15)
    {
        OutRevision = TEXT("R15");
        OutMaterials = MoveTemp(R15Materials);
    }
    else
    {
        OutRevision = TEXT("R13");
        OutMaterials = MoveTemp(R13Materials);
    }
    OutError.Reset();
    return true;
}

bool ValidateR10MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR10GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    if (!ValidateSoilMaterial(Soil, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    UMaterial* GroundOverlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateR10GroundOverlayMaterial(GroundOverlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(GroundOverlay);
    OutError.Reset();
    return true;
}

bool ValidateR12MaterialAssetsInternal(
    TArray<UMaterial*>& OutMaterials,
    FString& OutError)
{
    OutMaterials.Reset();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!ValidateR12GrassMaterial(Material, Index, OutError))
        {
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    if (!ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR12GroundOverlayMaterial(Overlay, OutError))
    {
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    OutError.Reset();
    return true;
}

bool ValidateR10UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    bool& bOutAlreadyR10,
    FString& OutError)
{
    OutMaterials.Reset();
    bOutAlreadyR10 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R10 upgrade requires every exact V5D grass package loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    FString SoilError;
    if (!Soil || !Overlay || !Soil->GetOutermost() ||
        !Overlay->GetOutermost() || Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, SoilError))
    {
        OutError = TEXT("The R10 upgrade requires the exact valid clean soil and lawn-overlay packages. ") +
            SoilError;
        OutMaterials.Reset();
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    bool bAllLegacy = true;
    bool bAllR10 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bAllLegacy &= ValidateLegacyGrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bAllR10 &= ValidateR10GrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    FString Ignored;
    bAllLegacy &= ValidateLegacyGroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bAllR10 &= ValidateR10GroundOverlayMaterial(Overlay, Ignored);
    if (!bAllLegacy && !bAllR10)
    {
        OutError = TEXT("The six-package V5D material roster is mixed-version or invalid; atomic R10 upgrade refuses partial repair.");
        OutMaterials.Reset();
        return false;
    }
    bOutAlreadyR10 = bAllR10;
    OutError.Reset();
    return true;
}

bool ValidateR12UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR12,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR12 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R12 upgrade requires every exact V5D grass package loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    FString ValidationError;
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, ValidationError) ||
        !ValidateEdgeGrassFadeMaterial(
            OutEdgeFadeMaterial, ValidationError))
    {
        OutError = TEXT("The R12 upgrade requires the exact valid clean soil, lawn-overlay and R11 edge-fade packages. ") +
            ValidationError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    bool bAllR10 = true;
    bool bAllR12 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bAllR10 &= ValidateR10GrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bAllR12 &= ValidateR12GrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    FString Ignored;
    bAllR10 &= ValidateR10GroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bAllR12 &= ValidateR12GroundOverlayMaterial(Overlay, Ignored);
    if (!bAllR10 && !bAllR12)
    {
        OutError = TEXT("The seven-package V5D material roster is mixed-version or invalid; atomic R12 upgrade refuses partial repair.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR12 = bAllR12;
    OutError.Reset();
    return true;
}

bool ValidateR13UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR13,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR13 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R13 upgrade requires every exact V5D grass package loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    FString ValidationError;
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, ValidationError) ||
        !ValidateEdgeGrassFadeMaterial(
            OutEdgeFadeMaterial, ValidationError))
    {
        OutError = TEXT("The R13 upgrade requires the exact valid clean soil, lawn-overlay and R11 edge-fade packages. ") +
            ValidationError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    bool bAllR12 = true;
    bool bAllR13 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bAllR12 &= ValidateR12GrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bAllR13 &= ValidateGrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    FString Ignored;
    bAllR12 &= ValidateR12GroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bAllR13 &= ValidateGroundOverlayMaterial(Overlay, Ignored);
    if (!bAllR12 && !bAllR13)
    {
        OutError = TEXT("The exact five R13 targets are mixed-version or invalid; atomic R13 upgrade admits only uniform clean R12 or idempotent R13 state.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR13 = bAllR13;
    OutError.Reset();
    return true;
}

bool ValidateR15UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR15,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR15 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R15 upgrade requires every exact V5D grass package loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    FString ValidationError;
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, ValidationError) ||
        !ValidateEdgeGrassFadeMaterial(
            OutEdgeFadeMaterial, ValidationError))
    {
        OutError = TEXT("The R15 upgrade requires the exact valid clean soil, lawn-overlay and R11 edge-fade packages. ") +
            ValidationError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);
    bool bAllR13 = true;
    bool bAllR15 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bAllR13 &= ValidateGrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bAllR15 &= ValidateR15GrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    FString Ignored;
    bAllR13 &= ValidateGroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bAllR15 &= ValidateR15GroundOverlayMaterial(Overlay, Ignored);
    if (!bAllR13 && !bAllR15)
    {
        OutError = TEXT("The exact five R15 targets are mixed-version or invalid; atomic R15 upgrade admits only uniform clean R13 or idempotent R15 state.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR15 = bAllR15;
    OutError.Reset();
    return true;
}

bool ValidateR16UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR16,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR16 = false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty() ||
            !ValidateR15GrassMaterial(Material, Index, OutError))
        {
            OutError = TEXT("The R16 seam-cover upgrade requires every exact R15 grass package valid and clean: ") +
                ObjectPath(GrassAssetNames[Index]) + TEXT(" ") + OutError;
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError) ||
        !ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError))
    {
        OutError = TEXT("The R16 seam-cover upgrade requires the exact valid clean soil, lawn-overlay and R11 edge-fade packages. ") +
            OutError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    FString R15Error;
    FString R16Error;
    const bool bR15 = ValidateR15GroundOverlayMaterial(Overlay, R15Error);
    const bool bR16 = ValidateR16GroundOverlayMaterial(Overlay, R16Error);
    if (bR15 == bR16)
    {
        OutError = TEXT("The sole R16 target is invalid or ambiguously revisioned; exactly one clean R15 or idempotent R16 lawn-overlay state is required. R15=") +
            R15Error + TEXT(" R16=") + R16Error;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR16 = bR16;
    OutError.Reset();
    return true;
}

bool ValidateR17UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR17,
    FString& OutError)
{
    OutMaterials.Reset(); OutEdgeFadeMaterial = nullptr; bOutAlreadyR17 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() || Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R17 upgrade requires every exact V5D grass package loaded and clean: ") + ObjectPath(Name);
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay = LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial = LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial || !Soil->GetOutermost() ||
        !Overlay->GetOutermost() || !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() || Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError) ||
        !ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError))
    {
        OutError = TEXT("The R17 upgrade requires clean immutable soil, R11 edge fade, and overlay packages. ") + OutError;
        OutMaterials.Reset(); OutEdgeFadeMaterial = nullptr; return false;
    }
    OutMaterials.Add(Soil); OutMaterials.Add(Overlay);
    bool bAllR16 = ValidateR16GroundOverlayMaterial(Overlay, OutError);
    bool bAllR17 = ValidateR17GroundOverlayMaterial(Overlay, OutError);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bAllR16 &= ValidateR15GrassMaterial(OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bAllR17 &= ValidateR17GrassMaterial(OutMaterials[Index], Index, Ignored);
    }
    if (bAllR16 == bAllR17)
    {
        OutError = TEXT("The exact five R17 targets are mixed-version or invalid; atomic R17 upgrade admits only uniform clean R16 or idempotent R17 state.");
        OutMaterials.Reset(); OutEdgeFadeMaterial = nullptr; return false;
    }
    bOutAlreadyR17 = bAllR17;
    OutError.Reset(); return true;
}

bool ValidateR18UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR18,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR18 = false;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material =
            LoadExact<UMaterial>(ObjectPath(GrassAssetNames[Index]));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty() ||
            !ValidateR17GrassMaterial(Material, Index, OutError))
        {
            OutError = TEXT("The R18 lawn-overlay calibration requires every exact R17 grass package valid and clean: ") +
                ObjectPath(GrassAssetNames[Index]) + TEXT(" ") + OutError;
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError) ||
        !ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError))
    {
        OutError = TEXT("The R18 lawn-overlay calibration requires the exact valid clean soil, overlay, and R11 edge-fade packages. ") +
            OutError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    FString R17Error;
    FString R18Error;
    const bool bR17 = ValidateR17GroundOverlayMaterial(Overlay, R17Error);
    const bool bR18 = ValidateR18GroundOverlayMaterial(Overlay, R18Error);
    if (bR17 == bR18)
    {
        OutError = TEXT("The sole R18 target is invalid or ambiguously revisioned; exactly one clean R17 input or idempotent R18 lawn-overlay state is required. R17=") +
            R17Error + TEXT(" R18=") + R18Error;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR18 = bR18;
    OutError.Reset();
    return true;
}

bool ValidateR19UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR19,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR19 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R19 grass upgrade requires every exact V5D grass package loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }

    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR18GroundOverlayMaterial(Overlay, OutError) ||
        !ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError))
    {
        OutError = TEXT("The R19 grass upgrade requires the exact clean soil, R18 lawn overlay and R11 edge-fade packages. ") +
            OutError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    bool bAllR17 = true;
    bool bAllR19 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bAllR17 &= ValidateR17GrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bAllR19 &= ValidateR19GrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    if (bAllR17 == bAllR19)
    {
        OutError = TEXT("The exact four R19 grass targets are mixed-version or invalid; atomic R19 upgrade admits only uniform clean R17 grass with the R18 overlay, or idempotent R19 grass state.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR19 = bAllR19;
    OutError.Reset();
    return true;
}

bool ValidateR21UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR21,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR21 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R21 grass calibration requires every exact V5D grass package loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }

    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError) ||
        !ValidateR18GroundOverlayMaterial(Overlay, OutError) ||
        !ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError))
    {
        OutError = TEXT("The R21 grass calibration requires exact clean soil, R18 lawn-overlay and R11 edge-fade packages. ") +
            OutError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    bool bAllR19 = true;
    bool bAllR21 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bAllR19 &= ValidateR19GrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bAllR21 &= ValidateR21GrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    if (bAllR19 == bAllR21)
    {
        OutError = TEXT("The exact four R21 grass targets are mixed-version or invalid; the atomic R21 transaction admits only uniform clean R19 predecessor grass or idempotent uniform R21 state.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR21 = bAllR21;
    OutError.Reset();
    return true;
}

bool ValidateR22UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR22,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR22 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R22 grass-system upgrade requires every exact grass target loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError))
    {
        OutError = TEXT("The R22 grass-system upgrade requires exact clean soil, lawn-overlay, and edge-fade packages. ") +
            OutError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    bool bPredecessor = true;
    bool bR22 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bPredecessor &= ValidateR21GrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bR22 &= ValidateR22GrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    FString Ignored;
    bPredecessor &= ValidateR18GroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bPredecessor &= ValidateEdgeGrassFadeMaterial(
        OutEdgeFadeMaterial, Ignored);
    Ignored.Reset();
    bR22 &= ValidateR22GroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bR22 &= ValidateR22EdgeGrassFadeMaterial(
        OutEdgeFadeMaterial, Ignored);
    if (bPredecessor == bR22)
    {
        OutError = TEXT("The six R22 targets are mixed-version or invalid; the atomic R22 transaction admits only the exact uniform R21/R18/R11 predecessor system or an idempotent uniform R22 system.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR22 = bR22;
    OutError.Reset();
    return true;
}

bool ValidateR23UpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR23,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR23 = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R23 grass-system upgrade requires every exact grass target loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError))
    {
        OutError = TEXT("The R23 grass-system upgrade requires exact clean soil, lawn-overlay, and edge-fade packages. ") +
            OutError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    bool bR22 = true;
    bool bR23 = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bR22 &= ValidateR22GrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bR23 &= ValidateR23GrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    FString Ignored;
    bR22 &= ValidateR22GroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bR22 &= ValidateR22EdgeGrassFadeMaterial(
        OutEdgeFadeMaterial, Ignored);
    Ignored.Reset();
    bR23 &= ValidateR23GroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bR23 &= ValidateR23EdgeGrassFadeMaterial(
        OutEdgeFadeMaterial, Ignored);
    if (bR22 == bR23)
    {
        OutError = TEXT("The six R23 targets are mixed-version or invalid; the atomic R23 transaction admits only the exact uniform R22 predecessor system or an idempotent uniform R23 system.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR23 = bR23;
    OutError.Reset();
    return true;
}

bool ValidateR23BUpgradeInputMaterialAssets(
    TArray<UMaterial*>& OutMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    bool& bOutAlreadyR23B,
    FString& OutError)
{
    OutMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    bOutAlreadyR23B = false;
    for (const FString& Name : GrassAssetNames)
    {
        UMaterial* Material = LoadExact<UMaterial>(ObjectPath(Name));
        if (!Material || !Material->GetOutermost() ||
            Material->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R23B grass-system upgrade requires every exact grass target loaded and clean: ") +
                ObjectPath(Name);
            OutMaterials.Reset();
            return false;
        }
        OutMaterials.Add(Material);
    }
    UMaterial* Soil = LoadExact<UMaterial>(ObjectPath(SoilAssetName));
    UMaterial* Overlay =
        LoadExact<UMaterial>(ObjectPath(GroundOverlayAssetName));
    OutEdgeFadeMaterial =
        LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!Soil || !Overlay || !OutEdgeFadeMaterial ||
        !Soil->GetOutermost() || !Overlay->GetOutermost() ||
        !OutEdgeFadeMaterial->GetOutermost() ||
        Soil->GetOutermost()->IsDirty() ||
        Overlay->GetOutermost()->IsDirty() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty() ||
        !ValidateSoilMaterial(Soil, OutError))
    {
        OutError = TEXT("The R23B grass-system upgrade requires exact clean soil, lawn-overlay, and edge-fade packages. ") +
            OutError;
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutMaterials.Add(Soil);
    OutMaterials.Add(Overlay);

    bool bR23 = true;
    bool bR23B = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString Ignored;
        bR23 &= ValidateR23GrassMaterial(
            OutMaterials[Index], Index, Ignored);
        Ignored.Reset();
        bR23B &= ValidateR23BGrassMaterial(
            OutMaterials[Index], Index, Ignored);
    }
    FString Ignored;
    bR23 &= ValidateR23GroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bR23 &= ValidateR23EdgeGrassFadeMaterial(
        OutEdgeFadeMaterial, Ignored);
    Ignored.Reset();
    bR23B &= ValidateR23BGroundOverlayMaterial(Overlay, Ignored);
    Ignored.Reset();
    bR23B &= ValidateR23BEdgeGrassFadeMaterial(
        OutEdgeFadeMaterial, Ignored);
    if (bR23 == bR23B)
    {
        OutError = TEXT("The six R23B targets are mixed-version or invalid; the atomic R23B transaction admits only the exact uniform R23 predecessor system or an idempotent uniform R23B system.");
        OutMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    bOutAlreadyR23B = bR23B;
    OutError.Reset();
    return true;
}

UMaterial* DuplicateMaterialFresh(
    UMaterial* Source,
    const FString& AssetName,
    FString& OutError)
{
    const FString NewPackageName = PackagePath(AssetName);
    if (!Source || FPackageName::DoesPackageExist(NewPackageName) ||
        FindPackage(nullptr, *NewPackageName))
    {
        OutError = TEXT("A source material is absent or the isolated V5D destination package is no longer fresh: ") +
            NewPackageName;
        return nullptr;
    }
    UPackage* Package = CreatePackage(*NewPackageName);
    UMaterial* Duplicate = Package
        ? Cast<UMaterial>(StaticDuplicateObject(
              Source,
              Package,
              FName(*AssetName),
              RF_Public | RF_Standalone | RF_Transactional))
        : nullptr;
    if (!Duplicate || Duplicate->GetPathName() != ObjectPath(AssetName))
    {
        OutError = TEXT("Could not duplicate a source material into the exact V5D namespace: ") +
            AssetName;
        return nullptr;
    }
    Duplicate->Modify();
    FAssetRegistryModule::AssetCreated(Duplicate);
    return Duplicate;
}

bool ConfigureGrassMaterialVersion(
    UMaterial* Material,
    int32 ProfileIndex,
    const FGrassMaterialProfile& Profile,
    const FString& VersionSuffix,
    const FString& ColorCode,
    bool bR12,
    FString& OutError)
{
    UMaterialExpressionCustom* Color = FindCustomByPrefix(
        Material,
        GrassDescriptionPrefix);
    if (!Color)
    {
        Color = FindCustomByPrefix(
            Material,
            TEXT("TRIAD_EXPLORE_V5B_MODELED_BLADE_COLOR_"));
    }
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material,
        TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionCustom* Wind =
        FindCustomByDescription(Material, GrassWindDescription);
    UMaterialExpressionScalarParameter* Roughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* Specular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionScalarParameter* WindResponse =
        FindScalar(Material, TEXT("TRIAD_WindResponseScale"));
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material,
            TEXT("V5B_BERMUDA_LIT_SUBSURFACE"));
    if (!Material || !Color || !Wind || !WorldPosition || !Roughness || !Specular ||
        !WindResponse || !Subsurface ||
        (Color->Inputs.Num() != 2 && Color->Inputs.Num() != 3))
    {
        OutError = TEXT("The admitted V5B grass graph no longer exposes the exact blade colour/world-position/roughness/wind nodes needed by V5D.");
        return false;
    }
    Color->Modify();
    Wind->Modify();
    Roughness->Modify();
    Specular->Modify();
    WindResponse->Modify();
    Subsurface->Modify();
    Color->Description = GrassDescriptionPrefix +
        Profile.Id + VersionSuffix;
    Color->Code = ColorCode;
    ResetCustomAuxiliaryState(Color);
    ResetCustomAuxiliaryState(Wind);
    Color->Inputs.SetNum(3);
    Color->Inputs[2].InputName = TEXT("WorldPosition");
    Color->Inputs[2].Input.Connect(0, WorldPosition);
    Roughness->DefaultValue = Profile.Roughness;
    Specular->DefaultValue = Profile.Specular;
    WindResponse->DefaultValue = Profile.WindResponse;
    Subsurface->Constant = Profile.Subsurface;
    Material->PreEditChange(nullptr);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return bR12
        ? ValidateR12GrassMaterial(Material, ProfileIndex, OutError)
        : ValidateR10GrassMaterial(Material, ProfileIndex, OutError);
}

bool ConfigureR12GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    return ConfigureGrassMaterialVersion(
        Material,
        ProfileIndex,
        GrassProfiles[ProfileIndex],
        TEXT("_V3"),
        BuildR12GrassColorCode(GrassProfiles[ProfileIndex]),
        true,
        OutError);
}

bool ConfigureGrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ConfigureR12GrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionCustom* Color = FindCustomByPrefix(
        Material, GrassDescriptionPrefix);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionConstant3Vector* Attenuation =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, TEXT("V5B_BERMUDA_LIT_SUBSURFACE"));
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"));
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (Expression && Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
            {
                InstanceRandom = Expression;
            }
            if (Expression && Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
            {
                InstanceFade = Expression;
            }
        }
    }
    if (!Material || !Data || !Color || !BladeUv || !WorldPosition ||
        !BaseRoughness || !BaseSpecular || !Attenuation ||
        !DistanceMatchedNormal || !InstanceRandom || !InstanceFade ||
        Data->ExpressionCollection.Expressions.Num() != 23)
    {
        OutError = TEXT("The admitted R12 grass graph no longer exposes the exact nodes required for the bounded R13 response upgrade.");
        return false;
    }
    Material->Modify();
    Material->PreEditChange(nullptr);
    UMaterialExpressionCustom* Roughness =
        AddExpression<UMaterialExpressionCustom>(
            Material, GrassR13RoughnessDescription, 300, -220);
    UMaterialExpressionCustom* Specular =
        AddExpression<UMaterialExpressionCustom>(
            Material, GrassR13SpecularDescription, 300, -80);
    UMaterialExpressionCameraPositionWS* CameraPosition =
        AddExpression<UMaterialExpressionCameraPositionWS>(
            Material, GrassR13CameraDescription, -300, 440);
    UMaterialExpressionCustom* NormalAlpha =
        AddExpression<UMaterialExpressionCustom>(
            Material, GrassR13NormalAlphaDescription, 0, 400);
    UMaterialExpressionNormalize* FinalNormal =
        AddExpression<UMaterialExpressionNormalize>(
            Material, GrassR13NormalDescription, 520, 300);
    UMaterialExpressionMultiply* Subsurface =
        AddExpression<UMaterialExpressionMultiply>(
            Material, GrassR13SubsurfaceDescription, 520, 100);
    if (!Roughness || !Specular || !CameraPosition || !NormalAlpha ||
        !FinalNormal || !Subsurface)
    {
        OutError = TEXT("Could not allocate the exact six-node R13 grass response extension.");
        return false;
    }
    Color->Description = GrassDescriptionPrefix +
        GrassProfiles[ProfileIndex].Id + TEXT("_R13");
    Color->Code = BuildGrassColorCode(GrassProfiles[ProfileIndex]);
    ResetCustomAuxiliaryState(Color);
    Roughness->Description = GrassR13RoughnessDescription;
    Roughness->Code = GrassR13RoughnessCode;
    Roughness->OutputType = CMOT_Float1;
    ResetCustomAuxiliaryState(Roughness);
    Roughness->Inputs.SetNum(3);
    Roughness->Inputs[0].InputName = TEXT("BaseRoughness");
    Roughness->Inputs[0].Input.Connect(0, BaseRoughness);
    Roughness->Inputs[1].InputName = TEXT("BladeUV");
    Roughness->Inputs[1].Input.Connect(0, BladeUv);
    Roughness->Inputs[2].InputName = TEXT("Random01");
    Roughness->Inputs[2].Input.Connect(0, InstanceRandom);
    Specular->Description = GrassR13SpecularDescription;
    Specular->Code = GrassR13SpecularCode;
    Specular->OutputType = CMOT_Float1;
    ResetCustomAuxiliaryState(Specular);
    Specular->Inputs.SetNum(3);
    Specular->Inputs[0].InputName = TEXT("BaseSpecular");
    Specular->Inputs[0].Input.Connect(0, BaseSpecular);
    Specular->Inputs[1].InputName = TEXT("BladeUV");
    Specular->Inputs[1].Input.Connect(0, BladeUv);
    Specular->Inputs[2].InputName = TEXT("Random01");
    Specular->Inputs[2].Input.Connect(0, InstanceRandom);
    NormalAlpha->Description = GrassR13NormalAlphaDescription;
    NormalAlpha->Code = GrassR13NormalAlphaCode;
    NormalAlpha->OutputType = CMOT_Float1;
    ResetCustomAuxiliaryState(NormalAlpha);
    NormalAlpha->Inputs.SetNum(3);
    NormalAlpha->Inputs[0].InputName = TEXT("WorldPosition");
    NormalAlpha->Inputs[0].Input.Connect(0, WorldPosition);
    NormalAlpha->Inputs[1].InputName = TEXT("CameraPosition");
    NormalAlpha->Inputs[1].Input.Connect(0, CameraPosition);
    NormalAlpha->Inputs[2].InputName = TEXT("InstanceFade");
    NormalAlpha->Inputs[2].Input.Connect(0, InstanceFade);
    Attenuation->Desc = GrassR13SubsurfaceAttenuationDescription;
    Attenuation->Constant = GrassR13SubsurfaceAttenuation[ProfileIndex];
    Subsurface->A.Connect(0, Color);
    Subsurface->B.Connect(0, Attenuation);
    DistanceMatchedNormal->Alpha.Connect(0, NormalAlpha);
    FinalNormal->VectorInput.Connect(0, DistanceMatchedNormal);
    Data->BaseColor.Connect(0, Color);
    Data->Roughness.Connect(0, Roughness);
    Data->Specular.Connect(0, Specular);
    Data->Normal.Connect(0, FinalNormal);
    Data->SubsurfaceColor.Connect(0, Subsurface);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateGrassMaterial(Material, ProfileIndex, OutError);
}

bool ConfigureR15GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ValidateGrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material,
        GrassDescriptionPrefix + GrassProfiles[ProfileIndex].Id +
            TEXT("_R13"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR13RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR13SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR13NormalAlphaDescription);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionCameraPositionWS* CameraPosition =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR13CameraDescription);
    UMaterialExpressionConstant3Vector* SubsurfaceGain =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR13SubsurfaceAttenuationDescription);
    UMaterialExpressionMultiply* Subsurface =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR13SubsurfaceDescription);
    UMaterialExpressionNormalize* FinalNormal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR13NormalDescription);
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (Expression && Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
            {
                InstanceRandom = Expression;
            }
            if (Expression && Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
            {
                InstanceFade = Expression;
            }
        }
    }
    if (!Material || !Data || !Color || !Roughness || !Specular ||
        !NormalAlpha || !BladeUv || !WorldPosition || !CameraPosition ||
        !SubsurfaceGain || !Subsurface || !FinalNormal || !InstanceRandom ||
        !InstanceFade || Data->ExpressionCollection.Expressions.Num() != 29)
    {
        OutError = TEXT("The admitted R13 grass graph no longer exposes the exact 29 nodes required for the in-place R15 shade-readable response upgrade.");
        return false;
    }

    Material->Modify();
    Color->Modify();
    Roughness->Modify();
    Specular->Modify();
    NormalAlpha->Modify();
    CameraPosition->Modify();
    SubsurfaceGain->Modify();
    Subsurface->Modify();
    FinalNormal->Modify();
    Material->PreEditChange(nullptr);

    Color->Description = GrassDescriptionPrefix +
        GrassProfiles[ProfileIndex].Id + TEXT("_R15");
    Color->Code = BuildR15GrassColorCode(GrassProfiles[ProfileIndex]);
    ResetCustomAuxiliaryState(Color);
    Roughness->Description = GrassR15RoughnessDescription;
    Roughness->Code = GrassR15RoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    Specular->Description = GrassR15SpecularDescription;
    Specular->Code = GrassR15SpecularCode;
    ResetCustomAuxiliaryState(Specular);
    NormalAlpha->Description = GrassR15NormalAlphaDescription;
    NormalAlpha->Code = GrassR15NormalAlphaCode;
    ResetCustomAuxiliaryState(NormalAlpha);
    NormalAlpha->Inputs.SetNum(5);
    NormalAlpha->Inputs[0].InputName = TEXT("WorldPosition");
    NormalAlpha->Inputs[0].Input.Connect(0, WorldPosition);
    NormalAlpha->Inputs[1].InputName = TEXT("CameraPosition");
    NormalAlpha->Inputs[1].Input.Connect(0, CameraPosition);
    NormalAlpha->Inputs[2].InputName = TEXT("InstanceFade");
    NormalAlpha->Inputs[2].Input.Connect(0, InstanceFade);
    NormalAlpha->Inputs[3].InputName = TEXT("BladeUV");
    NormalAlpha->Inputs[3].Input.Connect(0, BladeUv);
    NormalAlpha->Inputs[4].InputName = TEXT("Random01");
    NormalAlpha->Inputs[4].Input.Connect(0, InstanceRandom);
    CameraPosition->Desc = GrassR15CameraDescription;
    SubsurfaceGain->Desc = GrassR15SubsurfaceGainDescription;
    SubsurfaceGain->Constant = GrassR15SubsurfaceGain[ProfileIndex];
    Subsurface->Desc = GrassR15SubsurfaceDescription;
    FinalNormal->Desc = GrassR15NormalDescription;

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR15GrassMaterial(Material, ProfileIndex, OutError);
}

bool ValidateR17GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (ProfileIndex < 0 || ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames))
    {
        OutError = TEXT("R17 grass profile index is outside the exact four-profile roster.");
        return false;
    }
    UMaterialEditorOnlyData* Data = Material ? Material->GetEditorOnlyData() : nullptr;
    const FGrassMaterialProfile& Profile = GrassProfiles[ProfileIndex];
    const FString ColorCode = BuildR17GrassColorCode(Profile);
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, GrassDescriptionPrefix + Profile.Id + TEXT("_R17"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR17RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR17SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR17NormalAlphaDescription);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR17SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR17SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR17CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR17NormalDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"));
    UMaterialExpressionCustom* Wind = FindCustomByDescription(
        Material, GrassWindDescription);
    UMaterialExpressionMaterialFunctionCall* Dither =
        FindExpressionByDescription<UMaterialExpressionMaterialFunctionCall>(
            Material, TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"));
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    int32 TextureSamples = 0;
    int32 InstanceRandomNodes = 0;
    int32 InstanceFadeNodes = 0;
    bool bNoCustomizedUvConnections = Data != nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression : Data->ExpressionCollection.Expressions)
        {
            TextureSamples += Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
            {
                ++InstanceRandomNodes;
                if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
                {
                    InstanceRandom = Expression;
                }
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
            {
                ++InstanceFadeNodes;
                if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
                {
                    InstanceFade = Expression;
                }
            }
        }
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bNoCustomizedUvConnections &= !CustomizedUv.Expression;
        }
    }
    const bool bExactColorInputs = Color && Color->Inputs.Num() == 3 &&
        Color->Inputs[0].InputName == TEXT("BladeUV") &&
        InputMatches(Color->Inputs[0].Input, BladeUv, 0) &&
        Color->Inputs[1].InputName == TEXT("Random01") &&
        InputMatches(Color->Inputs[1].Input, InstanceRandom, 0) &&
        Color->Inputs[2].InputName == TEXT("WorldPosition") &&
        InputMatches(Color->Inputs[2].Input, WorldPosition, 0);
    const bool bExactSurfaceInputs = Roughness && Specular &&
        Roughness->Inputs.Num() == 3 && Specular->Inputs.Num() == 3 &&
        Roughness->Inputs[0].InputName == TEXT("BaseRoughness") &&
        InputMatches(Roughness->Inputs[0].Input, BaseRoughness, 0) &&
        Roughness->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Roughness->Inputs[1].Input, BladeUv, 0) &&
        Roughness->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Roughness->Inputs[2].Input, InstanceRandom, 0) &&
        Specular->Inputs[0].InputName == TEXT("BaseSpecular") &&
        InputMatches(Specular->Inputs[0].Input, BaseSpecular, 0) &&
        Specular->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Specular->Inputs[1].Input, BladeUv, 0) &&
        Specular->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Specular->Inputs[2].Input, InstanceRandom, 0);
    const bool bExactNormalAlphaInputs = NormalAlpha &&
        NormalAlpha->Inputs.Num() == 5 &&
        NormalAlpha->Inputs[0].InputName == TEXT("WorldPosition") &&
        InputMatches(NormalAlpha->Inputs[0].Input, WorldPosition, 0) &&
        NormalAlpha->Inputs[1].InputName == TEXT("CameraPosition") &&
        InputMatches(NormalAlpha->Inputs[1].Input, Camera, 0) &&
        NormalAlpha->Inputs[2].InputName == TEXT("InstanceFade") &&
        InputMatches(NormalAlpha->Inputs[2].Input, InstanceFade, 0) &&
        NormalAlpha->Inputs[3].InputName == TEXT("BladeUV") &&
        InputMatches(NormalAlpha->Inputs[3].Input, BladeUv, 0) &&
        NormalAlpha->Inputs[4].InputName == TEXT("Random01") &&
        InputMatches(NormalAlpha->Inputs[4].Input, InstanceRandom, 0);
    if (!Material || !Data || !Profile.Id ||
        Material->GetPathName() != ObjectPath(GrassAssetNames[ProfileIndex]) ||
        Material->MaterialDomain != MD_Surface || Material->BlendMode != BLEND_Masked ||
        !Material->TwoSided || !Material->GetShadingModels().HasOnlyShadingModel(MSM_TwoSidedFoliage) ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() || Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Data->ExpressionCollection.Expressions.Num() != 29 || TextureSamples != 0 ||
        InstanceRandomNodes != 1 || InstanceFadeNodes != 1 || !bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        !BladeUv || !WorldPosition || !Camera || !InstanceRandom || !InstanceFade ||
        !Color || Color->Code != ColorCode ||
        ColorCode.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        ColorCode.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Color->OutputType != CMOT_Float3 || !bExactColorInputs || !bExactSurfaceInputs ||
        !Roughness || Roughness->Code != GrassR17RoughnessCode || Roughness->OutputType != CMOT_Float1 ||
        !Specular || Specular->Code != GrassR17SpecularCode || Specular->OutputType != CMOT_Float1 ||
        !bExactNormalAlphaInputs || NormalAlpha->Code != GrassR17NormalAlphaCode ||
        NormalAlpha->OutputType != CMOT_Float1 || !BaseRoughness || !BaseSpecular ||
        !FMath::IsNearlyEqual(BaseRoughness->DefaultValue, Profile.Roughness, 0.0001f) ||
        !FMath::IsNearlyEqual(BaseSpecular->DefaultValue, Profile.Specular, 0.0001f) ||
        !Subsurface || !Subsurface->Constant.Equals(GrassR17SubsurfaceGain[ProfileIndex], 0.0001f) ||
        !SubsurfaceMultiply || !InputMatches(SubsurfaceMultiply->A, Color, 0) ||
        !InputMatches(SubsurfaceMultiply->B, Subsurface, 0) ||
        !DistanceMatchedNormal || !InputMatches(DistanceMatchedNormal->Alpha, NormalAlpha, 0) ||
        !Normal || !InputMatches(Normal->VectorInput, DistanceMatchedNormal, 0) ||
        !Wind || Wind->Code != GrassWindCode || Wind->OutputType != CMOT_Float3 || !Dither ||
        !InputMatches(Data->BaseColor, Color, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->Normal, Normal, 0) ||
        !InputMatches(Data->SubsurfaceColor, SubsurfaceMultiply, 0) ||
        !InputMatches(Data->OpacityMask, Dither, 0) ||
        !InputMatches(Data->WorldPositionOffset, Wind, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression || Data->Tangent.Expression ||
        Data->EmissiveColor.Expression || Data->Opacity.Expression ||
        Data->AmbientOcclusion.Expression || Data->Displacement.Expression ||
        Data->PixelDepthOffset.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5D R17 grass material %d lost its exact 29-node photographic response, complete wiring, custom-node auxiliary-state seal, or non-emissive output state."),
            ProfileIndex);
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ConfigureR17GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ValidateR15GrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    const FGrassMaterialProfile& Profile = GrassProfiles[ProfileIndex];
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, GrassDescriptionPrefix + Profile.Id + TEXT("_R15"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR15RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR15SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR15NormalAlphaDescription);
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR15SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR15SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR15CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR15NormalDescription);
    if (!Color || !Roughness || !Specular || !NormalAlpha || !Subsurface ||
        !SubsurfaceMultiply || !Camera || !Normal)
    {
        OutError = TEXT("The clean R16 grass package does not expose every R15 response node required by R17.");
        return false;
    }
    Material->Modify(); Material->PreEditChange(nullptr);
    Color->Modify(); Roughness->Modify(); Specular->Modify(); NormalAlpha->Modify();
    Subsurface->Modify(); SubsurfaceMultiply->Modify(); Camera->Modify(); Normal->Modify();
    Color->Description = GrassDescriptionPrefix + Profile.Id + TEXT("_R17");
    Color->Code = BuildR17GrassColorCode(Profile);
    Roughness->Description = GrassR17RoughnessDescription;
    Roughness->Code = GrassR17RoughnessCode;
    Specular->Description = GrassR17SpecularDescription;
    Specular->Code = GrassR17SpecularCode;
    NormalAlpha->Description = GrassR17NormalAlphaDescription;
    NormalAlpha->Code = GrassR17NormalAlphaCode;
    Subsurface->Desc = GrassR17SubsurfaceGainDescription;
    Subsurface->Constant = GrassR17SubsurfaceGain[ProfileIndex];
    SubsurfaceMultiply->Desc = GrassR17SubsurfaceDescription;
    Camera->Desc = GrassR17CameraDescription;
    Normal->Desc = GrassR17NormalDescription;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange(); Material->MarkPackageDirty();
    return ValidateR17GrassMaterial(Material, ProfileIndex, OutError);
}

bool ValidateR19GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (ProfileIndex < 0 || ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames))
    {
        OutError = TEXT("R19 grass profile index is outside the exact four-profile roster.");
        return false;
    }
    UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    const FGrassMaterialProfile& Profile = GrassProfiles[ProfileIndex];
    const FString ColorCode = BuildR19GrassColorCode(Profile, ProfileIndex);
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, GrassDescriptionPrefix + Profile.Id + TEXT("_R19"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR19RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR19SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR19NormalAlphaDescription);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR19SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR19SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR19CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR19NormalDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"));
    UMaterialExpressionCustom* Wind = FindCustomByDescription(
        Material, GrassWindDescription);
    UMaterialExpressionMaterialFunctionCall* Dither =
        FindExpressionByDescription<UMaterialExpressionMaterialFunctionCall>(
            Material, TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"));
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    int32 TextureSamples = 0;
    int32 InstanceRandomNodes = 0;
    int32 InstanceFadeNodes = 0;
    bool bNoCustomizedUvConnections = Data != nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSamples +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
            {
                ++InstanceRandomNodes;
                if (Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
                {
                    InstanceRandom = Expression;
                }
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
            {
                ++InstanceFadeNodes;
                if (Expression->Desc ==
                    TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
                {
                    InstanceFade = Expression;
                }
            }
        }
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bNoCustomizedUvConnections &= !CustomizedUv.Expression;
        }
    }

    const bool bExactColorInputs = Color && Color->Inputs.Num() == 5 &&
        Color->Inputs[0].InputName == TEXT("BladeUV") &&
        InputMatches(Color->Inputs[0].Input, BladeUv, 0) &&
        Color->Inputs[1].InputName == TEXT("Random01") &&
        InputMatches(Color->Inputs[1].Input, InstanceRandom, 0) &&
        Color->Inputs[2].InputName == TEXT("WorldPosition") &&
        InputMatches(Color->Inputs[2].Input, WorldPosition, 0) &&
        Color->Inputs[3].InputName == TEXT("CameraPosition") &&
        InputMatches(Color->Inputs[3].Input, Camera, 0) &&
        Color->Inputs[4].InputName == TEXT("InstanceFade") &&
        InputMatches(Color->Inputs[4].Input, InstanceFade, 0);
    const bool bExactRoughnessInputs = Roughness &&
        Roughness->Inputs.Num() == 5 &&
        Roughness->Inputs[0].InputName == TEXT("BaseRoughness") &&
        InputMatches(Roughness->Inputs[0].Input, BaseRoughness, 0) &&
        Roughness->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Roughness->Inputs[1].Input, BladeUv, 0) &&
        Roughness->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Roughness->Inputs[2].Input, InstanceRandom, 0) &&
        Roughness->Inputs[3].InputName == TEXT("WorldPosition") &&
        InputMatches(Roughness->Inputs[3].Input, WorldPosition, 0) &&
        Roughness->Inputs[4].InputName == TEXT("CameraPosition") &&
        InputMatches(Roughness->Inputs[4].Input, Camera, 0);
    const bool bExactSpecularInputs = Specular &&
        Specular->Inputs.Num() == 5 &&
        Specular->Inputs[0].InputName == TEXT("BaseSpecular") &&
        InputMatches(Specular->Inputs[0].Input, BaseSpecular, 0) &&
        Specular->Inputs[1].InputName == TEXT("BladeUV") &&
        InputMatches(Specular->Inputs[1].Input, BladeUv, 0) &&
        Specular->Inputs[2].InputName == TEXT("Random01") &&
        InputMatches(Specular->Inputs[2].Input, InstanceRandom, 0) &&
        Specular->Inputs[3].InputName == TEXT("WorldPosition") &&
        InputMatches(Specular->Inputs[3].Input, WorldPosition, 0) &&
        Specular->Inputs[4].InputName == TEXT("CameraPosition") &&
        InputMatches(Specular->Inputs[4].Input, Camera, 0);
    const bool bExactNormalInputs = NormalAlpha &&
        NormalAlpha->Inputs.Num() == 5 &&
        NormalAlpha->Inputs[0].InputName == TEXT("WorldPosition") &&
        InputMatches(NormalAlpha->Inputs[0].Input, WorldPosition, 0) &&
        NormalAlpha->Inputs[1].InputName == TEXT("CameraPosition") &&
        InputMatches(NormalAlpha->Inputs[1].Input, Camera, 0) &&
        NormalAlpha->Inputs[2].InputName == TEXT("InstanceFade") &&
        InputMatches(NormalAlpha->Inputs[2].Input, InstanceFade, 0) &&
        NormalAlpha->Inputs[3].InputName == TEXT("BladeUV") &&
        InputMatches(NormalAlpha->Inputs[3].Input, BladeUv, 0) &&
        NormalAlpha->Inputs[4].InputName == TEXT("Random01") &&
        InputMatches(NormalAlpha->Inputs[4].Input, InstanceRandom, 0);

    if (!Material || !Data || !Profile.Id ||
        Material->GetPathName() != ObjectPath(GrassAssetNames[ProfileIndex]) ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || !Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(
            MSM_TwoSidedFoliage) ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        Data->ExpressionCollection.Expressions.Num() != 29 ||
        TextureSamples != 0 || InstanceRandomNodes != 1 ||
        InstanceFadeNodes != 1 || !bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        !BladeUv || !WorldPosition ||
        WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        !Camera || !InstanceRandom || !InstanceFade ||
        !Color || Color->Code != ColorCode ||
        ColorCode.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        ColorCode.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Color->OutputType != CMOT_Float3 || !bExactColorInputs ||
        !Roughness || Roughness->Code != GrassR19RoughnessCode ||
        Roughness->Code.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        Roughness->Code.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Roughness->OutputType != CMOT_Float1 || !bExactRoughnessInputs ||
        !Specular || Specular->Code != GrassR19SpecularCode ||
        Specular->Code.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        Specular->Code.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Specular->OutputType != CMOT_Float1 || !bExactSpecularInputs ||
        !NormalAlpha || NormalAlpha->Code != GrassR19NormalAlphaCode ||
        NormalAlpha->Code.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        NormalAlpha->Code.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        NormalAlpha->OutputType != CMOT_Float1 || !bExactNormalInputs ||
        !BaseRoughness || !BaseSpecular ||
        !FMath::IsNearlyEqual(
            BaseRoughness->DefaultValue, Profile.Roughness, 0.0001f) ||
        !FMath::IsNearlyEqual(
            BaseSpecular->DefaultValue, Profile.Specular, 0.0001f) ||
        !Subsurface || !Subsurface->Constant.Equals(
            GrassR19SubsurfaceGain[ProfileIndex], 0.0001f) ||
        !SubsurfaceMultiply ||
        !InputMatches(SubsurfaceMultiply->A, Color, 0) ||
        !InputMatches(SubsurfaceMultiply->B, Subsurface, 0) ||
        !DistanceMatchedNormal ||
        !InputMatches(DistanceMatchedNormal->Alpha, NormalAlpha, 0) ||
        !Normal ||
        !InputMatches(Normal->VectorInput, DistanceMatchedNormal, 0) ||
        !Wind || Wind->Code != GrassWindCode ||
        Wind->OutputType != CMOT_Float3 || !Dither ||
        !InputMatches(Data->BaseColor, Color, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->Normal, Normal, 0) ||
        !InputMatches(Data->SubsurfaceColor, SubsurfaceMultiply, 0) ||
        !InputMatches(Data->OpacityMask, Dither, 0) ||
        !InputMatches(Data->WorldPositionOffset, Wind, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->AmbientOcclusion.Expression ||
        Data->Displacement.Expression || Data->PixelDepthOffset.Expression)
    {
        OutError = FString::Printf(
            TEXT("V5D R19 grass material %d lost its exact 29-node two-scale photographic response, distance anti-moire wiring, custom-node auxiliary-state seal, or non-emissive output state."),
            ProfileIndex);
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ConfigureR19GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ValidateR17GrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    const FGrassMaterialProfile& Profile = GrassProfiles[ProfileIndex];
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, GrassDescriptionPrefix + Profile.Id + TEXT("_R17"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR17RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR17SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR17NormalAlphaDescription);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR17SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR17SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR17CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR17NormalDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom") &&
                Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
            {
                InstanceRandom = Expression;
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount") &&
                Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
            {
                InstanceFade = Expression;
            }
        }
    }
    if (!Data || !Color || !Roughness || !Specular || !NormalAlpha ||
        !BladeUv || !WorldPosition || !Subsurface || !SubsurfaceMultiply ||
        !Camera || !Normal || !BaseRoughness || !BaseSpecular ||
        !InstanceRandom || !InstanceFade)
    {
        OutError = TEXT("The clean R18 namespace does not expose every existing R17 graph node required by the zero-new-node R19 grass upgrade.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    Color->Modify(); Roughness->Modify(); Specular->Modify();
    NormalAlpha->Modify(); Subsurface->Modify();
    SubsurfaceMultiply->Modify(); Camera->Modify(); Normal->Modify();

    Color->Description = GrassDescriptionPrefix + Profile.Id + TEXT("_R19");
    Color->Code = BuildR19GrassColorCode(Profile, ProfileIndex);
    ResetCustomAuxiliaryState(Color);
    Color->Inputs.SetNum(5);
    Color->Inputs[0].InputName = TEXT("BladeUV");
    Color->Inputs[0].Input.Connect(0, BladeUv);
    Color->Inputs[1].InputName = TEXT("Random01");
    Color->Inputs[1].Input.Connect(0, InstanceRandom);
    Color->Inputs[2].InputName = TEXT("WorldPosition");
    Color->Inputs[2].Input.Connect(0, WorldPosition);
    Color->Inputs[3].InputName = TEXT("CameraPosition");
    Color->Inputs[3].Input.Connect(0, Camera);
    Color->Inputs[4].InputName = TEXT("InstanceFade");
    Color->Inputs[4].Input.Connect(0, InstanceFade);

    Roughness->Description = GrassR19RoughnessDescription;
    Roughness->Code = GrassR19RoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    Roughness->Inputs.SetNum(5);
    Roughness->Inputs[0].InputName = TEXT("BaseRoughness");
    Roughness->Inputs[0].Input.Connect(0, BaseRoughness);
    Roughness->Inputs[1].InputName = TEXT("BladeUV");
    Roughness->Inputs[1].Input.Connect(0, BladeUv);
    Roughness->Inputs[2].InputName = TEXT("Random01");
    Roughness->Inputs[2].Input.Connect(0, InstanceRandom);
    Roughness->Inputs[3].InputName = TEXT("WorldPosition");
    Roughness->Inputs[3].Input.Connect(0, WorldPosition);
    Roughness->Inputs[4].InputName = TEXT("CameraPosition");
    Roughness->Inputs[4].Input.Connect(0, Camera);

    Specular->Description = GrassR19SpecularDescription;
    Specular->Code = GrassR19SpecularCode;
    ResetCustomAuxiliaryState(Specular);
    Specular->Inputs.SetNum(5);
    Specular->Inputs[0].InputName = TEXT("BaseSpecular");
    Specular->Inputs[0].Input.Connect(0, BaseSpecular);
    Specular->Inputs[1].InputName = TEXT("BladeUV");
    Specular->Inputs[1].Input.Connect(0, BladeUv);
    Specular->Inputs[2].InputName = TEXT("Random01");
    Specular->Inputs[2].Input.Connect(0, InstanceRandom);
    Specular->Inputs[3].InputName = TEXT("WorldPosition");
    Specular->Inputs[3].Input.Connect(0, WorldPosition);
    Specular->Inputs[4].InputName = TEXT("CameraPosition");
    Specular->Inputs[4].Input.Connect(0, Camera);

    NormalAlpha->Description = GrassR19NormalAlphaDescription;
    NormalAlpha->Code = GrassR19NormalAlphaCode;
    ResetCustomAuxiliaryState(NormalAlpha);
    Subsurface->Desc = GrassR19SubsurfaceGainDescription;
    Subsurface->Constant = GrassR19SubsurfaceGain[ProfileIndex];
    SubsurfaceMultiply->Desc = GrassR19SubsurfaceDescription;
    Camera->Desc = GrassR19CameraDescription;
    Normal->Desc = GrassR19NormalDescription;

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR19GrassMaterial(Material, ProfileIndex, OutError);
}

FString BuildR21WpoFingerprint(const UMaterialExpressionCustom* Wind)
{
    if (!Wind)
    {
        return FString();
    }
    FString Result = FString::Printf(
        TEXT("description=%s|code=%s|output=%d|inputs=%d"),
        *Wind->Description,
        *Wind->Code,
        static_cast<int32>(Wind->OutputType),
        Wind->Inputs.Num());
    for (const auto& Input : Wind->Inputs)
    {
        Result += FString::Printf(
            TEXT("|%s=%s:%d"),
            *Input.InputName.ToString(),
            Input.Input.Expression
                ? *Input.Input.Expression->GetPathName()
                : TEXT("<null>"),
            Input.Input.OutputIndex);
    }
    const UMaterialExpressionTransformPosition* InstanceLocalPosition =
        Wind->Inputs.Num() > 1
        ? Cast<UMaterialExpressionTransformPosition>(
            Wind->Inputs[1].Input.Expression)
        : nullptr;
    const UMaterialExpressionWorldPosition* WorldPosition =
        InstanceLocalPosition
        ? Cast<UMaterialExpressionWorldPosition>(
            InstanceLocalPosition->Input.Expression)
        : nullptr;
    Result += FString::Printf(
        TEXT("|instanceLocalDesc=%s|transformSource=%d|transformType=%d|instanceLocalInput=%s:%d|worldPositionDesc=%s|worldPositionShaderOffset=%d"),
        InstanceLocalPosition ? *InstanceLocalPosition->Desc : TEXT("<null>"),
        InstanceLocalPosition
            ? static_cast<int32>(InstanceLocalPosition->TransformSourceType)
            : INDEX_NONE,
        InstanceLocalPosition
            ? static_cast<int32>(InstanceLocalPosition->TransformType)
            : INDEX_NONE,
        InstanceLocalPosition && InstanceLocalPosition->Input.Expression
            ? *InstanceLocalPosition->Input.Expression->GetPathName()
            : TEXT("<null>"),
        InstanceLocalPosition
            ? InstanceLocalPosition->Input.OutputIndex
            : INDEX_NONE,
        WorldPosition ? *WorldPosition->Desc : TEXT("<null>"),
        WorldPosition
            ? static_cast<int32>(
                WorldPosition->WorldPositionShaderOffset)
            : INDEX_NONE);
    Result += FString::Printf(
        TEXT("|additionalOutputs=%d|additionalDefines=%d|includes=%d"),
        Wind->AdditionalOutputs.Num(),
        Wind->AdditionalDefines.Num(),
        Wind->IncludeFilePaths.Num());
    return Result;
}

bool ValidateR21R22OrR23GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    int32 MaterialRevision,
    FString& OutError,
    bool bRequireR23BCalibration = false,
    const FString* ExpectedObjectPathOverride = nullptr,
    const FString* StableDescriptionOverride = nullptr,
    const FString* StableCodeOverride = nullptr,
    const FString* ColorDescriptionOverride = nullptr,
    const FString* ColorCodeOverride = nullptr)
{
    const bool bR22 = MaterialRevision == 22;
    const bool bR23 = MaterialRevision == 23;
    const bool bR23B = bR23 && bRequireR23BCalibration;
    const bool bStableRevision = bR22 || bR23;
    const bool bHasAnyDerivativeOverride = ExpectedObjectPathOverride ||
        StableDescriptionOverride || StableCodeOverride ||
        ColorDescriptionOverride || ColorCodeOverride;
    const bool bHasAnyColorOverride =
        ColorDescriptionOverride || ColorCodeOverride;
    if (bHasAnyDerivativeOverride &&
        (!bR23B || !ExpectedObjectPathOverride ||
         !StableDescriptionOverride || !StableCodeOverride))
    {
        OutError = TEXT("Grass material derivative overrides require the complete R23B object-path and stable-visibility contract.");
        return false;
    }
    if (bHasAnyColorOverride &&
        (!ColorDescriptionOverride || !ColorCodeOverride))
    {
        OutError = TEXT("Grass material visual derivatives require both the blade-colour description and code replacement.");
        return false;
    }
    if (MaterialRevision != 21 && !bStableRevision)
    {
        OutError = TEXT("Unsupported grass material validator revision.");
        return false;
    }
    if (ProfileIndex < 0 || ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames))
    {
        OutError = FString::Printf(
            TEXT("R%d grass profile index is outside the exact four-profile roster."),
            MaterialRevision);
        return false;
    }
    UMaterialEditorOnlyData* Data =
        Material ? Material->GetEditorOnlyData() : nullptr;
    const FR21GrassMaterialProfile& Profile = bR23B
        ? GrassR23BProfiles[ProfileIndex]
        : (bR23 ? GrassR23Profiles[ProfileIndex]
            : GrassR21Profiles[ProfileIndex]);
    const FString CanonicalColorCode = bR23B
        ? BuildR23BGrassColorCode(Profile)
        : (bR23 ? BuildR23GrassColorCode(Profile)
            : BuildR21GrassColorCode(Profile));
    const FString& ColorCode = ColorCodeOverride
        ? *ColorCodeOverride : CanonicalColorCode;
    const FString& RoughnessDescription = bR23B
        ? GrassR23BRoughnessDescription
        : (bR23 ? GrassR23RoughnessDescription : GrassR21RoughnessDescription);
    const FString& RoughnessCode = bR23B
        ? GrassR23BRoughnessCode
        : (bR23 ? GrassR23RoughnessCode : GrassR21RoughnessCode);
    const FString& SpecularDescription = bR23B
        ? GrassR23BSpecularDescription
        : (bR23 ? GrassR23SpecularDescription : GrassR21SpecularDescription);
    const FString& SpecularCode = bR23B
        ? GrassR23BSpecularCode
        : (bR23 ? GrassR23SpecularCode : GrassR21SpecularCode);
    const FString& NormalAlphaDescription = bR23B
        ? GrassR23BNormalAlphaDescription
        : (bR23 ? GrassR23NormalAlphaDescription : GrassR21NormalAlphaDescription);
    const FString& NormalAlphaCode = bR23B
        ? GrassR23BNormalAlphaCode
        : (bR23 ? GrassR23NormalAlphaCode : GrassR21NormalAlphaCode);
    const FString& SubsurfaceGainDescription = bR23B
        ? GrassR23BSubsurfaceGainDescription
        : (bR23 ? GrassR23SubsurfaceGainDescription
            : GrassR21SubsurfaceGainDescription);
    const FString& SubsurfaceDescription = bR23B
        ? GrassR23BSubsurfaceDescription
        : (bR23 ? GrassR23SubsurfaceDescription : GrassR21SubsurfaceDescription);
    const FString& CameraDescription = bR23B
        ? GrassR23BCameraDescription
        : (bR23 ? GrassR23CameraDescription : GrassR21CameraDescription);
    const FString& NormalDescription = bR23B
        ? GrassR23BNormalDescription
        : (bR23 ? GrassR23NormalDescription : GrassR21NormalDescription);
    const FString& StableDescription = StableDescriptionOverride
        ? *StableDescriptionOverride
        : (bR23B
            ? GrassR23BStableVisibilityDescription
            : (bR23 ? GrassR23StableVisibilityDescription
                : GrassR22StableVisibilityDescription));
    const FString& StableCode = StableCodeOverride
        ? *StableCodeOverride
        : (bR23B
            ? GrassR23BStableVisibilityCode
            : (bR23 ? GrassR23StableVisibilityCode
                : GrassR22StableVisibilityCode));
    const FString ExpectedObjectPath = ExpectedObjectPathOverride
        ? *ExpectedObjectPathOverride
        : ObjectPath(GrassAssetNames[ProfileIndex]);
    const FString CanonicalColorDescription =
        GrassDescriptionPrefix + Profile.Id +
        (bR23B ? TEXT("_R23B") : (bR23 ? TEXT("_R23") : TEXT("_R21")));
    const FString& ColorDescription = ColorDescriptionOverride
        ? *ColorDescriptionOverride : CanonicalColorDescription;
    const FString& RevisionDescription = bR23
        ? R23RuntimeRevisionDescription : R22RuntimeRevisionDescription;
    const float RevisionValue = bR23
        ? R23RuntimeRevisionValue : R22RuntimeRevisionValue;
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, ColorDescription);
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, NormalAlphaDescription);
    UMaterialExpressionTextureCoordinate* BladeUv =
        FindExpressionByDescription<UMaterialExpressionTextureCoordinate>(
            Material, TEXT("V5B_BERMUDA_BLADE_SEED_HEIGHT_UV0"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionTransformPosition* InstanceLocalPosition =
        FindExpressionByDescription<UMaterialExpressionTransformPosition>(
            Material, TEXT("V5B_BERMUDA_INSTANCE_LOCAL_POSITION"));
    UMaterialExpressionTime* Time =
        FindExpressionByDescription<UMaterialExpressionTime>(
            Material, TEXT("V5B_BERMUDA_TIME"));
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, NormalDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionScalarParameter* WindStrength =
        FindScalar(Material, TEXT("TRIAD_WindStrengthCm"));
    UMaterialExpressionScalarParameter* WindSpeed =
        FindScalar(Material, TEXT("TRIAD_WindSpeed"));
    UMaterialExpressionScalarParameter* WindHeight =
        FindScalar(Material, TEXT("TRIAD_WindHeightCm"));
    UMaterialExpressionScalarParameter* WindResponse =
        FindScalar(Material, TEXT("TRIAD_WindResponseScale"));
    UMaterialExpressionScalarParameter* MaximumWpo =
        FindScalar(Material, TEXT("TRIAD_MaxWpoCm"));
    UMaterialExpressionScalarParameter* RevisionMarker = bStableRevision
        ? FindScalar(Material, R22RuntimeRevisionParameterName)
        : nullptr;
    UMaterialExpressionScalarParameter* CalibrationRevisionMarker = bR23B
        ? FindScalar(Material, R23BRuntimeCalibrationRevisionParameterName)
        : nullptr;
    UMaterialExpressionVectorParameter* WindDirection =
        FindVector(Material, TEXT("TRIAD_WindDirection"));
    UMaterialExpressionConstant3Vector* WorldUpNormal =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material,
            TEXT("V5B_BERMUDA_WORLD_UP_NORMAL"));
    UMaterialExpressionVertexNormalWS* ImportedCurvatureNormal =
        FindExpressionByDescription<UMaterialExpressionVertexNormalWS>(
            Material,
            TEXT("V5B_BERMUDA_IMPORTED_CURVATURE_NORMAL_WS"));
    UMaterialExpressionTwoSidedSign* TwoSidedSign =
        FindExpressionByDescription<UMaterialExpressionTwoSidedSign>(
            Material,
            TEXT("V5B_BERMUDA_TWO_SIDED_SIGN"));
    UMaterialExpressionMultiply* FacingCorrectedNormal =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material,
            TEXT("V5B_BERMUDA_FACING_CORRECTED_NORMAL"));
    UMaterialExpressionLinearInterpolate* DistanceMatchedNormal =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5B_BERMUDA_DISTANCE_MATCHED_NORMAL"));
    UMaterialExpressionCustom* Wind = FindCustomByDescription(
        Material, GrassWindDescription);
    UMaterialExpressionMaterialFunctionCall* Dither =
        FindExpressionByDescription<UMaterialExpressionMaterialFunctionCall>(
            Material, TEXT("V5B_BERMUDA_DITHERED_INSTANCE_FADE"));
    UMaterialExpressionCustom* StableVisibility = FindCustomByDescription(
        Material, StableDescription);
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    int32 TextureSamples = 0;
    int32 InstanceRandomNodes = 0;
    int32 InstanceFadeNodes = 0;
    bool bNoCustomizedUvConnections = Data != nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            TextureSamples +=
                Cast<UMaterialExpressionTextureSample>(Expression) ? 1 : 0;
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom"))
            {
                ++InstanceRandomNodes;
                if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
                {
                    InstanceRandom = Expression;
                }
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount"))
            {
                ++InstanceFadeNodes;
                if (Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
                {
                    InstanceFade = Expression;
                }
            }
        }
        for (const FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
        {
            bNoCustomizedUvConnections &= !CustomizedUv.Expression;
        }
    }

    const bool bExactColorInputs = Color && Color->Inputs.Num() == 5 &&
        Color->Inputs[0].InputName == TEXT("BladeUV") &&
        InputMatches(Color->Inputs[0].Input, BladeUv, 0) &&
        Color->Inputs[1].InputName == TEXT("Random01") &&
        InputMatches(Color->Inputs[1].Input, InstanceRandom, 0) &&
        Color->Inputs[2].InputName == TEXT("WorldPosition") &&
        InputMatches(Color->Inputs[2].Input, WorldPosition, 0) &&
        Color->Inputs[3].InputName == TEXT("CameraPosition") &&
        InputMatches(Color->Inputs[3].Input, Camera, 0) &&
        Color->Inputs[4].InputName == TEXT("InstanceFade") &&
        InputMatches(Color->Inputs[4].Input, InstanceFade, 0);
    const auto HasExactFiveSurfaceInputs =
        [BladeUv, InstanceRandom, WorldPosition, Camera](
            const UMaterialExpressionCustom* Custom,
            const FName& BaseName,
            const UMaterialExpression* Base)
        {
            return Custom && Custom->Inputs.Num() == 5 &&
                Custom->Inputs[0].InputName == BaseName &&
                InputMatches(Custom->Inputs[0].Input, Base, 0) &&
                Custom->Inputs[1].InputName == TEXT("BladeUV") &&
                InputMatches(Custom->Inputs[1].Input, BladeUv, 0) &&
                Custom->Inputs[2].InputName == TEXT("Random01") &&
                InputMatches(Custom->Inputs[2].Input, InstanceRandom, 0) &&
                Custom->Inputs[3].InputName == TEXT("WorldPosition") &&
                InputMatches(Custom->Inputs[3].Input, WorldPosition, 0) &&
                Custom->Inputs[4].InputName == TEXT("CameraPosition") &&
                InputMatches(Custom->Inputs[4].Input, Camera, 0);
        };
    const bool bExactNormalInputs = NormalAlpha &&
        NormalAlpha->Inputs.Num() == 5 &&
        NormalAlpha->Inputs[0].InputName == TEXT("WorldPosition") &&
        InputMatches(NormalAlpha->Inputs[0].Input, WorldPosition, 0) &&
        NormalAlpha->Inputs[1].InputName == TEXT("CameraPosition") &&
        InputMatches(NormalAlpha->Inputs[1].Input, Camera, 0) &&
        NormalAlpha->Inputs[2].InputName == TEXT("InstanceFade") &&
        InputMatches(NormalAlpha->Inputs[2].Input, InstanceFade, 0) &&
        NormalAlpha->Inputs[3].InputName == TEXT("BladeUV") &&
        InputMatches(NormalAlpha->Inputs[3].Input, BladeUv, 0) &&
        NormalAlpha->Inputs[4].InputName == TEXT("Random01") &&
        InputMatches(NormalAlpha->Inputs[4].Input, InstanceRandom, 0);

    const FName WindInputNames[] = {
        TEXT("WorldPosition"), TEXT("InstanceLocalPosition"),
        TEXT("BladeUV"), TEXT("TimeSeconds"), TEXT("WindStrengthCm"),
        TEXT("WindSpeed"), TEXT("WindDirection"), TEXT("HeightCm"),
        TEXT("ResponseScale"), TEXT("MaxWpoCm"), TEXT("Random01"),
        TEXT("InstanceFade")};
    const UMaterialExpression* WindInputNodes[] = {
        WorldPosition, InstanceLocalPosition, BladeUv, Time, WindStrength,
        WindSpeed, WindDirection, WindHeight, WindResponse, MaximumWpo,
        InstanceRandom, InstanceFade};
    bool bExactWindInputs = Wind && Wind->Inputs.Num() == 12;
    for (int32 Index = 0; bExactWindInputs && Index < 12; ++Index)
    {
        bExactWindInputs &=
            Wind->Inputs[Index].InputName == WindInputNames[Index] &&
            InputMatches(Wind->Inputs[Index].Input, WindInputNodes[Index], 0);
    }
    int32 DitherAlphaIndex = INDEX_NONE;
    int32 DitherAlphaCount = 0;
    bool bNoUnexpectedDitherInputs = true;
    if (Dither && Dither->MaterialFunction &&
        Dither->MaterialFunction->GetPathName() ==
            GrassDitherTemporalAaFunctionPath)
    {
        for (int32 Index = 0; Index < Dither->FunctionInputs.Num(); ++Index)
        {
            if (Dither->GetInputName(Index).ToString().StartsWith(
                    TEXT("Alpha Threshold")))
            {
                DitherAlphaIndex = Index;
                ++DitherAlphaCount;
            }
            else
            {
                bNoUnexpectedDitherInputs &=
                    !Dither->FunctionInputs[Index].Input.Expression;
            }
        }
    }
    const bool bExactDither = Dither && DitherAlphaCount == 1 &&
        DitherAlphaIndex != INDEX_NONE && bNoUnexpectedDitherInputs &&
        InputMatches(
            Dither->FunctionInputs[DitherAlphaIndex].Input,
            InstanceFade,
            0);
    const bool bExactStableVisibility = bStableRevision && StableVisibility &&
        StableVisibility->Code == StableCode &&
        StableVisibility->OutputType == CMOT_Float1 &&
        StableVisibility->Inputs.Num() == (bR23B ? 6 : 5) &&
        StableVisibility->Inputs[0].InputName == TEXT("InstanceFade") &&
        InputMatches(
            StableVisibility->Inputs[0].Input, InstanceFade, 0) &&
        StableVisibility->Inputs[1].InputName == TEXT("Random01") &&
        InputMatches(
            StableVisibility->Inputs[1].Input, InstanceRandom, 0) &&
        StableVisibility->Inputs[2].InputName == TEXT("WorldPosition") &&
        InputMatches(
            StableVisibility->Inputs[2].Input, WorldPosition, 0) &&
        StableVisibility->Inputs[3].InputName == TEXT("CameraPosition") &&
        InputMatches(
            StableVisibility->Inputs[3].Input, Camera, 0) &&
        StableVisibility->Inputs[4].InputName == TEXT("MaterialRevision") &&
        InputMatches(
            StableVisibility->Inputs[4].Input, RevisionMarker, 0) &&
        RevisionMarker &&
        RevisionMarker->Desc == RevisionDescription &&
        FMath::IsNearlyEqual(
            RevisionMarker->DefaultValue,
            RevisionValue,
            0.0001f) &&
        (!bR23B ||
            (StableVisibility->Inputs[5].InputName ==
                 TEXT("CalibrationRevision") &&
             InputMatches(
                 StableVisibility->Inputs[5].Input,
                 CalibrationRevisionMarker,
                 0) &&
             CalibrationRevisionMarker &&
             CalibrationRevisionMarker->Desc ==
                 R23BRuntimeCalibrationRevisionDescription &&
             FMath::IsNearlyEqual(
                 CalibrationRevisionMarker->DefaultValue,
                 R23BRuntimeCalibrationRevisionValue,
                 0.0001f))) &&
        HasNoAuxiliaryCustomState(StableVisibility);

    if (!Material || !Data || !Profile.Id ||
        Material->GetPathName() != ExpectedObjectPath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || !Material->TwoSided ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_TwoSidedFoliage) ||
        Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        !Material->NaniteOverrideMaterial.bEnableOverride ||
        Material->NaniteOverrideMaterial.GetOverrideMaterial() ||
        Material->GetNaniteOverride() ||
        !Material->bUsedWithInstancedStaticMeshes ||
        !FMath::IsNearlyEqual(Material->OpacityMaskClipValue, 0.50f, 0.000001f) ||
        !FMath::IsNearlyEqual(
            Material->MaxWorldPositionOffsetDisplacement,
            GrassMaximumWpoCm,
            0.000001f) ||
        Data->ExpressionCollection.Expressions.Num() !=
            (bStableRevision ? (bR23B ? 32 : 31) : 29) ||
        TextureSamples != 0 || InstanceRandomNodes != 1 ||
        InstanceFadeNodes != 1 || !bNoCustomizedUvConnections ||
        !AllCustomExpressionsHaveNoAuxiliaryState(Data) ||
        !BladeUv || BladeUv->CoordinateIndex != 0 || BladeUv->UnMirrorU ||
        BladeUv->UnMirrorV ||
        !FMath::IsNearlyEqual(BladeUv->UTiling, 1.0f, 0.000001f) ||
        !FMath::IsNearlyEqual(BladeUv->VTiling, 1.0f, 0.000001f) ||
        !WorldPosition || WorldPosition->WorldPositionShaderOffset !=
            WPT_ExcludeAllShaderOffsets ||
        !InstanceLocalPosition ||
        InstanceLocalPosition->TransformSourceType !=
            TRANSFORMPOSSOURCE_World ||
        InstanceLocalPosition->TransformType !=
            TRANSFORMPOSSOURCE_Instance ||
        !InputMatches(InstanceLocalPosition->Input, WorldPosition, 0) ||
        !Time || Time->bOverride_Period ||
        Time->bIgnorePause || !Camera || !InstanceRandom || !InstanceFade ||
        !Color || Color->Code != ColorCode ||
        ColorCode.Contains(TEXT("sin("), ESearchCase::CaseSensitive) ||
        ColorCode.Contains(TEXT("cos("), ESearchCase::CaseSensitive) ||
        Color->OutputType != CMOT_Float3 || !bExactColorInputs ||
        !Roughness || Roughness->Code != RoughnessCode ||
        Roughness->OutputType != CMOT_Float1 ||
        !HasExactFiveSurfaceInputs(Roughness, TEXT("BaseRoughness"), BaseRoughness) ||
        !Specular || Specular->Code != SpecularCode ||
        Specular->OutputType != CMOT_Float1 ||
        !HasExactFiveSurfaceInputs(Specular, TEXT("BaseSpecular"), BaseSpecular) ||
        !NormalAlpha || NormalAlpha->Code != NormalAlphaCode ||
        NormalAlpha->OutputType != CMOT_Float1 || !bExactNormalInputs ||
        !BaseRoughness || !FMath::IsNearlyEqual(
            BaseRoughness->DefaultValue, Profile.Roughness, 0.0001f) ||
        !BaseSpecular || !FMath::IsNearlyEqual(
            BaseSpecular->DefaultValue, Profile.Specular, 0.0001f) ||
        !Subsurface || !Subsurface->Constant.Equals(
            Profile.SubsurfaceGain, 0.0001f) ||
        !SubsurfaceMultiply ||
        !InputMatches(SubsurfaceMultiply->A, Color, 0) ||
        !InputMatches(SubsurfaceMultiply->B, Subsurface, 0) ||
        !WorldUpNormal || !WorldUpNormal->Constant.Equals(
            FLinearColor(0.0f, 0.0f, 1.0f), 0.000001f) ||
        !ImportedCurvatureNormal || !TwoSidedSign ||
        !FacingCorrectedNormal ||
        !InputMatches(
            FacingCorrectedNormal->A, ImportedCurvatureNormal, 0) ||
        !InputMatches(FacingCorrectedNormal->B, TwoSidedSign, 0) ||
        !DistanceMatchedNormal ||
        !InputMatches(DistanceMatchedNormal->A, WorldUpNormal, 0) ||
        !InputMatches(
            DistanceMatchedNormal->B, FacingCorrectedNormal, 0) ||
        !InputMatches(DistanceMatchedNormal->Alpha, NormalAlpha, 0) ||
        !Normal || !InputMatches(Normal->VectorInput, DistanceMatchedNormal, 0) ||
        !Wind || Wind->Description != GrassWindDescription ||
        Wind->Code != GrassWindCode || Wind->OutputType != CMOT_Float3 ||
        !bExactWindInputs || !HasNoAuxiliaryCustomState(Wind) ||
        !WindStrength || !FMath::IsNearlyEqual(
            WindStrength->DefaultValue,
            bStableRevision ? 0.0f : GrassWindStrengthCm,
            0.0001f) ||
        !WindSpeed || !FMath::IsNearlyEqual(
            WindSpeed->DefaultValue, GrassWindSpeed, 0.0001f) ||
        !WindHeight || !FMath::IsNearlyEqual(
            WindHeight->DefaultValue, GrassWindHeightCm, 0.0001f) ||
        !WindResponse || !FMath::IsNearlyEqual(
            WindResponse->DefaultValue, Profile.WindResponse, 0.0001f) ||
        !MaximumWpo || !FMath::IsNearlyEqual(
            MaximumWpo->DefaultValue, GrassMaximumWpoCm, 0.0001f) ||
        !WindDirection || !WindDirection->DefaultValue.Equals(
            GrassWindDirection, 0.0001f) || !bExactDither ||
        (bStableRevision
            ? !bExactStableVisibility
            : StableVisibility != nullptr) ||
        !InputMatches(Data->BaseColor, Color, 0) ||
        !InputMatches(Data->Roughness, Roughness, 0) ||
        !InputMatches(Data->Specular, Specular, 0) ||
        !InputMatches(Data->Normal, Normal, 0) ||
        !InputMatches(Data->SubsurfaceColor, SubsurfaceMultiply, 0) ||
        !InputMatches(
            Data->OpacityMask,
            bStableRevision
                ? static_cast<UMaterialExpression*>(StableVisibility)
                : static_cast<UMaterialExpression*>(Dither),
            0) ||
        !InputMatches(Data->WorldPositionOffset, Wind, 0) ||
        Data->Metallic.Expression || Data->Anisotropy.Expression ||
        Data->Tangent.Expression || Data->EmissiveColor.Expression ||
        Data->Opacity.Expression || Data->AmbientOcclusion.Expression ||
        Data->Displacement.Expression || Data->PixelDepthOffset.Expression)
    {
        OutError = bStableRevision
            ? (bR23B
                ? FString::Printf(
                      TEXT("V5D R23B grass material %d lost its exact 32-node calibrated response, stable spatial visibility, base/runtime calibration revision markers, zero-default-wind, preserved dormant dither, or non-emissive output state."),
                      ProfileIndex)
                : FString::Printf(
                      TEXT("V5D R%d grass material %d lost its exact 31-node calibrated response, stable spatial visibility, runtime revision marker, zero-default-wind, preserved dormant dither, or non-emissive output state."),
                      MaterialRevision,
                      ProfileIndex))
            : FString::Printf(
                  TEXT("V5D R21 grass material %d lost its exact 29-node calibrated response, zero-texture graph, byte-stable WPO/opacity topology, or non-emissive output state."),
                  ProfileIndex);
        return false;
    }
    return ValidateCompiledMaterial(Material, OutError);
}

bool ValidateR21GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    return ValidateR21R22OrR23GrassMaterial(
        Material, ProfileIndex, 21, OutError);
}

bool ValidateR22GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    return ValidateR21R22OrR23GrassMaterial(
        Material, ProfileIndex, 22, OutError);
}

bool ValidateR23GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    return ValidateR21R22OrR23GrassMaterial(
        Material, ProfileIndex, 23, OutError);
}

bool ValidateR23BGrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    return ValidateR21R22OrR23GrassMaterial(
        Material, ProfileIndex, 23, OutError, true);
}

bool ConfigureR21GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ValidateR19GrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    const FGrassMaterialProfile& R19Profile = GrassProfiles[ProfileIndex];
    const FR21GrassMaterialProfile& Profile = GrassR21Profiles[ProfileIndex];
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, GrassDescriptionPrefix + R19Profile.Id + TEXT("_R19"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR19RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR19SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR19NormalAlphaDescription);
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR19SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR19SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR19CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR19NormalDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionCustom* Wind = FindCustomByDescription(
        Material, GrassWindDescription);
    const FString WpoFingerprintBefore = BuildR21WpoFingerprint(Wind);
    if (!Data || !Color || !Roughness || !Specular || !NormalAlpha ||
        !Subsurface || !SubsurfaceMultiply || !Camera || !Normal ||
        !BaseRoughness || !BaseSpecular || !Wind ||
        WpoFingerprintBefore.IsEmpty())
    {
        OutError = TEXT("The exact clean R19 predecessor does not expose every existing graph node required by the zero-new-node R21 calibration.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    Color->Modify(); Roughness->Modify(); Specular->Modify();
    NormalAlpha->Modify(); Subsurface->Modify();
    SubsurfaceMultiply->Modify(); Camera->Modify(); Normal->Modify();
    BaseRoughness->Modify(); BaseSpecular->Modify();

    Color->Description = GrassDescriptionPrefix + Profile.Id + TEXT("_R21");
    Color->Code = BuildR21GrassColorCode(Profile);
    ResetCustomAuxiliaryState(Color);
    Roughness->Description = GrassR21RoughnessDescription;
    Roughness->Code = GrassR21RoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    Specular->Description = GrassR21SpecularDescription;
    Specular->Code = GrassR21SpecularCode;
    ResetCustomAuxiliaryState(Specular);
    NormalAlpha->Description = GrassR21NormalAlphaDescription;
    NormalAlpha->Code = GrassR21NormalAlphaCode;
    ResetCustomAuxiliaryState(NormalAlpha);
    Subsurface->Desc = GrassR21SubsurfaceGainDescription;
    Subsurface->Constant = Profile.SubsurfaceGain;
    SubsurfaceMultiply->Desc = GrassR21SubsurfaceDescription;
    Camera->Desc = GrassR21CameraDescription;
    Normal->Desc = GrassR21NormalDescription;
    BaseRoughness->DefaultValue = Profile.Roughness;
    BaseSpecular->DefaultValue = Profile.Specular;

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (BuildR21WpoFingerprint(Wind) != WpoFingerprintBefore)
    {
        OutError = TEXT("The R21 in-place calibration changed the byte-stable calm-wind WPO custom expression or its pin fingerprint.");
        return false;
    }
    return ValidateR21GrassMaterial(Material, ProfileIndex, OutError);
}

bool ConfigureR22GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ValidateR21GrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5B_BERMUDA_WORLD_POSITION"));
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR21CameraDescription);
    UMaterialExpressionScalarParameter* WindStrength =
        FindScalar(Material, TEXT("TRIAD_WindStrengthCm"));
    UMaterialExpression* InstanceRandom = nullptr;
    UMaterialExpression* InstanceFade = nullptr;
    if (Data)
    {
        for (UMaterialExpression* Expression :
             Data->ExpressionCollection.Expressions)
        {
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceRandom") &&
                Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_RANDOM"))
            {
                InstanceRandom = Expression;
            }
            if (Expression && Expression->GetClass()->GetPathName() ==
                    TEXT("/Script/Engine.MaterialExpressionPerInstanceFadeAmount") &&
                Expression->Desc == TEXT("V5B_BERMUDA_PER_INSTANCE_FADE"))
            {
                InstanceFade = Expression;
            }
        }
    }
    if (!Data || !WorldPosition || !Camera || !WindStrength ||
        !InstanceRandom || !InstanceFade)
    {
        OutError = TEXT("The exact R21 grass predecessor does not expose every node required by the R22 stable-visibility migration.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    WindStrength->Modify();
    UMaterialExpressionCustom* Stable =
        AddExpression<UMaterialExpressionCustom>(
            Material,
            GrassR22StableVisibilityDescription,
            40,
            640);
    UMaterialExpressionScalarParameter* RevisionMarker =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, R22RuntimeRevisionDescription, -180, 760);
    if (!Stable || !RevisionMarker)
    {
        OutError = TEXT("Could not allocate the exact R22 grass stable-visibility and runtime-revision nodes.");
        return false;
    }
    RevisionMarker->ParameterName = R22RuntimeRevisionParameterName;
    RevisionMarker->DefaultValue = R22RuntimeRevisionValue;
    Stable->Description = GrassR22StableVisibilityDescription;
    Stable->Code = GrassR22StableVisibilityCode;
    Stable->OutputType = CMOT_Float1;
    Stable->Inputs.SetNum(5);
    Stable->Inputs[0].InputName = TEXT("InstanceFade");
    Stable->Inputs[0].Input.Connect(0, InstanceFade);
    Stable->Inputs[1].InputName = TEXT("Random01");
    Stable->Inputs[1].Input.Connect(0, InstanceRandom);
    Stable->Inputs[2].InputName = TEXT("WorldPosition");
    Stable->Inputs[2].Input.Connect(0, WorldPosition);
    Stable->Inputs[3].InputName = TEXT("CameraPosition");
    Stable->Inputs[3].Input.Connect(0, Camera);
    Stable->Inputs[4].InputName = TEXT("MaterialRevision");
    Stable->Inputs[4].Input.Connect(0, RevisionMarker);
    ResetCustomAuxiliaryState(Stable);
    WindStrength->DefaultValue = 0.0f;
    Data->OpacityMask.Connect(0, Stable);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR22GrassMaterial(Material, ProfileIndex, OutError);
}

bool ConfigureR23GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ValidateR22GrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    const FR21GrassMaterialProfile& R22Profile =
        GrassR21Profiles[ProfileIndex];
    const FR21GrassMaterialProfile& Profile =
        GrassR23Profiles[ProfileIndex];
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, GrassDescriptionPrefix + R22Profile.Id + TEXT("_R21"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR21RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR21SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR21NormalAlphaDescription);
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR21SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR21SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR21CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR21NormalDescription);
    UMaterialExpressionCustom* Stable = FindCustomByDescription(
        Material, GrassR22StableVisibilityDescription);
    UMaterialExpressionScalarParameter* RevisionMarker = FindScalar(
        Material, R22RuntimeRevisionParameterName);
    UMaterialExpressionCustom* Wind = FindCustomByDescription(
        Material, GrassWindDescription);
    const FString WpoFingerprintBefore = BuildR21WpoFingerprint(Wind);
    if (!Color || !Roughness || !Specular || !NormalAlpha || !Subsurface ||
        !SubsurfaceMultiply || !Camera || !Normal || !Stable ||
        !RevisionMarker || !Wind || WpoFingerprintBefore.IsEmpty())
    {
        OutError = TEXT("The exact R22 grass predecessor does not expose every existing node required by the zero-new-node R23 photographic-depth calibration.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    Color->Modify(); Roughness->Modify(); Specular->Modify();
    NormalAlpha->Modify(); Subsurface->Modify();
    SubsurfaceMultiply->Modify(); Camera->Modify(); Normal->Modify();
    Stable->Modify(); RevisionMarker->Modify();
    Color->Description = GrassDescriptionPrefix + Profile.Id + TEXT("_R23");
    Color->Code = BuildR23GrassColorCode(Profile);
    ResetCustomAuxiliaryState(Color);
    Roughness->Description = GrassR23RoughnessDescription;
    Roughness->Code = GrassR23RoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    Specular->Description = GrassR23SpecularDescription;
    Specular->Code = GrassR23SpecularCode;
    ResetCustomAuxiliaryState(Specular);
    NormalAlpha->Description = GrassR23NormalAlphaDescription;
    NormalAlpha->Code = GrassR23NormalAlphaCode;
    ResetCustomAuxiliaryState(NormalAlpha);
    Subsurface->Desc = GrassR23SubsurfaceGainDescription;
    Subsurface->Constant = Profile.SubsurfaceGain;
    SubsurfaceMultiply->Desc = GrassR23SubsurfaceDescription;
    Camera->Desc = GrassR23CameraDescription;
    Normal->Desc = GrassR23NormalDescription;
    Stable->Description = GrassR23StableVisibilityDescription;
    Stable->Code = GrassR23StableVisibilityCode;
    ResetCustomAuxiliaryState(Stable);
    RevisionMarker->Desc = R23RuntimeRevisionDescription;
    RevisionMarker->DefaultValue = R23RuntimeRevisionValue;

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (BuildR21WpoFingerprint(Wind) != WpoFingerprintBefore)
    {
        OutError = TEXT("The R23 calibration changed the preserved zero-wind WPO custom expression or pin fingerprint.");
        return false;
    }
    return ValidateR23GrassMaterial(Material, ProfileIndex, OutError);
}

bool ConfigureR23BGrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    if (!ValidateR23GrassMaterial(Material, ProfileIndex, OutError))
    {
        return false;
    }
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    const FR21GrassMaterialProfile& R23Profile =
        GrassR23Profiles[ProfileIndex];
    const FR21GrassMaterialProfile& Profile =
        GrassR23BProfiles[ProfileIndex];
    UMaterialExpressionCustom* Color = FindCustomByDescription(
        Material, GrassDescriptionPrefix + R23Profile.Id + TEXT("_R23"));
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GrassR23RoughnessDescription);
    UMaterialExpressionCustom* Specular = FindCustomByDescription(
        Material, GrassR23SpecularDescription);
    UMaterialExpressionCustom* NormalAlpha = FindCustomByDescription(
        Material, GrassR23NormalAlphaDescription);
    UMaterialExpressionConstant3Vector* Subsurface =
        FindExpressionByDescription<UMaterialExpressionConstant3Vector>(
            Material, GrassR23SubsurfaceGainDescription);
    UMaterialExpressionMultiply* SubsurfaceMultiply =
        FindExpressionByDescription<UMaterialExpressionMultiply>(
            Material, GrassR23SubsurfaceDescription);
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, GrassR23CameraDescription);
    UMaterialExpressionNormalize* Normal =
        FindExpressionByDescription<UMaterialExpressionNormalize>(
            Material, GrassR23NormalDescription);
    UMaterialExpressionScalarParameter* BaseRoughness =
        FindScalar(Material, TEXT("Roughness"));
    UMaterialExpressionScalarParameter* BaseSpecular =
        FindScalar(Material, TEXT("Specular"));
    UMaterialExpressionScalarParameter* WindResponse =
        FindScalar(Material, TEXT("TRIAD_WindResponseScale"));
    UMaterialExpressionScalarParameter* RevisionMarker = FindScalar(
        Material, R22RuntimeRevisionParameterName);
    UMaterialExpressionScalarParameter* ExistingCalibrationMarker = FindScalar(
        Material, R23BRuntimeCalibrationRevisionParameterName);
    UMaterialExpressionCustom* Stable = FindCustomByDescription(
        Material, GrassR23StableVisibilityDescription);
    UMaterialExpressionCustom* Wind = FindCustomByDescription(
        Material, GrassWindDescription);
    const FString WpoFingerprintBefore = BuildR21WpoFingerprint(Wind);
    if (!Data || !Color || !Roughness || !Specular || !NormalAlpha ||
        !Subsurface || !SubsurfaceMultiply || !Camera || !Normal ||
        !BaseRoughness || !BaseSpecular || !WindResponse || !RevisionMarker ||
        ExistingCalibrationMarker || !Stable || Stable->Inputs.Num() != 5 ||
        !Wind || WpoFingerprintBefore.IsEmpty())
    {
        OutError = TEXT("The exact R23 grass predecessor does not expose the sealed five-input visibility graph required by the additive R23B calibration marker.");
        return false;
    }

    UMaterialExpression* InstanceFade = Stable->Inputs[0].Input.Expression;
    UMaterialExpression* InstanceRandom = Stable->Inputs[1].Input.Expression;
    UMaterialExpression* WorldPosition = Stable->Inputs[2].Input.Expression;
    UMaterialExpression* CameraPosition = Stable->Inputs[3].Input.Expression;
    Material->Modify();
    Material->PreEditChange(nullptr);
    Color->Modify(); Roughness->Modify(); Specular->Modify();
    NormalAlpha->Modify(); Subsurface->Modify();
    SubsurfaceMultiply->Modify(); Camera->Modify(); Normal->Modify();
    BaseRoughness->Modify(); BaseSpecular->Modify(); WindResponse->Modify();
    Stable->Modify(); RevisionMarker->Modify();
    UMaterialExpressionScalarParameter* CalibrationRevisionMarker =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, R23BRuntimeCalibrationRevisionDescription, -180, 850);
    if (!CalibrationRevisionMarker)
    {
        OutError = TEXT("Could not allocate the additive R23B runtime calibration marker.");
        return false;
    }
    CalibrationRevisionMarker->ParameterName =
        R23BRuntimeCalibrationRevisionParameterName;
    CalibrationRevisionMarker->DefaultValue =
        R23BRuntimeCalibrationRevisionValue;

    Color->Description = GrassDescriptionPrefix + Profile.Id + TEXT("_R23B");
    Color->Code = BuildR23BGrassColorCode(Profile);
    ResetCustomAuxiliaryState(Color);
    Roughness->Description = GrassR23BRoughnessDescription;
    Roughness->Code = GrassR23BRoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    Specular->Description = GrassR23BSpecularDescription;
    Specular->Code = GrassR23BSpecularCode;
    ResetCustomAuxiliaryState(Specular);
    NormalAlpha->Description = GrassR23BNormalAlphaDescription;
    NormalAlpha->Code = GrassR23BNormalAlphaCode;
    ResetCustomAuxiliaryState(NormalAlpha);
    Subsurface->Desc = GrassR23BSubsurfaceGainDescription;
    Subsurface->Constant = Profile.SubsurfaceGain;
    SubsurfaceMultiply->Desc = GrassR23BSubsurfaceDescription;
    Camera->Desc = GrassR23BCameraDescription;
    Normal->Desc = GrassR23BNormalDescription;
    BaseRoughness->DefaultValue = Profile.Roughness;
    BaseSpecular->DefaultValue = Profile.Specular;
    WindResponse->DefaultValue = Profile.WindResponse;
    Stable->Description = GrassR23BStableVisibilityDescription;
    Stable->Code = GrassR23BStableVisibilityCode;
    Stable->OutputType = CMOT_Float1;
    Stable->Inputs.SetNum(6);
    Stable->Inputs[0].InputName = TEXT("InstanceFade");
    Stable->Inputs[0].Input.Connect(0, InstanceFade);
    Stable->Inputs[1].InputName = TEXT("Random01");
    Stable->Inputs[1].Input.Connect(0, InstanceRandom);
    Stable->Inputs[2].InputName = TEXT("WorldPosition");
    Stable->Inputs[2].Input.Connect(0, WorldPosition);
    Stable->Inputs[3].InputName = TEXT("CameraPosition");
    Stable->Inputs[3].Input.Connect(0, CameraPosition);
    Stable->Inputs[4].InputName = TEXT("MaterialRevision");
    Stable->Inputs[4].Input.Connect(0, RevisionMarker);
    Stable->Inputs[5].InputName = TEXT("CalibrationRevision");
    Stable->Inputs[5].Input.Connect(0, CalibrationRevisionMarker);
    ResetCustomAuxiliaryState(Stable);
    Data->OpacityMask.Connect(0, Stable);

    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    if (BuildR21WpoFingerprint(Wind) != WpoFingerprintBefore)
    {
        OutError = TEXT("The R23B calibration changed the preserved zero-wind WPO custom expression or pin fingerprint.");
        return false;
    }
    return ValidateR23BGrassMaterial(Material, ProfileIndex, OutError);
}

bool ConfigureR10GrassMaterial(
    UMaterial* Material,
    int32 ProfileIndex,
    FString& OutError)
{
    return ConfigureGrassMaterialVersion(
        Material,
        ProfileIndex,
        R10GrassProfiles[ProfileIndex],
        TEXT("_V2"),
        BuildR10GrassColorCode(R10GrassProfiles[ProfileIndex]),
        false,
        OutError);
}

bool CreateGrassMaterial(
    UMaterial* Source,
    int32 ProfileIndex,
    UMaterial*& OutMaterial,
    FString& OutError)
{
    OutMaterial = DuplicateMaterialFresh(
        Source,
        GrassAssetNames[ProfileIndex],
        OutError);
    return OutMaterial &&
        ConfigureGrassMaterial(OutMaterial, ProfileIndex, OutError) &&
        ConfigureR15GrassMaterial(OutMaterial, ProfileIndex, OutError);
}

bool CreateSoilMaterial(
    UMaterial* Source,
    UMaterial*& OutMaterial,
    FString& OutError)
{
    OutMaterial = DuplicateMaterialFresh(Source, SoilAssetName, OutError);
    UMaterialExpressionCustom* Base = FindCustomByPrefix(
        OutMaterial,
        TEXT("TRIAD_EXPLORE_V5B_FORMAL_BED_APRON_AND_TREE_MULCH_"));
    UMaterialExpressionCustom* Roughness = FindCustomByPrefix(
        OutMaterial,
        TEXT("TRIAD_EXPLORE_V5B_FORMAL_BED_SOIL_ROUGHNESS_"));
    UMaterialExpressionCustom* Normal = FindCustomByPrefix(
        OutMaterial,
        TEXT("TRIAD_EXPLORE_V5B_FORMAL_BED_MULCH_FINE_MICRO_NORMAL_"));
    if (!OutMaterial || !Base || !Roughness || !Normal)
    {
        OutError = TEXT("The admitted V5B soil graph no longer exposes its exact layered base/roughness/normal nodes.");
        return false;
    }
    Base->Modify();
    Roughness->Modify();
    Normal->Modify();
    Base->Description = SoilBaseDescription;
    Base->Code = SoilBaseCode;
    ResetCustomAuxiliaryState(Base);
    Roughness->Description = SoilRoughnessDescription;
    Roughness->Code = SoilRoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    Normal->Description = SoilNormalDescription;
    Normal->Code = SoilNormalCode;
    ResetCustomAuxiliaryState(Normal);
    OutMaterial->PreEditChange(nullptr);
    OutMaterial->PostEditChange();
    OutMaterial->MarkPackageDirty();
    return ValidateSoilMaterial(OutMaterial, OutError);
}

void ResetGroundOverlayGraph(UMaterial* Material)
{
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Data)
    {
        return;
    }
    Data->BaseColor.Expression = nullptr;
    Data->Metallic.Expression = nullptr;
    Data->Specular.Expression = nullptr;
    Data->Roughness.Expression = nullptr;
    Data->Anisotropy.Expression = nullptr;
    Data->EmissiveColor.Expression = nullptr;
    Data->Opacity.Expression = nullptr;
    Data->OpacityMask.Expression = nullptr;
    Data->Normal.Expression = nullptr;
    Data->Tangent.Expression = nullptr;
    Data->WorldPositionOffset.Expression = nullptr;
    Data->Displacement.Expression = nullptr;
    Data->SubsurfaceColor.Expression = nullptr;
    Data->ClearCoat.Expression = nullptr;
    Data->ClearCoatRoughness.Expression = nullptr;
    Data->AmbientOcclusion.Expression = nullptr;
    Data->Refraction.Expression = nullptr;
    Data->MaterialAttributes.Expression = nullptr;
    Data->PixelDepthOffset.Expression = nullptr;
    Data->ShadingModelFromMaterialExpression.Expression = nullptr;
    Data->SurfaceThickness.Expression = nullptr;
    Data->FrontMaterial.Expression = nullptr;
    for (FVector2MaterialInput& CustomizedUv : Data->CustomizedUVs)
    {
        CustomizedUv.Expression = nullptr;
    }
    for (UMaterialExpression* Expression :
         Data->ExpressionCollection.Expressions)
    {
        if (Expression)
        {
            Expression->Modify();
            Expression->MarkAsGarbage();
        }
    }
    Data->ExpressionCollection.Expressions.Reset();
}

UMaterial* CreateEmptyMaterialFresh(
    const FString& AssetName,
    FString& OutError)
{
    const FString NewPackageName = PackagePath(AssetName);
    if (FPackageName::DoesPackageExist(NewPackageName) ||
        FindPackage(nullptr, *NewPackageName))
    {
        OutError = TEXT("The dedicated V5D material destination is not fresh: ") +
            NewPackageName;
        return nullptr;
    }
    UPackage* Package = CreatePackage(*NewPackageName);
    UMaterial* Material = Package
        ? NewObject<UMaterial>(
              Package,
              FName(*AssetName),
              RF_Public | RF_Standalone | RF_Transactional)
        : nullptr;
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data ||
        !Data->ExpressionCollection.Expressions.IsEmpty() ||
        Material->GetPathName() != ObjectPath(AssetName))
    {
        OutError = TEXT("Could not create an exact empty V5D material: ") +
            AssetName;
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Material);
    return Material;
}

UMaterialExpressionTextureSampleParameter2D* AddGroundTextureSample(
    UMaterial* Material,
    const FString& Description,
    FName ParameterName,
    UTexture2D* Texture,
    EMaterialSamplerType SamplerType,
    int32 EditorY)
{
    UMaterialExpressionTextureSampleParameter2D* Sample =
        AddExpression<UMaterialExpressionTextureSampleParameter2D>(
            Material, Description, -980, EditorY);
    if (Sample)
    {
        Sample->ParameterName = ParameterName;
        Sample->Texture = Texture;
        Sample->SamplerType = SamplerType;
        Sample->SamplerSource = SSM_FromTextureAsset;
        Sample->MipValueMode = TMVM_None;
        Sample->AutomaticViewMipBias = true;
        Sample->TextureObject.Expression = nullptr;
        Sample->MipValue.Expression = nullptr;
        Sample->CoordinatesDX.Expression = nullptr;
        Sample->CoordinatesDY.Expression = nullptr;
        Sample->AutomaticViewMipBiasValue.Expression = nullptr;
    }
    return Sample;
}

void ConfigureGroundCustom(
    UMaterialExpressionCustom* Custom,
    const FString& Description,
    const FString& Code,
    ECustomMaterialOutputType OutputType,
    const TArray<FName>& InputNames)
{
    if (!Custom)
    {
        return;
    }
    Custom->Description = Description;
    Custom->Code = Code;
    Custom->OutputType = OutputType;
    ResetCustomAuxiliaryState(Custom);
    Custom->Inputs.SetNum(InputNames.Num());
    for (int32 Index = 0; Index < InputNames.Num(); ++Index)
    {
        Custom->Inputs[Index].InputName = InputNames[Index];
    }
}

enum class EGroundOverlayRevision : uint8
{
    R10,
    R12,
    R13,
    R15,
    R16
};

bool BuildGroundOverlayMaterialGraphVersion(
    UMaterial* Material,
    bool bResetExistingGraph,
    EGroundOverlayRevision Revision,
    FString& OutError)
{
    const bool bR16 = Revision == EGroundOverlayRevision::R16;
    const bool bR15 = Revision == EGroundOverlayRevision::R15;
    const bool bR15Appearance = bR15 || bR16;
    const bool bR13 = Revision == EGroundOverlayRevision::R13;
    const bool bR12 = Revision == EGroundOverlayRevision::R12;
    const FString& BaseDescription = bR15Appearance
        ? GroundR15BaseDescription
        : (bR13
            ? GroundR13BaseDescription
            : (bR12 ? GroundR12BaseDescription : GroundR10BaseDescription));
    const FString& BaseCode = bR15Appearance
        ? GroundR15BaseCode
        : (bR13
            ? GroundR13BaseCode
            : (bR12 ? GroundR12BaseCode : GroundR10BaseCode));
    const FString& RoughnessDescription = bR15Appearance
        ? GroundR15RoughnessDescription
        : (bR13 ? GroundR13RoughnessDescription
        : (bR12
            ? GroundR12RoughnessDescription
            : GroundR10RoughnessDescription));
    const FString& RoughnessCode = bR15Appearance
        ? GroundR15RoughnessCode
        : (bR13
            ? GroundR13RoughnessCode
            : (bR12 ? GroundR12RoughnessCode : GroundR10RoughnessCode));
    const float NearNormalStrength = bR15Appearance
        ? GroundR15NearNormalStrength
        : (bR13
            ? GroundR13NearNormalStrength
            : (bR12 ? GroundR12NearNormalStrength : GroundR10NearNormalStrength));
    const float FarNormalStrength = bR15Appearance
        ? GroundR15FarNormalStrength
        : (bR13
            ? GroundR13FarNormalStrength
            : (bR12 ? GroundR12FarNormalStrength : GroundR10FarNormalStrength));
    const float SpecularValue = bR15Appearance
        ? GroundR15Specular
        : (bR13
            ? GroundR13Specular
            : (bR12 ? GroundR12Specular : GroundR10Specular));
    const FString& BaseTexturePath = bR15Appearance || bR13
        ? GroundBaseColorTexturePath
        : GroundR12BaseColorTexturePath;
    const FString& NormalTexturePath = bR15Appearance || bR13
        ? GroundNormalTexturePath
        : GroundR12NormalTexturePath;
    const FString& RoughnessTexturePath = bR15Appearance || bR13
        ? GroundRoughnessTexturePath
        : GroundR12RoughnessTexturePath;
    const FString& AoTexturePath = bR15Appearance || bR13
        ? GroundAoTexturePath
        : GroundR12AoTexturePath;
    UTexture2D* BaseTexture = LoadExact<UTexture2D>(BaseTexturePath);
    UTexture2D* NormalTexture = LoadExact<UTexture2D>(NormalTexturePath);
    UTexture2D* RoughnessTexture =
        LoadExact<UTexture2D>(RoughnessTexturePath);
    UTexture2D* AoTexture = LoadExact<UTexture2D>(AoTexturePath);
    UMaterialEditorOnlyData* Data = Material
        ? Material->GetEditorOnlyData()
        : nullptr;
    if (!Material || !Data || !BaseTexture || !NormalTexture ||
        !RoughnessTexture || !AoTexture)
    {
        OutError = TEXT("The dedicated V5D overlay requires the exact four admitted revision-specific lawn textures.");
        return false;
    }
    Material->Modify();
    Material->PreEditChange(nullptr);
    if (bResetExistingGraph)
    {
        ResetGroundOverlayGraph(Material);
    }
    if (!Data->ExpressionCollection.Expressions.IsEmpty())
    {
        OutError = TEXT("The dedicated V5D overlay builder requires an empty expression collection.");
        return false;
    }

    UMaterialExpressionScalarParameter* TileMeters =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, TEXT("V5D.R10.TextureTileMeters"), -1420, -400);
    UMaterialExpressionCustom* SurfaceUv =
        AddExpression<UMaterialExpressionCustom>(
            Material, TEXT("V5D.R10.SurfaceUV"), -1190, -500);
    UMaterialExpressionTextureSampleParameter2D* BaseSample =
        AddGroundTextureSample(
            Material, TEXT("V5D.R10.BaseColorTexture"),
            TEXT("V5D_R10_BaseColorTexture"), BaseTexture,
            SAMPLERTYPE_Color, -620);
    UMaterialExpressionTextureSampleParameter2D* NormalSample =
        AddGroundTextureSample(
            Material, TEXT("V5D.R10.NormalTexture"),
            TEXT("V5D_R10_NormalTexture"), NormalTexture,
            SAMPLERTYPE_Normal, -420);
    UMaterialExpressionTextureSampleParameter2D* RoughnessSample =
        AddGroundTextureSample(
            Material, TEXT("V5D.R10.RoughnessTexture"),
            TEXT("V5D_R10_RoughnessTexture"), RoughnessTexture,
            SAMPLERTYPE_Masks, -220);
    UMaterialExpressionTextureSampleParameter2D* AoSample =
        AddGroundTextureSample(
            Material, TEXT("V5D.R10.AoTexture"),
            TEXT("V5D_R10_AoTexture"), AoTexture,
            SAMPLERTYPE_Masks, -20);
    UMaterialExpressionWorldPosition* WorldPosition =
        AddExpression<UMaterialExpressionWorldPosition>(
            Material, TEXT("V5D.R10.WorldPositionNoOffsets"), -1420, 200);
    UMaterialExpressionCameraPositionWS* CameraPosition =
        AddExpression<UMaterialExpressionCameraPositionWS>(
            Material, TEXT("V5D.R10.CameraPosition"), -1420, 320);
    UMaterialExpressionCustom* Field = AddExpression<UMaterialExpressionCustom>(
        Material, TEXT("V5D.R10.CoherentField"), -980, 200);
    UMaterialExpressionComponentMask* Macro =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.Macro11m48m"), -700, 170);
    UMaterialExpressionComponentMask* Mowing =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.Mowing3p2m7deg"), -700, 260);
    UMaterialExpressionComponentMask* Wear =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.BoundedWear"), -700, 350);
    UMaterialExpressionComponentMask* Wetness =
        AddExpression<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.BoundedWetness"), -700, 440);
    UMaterialExpressionCustom* NearDetail =
        AddExpression<UMaterialExpressionCustom>(
            Material, TEXT("V5D.R10.NearDetail30m55m"), -980, 560);
    UMaterialExpressionCustom* Base = AddExpression<UMaterialExpressionCustom>(
        Material, TEXT("V5D.R10.Base"), -300, -540);
    UMaterialExpressionCustom* Roughness =
        AddExpression<UMaterialExpressionCustom>(
            Material, TEXT("V5D.R10.Roughness"), -300, -200);
    UMaterialExpressionCustom* Ao = AddExpression<UMaterialExpressionCustom>(
        Material, TEXT("V5D.R10.AmbientOcclusion"), -300, 20);
    UMaterialExpressionConstant3Vector* FlatNormal =
        AddExpression<UMaterialExpressionConstant3Vector>(
            Material, TEXT("V5D.R10.FlatNormal"), -300, 250);
    UMaterialExpressionLinearInterpolate* NormalStrength =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormalStrength"), -80, 320);
    UMaterialExpressionLinearInterpolate* NormalBlend =
        AddExpression<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormal"), 150, 230);
    UMaterialExpressionNormalize* FinalNormal =
        AddExpression<UMaterialExpressionNormalize>(
            Material, TEXT("V5D.R10.FinalNormal"), 370, 230);
    UMaterialExpressionScalarParameter* Specular =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, TEXT("V5D.R10.Specular"), 120, -80);
    UMaterialExpressionCustom* CoreMask =
        AddExpression<UMaterialExpressionCustom>(
            Material,
            bR16 ? GroundCoreMaskDescription : GroundR15CoreMaskDescription,
            -300,
            700);
    if (!TileMeters || !SurfaceUv || !BaseSample || !NormalSample ||
        !RoughnessSample || !AoSample || !WorldPosition || !CameraPosition ||
        !Field || !Macro || !Mowing || !Wear || !Wetness || !NearDetail ||
        !Base || !Roughness || !Ao || !FlatNormal || !NormalStrength ||
        !NormalBlend || !FinalNormal || !Specular || !CoreMask)
    {
        OutError = FString::Printf(
            TEXT("Could not allocate the exact 23-node V5D %s lawn overlay graph."),
            bR16
                ? TEXT("R16")
                : (bR15
                    ? TEXT("R15")
                : (bR13
                    ? TEXT("R13")
                    : (bR12 ? TEXT("R12") : TEXT("R10")))));
        return false;
    }

    TileMeters->ParameterName = TEXT("V5D_R10_TextureTileMeters");
    TileMeters->DefaultValue = GroundR10TextureTileMeters;
    ConfigureGroundCustom(
        SurfaceUv, GroundR10SurfaceUvDescription, GroundR10SurfaceUvCode,
        CMOT_Float2, {TEXT("WorldPosition"), TEXT("TileMeters")});
    SurfaceUv->Inputs[0].Input.Connect(0, WorldPosition);
    SurfaceUv->Inputs[1].Input.Connect(0, TileMeters);
    const TArray<UMaterialExpressionTextureSampleParameter2D*> Samples = {
        BaseSample, NormalSample, RoughnessSample, AoSample};
    for (UMaterialExpressionTextureSampleParameter2D* Sample : Samples)
    {
        Sample->Coordinates.Connect(0, SurfaceUv);
    }
    WorldPosition->WorldPositionShaderOffset = WPT_ExcludeAllShaderOffsets;
    ConfigureGroundCustom(
        Field, GroundR10FieldDescription, GroundR10FieldCode, CMOT_Float4,
        {TEXT("WorldPosition")});
    Field->Inputs[0].Input.Connect(0, WorldPosition);
    Macro->Input.Connect(0, Field);
    Macro->R = true;
    Mowing->Input.Connect(0, Field);
    Mowing->G = true;
    Wear->Input.Connect(0, Field);
    Wear->B = true;
    Wetness->Input.Connect(0, Field);
    Wetness->A = true;
    ConfigureGroundCustom(
        NearDetail, GroundR10FarFadeDescription, GroundR10FarFadeCode,
        CMOT_Float1, {TEXT("WorldPosition"), TEXT("CameraPosition")});
    NearDetail->Inputs[0].Input.Connect(0, WorldPosition);
    NearDetail->Inputs[1].Input.Connect(0, CameraPosition);
    ConfigureGroundCustom(
        Base, BaseDescription, BaseCode, CMOT_Float3,
        {TEXT("BaseColor"), TEXT("Macro"), TEXT("Mowing"), TEXT("Wear"),
         TEXT("Wetness"), TEXT("NearDetail")});
    Base->Inputs[0].Input.Connect(0, BaseSample);
    Base->Inputs[1].Input.Connect(0, Macro);
    Base->Inputs[2].Input.Connect(0, Mowing);
    Base->Inputs[3].Input.Connect(0, Wear);
    Base->Inputs[4].Input.Connect(0, Wetness);
    Base->Inputs[5].Input.Connect(0, NearDetail);
    ConfigureGroundCustom(
        Roughness, RoughnessDescription, RoughnessCode,
        CMOT_Float1,
        {TEXT("TextureRoughness"), TEXT("Macro"), TEXT("Mowing"),
         TEXT("Wear"), TEXT("Wetness"), TEXT("NearDetail")});
    Roughness->Inputs[0].Input.Connect(1, RoughnessSample);
    Roughness->Inputs[1].Input.Connect(0, Macro);
    Roughness->Inputs[2].Input.Connect(0, Mowing);
    Roughness->Inputs[3].Input.Connect(0, Wear);
    Roughness->Inputs[4].Input.Connect(0, Wetness);
    Roughness->Inputs[5].Input.Connect(0, NearDetail);
    ConfigureGroundCustom(
        Ao, GroundR10AoDescription, GroundR10AoCode, CMOT_Float1,
        {TEXT("TextureAo"), TEXT("NearDetail")});
    Ao->Inputs[0].Input.Connect(1, AoSample);
    Ao->Inputs[1].Input.Connect(0, NearDetail);
    FlatNormal->Constant = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);
    NormalStrength->ConstA = FarNormalStrength;
    NormalStrength->ConstB = NearNormalStrength;
    NormalStrength->Alpha.Connect(0, NearDetail);
    NormalBlend->A.Connect(0, FlatNormal);
    NormalBlend->B.Connect(0, NormalSample);
    NormalBlend->Alpha.Connect(0, NormalStrength);
    FinalNormal->VectorInput.Connect(0, NormalBlend);
    Specular->ParameterName = TEXT("V5D_R10_Specular");
    Specular->DefaultValue = SpecularValue;
    ConfigureGroundCustom(
        CoreMask,
        bR16 ? GroundCoreMaskDescription : GroundR15CoreMaskDescription,
        bR16 ? GroundCoreMaskCode : GroundR15CoreMaskCode,
        CMOT_Float1, {TEXT("WorldPosition")});
    CoreMask->Inputs[0].Input.Connect(0, WorldPosition);

    Material->MaterialDomain = MD_Surface;
    Material->BlendMode = BLEND_Masked;
    Material->OpacityMaskClipValue = 0.5f;
    Material->SetShadingModel(MSM_DefaultLit);
    Material->TwoSided = false;
    Material->bTangentSpaceNormal = true;
    Material->bUseMaterialAttributes = false;
    Material->bUsedWithInstancedStaticMeshes = false;
    Material->bScreenSpaceReflections = false;
    Material->NaniteOverrideMaterial.bEnableOverride = true;
    Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr;
    Material->bEnableTessellation = false;
    Material->bEnableDisplacementFade = false;
    Material->MaxWorldPositionOffsetDisplacement = 0.0f;
    Data->BaseColor.Connect(0, Base);
    Data->Roughness.Connect(0, Roughness);
    Data->Specular.Connect(0, Specular);
    Data->Normal.Connect(0, FinalNormal);
    Data->AmbientOcclusion.Connect(0, Ao);
    Data->OpacityMask.Connect(0, CoreMask);
    Material->bUsedWithNanite = true;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return bR16
        ? ValidateR16GroundOverlayMaterial(Material, OutError)
        : (bR15
            ? ValidateR15GroundOverlayMaterial(Material, OutError)
        : (bR13
            ? ValidateGroundOverlayMaterial(Material, OutError)
        : (bR12
            ? ValidateR12GroundOverlayMaterial(Material, OutError)
            : ValidateR10GroundOverlayMaterial(Material, OutError))));
}

bool BuildR16GroundOverlayMaterialGraph(
    UMaterial* Material,
    bool bResetExistingGraph,
    FString& OutError)
{
    return BuildGroundOverlayMaterialGraphVersion(
        Material, bResetExistingGraph, EGroundOverlayRevision::R16, OutError);
}

bool ConfigureR16GroundOverlayMaskInPlace(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR15GroundOverlayMaterial(Material, OutError))
    {
        return false;
    }
    UMaterialExpressionCustom* CoreMask =
        FindCustomByDescription(Material, GroundR15CoreMaskDescription);
    if (!CoreMask || CoreMask->Code != GroundR15CoreMaskCode ||
        CoreMask->OutputType != CMOT_Float1 || CoreMask->Inputs.Num() != 1 ||
        CoreMask->Inputs[0].InputName != TEXT("WorldPosition") ||
        !CoreMask->Inputs[0].Input.Expression)
    {
        OutError = TEXT("The admitted R15 overlay no longer exposes its exact single-input V4 core-mask Custom node.");
        return false;
    }
    Material->Modify();
    Material->PreEditChange(nullptr);
    CoreMask->Modify();
    CoreMask->Description = GroundCoreMaskDescription;
    CoreMask->Code = GroundCoreMaskCode;
    ResetCustomAuxiliaryState(CoreMask);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR16GroundOverlayMaterial(Material, OutError);
}

bool ConfigureR17GroundOverlayPhotographicResponseInPlace(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR16GroundOverlayMaterial(Material, OutError))
    {
        return false;
    }
    UMaterialExpressionCustom* Base = FindCustomByDescription(
        Material, GroundR15BaseDescription);
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GroundR15RoughnessDescription);
    UMaterialExpressionLinearInterpolate* NormalStrength =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormalStrength"));
    UMaterialExpressionScalarParameter* Specular = FindScalar(
        Material, TEXT("V5D_R10_Specular"));
    UMaterialExpressionCustom* CoreMask = FindCustomByDescription(
        Material, GroundCoreMaskDescription);
    if (!Base || !Roughness || !NormalStrength || !Specular || !CoreMask ||
        CoreMask->Code != GroundCoreMaskCode)
    {
        OutError = TEXT("The clean R16 overlay no longer exposes the exact R16 seam mask and R15 photographic response nodes required by R17.");
        return false;
    }
    Material->Modify(); Material->PreEditChange(nullptr);
    Base->Modify(); Roughness->Modify(); NormalStrength->Modify(); Specular->Modify();
    Base->Description = GroundR17BaseDescription;
    Base->Code = GroundR17BaseCode;
    Roughness->Description = GroundR17RoughnessDescription;
    Roughness->Code = GroundR17RoughnessCode;
    NormalStrength->ConstA = GroundR17FarNormalStrength;
    NormalStrength->ConstB = GroundR17NearNormalStrength;
    Specular->DefaultValue = GroundR17Specular;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange(); Material->MarkPackageDirty();
    return ValidateR17GroundOverlayMaterial(Material, OutError);
}

bool ConfigureR18GroundOverlayTintInPlace(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR17GroundOverlayMaterial(Material, OutError))
    {
        return false;
    }
    UMaterialExpressionCustom* Base = FindCustomByDescription(
        Material, GroundR17BaseDescription);
    if (!Base || Base->Code != GroundR17BaseCode)
    {
        OutError = TEXT("The clean R17 overlay no longer exposes its exact photographic base node required by the one-node R18 calibration.");
        return false;
    }
    Material->Modify();
    Material->PreEditChange(nullptr);
    Base->Modify();
    Base->Description = GroundR18BaseDescription;
    Base->Code = GroundR18BaseCode;
    ResetCustomAuxiliaryState(Base);
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR18GroundOverlayMaterial(Material, OutError);
}

bool ConfigureR22GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR18GroundOverlayMaterial(Material, OutError))
    {
        return false;
    }
    UMaterialExpressionCustom* Base = FindCustomByDescription(
        Material, GroundR18BaseDescription);
    UMaterialExpressionCustom* NearDetail = FindCustomByDescription(
        Material, GroundR10FarFadeDescription);
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GroundR17RoughnessDescription);
    UMaterialExpressionLinearInterpolate* NormalStrength =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormalStrength"));
    UMaterialExpressionScalarParameter* Specular = FindScalar(
        Material, TEXT("V5D_R10_Specular"));
    UMaterialExpressionWorldPosition* WorldPosition = FindWorldPosition(
        Material, TEXT("V5D.R10.WorldPositionNoOffsets"));
    UMaterialExpressionCameraPositionWS* Camera =
        FindExpressionByDescription<UMaterialExpressionCameraPositionWS>(
            Material, TEXT("V5D.R10.CameraPosition"));
    UMaterialExpressionTextureSampleParameter2D* BaseSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.BaseColorTexture"));
    UMaterialExpressionTextureSampleParameter2D* NormalSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.NormalTexture"));
    UMaterialExpressionTextureSampleParameter2D* RoughnessSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.RoughnessTexture"));
    UMaterialExpressionTextureSampleParameter2D* AoSample =
        FindExpressionByDescription<UMaterialExpressionTextureSampleParameter2D>(
            Material, TEXT("V5D.R10.AoTexture"));
    UMaterialExpressionComponentMask* Macro =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.Macro11m48m"));
    UMaterialExpressionComponentMask* Mowing =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.Mowing3p2m7deg"));
    UMaterialExpressionComponentMask* Wear =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.BoundedWear"));
    UMaterialExpressionComponentMask* Wetness =
        FindExpressionByDescription<UMaterialExpressionComponentMask>(
            Material, TEXT("V5D.R10.BoundedWetness"));
    if (!Base || !NearDetail || !Roughness || !NormalStrength || !Specular ||
        !WorldPosition || !Camera || !BaseSample || !NormalSample ||
        !RoughnessSample || !AoSample || !Macro || !Mowing || !Wear ||
        !Wetness)
    {
        OutError = TEXT("The exact R18 overlay predecessor does not expose every existing response node required by R22.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    Base->Modify(); NearDetail->Modify(); Roughness->Modify();
    NormalStrength->Modify(); Specular->Modify();
    BaseSample->Modify(); NormalSample->Modify();
    RoughnessSample->Modify(); AoSample->Modify();
    UMaterialExpressionCustom* MipBias =
        AddExpression<UMaterialExpressionCustom>(
            Material, GroundR22MipBiasDescription, -720, 650);
    UMaterialExpressionScalarParameter* RevisionMarker =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, R22RuntimeRevisionDescription, -720, 780);
    if (!MipBias || !RevisionMarker)
    {
        OutError = TEXT("Could not allocate the exact R22 lawn distance-mip-bias and runtime-revision nodes.");
        return false;
    }
    RevisionMarker->ParameterName = R22RuntimeRevisionParameterName;
    RevisionMarker->DefaultValue = R22RuntimeRevisionValue;

    ConfigureGroundCustom(
        NearDetail,
        GroundR22FarFadeDescription,
        GroundR22FarFadeCode,
        CMOT_Float1,
        {TEXT("WorldPosition"), TEXT("CameraPosition")});
    NearDetail->Inputs[0].Input.Connect(0, WorldPosition);
    NearDetail->Inputs[1].Input.Connect(0, Camera);
    ConfigureGroundCustom(
        MipBias,
        GroundR22MipBiasDescription,
        GroundR22MipBiasCode,
        CMOT_Float1,
        {TEXT("NearDetail")});
    MipBias->Inputs[0].Input.Connect(0, NearDetail);
    const TArray<UMaterialExpressionTextureSampleParameter2D*> Samples = {
        BaseSample, NormalSample, RoughnessSample, AoSample};
    for (UMaterialExpressionTextureSampleParameter2D* Sample : Samples)
    {
        Sample->MipValueMode = TMVM_MipBias;
        Sample->MipValue.Connect(0, MipBias);
    }
    ConfigureGroundCustom(
        Base,
        GroundR22BaseDescription,
        GroundR22BaseCode,
        CMOT_Float3,
        {TEXT("BaseColor"), TEXT("Macro"), TEXT("Mowing"), TEXT("Wear"),
         TEXT("Wetness"), TEXT("NearDetail"), TEXT("WorldPosition"),
         TEXT("CameraPosition"), TEXT("MaterialRevision")});
    Base->Inputs[0].Input.Connect(0, BaseSample);
    Base->Inputs[1].Input.Connect(0, Macro);
    Base->Inputs[2].Input.Connect(0, Mowing);
    Base->Inputs[3].Input.Connect(0, Wear);
    Base->Inputs[4].Input.Connect(0, Wetness);
    Base->Inputs[5].Input.Connect(0, NearDetail);
    Base->Inputs[6].Input.Connect(0, WorldPosition);
    Base->Inputs[7].Input.Connect(0, Camera);
    Base->Inputs[8].Input.Connect(0, RevisionMarker);
    ConfigureGroundCustom(
        Roughness,
        GroundR22RoughnessDescription,
        GroundR22RoughnessCode,
        CMOT_Float1,
        {TEXT("TextureRoughness"), TEXT("Macro"), TEXT("Mowing"),
         TEXT("Wear"), TEXT("Wetness"), TEXT("NearDetail")});
    Roughness->Inputs[0].Input.Connect(1, RoughnessSample);
    Roughness->Inputs[1].Input.Connect(0, Macro);
    Roughness->Inputs[2].Input.Connect(0, Mowing);
    Roughness->Inputs[3].Input.Connect(0, Wear);
    Roughness->Inputs[4].Input.Connect(0, Wetness);
    Roughness->Inputs[5].Input.Connect(0, NearDetail);
    NormalStrength->ConstA = GroundR22FarNormalStrength;
    NormalStrength->ConstB = GroundR22NearNormalStrength;
    Specular->DefaultValue = GroundR22Specular;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR22GroundOverlayMaterial(Material, OutError);
}

bool ConfigureR23GroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR22GroundOverlayMaterial(Material, OutError))
    {
        return false;
    }
    UMaterialExpressionCustom* Base = FindCustomByDescription(
        Material, GroundR22BaseDescription);
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GroundR22RoughnessDescription);
    UMaterialExpressionLinearInterpolate* NormalStrength =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormalStrength"));
    UMaterialExpressionScalarParameter* Specular = FindScalar(
        Material, TEXT("V5D_R10_Specular"));
    UMaterialExpressionScalarParameter* RevisionMarker = FindScalar(
        Material, R22RuntimeRevisionParameterName);
    if (!Base || !Roughness || !NormalStrength || !Specular ||
        !RevisionMarker)
    {
        OutError = TEXT("The exact R22 overlay predecessor does not expose every existing response node required by R23.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    Base->Modify(); Roughness->Modify(); NormalStrength->Modify();
    Specular->Modify(); RevisionMarker->Modify();
    Base->Description = GroundR23BaseDescription;
    Base->Code = GroundR23BaseCode;
    ResetCustomAuxiliaryState(Base);
    Roughness->Description = GroundR23RoughnessDescription;
    Roughness->Code = GroundR23RoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    NormalStrength->ConstA = GroundR23FarNormalStrength;
    NormalStrength->ConstB = GroundR23NearNormalStrength;
    Specular->DefaultValue = GroundR23Specular;
    RevisionMarker->Desc = R23RuntimeRevisionDescription;
    RevisionMarker->DefaultValue = R23RuntimeRevisionValue;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR23GroundOverlayMaterial(Material, OutError);
}

bool ConfigureR23BGroundOverlayMaterial(
    UMaterial* Material,
    FString& OutError)
{
    if (!ValidateR23GroundOverlayMaterial(Material, OutError))
    {
        return false;
    }
    UMaterialExpressionCustom* Base = FindCustomByDescription(
        Material, GroundR23BaseDescription);
    UMaterialExpressionCustom* Roughness = FindCustomByDescription(
        Material, GroundR23RoughnessDescription);
    UMaterialExpressionLinearInterpolate* NormalStrength =
        FindExpressionByDescription<UMaterialExpressionLinearInterpolate>(
            Material, TEXT("V5D.R10.DistanceFadedNormalStrength"));
    UMaterialExpressionScalarParameter* Specular = FindScalar(
        Material, TEXT("V5D_R10_Specular"));
    UMaterialExpressionScalarParameter* RevisionMarker = FindScalar(
        Material, R22RuntimeRevisionParameterName);
    UMaterialExpressionScalarParameter* ExistingCalibrationMarker = FindScalar(
        Material, R23BRuntimeCalibrationRevisionParameterName);
    if (!Base || Base->Inputs.Num() != 9 || !Roughness ||
        !NormalStrength || !Specular || !RevisionMarker ||
        ExistingCalibrationMarker)
    {
        OutError = TEXT("The exact R23 overlay predecessor does not expose the sealed nine-input base graph required by the additive R23B calibration marker.");
        return false;
    }

    Material->Modify();
    Material->PreEditChange(nullptr);
    Base->Modify(); Roughness->Modify(); NormalStrength->Modify();
    Specular->Modify(); RevisionMarker->Modify();
    UMaterialExpressionScalarParameter* CalibrationRevisionMarker =
        AddExpression<UMaterialExpressionScalarParameter>(
            Material, R23BRuntimeCalibrationRevisionDescription, -720, 900);
    if (!CalibrationRevisionMarker)
    {
        OutError = TEXT("Could not allocate the additive R23B overlay runtime calibration marker.");
        return false;
    }
    CalibrationRevisionMarker->ParameterName =
        R23BRuntimeCalibrationRevisionParameterName;
    CalibrationRevisionMarker->DefaultValue =
        R23BRuntimeCalibrationRevisionValue;
    ConfigureGroundCustom(
        Base,
        GroundR23BBaseDescription,
        GroundR23BBaseCode,
        CMOT_Float3,
        {TEXT("BaseColor"), TEXT("Macro"), TEXT("Mowing"), TEXT("Wear"),
         TEXT("Wetness"), TEXT("NearDetail"), TEXT("WorldPosition"),
         TEXT("CameraPosition"), TEXT("MaterialRevision"),
         TEXT("CalibrationRevision")});
    Base->Inputs[9].Input.Connect(0, CalibrationRevisionMarker);
    Roughness->Description = GroundR23BRoughnessDescription;
    Roughness->Code = GroundR23BRoughnessCode;
    ResetCustomAuxiliaryState(Roughness);
    NormalStrength->ConstA = GroundR23BFarNormalStrength;
    NormalStrength->ConstB = GroundR23BNearNormalStrength;
    Specular->DefaultValue = GroundR23BSpecular;
    UMaterialEditingLibrary::RecompileMaterial(Material);
    Material->PostEditChange();
    Material->MarkPackageDirty();
    return ValidateR23BGroundOverlayMaterial(Material, OutError);
}

bool BuildR15GroundOverlayMaterialGraph(
    UMaterial* Material,
    bool bResetExistingGraph,
    FString& OutError)
{
    return BuildGroundOverlayMaterialGraphVersion(
        Material, bResetExistingGraph, EGroundOverlayRevision::R15, OutError);
}

bool BuildGroundOverlayMaterialGraph(
    UMaterial* Material,
    bool bResetExistingGraph,
    FString& OutError)
{
    return BuildGroundOverlayMaterialGraphVersion(
        Material, bResetExistingGraph, EGroundOverlayRevision::R13, OutError);
}

bool BuildR12GroundOverlayMaterialGraph(
    UMaterial* Material,
    bool bResetExistingGraph,
    FString& OutError)
{
    return BuildGroundOverlayMaterialGraphVersion(
        Material, bResetExistingGraph, EGroundOverlayRevision::R12, OutError);
}

bool BuildR10GroundOverlayMaterialGraph(
    UMaterial* Material,
    bool bResetExistingGraph,
    FString& OutError)
{
    return BuildGroundOverlayMaterialGraphVersion(
        Material, bResetExistingGraph, EGroundOverlayRevision::R10, OutError);
}

bool CreateGroundOverlayMaterial(
    UMaterial*& OutMaterial,
    FString& OutError)
{
    OutMaterial = CreateEmptyMaterialFresh(GroundOverlayAssetName, OutError);
    return OutMaterial &&
        BuildR16GroundOverlayMaterialGraph(OutMaterial, false, OutError);
}

bool EnsureMaterials(TArray<UMaterial*>& OutMaterials, FString& OutError)
{
    int32 ExistingCount = 0;
    for (const FString& Name : GrassAssetNames)
    {
        if (FPackageName::DoesPackageExist(PackagePath(Name)) ||
            FindPackage(nullptr, *PackagePath(Name)))
        {
            ++ExistingCount;
        }
    }
    if (FPackageName::DoesPackageExist(PackagePath(SoilAssetName)) ||
        FindPackage(nullptr, *PackagePath(SoilAssetName)))
    {
        ++ExistingCount;
    }
    if (FPackageName::DoesPackageExist(PackagePath(GroundOverlayAssetName)) ||
        FindPackage(nullptr, *PackagePath(GroundOverlayAssetName)))
    {
        ++ExistingCount;
    }
    if (ExistingCount == UE_ARRAY_COUNT(GrassAssetNames) + 2)
    {
        // Fresh creation remains the explicit historical R16 core stage. The
        // caller creates R11 separately and then performs the atomic R17 pass.
        return ValidateR16MaterialAssetsInternal(OutMaterials, OutError);
    }
    if (ExistingCount != 0)
    {
        OutError = FString::Printf(
            TEXT("The isolated V5D ground/vegetation material namespace is partial (%d/6 packages); overwrite or repair is refused."),
            ExistingCount);
        return false;
    }

    UMaterial* SourceGrass = LoadExact<UMaterial>(SourceGrassMaterialPath);
    UMaterial* SourceSoil = LoadExact<UMaterial>(SourceSoilMaterialPath);
    if (!SourceGrass || !SourceSoil)
    {
        OutError = TEXT("Run the V5B asset build first; its exact modeled fine-turf proxy and soil materials are required as lawful visual substrates.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        UMaterial* Material = nullptr;
        if (!CreateGrassMaterial(
                SourceGrass,
                Index,
                Material,
                OutError))
        {
            return false;
        }
        FreshAssets.Add(Material);
    }
    UMaterial* Soil = nullptr;
    if (!CreateSoilMaterial(SourceSoil, Soil, OutError))
    {
        return false;
    }
    FreshAssets.Add(Soil);
    UMaterial* GroundOverlay = nullptr;
    if (!CreateGroundOverlayMaterial(
            GroundOverlay,
            OutError))
    {
        return false;
    }
    FreshAssets.Add(GroundOverlay);
    FAssetCompilingManager::Get().FinishAllCompilation();

    UEditorAssetSubsystem* Assets = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!Assets || FreshAssets.Num() != 6 ||
        !Assets->SaveLoadedAssets(FreshAssets, false) ||
        !ValidateR16MaterialAssetsInternal(OutMaterials, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Only the six isolated historical R16 V5D materials were offered to save, but exact cold validation failed.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

int32 ExistingGroundMaterialPackageCountIncludingR11()
{
    int32 Count = 0;
    for (const FString& Name : GrassAssetNames)
    {
        Count += (FPackageName::DoesPackageExist(PackagePath(Name)) ||
            FindPackage(nullptr, *PackagePath(Name))) ? 1 : 0;
    }
    const FString RemainingNames[] = {
        SoilAssetName,
        GroundOverlayAssetName,
        EdgeGrassFadeAssetName};
    for (const FString& Name : RemainingNames)
    {
        Count += (FPackageName::DoesPackageExist(PackagePath(Name)) ||
            FindPackage(nullptr, *PackagePath(Name))) ? 1 : 0;
    }
    return Count;
}

bool ValidateCompleteR11MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateMaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) ||
        !Source || !Source->GetOutermost() ||
        Source->GetOutermost()->IsDirty())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The complete R11 roster requires the exact V4 edge source package clean.");
        }
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete seven-package roster requires the four R19 grass, unchanged soil and current R18 lawn-overlay packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The complete R11 roster requires the exact derivative package clean.");
        }
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR15MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR15MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) ||
        !Source || !Source->GetOutermost() ||
        Source->GetOutermost()->IsDirty())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The complete historical R15/R11 roster requires the exact V4 edge source package clean.");
        }
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete historical R15/R11 roster requires all six R15 core material packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR16MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR16MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) || !Source ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() || Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete historical R16/R11 roster requires all six R16 core packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset(); OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR17MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR17MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) || !Source ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() || Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete R17/R11 roster requires all six R17 core packages clean.");
            OutCoreMaterials.Reset(); return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset(); OutEdgeFadeMaterial = nullptr; return false;
    }
    OutError.Reset(); return true;
}

bool ValidateCompleteR18MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR18MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) || !Source ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete historical R18/R11 roster requires four R17 grass, soil and R18 overlay packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR19MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    return ValidateCompleteR11MaterialAssets(
        OutCoreMaterials, OutEdgeFadeMaterial, OutError);
}

bool ValidateCompleteR21MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR21MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) || !Source ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete R21/R18/R11 roster requires four calibrated R21 grass, unchanged soil and R18 lawn-overlay packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR22MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR22MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) || !Source ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete R22 roster requires four stable grass, unchanged soil, and calibrated lawn-overlay packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateR22EdgeGrassFadeMaterial(
            OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR23MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR23MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) || !Source ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete R23 roster requires four photographic-depth grass, unchanged soil, and natural multi-scale lawn-overlay packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateR23EdgeGrassFadeMaterial(
            OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR23BMaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR23BMaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) || !Source ||
        !Source->GetOutermost() || Source->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete R23B roster requires four calibrated photographic grass, unchanged soil, and calibrated lawn-overlay packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateR23BEdgeGrassFadeMaterial(
            OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

enum class ECompleteGrassMaterialRevision : uint8
{
    Invalid,
    R19,
    R21,
    R22,
    R23,
    R23B
};

ECompleteGrassMaterialRevision SelectCompleteGrassMaterialRevision(
    bool bR21Valid,
    bool bR19Valid)
{
    if (bR21Valid == bR19Valid)
    {
        return ECompleteGrassMaterialRevision::Invalid;
    }
    return bR21Valid
        ? ECompleteGrassMaterialRevision::R21
        : ECompleteGrassMaterialRevision::R19;
}

bool ValidateCompleteCurrentMaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutGrassRevision,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    OutGrassRevision.Reset();

    FString R23BError;
    const bool bR23BValid = ValidateCompleteR23BMaterialAssets(
        OutCoreMaterials,
        OutEdgeFadeMaterial,
        R23BError);
    if (bR23BValid)
    {
        OutGrassRevision = TEXT("R23B");
        OutError.Reset();
        return true;
    }

    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    FString R23Error;
    const bool bR23Valid = ValidateCompleteR23MaterialAssets(
        OutCoreMaterials,
        OutEdgeFadeMaterial,
        R23Error);
    if (bR23Valid)
    {
        OutGrassRevision = TEXT("R23");
        OutError.Reset();
        return true;
    }

    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    FString R22Error;
    const bool bR22Valid = ValidateCompleteR22MaterialAssets(
        OutCoreMaterials,
        OutEdgeFadeMaterial,
        R22Error);
    if (bR22Valid)
    {
        OutGrassRevision = TEXT("R22");
        OutError.Reset();
        return true;
    }

    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    FString R21Error;
    const bool bR21Valid = ValidateCompleteR21MaterialAssets(
        OutCoreMaterials,
        OutEdgeFadeMaterial,
        R21Error);
    if (SelectCompleteGrassMaterialRevision(bR21Valid, false) ==
        ECompleteGrassMaterialRevision::R21)
    {
        OutGrassRevision = TEXT("R21");
        OutError.Reset();
        return true;
    }

    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    FString R19Error;
    const bool bR19Valid = ValidateCompleteR19MaterialAssets(
        OutCoreMaterials,
        OutEdgeFadeMaterial,
        R19Error);
    if (SelectCompleteGrassMaterialRevision(false, bR19Valid) ==
        ECompleteGrassMaterialRevision::R19)
    {
        OutGrassRevision = TEXT("R19");
        OutError.Reset();
        return true;
    }

    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    OutError = TEXT("The complete seven-package material roster is neither uniformly R23B, R23, R22, calibrated R21, nor backward-compatible R19. R23B={") +
        R23BError + TEXT("} R23={") + R23Error + TEXT("} R22={") + R22Error + TEXT("} R21={") + R21Error + TEXT("} R19={") +
        R19Error + TEXT("}");
    return false;
}

bool ValidateCompleteR13MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    if (!ValidateR13MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, OutError) ||
        !Source || !Source->GetOutermost() ||
        Source->GetOutermost()->IsDirty())
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The complete historical R13/R11 roster requires the exact V4 edge source package clean.");
        }
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The complete historical R13/R11 roster requires all six R13 core material packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateCompleteR12MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    UMaterial*& OutEdgeFadeMaterial,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutEdgeFadeMaterial = nullptr;
    if (!ValidateR12MaterialAssetsInternal(OutCoreMaterials, OutError) ||
        OutCoreMaterials.Num() != 6)
    {
        OutCoreMaterials.Reset();
        return false;
    }
    for (UMaterial* Core : OutCoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The R12 cold roster requires all six R12 core packages clean.");
            OutCoreMaterials.Reset();
            return false;
        }
    }
    OutEdgeFadeMaterial = LoadExact<UMaterial>(
        ObjectPath(EdgeGrassFadeAssetName));
    if (!ValidateEdgeGrassFadeMaterial(OutEdgeFadeMaterial, OutError) ||
        !OutEdgeFadeMaterial || !OutEdgeFadeMaterial->GetOutermost() ||
        OutEdgeFadeMaterial->GetOutermost()->IsDirty())
    {
        OutCoreMaterials.Reset();
        OutEdgeFadeMaterial = nullptr;
        return false;
    }
    OutError.Reset();
    return true;
}

bool EnsureCompleteR11MaterialAssets(
    TArray<UMaterial*>& OutCoreMaterials,
    FString& OutGrassRevision,
    FString& OutError)
{
    OutCoreMaterials.Reset();
    OutGrassRevision.Reset();
    const int32 ExistingCount =
        ExistingGroundMaterialPackageCountIncludingR11();
    const FString EdgePackage = PackagePath(EdgeGrassFadeAssetName);
    const FString EdgeObject = ObjectPath(EdgeGrassFadeAssetName);
    const bool bEdgeHasAnyState =
        FPackageName::DoesPackageExist(EdgePackage) ||
        FindPackage(nullptr, *EdgePackage) ||
        FindObject<UMaterial>(nullptr, *EdgeObject) ||
        IAssetRegistry::GetChecked().GetAssetByObjectPath(
            FSoftObjectPath(EdgeObject)).IsValid();
    if (ExistingCount == 7)
    {
        UMaterial* EdgeFade = nullptr;
        if (ValidateCompleteCurrentMaterialAssets(
                OutCoreMaterials,
                EdgeFade,
                OutGrassRevision,
                OutError))
        {
            return true;
        }
        TArray<UMaterial*> HistoricalR18;
        UMaterial* HistoricalR18Edge = nullptr;
        FString HistoricalR18Error;
        if (ValidateCompleteR18MaterialAssets(
                HistoricalR18, HistoricalR18Edge, HistoricalR18Error))
        {
            OutCoreMaterials.Reset();
            OutError = TEXT("The seven-package V5D namespace is uniformly clean R18/R11 historical state. Run UpgradeGroundVegetationRealismGrassMaterialsToR19 before current Ensure validation.");
            return false;
        }
        TArray<UMaterial*> HistoricalR17;
        UMaterial* HistoricalR17Edge = nullptr;
        FString HistoricalR17Error;
        if (ValidateCompleteR17MaterialAssets(
                HistoricalR17, HistoricalR17Edge, HistoricalR17Error))
        {
            OutCoreMaterials.Reset();
            OutError = TEXT("The seven-package V5D namespace is uniformly clean R17/R11 historical state. Run UpgradeGroundVegetationRealismLawnOverlayToR18 and then UpgradeGroundVegetationRealismGrassMaterialsToR19 before current Ensure validation.");
            return false;
        }
        TArray<UMaterial*> HistoricalR16;
        UMaterial* HistoricalR16Edge = nullptr;
        FString HistoricalR16Error;
        if (ValidateCompleteR16MaterialAssets(
                HistoricalR16, HistoricalR16Edge, HistoricalR16Error))
        {
            OutCoreMaterials.Reset();
            OutError = TEXT("The seven-package V5D namespace is uniformly clean R16/R11 historical state. Run UpgradeGroundVegetationRealismMaterialAssetsToR17, UpgradeGroundVegetationRealismLawnOverlayToR18, and then UpgradeGroundVegetationRealismGrassMaterialsToR19 before current Ensure validation.");
            return false;
        }
        TArray<UMaterial*> HistoricalR15;
        UMaterial* HistoricalR15Edge = nullptr;
        FString HistoricalR15Error;
        if (ValidateCompleteR15MaterialAssets(
                HistoricalR15, HistoricalR15Edge, HistoricalR15Error))
        {
            OutCoreMaterials.Reset();
            OutError = TEXT("The seven-package V5D namespace is uniformly clean R15/R11 historical state. Run UpgradeGroundVegetationRealismLawnOverlayToR16 before the R17 material upgrade.");
            return false;
        }
        TArray<UMaterial*> HistoricalR13;
        UMaterial* HistoricalEdge = nullptr;
        FString HistoricalError;
        if (ValidateCompleteR13MaterialAssets(
                HistoricalR13, HistoricalEdge, HistoricalError))
        {
            OutCoreMaterials.Reset();
            OutError = TEXT("The seven-package V5D namespace is uniformly clean R13/R11 historical state. Run UpgradeGroundVegetationRealismMaterialAssetsToR15 before current Ensure validation.");
            return false;
        }
        return false;
    }
    if (ExistingCount == 6)
    {
        TArray<UMaterial*> CoreMaterials;
        bool bCorePackagesClean = true;
        FString CoreRevision;
        if (bEdgeHasAnyState ||
            !ValidateUniformR13R15OrR16MaterialAssetsInternal(
                CoreMaterials, CoreRevision, OutError) ||
            CoreMaterials.Num() != 6)
        {
            OutError = TEXT("The seven-package V5D namespace is mixed or invalid; automatic repair is refused. ") +
                OutError;
            return false;
        }
        for (UMaterial* Core : CoreMaterials)
        {
            bCorePackagesClean &= Core && Core->GetOutermost() &&
                !Core->GetOutermost()->IsDirty();
        }
        if (!bCorePackagesClean)
        {
            OutError = TEXT("The six-package V5D R13/R15/R16 roster is dirty; explicit R11 migration is refused until every core package is clean.");
            return false;
        }
        OutError = FString::Printf(
            TEXT("The six clean %s core materials are valid and the R11 edge derivative is absent. Run UpgradeGroundVegetationEdgeGrassFadeAssetToR11; Ensure refuses to hide this explicit migration boundary."),
            *CoreRevision);
        return false;
    }
    if (ExistingCount == 0)
    {
        if (bEdgeHasAnyState)
        {
            OutError = TEXT("The zero-package V5D namespace has a stale R11 edge package/object/AssetRegistry state; creation is refused before any core package mutation.");
            return false;
        }
        TArray<UMaterial*> CreatedCore;
        if (!EnsureMaterials(CreatedCore, OutError) ||
            CreatedCore.Num() != 6)
        {
            return false;
        }
        FString R11Report;
        if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
                UpgradeGroundVegetationEdgeGrassFadeAssetToR11(R11Report))
        {
            OutError = TEXT("The zero-package build created and cold-validated four R15 grass packages, soil and the R16 lawn overlay, but the separate R11 edge transaction failed: ") +
                R11Report;
            return false;
        }
        FString R17Report;
        if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
                UpgradeGroundVegetationRealismMaterialAssetsToR17(R17Report))
        {
            OutError = TEXT("The zero-package build created clean R16 core and R11 edge state, but the explicit atomic R17 material transaction failed: ") +
                R17Report;
            return false;
        }
        FString R18Report;
        if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
                UpgradeGroundVegetationRealismLawnOverlayToR18(R18Report))
        {
            OutError = TEXT("The zero-package build created clean R17 grass/overlay and R11 edge state, but the explicit atomic one-package R18 lawn-overlay transaction failed: ") +
                R18Report;
            return false;
        }
        FString R19Report;
        if (!UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
                UpgradeGroundVegetationRealismGrassMaterialsToR19(R19Report))
        {
            OutError = TEXT("The zero-package build created clean R17 grass, R18 overlay and R11 edge state, but the explicit atomic four-package R19 grass transaction failed: ") +
                R19Report;
            return false;
        }
        UMaterial* EdgeFade = nullptr;
        const bool bValidR19 = ValidateCompleteR19MaterialAssets(
            OutCoreMaterials,
            EdgeFade,
            OutError);
        if (bValidR19)
        {
            OutGrassRevision = TEXT("R19");
        }
        return bValidR19;
    }
    OutError = FString::Printf(
        TEXT("The isolated V5D ground/vegetation material namespace is partial or mixed (%d/7 packages); overwrite or repair is refused."),
        ExistingCount);
    return false;
}

FString FileMd5(const FString& Filename)
{
    const FMD5Hash Hash = FMD5Hash::HashFile(*Filename);
    return Hash.IsValid() ? LexToString(Hash).ToUpper() : FString();
}

bool ResolvePersistedPackageFile(
    const FString& ObjectPathValue,
    FString& OutPackageName,
    FString& OutFilename,
    FString& OutError)
{
    OutPackageName = FPackageName::ObjectPathToPackageName(ObjectPathValue);
    if (OutPackageName.IsEmpty() ||
        !FPackageName::DoesPackageExist(OutPackageName, &OutFilename) ||
        IFileManager::Get().FileSize(*OutFilename) <= 0)
    {
        OutError = TEXT("Could not resolve the exact persisted V5D material package: ") +
            ObjectPathValue;
        return false;
    }
    OutError.Reset();
    return true;
}

TArray<FString> PackageArtifactCandidates(const FString& PackageFilename)
{
    const FString Base = FPaths::Combine(
        FPaths::GetPath(PackageFilename),
        FPaths::GetBaseFilename(PackageFilename));
    const FString CanonicalExtension =
        FPaths::GetExtension(PackageFilename, true);
    return {
        Base + CanonicalExtension,
        Base + TEXT(".uexp"),
        Base + TEXT(".ubulk"),
        Base + TEXT(".uptnl"),
        Base + TEXT(".m.ubulk"),
        Base + TEXT(".o.ubulk")};
}

TArray<FString> R21PackageArtifactCandidates(
    const FString& PackageFilename)
{
    TArray<FString> Candidates =
        PackageArtifactCandidates(PackageFilename);
    const FString Base = FPaths::Combine(
        FPaths::GetPath(PackageFilename),
        FPaths::GetBaseFilename(PackageFilename));
    Candidates.Add(Base + TEXT(".upayload"));
    return Candidates;
}

struct FPackageArtifactSnapshot
{
    FString Original;
    FString Md5;
    int64 Bytes = -1;
    bool bPresent = false;
};

struct FPackageArtifactBackup : FPackageArtifactSnapshot
{
    FString Backup;
    FString Sha256;
};

struct FMaterialPackageBackup
{
    FString PackageName;
    FString PackageFilename;
    TArray<FPackageArtifactBackup> Artifacts;
};

struct FMaterialUpgradeBackup
{
    FString Directory;
    int32 ExpectedPackageCount = 0;
    int32 ExpectedArtifactsPerPackage = 6;
    TArray<FMaterialPackageBackup> Packages;
};

bool CapturePackageArtifacts(
    const FString& PackageFilename,
    TArray<FPackageArtifactSnapshot>& OutArtifacts,
    FString& OutError)
{
    OutArtifacts.Reset();
    for (const FString& Filename : PackageArtifactCandidates(PackageFilename))
    {
        FPackageArtifactSnapshot& State = OutArtifacts.AddDefaulted_GetRef();
        State.Original = Filename;
        State.Bytes = IFileManager::Get().FileSize(*Filename);
        State.bPresent = State.Bytes >= 0;
        if (State.bPresent)
        {
            State.Md5 = FileMd5(Filename);
            if (State.Md5.IsEmpty())
            {
                OutError = TEXT("Could not hash a V5D package artifact: ") +
                    Filename;
                OutArtifacts.Reset();
                return false;
            }
        }
    }
    if (OutArtifacts.Num() != 6 || !OutArtifacts[0].bPresent ||
        OutArtifacts[0].Bytes <= 0)
    {
        OutError = TEXT("The canonical V5D package artifact is absent or empty: ") +
            PackageFilename;
        OutArtifacts.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureR21PackageArtifacts(
    const FString& PackageFilename,
    TArray<FPackageArtifactSnapshot>& OutArtifacts,
    FString& OutError)
{
    OutArtifacts.Reset();
    for (const FString& Filename :
         R21PackageArtifactCandidates(PackageFilename))
    {
        FPackageArtifactSnapshot& State =
            OutArtifacts.AddDefaulted_GetRef();
        State.Original = Filename;
        State.Bytes = IFileManager::Get().FileSize(*Filename);
        State.bPresent = State.Bytes >= 0;
        if (State.bPresent)
        {
            State.Md5 = FileMd5(Filename);
            if (State.Md5.IsEmpty())
            {
                OutError = TEXT("Could not hash an R21 package artifact: ") +
                    Filename;
                OutArtifacts.Reset();
                return false;
            }
        }
    }
    if (OutArtifacts.Num() != 7 || !OutArtifacts[0].bPresent ||
        OutArtifacts[0].Bytes <= 0)
    {
        OutError = TEXT("The canonical R21 package artifact is absent or empty: ") +
            PackageFilename;
        OutArtifacts.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool PackageArtifactsMatch(
    const TArray<FPackageArtifactSnapshot>& Expected,
    FString& OutError);
bool CaptureObjectPackageArtifacts(
    const FString& ObjectPathValue,
    TArray<FPackageArtifactSnapshot>& OutArtifacts,
    FString& OutError);
bool CaptureTargetMapArtifacts(
    TArray<FPackageArtifactSnapshot>& OutArtifacts,
    FString& OutError);

bool CaptureAbsentPackageArtifacts(
    const FString& PackageFilename,
    TArray<FPackageArtifactSnapshot>& OutArtifacts,
    FString& OutError)
{
    OutArtifacts.Reset();
    for (const FString& Filename : PackageArtifactCandidates(PackageFilename))
    {
        FPackageArtifactSnapshot& State =
            OutArtifacts.AddDefaulted_GetRef();
        State.Original = Filename;
        State.Bytes = IFileManager::Get().FileSize(*Filename);
        State.bPresent = State.Bytes >= 0;
        if (State.bPresent)
        {
            State.Md5 = FileMd5(Filename);
        }
    }
    if (OutArtifacts.Num() != 6 ||
        OutArtifacts.ContainsByPredicate(
            [](const FPackageArtifactSnapshot& State)
            {
                return State.bPresent;
            }))
    {
        OutError = TEXT("The R11 destination must have six exactly absent canonical/sidecar artifacts before creation: ") +
            PackageFilename;
        OutArtifacts.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

struct FR11PreservedPackageSnapshot
{
    FString Label;
    TArray<FPackageArtifactSnapshot> Artifacts;
};

bool CaptureR11PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    OutSnapshots.Reset();
    TArray<FString> ObjectPaths;
    for (const FString& Name : GrassAssetNames)
    {
        ObjectPaths.Add(ObjectPath(Name));
    }
    ObjectPaths.Add(ObjectPath(SoilAssetName));
    ObjectPaths.Add(ObjectPath(GroundOverlayAssetName));
    ObjectPaths.Add(SourceEdgeGrassMaterialPath);
    for (const FString& Path : ObjectPaths)
    {
        FR11PreservedPackageSnapshot& Record =
            OutSnapshots.AddDefaulted_GetRef();
        Record.Label = Path;
        if (!CaptureObjectPackageArtifacts(
                Path,
                Record.Artifacts,
                OutError))
        {
            OutSnapshots.Reset();
            return false;
        }
    }
    FR11PreservedPackageSnapshot& Map =
        OutSnapshots.AddDefaulted_GetRef();
    Map.Label = TargetMapPackage;
    if (!CaptureTargetMapArtifacts(Map.Artifacts, OutError) ||
        OutSnapshots.Num() != 8)
    {
        OutSnapshots.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool R11PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (Snapshots.Num() != 8)
    {
        OutError = TEXT("The R11 preserved-package snapshot roster is incomplete; expected six uniform R13 or R15 core materials, one V4 source and one map.");
        return false;
    }
    for (const FR11PreservedPackageSnapshot& Record : Snapshots)
    {
        if (!PackageArtifactsMatch(Record.Artifacts, OutError))
        {
            OutError = TEXT("R11 immutable package drifted: ") +
                Record.Label + TEXT(" ") + OutError;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CaptureR16PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    OutSnapshots.Reset();
    TArray<FString> ObjectPaths;
    for (const FString& Name : GrassAssetNames)
    {
        ObjectPaths.Add(ObjectPath(Name));
    }
    ObjectPaths.Add(ObjectPath(SoilAssetName));
    ObjectPaths.Add(ObjectPath(EdgeGrassFadeAssetName));
    for (const FString& Path : ObjectPaths)
    {
        FR11PreservedPackageSnapshot& Record =
            OutSnapshots.AddDefaulted_GetRef();
        Record.Label = Path;
        if (!CaptureObjectPackageArtifacts(
                Path,
                Record.Artifacts,
                OutError))
        {
            OutSnapshots.Reset();
            return false;
        }
    }
    FR11PreservedPackageSnapshot& Map =
        OutSnapshots.AddDefaulted_GetRef();
    Map.Label = TargetMapPackage;
    if (!CaptureTargetMapArtifacts(Map.Artifacts, OutError) ||
        OutSnapshots.Num() != 7)
    {
        OutSnapshots.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool R16PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (Snapshots.Num() != 7)
    {
        OutError = TEXT("The R16 preserved-package snapshot roster is incomplete; expected four R15 grass materials, soil, R11 edge fade and one map.");
        return false;
    }
    for (const FR11PreservedPackageSnapshot& Record : Snapshots)
    {
        if (!PackageArtifactsMatch(Record.Artifacts, OutError))
        {
            OutError = TEXT("R16 immutable package drifted: ") +
                Record.Label + TEXT(" ") + OutError;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CaptureR17PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    OutSnapshots.Reset();
    const TArray<FString> ObjectPaths = {
        ObjectPath(SoilAssetName), ObjectPath(EdgeGrassFadeAssetName),
        SourceGrassMaterialPath, SourceSoilMaterialPath, SourceEdgeGrassMaterialPath,
        GroundBaseColorTexturePath, GroundNormalTexturePath,
        GroundRoughnessTexturePath, GroundAoTexturePath};
    for (const FString& Path : ObjectPaths)
    {
        FR11PreservedPackageSnapshot& Record = OutSnapshots.AddDefaulted_GetRef();
        Record.Label = Path;
        if (!CaptureObjectPackageArtifacts(Path, Record.Artifacts, OutError))
        {
            OutSnapshots.Reset(); return false;
        }
    }
    FR11PreservedPackageSnapshot& Map = OutSnapshots.AddDefaulted_GetRef();
    Map.Label = TargetMapPackage;
    if (!CaptureTargetMapArtifacts(Map.Artifacts, OutError) || OutSnapshots.Num() != 10)
    {
        OutSnapshots.Reset(); return false;
    }
    OutError.Reset(); return true;
}

bool R17PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (Snapshots.Num() != 10)
    {
        OutError = TEXT("The R17 immutable snapshot roster is incomplete; soil, R11 edge, V3/V4/V5B sources, Grass004 textures, and target map are required.");
        return false;
    }
    for (const FR11PreservedPackageSnapshot& Record : Snapshots)
    {
        if (!PackageArtifactsMatch(Record.Artifacts, OutError))
        {
            OutError = TEXT("R17 immutable package drifted: ") + Record.Label + TEXT(" ") + OutError;
            return false;
        }
    }
    OutError.Reset(); return true;
}

bool CaptureR18PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    OutSnapshots.Reset();
    TArray<FString> ObjectPaths;
    for (const FString& Name : GrassAssetNames)
    {
        ObjectPaths.Add(ObjectPath(Name));
    }
    ObjectPaths.Add(ObjectPath(SoilAssetName));
    ObjectPaths.Add(ObjectPath(EdgeGrassFadeAssetName));
    ObjectPaths.Add(SourceGrassMaterialPath);
    ObjectPaths.Add(SourceSoilMaterialPath);
    ObjectPaths.Add(SourceEdgeGrassMaterialPath);
    ObjectPaths.Add(GroundBaseColorTexturePath);
    ObjectPaths.Add(GroundNormalTexturePath);
    ObjectPaths.Add(GroundRoughnessTexturePath);
    ObjectPaths.Add(GroundAoTexturePath);
    for (const FString& Path : ObjectPaths)
    {
        FR11PreservedPackageSnapshot& Record =
            OutSnapshots.AddDefaulted_GetRef();
        Record.Label = Path;
        if (!CaptureObjectPackageArtifacts(Path, Record.Artifacts, OutError))
        {
            OutSnapshots.Reset();
            return false;
        }
    }
    FR11PreservedPackageSnapshot& Map =
        OutSnapshots.AddDefaulted_GetRef();
    Map.Label = TargetMapPackage;
    if (!CaptureTargetMapArtifacts(Map.Artifacts, OutError) ||
        OutSnapshots.Num() != 14)
    {
        OutSnapshots.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool R18PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (Snapshots.Num() != 14)
    {
        OutError = TEXT("The R18 immutable snapshot roster is incomplete; four R17 grass packages, soil, R11 edge, V3/V4/V5B sources, Grass004 textures, and target map are required.");
        return false;
    }
    for (const FR11PreservedPackageSnapshot& Record : Snapshots)
    {
        if (!PackageArtifactsMatch(Record.Artifacts, OutError))
        {
            OutError = TEXT("R18 immutable package drifted: ") +
                Record.Label + TEXT(" ") + OutError;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CaptureR19PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    OutSnapshots.Reset();
    const TArray<FString> ObjectPaths = {
        ObjectPath(SoilAssetName),
        ObjectPath(GroundOverlayAssetName),
        ObjectPath(EdgeGrassFadeAssetName),
        SourceGrassMaterialPath,
        SourceSoilMaterialPath,
        SourceEdgeGrassMaterialPath,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath};
    for (const FString& Path : ObjectPaths)
    {
        FR11PreservedPackageSnapshot& Record =
            OutSnapshots.AddDefaulted_GetRef();
        Record.Label = Path;
        if (!CaptureObjectPackageArtifacts(
                Path, Record.Artifacts, OutError))
        {
            OutSnapshots.Reset();
            return false;
        }
    }
    FR11PreservedPackageSnapshot& Map =
        OutSnapshots.AddDefaulted_GetRef();
    Map.Label = TargetMapPackage;
    if (!CaptureTargetMapArtifacts(Map.Artifacts, OutError) ||
        OutSnapshots.Num() != 11)
    {
        OutSnapshots.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool R19PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (Snapshots.Num() != 11)
    {
        OutError = TEXT("The R19 immutable snapshot roster is incomplete; R18 overlay, soil, R11 edge, V3/V4/V5B sources, Grass004 textures, and target map are required.");
        return false;
    }
    for (const FR11PreservedPackageSnapshot& Record : Snapshots)
    {
        if (!PackageArtifactsMatch(Record.Artifacts, OutError))
        {
            OutError = TEXT("R19 immutable package drifted: ") +
                Record.Label + TEXT(" ") + OutError;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CaptureR21PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    return CaptureR19PreservedPackageSnapshots(OutSnapshots, OutError);
}

bool R21PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (!R19PreservedPackageSnapshotsMatch(Snapshots, OutError))
    {
        OutError = TEXT("R21 immutable package invariant failed: ") + OutError;
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureR22PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    OutSnapshots.Reset();
    const TArray<FString> ObjectPaths = {
        ObjectPath(SoilAssetName),
        SourceGrassMaterialPath,
        SourceSoilMaterialPath,
        SourceEdgeGrassMaterialPath,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath};
    for (const FString& Path : ObjectPaths)
    {
        FR11PreservedPackageSnapshot& Record =
            OutSnapshots.AddDefaulted_GetRef();
        Record.Label = Path;
        if (!CaptureObjectPackageArtifacts(
                Path, Record.Artifacts, OutError))
        {
            OutSnapshots.Reset();
            return false;
        }
    }
    FR11PreservedPackageSnapshot& Map =
        OutSnapshots.AddDefaulted_GetRef();
    Map.Label = TargetMapPackage;
    if (!CaptureTargetMapArtifacts(Map.Artifacts, OutError) ||
        OutSnapshots.Num() != 9)
    {
        OutSnapshots.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool R22PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (Snapshots.Num() != 9)
    {
        OutError = TEXT("The R22 immutable snapshot roster is incomplete; soil, V3/V4/V5B sources, Grass004 textures, and target map are required.");
        return false;
    }
    for (const FR11PreservedPackageSnapshot& Record : Snapshots)
    {
        if (!PackageArtifactsMatch(Record.Artifacts, OutError))
        {
            OutError = TEXT("R22 immutable package drifted: ") +
                Record.Label + TEXT(" ") + OutError;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool CaptureR23PreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    return CaptureR22PreservedPackageSnapshots(OutSnapshots, OutError);
}

bool R23PreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (!R22PreservedPackageSnapshotsMatch(Snapshots, OutError))
    {
        OutError = TEXT("R23 immutable package invariant failed: ") +
            OutError;
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureR23BPreservedPackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    return CaptureR23PreservedPackageSnapshots(OutSnapshots, OutError);
}

bool R23BPreservedPackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (!R23PreservedPackageSnapshotsMatch(Snapshots, OutError))
    {
        OutError = TEXT("R23B immutable package invariant failed: ") +
            OutError;
        return false;
    }
    OutError.Reset();
    return true;
}

struct FR11AbsentPackageBackup
{
    FString Directory;
    FString PackageName;
    FString PackageFilename;
    TArray<FPackageArtifactSnapshot> Artifacts;
};

bool BackUpAbsentR11PackageState(
    FR11AbsentPackageBackup& OutBackup,
    FString& OutError)
{
    OutBackup = FR11AbsentPackageBackup();
    OutBackup.PackageName = PackagePath(EdgeGrassFadeAssetName);
    OutBackup.PackageFilename = FPaths::ConvertRelativePathToFull(
        FPackageName::LongPackageNameToFilename(
            OutBackup.PackageName,
            FPackageName::GetAssetPackageExtension()));
    FPaths::NormalizeFilename(OutBackup.PackageFilename);
    if (!CaptureAbsentPackageArtifacts(
            OutBackup.PackageFilename,
            OutBackup.Artifacts,
            OutError))
    {
        return false;
    }

    FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/Backups/V5D_R11_EdgeGrassFade")));
    FPaths::NormalizeDirectoryName(BackupRoot);
    OutBackup.Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        BackupRoot,
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%f"))));
    FPaths::NormalizeDirectoryName(OutBackup.Directory);
    if (!OutBackup.Directory.StartsWith(
            BackupRoot + TEXT("/"),
            ESearchCase::IgnoreCase) ||
        IFileManager::Get().DirectoryExists(*OutBackup.Directory) ||
        !IFileManager::Get().MakeDirectory(*OutBackup.Directory, true))
    {
        OutError = TEXT("Could not create a fresh contained V5D R11 backup directory: ") +
            OutBackup.Directory;
        return false;
    }
    FString Receipt =
        TEXT("TRIAD_V5D_R11_EDGE_GRASS_FADE_BACKUP_V1\n") +
        FString(TEXT("package=")) + OutBackup.PackageName + TEXT("\n");
    for (const FPackageArtifactSnapshot& Artifact : OutBackup.Artifacts)
    {
        Receipt += FString::Printf(
            TEXT("original=%s\nstate=ABSENT\nrollback=DELETE_IF_PRESENT\n"),
            *Artifact.Original);
    }
    const FString ReceiptFilename = FPaths::Combine(
        OutBackup.Directory,
        TEXT("backup.receipt.txt"));
    FString Readback;
    if (!FFileHelper::SaveStringToFile(
            Receipt,
            *ReceiptFilename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !FFileHelper::LoadFileToString(Readback, *ReceiptFilename) ||
        Readback != Receipt ||
        !PackageArtifactsMatch(OutBackup.Artifacts, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D R11 absent-state backup receipt failed exact readback verification.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool PackageArtifactsMatch(
    const TArray<FPackageArtifactSnapshot>& Expected,
    FString& OutError)
{
    if (Expected.Num() != 6)
    {
        OutError = TEXT("A V5D package artifact snapshot is incomplete.");
        return false;
    }
    for (const FPackageArtifactSnapshot& State : Expected)
    {
        const int64 ActualBytes =
            IFileManager::Get().FileSize(*State.Original);
        if (State.bPresent)
        {
            if (ActualBytes != State.Bytes ||
                FileMd5(State.Original) != State.Md5)
            {
                OutError = TEXT("A preserved V5D package artifact hash/size changed: ") +
                    State.Original;
                return false;
            }
        }
        else if (ActualBytes >= 0)
        {
            OutError = TEXT("A sidecar that was absent from the V5D snapshot appeared: ") +
                State.Original;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool R21PackageArtifactsMatch(
    const TArray<FPackageArtifactSnapshot>& Expected,
    FString& OutError)
{
    if (Expected.Num() != 7)
    {
        OutError = TEXT("An R21 package artifact snapshot must contain the canonical artifact plus all six watched sidecar states.");
        return false;
    }
    for (const FPackageArtifactSnapshot& State : Expected)
    {
        const int64 ActualBytes =
            IFileManager::Get().FileSize(*State.Original);
        if (State.bPresent)
        {
            if (ActualBytes != State.Bytes ||
                FileMd5(State.Original) != State.Md5)
            {
                OutError = TEXT("A preserved R21 package artifact hash/size changed: ") +
                    State.Original;
                return false;
            }
        }
        else if (ActualBytes >= 0)
        {
            OutError = TEXT("An R21 sidecar that was absent from the seven-artifact snapshot appeared: ") +
                State.Original;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool PackageArtifactsMatch(
    const TArray<FPackageArtifactBackup>& Expected,
    FString& OutError)
{
    TArray<FPackageArtifactSnapshot> Snapshot;
    Snapshot.Reserve(Expected.Num());
    for (const FPackageArtifactBackup& Backup : Expected)
    {
        FPackageArtifactSnapshot& State = Snapshot.AddDefaulted_GetRef();
        State.Original = Backup.Original;
        State.Md5 = Backup.Md5;
        State.Bytes = Backup.Bytes;
        State.bPresent = Backup.bPresent;
    }
    return PackageArtifactsMatch(Snapshot, OutError);
}

bool R21PackageArtifactsMatch(
    const TArray<FPackageArtifactBackup>& Expected,
    FString& OutError)
{
    TArray<FPackageArtifactSnapshot> Snapshot;
    Snapshot.Reserve(Expected.Num());
    for (const FPackageArtifactBackup& Backup : Expected)
    {
        FPackageArtifactSnapshot& State = Snapshot.AddDefaulted_GetRef();
        State.Original = Backup.Original;
        State.Md5 = Backup.Md5;
        State.Bytes = Backup.Bytes;
        State.bPresent = Backup.bPresent;
    }
    return R21PackageArtifactsMatch(Snapshot, OutError);
}

bool CaptureObjectPackageArtifacts(
    const FString& ObjectPathValue,
    TArray<FPackageArtifactSnapshot>& OutArtifacts,
    FString& OutError)
{
    FString PackageName;
    FString PackageFilename;
    return ResolvePersistedPackageFile(
               ObjectPathValue,
               PackageName,
               PackageFilename,
               OutError) &&
        CapturePackageArtifacts(PackageFilename, OutArtifacts, OutError);
}

bool CaptureTargetMapArtifacts(
    TArray<FPackageArtifactSnapshot>& OutArtifacts,
    FString& OutError)
{
    FString Filename;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &Filename) ||
        IFileManager::Get().FileSize(*Filename) <= 0)
    {
        OutError = TEXT("The exact persisted V5D target map is required as a no-save hash invariant.");
        return false;
    }
    return CapturePackageArtifacts(Filename, OutArtifacts, OutError);
}

bool HashFileSha256R20(
    const FString& Filename,
    FString& OutSha256,
    int64& OutBytes,
    FString& OutError)
{
    OutSha256.Reset();
    OutBytes = INDEX_NONE;
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
    {
        OutError = TEXT("Could not read an R20 grass-layout transaction artifact: ") +
            Filename;
        return false;
    }
    OutBytes = Bytes.Num();
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr)
    {
        OutError = TEXT("Could not compute an R20 grass-layout SHA-256: ") +
            Filename;
        return false;
    }
    static_assert(SHA256_DIGEST_LENGTH == 32);
    OutSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH).ToUpper();
#else
    OutError = TEXT("The R20 grass-layout transaction requires WITH_SSL for SHA-256 admission.");
    return false;
#endif
    OutError.Reset();
    return true;
}

bool ValidateR21ExactDiskPins(
    bool bAlreadyR21,
    FString& OutPinReport,
    FString& OutError)
{
    OutPinReport.Reset();
    FString MapFilename;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &MapFilename))
    {
        OutError = TEXT("The R21 grass calibration requires the exact persisted R20 target map artifact.");
        return false;
    }
    MapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
    FPaths::NormalizeFilename(MapFilename);
    FString MapSha256;
    int64 MapBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            MapFilename, MapSha256, MapBytes, OutError) ||
        MapBytes != R20FinalMapBytes || MapSha256 != R20FinalMapSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R21 rejected target-map drift: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R20FinalMapBytes,
                MapBytes,
                *R20FinalMapSha256,
                *MapSha256);
        }
        return false;
    }
    OutPinReport = FString::Printf(
        TEXT("mapBytes=%lld mapSha256=%s"),
        MapBytes,
        *MapSha256);

    if (bAlreadyR21)
    {
        FString ArtifactStateReport;
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
        {
            FString PackageName;
            FString Filename;
            TArray<FPackageArtifactSnapshot> Artifacts;
            if (!ResolvePersistedPackageFile(
                    ObjectPath(GrassAssetNames[Index]),
                    PackageName,
                    Filename,
                    OutError) ||
                !CaptureR21PackageArtifacts(
                    Filename, Artifacts, OutError) ||
                Artifacts.Num() != 7)
            {
                return false;
            }
            for (int32 ArtifactIndex = 1;
                 ArtifactIndex < Artifacts.Num();
                 ++ArtifactIndex)
            {
                if (Artifacts[ArtifactIndex].bPresent)
                {
                    OutError = FString::Printf(
                        TEXT("R21 idempotence rejected unexpected target sidecar state for %s: %s."),
                        *GrassAssetNames[Index],
                        *Artifacts[ArtifactIndex].Original);
                    return false;
                }
            }
            if (!ArtifactStateReport.IsEmpty())
            {
                ArtifactStateReport += TEXT(",");
            }
            ArtifactStateReport += GrassAssetNames[Index] +
                TEXT(":canonical_only");
        }
        OutPinReport +=
            TEXT(" predecessorPins=not_applicable_uniform_r21 currentR21TargetArtifactStates=") +
            ArtifactStateReport;
        OutError.Reset();
        return true;
    }

    FString PredecessorPinReport;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        FString PackageName;
        FString Filename;
        if (!ResolvePersistedPackageFile(
                ObjectPath(GrassAssetNames[Index]),
                PackageName,
                Filename,
                OutError))
        {
            return false;
        }
        Filename = FPaths::ConvertRelativePathToFull(Filename);
        FPaths::NormalizeFilename(Filename);
        TArray<FPackageArtifactSnapshot> Artifacts;
        if (!CaptureR21PackageArtifacts(
                Filename, Artifacts, OutError) ||
            Artifacts.Num() != 7)
        {
            return false;
        }
        for (int32 ArtifactIndex = 1;
             ArtifactIndex < Artifacts.Num();
             ++ArtifactIndex)
        {
            if (Artifacts[ArtifactIndex].bPresent)
            {
                OutError = FString::Printf(
                    TEXT("R21 exact predecessor pin rejected unexpected sidecar state for %s: %s."),
                    *GrassAssetNames[Index],
                    *Artifacts[ArtifactIndex].Original);
                return false;
            }
        }
        FString Sha256;
        int64 Bytes = INDEX_NONE;
        if (!HashFileSha256R20(Filename, Sha256, Bytes, OutError) ||
            Bytes != R21PredecessorGrassPins[Index].Bytes ||
            Sha256 != R21PredecessorGrassPins[Index].Sha256)
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("R21 rejected non-canonical R19 predecessor %s: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                    *GrassAssetNames[Index],
                    R21PredecessorGrassPins[Index].Bytes,
                    Bytes,
                    R21PredecessorGrassPins[Index].Sha256,
                    *Sha256);
            }
            return false;
        }
        if (!PredecessorPinReport.IsEmpty())
        {
            PredecessorPinReport += TEXT(",");
        }
        PredecessorPinReport += FString::Printf(
            TEXT("%s:%lld:%s"),
            *GrassAssetNames[Index],
            Bytes,
            *Sha256);
    }
    OutPinReport += TEXT(" predecessorPins=") + PredecessorPinReport;
    OutError.Reset();
    return true;
}

bool ValidateR22ExactDiskPins(
    bool bAlreadyR22,
    FString& OutPinReport,
    FString& OutError)
{
    OutPinReport.Reset();
    FString MapFilename;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &MapFilename))
    {
        OutError = TEXT("The R22 grass-system migration requires the exact persisted R20 target map artifact.");
        return false;
    }
    MapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
    FPaths::NormalizeFilename(MapFilename);
    FString MapSha256;
    int64 MapBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            MapFilename, MapSha256, MapBytes, OutError) ||
        MapBytes != R20FinalMapBytes || MapSha256 != R20FinalMapSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R22 rejected target-map drift: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R20FinalMapBytes,
                MapBytes,
                *R20FinalMapSha256,
                *MapSha256);
        }
        return false;
    }

    FString SoilPackageName;
    FString SoilFilename;
    FString SoilSha256;
    int64 SoilBytes = INDEX_NONE;
    if (!ResolvePersistedPackageFile(
            ObjectPath(SoilAssetName),
            SoilPackageName,
            SoilFilename,
            OutError) ||
        !HashFileSha256R20(
            SoilFilename, SoilSha256, SoilBytes, OutError) ||
        SoilBytes != R22PreservedSoilBytes ||
        SoilSha256 != R22PreservedSoilSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R22 rejected soil drift: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R22PreservedSoilBytes,
                SoilBytes,
                *R22PreservedSoilSha256,
                *SoilSha256);
        }
        return false;
    }
    OutPinReport = FString::Printf(
        TEXT("mapBytes=%lld mapSha256=%s soilBytes=%lld soilSha256=%s"),
        MapBytes,
        *MapSha256,
        SoilBytes,
        *SoilSha256);

    FString TargetReport;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(R22PredecessorPins); ++Index)
    {
        const FR22PredecessorArtifactPin& Pin = R22PredecessorPins[Index];
        const FString AssetName(Pin.AssetName);
        FString PackageName;
        FString Filename;
        TArray<FPackageArtifactSnapshot> Artifacts;
        if (!ResolvePersistedPackageFile(
                ObjectPath(AssetName),
                PackageName,
                Filename,
                OutError) ||
            !CaptureR21PackageArtifacts(Filename, Artifacts, OutError) ||
            Artifacts.Num() != 7)
        {
            return false;
        }
        for (int32 ArtifactIndex = 1;
             ArtifactIndex < Artifacts.Num();
             ++ArtifactIndex)
        {
            if (Artifacts[ArtifactIndex].bPresent)
            {
                OutError = FString::Printf(
                    TEXT("R22 rejected unexpected target sidecar state for %s: %s."),
                    *AssetName,
                    *Artifacts[ArtifactIndex].Original);
                return false;
            }
        }
        if (!TargetReport.IsEmpty())
        {
            TargetReport += TEXT(",");
        }
        if (bAlreadyR22)
        {
            TargetReport += AssetName + TEXT(":canonical_only");
            continue;
        }
        FString Sha256;
        int64 Bytes = INDEX_NONE;
        if (!HashFileSha256R20(Filename, Sha256, Bytes, OutError) ||
            Bytes != Pin.Bytes || Sha256 != Pin.Sha256)
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("R22 rejected non-canonical predecessor %s: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                    *AssetName,
                    Pin.Bytes,
                    Bytes,
                    Pin.Sha256,
                    *Sha256);
            }
            return false;
        }
        TargetReport += FString::Printf(
            TEXT("%s:%lld:%s"),
            *AssetName,
            Bytes,
            *Sha256);
    }
    OutPinReport += bAlreadyR22
        ? TEXT(" predecessorPins=not_applicable_uniform_r22 currentR22TargetArtifactStates=") +
            TargetReport
        : TEXT(" predecessorPins=") + TargetReport;
    OutError.Reset();
    return true;
}

bool ValidateR23ExactDiskPins(
    bool bAlreadyR23,
    FString& OutPinReport,
    FString& OutError)
{
    OutPinReport.Reset();
    FString MapFilename;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &MapFilename))
    {
        OutError = TEXT("The R23 grass-system migration requires the exact persisted R20 predecessor or R23 layout target map artifact.");
        return false;
    }
    MapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
    FPaths::NormalizeFilename(MapFilename);
    FString MapSha256;
    int64 MapBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            MapFilename, MapSha256, MapBytes, OutError))
    {
        return false;
    }
    const bool bExactR20Map = MapBytes == R20FinalMapBytes &&
        MapSha256 == R20FinalMapSha256;
    const bool bExactR23Map = MapBytes == R23FinalMapBytes &&
        MapSha256 == R23FinalMapSha256;
    const bool bMapAdmitted = bAlreadyR23
        ? bExactR20Map || bExactR23Map
        : bExactR20Map;
    if (!bMapAdmitted)
    {
        if (OutError.IsEmpty())
        {
            OutError = bAlreadyR23
                ? FString::Printf(
                      TEXT("R23 rejected target-map drift: a uniform R23 material state requires the exact R20 predecessor or exact R23 layout map; r20Bytes=%lld actualBytes=%lld r20Sha256=%s actualSha256=%s r23Bytes=%lld r23Sha256=%s."),
                      R20FinalMapBytes,
                      MapBytes,
                      *R20FinalMapSha256,
                      *MapSha256,
                      R23FinalMapBytes,
                      *R23FinalMapSha256)
                : FString::Printf(
                      TEXT("R23 rejected target-map drift: R22-to-R23 material mutation requires the exact R20 predecessor map; expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s r23Bytes=%lld r23Sha256=%s."),
                      R20FinalMapBytes,
                      MapBytes,
                      *R20FinalMapSha256,
                      *MapSha256,
                      R23FinalMapBytes,
                      *R23FinalMapSha256);
        }
        return false;
    }

    FString SoilPackageName;
    FString SoilFilename;
    FString SoilSha256;
    int64 SoilBytes = INDEX_NONE;
    if (!ResolvePersistedPackageFile(
            ObjectPath(SoilAssetName),
            SoilPackageName,
            SoilFilename,
            OutError) ||
        !HashFileSha256R20(
            SoilFilename, SoilSha256, SoilBytes, OutError) ||
        SoilBytes != R22PreservedSoilBytes ||
        SoilSha256 != R22PreservedSoilSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R23 rejected soil drift: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R22PreservedSoilBytes,
                SoilBytes,
                *R22PreservedSoilSha256,
                *SoilSha256);
        }
        return false;
    }
    OutPinReport = FString::Printf(
        TEXT("mapBytes=%lld mapSha256=%s admittedMapRevision=%s exactAdmittedR20OrR23MapPin=true exactR20MapPin=%s exactR23MapPin=%s soilBytes=%lld soilSha256=%s"),
        MapBytes,
        *MapSha256,
        bExactR23Map ? TEXT("R23") : TEXT("R20"),
        bExactR20Map ? TEXT("true") : TEXT("false"),
        bExactR23Map ? TEXT("true") : TEXT("false"),
        SoilBytes,
        *SoilSha256);

    FString TargetReport;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(R23PredecessorPins); ++Index)
    {
        const FR22PredecessorArtifactPin& Pin = R23PredecessorPins[Index];
        const FString AssetName(Pin.AssetName);
        FString PackageName;
        FString Filename;
        TArray<FPackageArtifactSnapshot> Artifacts;
        if (!ResolvePersistedPackageFile(
                ObjectPath(AssetName),
                PackageName,
                Filename,
                OutError) ||
            !CaptureR21PackageArtifacts(Filename, Artifacts, OutError) ||
            Artifacts.Num() != 7)
        {
            return false;
        }
        for (int32 ArtifactIndex = 1;
             ArtifactIndex < Artifacts.Num();
             ++ArtifactIndex)
        {
            if (Artifacts[ArtifactIndex].bPresent)
            {
                OutError = FString::Printf(
                    TEXT("R23 rejected unexpected target sidecar state for %s: %s."),
                    *AssetName,
                    *Artifacts[ArtifactIndex].Original);
                return false;
            }
        }
        if (!TargetReport.IsEmpty())
        {
            TargetReport += TEXT(",");
        }
        if (bAlreadyR23)
        {
            TargetReport += AssetName + TEXT(":canonical_only");
            continue;
        }
        FString Sha256;
        int64 Bytes = INDEX_NONE;
        if (!HashFileSha256R20(Filename, Sha256, Bytes, OutError) ||
            Bytes != Pin.Bytes || Sha256 != Pin.Sha256)
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("R23 rejected non-canonical R22 predecessor %s: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                    *AssetName,
                    Pin.Bytes,
                    Bytes,
                    Pin.Sha256,
                    *Sha256);
            }
            return false;
        }
        TargetReport += FString::Printf(
            TEXT("%s:%lld:%s"),
            *AssetName,
            Bytes,
            *Sha256);
    }
    OutPinReport += bAlreadyR23
        ? TEXT(" predecessorPins=not_applicable_uniform_r23 currentR23TargetArtifactStates=") +
            TargetReport
        : TEXT(" predecessorR22Pins=") + TargetReport;
    OutError.Reset();
    return true;
}

bool ValidateR23BExactDiskPins(
    bool bAlreadyR23B,
    FString& OutPinReport,
    FString& OutError)
{
    OutPinReport.Reset();
    FString MapFilename;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &MapFilename))
    {
        OutError = TEXT("The R23B material-only calibration requires the exact persisted R23 target map artifact.");
        return false;
    }
    MapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
    FPaths::NormalizeFilename(MapFilename);
    FString MapSha256;
    int64 MapBytes = INDEX_NONE;
    if (!HashFileSha256R20(MapFilename, MapSha256, MapBytes, OutError) ||
        MapBytes != R23FinalMapBytes || MapSha256 != R23FinalMapSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R23B rejected target-map drift: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R23FinalMapBytes,
                MapBytes,
                *R23FinalMapSha256,
                *MapSha256);
        }
        return false;
    }

    FString SoilPackageName;
    FString SoilFilename;
    FString SoilSha256;
    int64 SoilBytes = INDEX_NONE;
    if (!ResolvePersistedPackageFile(
            ObjectPath(SoilAssetName),
            SoilPackageName,
            SoilFilename,
            OutError) ||
        SoilPackageName != PackagePath(SoilAssetName) ||
        !HashFileSha256R20(
            SoilFilename, SoilSha256, SoilBytes, OutError) ||
        SoilBytes != R22PreservedSoilBytes ||
        SoilSha256 != R22PreservedSoilSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R23B rejected soil drift: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R22PreservedSoilBytes,
                SoilBytes,
                *R22PreservedSoilSha256,
                *SoilSha256);
        }
        return false;
    }
    OutPinReport = FString::Printf(
        TEXT("mapBytes=%lld mapSha256=%s exactR23MapPin=true soilBytes=%lld soilSha256=%s"),
        MapBytes,
        *MapSha256,
        SoilBytes,
        *SoilSha256);

    const bool bRequireExactTargetPins =
        !bAlreadyR23B || R23BFinalPinsAreSealed;
    const FR22PredecessorArtifactPin* ExpectedPins = bAlreadyR23B
        ? R23BFinalMaterialPins : R23BPredecessorPins;
    FString TargetReport;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(R23BPredecessorPins); ++Index)
    {
        const FR22PredecessorArtifactPin& Pin = ExpectedPins[Index];
        const FString AssetName(Pin.AssetName);
        FString PackageName;
        FString Filename;
        TArray<FPackageArtifactSnapshot> Artifacts;
        if (!ResolvePersistedPackageFile(
                ObjectPath(AssetName),
                PackageName,
                Filename,
                OutError) ||
            PackageName != PackagePath(AssetName) ||
            !CaptureR21PackageArtifacts(Filename, Artifacts, OutError) ||
            Artifacts.Num() != 7 || !Artifacts[0].bPresent)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("R23B could not resolve an exact canonical target package and seven artifact states: ") +
                    AssetName;
            }
            return false;
        }
        for (int32 ArtifactIndex = 1; ArtifactIndex < Artifacts.Num(); ++ArtifactIndex)
        {
            if (Artifacts[ArtifactIndex].bPresent)
            {
                OutError = FString::Printf(
                    TEXT("R23B rejected unexpected target sidecar state for %s: %s."),
                    *AssetName,
                    *Artifacts[ArtifactIndex].Original);
                return false;
            }
        }
        if (!TargetReport.IsEmpty())
        {
            TargetReport += TEXT(",");
        }
        if (!bRequireExactTargetPins)
        {
            TargetReport += AssetName + TEXT(":canonical_only_unsealed");
            continue;
        }
        FString Sha256;
        int64 Bytes = INDEX_NONE;
        if (!HashFileSha256R20(Filename, Sha256, Bytes, OutError) ||
            Bytes != Pin.Bytes || Sha256 != Pin.Sha256)
        {
            if (OutError.IsEmpty())
            {
                if (bAlreadyR23B)
                {
                    OutError = FString::Printf(
                        TEXT("R23B rejected non-canonical sealed final package %s: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                        *AssetName,
                        Pin.Bytes,
                        Bytes,
                        Pin.Sha256,
                        *Sha256);
                }
                else
                {
                    OutError = FString::Printf(
                        TEXT("R23B rejected non-canonical R23 predecessor %s: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                        *AssetName,
                        Pin.Bytes,
                        Bytes,
                        Pin.Sha256,
                        *Sha256);
                }
            }
            return false;
        }
        TargetReport += FString::Printf(
            TEXT("%s:%lld:%s"), *AssetName, Bytes, *Sha256);
    }
    if (bAlreadyR23B)
    {
        OutPinReport += R23BFinalPinsAreSealed
            ? TEXT(" finalPinsSealed=true currentR23BTargetPins=") +
                TargetReport
            : TEXT(" finalPinsSealed=false finalPins=UNSEALED_BOOTSTRAP currentR23BTargetArtifactStates=") +
                TargetReport;
    }
    else
    {
        OutPinReport += TEXT(" predecessorR23Pins=") + TargetReport;
    }
    OutError.Reset();
    return true;
}

bool ValidateR23LayoutMaterialDiskPins(
    FString& OutPinReport,
    FString& OutError)
{
    OutPinReport.Reset();
    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    if (!ValidateCompleteR23MaterialAssets(
            Materials,
            EdgeFadeMaterial,
            OutError) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R23 layout migration requires the exact clean seven-material R23 graph roster.");
        }
        return false;
    }

    FString SoilPackageName;
    FString SoilFilename;
    TArray<FPackageArtifactSnapshot> SoilArtifacts;
    if (!ResolvePersistedPackageFile(
            ObjectPath(SoilAssetName),
            SoilPackageName,
            SoilFilename,
            OutError) ||
        SoilPackageName != PackagePath(SoilAssetName) ||
        !CaptureR21PackageArtifacts(
            SoilFilename,
            SoilArtifacts,
            OutError) ||
        SoilArtifacts.Num() != 7 || !SoilArtifacts[0].bPresent)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R23 layout migration could not resolve the exact canonical soil package and seven artifact states.");
        }
        return false;
    }
    for (int32 ArtifactIndex = 1;
         ArtifactIndex < SoilArtifacts.Num();
         ++ArtifactIndex)
    {
        if (SoilArtifacts[ArtifactIndex].bPresent)
        {
            OutError = TEXT("The R23 layout migration rejected an unexpected soil sidecar: ") +
                SoilArtifacts[ArtifactIndex].Original;
            return false;
        }
    }
    FString SoilSha256;
    int64 SoilBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            SoilFilename,
            SoilSha256,
            SoilBytes,
            OutError) ||
        SoilBytes != R22PreservedSoilBytes ||
        SoilSha256 != R22PreservedSoilSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R23 layout migration rejected soil drift: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R22PreservedSoilBytes,
                SoilBytes,
                *R22PreservedSoilSha256,
                *SoilSha256);
        }
        return false;
    }

    FString MaterialPinReport;
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(R23LayoutMaterialPins);
         ++Index)
    {
        const FR22PredecessorArtifactPin& Pin =
            R23LayoutMaterialPins[Index];
        const FString AssetName(Pin.AssetName);
        FString PackageName;
        FString Filename;
        TArray<FPackageArtifactSnapshot> Artifacts;
        if (!ResolvePersistedPackageFile(
                ObjectPath(AssetName),
                PackageName,
                Filename,
                OutError) ||
            PackageName != PackagePath(AssetName) ||
            !CaptureR21PackageArtifacts(Filename, Artifacts, OutError) ||
            Artifacts.Num() != 7 || !Artifacts[0].bPresent)
        {
            if (OutError.IsEmpty())
            {
                OutError = TEXT("The R23 layout migration could not resolve an exact canonical material package and seven artifact states: ") +
                    AssetName;
            }
            return false;
        }
        for (int32 ArtifactIndex = 1;
             ArtifactIndex < Artifacts.Num();
             ++ArtifactIndex)
        {
            if (Artifacts[ArtifactIndex].bPresent)
            {
                OutError = FString::Printf(
                    TEXT("R23 layout migration rejected an unexpected material sidecar for %s: %s."),
                    *AssetName,
                    *Artifacts[ArtifactIndex].Original);
                return false;
            }
        }
        FString Sha256;
        int64 Bytes = INDEX_NONE;
        if (!HashFileSha256R20(Filename, Sha256, Bytes, OutError) ||
            Bytes != Pin.Bytes || Sha256 != Pin.Sha256)
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("R23 layout migration rejected material drift for %s: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                    *AssetName,
                    Pin.Bytes,
                    Bytes,
                    Pin.Sha256,
                    *Sha256);
            }
            return false;
        }
        if (!MaterialPinReport.IsEmpty())
        {
            MaterialPinReport += TEXT(",");
        }
        MaterialPinReport += FString::Printf(
            TEXT("%s:%lld:%s"),
            *AssetName,
            Bytes,
            *Sha256);
    }
    OutPinReport = FString::Printf(
        TEXT("soilBytes=%lld soilSha256=%s r23MaterialPins=%s"),
        SoilBytes,
        *SoilSha256,
        *MaterialPinReport);
    OutError.Reset();
    return true;
}

struct FR20MapArtifactBackup : FPackageArtifactBackup
{
    FString Sha256;
};

struct FR20MapBackup
{
    FString Directory;
    FString PackageFilename;
    TArray<FR20MapArtifactBackup> Artifacts;
};

bool CreateVerifiedR20MapBackup(
    FR20MapBackup& OutBackup,
    FString& OutError)
{
    OutBackup = FR20MapBackup();
    if (!FPackageName::DoesPackageExist(
            TargetMapPackage,
            &OutBackup.PackageFilename))
    {
        OutError = TEXT("The exact persisted V5D R20 target map is absent.");
        return false;
    }
    OutBackup.PackageFilename = FPaths::ConvertRelativePathToFull(
        OutBackup.PackageFilename);
    FPaths::NormalizeFilename(OutBackup.PackageFilename);

    TArray<FPackageArtifactSnapshot> Snapshot;
    if (!CapturePackageArtifacts(
            OutBackup.PackageFilename,
            Snapshot,
            OutError) ||
        Snapshot.Num() != 6 ||
        Snapshot[0].Bytes != R20PredecessorMapBytes)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R20 map backup did not see the exact predecessor canonical byte count.");
        }
        return false;
    }
    FString CanonicalSha256;
    int64 CanonicalBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            OutBackup.PackageFilename,
            CanonicalSha256,
            CanonicalBytes,
            OutError) ||
        CanonicalBytes != R20PredecessorMapBytes ||
        CanonicalSha256 != R20PredecessorMapSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R20 map backup rejected non-predecessor disk content.");
        }
        return false;
    }

    FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("TRIAD/Backups/V5D_R20_GroundLayout")));
    FPaths::NormalizeDirectoryName(BackupRoot);
    OutBackup.Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        BackupRoot,
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%f"))));
    FPaths::NormalizeDirectoryName(OutBackup.Directory);
    if (!OutBackup.Directory.StartsWith(
            BackupRoot + TEXT("/"),
            ESearchCase::IgnoreCase) ||
        IFileManager::Get().DirectoryExists(*OutBackup.Directory) ||
        !IFileManager::Get().MakeDirectory(*OutBackup.Directory, true))
    {
        OutError = TEXT("Could not create a fresh contained V5D R20 grass-layout backup directory: ") +
            OutBackup.Directory;
        return false;
    }

    FString Receipt =
        TEXT("TRIAD_V5D_R20_GRASS_LAYOUT_BACKUP_V1\n")
        TEXT("package=/Game/Maps/Istana_PublicView_Explore_v5d_hybrid\n");
    for (const FPackageArtifactSnapshot& State : Snapshot)
    {
        FR20MapArtifactBackup& Artifact =
            OutBackup.Artifacts.AddDefaulted_GetRef();
        Artifact.Original = State.Original;
        Artifact.Md5 = State.Md5;
        Artifact.Bytes = State.Bytes;
        Artifact.bPresent = State.bPresent;
        if (State.bPresent)
        {
            int64 OriginalBytes = INDEX_NONE;
            if (!HashFileSha256R20(
                    State.Original,
                    Artifact.Sha256,
                    OriginalBytes,
                    OutError) ||
                OriginalBytes != State.Bytes)
            {
                return false;
            }
            Artifact.Backup = FPaths::Combine(
                OutBackup.Directory,
                FPaths::GetCleanFilename(State.Original));
            if (IFileManager::Get().FileSize(*Artifact.Backup) >= 0 ||
                IFileManager::Get().Copy(
                    *Artifact.Backup,
                    *State.Original,
                    true,
                    true) != COPY_OK)
            {
                OutError = TEXT("Could not copy an exact V5D R20 map artifact into the backup: ") +
                    State.Original;
                return false;
            }
            FString BackupSha256;
            int64 BackupBytes = INDEX_NONE;
            if (!HashFileSha256R20(
                    Artifact.Backup,
                    BackupSha256,
                    BackupBytes,
                    OutError) ||
                BackupBytes != State.Bytes ||
                BackupSha256 != Artifact.Sha256 ||
                FileMd5(Artifact.Backup) != State.Md5)
            {
                OutError = TEXT("A copied V5D R20 map backup artifact failed hash/size readback: ") +
                    State.Original;
                return false;
            }
            Receipt += FString::Printf(
                TEXT("original=%s\nstate=PRESENT\nbackup=%s\nbytes=%lld\nmd5=%s\nsha256=%s\n"),
                *Artifact.Original,
                *Artifact.Backup,
                Artifact.Bytes,
                *Artifact.Md5,
                *Artifact.Sha256);
        }
        else
        {
            Receipt += FString::Printf(
                TEXT("original=%s\nstate=ABSENT\nrollback=DELETE_IF_PRESENT\n"),
                *Artifact.Original);
        }
    }

    const FString ReceiptFilename = FPaths::Combine(
        OutBackup.Directory,
        TEXT("backup.receipt.txt"));
    FString ReceiptReadback;
    if (OutBackup.Artifacts.Num() != 6 ||
        !FFileHelper::SaveStringToFile(
            Receipt,
            *ReceiptFilename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !FFileHelper::LoadFileToString(
            ReceiptReadback,
            *ReceiptFilename) ||
        ReceiptReadback != Receipt ||
        !PackageArtifactsMatch(Snapshot, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D R20 map backup receipt failed exact readback verification.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool CreateVerifiedR23MapBackup(
    FR20MapBackup& OutBackup,
    FString& OutError)
{
    OutBackup = FR20MapBackup();
    if (!FPackageName::DoesPackageExist(
            TargetMapPackage,
            &OutBackup.PackageFilename))
    {
        OutError = TEXT("The exact persisted V5D R20 predecessor map for the R23 layout migration is absent.");
        return false;
    }
    OutBackup.PackageFilename = FPaths::ConvertRelativePathToFull(
        OutBackup.PackageFilename);
    FPaths::NormalizeFilename(OutBackup.PackageFilename);

    TArray<FPackageArtifactSnapshot> Snapshot;
    if (!CapturePackageArtifacts(
            OutBackup.PackageFilename,
            Snapshot,
            OutError) ||
        Snapshot.Num() != 6 || !Snapshot[0].bPresent ||
        Snapshot[0].Bytes != R20FinalMapBytes)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The R23 map backup did not see the exact six-state R20 predecessor and canonical byte count.");
        }
        return false;
    }
    FString CanonicalSha256;
    int64 CanonicalBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            OutBackup.PackageFilename,
            CanonicalSha256,
            CanonicalBytes,
            OutError) ||
        CanonicalBytes != R20FinalMapBytes ||
        CanonicalSha256 != R20FinalMapSha256)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("The R23 map backup rejected non-canonical R20 disk content: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s."),
                R20FinalMapBytes,
                CanonicalBytes,
                *R20FinalMapSha256,
                *CanonicalSha256);
        }
        return false;
    }

    FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        R23LayoutBackupRelativeRoot));
    FPaths::NormalizeDirectoryName(BackupRoot);
    OutBackup.Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        BackupRoot,
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%f"))));
    FPaths::NormalizeDirectoryName(OutBackup.Directory);
    if (!OutBackup.Directory.StartsWith(
            BackupRoot + TEXT("/"),
            ESearchCase::IgnoreCase) ||
        IFileManager::Get().DirectoryExists(*OutBackup.Directory) ||
        !IFileManager::Get().MakeDirectory(*OutBackup.Directory, true))
    {
        OutError = TEXT("Could not create a fresh contained V5D R23 grass-layout backup directory: ") +
            OutBackup.Directory;
        return false;
    }

    FString Receipt = R23LayoutBackupReceiptHeader + TEXT("\n") +
        TEXT("package=/Game/Maps/Istana_PublicView_Explore_v5d_hybrid\n")
        TEXT("predecessorRevision=20\n")
        TEXT("targetRevision=23\n");
    for (const FPackageArtifactSnapshot& State : Snapshot)
    {
        FR20MapArtifactBackup& Artifact =
            OutBackup.Artifacts.AddDefaulted_GetRef();
        Artifact.Original = State.Original;
        Artifact.Md5 = State.Md5;
        Artifact.Bytes = State.Bytes;
        Artifact.bPresent = State.bPresent;
        if (State.bPresent)
        {
            int64 OriginalBytes = INDEX_NONE;
            if (!HashFileSha256R20(
                    State.Original,
                    Artifact.Sha256,
                    OriginalBytes,
                    OutError) ||
                OriginalBytes != State.Bytes)
            {
                return false;
            }
            Artifact.Backup = FPaths::Combine(
                OutBackup.Directory,
                FPaths::GetCleanFilename(State.Original));
            if (IFileManager::Get().FileSize(*Artifact.Backup) >= 0 ||
                IFileManager::Get().Copy(
                    *Artifact.Backup,
                    *State.Original,
                    true,
                    true) != COPY_OK)
            {
                OutError = TEXT("Could not copy an exact V5D R20 predecessor map artifact into the R23 backup: ") +
                    State.Original;
                return false;
            }
            FString BackupSha256;
            int64 BackupBytes = INDEX_NONE;
            if (!HashFileSha256R20(
                    Artifact.Backup,
                    BackupSha256,
                    BackupBytes,
                    OutError) ||
                BackupBytes != State.Bytes ||
                BackupSha256 != Artifact.Sha256 ||
                FileMd5(Artifact.Backup) != State.Md5)
            {
                OutError = TEXT("A copied V5D R23 map backup artifact failed hash/size readback: ") +
                    State.Original;
                return false;
            }
            Receipt += FString::Printf(
                TEXT("original=%s\nstate=PRESENT\nbackup=%s\nbytes=%lld\nmd5=%s\nsha256=%s\n"),
                *Artifact.Original,
                *Artifact.Backup,
                Artifact.Bytes,
                *Artifact.Md5,
                *Artifact.Sha256);
        }
        else
        {
            Receipt += FString::Printf(
                TEXT("original=%s\nstate=ABSENT\nrollback=DELETE_IF_PRESENT\n"),
                *Artifact.Original);
        }
    }

    const FString ReceiptFilename = FPaths::Combine(
        OutBackup.Directory,
        TEXT("backup.receipt.txt"));
    FString ReceiptReadback;
    if (OutBackup.Artifacts.Num() != 6 ||
        !FFileHelper::SaveStringToFile(
            Receipt,
            *ReceiptFilename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) ||
        !FFileHelper::LoadFileToString(
            ReceiptReadback,
            *ReceiptFilename) ||
        ReceiptReadback != Receipt ||
        !PackageArtifactsMatch(Snapshot, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D R23 map backup receipt failed exact readback verification.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool R20MapBackupStillExact(
    const FR20MapBackup& Backup,
    FString& OutError)
{
    if (Backup.Artifacts.Num() != 6)
    {
        OutError = TEXT("The V5D R20 map backup artifact roster is incomplete.");
        return false;
    }
    for (const FR20MapArtifactBackup& Artifact : Backup.Artifacts)
    {
        if (!Artifact.bPresent)
        {
            continue;
        }
        FString Sha256;
        int64 Bytes = INDEX_NONE;
        if (Artifact.Backup.IsEmpty() ||
            !HashFileSha256R20(
                Artifact.Backup,
                Sha256,
                Bytes,
                OutError) ||
            Bytes != Artifact.Bytes ||
            Sha256 != Artifact.Sha256 ||
            FileMd5(Artifact.Backup) != Artifact.Md5)
        {
            OutError = TEXT("A V5D R20 backup artifact no longer matches its sealed receipt: ") +
                Artifact.Backup;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool RestoreR20MapArtifactsOnDisk(
    const FR20MapBackup& Backup,
    FString& OutError)
{
    if (!R20MapBackupStillExact(Backup, OutError))
    {
        return false;
    }
    bool bRestored = Backup.Artifacts.Num() == 6;
    for (const FR20MapArtifactBackup& Artifact : Backup.Artifacts)
    {
        if (Artifact.bPresent)
        {
            bRestored &= IFileManager::Get().Copy(
                *Artifact.Original,
                *Artifact.Backup,
                true,
                true) == COPY_OK;
        }
        else if (IFileManager::Get().FileSize(*Artifact.Original) >= 0)
        {
            bRestored &= IFileManager::Get().Delete(
                *Artifact.Original,
                false,
                true);
        }
    }
    TArray<FPackageArtifactSnapshot> RestoredSnapshot;
    RestoredSnapshot.Reserve(Backup.Artifacts.Num());
    for (const FR20MapArtifactBackup& Artifact : Backup.Artifacts)
    {
        FPackageArtifactSnapshot& State =
            RestoredSnapshot.AddDefaulted_GetRef();
        State.Original = Artifact.Original;
        State.Md5 = Artifact.Md5;
        State.Bytes = Artifact.Bytes;
        State.bPresent = Artifact.bPresent;
    }
    if (!bRestored || !PackageArtifactsMatch(RestoredSnapshot, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Could not restore all six exact V5D R20 predecessor map artifacts.");
        }
        return false;
    }
    IAssetRegistry::GetChecked().ScanModifiedAssetFiles(
        {Backup.PackageFilename});
    OutError.Reset();
    return true;
}

bool RestoreR23MapArtifactsOnDisk(
    const FR20MapBackup& Backup,
    FString& OutError)
{
    if (!RestoreR20MapArtifactsOnDisk(Backup, OutError))
    {
        OutError = TEXT("R23 layout rollback failed to restore its exact R20 predecessor: ") +
            OutError;
        return false;
    }
    OutError.Reset();
    return true;
}

bool CaptureR20ImmutablePackageSnapshots(
    TArray<FR11PreservedPackageSnapshot>& OutSnapshots,
    FString& OutError)
{
    OutSnapshots.Reset();
    TArray<FString> ObjectPaths;
    for (const FString& Name : GrassAssetNames)
    {
        ObjectPaths.Add(ObjectPath(Name));
    }
    const TArray<FString> AdditionalPaths = {
        ObjectPath(SoilAssetName),
        ObjectPath(GroundOverlayAssetName),
        ObjectPath(EdgeGrassFadeAssetName),
        SourceGrassMaterialPath,
        SourceSoilMaterialPath,
        SourceEdgeGrassMaterialPath,
        GroundBaseColorTexturePath,
        GroundNormalTexturePath,
        GroundRoughnessTexturePath,
        GroundAoTexturePath};
    ObjectPaths.Append(AdditionalPaths);
    for (const FString& Path : ObjectPaths)
    {
        UObject* Object = StaticLoadObject(
            UObject::StaticClass(),
            nullptr,
            *Path);
        if (!Object || Object->GetPathName() != Path ||
            !Object->GetOutermost() || Object->GetOutermost()->IsDirty())
        {
            OutError = TEXT("The exact clean R20 immutable object/package is unavailable: ") +
                Path;
            OutSnapshots.Reset();
            return false;
        }
        FR11PreservedPackageSnapshot& Record =
            OutSnapshots.AddDefaulted_GetRef();
        Record.Label = Path;
        if (!CaptureObjectPackageArtifacts(
                Path,
                Record.Artifacts,
                OutError))
        {
            OutSnapshots.Reset();
            return false;
        }
    }
    if (OutSnapshots.Num() != 14)
    {
        OutError = TEXT("The R20 immutable roster requires exactly seven authored materials, three source materials and four Grass004 textures.");
        OutSnapshots.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool R20ImmutablePackageSnapshotsMatch(
    const TArray<FR11PreservedPackageSnapshot>& Snapshots,
    FString& OutError)
{
    if (Snapshots.Num() != 14)
    {
        OutError = TEXT("The captured R20 immutable material/source/texture roster is incomplete.");
        return false;
    }
    for (const FR11PreservedPackageSnapshot& Record : Snapshots)
    {
        if (!PackageArtifactsMatch(Record.Artifacts, OutError))
        {
            OutError = TEXT("An R20 immutable material/source/texture package drifted: ") +
                Record.Label + TEXT(" ") + OutError;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool PersistExactR20TargetWorld(UWorld* World)
{
    if (!World)
    {
        return false;
    }
    World->MarkPackageDirty();
    return UEditorLoadingAndSavingUtils::SaveMap(World, TargetMapPackage);
}

bool PersistExactR23TargetWorld(UWorld* World)
{
    return PersistExactR20TargetWorld(World);
}

bool ReloadSingleR11MaterialPackage(FString& OutError)
{
    UPackage* Package = FindPackage(
        nullptr,
        *PackagePath(EdgeGrassFadeAssetName));
    if (!Package)
    {
        OutError = TEXT("The exact R11 edge-fade package disappeared before its one-package reload.");
        return false;
    }
    Package->SetDirtyFlag(false);
    TArray<UPackage*> Packages = {Package};
    FText ReloadError;
    if (!UPackageTools::ReloadPackages(
            Packages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutError = TEXT("The exact one-package V5D R11 reload failed: ") +
            ReloadError.ToString();
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreAbsentR11PackageAtomically(
    const FR11AbsentPackageBackup& Backup,
    FString& OutError)
{
    UPackage* Package = FindPackage(nullptr, *Backup.PackageName);
    if (Package)
    {
        TArray<UPackage*> Packages = {Package};
        FText UnloadError;
        if (!UPackageTools::UnloadPackages(
                Packages,
                UnloadError,
                true))
        {
            OutError = TEXT("V5D R11 rollback stopped before deletion because the new package could not be unloaded; artifacts and backup are retained for manual recovery. ") +
                UnloadError.ToString() + TEXT(" backup=") +
                Backup.Directory;
            return false;
        }
    }
    if (FindPackage(nullptr, *Backup.PackageName))
    {
        OutError = TEXT("V5D R11 rollback stopped before deletion because the unloaded package still resolves; artifacts and backup are retained. backup=") +
            Backup.Directory;
        return false;
    }

    bool bDeleted = Backup.Artifacts.Num() == 6;
    for (const FPackageArtifactSnapshot& Artifact : Backup.Artifacts)
    {
        if (IFileManager::Get().FileSize(*Artifact.Original) >= 0)
        {
            bDeleted &= IFileManager::Get().Delete(
                *Artifact.Original,
                false,
                true);
        }
    }
    if (!bDeleted || !PackageArtifactsMatch(Backup.Artifacts, OutError))
    {
        OutError = TEXT("V5D R11 rollback could not restore all six initially absent package artifacts. backup=") +
            Backup.Directory + TEXT(" ") + OutError;
        return false;
    }

    IAssetRegistry& Registry = IAssetRegistry::GetChecked();
    Registry.ScanModifiedAssetFiles({Backup.PackageFilename});
    const FString EdgeObjectPath = ObjectPath(EdgeGrassFadeAssetName);
    const FAssetData AssetData = Registry.GetAssetByObjectPath(
        FSoftObjectPath(EdgeObjectPath));
    if (FindPackage(nullptr, *Backup.PackageName) ||
        FindObject<UMaterial>(nullptr, *EdgeObjectPath) ||
        AssetData.IsValid())
    {
        OutError = TEXT("V5D R11 rollback removed disk artifacts but the exact transient package/object/AssetRegistry record still resolves. backup=") +
            Backup.Directory;
        return false;
    }
    OutError.Reset();
    return true;
}

bool BackUpMaterialPackagesForRevision(
    const TArray<UMaterial*>& Targets,
    int32 ExpectedPackageCount,
    const FString& Revision,
    const FString& ReceiptHeader,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError,
    bool bUseR21SevenArtifactRoster = false,
    bool bSealSha256ForRestore = false)
{
    OutBackup = FMaterialUpgradeBackup();
    if (ExpectedPackageCount <= 0 || Targets.Num() != ExpectedPackageCount ||
        Targets.Contains(nullptr))
    {
        OutError = TEXT("The V5D material backup target roster does not match its exact positive package count.");
        return false;
    }
    OutBackup.ExpectedPackageCount = ExpectedPackageCount;
    OutBackup.ExpectedArtifactsPerPackage =
        bUseR21SevenArtifactRoster ? 7 : 6;
    FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        FString::Printf(
            TEXT("TRIAD/Backups/V5D_%s_GroundMaterials"),
            *Revision)));
    FPaths::NormalizeDirectoryName(BackupRoot);
    OutBackup.Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        BackupRoot,
        FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S_%f"))));
    FPaths::NormalizeDirectoryName(OutBackup.Directory);
    if (!OutBackup.Directory.StartsWith(BackupRoot + TEXT("/"),
            ESearchCase::IgnoreCase) ||
        IFileManager::Get().DirectoryExists(*OutBackup.Directory) ||
        !IFileManager::Get().MakeDirectory(*OutBackup.Directory, true))
    {
        OutError = TEXT("Could not create a fresh contained V5D ") +
            Revision + TEXT(" backup directory: ") +
            OutBackup.Directory;
        return false;
    }

    FString Receipt = ReceiptHeader + TEXT("\n");
    for (UMaterial* Target : Targets)
    {
        if (!Target || !Target->GetOutermost() ||
            Target->GetOutermost()->IsDirty())
        {
            OutError = TEXT("A V5D ") + Revision +
                TEXT(" target became absent or dirty before backup.");
            return false;
        }
        FMaterialPackageBackup& Package =
            OutBackup.Packages.AddDefaulted_GetRef();
        Package.PackageName = Target->GetOutermost()->GetName();
        FString ResolvedPackageName;
        if (!ResolvePersistedPackageFile(
                Target->GetPathName(),
                ResolvedPackageName,
                Package.PackageFilename,
                OutError) ||
            ResolvedPackageName != Package.PackageName)
        {
            return false;
        }
        TArray<FPackageArtifactSnapshot> Snapshot;
        const bool bCaptured = bUseR21SevenArtifactRoster
            ? CaptureR21PackageArtifacts(
                Package.PackageFilename, Snapshot, OutError)
            : CapturePackageArtifacts(
                Package.PackageFilename, Snapshot, OutError);
        if (!bCaptured ||
            Snapshot.Num() != OutBackup.ExpectedArtifactsPerPackage)
        {
            return false;
        }
        if (bUseR21SevenArtifactRoster)
        {
            for (int32 ArtifactIndex = 1;
                 ArtifactIndex < Snapshot.Num();
                 ++ArtifactIndex)
            {
                if (Snapshot[ArtifactIndex].bPresent)
                {
                    OutError = TEXT("The R21 backup gate requires canonical-only target state across all seven watched artifacts: ") +
                        Snapshot[ArtifactIndex].Original;
                    return false;
                }
            }
        }
        Receipt += TEXT("package=") + Package.PackageName + TEXT("\n");
        for (const FPackageArtifactSnapshot& State : Snapshot)
        {
            FPackageArtifactBackup& Artifact =
                Package.Artifacts.AddDefaulted_GetRef();
            Artifact.Original = State.Original;
            Artifact.Md5 = State.Md5;
            Artifact.Bytes = State.Bytes;
            Artifact.bPresent = State.bPresent;
            if (State.bPresent)
            {
                if (bSealSha256ForRestore)
                {
                    int64 OriginalBytes = INDEX_NONE;
                    if (!HashFileSha256R20(
                            State.Original,
                            Artifact.Sha256,
                            OriginalBytes,
                            OutError) ||
                        OriginalBytes != State.Bytes ||
                        Artifact.Sha256.IsEmpty())
                    {
                        OutError = TEXT("Could not SHA-256 seal an R23B predecessor artifact before backup: ") +
                            State.Original + TEXT(" ") + OutError;
                        return false;
                    }
                }
                Artifact.Backup = FPaths::Combine(
                    OutBackup.Directory,
                    FPaths::GetCleanFilename(State.Original));
                if (IFileManager::Get().FileSize(*Artifact.Backup) >= 0 ||
                    IFileManager::Get().Copy(
                        *Artifact.Backup, *State.Original, true, true) !=
                        COPY_OK ||
                    IFileManager::Get().FileSize(*Artifact.Backup) !=
                        State.Bytes ||
                    FileMd5(Artifact.Backup) != State.Md5)
                {
                    OutError = TEXT("Could not create and verify a V5D package-artifact backup: ") +
                        State.Original;
                    return false;
                }
                if (bSealSha256ForRestore)
                {
                    FString BackupSha256;
                    int64 BackupBytes = INDEX_NONE;
                    if (!HashFileSha256R20(
                            Artifact.Backup,
                            BackupSha256,
                            BackupBytes,
                            OutError) ||
                        BackupBytes != State.Bytes ||
                        BackupSha256 != Artifact.Sha256)
                    {
                        OutError = TEXT("An R23B backup artifact failed SHA-256/size readback: ") +
                            Artifact.Backup;
                        return false;
                    }
                    Receipt += FString::Printf(
                        TEXT("original=%s\nstate=PRESENT\nbackup=%s\nbytes=%lld\nmd5=%s\nsha256=%s\n"),
                        *State.Original,
                        *Artifact.Backup,
                        State.Bytes,
                        *State.Md5,
                        *Artifact.Sha256);
                }
                else
                {
                    Receipt += FString::Printf(
                        TEXT("original=%s\nstate=PRESENT\nbackup=%s\nbytes=%lld\nmd5=%s\n"),
                        *State.Original,
                        *Artifact.Backup,
                        State.Bytes,
                        *State.Md5);
                }
            }
            else
            {
                Receipt += FString::Printf(
                    TEXT("original=%s\nstate=ABSENT\nrollback=DELETE_IF_PRESENT\n"),
                    *State.Original);
            }
        }
    }
    const FString ReceiptFilename = FPaths::Combine(
        OutBackup.Directory, TEXT("backup.receipt.txt"));
    if (OutBackup.Packages.Num() != ExpectedPackageCount ||
        !FFileHelper::SaveStringToFile(
            Receipt,
            *ReceiptFilename,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("The exact %d-package V5D "), ExpectedPackageCount) + Revision +
            TEXT(" backup or receipt is incomplete.");
        return false;
    }
    FString ReceiptReadback;
    if (!FFileHelper::LoadFileToString(ReceiptReadback, *ReceiptFilename) ||
        ReceiptReadback != Receipt)
    {
        OutError = TEXT("The V5D ") + Revision +
            TEXT(" backup receipt failed exact readback verification.");
        return false;
    }
    for (const FMaterialPackageBackup& Package : OutBackup.Packages)
    {
        const bool bMatch = bUseR21SevenArtifactRoster
            ? R21PackageArtifactsMatch(Package.Artifacts, OutError)
            : PackageArtifactsMatch(Package.Artifacts, OutError);
        if (!bMatch)
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool BackUpMaterialPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        5,
        TEXT("R10"),
        TEXT("TRIAD_V5D_R10_GROUND_MATERIAL_BACKUP_V2"),
        OutBackup,
        OutError);
}

bool BackUpR12MaterialPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        5,
        TEXT("R12"),
        TEXT("TRIAD_V5D_R12_GROUND_MATERIAL_BACKUP_V1"),
        OutBackup,
        OutError);
}

bool BackUpR13MaterialPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        5,
        TEXT("R13"),
        TEXT("TRIAD_V5D_R13_GROUND_MATERIAL_BACKUP_V1"),
        OutBackup,
        OutError);
}

bool BackUpR15MaterialPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        5,
        TEXT("R15"),
        TEXT("TRIAD_V5D_R15_GROUND_MATERIAL_BACKUP_V1"),
        OutBackup,
        OutError);
}

bool BackUpR16LawnOverlayPackage(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        1,
        TEXT("R16"),
        TEXT("TRIAD_V5D_R16_LAWN_OVERLAY_BACKUP_V1"),
        OutBackup,
        OutError);
}

bool BackUpR17GroundMaterialPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        5,
        TEXT("R17"),
        TEXT("TRIAD_V5D_R17_GROUND_MATERIALS_BACKUP_V1"),
        OutBackup,
        OutError);
}

bool BackUpR18LawnOverlayPackage(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        1,
        TEXT("R18"),
        TEXT("TRIAD_V5D_R18_LAWN_OVERLAY_BACKUP_V1"),
        OutBackup,
        OutError);
}

bool BackUpR19GrassMaterialPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        4,
        TEXT("R19"),
        TEXT("TRIAD_V5D_R19_GRASS_MATERIALS_BACKUP_V1"),
        OutBackup,
        OutError);
}

bool BackUpR21GrassMaterialPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        4,
        TEXT("R21"),
        TEXT("TRIAD_V5D_R21_GRASS_MATERIALS_BACKUP_V1"),
        OutBackup,
        OutError,
        true);
}

bool BackUpR22GrassSystemPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        6,
        TEXT("R22"),
        TEXT("TRIAD_V5D_R22_GRASS_SYSTEM_BACKUP_V1"),
        OutBackup,
        OutError,
        true);
}

bool BackUpR23GrassSystemPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        6,
        TEXT("R23"),
        TEXT("TRIAD_V5D_R23_GRASS_SYSTEM_BACKUP_V1"),
        OutBackup,
        OutError,
        true);
}

bool BackUpR23BGrassSystemPackages(
    const TArray<UMaterial*>& Targets,
    FMaterialUpgradeBackup& OutBackup,
    FString& OutError)
{
    return BackUpMaterialPackagesForRevision(
        Targets,
        6,
        TEXT("R23B"),
        TEXT("TRIAD_V5D_R23B_GRASS_SYSTEM_BACKUP_V1"),
        OutBackup,
        OutError,
        true,
        true);
}

bool ReloadMaterialPackages(
    const FMaterialUpgradeBackup& Backup,
    FString& OutError)
{
    TArray<UPackage*> Packages;
    TArray<FString> MissingPackageNames;
    for (const FMaterialPackageBackup& Record : Backup.Packages)
    {
        UPackage* Package = FindPackage(nullptr, *Record.PackageName);
        if (!Package)
        {
            MissingPackageNames.Add(Record.PackageName);
            continue;
        }
        Package->SetDirtyFlag(false);
        Packages.Add(Package);
    }
    FText ReloadError;
    if (Backup.ExpectedPackageCount <= 0 ||
        Packages.Num() + MissingPackageNames.Num() !=
            Backup.ExpectedPackageCount ||
        (Packages.Num() > 0 &&
            !UPackageTools::ReloadPackages(
                Packages,
                ReloadError,
                EReloadPackagesInteractionMode::AssumePositive)))
    {
        OutError = FString::Printf(
            TEXT("Exact %d-package V5D material reload failed: "),
            Backup.ExpectedPackageCount) +
            ReloadError.ToString();
        return false;
    }
    // ReloadPackages can leave a package absent after a partial failure. A
    // rollback has already restored the canonical disk artifacts before it
    // reaches this helper, so load any missing package from that exact path
    // instead of making successful disk restoration impossible to rehydrate.
    for (const FString& PackageName : MissingPackageNames)
    {
        UPackage* LoadedPackage = LoadPackage(
            nullptr,
            *PackageName,
            LOAD_None);
        if (!LoadedPackage || LoadedPackage->GetName() != PackageName)
        {
            OutError = TEXT("A restored V5D material package could not be loaded from its canonical disk path: ") +
                PackageName;
            return false;
        }
        LoadedPackage->SetDirtyFlag(false);
    }
    for (const FMaterialPackageBackup& Record : Backup.Packages)
    {
        UPackage* ReloadedPackage = FindPackage(nullptr, *Record.PackageName);
        if (!ReloadedPackage || ReloadedPackage->IsDirty())
        {
            OutError = TEXT("A V5D material package was absent or dirty after exact reload: ") +
                Record.PackageName;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool PrevalidateMaterialUpgradeBackupForRestore(
    const FMaterialUpgradeBackup& Backup,
    FString& OutError)
{
    if (Backup.ExpectedPackageCount <= 0 ||
        Backup.Packages.Num() != Backup.ExpectedPackageCount ||
        (Backup.ExpectedArtifactsPerPackage != 6 &&
            Backup.ExpectedArtifactsPerPackage != 7))
    {
        OutError = TEXT("The V5D material rollback backup roster is incomplete before canonical restore.");
        return false;
    }
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.PackageName.IsEmpty() ||
            Package.PackageFilename.IsEmpty() ||
            Package.Artifacts.Num() != Backup.ExpectedArtifactsPerPackage ||
            !Package.Artifacts[0].bPresent ||
            Package.Artifacts[0].Original != Package.PackageFilename ||
            Package.Artifacts[0].Bytes <= 0)
        {
            OutError = TEXT("A V5D material rollback package record is incomplete before canonical restore: ") +
                Package.PackageName;
            return false;
        }
        for (const FPackageArtifactBackup& Artifact : Package.Artifacts)
        {
            if (Artifact.Original.IsEmpty())
            {
                OutError = TEXT("A V5D material rollback artifact has no canonical path before restore.");
                return false;
            }
            if (Artifact.bPresent)
            {
                FString BackupSha256;
                int64 BackupBytes = INDEX_NONE;
                const bool bSha256StillExact = Artifact.Sha256.IsEmpty() ||
                    (HashFileSha256R20(
                         Artifact.Backup,
                         BackupSha256,
                         BackupBytes,
                         OutError) &&
                     BackupBytes == Artifact.Bytes &&
                     BackupSha256 == Artifact.Sha256);
                if (Artifact.Backup.IsEmpty() || Artifact.Bytes < 0 ||
                    Artifact.Md5.IsEmpty() ||
                    IFileManager::Get().FileSize(*Artifact.Backup) !=
                        Artifact.Bytes ||
                    FileMd5(Artifact.Backup) != Artifact.Md5 ||
                    !bSha256StillExact)
                {
                    OutError = TEXT("A V5D material rollback backup failed complete hash/size prevalidation before canonical restore: ") +
                        Artifact.Backup;
                    return false;
                }
            }
            else if (!Artifact.Backup.IsEmpty() || Artifact.Bytes >= 0 ||
                !Artifact.Md5.IsEmpty() || !Artifact.Sha256.IsEmpty())
            {
                OutError = TEXT("A V5D material rollback absent-artifact record is inconsistent before canonical restore: ") +
                    Artifact.Original;
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool RestoreMaterialPackagesAtomically(
    const FMaterialUpgradeBackup& Backup,
    FString& OutError)
{
    if (!PrevalidateMaterialUpgradeBackupForRestore(Backup, OutError))
    {
        return false;
    }
    bool bDiskRestoreSucceeded = true;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        bDiskRestoreSucceeded &=
            Package.Artifacts.Num() == Backup.ExpectedArtifactsPerPackage;
        for (const FPackageArtifactBackup& Artifact : Package.Artifacts)
        {
            if (Artifact.bPresent)
            {
                bDiskRestoreSucceeded &=
                    !Artifact.Backup.IsEmpty() &&
                    IFileManager::Get().Copy(
                        *Artifact.Original,
                        *Artifact.Backup,
                        true,
                        true) == COPY_OK;
            }
            else if (IFileManager::Get().FileSize(*Artifact.Original) >= 0)
            {
                bDiskRestoreSucceeded &= IFileManager::Get().Delete(
                    *Artifact.Original, false, true);
            }
        }
    }
    if (!bDiskRestoreSucceeded)
    {
        OutError = TEXT("Atomic V5D material rollback could not restore every package artifact. Backup: ") +
            Backup.Directory;
        return false;
    }
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        const bool bMatch = Backup.ExpectedArtifactsPerPackage == 7
            ? R21PackageArtifactsMatch(Package.Artifacts, OutError)
            : PackageArtifactsMatch(Package.Artifacts, OutError);
        if (!bMatch)
        {
            OutError = TEXT("V5D material rollback pre-reload verification failed: ") +
                OutError + TEXT(" Backup: ") + Backup.Directory;
            return false;
        }
    }
    if (!ReloadMaterialPackages(Backup, OutError))
    {
        OutError += TEXT(" Backup: ") + Backup.Directory;
        return false;
    }
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        const bool bMatch = Backup.ExpectedArtifactsPerPackage == 7
            ? R21PackageArtifactsMatch(Package.Artifacts, OutError)
            : PackageArtifactsMatch(Package.Artifacts, OutError);
        if (!bMatch)
        {
            OutError = TEXT("V5D material rollback post-reload verification failed: ") +
                OutError + TEXT(" Backup: ") + Backup.Directory;
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool BuildAssetRoster(
    ATRIADIstanaPublicViewSceneActor* Scene,
    ATRIADIstanaExploreV4LandscapeActor* V4,
    ATRIADIstanaExploreV5BVisualActor* V5B,
    const TArray<UMaterial*>& Materials,
    FTRIADIstanaExploreV5DGroundVegetationAssets& OutAssets,
    FString& OutError)
{
    OutAssets = FTRIADIstanaExploreV5DGroundVegetationAssets();
    if (!Scene || !Scene->TerrainComponent || !V4 || !V5B ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !V5B->AccentTurfInstances || !V5B->TreeBaseMulchInstances ||
        !V4->GeometryGrassInstances || !V4->ShrubInstances ||
        !V4->UnderstoreyInstances || !V4->FlowerInstances)
    {
        OutError = TEXT("The exact public-view/V4/V5B ground and vegetation source roster is incomplete.");
        return false;
    }
    OutAssets.GroundOverlayMesh =
        Scene->TerrainComponent->GetStaticMesh();
    OutAssets.GroundOverlayMaterial = Materials[5];
    OutAssets.GrassMicroDetailMesh = V5B->AccentTurfInstances->GetStaticMesh();
    for (int32 Index = 0; Index < 4; ++Index)
    {
        OutAssets.GrassProfileMaterials.Add(Materials[Index]);
    }
    OutAssets.EdgeGrassMesh = V4->GeometryGrassInstances->GetStaticMesh();
    OutAssets.EdgeGrassMaterial = V4->GeometryGrassInstances->GetMaterial(0);
    OutAssets.SoilPatchMesh = V5B->TreeBaseMulchInstances->GetStaticMesh();
    OutAssets.SoilPatchMaterial = Materials[4];
    OutAssets.ShrubMesh = V4->ShrubInstances->GetStaticMesh();
    OutAssets.ShrubMaterial = V4->ShrubInstances->GetMaterial(0);
    OutAssets.UnderstoreyMesh = V4->UnderstoreyInstances->GetStaticMesh();
    OutAssets.UnderstoreyMaterial = V4->UnderstoreyInstances->GetMaterial(0);
    OutAssets.FlowerMesh = V4->FlowerInstances->GetStaticMesh();
    OutAssets.FlowerMaterial = V4->FlowerInstances->GetMaterial(0);
    return ATRIADIstanaExploreV5DGroundVegetationActor::ValidateAssetRoster(
        OutAssets,
        OutError);
}

bool ValidateLoadedWorld(
    UWorld* World,
    ATRIADIstanaExploreV5DGroundVegetationActor*& OutActor,
    FString& OutReport)
{
    OutActor = nullptr;
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != TargetMapPackage)
    {
        OutReport = TEXT("The current editor world is not /Game/Maps/Istana_PublicView_Explore_v5d_hybrid.");
        return false;
    }
    int32 Count = 0;
    OutActor = FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
        World,
        Count);
    if (Count != 1 || !OutActor ||
        !OutActor->Tags.Contains(GroundVegetationTag) ||
        !OutActor->ValidateGroundVegetationRealism(OutReport))
    {
        if (OutReport.IsEmpty())
        {
            OutReport = FString::Printf(
                TEXT("V5D requires exactly one tagged ground/vegetation actor; found %d."),
                Count);
        }
        return false;
    }
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    ValidateExactR23BDerivativeGrassMaterial(
        const FString& MaterialObjectPath,
        int32 ProfileIndex,
        const FString& StableVisibilityDescription,
        const FString& StableVisibilityCode,
        bool bRequireSaved,
        FString& OutReport)
{
    if (ProfileIndex < 0 ||
        ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames) ||
        MaterialObjectPath.IsEmpty() ||
        StableVisibilityDescription.IsEmpty() ||
        StableVisibilityCode.IsEmpty())
    {
        OutReport = TEXT("R23B derivative validation requires an exact profile, object path and stable-visibility replacement.");
        return false;
    }

    UMaterial* Source = LoadExact<UMaterial>(
        ObjectPath(GrassAssetNames[ProfileIndex]));
    UMaterial* Target = LoadExact<UMaterial>(MaterialObjectPath);
    FString Error;
    if (!ValidateR23BGrassMaterial(Source, ProfileIndex, Error) ||
        !ValidateR21R22OrR23GrassMaterial(
            Target,
            ProfileIndex,
            23,
            Error,
            true,
            &MaterialObjectPath,
            &StableVisibilityDescription,
            &StableVisibilityCode))
    {
        OutReport = TEXT("R23B derivative graph validation failed: ") + Error;
        return false;
    }

    const FString TargetPackagePath =
        FPackageName::ObjectPathToPackageName(MaterialObjectPath);
    if (bRequireSaved &&
        (!Source || !Target || !Source->GetOutermost() ||
         !Target->GetOutermost() || Source->GetOutermost()->IsDirty() ||
         Target->GetOutermost()->IsDirty() ||
         !FPackageName::DoesPackageExist(
             PackagePath(GrassAssetNames[ProfileIndex])) ||
         !FPackageName::DoesPackageExist(TargetPackagePath)))
    {
        OutReport = TEXT("R23B derivative validation requires clean disk-backed source and target packages.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("V5D_R23B_DERIVATIVE_MATERIAL_VALID profileIndex=%d exactGraph=true compiled=true saved=%s"),
        ProfileIndex,
        bRequireSaved ? TEXT("true") : TEXT("not-required"));
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    ValidateExactR23BVisualDerivativeGrassMaterial(
        const FString& MaterialObjectPath,
        int32 ProfileIndex,
        const FString& StableVisibilityDescription,
        const FString& StableVisibilityCode,
        const FString& BladeColorDescription,
        const FString& BladeColorCode,
        bool bRequireSaved,
        FString& OutReport)
{
    if (ProfileIndex < 0 ||
        ProfileIndex >= UE_ARRAY_COUNT(GrassAssetNames) ||
        MaterialObjectPath.IsEmpty() ||
        StableVisibilityDescription.IsEmpty() ||
        StableVisibilityCode.IsEmpty() ||
        BladeColorDescription.IsEmpty() ||
        BladeColorCode.IsEmpty())
    {
        OutReport = TEXT("R23B visual-derivative validation requires an exact profile, object path, visibility replacement and blade-colour replacement.");
        return false;
    }

    UMaterial* Source = LoadExact<UMaterial>(
        ObjectPath(GrassAssetNames[ProfileIndex]));
    UMaterial* Target = LoadExact<UMaterial>(MaterialObjectPath);
    FString Error;
    if (!ValidateR23BGrassMaterial(Source, ProfileIndex, Error) ||
        !ValidateR21R22OrR23GrassMaterial(
            Target,
            ProfileIndex,
            23,
            Error,
            true,
            &MaterialObjectPath,
            &StableVisibilityDescription,
            &StableVisibilityCode,
            &BladeColorDescription,
            &BladeColorCode))
    {
        OutReport = TEXT("R23B visual-derivative graph validation failed: ") +
            Error;
        return false;
    }

    const FString TargetPackagePath =
        FPackageName::ObjectPathToPackageName(MaterialObjectPath);
    if (bRequireSaved &&
        (!Source || !Target || !Source->GetOutermost() ||
         !Target->GetOutermost() || Source->GetOutermost()->IsDirty() ||
         Target->GetOutermost()->IsDirty() ||
         !FPackageName::DoesPackageExist(
             PackagePath(GrassAssetNames[ProfileIndex])) ||
         !FPackageName::DoesPackageExist(TargetPackagePath)))
    {
        OutReport = TEXT("R23B visual-derivative validation requires clean disk-backed source and target packages.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("V5D_R23B_VISUAL_DERIVATIVE_MATERIAL_VALID profileIndex=%d exactGraph=true allowedNodeDeltas=visibility,color compiled=true saved=%s"),
        ProfileIndex,
        bRequireSaved ? TEXT("true") : TEXT("not-required"));
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    EnsureGroundVegetationRealismMaterialAssets(FString& OutReport)
{
    TArray<UMaterial*> Materials;
    FString GrassRevision;
    FString Error;
    if (!EnsureCompleteR11MaterialAssets(
            Materials,
            GrassRevision,
            Error))
    {
        OutReport = TEXT("V5D_GROUND_VEGETATION_MATERIALS_FAILED: ") + Error;
        return false;
    }
    if (GrassRevision == TEXT("R23B"))
    {
        OutReport =
            TEXT("V5D_GROUND_VEGETATION_MATERIALS_VALID revision=R23B baseMaterialRevision=R23 calibrationRevision=R23B count=7 coreRevision=R23B coreCount=6 grassRevision=R23B groundOverlayRevision=R23B edgeRevision=R23B validatedGrassRevisionSelection=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK runtimeMaterialRevisionMarker=23 runtimeMaterialCalibrationRevisionMarker=1 grassR23BPhotographicCalibration=true grassProfiles=4 grassExpressionsPerProfile=32 grassTextureSamplesPerProfile=0 grassStableVisibilityInputs=6 grassStableVisibilityMeters=20,28 grassTemporalDitherDisconnectedFromOpacity=true grassDefaultWindStrengthCm=0 grassOptionalWindGraphPreserved=true layeredSoilPreserved=true overlayExpressions=26 overlayBaseInputs=10 exactTextureSamples=4 overlaySpecular=0.15 overlayRoughnessRange=0.72,0.92 nearNormalStrength=0.17 farNormalStrength=0.01 edgeExpressions=26 edgeStableVisibilityInputs=6 mapsSaved=0 sourceV3V4V5BMaterialAssetsUntouched=true globalLightingUntouched=true instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true ") +
            (R23BFinalPinsAreSealed
                ? FString(TEXT("publicationReady=true finalPinsSealed=true"))
                : FString(TEXT("publicationReady=false finalPinsSealed=false bootstrapState=UNSEALED_FINAL_PACKAGE_PINS")));
        return true;
    }
    if (GrassRevision == TEXT("R23"))
    {
        OutReport =
            TEXT("V5D_GROUND_VEGETATION_MATERIALS_VALID revision=R23 count=7 coreRevision=R23 coreCount=6 grassRevision=R23 groundOverlayRevision=R23 edgeRevision=R23 validatedGrassRevisionSelection=R23_FIRST_R22_THEN_R21_THEN_R19_FALLBACK edgeFadeCount=1 edgeFadeMaterial=M_IPV5D_GrassMedium_EdgeFade edgeFadeExpressions=25 edgeFadeTextureSamples=4 edgeFade=StableSpatialVisibility20m28m sourceOpacityMaskOutput=1_red sourceV4NineInputWpo5cmPreserved=true serializedNativeCdoHardReference=true restartBeforeCookRequired=true freshCookManifestAndPackagedLoadRequired=true ")
            TEXT("fineTurfAppearanceProxy=true photographicDepthTurfMaterialResponse=true currentBotanicalOrSpeciesClaim=false grassProfiles=4 grassExpressionsPerProfile=31 grassTextureSamplesPerProfile=0 grassColorInputs=5 grassRoughnessInputs=5 grassSpecularInputs=5 grassNormalInputs=5 grassStableVisibilityInputs=5 grassStableVisibilityMeters=20,28 grassTemporalDitherDisconnectedFromOpacity=true grassDefaultWindStrengthCm=0 runtimeMaterialRevisionMarker=23 grassOptionalWindGraphPreserved=true grassColorDetailFadeMeters=10,24 grassNormalFadeMeters=12,24 grassNormalFarFloor=0.08 materialsEmissiveUntouched=true grassGraphWiringValidated=true soilGraphWiringValidated=true customAuxiliaryStateValidated=true compiledActiveFeatureLevelValidated=true ")
            TEXT("layeredSoilPreserved=true dedicatedLawnOverlay=true overlayExpressions=25 exactTextureSamples=4 overlayTintCameraHeightIndependent=true overlayTintNear=0.78,1.10,0.70 overlayTintFar=0.76,1.02,0.68 overlaySubstrateBreakupMeters=7.3,1.7,0.48 overlayMipBiasMeters=30,55 overlayMipBiasRange=0,1 overlaySpecular=0.18 overlayRoughnessRange=0.68,0.91 nearNormalStrength=0.22 farNormalStrength=0.02 groundCoreMaskFingerprint=V5 opaqueCollarMeters=50 outwardStableWorldDitherFeatherMeters=8 maximumOutsideProviderClipMeters=58 coverageGapAtProviderBoundary=false sourceV3V4V5BMaterialAssetsUntouched=true globalLightingUntouched=true instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true inheritedV5BTurfRuntimeMaterialBinding=ORIGINAL_V5B_UNTOUCHED inheritedV5BTurfRuntimeRendererPolicy=HIDDEN_DURING_V5D_RUNTIME_EXCLUSIVE_OWNERSHIP");
        return true;
    }
    if (GrassRevision == TEXT("R22"))
    {
        OutReport =
            TEXT("V5D_GROUND_VEGETATION_MATERIALS_VALID revision=R22 count=7 coreRevision=R22 coreCount=6 grassRevision=R22 groundOverlayRevision=R22 edgeRevision=R22 validatedGrassRevisionSelection=R22_FIRST_R21_THEN_R19_FALLBACK edgeFadeCount=1 edgeFadeMaterial=M_IPV5D_GrassMedium_EdgeFade edgeFadeExpressions=25 edgeFadeTextureSamples=4 edgeFade=StableSpatialVisibility12m18m sourceOpacityMaskOutput=1_red sourceV4NineInputWpo5cmPreserved=true serializedNativeCdoHardReference=true restartBeforeCookRequired=true freshCookManifestAndPackagedLoadRequired=true ")
            TEXT("fineTurfAppearanceProxy=true hyperrealTurfMaterialResponseCandidate=true currentBotanicalOrSpeciesClaim=false grassProfiles=4 grassExpressionsPerProfile=31 grassTextureSamplesPerProfile=0 grassColorInputs=5 grassRoughnessInputs=5 grassSpecularInputs=5 grassNormalInputs=5 grassStableVisibilityInputs=5 grassStableVisibilityMeters=12,18 grassTemporalDitherDisconnectedFromOpacity=true grassDefaultWindStrengthCm=0 runtimeMaterialRevisionMarker=22 grassOptionalWindGraphPreserved=true materialsEmissiveUntouched=true grassGraphWiringValidated=true soilGraphWiringValidated=true customAuxiliaryStateValidated=true compiledActiveFeatureLevelValidated=true ")
            TEXT("layeredSoilPreserved=true dedicatedLawnOverlay=true overlayExpressions=25 exactTextureSamples=4 overlayTintGroundLevel=1.96625,0.91675,0.56525 overlayTintElevated=0.715,0.965,0.595 overlayTintHeightFadeMeters=50,150 overlayMipBiasMeters=30,55 overlayMipBiasRange=0,1 overlaySpecular=0.14 overlayRoughnessFar=0.88 nearNormalStrength=0.12 farNormalStrength=0 groundCoreMaskFingerprint=V5 opaqueCollarMeters=50 outwardStableWorldDitherFeatherMeters=8 maximumOutsideProviderClipMeters=58 coverageGapAtProviderBoundary=false sourceV3V4V5BMaterialAssetsUntouched=true globalLightingUntouched=true instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true inheritedV5BTurfRuntimeMaterialBinding=ORIGINAL_V5B_UNTOUCHED inheritedV5BTurfRuntimeRendererPolicy=HIDDEN_DURING_V5D_RUNTIME_EXCLUSIVE_OWNERSHIP");
        return true;
    }
    if (GrassRevision == TEXT("R21"))
    {
        OutReport =
            TEXT("V5D_GROUND_VEGETATION_MATERIALS_VALID revision=R21 count=7 coreRevision=R21 coreCount=6 grassRevision=R21 groundOverlayRevision=R18 edgeRevision=R11 validatedGrassRevisionSelection=R21_FIRST_R19_FALLBACK edgeFadeCount=1 edgeFadeMaterial=M_IPV5D_GrassMedium_EdgeFade edgeFadeExpressions=21 edgeFadeTextureSamples=4 edgeFade=DitherTemporalAA_PerInstanceFade sourceOpacityMaskOutput=1_red sourceV4NineInputWpo5cmPreserved=true serializedNativeCdoHardReference=true restartBeforeCookRequired=true freshCookManifestAndPackagedLoadRequired=true ")
            TEXT("fineTurfAppearanceProxy=true calibratedPhotographicTurfMaterialResponse=true currentBotanicalOrSpeciesClaim=false grassProfiles=4 grassExpressionsPerProfile=29 grassTextureSamplesPerProfile=0 grassColorInputs=5 grassRoughnessInputs=5 grassSpecularInputs=5 grassNormalInputs=5 grassMacroFieldsPerProfile=2 grassMacroScaleMeters=13.5,4.7 grassDistanceDetailFadeMeters=5,14 grassNormalFadeMeters=4,12 grassArithmeticHashes=true grassSinCosOperations=0 grassBroadValueRange=0.975,1.025 grassFineValueRange=0.990,1.010 grassRoughnessNear=0.68,0.66,0.72,0.75 grassRoughnessFar=0.74 grassSpecularNear=0.30,0.31,0.28,0.26 grassSpecularFar=0.19 grassNormalNearFar=0.42,0.0 grassR21CalibratedColorNormalAndSurfaceResponse=true materialsEmissiveUntouched=true boundedCalmWind=true grassGraphWiringValidated=true soilGraphWiringValidated=true customAuxiliaryStateValidated=true compiledActiveFeatureLevelValidated=true ")
            TEXT("layeredSoilPreserved=true dedicatedLawnOverlay=true overlayExpressions=23 exactTextureSamples=4 groundCoreMaskFingerprint=V5 opaqueCollarMeters=50 outwardStableWorldDitherFeatherMeters=8 maximumOutsideProviderClipMeters=58 coverageGapAtProviderBoundary=false sourceV3V4V5BMaterialAssetsUntouched=true globalLightingUntouched=true instanceTransformsUntouched=true inheritedV5BTurfRuntimeMaterialBinding=ORIGINAL_V5B_UNTOUCHED inheritedV5BTurfRuntimeRendererPolicy=HIDDEN_DURING_V5D_RUNTIME_EXCLUSIVE_OWNERSHIP");
        return true;
    }
    OutReport =
        TEXT("V5D_GROUND_VEGETATION_MATERIALS_VALID revision=R19 count=7 coreRevision=R19 coreCount=6 grassRevision=R19 groundOverlayRevision=R18 edgeRevision=R11 edgeFadeCount=1 edgeFadeMaterial=M_IPV5D_GrassMedium_EdgeFade edgeFadeExpressions=21 edgeFadeTextureSamples=4 edgeFade=DitherTemporalAA_PerInstanceFade sourceOpacityMaskOutput=1_red sourceV4NineInputWpo5cmPreserved=true serializedNativeCdoHardReference=true restartBeforeCookRequired=true freshCookManifestAndPackagedLoadRequired=true ")
        TEXT("fineTurfAppearanceProxy=true photographicTurfMaterialResponse=true currentBotanicalOrSpeciesClaim=false grassProfiles=4 grassExpressionsPerProfile=29 grassTextureSamplesPerProfile=0 grassColorInputs=5 grassRoughnessInputs=5 grassSpecularInputs=5 grassNormalInputs=5 grassMacroFieldsPerProfile=2 grassMacroScaleMeters=11,3.6 grassDistanceDetailFadeMeters=8,20 grassArithmeticHashes=true grassSinCosOperations=0 grassBroadValueRange=0.970,1.035 grassFineValueRange=0.990,1.015 grassCutStart=0.82 grassCutDesaturation=0.18 grassCutGain=0.95 grassDiffuseGain=1.34,1.18,1.03 grassDiffuseGainLuminance=1.20319344 grassFinalChroma=0.88 dryBladeFractions=0.045,0.030,0.065,0.160 rootThatchMix=0.10,0.08,0.14,0.22 grassRoughnessBase=0.70,0.66,0.72,0.76 grassRoughnessBias=0.035 grassRoughnessMicro=-0.012,0.018 grassRoughnessRootCut=0.025,0.030 grassRoughnessClamp=0.70,0.90 grassSpecularBase=0.30,0.32,0.28,0.26 grassSpecularMultiplier=0.62,0.72 grassSpecularCutAddition=0.003 grassSpecularClamp=0.16,0.23 grassNormalNearFar=0.50,0.03 grassNormalBladeMultiplier=0.96,1.04 grassNormalFadeMeters=8,20 grassSubsurfaceGains=1.20,1.08,0.90;1.18,1.09,0.92;1.23,1.11,0.98;1.15,1.03,0.86 grassSubsurfaceFromBladeColor=true materialsEmissiveUntouched=true boundedCalmWind=true grassGraphWiringValidated=true soilGraphWiringValidated=true customAuxiliaryStateValidated=true compiledActiveFeatureLevelValidated=true ")
        TEXT("layeredSoilPreserved=true dedicatedLawnOverlay=true overlayExpressions=23 clonedV5EightSampleGraph=false exactTextureSamples=4 overlayTextureSet=Grass004 overlayTextureSampleStateValidated=true textureTileMeters=1.4 mowingBandMeters=3.2 mowingAngleDegrees=7 mowingRoughnessAmplitude=0.025 mowingRoughnessPhase=opposesBaseColorGain arithmeticValueHashes=true appearanceFieldPeriodicOperations=1 appearanceFieldPeriodicOperationScope=GroundR10FieldCodeOnly coreMaskTopologyFingerprint=V5 opaqueCollarMeters=50 outwardStableWorldDitherFeatherMeters=8 maximumOutsideProviderClipMeters=58 nearColorDetailPresence=0.82 farColorDetailPresence=0.38 maximumWetLuminanceDarkeningApprox=0.057 wearWarmDesaturated=true overlayTint=0.715,0.965,0.595 overlaySpecular=0.18 overlayRoughnessNear=0.63+0.50xTexture overlayRoughnessClamp=0.70,0.91 nearNormalStrength=0.15 farNormalStrength=0.01 groundCoreMask=irregularEllipse64 coreCenterMeters=0,55 coverageGapAtProviderBoundary=false sourceV3V4V5BMaterialAssetsUntouched=true globalLightingUntouched=true instanceTransformsUntouched=true inheritedV5BTurfRuntimeMaterialBinding=ORIGINAL_V5B_UNTOUCHED inheritedV5BTurfRuntimeRendererPolicy=HIDDEN_DURING_V5D_RUNTIME_EXCLUSIVE_OWNERSHIP");
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismMaterialAssetsToR10(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    bool bAlreadyR10 = false;
    FString Error;
    if (!ValidateR10UpgradeInputMaterialAssets(
            Materials, bAlreadyR10, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr))
    {
        OutReport = TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }
    TArray<FPackageArtifactSnapshot> SoilBefore;
    TArray<FPackageArtifactSnapshot> MapBefore;
    if (!CaptureObjectPackageArtifacts(
            ObjectPath(SoilAssetName), SoilBefore, Error) ||
        !CaptureTargetMapArtifacts(MapBefore, Error))
    {
        OutReport = TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bAlreadyR10)
    {
        if (!PackageArtifactsMatch(SoilBefore, Error) ||
            !PackageArtifactsMatch(MapBefore, Error))
        {
            OutReport = TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R10_GROUND_MATERIALS_ALREADY_VALID targets=5 soilHashPreserved=true mapHashPreserved=true mapsSaved=0 exactTextureSamples=4 fineTurfAppearanceProxy=true currentBotanicalOrSpeciesClaim=false");
        return true;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3], Materials[5]};
    FMaterialUpgradeBackup Backup;
    if (Targets.Num() != 5 || Targets.Contains(nullptr) ||
        !BackUpMaterialPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_REFUSED_BACKUP: ") +
            Error;
        return false;
    }
    const auto FailWithRollback = [
        &OutReport,
        &Backup,
        &SoilBefore,
        &MapBefore](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!PackageArtifactsMatch(SoilBefore, InvariantError) ||
            !PackageArtifactsMatch(MapBefore, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" Soil/map invariant verification failed: ") +
                InvariantError;
        }
        OutReport = Failure +
            (bRolledBack
                ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=5 preReloadHashes=true postReloadHashes=true absentSidecarsRestored=true soilHashPreserved=true mapHashPreserved=true mapsSaved=0 backup=")) +
                    Backup.Directory
                : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) +
                    RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR10GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() != ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(
                TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_GRASS_IN_PLACE: ") +
                Error);
        }
    }
    if (!BuildR10GroundOverlayMaterialGraph(Materials[5], true, Error) ||
        Materials[5] != Targets[4] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 5 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_SAVE: only the exact five material targets were offered to save; maps and soil were not offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    if (!ValidateR10MaterialAssetsInternal(ColdMaterials, Error) ||
        ColdMaterials.Num() != 6)
    {
        return FailWithRollback(
            TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = true;
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    if (!bColdPackagesClean)
    {
        Error = TEXT("One or more reloaded V5D material packages remained dirty after the exact five-target save/reload.");
    }
    if (!bColdPackagesClean ||
        !PackageArtifactsMatch(SoilBefore, Error) ||
        !PackageArtifactsMatch(MapBefore, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    FString HashDeltas;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 6 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
        }
        const FString NewHash = FileMd5(Package.PackageFilename);
        if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
        {
            return FailWithRollback(
                TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_FAILED_DISK_DELTA: every target must acquire a distinct valid canonical package hash."));
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
        }
        HashDeltas += FPaths::GetBaseFilename(Package.PackageFilename) +
            TEXT(":") + Package.Artifacts[0].Md5 + TEXT("->") + NewHash;
    }

    OutReport = FString::Printf(
        TEXT("V5D_R10_GROUND_MATERIAL_UPGRADE_PASS packages=5 allSixInputPackagesValid=true inPlace=true objectPathsStable=true saveTargets=5 reloadTargets=5 coldValidatedMaterials=6 coldPackagesClean=true grassGraphWiringValidated=true soilGraphWiringValidated=true customAuxiliaryStateValidated=true compiledActiveFeatureLevelValidated=true fullArtifactRoster=true absentSidecarsRecorded=true soilHashPreserved=true mapHashPreserved=true mapsSaved=0 preReloadRollbackHashes=true postReloadRollbackHashes=true exactTextureSamples=4 overlayTextureSampleStateValidated=true grassArithmeticHashes=true grassPeriodicOperations=0 arithmeticValueHashes=true appearanceFieldPeriodicOperations=1 appearanceFieldPeriodicOperationScope=GroundR10FieldCodeOnly coreMaskTopologyFingerprint=V4 mowingRoughnessAmplitude=0.025 mowingRoughnessPhase=opposesBaseColorGain shadeLiftLuminanceApprox=1.087 maximumWetLuminanceDarkeningApprox=0.057 wetCoolShift=true maximumWetRoughnessReduction=0.06 wearWarmDesaturated=true fineTurfAppearanceProxy=true currentBotanicalOrSpeciesClaim=false backup=%s deltas=%s"),
        *Backup.Directory,
        *HashDeltas);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismMaterialAssetsToR12(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR12 = false;
    FString Error;
    if (!ValidateR12UpgradeInputMaterialAssets(
            Materials,
            EdgeFadeMaterial,
            bAlreadyR12,
            Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }
    TArray<FPackageArtifactSnapshot> SoilBefore;
    TArray<FPackageArtifactSnapshot> EdgeBefore;
    TArray<FPackageArtifactSnapshot> MapBefore;
    if (!CaptureObjectPackageArtifacts(
            ObjectPath(SoilAssetName), SoilBefore, Error) ||
        !CaptureObjectPackageArtifacts(
            ObjectPath(EdgeGrassFadeAssetName), EdgeBefore, Error) ||
        !CaptureTargetMapArtifacts(MapBefore, Error))
    {
        OutReport = TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bAlreadyR12)
    {
        if (!PackageArtifactsMatch(SoilBefore, Error) ||
            !PackageArtifactsMatch(EdgeBefore, Error) ||
            !PackageArtifactsMatch(MapBefore, Error))
        {
            OutReport = TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R12_GROUND_MATERIALS_ALREADY_VALID targets=5 soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 exactTextureSamples=4 fineTurfAppearanceProxy=true currentBotanicalOrSpeciesClaim=false");
        return true;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3], Materials[5]};
    FMaterialUpgradeBackup Backup;
    if (Targets.Num() != 5 || Targets.Contains(nullptr) ||
        !BackUpR12MaterialPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_REFUSED_BACKUP: ") +
            Error;
        return false;
    }
    const auto FailWithRollback = [
        &OutReport,
        &Backup,
        &SoilBefore,
        &EdgeBefore,
        &MapBefore](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!PackageArtifactsMatch(SoilBefore, InvariantError) ||
            !PackageArtifactsMatch(EdgeBefore, InvariantError) ||
            !PackageArtifactsMatch(MapBefore, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" Soil/edge/map invariant verification failed: ") +
                InvariantError;
        }
        OutReport = Failure +
            (bRolledBack
                ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=5 preReloadHashes=true postReloadHashes=true absentSidecarsRestored=true soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 backup=")) +
                    Backup.Directory
                : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) +
                    RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR12GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() != ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(
                TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_GRASS_IN_PLACE: ") +
                Error);
        }
    }
    if (!BuildR12GroundOverlayMaterialGraph(Materials[5], true, Error) ||
        Materials[5] != Targets[4] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 5 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_SAVE: only the exact five material targets were offered to save; maps, soil and edge fade were not offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR12MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || !ColdEdgeFade)
    {
        return FailWithRollback(
            TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    if (!bColdPackagesClean)
    {
        Error = TEXT("One or more reloaded V5D R12 material packages or the preserved edge package remained dirty after the exact five-target save/reload.");
    }
    if (!bColdPackagesClean ||
        !PackageArtifactsMatch(SoilBefore, Error) ||
        !PackageArtifactsMatch(EdgeBefore, Error) ||
        !PackageArtifactsMatch(MapBefore, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    FString HashDeltas;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 6 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
        }
        const FString NewHash = FileMd5(Package.PackageFilename);
        if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
        {
            return FailWithRollback(
                TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_FAILED_DISK_DELTA: every target must acquire a distinct valid canonical package hash."));
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
        }
        HashDeltas += FPaths::GetBaseFilename(Package.PackageFilename) +
            TEXT(":") + Package.Artifacts[0].Md5 + TEXT("->") + NewHash;
    }

    OutReport = FString::Printf(
        TEXT("V5D_R12_GROUND_MATERIAL_UPGRADE_PASS packages=5 allSevenInputPackagesValid=true uniformR10Input=true inPlace=true objectPathsStable=true saveTargets=5 reloadTargets=5 coldValidatedMaterials=7 coldPackagesClean=true grassGraphWiringValidated=true soilGraphWiringValidated=true edgeFadeGraphWiringValidated=true customAuxiliaryStateValidated=true compiledActiveFeatureLevelValidated=true fullArtifactRoster=true absentSidecarsRecorded=true soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 preReloadRollbackHashes=true postReloadRollbackHashes=true exactTextureSamples=4 overlayTextureSampleStateValidated=true grassArithmeticHashes=true grassPeriodicOperations=0 grassRoughness=0.70,0.66,0.72,0.76 grassSpecular=0.30,0.32,0.28,0.26 grassSubsurfaceGreen=0.195,0.189,0.174,0.164 nearColorDetailPresence=0.82 farColorDetailPresence=0.38 overlayTint=1.085,1.060,1.075 overlaySpecular=0.28 overlayRoughnessBias=0.06 overlayRoughnessClamp=0.70,0.93 nearNormalStrength=0.24 farNormalStrength=0.03 sourceV4V5BMaterialAssetsUntouched=true fineTurfAppearanceProxy=true currentBotanicalOrSpeciesClaim=false backup=%s deltas=%s"),
        *Backup.Directory,
        *HashDeltas);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismMaterialAssetsToR13(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR13 = false;
    FString Error;
    if (!ValidateR13UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR13, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }
    TArray<FPackageArtifactSnapshot> SoilBefore;
    TArray<FPackageArtifactSnapshot> EdgeBefore;
    TArray<FPackageArtifactSnapshot> MapBefore;
    if (!CaptureObjectPackageArtifacts(
            ObjectPath(SoilAssetName), SoilBefore, Error) ||
        !CaptureObjectPackageArtifacts(
            ObjectPath(EdgeGrassFadeAssetName), EdgeBefore, Error) ||
        !CaptureTargetMapArtifacts(MapBefore, Error))
    {
        OutReport = TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bAlreadyR13)
    {
        if (!PackageArtifactsMatch(SoilBefore, Error) ||
            !PackageArtifactsMatch(EdgeBefore, Error) ||
            !PackageArtifactsMatch(MapBefore, Error))
        {
            OutReport = TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R13_GROUND_MATERIALS_ALREADY_VALID targets=5 soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 exactTextureSamples=4 grass004=true");
        return true;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3], Materials[5]};
    FMaterialUpgradeBackup Backup;
    if (Targets.Num() != 5 || Targets.Contains(nullptr) ||
        !BackUpR13MaterialPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_REFUSED_BACKUP: ") +
            Error;
        return false;
    }
    const auto FailWithRollback = [
        &OutReport,
        &Backup,
        &SoilBefore,
        &EdgeBefore,
        &MapBefore](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!PackageArtifactsMatch(SoilBefore, InvariantError) ||
            !PackageArtifactsMatch(EdgeBefore, InvariantError) ||
            !PackageArtifactsMatch(MapBefore, InvariantError))
        {
            bRolledBack = false;
            RollbackError +=
                TEXT(" Soil/edge/map invariant verification failed: ") +
                InvariantError;
        }
        OutReport = Failure +
            (bRolledBack
                ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=5 preReloadHashes=true postReloadHashes=true absentSidecarsRestored=true soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 backup=")) +
                    Backup.Directory
                : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) +
                    RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureGrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() !=
                ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(
                TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_GRASS_IN_PLACE: ") +
                Error);
        }
    }
    if (!BuildGroundOverlayMaterialGraph(Materials[5], true, Error) ||
        Materials[5] != Targets[4] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 5 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_SAVE: only the exact five R13 targets were offered to save; map, soil and edge fade were not offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_RELOAD: ") +
            Error);
    }
    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR13MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || !ColdEdgeFade)
    {
        return FailWithRollback(
            TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    if (!bColdPackagesClean ||
        !PackageArtifactsMatch(SoilBefore, Error) ||
        !PackageArtifactsMatch(EdgeBefore, Error) ||
        !PackageArtifactsMatch(MapBefore, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    FString HashDeltas;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 6 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
        }
        const FString NewHash = FileMd5(Package.PackageFilename);
        if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
        {
            return FailWithRollback(
                TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_FAILED_DISK_DELTA: every target must acquire a distinct valid canonical package hash."));
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
        }
        HashDeltas += FPaths::GetBaseFilename(Package.PackageFilename) +
            TEXT(":") + Package.Artifacts[0].Md5 + TEXT("->") + NewHash;
    }
    OutReport = FString::Printf(
        TEXT("V5D_R13_GROUND_MATERIAL_UPGRADE_PASS packages=5 allSevenInputPackagesValid=true uniformR12Input=true inPlace=true objectPathsStable=true saveTargets=5 reloadTargets=5 coldValidatedMaterials=7 coldPackagesClean=true grassExpressions=29 grass004Textures=4 soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 collisionPreserved=true navigationPreserved=true rfAuthorityPreserved=true sourceV4V5BMaterialAssetsUntouched=true backup=%s deltas=%s"),
        *Backup.Directory,
        *HashDeltas);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismMaterialAssetsToR15(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR15 = false;
    FString Error;
    if (!ValidateR15UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR15, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }
    TArray<FPackageArtifactSnapshot> SoilBefore;
    TArray<FPackageArtifactSnapshot> EdgeBefore;
    TArray<FPackageArtifactSnapshot> MapBefore;
    if (!CaptureObjectPackageArtifacts(
            ObjectPath(SoilAssetName), SoilBefore, Error) ||
        !CaptureObjectPackageArtifacts(
            ObjectPath(EdgeGrassFadeAssetName), EdgeBefore, Error) ||
        !CaptureTargetMapArtifacts(MapBefore, Error))
    {
        OutReport = TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bAlreadyR15)
    {
        if (!PackageArtifactsMatch(SoilBefore, Error) ||
            !PackageArtifactsMatch(EdgeBefore, Error) ||
            !PackageArtifactsMatch(MapBefore, Error))
        {
            OutReport = TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R15_GROUND_MATERIALS_ALREADY_VALID targets=5 soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 grassExpressions=29 grassTextureSamples=0 overlayExpressions=23 grass004Textures=4");
        return true;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3], Materials[5]};
    FMaterialUpgradeBackup Backup;
    if (Targets.Num() != 5 || Targets.Contains(nullptr) ||
        !BackUpR15MaterialPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_REFUSED_BACKUP: ") +
            Error;
        return false;
    }
    const auto FailWithRollback = [
        &OutReport,
        &Backup,
        &SoilBefore,
        &EdgeBefore,
        &MapBefore](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!PackageArtifactsMatch(SoilBefore, InvariantError) ||
            !PackageArtifactsMatch(EdgeBefore, InvariantError) ||
            !PackageArtifactsMatch(MapBefore, InvariantError))
        {
            bRolledBack = false;
            RollbackError +=
                TEXT(" Soil/edge/map invariant verification failed: ") +
                InvariantError;
        }
        OutReport = Failure +
            (bRolledBack
                ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=5 preReloadHashes=true postReloadHashes=true absentSidecarsRestored=true soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 backup=")) +
                    Backup.Directory
                : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) +
                    RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR15GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() !=
                ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(
                TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_GRASS_IN_PLACE: ") +
                Error);
        }
    }
    if (!BuildR15GroundOverlayMaterialGraph(Materials[5], true, Error) ||
        Materials[5] != Targets[4] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 5 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_SAVE: only the exact five R15 targets were offered to save; map, soil and edge fade were not offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_RELOAD: ") +
            Error);
    }
    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR15MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || !ColdEdgeFade)
    {
        return FailWithRollback(
            TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    if (!bColdPackagesClean ||
        !PackageArtifactsMatch(SoilBefore, Error) ||
        !PackageArtifactsMatch(EdgeBefore, Error) ||
        !PackageArtifactsMatch(MapBefore, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    FString HashDeltas;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 6 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
        }
        const FString NewHash = FileMd5(Package.PackageFilename);
        if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
        {
            return FailWithRollback(
                TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_FAILED_DISK_DELTA: every target must acquire a distinct valid canonical package hash."));
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
        }
        HashDeltas += FPaths::GetBaseFilename(Package.PackageFilename) +
            TEXT(":") + Package.Artifacts[0].Md5 + TEXT("->") + NewHash;
    }
    OutReport = FString::Printf(
        TEXT("V5D_R15_GROUND_MATERIAL_UPGRADE_PASS packages=5 allSevenInputPackagesValid=true uniformR13Input=true inPlace=true objectPathsStable=true saveTargets=5 reloadTargets=5 coldValidatedMaterials=7 coldPackagesClean=true grassExpressions=29 grassTextureSamples=0 grassNormalInputs=5 grassDiffuseGain=1.10,1.16,1.08 grassRoughnessClamp=0.56,0.80 grassSpecularClamp=0.24,0.40 grassNormalNearFar=0.82,0.32 grassNormalBladeMultiplier=0.86,1.14 overlayExpressions=23 grass004Textures=4 overlayTint=0.640,0.805,0.765 overlayNormalNearFar=0.20,0.02 overlaySpecular=0.22 soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true mapsSaved=0 collisionPreserved=true navigationPreserved=true rfAuthorityPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true globalLightingUntouched=true materialsEmissiveUntouched=true instanceTransformsUntouched=true backup=%s deltas=%s"),
        *Backup.Directory,
        *HashDeltas);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismLawnOverlayToR16(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR16 = false;
    FString Error;
    if (!ValidateR16UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR16, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR16PreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bAlreadyR16)
    {
        if (!R16PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
        {
            OutReport = TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R16_LAWN_OVERLAY_ALREADY_VALID targets=1 saveTargets=0 reloadTargets=0 immutablePackageHashesPreserved=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true");
        return true;
    }

    TArray<UMaterial*> Targets = {Materials[5]};
    FMaterialUpgradeBackup Backup;
    if (Targets.Num() != 1 || Targets.Contains(nullptr) ||
        !BackUpR16LawnOverlayPackage(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_REFUSED_BACKUP: ") +
            Error;
        return false;
    }
    const auto FailWithRollback = [&OutReport, &Backup, &PreservedSnapshots](
                                      const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack = RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R16PreservedPackageSnapshotsMatch(
                PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" Immutable grass/soil/edge/map invariant verification failed: ") +
                InvariantError;
        }
        OutReport = Failure +
            (bRolledBack
                ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=1 preReloadHashes=true postReloadHashes=true absentSidecarsRestored=true immutablePackageHashesPreserved=true mapsSaved=0 backup=")) +
                    Backup.Directory
                : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) +
                    RollbackError);
        return false;
    };

    if (!ConfigureR16GroundOverlayMaskInPlace(Materials[5], Error) ||
        Materials[5] != Targets[0] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_FAILED_IN_PLACE: ") + Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets = {Targets[0]};
    if (ExactSaveTargets.Num() != 1 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_FAILED_SAVE: only M_IPV5D_LawnMacroVariation was offered to save; grass, soil, edge fade and map were not offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR16MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || !ColdEdgeFade)
    {
        return FailWithRollback(
            TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_FAILED_COLD_VALIDATION: ") + Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    if (!bColdPackagesClean ||
        !R16PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_FAILED_COLD_VALIDATION: ") + Error);
    }

    if (Backup.ExpectedPackageCount != 1 || Backup.Packages.Num() != 1 ||
        Backup.Packages[0].Artifacts.Num() != 6 ||
        !Backup.Packages[0].Artifacts[0].bPresent)
    {
        return FailWithRollback(
            TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
    }
    const FMaterialPackageBackup& Package = Backup.Packages[0];
    const FString NewHash = FileMd5(Package.PackageFilename);
    if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
    {
        return FailWithRollback(
            TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_FAILED_DISK_DELTA: M_IPV5D_LawnMacroVariation did not acquire a distinct canonical package hash."));
    }
    OutReport = FString::Printf(
        TEXT("V5D_R16_LAWN_OVERLAY_UPGRADE_PASS packages=1 exactOnePackageAtomicTransaction=true allSevenInputPackagesValid=true uniformR15Input=true inPlace=true objectPathsStable=true saveTargets=1 reloadTargets=1 coldValidatedMaterials=7 coldPackagesClean=true opaqueCollarMeters=50 outwardFeatherMeters=8 maximumOutsideProviderClipMeters=58 providerBoundaryCoverage=1 collarBoundaryCoverage=1 midFeatherCoverage=0.5 immutableGrassSoilEdgeAndMapHashesPreserved=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true globalLightingUntouched=true materialsEmissiveUntouched=true instanceTransformsUntouched=true backup=%s delta=%s:%s->%s"),
        *Backup.Directory,
        *FPaths::GetBaseFilename(Package.PackageFilename),
        *Package.Artifacts[0].Md5,
        *NewHash);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismMaterialAssetsToR17(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets = GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }
    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR17 = false;
    FString Error;
    if (!ValidateR17UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR17, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) || !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_REFUSED_ROSTER: ") + Error;
        return false;
    }
    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR17PreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_REFUSED_INVARIANTS: ") + Error;
        return false;
    }
    if (bAlreadyR17)
    {
        if (!R17PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
        {
            OutReport = TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") + Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R17_GROUND_MATERIALS_ALREADY_VALID targets=5 saveTargets=0 reloadTargets=0 immutablePackageHashesPreserved=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true");
        return true;
    }
    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3], Materials[5]};
    FMaterialUpgradeBackup Backup;
    if (Targets.Num() != 5 || Targets.Contains(nullptr) ||
        !BackUpR17GroundMaterialPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_REFUSED_BACKUP: ") + Error;
        return false;
    }
    const auto FailWithRollback = [&OutReport, &Backup, &PreservedSnapshots](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack = RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R17PreservedPackageSnapshotsMatch(PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R17 immutable invariant verification failed: ") + InvariantError;
        }
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=5 immutablePackageHashesPreserved=true mapsSaved=0 backup=")) + Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError);
        return false;
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR17GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] || Materials[Index]->GetPathName() != ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_FAILED_GRASS_IN_PLACE: ") + Error);
        }
    }
    if (!ConfigureR17GroundOverlayPhotographicResponseInPlace(Materials[5], Error) ||
        Materials[5] != Targets[4] || Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") + Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets) { ExactSaveTargets.Add(Target); }
    if (ExactSaveTargets.Num() != 5 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_FAILED_SAVE: only the exact five R17 grass and lawn-overlay packages were offered to save."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_FAILED_RELOAD: ") + Error);
    }
    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdge = nullptr;
    if (!ValidateCompleteR17MaterialAssets(ColdMaterials, ColdEdge, Error) ||
        ColdMaterials.Num() != 6 || !ColdEdge || !R17PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") + Error);
    }
    FString HashDeltas;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 6 || !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
        }
        const FString NewHash = FileMd5(Package.PackageFilename);
        if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
        {
            return FailWithRollback(TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_FAILED_DISK_DELTA: every R17 target must acquire a distinct canonical package hash."));
        }
        if (!HashDeltas.IsEmpty()) { HashDeltas += TEXT(","); }
        HashDeltas += FPaths::GetBaseFilename(Package.PackageFilename) + TEXT(":") + Package.Artifacts[0].Md5 + TEXT("->") + NewHash;
    }
    OutReport = FString::Printf(
        TEXT("V5D_R17_GROUND_MATERIAL_UPGRADE_PASS packages=5 allSevenInputPackagesValid=true uniformR16Input=true inPlace=true objectPathsStable=true saveTargets=5 reloadTargets=5 coldValidatedMaterials=7 coldPackagesClean=true grassExpressions=29 grassTextureSamples=0 grassNormalInputs=5 grassDiffuseGain=1.16,1.18,1.02 grassRoughnessBias=-0.020 grassRoughnessMicro=-0.035,0.040 grassRoughnessRootCut=0.020,0.015 grassRoughnessClamp=0.63,0.84 grassSpecularMultiplier=0.86,1.08 grassSpecularCutAddition=0.015 grassSpecularClamp=0.20,0.33 grassNormalNearFar=0.66,0.20 grassNormalBladeMultiplier=0.90,1.10 overlayExpressions=23 grass004Textures=4 overlayTint=0.700,0.900,0.620 overlayNormalNearFar=0.15,0.01 overlaySpecular=0.18 opaqueCollarMeters=50 outwardFeatherMeters=8 maximumOutsideProviderClipMeters=58 soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true materialsEmissiveUntouched=true instanceTransformsUntouched=true backup=%s deltas=%s"),
        *Backup.Directory, *HashDeltas);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismLawnOverlayToR18(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR18 = false;
    FString Error;
    if (!ValidateR18UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR18, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR18PreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bAlreadyR18)
    {
        if (!R18PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
        {
            OutReport = TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R18_LAWN_OVERLAY_ALREADY_VALID targets=1 saveTargets=0 reloadTargets=0 fourR17GrassHashesPreserved=true immutablePackageHashesPreserved=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true");
        return true;
    }

    TArray<UMaterial*> Targets = {Materials[5]};
    FMaterialUpgradeBackup Backup;
    if (Targets.Num() != 1 || Targets.Contains(nullptr) ||
        !BackUpR18LawnOverlayPackage(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_REFUSED_BACKUP: ") +
            Error;
        return false;
    }
    const auto FailWithRollback =
        [&OutReport, &Backup, &PreservedSnapshots](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R18PreservedPackageSnapshotsMatch(
                PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R18 immutable invariant verification failed: ") +
                InvariantError;
        }
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=1 preReloadHashes=true postReloadHashes=true absentSidecarsRestored=true fourR17GrassHashesPreserved=true immutablePackageHashesPreserved=true mapsSaved=0 backup=")) +
                Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError);
        return false;
    };

    if (!ConfigureR18GroundOverlayTintInPlace(Materials[5], Error) ||
        Materials[5] != Targets[0] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_FAILED_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets = {Targets[0]};
    if (ExactSaveTargets.Num() != 1 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_FAILED_SAVE: only M_IPV5D_LawnMacroVariation was offered to save; R17 grass, soil, edge fade, sources, textures, and map were not offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR18MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || !ColdEdgeFade ||
        !R18PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    if (!bColdPackagesClean)
    {
        return FailWithRollback(
            TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_FAILED_COLD_VALIDATION: one or more of the seven exact materials remained dirty."));
    }

    if (Backup.ExpectedPackageCount != 1 || Backup.Packages.Num() != 1 ||
        Backup.Packages[0].Artifacts.Num() != 6 ||
        !Backup.Packages[0].Artifacts[0].bPresent)
    {
        return FailWithRollback(
            TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
    }
    const FMaterialPackageBackup& Package = Backup.Packages[0];
    const FString NewHash = FileMd5(Package.PackageFilename);
    if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
    {
        return FailWithRollback(
            TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_FAILED_DISK_DELTA: M_IPV5D_LawnMacroVariation did not acquire a distinct canonical package hash."));
    }
    OutReport = FString::Printf(
        TEXT("V5D_R18_LAWN_OVERLAY_UPGRADE_PASS packages=1 exactOnePackageAtomicTransaction=true allSevenInputPackagesValid=true uniformR17Input=true inPlace=true objectPathsStable=true saveTargets=1 reloadTargets=1 coldValidatedMaterials=7 coldPackagesClean=true fourR17GrassHashesPreserved=true overlayExpressions=23 grass004Textures=4 overlayTint=0.715,0.965,0.595 overlayNormalNearFar=0.15,0.01 overlaySpecular=0.18 r17RoughnessAndNormalResponsePreservedExactly=true r16CoreMaskTopologyPreservedExactly=true opaqueCollarMeters=50 outwardFeatherMeters=8 maximumOutsideProviderClipMeters=58 soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true materialsEmissiveUntouched=true instanceTransformsUntouched=true backup=%s delta=%s:%s->%s"),
        *Backup.Directory,
        *FPaths::GetBaseFilename(Package.PackageFilename),
        *Package.Artifacts[0].Md5,
        *NewHash);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismGrassMaterialsToR19(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR19 = false;
    FString Error;
    if (!ValidateR19UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR19, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR19PreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bAlreadyR19)
    {
        if (!R19PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
        {
            OutReport = TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport = TEXT("IDEMPOTENT_V5D_R19_GRASS_MATERIALS_ALREADY_VALID targets=4 saveTargets=0 reloadTargets=0 overlayR18HashPreserved=true r18OverlayHashPreserved=true immutablePackageHashesPreserved=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true");
        return true;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3]};
    bool bExactTargetRoster = Targets.Num() ==
        UE_ARRAY_COUNT(GrassAssetNames) && !Targets.Contains(nullptr);
    for (int32 Index = 0;
         bExactTargetRoster && Index < UE_ARRAY_COUNT(GrassAssetNames);
         ++Index)
    {
        bExactTargetRoster &=
            Targets[Index]->GetPathName() == ObjectPath(GrassAssetNames[Index]);
    }
    FMaterialUpgradeBackup Backup;
    if (!bExactTargetRoster ||
        !BackUpR19GrassMaterialPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_REFUSED_BACKUP: the target roster must be exactly the four ordered V5D grass packages. ") +
            Error;
        return false;
    }
    const auto FailWithRollback =
        [&OutReport, &Backup, &PreservedSnapshots](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R19PreservedPackageSnapshotsMatch(
                PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R19 immutable invariant verification failed: ") +
                InvariantError;
        }
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=4 preReloadHashes=true postReloadHashes=true absentSidecarsRestored=true overlayR18HashPreserved=true r18OverlayHashPreserved=true immutablePackageHashesPreserved=true mapsSaved=0 backup=")) +
                Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR19GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() !=
                ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(
                FString::Printf(
                    TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_IN_PLACE: grass profile %d did not retain its exact object identity and R19 graph. "),
                    Index) + Error);
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 4 ||
        ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_SAVE: only the exact four V5D grass packages were offered to save; soil, R18 overlay, edge fade, sources, textures, and map were not offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR19MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || ColdMaterials.Contains(nullptr) ||
        !ColdEdgeFade ||
        !R19PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    if (!bColdPackagesClean)
    {
        return FailWithRollback(
            TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: one or more of the seven exact materials remained dirty."));
    }

    if (Backup.ExpectedPackageCount != 4 || Backup.Packages.Num() != 4)
    {
        return FailWithRollback(
            TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: the exact four-package backup roster is incomplete."));
    }
    FString HashDeltas;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 6 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
        }
        const FString NewHash = FileMd5(Package.PackageFilename);
        if (NewHash.IsEmpty() || NewHash == Package.Artifacts[0].Md5)
        {
            return FailWithRollback(
                TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_FAILED_DISK_DELTA: every R19 grass target must acquire a distinct canonical package hash."));
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
        }
        HashDeltas += FPaths::GetBaseFilename(Package.PackageFilename) +
            TEXT(":") + Package.Artifacts[0].Md5 + TEXT("->") + NewHash;
    }
    OutReport = FString::Printf(
        TEXT("V5D_R19_GRASS_MATERIAL_UPGRADE_PASS packages=4 exactFourPackageAtomicTransaction=true allSevenInputPackagesValid=true uniformR18Input=true uniformR17GrassWithR18OverlayInput=true inPlace=true objectPathsStable=true saveTargets=4 reloadTargets=4 coldValidatedMaterials=7 coldPackagesClean=true grassExpressions=29 grassTextureSamples=0 grassColorInputs=5 grassRoughnessInputs=5 grassSpecularInputs=5 grassNormalInputs=5 grassMacroFields=2 grassMacroScaleMeters=11,3.6 grassDetailFadeMeters=8,20 grassDetailFadeCm=800,2000 grassArithmeticHashes=true grassSinCosOperations=0 grassBroadValueRange=0.970,1.035 grassFineValueRange=0.990,1.015 grassCutStart=0.82 grassCutDesaturation=0.18 grassCutGain=0.95 grassDiffuseGain=1.34,1.18,1.03 grassDiffuseGainLuminance=1.20319344 grassFinalChroma=0.88 dryBladeFractions=0.045,0.030,0.065,0.160 rootThatchMix=0.10,0.08,0.14,0.22 grassRoughnessBias=0.035 grassRoughnessMicro=-0.012,0.018 grassRoughnessRootCut=0.025,0.030 grassRoughnessClamp=0.70,0.90 grassSpecularMultiplier=0.62,0.72 grassSpecularCutAddition=0.003 grassSpecularClamp=0.16,0.23 grassNormalNearFar=0.50,0.03 grassNormalBladeMultiplier=0.96,1.04 grassSubsurfaceGains=1.20,1.08,0.90;1.18,1.09,0.92;1.23,1.11,0.98;1.15,1.03,0.86 overlayR18HashPreserved=true r18OverlayHashPreserved=true soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true immutablePackageHashesPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true materialsEmissiveUntouched=true instanceTransformsUntouched=true windCodePreservedExactly=true backup=%s deltas=%s"),
        *Backup.Directory,
        *HashDeltas);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationSerializedPresentationToR20(FString& OutReport)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutReport = TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_EDITOR_STATE: a non-PIE editor is required.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    if (!World || !MapPackage ||
        MapPackage->GetName() != TargetMapPackage ||
        MapPackage->IsDirty())
    {
        OutReport = TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_MAP: load the exact clean /Game/Maps/Istana_PublicView_Explore_v5d_hybrid map first.");
        return false;
    }

    TArray<UMaterial*> R19Materials;
    UMaterial* R11EdgeFade = nullptr;
    FString Error;
    if (!ValidateCompleteR19MaterialAssets(
            R19Materials,
            R11EdgeFade,
            Error) ||
        R19Materials.Num() != 6 ||
        R19Materials.Contains(nullptr) ||
        !R11EdgeFade)
    {
        OutReport = TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_R19_MATERIALS: the exact clean seven-material R19/R18/R11 roster is required. ") +
            Error;
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> ImmutableSnapshots;
    if (!CaptureR20ImmutablePackageSnapshots(
            ImmutableSnapshots,
            Error))
    {
        OutReport = TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_IMMUTABLE_ROSTER: ") +
            Error;
        return false;
    }

    int32 GroundActorCount = 0;
    ATRIADIstanaExploreV5DGroundVegetationActor* Actor =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            World,
            GroundActorCount);
    if (GroundActorCount != 1 || !Actor ||
        Actor->GetClass() !=
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() ||
        !Actor->Tags.Contains(GroundVegetationTag))
    {
        OutReport = FString::Printf(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_ACTOR: expected exactly one tagged exact-class ground actor; found=%d exactClass=%s tagged=%s."),
            GroundActorCount,
            Actor && Actor->GetClass() ==
                    ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass()
                ? TEXT("true")
                : TEXT("false"),
            Actor && Actor->Tags.Contains(GroundVegetationTag)
                ? TEXT("true")
                : TEXT("false"));
        return false;
    }

    FString MapFilename;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &MapFilename))
    {
        OutReport = TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_DISK_MAP: the exact target map artifact is absent.");
        return false;
    }
    MapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
    FPaths::NormalizeFilename(MapFilename);
    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            MapFilename,
            CurrentSha256,
            CurrentBytes,
            Error))
    {
        OutReport = TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_DISK_HASH: ") +
            Error;
        return false;
    }

    FString CurrentR20Report;
    if (Actor->ValidateR20PredecessorForR23Migration(CurrentR20Report))
    {
        const bool bFinalHashPinned = R20FinalMapBytes > 0 &&
            !R20FinalMapSha256.IsEmpty();
        if (!bFinalHashPinned)
        {
            OutReport = FString::Printf(
                TEXT("V5D_R20_GRASS_LAYOUT_IDEMPOTENCE_REFUSED_UNPINNED_FINAL_HASH measuredFinalBytes=%lld measuredFinalSha256=%s finalPinRequired=true actor={%s}"),
                CurrentBytes,
                *CurrentSha256,
                *CurrentR20Report);
            return false;
        }
        if (CurrentBytes != R20FinalMapBytes ||
            CurrentSha256 != R20FinalMapSha256 ||
            !R20ImmutablePackageSnapshotsMatch(
                ImmutableSnapshots,
                Error))
        {
            OutReport = FString::Printf(
                TEXT("V5D_R20_GRASS_LAYOUT_IDEMPOTENCE_REFUSED_HASH_OR_INVARIANT_DRIFT expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s immutable={%s}"),
                R20FinalMapBytes,
                CurrentBytes,
                *R20FinalMapSha256,
                *CurrentSha256,
                *Error);
            return false;
        }
        OutReport = FString::Printf(
            TEXT("IDEMPOTENT_V5D_R20_GRASS_LAYOUT_ALREADY_VALID mapBytes=%lld mapSha256=%s actorRevision=%d saveTargets=0 mapsSaved=0 coldValidated=true immutableMaterialSourceTextureArtifacts=14 immutablePackageHashesPreserved=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true actor={%s}"),
            CurrentBytes,
            *CurrentSha256,
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedR23PredecessorGrassPresentationRevision(),
            *CurrentR20Report);
        return true;
    }

    FString PredecessorReport;
    if (!Actor->ValidateR14PredecessorForR20Migration(
            PredecessorReport) ||
        CurrentBytes != R20PredecessorMapBytes ||
        CurrentSha256 != R20PredecessorMapSha256)
    {
        OutReport = FString::Printf(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_PREDECESSOR expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s currentR20={%s} predecessor={%s}"),
            R20PredecessorMapBytes,
            CurrentBytes,
            *R20PredecessorMapSha256,
            *CurrentSha256,
            *CurrentR20Report,
            *PredecessorReport);
        return false;
    }

    FR20MapBackup Backup;
    if (!CreateVerifiedR20MapBackup(Backup, Error))
    {
        OutReport = TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }

    const auto ColdRestoreExactPredecessor =
        [&Backup, &ImmutableSnapshots](FString& OutRollbackReport) -> bool
    {
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (CurrentPackage &&
            CurrentPackage->GetName() == TargetMapPackage)
        {
            CurrentPackage->SetDirtyFlag(false);
        }

        FString UnloadFilename;
        UWorld* UnloadWorld =
            FPackageName::DoesPackageExist(
                R20UnloadMapPackage,
                &UnloadFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(UnloadFilename)
            : nullptr;
        if (!UnloadWorld || !UnloadWorld->GetOutermost() ||
            UnloadWorld->GetOutermost()->GetName() !=
                R20UnloadMapPackage)
        {
            OutRollbackReport = TEXT("rollback could not unload the target through exact /Game/Maps/Istana_PublicView_Explore_v5b; no disk copy was attempted and the sealed backup remains authoritative.");
            return false;
        }

        FString RestoreError;
        if (!RestoreR20MapArtifactsOnDisk(Backup, RestoreError))
        {
            OutRollbackReport = TEXT("rollback unloaded the target but failed exact six-artifact restore: ") +
                RestoreError + TEXT(" backup=") + Backup.Directory;
            return false;
        }

        UWorld* RestoredWorld =
            UEditorLoadingAndSavingUtils::LoadMap(Backup.PackageFilename);
        if (RestoredWorld)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage = RestoredWorld
            ? RestoredWorld->GetOutermost()
            : nullptr;
        int32 RestoredActorCount = 0;
        ATRIADIstanaExploreV5DGroundVegetationActor* RestoredActor =
            RestoredWorld
            ? FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
                  RestoredWorld,
                  RestoredActorCount)
            : nullptr;
        FString RestoredPredecessorReport;
        const bool bRestoredActorValid =
            RestoredActorCount == 1 && RestoredActor &&
            RestoredActor->GetClass() ==
                ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() &&
            RestoredActor->Tags.Contains(GroundVegetationTag) &&
            RestoredActor->ValidateR14PredecessorForR20Migration(
                RestoredPredecessorReport);
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bRestoredHash = HashFileSha256R20(
            Backup.PackageFilename,
            RestoredSha256,
            RestoredBytes,
            HashError) &&
            RestoredBytes == R20PredecessorMapBytes &&
            RestoredSha256 == R20PredecessorMapSha256;
        TArray<UMaterial*> RestoredMaterials;
        UMaterial* RestoredEdge = nullptr;
        FString MaterialError;
        const bool bRestoredMaterials = ValidateCompleteR19MaterialAssets(
            RestoredMaterials,
            RestoredEdge,
            MaterialError) &&
            RestoredMaterials.Num() == 6 &&
            !RestoredMaterials.Contains(nullptr) && RestoredEdge;
        FString ImmutableError;
        const bool bImmutable = R20ImmutablePackageSnapshotsMatch(
            ImmutableSnapshots,
            ImmutableError);
        const bool bRestored = RestoredWorld && RestoredPackage &&
            RestoredPackage->GetName() == TargetMapPackage &&
            !RestoredPackage->IsDirty() && bRestoredActorValid &&
            bRestoredHash && bRestoredMaterials && bImmutable;
        OutRollbackReport = FString::Printf(
            TEXT("coldPredecessorRestore=%s mapPackage=%s clean=%s actorCount=%d exactActor=%s bytes=%lld sha256=%s r19Materials=%s immutableArtifacts=%s backup=%s predecessor={%s} hashError={%s} materialError={%s} immutableError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            RestoredPackage ? *RestoredPackage->GetName() : TEXT("<null>"),
            RestoredPackage && !RestoredPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            RestoredActorCount,
            bRestoredActorValid ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            bRestoredMaterials ? TEXT("true") : TEXT("false"),
            bImmutable ? TEXT("true") : TEXT("false"),
            *Backup.Directory,
            *RestoredPredecessorReport,
            *HashError,
            *MaterialError,
            *ImmutableError);
        return bRestored;
    };

    const auto FailWithRollback =
        [&OutReport, &Backup, &ColdRestoreExactPredecessor](
            const FString& Failure) -> bool
    {
        FString RollbackReport;
        const bool bRolledBack =
            ColdRestoreExactPredecessor(RollbackReport);
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK mapArtifacts=6 coldReloaded=true predecessorBytes=37414604 predecessorSha256=E01C5ECB476E9723F3FB85AFFE101EE4CEBC719FD493E40C26E3BC85584FFAAB immutablePackageHashesPreserved=true backup=")) +
                Backup.Directory + TEXT(" rollback={") +
                RollbackReport + TEXT("}")
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED backup=")) +
                Backup.Directory + TEXT(" rollback={") +
                RollbackReport + TEXT("}"));
        return false;
    };

    FString RebuildReport;
    if (!Actor->RebuildGroundVegetationLayoutToR20(RebuildReport))
    {
        return FailWithRollback(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_REBUILD: ") +
            RebuildReport);
    }
    FString PreSaveReport;
    if (!Actor->ValidateR20PredecessorForR23Migration(PreSaveReport) ||
        Actor->GetClass() !=
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() ||
        !Actor->Tags.Contains(GroundVegetationTag) ||
        !R20ImmutablePackageSnapshotsMatch(
            ImmutableSnapshots,
            Error))
    {
        return FailWithRollback(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_PRE_SAVE_VALIDATION: ") +
            PreSaveReport + TEXT(" ") + Error);
    }

    if (!PersistExactR20TargetWorld(World))
    {
        return FailWithRollback(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_SAVE: only the exact target map was offered to the target-world persistence boundary."));
    }
    if (!R20ImmutablePackageSnapshotsMatch(
            ImmutableSnapshots,
            Error))
    {
        return FailWithRollback(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_POST_SAVE_IMMUTABLE_DRIFT: ") +
            Error);
    }

    FString UnloadFilename;
    UWorld* UnloadWorld =
        FPackageName::DoesPackageExist(
            R20UnloadMapPackage,
            &UnloadFilename)
        ? UEditorLoadingAndSavingUtils::LoadMap(UnloadFilename)
        : nullptr;
    if (!UnloadWorld || !UnloadWorld->GetOutermost() ||
        UnloadWorld->GetOutermost()->GetName() != R20UnloadMapPackage)
    {
        return FailWithRollback(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_COLD_UNLOAD: exact V5B unload map did not load."));
    }

    UWorld* ReloadedWorld =
        UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
    if (ReloadedWorld)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    UPackage* ReloadedPackage = ReloadedWorld
        ? ReloadedWorld->GetOutermost()
        : nullptr;
    int32 ReloadedActorCount = 0;
    ATRIADIstanaExploreV5DGroundVegetationActor* ReloadedActor =
        ReloadedWorld
        ? FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
              ReloadedWorld,
              ReloadedActorCount)
        : nullptr;
    FString ColdReport;
    const bool bColdActorValid = ReloadedActorCount == 1 &&
        ReloadedActor &&
        ReloadedActor->GetClass() ==
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() &&
        ReloadedActor->Tags.Contains(GroundVegetationTag) &&
        ReloadedActor->ValidateR20PredecessorForR23Migration(ColdReport);
    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    FString ColdMaterialError;
    const bool bColdMaterialsValid = ValidateCompleteR19MaterialAssets(
        ColdMaterials,
        ColdEdgeFade,
        ColdMaterialError) &&
        ColdMaterials.Num() == 6 &&
        !ColdMaterials.Contains(nullptr) && ColdEdgeFade;
    if (!ReloadedWorld || !ReloadedPackage ||
        ReloadedPackage->GetName() != TargetMapPackage ||
        ReloadedPackage->IsDirty() || !bColdActorValid ||
        !bColdMaterialsValid ||
        !R20ImmutablePackageSnapshotsMatch(
            ImmutableSnapshots,
            Error))
    {
        return FailWithRollback(FString::Printf(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_COLD_VALIDATION: package=%s clean=%s actorCount=%d actorValid=%s materialsValid=%s actor={%s} materials={%s} immutable={%s}"),
            ReloadedPackage
                ? *ReloadedPackage->GetName()
                : TEXT("<null>"),
            ReloadedPackage && !ReloadedPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            ReloadedActorCount,
            bColdActorValid ? TEXT("true") : TEXT("false"),
            bColdMaterialsValid ? TEXT("true") : TEXT("false"),
            *ColdReport,
            *ColdMaterialError,
            *Error));
    }

    FString FinalSha256;
    int64 FinalBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            MapFilename,
            FinalSha256,
            FinalBytes,
            Error))
    {
        return FailWithRollback(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_FINAL_HASH: ") +
            Error);
    }
    const bool bFinalHashPinned = R20FinalMapBytes > 0 &&
        !R20FinalMapSha256.IsEmpty();
    if (bFinalHashPinned &&
        (FinalBytes != R20FinalMapBytes ||
         FinalSha256 != R20FinalMapSha256))
    {
        return FailWithRollback(FString::Printf(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_FINAL_HASH_DRIFT: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s"),
            R20FinalMapBytes,
            FinalBytes,
            *R20FinalMapSha256,
            *FinalSha256));
    }

    int32 ChangedMapArtifacts = 0;
    for (const FR20MapArtifactBackup& Artifact : Backup.Artifacts)
    {
        const int64 Bytes = IFileManager::Get().FileSize(*Artifact.Original);
        const bool bPresent = Bytes >= 0;
        FString Md5;
        if (bPresent)
        {
            Md5 = FileMd5(Artifact.Original);
        }
        if (bPresent != Artifact.bPresent || Bytes != Artifact.Bytes ||
            Md5 != Artifact.Md5)
        {
            ++ChangedMapArtifacts;
        }
    }
    if (ChangedMapArtifacts < 1)
    {
        return FailWithRollback(
            TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_FAILED_NO_DISK_DELTA: the serialized map did not acquire a changed artifact."));
    }

    OutReport = FString::Printf(
        TEXT("V5D_R20_GRASS_LAYOUT_UPGRADE_PASS predecessorRevision=%d actorRevision=%d exactOneTaggedActor=true exactActorClass=true inPlace=true mapOnlyTransaction=true saveTargets=1 mapsSaved=1 unloadMap=/Game/Maps/Istana_PublicView_Explore_v5b coldReloaded=true coldValidated=true mapArtifactsBackedUp=6 changedMapArtifacts=%d predecessorBytes=%lld predecessorSha256=%s measuredFinalBytes=%lld measuredFinalSha256=%s finalHashPinned=%s r19MaterialsValidated=7 immutableMaterialSourceTextureArtifacts=14 immutablePackageHashesPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true materialPackagesSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true backup=%s actor={%s}"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedPredecessorGrassPresentationRevision(),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedR23PredecessorGrassPresentationRevision(),
        ChangedMapArtifacts,
        R20PredecessorMapBytes,
        *R20PredecessorMapSha256,
        FinalBytes,
        *FinalSha256,
        bFinalHashPinned ? TEXT("true") : TEXT("false"),
        *Backup.Directory,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismGrassMaterialsToR21(FString& OutReport)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutReport = TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_REFUSED_EDITOR_STATE: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR21 = false;
    FString Error;
    if (!ValidateR21UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR21, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR21PreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    FString PinReport;
    if (!ValidateR21ExactDiskPins(bAlreadyR21, PinReport, Error))
    {
        OutReport = TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_REFUSED_EXACT_PINS: ") +
            Error;
        return false;
    }
    if (bAlreadyR21)
    {
        if (!R21PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
        {
            OutReport = TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport =
            TEXT("IDEMPOTENT_V5D_R21_GRASS_MATERIALS_ALREADY_VALID targets=4 saveTargets=0 reloadTargets=0 exactR20MapPin=true graphBasedColdValidationPrimary=true overlayR18HashPreserved=true soilHashPreserved=true edgeFadeHashPreserved=true immutablePackageHashesPreserved=true mapsSaved=0 instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true ") +
            PinReport;
        return true;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3]};
    bool bExactTargetRoster = Targets.Num() ==
        UE_ARRAY_COUNT(GrassAssetNames) && !Targets.Contains(nullptr);
    for (int32 Index = 0;
         bExactTargetRoster && Index < UE_ARRAY_COUNT(GrassAssetNames);
         ++Index)
    {
        bExactTargetRoster &= Targets[Index]->GetPathName() ==
            ObjectPath(GrassAssetNames[Index]);
    }
    TArray<FString> WpoFingerprintsBefore;
    for (UMaterial* Target : Targets)
    {
        WpoFingerprintsBefore.Add(BuildR21WpoFingerprint(
            FindCustomByDescription(Target, GrassWindDescription)));
    }
    const bool bCompleteWpoFingerprints =
        WpoFingerprintsBefore.Num() == 4 &&
        !WpoFingerprintsBefore.Contains(FString());

    FMaterialUpgradeBackup Backup;
    if (!bExactTargetRoster || !bCompleteWpoFingerprints ||
        !BackUpR21GrassMaterialPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_REFUSED_BACKUP: the target roster must be exactly the four ordered R19 grass packages with complete WPO fingerprints. ") +
            Error;
        return false;
    }
    const auto FailWithRollback =
        [&OutReport, &Backup, &PreservedSnapshots](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R21PreservedPackageSnapshotsMatch(
                PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R21 immutable invariant verification failed: ") +
                InvariantError;
        }
        TArray<UMaterial*> RestoredMaterials;
        UMaterial* RestoredEdge = nullptr;
        bool bRestoredAlreadyR21 = true;
        FString RestoredError;
        FString RestoredPinReport;
        if (!ValidateR21UpgradeInputMaterialAssets(
                RestoredMaterials,
                RestoredEdge,
                bRestoredAlreadyR21,
                RestoredError) ||
            bRestoredAlreadyR21 || RestoredMaterials.Num() != 6 ||
            !RestoredEdge ||
            !ValidateR21ExactDiskPins(
                false, RestoredPinReport, RestoredError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R21 exact R19 predecessor rollback validation failed: ") +
                RestoredError;
        }
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=4 preReloadHashes=true postReloadHashes=true exactR19PredecessorPinsRestored=true absentSidecarsRestored=true exactR20MapPin=true immutablePackageHashesPreserved=true mapsSaved=0 backup=")) +
                Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR21GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() !=
                ObjectPath(GrassAssetNames[Index]) ||
            BuildR21WpoFingerprint(FindCustomByDescription(
                Materials[Index], GrassWindDescription)) !=
                WpoFingerprintsBefore[Index])
        {
            return FailWithRollback(FString::Printf(
                TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_IN_PLACE: grass profile %d did not retain exact object identity, 29-node topology, or byte-stable WPO fingerprint. "),
                Index) + Error);
        }
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 4 ||
        ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_SAVE: only the exact four V5D grass packages were offered to save; the R20 map, R18 overlay, soil, edge fade, source packages and textures were never offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR21MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || ColdMaterials.Contains(nullptr) ||
        !ColdEdgeFade ||
        !R21PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    bool bColdWpoByteStable = true;
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    for (int32 Index = 0; Index < 4; ++Index)
    {
        bColdWpoByteStable &= BuildR21WpoFingerprint(
            FindCustomByDescription(
                ColdMaterials[Index], GrassWindDescription)) ==
            WpoFingerprintsBefore[Index];
    }
    FString FinalMapPinReport;
    if (!bColdPackagesClean || !bColdWpoByteStable ||
        !ValidateR21ExactDiskPins(true, FinalMapPinReport, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_COLD_VALIDATION: clean packages, exact WPO fingerprints and the exact R20 map pin are mandatory. ") +
            Error);
    }

    if (Backup.ExpectedPackageCount != 4 || Backup.Packages.Num() != 4 ||
        Backup.ExpectedArtifactsPerPackage != 7)
    {
        return FailWithRollback(
            TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: the exact four-package backup roster is incomplete."));
    }
    FString HashDeltas;
    FString FinalPins;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 7 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_BACKUP_READBACK: canonical target state is incomplete."));
        }
        const FString NewMd5 = FileMd5(Package.PackageFilename);
        FString NewSha256;
        int64 NewBytes = INDEX_NONE;
        if (NewMd5.IsEmpty() ||
            NewMd5 == Package.Artifacts[0].Md5 ||
            !HashFileSha256R20(
                Package.PackageFilename,
                NewSha256,
                NewBytes,
                Error) ||
            NewBytes <= 0 || NewSha256.IsEmpty())
        {
            return FailWithRollback(
                TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_FAILED_DISK_DELTA: every R21 grass target must acquire a distinct, readable canonical package hash. ") +
                Error);
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
            FinalPins += TEXT(",");
        }
        const FString Stem = FPaths::GetBaseFilename(Package.PackageFilename);
        HashDeltas += Stem + TEXT(":") + Package.Artifacts[0].Md5 +
            TEXT("->") + NewMd5;
        FinalPins += FString::Printf(
            TEXT("%s:%lld:%s"), *Stem, NewBytes, *NewSha256);
    }

    OutReport = FString::Printf(
        TEXT("V5D_R21_GRASS_MATERIAL_UPGRADE_PASS packages=4 exactFourPackageAtomicTransaction=true allSevenInputPackagesValid=true uniformR19Input=true exactR19PredecessorPins=true exactR20MapPin=true graphBasedColdValidationPrimary=true inPlace=true objectPathsStable=true saveTargets=4 reloadTargets=4 coldValidatedMaterials=7 coldPackagesClean=true grassExpressions=29 grassTextureSamples=0 grassColorInputs=5 grassRoughnessInputs=5 grassSpecularInputs=5 grassNormalInputs=5 grassMacroFields=2 grassMacroScaleMeters=13.5,4.7 grassColorDetailFadeMeters=5,14 grassNormalFadeMeters=4,12 grassBroadValueRange=0.975,1.025 grassNearFineValueRange=0.990,1.010 grassRootBodyRise=0.08,0.62 grassCutMask=0.84,1.0 grassRootMask=0.06,0.34 grassSoilRootMask=0.02,0.18 grassClassTransitionHalfWidths=0.018,0.020,0.015 grassDryColor=0.285,0.205,0.060 grassThatchColor=0.105,0.072,0.024 grassSoilRootColor=0.050,0.034,0.014 grassDryBlend=0.90 grassThatchBlend=0.92 grassSoilRootBlend=0.98 grassDryFractions=0.10,0.07,0.13,0.26 grassThatchFractions=0.18,0.13,0.22,0.34 grassSoilRootFractions=0.035,0.025,0.045,0.075 grassDiffuseGain=1.08,1.00,0.92 grassFinalChroma=0.90 grassCutDesaturation=0.20 grassCutGain=0.97 grassRoughnessNear=0.68,0.66,0.72,0.75 grassRoughnessFar=0.74 grassSpecularNear=0.30,0.31,0.28,0.26 grassSpecularFar=0.19 grassNormalNearFar=0.42,0.0 grassNormalBladeMultiplier=0.95,1.05 grassSubsurfaceGains=1.30,0.84,0.56;1.28,0.86,0.58;1.34,0.88,0.62;1.24,0.80,0.52 calmWindStrengthCm=0.48 calmWindSpeed=0.12 calmWindHeightCm=4.8 calmWindMaximumWpoCm=0.55 calmWindResponse=0.14,0.18,0.11,0.14 wpoCustomExpressionCodeBytesPreservedExactly=true wpoGraphFingerprintPreservedExactly=true opacityGraphPreservedExactly=true overlayR18HashPreserved=true soilHashPreserved=true edgeFadeHashPreserved=true mapHashPreserved=true immutablePackageHashesPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true mapsSaved=0 collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true materialsEmissiveUntouched=true instanceTransformsUntouched=true backup=%s deltas=%s finalPins=%s %s"),
        *Backup.Directory,
        *HashDeltas,
        *FinalPins,
        *FinalMapPinReport);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismGrassSystemToR22(FString& OutReport)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutReport = TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_REFUSED_EDITOR_STATE: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR22 = false;
    FString Error;
    if (!ValidateR22UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR22, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR22PreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    FString PinReport;
    if (!ValidateR22ExactDiskPins(bAlreadyR22, PinReport, Error))
    {
        OutReport = TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_REFUSED_EXACT_PINS: ") +
            Error;
        return false;
    }
    if (bAlreadyR22)
    {
        if (!R22PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
        {
            OutReport = TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_REFUSED_IDEMPOTENT_INVARIANTS: ") +
                Error;
            return false;
        }
        OutReport =
            TEXT("IDEMPOTENT_V5D_R22_GRASS_SYSTEM_ALREADY_VALID operation=R22SixPackageHyperrealGrassSystemUpgrade targets=6 saveTargets=0 reloadTargets=0 graphBasedColdValidationPrimary=true exactR20MapPin=true exactSoilPin=true canonicalOnlyTargetArtifacts=true immutablePackageHashesPreserved=true mapsSaved=0 instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true runtimeMaterialRevisionMarker=22 ") +
            PinReport;
        return true;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3],
        Materials[5], EdgeFadeMaterial};
    bool bExactTargetRoster = Targets.Num() ==
        UE_ARRAY_COUNT(R22PredecessorPins) && !Targets.Contains(nullptr);
    for (int32 Index = 0;
         bExactTargetRoster && Index < UE_ARRAY_COUNT(R22PredecessorPins);
         ++Index)
    {
        bExactTargetRoster &= Targets[Index]->GetPathName() ==
            ObjectPath(FString(R22PredecessorPins[Index].AssetName));
    }

    FMaterialUpgradeBackup Backup;
    if (!bExactTargetRoster ||
        !BackUpR22GrassSystemPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_REFUSED_BACKUP: the target roster must be exactly the ordered four R21 grass, R18 overlay, and R11 edge packages. ") +
            Error;
        return false;
    }
    const auto FailWithRollback =
        [&OutReport, &Backup, &PreservedSnapshots](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R22PreservedPackageSnapshotsMatch(
                PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R22 immutable invariant verification failed: ") +
                InvariantError;
        }
        TArray<UMaterial*> RestoredMaterials;
        UMaterial* RestoredEdge = nullptr;
        bool bRestoredAlreadyR22 = true;
        FString RestoredError;
        FString RestoredPinReport;
        if (!ValidateR22UpgradeInputMaterialAssets(
                RestoredMaterials,
                RestoredEdge,
                bRestoredAlreadyR22,
                RestoredError) ||
            bRestoredAlreadyR22 || RestoredMaterials.Num() != 6 ||
            RestoredMaterials.Contains(nullptr) || !RestoredEdge ||
            !ValidateR22ExactDiskPins(
                false, RestoredPinReport, RestoredError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R22 exact six-package predecessor rollback validation failed: ") +
                RestoredError;
        }
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=6 preReloadHashes=true postReloadHashes=true exactSixPackagePredecessorPinsRestored=true absentSidecarsRestored=true exactR20MapPin=true exactSoilPin=true immutablePackageHashesPreserved=true mapsSaved=0 backup=")) +
                Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR22GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() !=
                ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(FString::Printf(
                TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_GRASS_IN_PLACE: grass profile %d did not retain exact object identity and its R22 stable graph. "),
                Index) + Error);
        }
    }
    if (!ConfigureR22GroundOverlayMaterial(Materials[5], Error) ||
        Materials[5] != Targets[4] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") +
            Error);
    }
    if (!ConfigureR22EdgeGrassFadeMaterial(EdgeFadeMaterial, Error) ||
        EdgeFadeMaterial != Targets[5] ||
        EdgeFadeMaterial->GetPathName() !=
            ObjectPath(EdgeGrassFadeAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_EDGE_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 6 ||
        ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_SAVE: only the exact six ordered grass-system packages were offered to save; the R20 map, soil, source packages, textures, transforms, collision, navigation, and RF state were never offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR22MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || ColdMaterials.Contains(nullptr) ||
        !ColdEdgeFade ||
        !R22PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    FString FinalPinReport;
    if (!bColdPackagesClean ||
        !ValidateR22ExactDiskPins(true, FinalPinReport, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_COLD_VALIDATION: clean packages and exact immutable disk pins are mandatory. ") +
            Error);
    }

    if (Backup.ExpectedPackageCount != 6 || Backup.Packages.Num() != 6 ||
        Backup.ExpectedArtifactsPerPackage != 7)
    {
        return FailWithRollback(
            TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_BACKUP_READBACK: the exact six-package, seven-artifact backup roster is incomplete."));
    }
    FString HashDeltas;
    FString FinalPins;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 7 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_BACKUP_READBACK: a canonical target state is incomplete."));
        }
        const FString NewMd5 = FileMd5(Package.PackageFilename);
        FString NewSha256;
        int64 NewBytes = INDEX_NONE;
        if (NewMd5.IsEmpty() || NewMd5 == Package.Artifacts[0].Md5 ||
            !HashFileSha256R20(
                Package.PackageFilename,
                NewSha256,
                NewBytes,
                Error) ||
            NewBytes <= 0 || NewSha256.IsEmpty())
        {
            return FailWithRollback(
                TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_FAILED_DISK_DELTA: every R22 target must acquire a distinct, readable canonical package hash. ") +
                Error);
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
            FinalPins += TEXT(",");
        }
        const FString Stem = FPaths::GetBaseFilename(Package.PackageFilename);
        HashDeltas += Stem + TEXT(":") + Package.Artifacts[0].Md5 +
            TEXT("->") + NewMd5;
        FinalPins += FString::Printf(
            TEXT("%s:%lld:%s"), *Stem, NewBytes, *NewSha256);
    }

    OutReport = FString::Printf(
        TEXT("V5D_R22_GRASS_SYSTEM_UPGRADE_PASS operation=R22SixPackageHyperrealGrassSystemUpgrade packages=6 exactSixPackageAtomicTransaction=true allSevenInputPackagesValid=true uniformR21R18R11Input=true exactSixPackagePredecessorPins=true exactR20MapPin=true exactSoilPin=true graphBasedColdValidationPrimary=true inPlace=true objectPathsStable=true saveTargets=6 reloadTargets=6 coldValidatedMaterials=7 coldPackagesClean=true targetArtifacts=42 absentTargetSidecars=36 grassExpressions=31 grassStableVisibilityInputs=5 grassStableVisibilityMeters=12,18 grassTemporalDitherDisconnectedFromOpacity=true grassDefaultWindStrengthCm=0 grassWindCustomExpressionPreserved=true overlayExpressions=25 overlayTextureSamples=4 overlayMipBiasMode=TMVM_MipBias overlayMipBiasMeters=30,55 overlayTintGroundLevel=1.96625,0.91675,0.56525 overlayTintElevated=0.715,0.965,0.595 overlayTintHeightFadeMeters=50,150 overlaySpecular=0.14 overlayRoughnessFar=0.88 overlayNormalNearFar=0.12,0 edgeExpressions=25 edgeTextureSamples=4 edgeStableVisibilityMeters=12,18 edgeDefaultWindStrengthCm=0 runtimeMaterialRevisionMarker=22 mapHashPreserved=true soilHashPreserved=true immutablePackageHashesPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true grass004TexturesUntouched=true mapsSaved=0 instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true materialsEmissiveUntouched=true backup=%s deltas=%s finalPins=%s %s"),
        *Backup.Directory,
        *HashDeltas,
        *FinalPins,
        *FinalPinReport);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismGrassSystemToR23(FString& OutReport)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_EDITOR_STATE: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR23 = false;
    FString Error;
    if (!ValidateR23UpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR23, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3],
        Materials[5], EdgeFadeMaterial};
    bool bExactTargetRoster = Targets.Num() ==
        UE_ARRAY_COUNT(R23PredecessorPins) && !Targets.Contains(nullptr);
    for (int32 Index = 0;
         bExactTargetRoster && Index < UE_ARRAY_COUNT(R23PredecessorPins);
         ++Index)
    {
        bExactTargetRoster &= Targets[Index]->GetPathName() ==
            ObjectPath(FString(R23PredecessorPins[Index].AssetName));
    }
    if (!bExactTargetRoster)
    {
        OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_ROSTER: the target roster is not the exact ordered four grass, overlay, and edge packages.");
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR23PreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    FString PinReport;
    if (!ValidateR23ExactDiskPins(bAlreadyR23, PinReport, Error))
    {
        OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_EXACT_PINS: ") +
            Error;
        return false;
    }
    if (bAlreadyR23)
    {
        FMaterialUpgradeBackup ColdReloadDescriptor;
        ColdReloadDescriptor.ExpectedPackageCount = 6;
        for (UMaterial* Target : Targets)
        {
            FMaterialPackageBackup& Record =
                ColdReloadDescriptor.Packages.AddDefaulted_GetRef();
            Record.PackageName = Target->GetOutermost()->GetName();
        }
        TArray<UMaterial*> ColdMaterials;
        UMaterial* ColdEdgeFade = nullptr;
        FString ColdPinReport;
        if (!ReloadMaterialPackages(ColdReloadDescriptor, Error) ||
            !ValidateCompleteR23MaterialAssets(
                ColdMaterials, ColdEdgeFade, Error) ||
            ColdMaterials.Num() != 6 || ColdMaterials.Contains(nullptr) ||
            !ColdEdgeFade ||
            !R23PreservedPackageSnapshotsMatch(PreservedSnapshots, Error) ||
            !ValidateR23ExactDiskPins(true, ColdPinReport, Error))
        {
            OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_IDEMPOTENT_COLD_DISK_VALIDATION: ") +
                Error;
            return false;
        }
        bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
            !ColdEdgeFade->GetOutermost()->IsDirty();
        for (UMaterial* Material : ColdMaterials)
        {
            bColdPackagesClean &= Material && Material->GetOutermost() &&
                !Material->GetOutermost()->IsDirty();
        }
        if (!bColdPackagesClean)
        {
            OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_IDEMPOTENT_COLD_DISK_VALIDATION: a canonical R23 target remained dirty after reload.");
            return false;
        }
        OutReport =
            TEXT("IDEMPOTENT_V5D_R23_GRASS_SYSTEM_ALREADY_VALID operation=R23SixPackagePhotographicDepthGrassSystemUpgrade targets=6 saveTargets=0 reloadTargets=6 diskColdValidated=true graphBasedColdValidationPrimary=true exactAdmittedR20OrR23MapPin=true exactSoilPin=true canonicalOnlyTargetArtifacts=true immutablePackageHashesPreserved=true mapsSaved=0 instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true runtimeMaterialRevisionMarker=23 ") +
            ColdPinReport;
        return true;
    }

    FMaterialUpgradeBackup Backup;
    if (!bExactTargetRoster ||
        !BackUpR23GrassSystemPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_REFUSED_BACKUP: the target roster must be exactly the ordered four R22 grass, R22 overlay, and R22 edge packages. ") +
            Error;
        return false;
    }
    const auto FailWithRollback =
        [&OutReport, &Backup, &PreservedSnapshots](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R23PreservedPackageSnapshotsMatch(
                PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R23 immutable invariant verification failed: ") +
                InvariantError;
        }
        TArray<UMaterial*> RestoredMaterials;
        UMaterial* RestoredEdge = nullptr;
        bool bRestoredAlreadyR23 = true;
        FString RestoredError;
        FString RestoredPinReport;
        if (!ValidateR23UpgradeInputMaterialAssets(
                RestoredMaterials,
                RestoredEdge,
                bRestoredAlreadyR23,
                RestoredError) ||
            bRestoredAlreadyR23 || RestoredMaterials.Num() != 6 ||
            RestoredMaterials.Contains(nullptr) || !RestoredEdge ||
            !ValidateR23ExactDiskPins(
                false, RestoredPinReport, RestoredError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R23 exact six-package R22 predecessor rollback validation failed: ") +
                RestoredError;
        }
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=6 preReloadHashes=true postReloadHashes=true exactR22PredecessorPinsRestored=true absentSidecarsRestored=true exactR20MapPin=true exactSoilPin=true immutablePackageHashesPreserved=true mapsSaved=0 backup=")) +
                Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR23GrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() !=
                ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(FString::Printf(
                TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_GRASS_IN_PLACE: grass profile %d did not retain exact object identity and its R23 stable graph. "),
                Index) + Error);
        }
    }
    if (!ConfigureR23GroundOverlayMaterial(Materials[5], Error) ||
        Materials[5] != Targets[4] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") +
            Error);
    }
    if (!ConfigureR23EdgeGrassFadeMaterial(EdgeFadeMaterial, Error) ||
        EdgeFadeMaterial != Targets[5] ||
        EdgeFadeMaterial->GetPathName() !=
            ObjectPath(EdgeGrassFadeAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_EDGE_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 6 ||
        ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_SAVE: only the exact six ordered grass-system packages were offered to save; the R20 map, soil, source packages, textures, transforms, collision, navigation, and RF state were never offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR23MaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || ColdMaterials.Contains(nullptr) ||
        !ColdEdgeFade ||
        !R23PreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    FString FinalPinReport;
    if (!bColdPackagesClean ||
        !ValidateR23ExactDiskPins(true, FinalPinReport, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_COLD_VALIDATION: clean packages and exact immutable disk pins are mandatory. ") +
            Error);
    }

    if (Backup.ExpectedPackageCount != 6 || Backup.Packages.Num() != 6 ||
        Backup.ExpectedArtifactsPerPackage != 7)
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_BACKUP_READBACK: the exact six-package, seven-artifact backup roster is incomplete."));
    }
    FString HashDeltas;
    FString FinalPins;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 7 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_BACKUP_READBACK: a canonical target state is incomplete."));
        }
        const FString NewMd5 = FileMd5(Package.PackageFilename);
        FString NewSha256;
        int64 NewBytes = INDEX_NONE;
        if (NewMd5.IsEmpty() || NewMd5 == Package.Artifacts[0].Md5 ||
            !HashFileSha256R20(
                Package.PackageFilename,
                NewSha256,
                NewBytes,
                Error) ||
            NewBytes <= 0 || NewSha256.IsEmpty())
        {
            return FailWithRollback(
                TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_FAILED_DISK_DELTA: every R23 target must acquire a distinct, readable canonical package hash. ") +
                Error);
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
            FinalPins += TEXT(",");
        }
        const FString Stem = FPaths::GetBaseFilename(Package.PackageFilename);
        HashDeltas += Stem + TEXT(":") + Package.Artifacts[0].Md5 +
            TEXT("->") + NewMd5;
        FinalPins += FString::Printf(
            TEXT("%s:%lld:%s"), *Stem, NewBytes, *NewSha256);
    }

    OutReport = FString::Printf(
        TEXT("V5D_R23_GRASS_SYSTEM_UPGRADE_PASS operation=R23SixPackagePhotographicDepthGrassSystemUpgrade packages=6 exactSixPackageAtomicTransaction=true allSevenInputPackagesValid=true uniformR22Input=true exactR22PredecessorPins=true exactR20MapPin=true exactSoilPin=true graphBasedColdValidationPrimary=true inPlace=true objectPathsStable=true saveTargets=6 reloadTargets=6 coldValidatedMaterials=7 coldPackagesClean=true targetArtifacts=42 absentTargetSidecars=36 grassExpressions=31 grassStableVisibilityInputs=5 grassStableVisibilityMeters=20,28 grassTemporalDitherDisconnectedFromOpacity=true grassDefaultWindStrengthCm=0 grassWindCustomExpressionPreserved=true grassColorDetailFadeMeters=10,24 grassNormalNearFar=0.50,0.08 grassNormalFadeMeters=12,24 grassMacroScaleMeters=9.5,2.1 overlayExpressions=25 overlayTextureSamples=4 overlayMipBiasMode=TMVM_MipBias overlayMipBiasMeters=30,55 overlayTintCameraHeightIndependent=true overlayTintNear=0.78,1.10,0.70 overlayTintFar=0.76,1.02,0.68 overlaySubstrateBreakupMeters=7.3,1.7,0.48 overlaySpecular=0.18 overlayRoughnessRange=0.68,0.91 overlayNormalNearFar=0.22,0.02 edgeExpressions=25 edgeTextureSamples=4 edgeStableVisibilityMeters=20,28 edgeDefaultWindStrengthCm=0 runtimeMaterialRevisionMarker=23 mapHashPreserved=true soilHashPreserved=true immutablePackageHashesPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true grass004TexturesUntouched=true mapsSaved=0 instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true materialsEmissiveUntouched=true backup=%s deltas=%s finalPins=%s %s"),
        *Backup.Directory,
        *HashDeltas,
        *FinalPins,
        *FinalPinReport);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationRealismGrassSystemToR23B(FString& OutReport)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_EDITOR_STATE: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> Materials;
    UMaterial* EdgeFadeMaterial = nullptr;
    bool bAlreadyR23B = false;
    FString Error;
    if (!ValidateR23BUpgradeInputMaterialAssets(
            Materials, EdgeFadeMaterial, bAlreadyR23B, Error) ||
        Materials.Num() != 6 || Materials.Contains(nullptr) ||
        !EdgeFadeMaterial)
    {
        OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_ROSTER: ") +
            Error;
        return false;
    }

    TArray<UMaterial*> Targets = {
        Materials[0], Materials[1], Materials[2], Materials[3],
        Materials[5], EdgeFadeMaterial};
    bool bExactTargetRoster = Targets.Num() ==
        UE_ARRAY_COUNT(R23BPredecessorPins) && !Targets.Contains(nullptr);
    for (int32 Index = 0;
         bExactTargetRoster &&
             Index < UE_ARRAY_COUNT(R23BPredecessorPins);
         ++Index)
    {
        bExactTargetRoster &= Targets[Index]->GetPathName() ==
            ObjectPath(FString(R23BPredecessorPins[Index].AssetName));
    }
    if (!bExactTargetRoster)
    {
        OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_ROSTER: the target roster is not the exact ordered four grass, overlay, and edge packages.");
        return false;
    }

    TArray<FR11PreservedPackageSnapshot> PreservedSnapshots;
    if (!CaptureR23BPreservedPackageSnapshots(PreservedSnapshots, Error))
    {
        OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    FString PinReport;
    if (!ValidateR23BExactDiskPins(bAlreadyR23B, PinReport, Error))
    {
        OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_EXACT_PINS: ") +
            Error;
        return false;
    }

    const FString CommonReport =
        TEXT("operation=R23BSixPackagePhotographicMaterialCalibration targets=6 exactSixPackageAtomicTransaction=true allSevenInputPackagesValid=true graphBasedColdValidationPrimary=true inPlace=true objectPathsStable=true mapLayoutPreserved=true meshPlacementPreserved=true rfPlacementPreserved=true instanceTransformsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true materialsEmissiveUntouched=true sourceV3V4V5BMaterialAssetsUntouched=true grass004TexturesUntouched=true mapsSaved=0 runtimeMaterialRevisionMarker=23 runtimeMaterialCalibrationRevisionMarker=1 grassMaterialCalibrationRevision=R23B groundOverlayMaterialCalibrationRevision=R23B edgeGrassMaterialCalibrationRevision=R23B grassR23BPhotographicCalibration=true validatedGrassRevisionSelection=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK grassExpressions=32 grassTextureSamples=0 grassColorInputs=5 grassRoughnessInputs=5 grassSpecularInputs=5 grassNormalInputs=5 grassStableVisibilityInputs=6 grassStableVisibilityMeters=20,28 grassDefaultWindStrengthCm=0 grassWindCustomExpressionPreserved=true grassWpoGraphFingerprintPreservedExactly=true grassRoughness=0.71,0.70,0.75,0.78 grassSpecular=0.27,0.28,0.25,0.23 grassNormalNearFar=0.38,0.04 grassNormalBladeMultiplier=0.95,1.05 overlayExpressions=26 overlayTextureSamples=4 overlayBaseInputs=10 overlayMipBiasMode=TMVM_MipBias overlayMipBiasMeters=30,55 overlayTintCameraHeightIndependent=true overlaySubstrateBreakupMeters=7.3,1.7,0.48 overlaySpecular=0.15 overlayRoughnessRange=0.72,0.92 overlayNormalNearFar=0.17,0.01 edgeExpressions=26 edgeTextureSamples=4 edgeStableVisibilityInputs=6 edgeStableVisibilityMeters=20,28 edgeDefaultWindStrengthCm=0 exactR23MapPin=true exactSoilPin=true immutablePackageHashesPreserved=true ");

    if (bAlreadyR23B)
    {
        FMaterialUpgradeBackup ColdReloadDescriptor;
        ColdReloadDescriptor.ExpectedPackageCount = 6;
        for (UMaterial* Target : Targets)
        {
            FMaterialPackageBackup& Record =
                ColdReloadDescriptor.Packages.AddDefaulted_GetRef();
            Record.PackageName = Target->GetOutermost()->GetName();
        }
        TArray<UMaterial*> ColdMaterials;
        UMaterial* ColdEdgeFade = nullptr;
        FString ColdPinReport;
        if (!ReloadMaterialPackages(ColdReloadDescriptor, Error) ||
            !ValidateCompleteR23BMaterialAssets(
                ColdMaterials, ColdEdgeFade, Error) ||
            ColdMaterials.Num() != 6 || ColdMaterials.Contains(nullptr) ||
            !ColdEdgeFade ||
            !R23BPreservedPackageSnapshotsMatch(PreservedSnapshots, Error) ||
            !ValidateR23BExactDiskPins(true, ColdPinReport, Error))
        {
            OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_IDEMPOTENT_COLD_DISK_VALIDATION: ") +
                Error;
            return false;
        }
        bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
            !ColdEdgeFade->GetOutermost()->IsDirty();
        for (UMaterial* Material : ColdMaterials)
        {
            bColdPackagesClean &= Material && Material->GetOutermost() &&
                !Material->GetOutermost()->IsDirty();
        }
        if (!bColdPackagesClean)
        {
            OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_IDEMPOTENT_COLD_DISK_VALIDATION: a canonical R23B target remained dirty after reload.");
            return false;
        }
        OutReport = (R23BFinalPinsAreSealed
            ? FString(TEXT("IDEMPOTENT_V5D_R23B_GRASS_SYSTEM_ALREADY_VALID publicationReady=true finalPinsSealed=true saveTargets=0 reloadTargets=6 diskColdValidated=true "))
            : FString(TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_BOOTSTRAP_PASS publicationReady=false finalPinsSealed=false bootstrapState=UNSEALED_FINAL_PACKAGE_PINS saveTargets=0 reloadTargets=6 diskColdValidated=true "))) +
            CommonReport + ColdPinReport;
        return true;
    }

    FMaterialUpgradeBackup Backup;
    if (!BackUpR23BGrassSystemPackages(Targets, Backup, Error))
    {
        OutReport = TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_REFUSED_BACKUP: the target roster must be exactly the ordered four R23 grass, R23 overlay, and R23 edge packages. ") +
            Error;
        return false;
    }
    const auto FailWithRollback =
        [&OutReport, &Backup, &PreservedSnapshots](const FString& Failure)
    {
        FString RollbackError;
        bool bRolledBack =
            RestoreMaterialPackagesAtomically(Backup, RollbackError);
        FString InvariantError;
        if (!R23BPreservedPackageSnapshotsMatch(
                PreservedSnapshots, InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R23B immutable invariant verification failed: ") +
                InvariantError;
        }
        TArray<UMaterial*> RestoredMaterials;
        UMaterial* RestoredEdge = nullptr;
        bool bRestoredAlreadyR23B = true;
        FString RestoredError;
        FString RestoredPinReport;
        if (!ValidateR23BUpgradeInputMaterialAssets(
                RestoredMaterials,
                RestoredEdge,
                bRestoredAlreadyR23B,
                RestoredError) ||
            bRestoredAlreadyR23B || RestoredMaterials.Num() != 6 ||
            RestoredMaterials.Contains(nullptr) || !RestoredEdge ||
            !ValidateR23BExactDiskPins(
                false, RestoredPinReport, RestoredError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" R23B exact six-package R23 predecessor rollback validation failed: ") +
                RestoredError;
        }
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK packages=6 preReloadHashes=true postReloadHashes=true exactR23PredecessorPinsRestored=true absentSidecarsRestored=true exactR23MapPin=true exactSoilPin=true immutablePackageHashesPreserved=true mapsSaved=0 backup=")) +
                Backup.Directory
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED ")) + RollbackError);
        return false;
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassAssetNames); ++Index)
    {
        if (!ConfigureR23BGrassMaterial(Materials[Index], Index, Error) ||
            Materials[Index] != Targets[Index] ||
            Materials[Index]->GetPathName() !=
                ObjectPath(GrassAssetNames[Index]))
        {
            return FailWithRollback(FString::Printf(
                TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_GRASS_IN_PLACE: grass profile %d did not retain exact object identity and its calibrated R23B stable graph. "),
                Index) + Error);
        }
    }
    if (!ConfigureR23BGroundOverlayMaterial(Materials[5], Error) ||
        Materials[5] != Targets[4] ||
        Materials[5]->GetPathName() != ObjectPath(GroundOverlayAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_OVERLAY_IN_PLACE: ") +
            Error);
    }
    if (!ConfigureR23BEdgeGrassFadeMaterial(EdgeFadeMaterial, Error) ||
        EdgeFadeMaterial != Targets[5] ||
        EdgeFadeMaterial->GetPathName() != ObjectPath(EdgeGrassFadeAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_EDGE_IN_PLACE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();

    TArray<UObject*> ExactSaveTargets;
    for (UMaterial* Target : Targets)
    {
        ExactSaveTargets.Add(Target);
    }
    if (ExactSaveTargets.Num() != 6 || ExactSaveTargets.Contains(nullptr) ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        return FailWithRollback(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_SAVE: only the exact six ordered material packages were offered to save; the R23 map, soil, source packages, textures, transforms, collision, navigation, and RF state were never offered."));
    }
    if (!ReloadMaterialPackages(Backup, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_RELOAD: ") + Error);
    }

    TArray<UMaterial*> ColdMaterials;
    UMaterial* ColdEdgeFade = nullptr;
    if (!ValidateCompleteR23BMaterialAssets(
            ColdMaterials, ColdEdgeFade, Error) ||
        ColdMaterials.Num() != 6 || ColdMaterials.Contains(nullptr) ||
        !ColdEdgeFade ||
        !R23BPreservedPackageSnapshotsMatch(PreservedSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    bool bColdPackagesClean = ColdEdgeFade->GetOutermost() &&
        !ColdEdgeFade->GetOutermost()->IsDirty();
    for (UMaterial* Material : ColdMaterials)
    {
        bColdPackagesClean &= Material && Material->GetOutermost() &&
            !Material->GetOutermost()->IsDirty();
    }
    FString FinalPinReport;
    if (!bColdPackagesClean ||
        !ValidateR23BExactDiskPins(true, FinalPinReport, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_COLD_VALIDATION: clean packages, exact preserved pins, and the configured final-pin seal policy are mandatory. ") +
            Error);
    }

    if (Backup.ExpectedPackageCount != 6 || Backup.Packages.Num() != 6 ||
        Backup.ExpectedArtifactsPerPackage != 7)
    {
        return FailWithRollback(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_BACKUP_READBACK: the exact six-package, seven-artifact backup roster is incomplete."));
    }
    FString HashDeltas;
    FString FinalPins;
    for (const FMaterialPackageBackup& Package : Backup.Packages)
    {
        if (Package.Artifacts.Num() != 7 ||
            !Package.Artifacts[0].bPresent)
        {
            return FailWithRollback(
                TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_BACKUP_READBACK: a canonical target state is incomplete."));
        }
        const FString NewMd5 = FileMd5(Package.PackageFilename);
        FString NewSha256;
        int64 NewBytes = INDEX_NONE;
        if (NewMd5.IsEmpty() || NewMd5 == Package.Artifacts[0].Md5 ||
            !HashFileSha256R20(
                Package.PackageFilename, NewSha256, NewBytes, Error) ||
            NewBytes <= 0 || NewSha256.IsEmpty())
        {
            return FailWithRollback(
                TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_FAILED_DISK_DELTA: every R23B target must acquire a distinct, readable canonical package hash. ") +
                Error);
        }
        if (!HashDeltas.IsEmpty())
        {
            HashDeltas += TEXT(",");
            FinalPins += TEXT(",");
        }
        const FString Stem = FPaths::GetBaseFilename(Package.PackageFilename);
        HashDeltas += Stem + TEXT(":") + Package.Artifacts[0].Md5 +
            TEXT("->") + NewMd5;
        FinalPins += FString::Printf(
            TEXT("%s:%lld:%s"), *Stem, NewBytes, *NewSha256);
    }

    if (R23BFinalPinsAreSealed)
    {
        OutReport = FString::Printf(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_PASS publicationReady=true finalPinsSealed=true uniformR23Input=true exactR23PredecessorPins=true saveTargets=6 reloadTargets=6 diskColdValidated=true coldValidatedMaterials=7 coldPackagesClean=true targetArtifacts=42 absentTargetSidecars=36 %s backup=%s deltas=%s finalPins=%s %s"),
            *CommonReport,
            *Backup.Directory,
            *HashDeltas,
            *FinalPins,
            *FinalPinReport);
    }
    else
    {
        OutReport = FString::Printf(
            TEXT("V5D_R23B_GRASS_SYSTEM_UPGRADE_BOOTSTRAP_PASS publicationReady=false finalPinsSealed=false bootstrapState=CAPTURE_FINAL_PACKAGE_PINS_AND_SEAL_SOURCE uniformR23Input=true exactR23PredecessorPins=true saveTargets=6 reloadTargets=6 diskColdValidated=true coldValidatedMaterials=7 coldPackagesClean=true targetArtifacts=42 absentTargetSidecars=36 %s backup=%s deltas=%s sealCandidatePins=%s %s"),
            *CommonReport,
            *Backup.Directory,
            *HashDeltas,
            *FinalPins,
            *FinalPinReport);
    }
    return true;
}

// The material-only R23B transaction ends above. The independent
// UpgradeGroundVegetationEdgeGrassFadeAssetToR11 endpoint remains below the
// serialized R23 presentation endpoint and is never invoked by R23B.
bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationSerializedPresentationToR23(FString& OutReport)
{
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress())
    {
        OutReport = TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_EDITOR_STATE: a non-PIE editor is required.");
        return false;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    UPackage* MapPackage = World ? World->GetOutermost() : nullptr;
    if (!World || !MapPackage ||
        MapPackage->GetName() != TargetMapPackage ||
        MapPackage->IsDirty())
    {
        OutReport = TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_MAP: load the exact clean /Game/Maps/Istana_PublicView_Explore_v5d_hybrid map first.");
        return false;
    }

    int32 GroundActorCount = 0;
    ATRIADIstanaExploreV5DGroundVegetationActor* Actor =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            World,
            GroundActorCount);
    if (GroundActorCount != 1 || !Actor || Actor->GetWorld() != World ||
        Actor->GetClass() !=
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() ||
        !Actor->Tags.Contains(GroundVegetationTag))
    {
        OutReport = FString::Printf(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_ACTOR: expected exactly one tagged exact-class ground actor in the loaded target world; found=%d exactClass=%s tagged=%s."),
            GroundActorCount,
            Actor && Actor->GetClass() ==
                    ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass()
                ? TEXT("true")
                : TEXT("false"),
            Actor && Actor->Tags.Contains(GroundVegetationTag)
                ? TEXT("true")
                : TEXT("false"));
        return false;
    }

    FString Error;
    FString MaterialPinReport;
    if (!ValidateR23LayoutMaterialDiskPins(MaterialPinReport, Error))
    {
        OutReport = TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_R23_MATERIAL_PINS: ") +
            Error;
        return false;
    }
    TArray<FR11PreservedPackageSnapshot> ImmutableSnapshots;
    if (!CaptureR20ImmutablePackageSnapshots(ImmutableSnapshots, Error))
    {
        OutReport = TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_IMMUTABLE_ROSTER: ") +
            Error;
        return false;
    }

    FString MapFilename;
    if (!FPackageName::DoesPackageExist(TargetMapPackage, &MapFilename))
    {
        OutReport = TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_DISK_MAP: the exact target map artifact is absent.");
        return false;
    }
    MapFilename = FPaths::ConvertRelativePathToFull(MapFilename);
    FPaths::NormalizeFilename(MapFilename);
    FString CurrentSha256;
    int64 CurrentBytes = INDEX_NONE;
    if (!HashFileSha256R20(
            MapFilename,
            CurrentSha256,
            CurrentBytes,
            Error))
    {
        OutReport = TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_DISK_HASH: ") +
            Error;
        return false;
    }

    FString CurrentR23Report;
    const bool bCurrentR23 = !Actor->HasActorBegunPlay() &&
        Actor->ValidateGroundVegetationRealism(CurrentR23Report);
    FString PredecessorReport;
    const bool bR20Predecessor =
        Actor->ValidateR20PredecessorForR23Migration(PredecessorReport);
    if (bCurrentR23 == bR20Predecessor)
    {
        OutReport = FString::Printf(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_REVISION_STATE: the actor must be exactly one cold R20 predecessor or one cold valid R23 result, never neither or both. currentR23=%s r20Predecessor=%s actorR23={%s} predecessor={%s}"),
            bCurrentR23 ? TEXT("true") : TEXT("false"),
            bR20Predecessor ? TEXT("true") : TEXT("false"),
            *CurrentR23Report,
            *PredecessorReport);
        return false;
    }
    if (bCurrentR23 &&
        (CurrentBytes != R23FinalMapBytes ||
         CurrentSha256 != R23FinalMapSha256))
    {
        OutReport = FString::Printf(
            TEXT("V5D_R23_GRASS_LAYOUT_IDEMPOTENCE_REFUSED_CURRENT_MAP_PIN expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s actor={%s}"),
            R23FinalMapBytes,
            CurrentBytes,
            *R23FinalMapSha256,
            *CurrentSha256,
            *CurrentR23Report);
        return false;
    }

    const auto ColdLoadAndValidateR23 =
        [&MapFilename, &ImmutableSnapshots](
            int64& OutMapBytes,
            FString& OutMapSha256,
            FString& OutActorReport,
            FString& OutMaterialPinReport,
            FString& OutColdError) -> bool
    {
        OutMapBytes = INDEX_NONE;
        OutMapSha256.Reset();
        OutActorReport.Reset();
        OutMaterialPinReport.Reset();
        OutColdError.Reset();
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (!CurrentPackage ||
            CurrentPackage->GetName() != TargetMapPackage ||
            CurrentPackage->IsDirty())
        {
            OutColdError = TEXT("the exact target map was absent or dirty before cold unload");
            return false;
        }

        FString UnloadFilename;
        UWorld* UnloadWorld =
            FPackageName::DoesPackageExist(
                R20UnloadMapPackage,
                &UnloadFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(UnloadFilename)
            : nullptr;
        if (!UnloadWorld || !UnloadWorld->GetOutermost() ||
            UnloadWorld->GetOutermost()->GetName() != R20UnloadMapPackage)
        {
            OutColdError = TEXT("the exact V5B unload map did not load");
            return false;
        }

        UWorld* ReloadedWorld =
            UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
        if (ReloadedWorld)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* ReloadedPackage = ReloadedWorld
            ? ReloadedWorld->GetOutermost()
            : nullptr;
        int32 ReloadedActorCount = 0;
        ATRIADIstanaExploreV5DGroundVegetationActor* ReloadedActor =
            ReloadedWorld
            ? FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
                  ReloadedWorld,
                  ReloadedActorCount)
            : nullptr;
        const bool bActorValid = ReloadedActorCount == 1 && ReloadedActor &&
            !ReloadedActor->HasActorBegunPlay() &&
            ReloadedActor->GetClass() ==
                ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() &&
            ReloadedActor->Tags.Contains(GroundVegetationTag) &&
            ReloadedActor->ValidateGroundVegetationRealism(OutActorReport);
        FString MaterialError;
        const bool bMaterialsValid = ValidateR23LayoutMaterialDiskPins(
            OutMaterialPinReport,
            MaterialError);
        FString ImmutableError;
        const bool bImmutable = R20ImmutablePackageSnapshotsMatch(
            ImmutableSnapshots,
            ImmutableError);
        FString HashError;
        const bool bHashValid = HashFileSha256R20(
            MapFilename,
            OutMapSha256,
            OutMapBytes,
            HashError) &&
            OutMapBytes > 0 && !OutMapSha256.IsEmpty();
        if (!ReloadedWorld || !ReloadedPackage ||
            ReloadedPackage->GetName() != TargetMapPackage ||
            ReloadedPackage->IsDirty() || !bActorValid ||
            !bMaterialsValid || !bImmutable || !bHashValid)
        {
            OutColdError = FString::Printf(
                TEXT("package=%s clean=%s actorCount=%d actorValid=%s materialsValid=%s immutable=%s hashValid=%s actor={%s} materialPins={%s} materialError={%s} immutableError={%s} hashError={%s}"),
                ReloadedPackage
                    ? *ReloadedPackage->GetName()
                    : TEXT("<null>"),
                ReloadedPackage && !ReloadedPackage->IsDirty()
                    ? TEXT("true")
                    : TEXT("false"),
                ReloadedActorCount,
                bActorValid ? TEXT("true") : TEXT("false"),
                bMaterialsValid ? TEXT("true") : TEXT("false"),
                bImmutable ? TEXT("true") : TEXT("false"),
                bHashValid ? TEXT("true") : TEXT("false"),
                *OutActorReport,
                *OutMaterialPinReport,
                *MaterialError,
                *ImmutableError,
                *HashError);
            return false;
        }
        return true;
    };

    if (bCurrentR23)
    {
        int64 ColdMapBytes = INDEX_NONE;
        FString ColdMapSha256;
        FString ColdActorReport;
        FString ColdMaterialPinReport;
        FString ColdError;
        if (!ColdLoadAndValidateR23(
                ColdMapBytes,
                ColdMapSha256,
                ColdActorReport,
                ColdMaterialPinReport,
                ColdError))
        {
            OutReport = TEXT("V5D_R23_GRASS_LAYOUT_IDEMPOTENCE_REFUSED_COLD_VALIDATION: ") +
                ColdError;
            return false;
        }
        if (ColdMapBytes != R23FinalMapBytes ||
            ColdMapSha256 != R23FinalMapSha256)
        {
            OutReport = FString::Printf(
                TEXT("V5D_R23_GRASS_LAYOUT_IDEMPOTENCE_REFUSED_FINAL_MAP_PIN expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s"),
                R23FinalMapBytes,
                ColdMapBytes,
                *R23FinalMapSha256,
                *ColdMapSha256);
            return false;
        }
        OutReport = FString::Printf(
            TEXT("IDEMPOTENT_V5D_R23_GRASS_LAYOUT_ALREADY_VALID operation=R20ToR23PhotographicGrassLayout mapBytes=%lld mapSha256=%s exactR23FinalMapPin=true actorRevision=%d exactOneTaggedActor=true exactActorClass=true saveTargets=0 mapsSaved=0 unloadTargets=1 reloadTargets=1 diskColdValidated=true exactR23MaterialPins=6 exactSoilPin=true immutableMaterialSourceTextureArtifacts=14 immutablePackageHashesPreserved=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true materialPins={%s} actor={%s}"),
            ColdMapBytes,
            *ColdMapSha256,
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGrassPresentationRevision(),
            *ColdMaterialPinReport,
            *ColdActorReport);
        return true;
    }

    if (CurrentBytes != R20FinalMapBytes ||
        CurrentSha256 != R20FinalMapSha256)
    {
        OutReport = FString::Printf(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_PREDECESSOR_MAP_PIN expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s predecessor={%s}"),
            R20FinalMapBytes,
            CurrentBytes,
            *R20FinalMapSha256,
            *CurrentSha256,
            *PredecessorReport);
        return false;
    }

    FR20MapBackup Backup;
    if (!CreateVerifiedR23MapBackup(Backup, Error))
    {
        OutReport = TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_REFUSED_BACKUP: map was not changed. ") +
            Error;
        return false;
    }

    const auto ColdRestoreExactR20 =
        [&Backup, &ImmutableSnapshots](FString& OutRollbackReport) -> bool
    {
        UWorld* CurrentWorld = GEditor
            ? GEditor->GetEditorWorldContext().World()
            : nullptr;
        UPackage* CurrentPackage = CurrentWorld
            ? CurrentWorld->GetOutermost()
            : nullptr;
        if (CurrentPackage &&
            CurrentPackage->GetName() == TargetMapPackage)
        {
            CurrentPackage->SetDirtyFlag(false);
        }

        FString UnloadFilename;
        UWorld* UnloadWorld =
            FPackageName::DoesPackageExist(
                R20UnloadMapPackage,
                &UnloadFilename)
            ? UEditorLoadingAndSavingUtils::LoadMap(UnloadFilename)
            : nullptr;
        if (!UnloadWorld || !UnloadWorld->GetOutermost() ||
            UnloadWorld->GetOutermost()->GetName() != R20UnloadMapPackage)
        {
            OutRollbackReport = TEXT("rollback could not unload the target through exact /Game/Maps/Istana_PublicView_Explore_v5b; no disk copy was attempted and the sealed backup remains authoritative");
            return false;
        }

        FString RestoreError;
        if (!RestoreR23MapArtifactsOnDisk(Backup, RestoreError))
        {
            OutRollbackReport = TEXT("rollback unloaded the target but failed exact six-artifact R20 restore: ") +
                RestoreError + TEXT(" backup=") + Backup.Directory;
            return false;
        }

        UWorld* RestoredWorld =
            UEditorLoadingAndSavingUtils::LoadMap(Backup.PackageFilename);
        if (RestoredWorld)
        {
            FAssetCompilingManager::Get().FinishAllCompilation();
        }
        UPackage* RestoredPackage = RestoredWorld
            ? RestoredWorld->GetOutermost()
            : nullptr;
        int32 RestoredActorCount = 0;
        ATRIADIstanaExploreV5DGroundVegetationActor* RestoredActor =
            RestoredWorld
            ? FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
                  RestoredWorld,
                  RestoredActorCount)
            : nullptr;
        FString RestoredPredecessorReport;
        const bool bRestoredActorValid = RestoredActorCount == 1 &&
            RestoredActor && !RestoredActor->HasActorBegunPlay() &&
            RestoredActor->GetClass() ==
                ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() &&
            RestoredActor->Tags.Contains(GroundVegetationTag) &&
            RestoredActor->ValidateR20PredecessorForR23Migration(
                RestoredPredecessorReport);
        FString RestoredSha256;
        int64 RestoredBytes = INDEX_NONE;
        FString HashError;
        const bool bRestoredHash = HashFileSha256R20(
            Backup.PackageFilename,
            RestoredSha256,
            RestoredBytes,
            HashError) &&
            RestoredBytes == R20FinalMapBytes &&
            RestoredSha256 == R20FinalMapSha256;
        FString RestoredMaterialPinReport;
        FString MaterialError;
        const bool bRestoredMaterials = ValidateR23LayoutMaterialDiskPins(
            RestoredMaterialPinReport,
            MaterialError);
        FString ImmutableError;
        const bool bImmutable = R20ImmutablePackageSnapshotsMatch(
            ImmutableSnapshots,
            ImmutableError);
        const bool bRestored = RestoredWorld && RestoredPackage &&
            RestoredPackage->GetName() == TargetMapPackage &&
            !RestoredPackage->IsDirty() && bRestoredActorValid &&
            bRestoredHash && bRestoredMaterials && bImmutable;
        OutRollbackReport = FString::Printf(
            TEXT("coldR20Restore=%s mapPackage=%s clean=%s actorCount=%d exactActor=%s bytes=%lld sha256=%s r23MaterialPins=%s immutableArtifacts=%s backup=%s predecessor={%s} materialPins={%s} hashError={%s} materialError={%s} immutableError={%s}"),
            bRestored ? TEXT("true") : TEXT("false"),
            RestoredPackage ? *RestoredPackage->GetName() : TEXT("<null>"),
            RestoredPackage && !RestoredPackage->IsDirty()
                ? TEXT("true")
                : TEXT("false"),
            RestoredActorCount,
            bRestoredActorValid ? TEXT("true") : TEXT("false"),
            RestoredBytes,
            *RestoredSha256,
            bRestoredMaterials ? TEXT("true") : TEXT("false"),
            bImmutable ? TEXT("true") : TEXT("false"),
            *Backup.Directory,
            *RestoredPredecessorReport,
            *RestoredMaterialPinReport,
            *HashError,
            *MaterialError,
            *ImmutableError);
        return bRestored;
    };

    const auto FailWithRollback =
        [&OutReport, &Backup, &ColdRestoreExactR20](
            const FString& Failure) -> bool
    {
        FString RollbackReport;
        const bool bRolledBack = ColdRestoreExactR20(RollbackReport);
        OutReport = Failure + (bRolledBack
            ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK mapArtifacts=6 coldReloaded=true predecessorBytes=32390049 predecessorSha256=4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33 exactR23MaterialPinsPreserved=true exactSoilPinPreserved=true immutablePackageHashesPreserved=true backup=")) +
                Backup.Directory + TEXT(" rollback={") +
                RollbackReport + TEXT("}")
            : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED backup=")) +
                Backup.Directory + TEXT(" rollback={") +
                RollbackReport + TEXT("}"));
        return false;
    };

    FString RebuildReport;
    if (!Actor->RebuildGroundVegetationLayoutToR23(RebuildReport))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_FAILED_REBUILD: ") +
            RebuildReport);
    }
    FString PreSaveActorReport;
    FString PreSaveMaterialPinReport;
    if (!Actor->ValidateGroundVegetationRealism(PreSaveActorReport) ||
        Actor->GetClass() !=
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() ||
        !Actor->Tags.Contains(GroundVegetationTag) ||
        !ValidateR23LayoutMaterialDiskPins(
            PreSaveMaterialPinReport,
            Error) ||
        !R20ImmutablePackageSnapshotsMatch(ImmutableSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_FAILED_PRE_SAVE_VALIDATION: actor={") +
            PreSaveActorReport + TEXT("} materials={") +
            PreSaveMaterialPinReport + TEXT("} error={") + Error +
            TEXT("}"));
    }

    if (!PersistExactR23TargetWorld(World))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_FAILED_SAVE: only the exact target map was offered to the target-world persistence boundary."));
    }
    if (!ValidateR23LayoutMaterialDiskPins(
            MaterialPinReport,
            Error) ||
        !R20ImmutablePackageSnapshotsMatch(ImmutableSnapshots, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_FAILED_POST_SAVE_IMMUTABLE_DRIFT: ") +
            Error);
    }

    int64 FinalBytes = INDEX_NONE;
    FString FinalSha256;
    FString ColdActorReport;
    FString ColdMaterialPinReport;
    FString ColdError;
    if (!ColdLoadAndValidateR23(
            FinalBytes,
            FinalSha256,
            ColdActorReport,
            ColdMaterialPinReport,
            ColdError))
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_FAILED_COLD_VALIDATION: ") +
            ColdError);
    }
    if (FinalBytes != R23FinalMapBytes ||
        FinalSha256 != R23FinalMapSha256)
    {
        return FailWithRollback(FString::Printf(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_FAILED_FINAL_PIN: expectedBytes=%lld actualBytes=%lld expectedSha256=%s actualSha256=%s"),
            R23FinalMapBytes,
            FinalBytes,
            *R23FinalMapSha256,
            *FinalSha256));
    }

    int32 ChangedMapArtifacts = 0;
    for (const FR20MapArtifactBackup& Artifact : Backup.Artifacts)
    {
        const int64 Bytes = IFileManager::Get().FileSize(*Artifact.Original);
        const bool bPresent = Bytes >= 0;
        FString Md5;
        if (bPresent)
        {
            Md5 = FileMd5(Artifact.Original);
        }
        if (bPresent != Artifact.bPresent || Bytes != Artifact.Bytes ||
            Md5 != Artifact.Md5)
        {
            ++ChangedMapArtifacts;
        }
    }
    if (Backup.Artifacts.Num() != 6 || ChangedMapArtifacts < 1)
    {
        return FailWithRollback(
            TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_FAILED_NO_DISK_DELTA: the exact six-slot serialized map roster did not acquire a changed artifact."));
    }

    OutReport = FString::Printf(
        TEXT("V5D_R23_GRASS_LAYOUT_UPGRADE_PASS operation=R20ToR23PhotographicGrassLayout predecessorRevision=%d actorRevision=%d exactOneTaggedActor=true exactActorClass=true inPlace=true mapOnlyTransaction=true exactR20PredecessorMapPin=true predecessorBytes=%lld predecessorSha256=%s saveTargets=1 mapsSaved=1 materialPackagesSaved=0 unloadMap=/Game/Maps/Istana_PublicView_Explore_v5b coldReloaded=true coldValidated=true mapArtifactsBackedUp=6 changedMapArtifacts=%d measuredFinalBytes=%lld measuredFinalSha256=%s exactR23FinalMapPin=true exactR23MaterialPins=6 exactSoilPin=true immutableMaterialSourceTextureArtifacts=14 immutablePackageHashesPreserved=true sourceV3V4V5BMaterialAssetsUntouched=true collisionPreserved=true navigationPreserved=true sensorAndRfAuthorityPreserved=true globalLightingUntouched=true backup=%s materialPins={%s} actor={%s}"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedR23PredecessorGrassPresentationRevision(),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGrassPresentationRevision(),
        R20FinalMapBytes,
        *R20FinalMapSha256,
        ChangedMapArtifacts,
        FinalBytes,
        *FinalSha256,
        *Backup.Directory,
        *ColdMaterialPinReport,
        *ColdActorReport);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    UpgradeGroundVegetationEdgeGrassFadeAssetToR11(FString& OutReport)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        OutReport = TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_REFUSED: a non-PIE editor is required.");
        return false;
    }
    UEditorAssetSubsystem* Assets =
        GEditor->GetEditorSubsystem<UEditorAssetSubsystem>();
    if (!Assets)
    {
        OutReport = TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UMaterial*> CoreMaterials;
    UMaterial* Source = LoadExact<UMaterial>(SourceEdgeGrassMaterialPath);
    FString CoreRevision;
    FString Error;
    if (!ValidateUniformR13R15OrR16MaterialAssetsInternal(
            CoreMaterials, CoreRevision, Error) ||
        CoreMaterials.Num() != 6 ||
        !ValidateEdgeGrassSourceMaterial(Source, Error) ||
        !Source || !Source->GetOutermost() ||
        Source->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_REFUSED_ROSTER: the sole migration input is six uniformly valid clean R13, R15 or R16 core packages plus the exact clean V4 source. ") +
            Error;
        return false;
    }
    for (UMaterial* Core : CoreMaterials)
    {
        if (!Core || !Core->GetOutermost() ||
            Core->GetOutermost()->IsDirty())
        {
            OutReport = TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_REFUSED_ROSTER: every uniformly admitted R13 or R15 core package must be loaded and clean.");
            return false;
        }
    }
    const FString EdgePackageName = PackagePath(EdgeGrassFadeAssetName);
    const FString EdgeObjectPath = ObjectPath(EdgeGrassFadeAssetName);
    const bool bEdgePackageOnDisk =
        FPackageName::DoesPackageExist(EdgePackageName);
    UPackage* LoadedEdgePackage = FindPackage(nullptr, *EdgePackageName);
    UMaterial* ExistingEdge = FindObject<UMaterial>(nullptr, *EdgeObjectPath);
    FAssetData ExistingEdgeAssetData =
        IAssetRegistry::GetChecked().GetAssetByObjectPath(
            FSoftObjectPath(EdgeObjectPath));
    if (bEdgePackageOnDisk && !ExistingEdge)
    {
        ExistingEdge = LoadExact<UMaterial>(EdgeObjectPath);
        LoadedEdgePackage = FindPackage(nullptr, *EdgePackageName);
        ExistingEdgeAssetData =
            IAssetRegistry::GetChecked().GetAssetByObjectPath(
                FSoftObjectPath(EdgeObjectPath));
    }
    TArray<FR11PreservedPackageSnapshot> PreservedBefore;
    if (!CaptureR11PreservedPackageSnapshots(PreservedBefore, Error))
    {
        OutReport = TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_REFUSED_INVARIANTS: ") +
            Error;
        return false;
    }
    if (bEdgePackageOnDisk || LoadedEdgePackage || ExistingEdge ||
        ExistingEdgeAssetData.IsValid())
    {
        if (!bEdgePackageOnDisk || !LoadedEdgePackage || !ExistingEdge ||
            !ExistingEdgeAssetData.IsValid() ||
            !ExistingEdge->GetOutermost() ||
            ExistingEdge->GetOutermost()->IsDirty() ||
            !ValidateEdgeGrassFadeMaterial(ExistingEdge, Error) ||
            !R11PreservedPackageSnapshotsMatch(PreservedBefore, Error))
        {
            OutReport = TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_REFUSED_MIXED: the seventh package is partial, dirty, invalid, or accompanied by immutable-package drift. ") +
                Error;
            return false;
        }
        OutReport = FString::Printf(
            TEXT("IDEMPOTENT_V5D_R11_EDGE_GRASS_FADE_ALREADY_VALID coreRevision=%s corePackages=6 edgePackages=1 totalMaterials=7 exact21Expressions=true exactTextureSamples=4 sourceOpacityMaskOutput=1_red ditherTemporalAaPerInstanceFade=true sourceV4HashPreserved=true coreHashesPreserved=true mapHashPreserved=true mapsSaved=0 restartBeforeCookRequired=true"),
            *CoreRevision);
        return true;
    }

    FR11AbsentPackageBackup Backup;
    if (!BackUpAbsentR11PackageState(Backup, Error))
    {
        OutReport = TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_REFUSED_BACKUP: ") +
            Error;
        return false;
    }

    UMaterial* NewEdge = nullptr;
    const auto FailWithRollback = [
        &OutReport,
        &NewEdge,
        &Backup,
        &PreservedBefore,
        &CoreRevision](const FString& Failure)
    {
        NewEdge = nullptr;
        FString RollbackError;
        bool bRolledBack = RestoreAbsentR11PackageAtomically(
            Backup,
            RollbackError);
        FString InvariantError;
        if (!R11PreservedPackageSnapshotsMatch(
                PreservedBefore,
                InvariantError))
        {
            bRolledBack = false;
            RollbackError += TEXT(" Immutable ") + CoreRevision +
                TEXT("/V4/map verification failed: ") +
                InvariantError;
        }
        OutReport = Failure +
            (bRolledBack
                ? FString(TEXT(" AUTOMATIC_ROLLBACK_OK newPackageArtifacts=6 restoredToAbsent=true unloadedBeforeDelete=true assetRegistryRescanned=true coreRevision=")) +
                    CoreRevision +
                    TEXT(" coreHashesPreserved=true sourceV4HashPreserved=true mapHashPreserved=true mapsSaved=0 backup=") +
                    Backup.Directory
                : FString(TEXT(" AUTOMATIC_ROLLBACK_FAILED artifactsAndBackupRetainedWhenUnloadUnproven=true ")) +
                    RollbackError);
        return false;
    };

    if (!CreateEdgeGrassFadeMaterial(Source, NewEdge, Error) ||
        !NewEdge || NewEdge->GetPathName() !=
            ObjectPath(EdgeGrassFadeAssetName))
    {
        return FailWithRollback(
            TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_FAILED_CREATE: ") +
            Error);
    }
    FAssetCompilingManager::Get().FinishAllCompilation();
    TArray<UObject*> ExactSaveTargets = {NewEdge};
    if (ExactSaveTargets.Num() != 1 ||
        !Assets->SaveLoadedAssets(ExactSaveTargets, false))
    {
        ExactSaveTargets.Reset();
        return FailWithRollback(
            TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_FAILED_SAVE: only the exact new derivative package was offered to save; no R13/R15 core, V4, soil or map package was offered."));
    }
    ExactSaveTargets.Reset();
    NewEdge = nullptr;
    if (!ReloadSingleR11MaterialPackage(Error))
    {
        return FailWithRollback(
            TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_FAILED_RELOAD: ") +
            Error);
    }

    TArray<UMaterial*> ColdCoreMaterials;
    UMaterial* ColdEdge = nullptr;
    bool bAllSevenColdPackagesClean =
        (CoreRevision == TEXT("R16")
            ? ValidateCompleteR16MaterialAssets(
                ColdCoreMaterials, ColdEdge, Error)
            : (CoreRevision == TEXT("R15")
                ? ValidateCompleteR15MaterialAssets(
                    ColdCoreMaterials, ColdEdge, Error)
                : ValidateCompleteR13MaterialAssets(
                    ColdCoreMaterials, ColdEdge, Error))) &&
        ColdCoreMaterials.Num() == 6 && ColdEdge;
    for (UMaterial* Core : ColdCoreMaterials)
    {
        bAllSevenColdPackagesClean &= Core && Core->GetOutermost() &&
            !Core->GetOutermost()->IsDirty();
    }
    bAllSevenColdPackagesClean &= ColdEdge && ColdEdge->GetOutermost() &&
        !ColdEdge->GetOutermost()->IsDirty();
    if (!bAllSevenColdPackagesClean ||
        !R11PreservedPackageSnapshotsMatch(PreservedBefore, Error))
    {
        ColdEdge = nullptr;
        return FailWithRollback(
            TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_FAILED_COLD_VALIDATION: ") +
            Error);
    }
    ColdEdge = nullptr;

    TArray<FPackageArtifactSnapshot> CreatedArtifacts;
    if (!CaptureObjectPackageArtifacts(
            ObjectPath(EdgeGrassFadeAssetName),
            CreatedArtifacts,
            Error) ||
        CreatedArtifacts.Num() != 6 ||
        !CreatedArtifacts[0].bPresent ||
        CreatedArtifacts[0].Md5.IsEmpty() ||
        !PackageArtifactsMatch(PreservedBefore[0].Artifacts, Error) ||
        !R11PreservedPackageSnapshotsMatch(PreservedBefore, Error))
    {
        return FailWithRollback(
            TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_FAILED_ARTIFACT_SEAL: ") +
            Error);
    }

    OutReport = FString::Printf(
        TEXT("V5D_R11_EDGE_GRASS_FADE_UPGRADE_PASS newPackages=1 saveTargets=1 reloadTargets=1 coldValidatedMaterials=7 exact21Expressions=true exactTextureSamples=4 sourceOpacityMaskOutput=1_red opacityMask=DitherTemporalAA_PerInstanceFade_times_red sourceV4NineInputWpo5cmPreserved=true twoSidedNormalCorrectionPreserved=true textureUvSamplerMipAuxiliaryStatePreserved=true compiledActiveFeatureLevelValidated=true fullNewArtifactRoster=6 initialNewArtifactsAbsent=true unloadBeforeRollbackDelete=true assetRegistryRollbackRescan=true coreRevision=%s corePackagesHashPreserved=6 sourceV4HashPreserved=true mapHashPreserved=true mapsSaved=0 serializedNativeCdoHardReferenceRequiresFreshRestartBeforeCook=true freshCookManifestAndPackagedLoadRequired=true backup=%s canonicalMd5=%s"),
        *CoreRevision,
        *Backup.Directory,
        *CreatedArtifacts[0].Md5);
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    ApplyGroundVegetationRealismPassToLoadedV5DHybridMap(
        FString& OutMessage)
{
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    if (!World || !World->GetOutermost() ||
        World->GetOutermost()->GetName() != TargetMapPackage)
    {
        OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLY_REFUSED: load the exact V5D hybrid destination map first.");
        return false;
    }
    return ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder(
        World,
        false,
        OutMessage);
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    ApplyGroundVegetationRealismPassToWorldForTrustedHybridBuilder(
        UWorld* World,
        bool bTrustedUntitledHybridBuilder,
        FString& OutMessage)
{
    const bool bExactTarget = World && World->GetOutermost() &&
        World->GetOutermost()->GetName() == TargetMapPackage;
    const bool bTrustedTemp = World && World->GetOutermost() &&
        bTrustedUntitledHybridBuilder &&
        FPackageName::IsTempPackage(World->GetOutermost()->GetName());
    if (!bExactTarget && !bTrustedTemp)
    {
        OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLY_REFUSED_WORLD: only the exact target or an explicitly trusted untitled hybrid-builder world is admitted.");
        return false;
    }

    TArray<UMaterial*> Materials;
    FString GrassRevision;
    FString Error;
    if (!EnsureCompleteR11MaterialAssets(
            Materials,
            GrassRevision,
            Error))
    {
        OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLY_REFUSED_MATERIALS: ") + Error;
        return false;
    }
    const FString ExpectedGrassRevisionMarker =
        TEXT("grassMaterialRevision=") + GrassRevision;

    int32 SceneCount = 0;
    int32 V4Count = 0;
    int32 V5BCount = 0;
    int32 ExistingCount = 0;
    ATRIADIstanaPublicViewSceneActor* Scene =
        FindExactlyOne<ATRIADIstanaPublicViewSceneActor>(World, SceneCount);
    ATRIADIstanaExploreV4LandscapeActor* V4 =
        FindExactlyOne<ATRIADIstanaExploreV4LandscapeActor>(World, V4Count);
    ATRIADIstanaExploreV5BVisualActor* V5B =
        FindExactlyOne<ATRIADIstanaExploreV5BVisualActor>(World, V5BCount);
    ATRIADIstanaExploreV5DGroundVegetationActor* Existing =
        FindExactlyOne<ATRIADIstanaExploreV5DGroundVegetationActor>(
            World,
            ExistingCount);
    FString V5BReport;
    if (SceneCount != 1 || V4Count != 1 || V5BCount != 1 ||
        !Scene || !V4 || !V5B ||
        !V5B->ValidateExploreV5BVisuals(V5BReport))
    {
        OutMessage = FString::Printf(
            TEXT("V5D_GROUND_VEGETATION_APPLY_REFUSED_SOURCES: scene=%d V4=%d V5B=%d %s"),
            SceneCount,
            V4Count,
            V5BCount,
            *V5BReport);
        return false;
    }

    if (ExistingCount == 1 && Existing)
    {
        FString ExistingReport;
        if (Existing->Tags.Contains(GroundVegetationTag) &&
            Existing->ValidateGroundVegetationRealism(ExistingReport) &&
            ExistingReport.Contains(
                ExpectedGrassRevisionMarker,
                ESearchCase::CaseSensitive))
        {
            OutMessage = TEXT("IDEMPOTENT_V5D_GROUND_VEGETATION_ALREADY_VALID: ") +
                ExistingReport;
            return true;
        }
        OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLY_REFUSED_EXISTING_INVALID: ") +
            ExistingReport;
        return false;
    }
    if (ExistingCount != 0)
    {
        OutMessage = FString::Printf(
            TEXT("V5D_GROUND_VEGETATION_APPLY_REFUSED_DUPLICATES: found %d actors."),
            ExistingCount);
        return false;
    }

    FTRIADIstanaExploreV5DGroundVegetationAssets Roster;
    if (!BuildAssetRoster(Scene, V4, V5B, Materials, Roster, Error))
    {
        OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLY_REFUSED_ROSTER: ") + Error;
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIADIstanaExploreV5DGroundVegetation");
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ATRIADIstanaExploreV5DGroundVegetationActor* Actor =
        World->SpawnActor<ATRIADIstanaExploreV5DGroundVegetationActor>(
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Actor)
    {
        OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLY_FAILED_SPAWN: identity visual actor could not be spawned.");
        return false;
    }
    Actor->Tags.AddUnique(GroundVegetationTag);
    Actor->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D Ground and Vegetation Visual Assumption"));
    FString Report;
    if (!Actor->ConfigureGroundVegetationRealism(
            Scene,
            V4,
            V5B,
            Roster,
            ETRIADIstanaExploreV5DSeasonProfile::HumidWet,
            Error) ||
        !Actor->ValidateGroundVegetationRealism(Report) ||
        !Report.Contains(
            ExpectedGrassRevisionMarker,
            ESearchCase::CaseSensitive))
    {
        Actor->Destroy();
        OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLY_FAILED_CONFIGURATION: ") +
            Error + TEXT(" ") + Report;
        return false;
    }

    World->MarkPackageDirty();
    OutMessage = TEXT("V5D_GROUND_VEGETATION_APPLIED_CALLER_MUST_SAVE_MAP: ") +
        Report;
    return true;
}

bool UTRIADIstanaExploreV5DGroundVegetationEditorLibrary::
    ValidateGroundVegetationRealismPassInLoadedV5DHybridMap(
        FString& OutReport)
{
    TArray<UMaterial*> Materials;
    FString GrassRevision;
    FString Error;
    UMaterial* EdgeFade = nullptr;
    if (!ValidateCompleteCurrentMaterialAssets(
            Materials,
            EdgeFade,
            GrassRevision,
            Error))
    {
        OutReport = TEXT("V5D_GROUND_VEGETATION_VALIDATION_FAILED_MATERIALS: ") +
            Error;
        return false;
    }
    UWorld* World = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    ATRIADIstanaExploreV5DGroundVegetationActor* Actor = nullptr;
    if (!ValidateLoadedWorld(World, Actor, OutReport))
    {
        return false;
    }
    const FString ExpectedGrassRevisionMarker =
        TEXT("grassMaterialRevision=") + GrassRevision;
    if (!OutReport.Contains(
            ExpectedGrassRevisionMarker,
            ESearchCase::CaseSensitive))
    {
        OutReport = TEXT("V5D_GROUND_VEGETATION_VALIDATION_FAILED_REVISION_MISMATCH: the exact editor graph validator and actor runtime material signature disagree. expected=") +
            ExpectedGrassRevisionMarker + TEXT(" actor={") + OutReport +
            TEXT("}");
        return false;
    }
    OutReport += TEXT(" editorGrassMaterialGraphRevision=") + GrassRevision +
        TEXT(" editorGrassMaterialGraphValidated=true materialRevisionAdmission=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK validatedGrassRevisionSelection=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK");
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DR21MaterialContractTest,
    "TRIAD.Istana.ExploreV5D.GroundVegetation.R21MaterialContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DR21MaterialContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestEqual(
        TEXT("R21 profile count"),
        static_cast<int32>(UE_ARRAY_COUNT(GrassR21Profiles)),
        4);
    TestEqual(
        TEXT("R21 exact predecessor pin count"),
        static_cast<int32>(UE_ARRAY_COUNT(R21PredecessorGrassPins)),
        4);
    TestEqual(
        TEXT("Current material admission prefers complete R21"),
        static_cast<int32>(SelectCompleteGrassMaterialRevision(true, false)),
        static_cast<int32>(ECompleteGrassMaterialRevision::R21));
    TestEqual(
        TEXT("Current material admission retains complete R19 fallback"),
        static_cast<int32>(SelectCompleteGrassMaterialRevision(false, true)),
        static_cast<int32>(ECompleteGrassMaterialRevision::R19));
    TestEqual(
        TEXT("Current material admission rejects absent revisions"),
        static_cast<int32>(SelectCompleteGrassMaterialRevision(false, false)),
        static_cast<int32>(ECompleteGrassMaterialRevision::Invalid));
    TestEqual(
        TEXT("Current material admission rejects ambiguous revisions"),
        static_cast<int32>(SelectCompleteGrassMaterialRevision(true, true)),
        static_cast<int32>(ECompleteGrassMaterialRevision::Invalid));
    const FString ArtifactProbe = TEXT("C:/TRIAD/M_IPV5D_Turf_Manicured.uasset");
    const TArray<FString> LegacyArtifactRoster =
        PackageArtifactCandidates(ArtifactProbe);
    const TArray<FString> R21ArtifactRoster =
        R21PackageArtifactCandidates(ArtifactProbe);
    TestEqual(
        TEXT("Legacy material transactions retain six artifact states"),
        LegacyArtifactRoster.Num(),
        6);
    TestEqual(
        TEXT("R21 material transaction watches seven artifact states"),
        R21ArtifactRoster.Num(),
        7);
    TestTrue(
        TEXT("R21 seventh artifact is upayload"),
        R21ArtifactRoster[6].EndsWith(TEXT(".upayload")));
    TestEqual(TEXT("R20 map bytes remain pinned"), R20FinalMapBytes, 32390049LL);
    TestEqual(
        TEXT("R20 map SHA-256 remains pinned"),
        R20FinalMapSha256,
        FString(TEXT("4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33")));
    const int64 ExpectedBytes[] = {37718, 37645, 37706, 37805};
    const FString ExpectedSha256[] = {
        TEXT("42B3E4F81046415B3A65CE0F4F81A5D168F4FE270EF70E5872A21131645C6339"),
        TEXT("949324A4266C0A09684B213695295B0D2C9ADDDC310DB74F3FE34D12E10C9E51"),
        TEXT("AC8D77D34C4C9BCA5144949A1989C44E2F6F4DBD2702F9705977818763CF5109"),
        TEXT("197177AA26CF23A22E8E4297112441E75062EC643E43145A56065D7C7C0EB9DA")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        TestEqual(
            FString::Printf(TEXT("R21 predecessor bytes %d"), Index),
            R21PredecessorGrassPins[Index].Bytes,
            ExpectedBytes[Index]);
        TestEqual(
            FString::Printf(TEXT("R21 predecessor SHA-256 %d"), Index),
            FString(R21PredecessorGrassPins[Index].Sha256),
            ExpectedSha256[Index]);
        const FString Code = BuildR21GrassColorCode(GrassR21Profiles[Index]);
        TestTrue(
            FString::Printf(TEXT("R21 color code %d has exact far collapse"), Index),
            Code.Contains(TEXT("farColor=lerp(body,dryColor,0.90*dryFraction)")) &&
                Code.Contains(TEXT("farColor=lerp(farColor,thatchColor,0.08*thatchFraction)")) &&
                Code.Contains(TEXT("result=lerp(farColor,nearDetailedColor,detailWeight)")));
        TestFalse(
            FString::Printf(TEXT("R21 color code %d has no sine"), Index),
            Code.Contains(TEXT("sin("), ESearchCase::CaseSensitive));
        TestFalse(
            FString::Printf(TEXT("R21 color code %d has no cosine"), Index),
            Code.Contains(TEXT("cos("), ESearchCase::CaseSensitive));
    }
    TestTrue(
        TEXT("R21 roughness converges to exact far value"),
        GrassR21RoughnessCode.Contains(TEXT("lerp(0.74,BaseRoughness,detail)")));
    TestTrue(
        TEXT("R21 specular converges to exact far value"),
        GrassR21SpecularCode.Contains(TEXT("lerp(0.19,BaseSpecular,detail)")));
    TestTrue(
        TEXT("R21 normal reaches zero after its distance fade"),
        GrassR21NormalAlphaCode.Contains(TEXT("0.42*detail*bladeWeight")));
    TestEqual(TEXT("Calm wind strength is unchanged"), GrassWindStrengthCm, 0.48f);
    TestEqual(TEXT("Calm wind speed is unchanged"), GrassWindSpeed, 0.12f);
    TestEqual(TEXT("Calm wind height is unchanged"), GrassWindHeightCm, 4.8f);
    TestEqual(TEXT("Maximum WPO is unchanged"), GrassMaximumWpoCm, 0.55f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DR23LayoutContractTest,
    "TRIAD.Istana.ExploreV5D.GroundVegetation.R23LayoutContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DR23LayoutContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestEqual(
        TEXT("R23 layout material pin count"),
        static_cast<int32>(UE_ARRAY_COUNT(R23LayoutMaterialPins)),
        6);
    TestEqual(
        TEXT("R23 layout predecessor map bytes"),
        R20FinalMapBytes,
        32390049LL);
    TestEqual(
        TEXT("R23 layout predecessor map SHA-256"),
        R20FinalMapSha256,
        FString(TEXT("4A4291C6D921671B9F5D08928C91688CC9A28FBA2B229246D1466C4E89D44E33")));
    TestEqual(
        TEXT("R23 layout final map bytes"),
        R23FinalMapBytes,
        34986401LL);
    TestEqual(
        TEXT("R23 layout final map SHA-256"),
        R23FinalMapSha256,
        FString(TEXT("49A2879BE33704DDE1C6C0EFBAAE2364300E36EB1F0466B775EDE4F7BB4870DC")));
    TestEqual(
        TEXT("R23 layout soil bytes"),
        R22PreservedSoilBytes,
        12219LL);
    TestEqual(
        TEXT("R23 layout soil SHA-256"),
        R22PreservedSoilSha256,
        FString(TEXT("1BA32CE17F611EA14C6D42C5D125D9562C15E1C8517BA845DEA3FC51B889B5AD")));
    TestEqual(
        TEXT("R23 layout backup root"),
        R23LayoutBackupRelativeRoot,
        FString(TEXT("TRIAD/Backups/V5D_R23_GrassLayout")));
    TestEqual(
        TEXT("R23 layout backup receipt"),
        R23LayoutBackupReceiptHeader,
        FString(TEXT("TRIAD_V5D_R23_GRASS_LAYOUT_BACKUP_V1")));
    TestEqual(
        TEXT("R23 layout predecessor actor revision"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedR23PredecessorGrassPresentationRevision(),
        20);
    TestEqual(
        TEXT("R23 layout actor revision"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGrassPresentationRevision(),
        23);

    const FString ExpectedNames[] = {
        TEXT("M_IPV5D_Turf_Manicured"),
        TEXT("M_IPV5D_Turf_Humid"),
        TEXT("M_IPV5D_Turf_Shade"),
        TEXT("M_IPV5D_Turf_DryEdge"),
        TEXT("M_IPV5D_LawnMacroVariation"),
        TEXT("M_IPV5D_GrassMedium_EdgeFade")};
    const int64 ExpectedBytes[] = {
        41410, 41082, 41123, 41863, 37007, 35755};
    const FString ExpectedSha256[] = {
        TEXT("ADC96F6308597891F641981939F831C4BA3E77C0DC3F8B290F636D2B9B63203A"),
        TEXT("FD3354E36C9D30E16FDD2B8B0474E414B9775C76B1E53F10E9F0FEF29B2B71A0"),
        TEXT("7974B40558A698B689ECC6158F3EA92BFD063E6B6AA6980E18C5C90AEA9B41E7"),
        TEXT("5D1EDB35B52E779878C1B593373EA4C29B1AC64D625B80B4C4F8F1A7B4D530DE"),
        TEXT("DF1C5C7DADF704FA5F70093F791066F8AB2BD601AEF95C4158BF1E2B936A0249"),
        TEXT("FE68E9021BEDBA7A32DDEE7C1BF485287C6A0B98B26D684EEECFDEA774EE464E")};
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(R23LayoutMaterialPins);
         ++Index)
    {
        TestEqual(
            FString::Printf(TEXT("R23 layout material name %d"), Index),
            FString(R23LayoutMaterialPins[Index].AssetName),
            ExpectedNames[Index]);
        TestEqual(
            FString::Printf(TEXT("R23 layout material bytes %d"), Index),
            R23LayoutMaterialPins[Index].Bytes,
            ExpectedBytes[Index]);
        TestEqual(
            FString::Printf(TEXT("R23 layout material SHA-256 %d"), Index),
            FString(R23LayoutMaterialPins[Index].Sha256),
            ExpectedSha256[Index]);
    }
    return true;
}
#endif
