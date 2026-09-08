#include "TRIADIstanaExploreV5DGroundVegetationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5BVisualActor.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADIstanaExploreV5DGroundVegetation,
    Log,
    All);

namespace
{
const FString GroundVegetationClaimLabel(
    TEXT("ISTANA_EXPLORE_V5D_RENDER_ONLY_GROUND_VEGETATION_VISUAL_ASSUMPTION_NOT_SURVEY_NOT_AS_BUILT_NOT_BOTANICAL_INVENTORY_NOT_CURRENT_WEATHER_NOT_COLLISION_NAVIGATION_SENSOR_OR_RF_TRUTH"));

constexpr int32 SourceGrassCount = 18432;
constexpr int32 SourceEdgeGrassCount = 1536;
constexpr int32 SourceTreeCount = 729;
constexpr int32 ExistingSoilCount = 64;
constexpr int32 PredecessorGrassPresentationRevision = 14;
constexpr int32 GrassPresentationRevisionR20 = 20;
constexpr int32 GrassPresentationRevisionR23 = 23;

// Exact serialized R20 predecessor policy. These values stay isolated from
// the R23 builder so a cold R20 map can be proved before any mutation.
constexpr int32 R20GrassMicroDetailCount = 12288;
constexpr int32 R20MaximumGrassReuseLayers = 1;
constexpr double R20GrassLayerOffsetMinimumRadiusCm = 28.0;
constexpr double R20GrassLayerOffsetBandWidthCm = 10.0;
constexpr double R20GrassLayerOffsetBandStepCm = 0.0;
constexpr double R20GrassLayerVerticalSeparationCm = 0.04;
constexpr double R20GrassCoverageScaleMin = 0.52;
constexpr double R20GrassCoverageScaleMax = 0.72;
constexpr double R20GrassHeightScaleMin = 0.95;
constexpr double R20GrassHeightScaleMax = 1.12;
constexpr double R20GrassShortHeightClassScale = 0.92;
constexpr double R20GrassMediumHeightClassScale = 1.04;
constexpr double R20GrassTallHeightClassScale = 1.16;
constexpr int32 R20GrassCullStartDistanceCm = 2400;
constexpr int32 R20GrassCullEndDistanceCm = 3800;
constexpr int32 R20GrassWpoDisableDistanceCm = 2400;
constexpr int32 R20EdgeGrassCullStartDistanceCm = 3000;
constexpr int32 R20EdgeGrassCullEndDistanceCm = 4500;
constexpr int32 R20EdgeGrassWpoDisableDistanceCm = 2400;
constexpr float R20GrassLodDistanceScale = 0.60f;

// Current R23 modeled-blade layout. Two deterministically decorrelated layers
// increase near-field volume without changing collision, navigation or RF.
constexpr int32 GrassMicroDetailCount = 18432;
constexpr int32 MaximumGrassReuseLayers = 2;
constexpr double GrassLayerOffsetMinimumRadiusCm = 18.0;
constexpr double GrassLayerOffsetBandWidthCm = 24.0;
constexpr double GrassLayerOffsetBandStepCm = 0.0;
constexpr double GrassLayerVerticalSeparationCm = 0.0;
constexpr double GrassPlacementGapMinimumCm = 0.01;
constexpr double GrassPlacementGapMaximumCm = 0.05;
constexpr double GrassCoverageScaleMin = 0.72;
constexpr double GrassCoverageScaleMax = 0.92;
constexpr double GrassHeightScaleMin = 1.05;
constexpr double GrassHeightScaleMax = 1.25;
constexpr double GrassAnisotropyScaleMin = 0.98;
constexpr double GrassAnisotropyScaleMax = 1.02;
constexpr double GrassShortHeightClassScale = 0.88;
constexpr double GrassMediumHeightClassScale = 1.02;
constexpr double GrassTallHeightClassScale = 1.18;
constexpr double GrassShortHeightClassFraction = 0.60;
constexpr double GrassMediumHeightClassFraction = 0.30;
constexpr float GrassLodDistanceScale = 0.60f;
constexpr double GrassGoldenAngleDegrees = 137.50776405003785;

// Exact serialized R14 predecessor policy. These values stay isolated from the
// current builder so the editor migration can prove the old payload before any
// component or transform mutation.
constexpr int32 R14GrassMicroDetailCount = 24576;
constexpr int32 R14MaximumGrassReuseLayers = 2;
constexpr double R14GrassLayerOffsetMinimumRadiusCm = 8.0;
constexpr double R14GrassLayerOffsetBandWidthCm = 10.0;
constexpr double R14GrassLayerOffsetBandStepCm = 20.0;
constexpr double R14GrassLayerVerticalSeparationCm = 0.04;
constexpr double R14GrassCoverageScaleMin = 0.68;
constexpr double R14GrassCoverageScaleMax = 1.18;
constexpr double R14GrassHeightScaleMin = 1.15;
constexpr double R14GrassHeightScaleMax = 1.45;
constexpr int32 R14GrassCullStartDistanceCm = 5200;
constexpr int32 R14GrassCullEndDistanceCm = 8000;
constexpr int32 R14GrassWpoDisableDistanceCm = 2400;
constexpr int32 R14EdgeGrassCullStartDistanceCm = 6500;
constexpr int32 R14EdgeGrassCullEndDistanceCm = 8000;
constexpr int32 R14EdgeGrassWpoDisableDistanceCm = 2400;
constexpr float R14GrassLodDistanceScale = 1.0f;
constexpr int32 SupplementalSoilCount = 24;
constexpr int32 ShrubsPerPatch = 2;
constexpr int32 UnderstoreyPerPatch = 4;
constexpr int32 FlowersPerPatch = 1;
// R23 owns the runtime blade presentation exclusively. Its material visibility
// reaches zero by 28 m; the later HISM end cull prevents a component-level pop.
constexpr int32 GrassCullStartDistanceCm = 2600;
constexpr int32 GrassCullEndDistanceCm = 3400;
constexpr int32 GrassWpoDisableDistanceCm = 2400;
constexpr int32 EdgeGrassCullStartDistanceCm = 3000;
constexpr int32 EdgeGrassCullEndDistanceCm = 4500;
constexpr int32 EdgeGrassWpoDisableDistanceCm = 2400;
constexpr int32 SourceV5BTurfCullStartDistanceCm = 3000;
constexpr int32 SourceV5BTurfCullEndDistanceCm = 5200;
constexpr int32 SourceV5BTurfWpoDisableDistanceCm = 3200;
// Runtime-only medium-range composite. The exact V5B component is snapshotted
// and restored, but remains visible behind the denser V5D near layer. Extending
// only its cull/LOD presentation keeps modeled blades readable in ordinary
// walk-through views without touching meshes, transforms, collision or RF.
constexpr int32 CompositeSourceV5BTurfCullStartDistanceCm = 4000;
constexpr int32 CompositeSourceV5BTurfCullEndDistanceCm = 6500;
constexpr float CompositeSourceV5BTurfLodDistanceScale = 1.10f;
constexpr int32 CollisionResponseChannelCount =
    static_cast<int32>(ECC_GameTraceChannel18) + 1;
constexpr uint32 PlacementSeed = 0x56474444u;
constexpr double GroundOverlayOffsetCm = 0.20;
constexpr double RequiredGroundOverlayOpaqueCollarMeters = 50.0;
constexpr double RequiredGroundOverlayOutwardFeatherMeters = 8.0;
constexpr double RequiredGroundOverlayDitherCellMeters = 0.25;
constexpr double CentimetersPerMeter = 100.0;
const FName HybridContextPolicyTag(
    TEXT("TRIADIstanaExploreV5DContextPolicy"));
const FName HybridSiteClipTag(
    TEXT("TRIADIstanaExploreV5DProviderSiteClip"));
const FName RuntimeSourceTurfPresentationTagName(
    TEXT("TRIADIstanaExploreV5DSourceTurfPresentation"));

constexpr double AccentMinimumXCm = -9600.0;
constexpr double AccentMaximumXCm = 9600.0;
constexpr double AccentMinimumYCm = 800.0;
constexpr double AccentMaximumYCm = 22000.0;
constexpr double HeroLawnHalfWidthCm = 5200.0;
constexpr double HeroLawnMinimumYCm = 2500.0;
constexpr double HeroLawnMaximumYCm = 17500.0;
constexpr double CeremonialAxisHalfWidthCm = 1700.0;
constexpr double EdgeTransitionWidthCm = 420.0;
constexpr double SupplementalTreeMinimumAbsXCm = 6000.0;
constexpr double SupplementalTreeMaximumAbsXCm = 18500.0;
constexpr double SupplementalTreeMinimumYCm = 2600.0;
constexpr double SupplementalTreeMaximumYCm = 24000.0;
constexpr double ExistingPatchClearanceCm = 430.0;
constexpr double SupplementalPatchSpacingCm = 610.0;
// The maximum exclusion includes the supplemental patch radius and the widest
// supported deterministic carrier displacement (R23's 42 cm band).
constexpr double GrassPatchSuppressionRadiusCm = 350.0;
constexpr double ExistingTreeBasePatchSourceRadiusCm = 100.0;
constexpr double TreeBaseGrassSuppressionRadiusFraction = 1.0;
constexpr double TreeBaseGrassCarrierFootprintMarginCm = 220.0;

const FString GrassMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Manicured.M_IPV5D_Turf_Manicured"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Humid.M_IPV5D_Turf_Humid"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_Shade.M_IPV5D_Turf_Shade"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_Turf_DryEdge.M_IPV5D_Turf_DryEdge")};
constexpr float GrassR19RoughnessSignature[] = {
    0.70f, 0.66f, 0.72f, 0.76f};
constexpr float GrassR19SpecularSignature[] = {
    0.30f, 0.32f, 0.28f, 0.26f};
constexpr float GrassR21RoughnessSignature[] = {
    0.68f, 0.66f, 0.72f, 0.75f};
constexpr float GrassR21SpecularSignature[] = {
    0.30f, 0.31f, 0.28f, 0.26f};
constexpr float GrassR23BRoughnessSignature[] = {
    0.71f, 0.70f, 0.75f, 0.78f};
constexpr float GrassR23BSpecularSignature[] = {
    0.27f, 0.28f, 0.25f, 0.23f};
constexpr float GrassAnimatedWindStrengthSignature = 0.48f;
constexpr float GrassR22DefaultWindStrengthSignature = 0.0f;
const FName RuntimeMaterialRevisionParameter(
    TEXT("TRIAD_RuntimeMaterialRevision"));
const FName RuntimeMaterialCalibrationRevisionParameter(
    TEXT("TRIAD_RuntimeMaterialCalibrationRevision"));
constexpr float R22RuntimeMaterialRevisionSignature = 22.0f;
constexpr float R23RuntimeMaterialRevisionSignature = 23.0f;
constexpr float R23BRuntimeMaterialCalibrationRevisionSignature = 1.0f;
static_assert(
    UE_ARRAY_COUNT(GrassR19RoughnessSignature) ==
            UE_ARRAY_COUNT(GrassMaterialPaths) &&
        UE_ARRAY_COUNT(GrassR19SpecularSignature) ==
            UE_ARRAY_COUNT(GrassMaterialPaths) &&
        UE_ARRAY_COUNT(GrassR21RoughnessSignature) ==
            UE_ARRAY_COUNT(GrassMaterialPaths) &&
        UE_ARRAY_COUNT(GrassR21SpecularSignature) ==
            UE_ARRAY_COUNT(GrassMaterialPaths) &&
        UE_ARRAY_COUNT(GrassR23BRoughnessSignature) ==
            UE_ARRAY_COUNT(GrassMaterialPaths) &&
        UE_ARRAY_COUNT(GrassR23BSpecularSignature) ==
            UE_ARRAY_COUNT(GrassMaterialPaths),
    "The runtime R19/R21/R22/R23/R23B material signatures must remain one-to-one with the four grass profiles.");
const FString SoilMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_SoilMulch_Layered.M_IPV5D_SoilMulch_Layered"));
const FString GroundOverlayMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_LawnMacroVariation.M_IPV5D_LawnMacroVariation"));
const FString SourceEdgeGrassMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_GrassMedium_Wind.M_IPV4_GrassMedium_Wind"));
const FString EdgeGrassFadeMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_GrassMedium_EdgeFade.M_IPV5D_GrassMedium_EdgeFade"));
const FString GroundOverlayMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Terrain.SM_IstanaPublicView_Terrain"));
const FString SourceTerrainLawnMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_Grass001_Lawn.MI_IPV5_Grass001_Lawn"));

enum class EGrassMaterialRuntimeRevision : uint8
{
    Unsupported,
    R19,
    R21,
    R22,
    R23,
    R23B
};

EGrassMaterialRuntimeRevision ClassifyGrassMaterialRuntimeSignature(
    const TArray<float>& RoughnessValues,
    const TArray<float>& SpecularValues,
    const TArray<float>& WindStrengthValues,
    const TArray<float>& RevisionValues,
    const TArray<float>& CalibrationRevisionValues)
{
    if (RoughnessValues.Num() != UE_ARRAY_COUNT(GrassMaterialPaths) ||
        SpecularValues.Num() != UE_ARRAY_COUNT(GrassMaterialPaths) ||
        WindStrengthValues.Num() != UE_ARRAY_COUNT(GrassMaterialPaths) ||
        RevisionValues.Num() != UE_ARRAY_COUNT(GrassMaterialPaths) ||
        CalibrationRevisionValues.Num() !=
            UE_ARRAY_COUNT(GrassMaterialPaths))
    {
        return EGrassMaterialRuntimeRevision::Unsupported;
    }
    bool bAllR19 = true;
    bool bAllR21 = true;
    bool bAllR22 = true;
    bool bAllR23 = true;
    bool bAllR23B = true;
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassMaterialPaths); ++Index)
    {
        bAllR19 &= FMath::IsNearlyEqual(
                RoughnessValues[Index],
                GrassR19RoughnessSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                SpecularValues[Index],
                GrassR19SpecularSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                WindStrengthValues[Index],
                GrassAnimatedWindStrengthSignature,
                0.0001f) &&
            FMath::IsNearlyZero(
                RevisionValues[Index],
                0.0001f) &&
            FMath::IsNearlyZero(
                CalibrationRevisionValues[Index],
                0.0001f);
        bAllR21 &= FMath::IsNearlyEqual(
                RoughnessValues[Index],
                GrassR21RoughnessSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                SpecularValues[Index],
                GrassR21SpecularSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                WindStrengthValues[Index],
                GrassAnimatedWindStrengthSignature,
                0.0001f) &&
            FMath::IsNearlyZero(
                RevisionValues[Index],
                0.0001f) &&
            FMath::IsNearlyZero(
                CalibrationRevisionValues[Index],
                0.0001f);
        bAllR22 &= FMath::IsNearlyEqual(
                RoughnessValues[Index],
                GrassR21RoughnessSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                SpecularValues[Index],
                GrassR21SpecularSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                WindStrengthValues[Index],
                GrassR22DefaultWindStrengthSignature,
                0.0001f) &&
            FMath::IsNearlyEqual(
                RevisionValues[Index],
                R22RuntimeMaterialRevisionSignature,
                0.0001f) &&
            FMath::IsNearlyZero(
                CalibrationRevisionValues[Index],
                0.0001f);
        bAllR23 &= FMath::IsNearlyEqual(
                RoughnessValues[Index],
                GrassR21RoughnessSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                SpecularValues[Index],
                GrassR21SpecularSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                WindStrengthValues[Index],
                GrassR22DefaultWindStrengthSignature,
                0.0001f) &&
            FMath::IsNearlyEqual(
                RevisionValues[Index],
                R23RuntimeMaterialRevisionSignature,
                0.0001f) &&
            FMath::IsNearlyZero(
                CalibrationRevisionValues[Index],
                0.0001f);
        bAllR23B &= FMath::IsNearlyEqual(
                RoughnessValues[Index],
                GrassR23BRoughnessSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                SpecularValues[Index],
                GrassR23BSpecularSignature[Index],
                0.0001f) &&
            FMath::IsNearlyEqual(
                WindStrengthValues[Index],
                GrassR22DefaultWindStrengthSignature,
                0.0001f) &&
            FMath::IsNearlyEqual(
                RevisionValues[Index],
                R23RuntimeMaterialRevisionSignature,
                0.0001f) &&
            FMath::IsNearlyEqual(
                CalibrationRevisionValues[Index],
                R23BRuntimeMaterialCalibrationRevisionSignature,
                0.0001f);
    }
    const int32 MatchCount =
        (bAllR19 ? 1 : 0) + (bAllR21 ? 1 : 0) +
        (bAllR22 ? 1 : 0) + (bAllR23 ? 1 : 0) +
        (bAllR23B ? 1 : 0);
    if (MatchCount != 1)
    {
        return EGrassMaterialRuntimeRevision::Unsupported;
    }
    if (bAllR23B)
    {
        return EGrassMaterialRuntimeRevision::R23B;
    }
    if (bAllR23)
    {
        return EGrassMaterialRuntimeRevision::R23;
    }
    if (bAllR22)
    {
        return EGrassMaterialRuntimeRevision::R22;
    }
    return bAllR21
        ? EGrassMaterialRuntimeRevision::R21
        : EGrassMaterialRuntimeRevision::R19;
}

EGrassMaterialRuntimeRevision ClassifyGrassMaterialRuntimeSignature(
    const TArray<float>& RoughnessValues,
    const TArray<float>& SpecularValues,
    const TArray<float>& WindStrengthValues,
    const TArray<float>& RevisionValues)
{
    TArray<float> NoCalibrationRevision;
    NoCalibrationRevision.Init(0.0f, RevisionValues.Num());
    return ClassifyGrassMaterialRuntimeSignature(
        RoughnessValues,
        SpecularValues,
        WindStrengthValues,
        RevisionValues,
        NoCalibrationRevision);
}

bool ResolveGrassMaterialRuntimeRevision(
    const TArray<TObjectPtr<UMaterialInterface>>& Materials,
    EGrassMaterialRuntimeRevision& OutRevision,
    FString& OutError)
{
    OutRevision = EGrassMaterialRuntimeRevision::Unsupported;
    if (Materials.Num() != UE_ARRAY_COUNT(GrassMaterialPaths))
    {
        OutError = TEXT("The runtime grass-material revision signal requires exactly four ordered materials.");
        return false;
    }
    TArray<float> RoughnessValues;
    TArray<float> SpecularValues;
    TArray<float> WindStrengthValues;
    TArray<float> RevisionValues;
    TArray<float> CalibrationRevisionValues;
    RoughnessValues.Reserve(UE_ARRAY_COUNT(GrassMaterialPaths));
    SpecularValues.Reserve(UE_ARRAY_COUNT(GrassMaterialPaths));
    WindStrengthValues.Reserve(UE_ARRAY_COUNT(GrassMaterialPaths));
    RevisionValues.Reserve(UE_ARRAY_COUNT(GrassMaterialPaths));
    CalibrationRevisionValues.Reserve(UE_ARRAY_COUNT(GrassMaterialPaths));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassMaterialPaths); ++Index)
    {
        const UMaterialInterface* Material = Materials[Index];
        float Roughness = 0.0f;
        float Specular = 0.0f;
        float WindStrength = 0.0f;
        float Revision = 0.0f;
        float CalibrationRevision = 0.0f;
        if (!Material ||
            !Material->GetScalarParameterValue(
                FMaterialParameterInfo(FName(TEXT("Roughness"))),
                Roughness) ||
            !Material->GetScalarParameterValue(
                FMaterialParameterInfo(FName(TEXT("Specular"))),
                Specular) ||
            !Material->GetScalarParameterValue(
                FMaterialParameterInfo(FName(TEXT("TRIAD_WindStrengthCm"))),
                WindStrength))
        {
            OutError = FString::Printf(
                TEXT("Grass material %d does not expose the exact runtime Roughness/Specular/WindStrength revision signal."),
                Index);
            return false;
        }
        RoughnessValues.Add(Roughness);
        SpecularValues.Add(Specular);
        WindStrengthValues.Add(WindStrength);
        Material->GetScalarParameterValue(
            FMaterialParameterInfo(RuntimeMaterialRevisionParameter),
            Revision);
        RevisionValues.Add(Revision);
        Material->GetScalarParameterValue(
            FMaterialParameterInfo(
                RuntimeMaterialCalibrationRevisionParameter),
            CalibrationRevision);
        CalibrationRevisionValues.Add(CalibrationRevision);
    }
    OutRevision = ClassifyGrassMaterialRuntimeSignature(
        RoughnessValues,
        SpecularValues,
        WindStrengthValues,
        RevisionValues,
        CalibrationRevisionValues);
    if (OutRevision == EGrassMaterialRuntimeRevision::Unsupported)
    {
        OutError = TEXT("The four grass materials are neither the exact uniform R23B, R23, R22, R21, nor R19 runtime parameter signature.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ResolveOptionalRuntimeMaterialRevision(
    const UMaterialInterface* Material,
    int32& OutRevision,
    FString& OutError)
{
    OutRevision = 0;
    if (!Material)
    {
        OutError = TEXT("A required material is absent from the runtime revision roster.");
        return false;
    }
    float Revision = 0.0f;
    if (!Material->GetScalarParameterValue(
            FMaterialParameterInfo(RuntimeMaterialRevisionParameter),
            Revision))
    {
        OutError.Reset();
        return true;
    }
    if (FMath::IsNearlyEqual(
            Revision,
            R22RuntimeMaterialRevisionSignature,
            0.0001f))
    {
        OutRevision = 22;
    }
    else if (FMath::IsNearlyEqual(
                 Revision,
                 R23RuntimeMaterialRevisionSignature,
                 0.0001f))
    {
        OutRevision = 23;
    }
    else
    {
        OutError = FString::Printf(
            TEXT("Material %s exposes an unsupported runtime revision marker %.6f."),
            *Material->GetPathName(),
            Revision);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ResolveOptionalRuntimeMaterialCalibrationRevision(
    const UMaterialInterface* Material,
    int32& OutCalibrationRevision,
    FString& OutError)
{
    OutCalibrationRevision = 0;
    if (!Material)
    {
        OutError = TEXT("A required material is absent from the runtime calibration-revision roster.");
        return false;
    }
    float CalibrationRevision = 0.0f;
    if (!Material->GetScalarParameterValue(
            FMaterialParameterInfo(
                RuntimeMaterialCalibrationRevisionParameter),
            CalibrationRevision))
    {
        OutError.Reset();
        return true;
    }
    if (!FMath::IsNearlyEqual(
            CalibrationRevision,
            R23BRuntimeMaterialCalibrationRevisionSignature,
            0.0001f))
    {
        OutError = FString::Printf(
            TEXT("Material %s exposes an unsupported runtime calibration revision marker %.6f."),
            *Material->GetPathName(),
            CalibrationRevision);
        return false;
    }
    OutCalibrationRevision = 1;
    OutError.Reset();
    return true;
}

uint32 MixBits(uint32 Value)
{
    Value ^= Value >> 16;
    Value *= 0x7FEB352Du;
    Value ^= Value >> 15;
    Value *= 0x846CA68Bu;
    Value ^= Value >> 16;
    return Value;
}

uint32 HashPair(int32 X, int32 Y, uint32 Salt)
{
    return MixBits(
        static_cast<uint32>(X) * 0x9E3779B9u ^
        static_cast<uint32>(Y) * 0x85EBCA6Bu ^ Salt ^ PlacementSeed);
}

uint32 HashTransform(const FTransform& Transform, uint32 Salt)
{
    const FVector P = Transform.GetTranslation();
    return HashPair(
        FMath::RoundToInt(P.X * 0.25),
        FMath::RoundToInt(P.Y * 0.25),
        Salt);
}

double HashUnit(uint32 Value)
{
    return static_cast<double>(MixBits(Value) & 0x00FFFFFFu) /
        static_cast<double>(0x01000000u);
}

// Exact copy of the V5B analytic accent-terrain surface. R23 uses this pure
// formula at both source and displaced XY, avoiding a world trace and keeping
// the visual-only layout independent from collision/navigation/RF geometry.
double AnalyticAccentTerrainHeightCm(double XCm, double YCm)
{
    const double XMeters = XCm / 100.0;
    const double YMeters = YCm / 100.0;
    const double RadiusMeters = FMath::Sqrt(
        XMeters * XMeters + YMeters * YMeters);
    const double HeightMeters =
        0.72 * FMath::Sin(XMeters / 185.0) +
        0.48 * FMath::Cos(YMeters / 230.0) +
        0.22 * FMath::Sin((XMeters + YMeters) / 97.0) +
        0.0000011 * RadiusMeters * RadiusMeters - 0.48;
    return 100.0 * HeightMeters;
}

double SmoothUnit(double Value)
{
    const double T = FMath::Clamp(Value, 0.0, 1.0);
    return T * T * (3.0 - 2.0 * T);
}

double SpatialClumpNoise(double X, double Y, double CellSizeCm, uint32 Salt)
{
    const double GridX = X / CellSizeCm;
    const double GridY = Y / CellSizeCm;
    const int32 X0 = FMath::FloorToInt(GridX);
    const int32 Y0 = FMath::FloorToInt(GridY);
    const double Tx = SmoothUnit(GridX - static_cast<double>(X0));
    const double Ty = SmoothUnit(GridY - static_cast<double>(Y0));
    const double N00 = HashUnit(HashPair(X0, Y0, Salt));
    const double N10 = HashUnit(HashPair(X0 + 1, Y0, Salt));
    const double N01 = HashUnit(HashPair(X0, Y0 + 1, Salt));
    const double N11 = HashUnit(HashPair(X0 + 1, Y0 + 1, Salt));
    return FMath::Lerp(
        FMath::Lerp(N00, N10, Tx),
        FMath::Lerp(N01, N11, Tx),
        Ty);
}

bool IsFinitePositiveTransform(const FTransform& Transform)
{
    const FVector Location = Transform.GetTranslation();
    const FVector Scale = Transform.GetScale3D();
    const FQuat Rotation = Transform.GetRotation();
    return !Transform.ContainsNaN() && !Location.ContainsNaN() &&
        !Scale.ContainsNaN() && Rotation.IsNormalized() &&
        FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y) &&
        FMath::IsFinite(Location.Z) && FMath::IsFinite(Scale.X) &&
        FMath::IsFinite(Scale.Y) && FMath::IsFinite(Scale.Z) &&
        Scale.X > 0.0 && Scale.Y > 0.0 && Scale.Z > 0.0;
}

double FractionalPart(double Value)
{
    return Value - FMath::FloorToDouble(Value);
}

double GroundOverlayCoreCoverage(const FVector2D& WorldXYCentimeters)
{
    if (!FMath::IsFinite(WorldXYCentimeters.X) ||
        !FMath::IsFinite(WorldXYCentimeters.Y))
    {
        return 0.0;
    }
    const double SignedInwardDistanceCm =
        ATRIADIstanaExploreV5DContextPolicyActor::
            EvaluateProviderSiteClipSignedInwardDistanceCentimeters(
                WorldXYCentimeters);
    return FMath::Clamp(
        1.0 +
            (SignedInwardDistanceCm +
                RequiredGroundOverlayOpaqueCollarMeters *
                    CentimetersPerMeter) /
            (RequiredGroundOverlayOutwardFeatherMeters *
                CentimetersPerMeter),
        0.0,
        1.0);
}

double StableGroundOverlayDitherThreshold(
    const FVector2D& WorldXYCentimeters)
{
    const double CellCm =
        RequiredGroundOverlayDitherCellMeters * CentimetersPerMeter;
    const FVector2D StableCell(
        FMath::FloorToDouble(WorldXYCentimeters.X / CellCm),
        FMath::FloorToDouble(WorldXYCentimeters.Y / CellCm));
    return FractionalPart(
        FMath::Sin(StableCell.X * 12.9898 + StableCell.Y * 78.233) *
        43758.5453);
}

bool StableGroundOverlayCoreMask(const FVector2D& WorldXYCentimeters)
{
    const double Coverage = GroundOverlayCoreCoverage(WorldXYCentimeters);
    return Coverage > 0.0 &&
        Coverage >= StableGroundOverlayDitherThreshold(WorldXYCentimeters);
}

bool ShouldHideSourceTerrainRenderer(
    bool bPolicyValid,
    bool bPolicyHasBegunPlay,
    bool bLocalFallbackHidden)
{
    return bPolicyValid && bPolicyHasBegunPlay && bLocalFallbackHidden;
}

bool ApplySourceTerrainRendererVisibility(
    UStaticMeshComponent* TerrainComponent,
    bool bVisible)
{
    if (!TerrainComponent)
    {
        return false;
    }
    const ECollisionEnabled::Type CollisionBefore =
        TerrainComponent->GetCollisionEnabled();
    TerrainComponent->SetVisibility(bVisible, true);
    TerrainComponent->SetHiddenInGame(!bVisible, true);
    return TerrainComponent->IsVisible() == bVisible &&
        TerrainComponent->bHiddenInGame == !bVisible &&
        CollisionBefore == ECollisionEnabled::QueryAndPhysics &&
        TerrainComponent->GetCollisionEnabled() == CollisionBefore;
}

bool IsInsideHeroLawn(const FVector& Location)
{
    return FMath::Abs(Location.X) <= HeroLawnHalfWidthCm &&
        Location.Y >= HeroLawnMinimumYCm &&
        Location.Y <= HeroLawnMaximumYCm;
}

double DistanceSquared2D(const FVector& A, const FVector& B)
{
    const double DX = A.X - B.X;
    const double DY = A.Y - B.Y;
    return DX * DX + DY * DY;
}

double LawnEdgeWeight(const FVector& Location)
{
    const double HorizontalDistance = FMath::Min(
        Location.X - AccentMinimumXCm,
        AccentMaximumXCm - Location.X);
    const double VerticalDistance = FMath::Min(
        Location.Y - AccentMinimumYCm,
        AccentMaximumYCm - Location.Y);
    const double EdgeDistance = FMath::Max(
        0.0,
        FMath::Min(HorizontalDistance, VerticalDistance));
    return 1.0 - SmoothUnit(EdgeDistance / EdgeTransitionWidthCm);
}

bool IsNearAny(
    const FVector& Location,
    const TArray<FTransform>& Transforms,
    double RadiusCm)
{
    const double RadiusSquared = RadiusCm * RadiusCm;
    for (const FTransform& Transform : Transforms)
    {
        if (DistanceSquared2D(Location, Transform.GetTranslation()) <
            RadiusSquared)
        {
            return true;
        }
    }
    return false;
}

bool IsInsideAnyScaledPatchExclusion(
    const FVector& Location,
    const TArray<FTransform>& PatchTransforms,
    double SourceRadiusCm,
    double RadiusFraction,
    double CarrierFootprintMarginCm)
{
    for (const FTransform& PatchTransform : PatchTransforms)
    {
        const double SuppressionRadiusCm =
            FMath::Abs(PatchTransform.GetScale3D().X) *
                SourceRadiusCm * RadiusFraction +
            CarrierFootprintMarginCm;
        if (DistanceSquared2D(Location, PatchTransform.GetTranslation()) <=
            FMath::Square(SuppressionRadiusCm))
        {
            return true;
        }
    }
    return false;
}

double NearestDistanceCm(
    const FVector& Location,
    const TArray<FTransform>& Transforms,
    double MaximumCm)
{
    double BestSquared = MaximumCm * MaximumCm;
    for (const FTransform& Transform : Transforms)
    {
        BestSquared = FMath::Min(
            BestSquared,
            DistanceSquared2D(Location, Transform.GetTranslation()));
    }
    return FMath::Sqrt(BestSquared);
}

bool TransformArraysEqual(
    const TArray<FTransform>& A,
    const TArray<FTransform>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (!A[Index].Equals(B[Index], 0.0001))
        {
            return false;
        }
    }
    return true;
}

bool HasOnlyIgnoredCollisionResponses(const UPrimitiveComponent* Component)
{
    return Component &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore);
}

bool OverrideMaterialArraysEqual(
    const TArray<TObjectPtr<UMaterialInterface>>& A,
    const TArray<TObjectPtr<UMaterialInterface>>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (A[Index].Get() != B[Index].Get())
        {
            return false;
        }
    }
    return true;
}

bool HasExactPresentedOverrideArray(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<TObjectPtr<UMaterialInterface>>& OriginalOverrides,
    const UMaterialInterface* PresentationMaterial)
{
    if (!Component || !PresentationMaterial)
    {
        return false;
    }
    const int32 ExpectedCount = FMath::Max(1, OriginalOverrides.Num());
    if (Component->OverrideMaterials.Num() != ExpectedCount)
    {
        return false;
    }
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        const UMaterialInterface* Expected = Index == 0
            ? PresentationMaterial
            : OriginalOverrides[Index].Get();
        if (Component->OverrideMaterials[Index].Get() != Expected)
        {
            return false;
        }
    }
    return true;
}

bool ValidateTransformArray(
    const TArray<FTransform>& Transforms,
    int32 ExpectedCount,
    const TCHAR* Label,
    FString& OutError)
{
    if (Transforms.Num() != ExpectedCount)
    {
        OutError = FString::Printf(
            TEXT("V5D %s requires %d transforms; found %d."),
            Label,
            ExpectedCount,
            Transforms.Num());
        return false;
    }
    for (int32 Index = 0; Index < Transforms.Num(); ++Index)
    {
        if (!IsFinitePositiveTransform(Transforms[Index]))
        {
            OutError = FString::Printf(
                TEXT("V5D %s transform %d is non-finite, non-normalized or non-positive."),
                Label,
                Index);
            return false;
        }
    }
    return true;
}

void ConfigureRenderOnlyHism(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 StartCullDistance,
    int32 EndCullDistance,
    int32 WpoDisableDistance,
    bool bCastShadow)
{
    if (!Component)
    {
        return;
    }
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastShadow);
    Component->SetCastContactShadow(bCastShadow);
    Component->SetAffectDistanceFieldLighting(bCastShadow);
    Component->SetCullDistances(StartCullDistance, EndCullDistance);
    Component->WorldPositionOffsetDisableDistance = WpoDisableDistance;
    Component->bEnableDensityScaling = false;
    Component->bAutoRebuildTreeOnInstanceChanges = false;
}

void ConfigureRenderOnlyGroundOverlay(UStaticMeshComponent* Component)
{
    if (!Component)
    {
        return;
    }
    // ReapplyGroundVegetationRealism restores the exact saved world transform
    // after PIE duplication. This owned render-only overlay must therefore be
    // movable; the authoritative source terrain remains static and untouched.
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    Component->SetCastContactShadow(false);
    Component->SetAffectDistanceFieldLighting(false);
    Component->bReceivesDecals = true;
}

bool AppendWorldTransforms(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh())
    {
        OutError = TEXT("A required V4/V5B visual HISM or its mesh is absent.");
        return false;
    }
    for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
    {
        FTransform WorldTransform;
        if (!Component->GetInstanceTransform(Index, WorldTransform, true) ||
            !IsFinitePositiveTransform(WorldTransform))
        {
            OutError = FString::Printf(
                TEXT("Could not read finite world transform %d from component '%s'."),
                Index,
                *Component->GetName());
            return false;
        }
        OutTransforms.Add(WorldTransform);
    }
    return true;
}

struct FScoredIndex
{
    int32 Index = INDEX_NONE;
    double Score = 0.0;
    uint32 Hash = 0u;
};

void SortScored(TArray<FScoredIndex>& Values)
{
    Values.Sort([](const FScoredIndex& A, const FScoredIndex& B)
    {
        if (!FMath::IsNearlyEqual(A.Score, B.Score, 1.0e-12))
        {
            return A.Score < B.Score;
        }
        return A.Index < B.Index;
    });
}

FTransform BuildPlantTransform(
    const FVector& PatchLocation,
    uint32 Hash,
    int32 Ordinal,
    double MinimumRadiusCm,
    double MaximumRadiusCm,
    double MinimumScale,
    double MaximumScale,
    double HorizontalScale,
    double SeasonalScale)
{
    const uint32 RowHash = MixBits(Hash ^
        (static_cast<uint32>(Ordinal) + 1u) * 0x9E3779B9u);
    const double Angle = HashUnit(RowHash ^ 0xA511E9B3u) * 2.0 * PI;
    const double Radius = FMath::Lerp(
        MinimumRadiusCm,
        MaximumRadiusCm,
        HashUnit(RowHash ^ 0x63D83595u));
    const double Scale = FMath::Lerp(
        MinimumScale,
        MaximumScale,
        HashUnit(RowHash ^ 0xC2B2AE35u)) * SeasonalScale;
    return FTransform(
        FRotator(0.0, HashUnit(RowHash ^ 0x27D4EB2Fu) * 360.0, 0.0),
        PatchLocation + FVector(
            FMath::Cos(Angle) * Radius,
            FMath::Sin(Angle) * Radius,
            0.8),
        FVector(Scale * HorizontalScale, Scale * HorizontalScale, Scale));
}

} // namespace

ATRIADIstanaExploreV5DGroundVegetationActor::
    ATRIADIstanaExploreV5DGroundVegetationActor()
{
    const FString EdgeGrassFadePackagePath =
        FPackageName::ObjectPathToPackageName(EdgeGrassFadeMaterialPath);
    if (FPackageName::DoesPackageExist(EdgeGrassFadePackagePath))
    {
        static ConstructorHelpers::FObjectFinderOptional<UMaterialInterface>
            EdgeGrassFadeMaterialFinder(*EdgeGrassFadeMaterialPath);
        EdgeGrassFadeMaterialCookReference =
            EdgeGrassFadeMaterialFinder.Get();
    }

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;
    PrimaryActorTick.TickInterval = 0.5f;
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("V5DGroundVegetationRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    GroundMacroVariationOverlay =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("V5DGroundMacroVariationOverlay"));
    GroundMacroVariationOverlay->SetupAttachment(SceneRoot);
    GroundMacroVariationOverlay->SetRelativeTransform(FTransform::Identity);
    ConfigureRenderOnlyGroundOverlay(GroundMacroVariationOverlay);

    GrassManicuredInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DGrassManicuredMicroClumps"));
    GrassHumidInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DGrassHumidMicroClumps"));
    GrassShadeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DGrassShadeMicroClumps"));
    GrassDryEdgeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DGrassDryEdgeMicroClumps"));
    EdgeGrassInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DEdgeGrassSeasonalAccents"));
    SupplementalSoilInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DSupplementalSoilMulch"));
    SupplementalShrubInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DSupplementalShrubDiversity"));
    SupplementalUnderstoreyInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DSupplementalUnderstoreyDiversity"));
    SupplementalFlowerInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DSupplementalFlowerAccents"));

    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances,
        EdgeGrassInstances,
        SupplementalSoilInstances,
        SupplementalShrubInstances,
        SupplementalUnderstoreyInstances,
        SupplementalFlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
    }

    // Legacy maps did not serialize these native default-subobject policy
    // values. Keep the CDO at the exact R14 policy so a cold R14 map remains
    // admissible after this class revision; explicit fresh configuration and
    // staged transactional migrations switch owned components to R20/R23.
    ConfigureGrassPresentationComponentsForRevision(
        PredecessorGrassPresentationRevision);
    ConfigureRenderOnlyHism(SupplementalSoilInstances, 0, 65000, 0, true);
    ConfigureRenderOnlyHism(SupplementalShrubInstances, 9000, 52000, 28000, true);
    ConfigureRenderOnlyHism(SupplementalUnderstoreyInstances, 9000, 48000, 26000, true);
    ConfigureRenderOnlyHism(SupplementalFlowerInstances, 7000, 40000, 22000, true);
    SupplementalShrubInstances->ForcedLodModel = 2;
    SupplementalUnderstoreyInstances->ForcedLodModel = 2;
    SupplementalFlowerInstances->ForcedLodModel = 2;
}

void ATRIADIstanaExploreV5DGroundVegetationActor::
    ConfigureGrassPresentationComponentsForRevision(
        int32 PresentationRevision)
{
    const bool bR14 =
        PresentationRevision == PredecessorGrassPresentationRevision;
    const bool bR20 = PresentationRevision == GrassPresentationRevisionR20;
    const int32 GrassCullStart = bR14
        ? R14GrassCullStartDistanceCm
        : (bR20 ? R20GrassCullStartDistanceCm
                : GrassCullStartDistanceCm);
    const int32 GrassCullEnd = bR14
        ? R14GrassCullEndDistanceCm
        : (bR20 ? R20GrassCullEndDistanceCm
                : GrassCullEndDistanceCm);
    const int32 GrassWpoDisable = bR14
        ? R14GrassWpoDisableDistanceCm
        : (bR20 ? R20GrassWpoDisableDistanceCm
                : GrassWpoDisableDistanceCm);
    const int32 EdgeCullStart = bR14
        ? R14EdgeGrassCullStartDistanceCm
        : (bR20 ? R20EdgeGrassCullStartDistanceCm
                : EdgeGrassCullStartDistanceCm);
    const int32 EdgeCullEnd = bR14
        ? R14EdgeGrassCullEndDistanceCm
        : (bR20 ? R20EdgeGrassCullEndDistanceCm
                : EdgeGrassCullEndDistanceCm);
    const int32 EdgeWpoDisable = bR14
        ? R14EdgeGrassWpoDisableDistanceCm
        : (bR20 ? R20EdgeGrassWpoDisableDistanceCm
                : EdgeGrassWpoDisableDistanceCm);
    const float LodDistanceScale = bR14
        ? R14GrassLodDistanceScale
        : (bR20 ? R20GrassLodDistanceScale
                : GrassLodDistanceScale);

    UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
    {
        ConfigureRenderOnlyHism(
            Component,
            GrassCullStart,
            GrassCullEnd,
            GrassWpoDisable,
            false);
        if (Component)
        {
            Component->InstanceLODDistanceScale = LodDistanceScale;
        }
    }
    ConfigureRenderOnlyHism(
        EdgeGrassInstances,
        EdgeCullStart,
        EdgeCullEnd,
        EdgeWpoDisable,
        false);
    if (EdgeGrassInstances)
    {
        EdgeGrassInstances->InstanceLODDistanceScale = LodDistanceScale;
    }
}

const FString& ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedClaimLabel()
{
    return GroundVegetationClaimLabel;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::ExpectedSourceGrassCount()
{
    return SourceGrassCount;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedSourceEdgeGrassCount()
{
    return SourceEdgeGrassCount;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::ExpectedSourceTreeCount()
{
    return SourceTreeCount;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::ExpectedExistingSoilCount()
{
    return ExistingSoilCount;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedGrassMicroDetailCount()
{
    return GrassMicroDetailCount;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedGrassPresentationRevision()
{
    return GrassPresentationRevisionR23;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedPredecessorGrassPresentationRevision()
{
    return PredecessorGrassPresentationRevision;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedR23PredecessorGrassPresentationRevision()
{
    return GrassPresentationRevisionR20;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedSupplementalSoilCount()
{
    return SupplementalSoilCount;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::ExpectedEdgeGrassCount(
    ETRIADIstanaExploreV5DSeasonProfile InSeasonProfile)
{
    switch (InSeasonProfile)
    {
    case ETRIADIstanaExploreV5DSeasonProfile::HumidWet:
        return 512;
    case ETRIADIstanaExploreV5DSeasonProfile::Transition:
        return 640;
    case ETRIADIstanaExploreV5DSeasonProfile::DryStressPreview:
        return 768;
    default:
        return 0;
    }
}

FVector2D ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedProviderSiteClipCenterMeters()
{
    return ATRIADIstanaExploreV5DContextPolicyActor::
        ExpectedProviderSiteClipCenterCentimeters() /
        CentimetersPerMeter;
}

FVector2D ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedProviderSiteClipSemiAxesMeters()
{
    return ATRIADIstanaExploreV5DContextPolicyActor::
        ExpectedProviderSiteClipSemiAxesCentimeters() /
        CentimetersPerMeter;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedProviderSiteClipSplinePoints()
{
    return ATRIADIstanaExploreV5DContextPolicyActor::
        ExpectedProviderSiteClipSplinePoints();
}

double ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedGroundOverlayOpaqueCollarMeters()
{
    return RequiredGroundOverlayOpaqueCollarMeters;
}

double ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedGroundOverlayOutwardFeatherMeters()
{
    return RequiredGroundOverlayOutwardFeatherMeters;
}

double ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedGroundOverlayDitherCellMeters()
{
    return RequiredGroundOverlayDitherCellMeters;
}

const FString& ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedEdgeGrassFadeMaterialPath()
{
    return EdgeGrassFadeMaterialPath;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateNativeEdgeGrassFadeCookDependency(FString& OutReport)
{
    const ATRIADIstanaExploreV5DGroundVegetationActor* Defaults =
        GetDefault<ATRIADIstanaExploreV5DGroundVegetationActor>();
    UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(
        nullptr,
        *EdgeGrassFadeMaterialPath);
    if (!Defaults || !Defaults->EdgeGrassFadeMaterialCookReference ||
        Defaults->EdgeGrassFadeMaterialCookReference->GetPathName() !=
            EdgeGrassFadeMaterialPath ||
        !Loaded || Loaded != Defaults->EdgeGrassFadeMaterialCookReference)
    {
        OutReport = TEXT("The native V5D ground-vegetation CDO does not hold the exact R11 edge-fade material; restart after migration before cook/package validation.");
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_EDGE_GRASS_COOK_DEPENDENCY_VALID serializedNativeCdoHardReference=true exactPath=M_IPV5D_GrassMedium_EdgeFade exactLoad=true freshCookManifestAndPackagedLoadStillRequired=true");
    return true;
}

const FName& ATRIADIstanaExploreV5DGroundVegetationActor::
    RuntimeSourceTurfPresentationTag()
{
    return RuntimeSourceTurfPresentationTagName;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedRuntimeSourceTurfCullStartDistanceCm()
{
    return CompositeSourceV5BTurfCullStartDistanceCm;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedRuntimeSourceTurfCullEndDistanceCm()
{
    return CompositeSourceV5BTurfCullEndDistanceCm;
}

float ATRIADIstanaExploreV5DGroundVegetationActor::
    ExpectedRuntimeSourceTurfLodDistanceScale()
{
    return CompositeSourceV5BTurfLodDistanceScale;
}

double ATRIADIstanaExploreV5DGroundVegetationActor::
    EvaluateGroundOverlayCoreCoverage(
        const FVector2D& WorldXYCentimeters)
{
    return GroundOverlayCoreCoverage(WorldXYCentimeters);
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    EvaluateGroundOverlayStableDitherMask(
        const FVector2D& WorldXYCentimeters)
{
    return StableGroundOverlayCoreMask(WorldXYCentimeters);
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DGroundVegetationAssets& Assets,
    FString& OutError)
{
    const UObject* RequiredObjects[] = {
        Assets.GroundOverlayMesh,
        Assets.GroundOverlayMaterial,
        Assets.GrassMicroDetailMesh,
        Assets.EdgeGrassMesh,
        Assets.EdgeGrassMaterial,
        Assets.SoilPatchMesh,
        Assets.SoilPatchMaterial,
        Assets.ShrubMesh,
        Assets.ShrubMaterial,
        Assets.UnderstoreyMesh,
        Assets.UnderstoreyMaterial,
        Assets.FlowerMesh,
        Assets.FlowerMaterial};
    for (const UObject* Object : RequiredObjects)
    {
        if (!Object || Object->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
        {
            OutError = TEXT("The V5D ground/vegetation roster contains a missing or destroyed visual asset.");
            return false;
        }
    }
    if (Assets.GrassProfileMaterials.Num() != UE_ARRAY_COUNT(GrassMaterialPaths))
    {
        OutError = TEXT("The V5D ground/vegetation roster requires exactly four ordered grass profile materials.");
        return false;
    }
    TSet<const UMaterialInterface*> UniqueGrassMaterials;
    for (int32 Index = 0; Index < Assets.GrassProfileMaterials.Num(); ++Index)
    {
        const UMaterialInterface* Material = Assets.GrassProfileMaterials[Index];
        if (!Material || Material->GetPathName() != GrassMaterialPaths[Index] ||
            UniqueGrassMaterials.Contains(Material))
        {
            OutError = FString::Printf(
                TEXT("V5D grass profile material %d is missing, duplicated or not at its exact isolated path."),
                Index);
            return false;
        }
        UniqueGrassMaterials.Add(Material);
    }
    if (Assets.GroundOverlayMesh->GetPathName() != GroundOverlayMeshPath ||
        Assets.GroundOverlayMaterial->GetPathName() !=
            GroundOverlayMaterialPath ||
        Assets.SoilPatchMaterial->GetPathName() != SoilMaterialPath ||
        (Assets.EdgeGrassMaterial->GetPathName() !=
                SourceEdgeGrassMaterialPath &&
            Assets.EdgeGrassMaterial->GetPathName() !=
                EdgeGrassFadeMaterialPath) ||
        Assets.GrassMicroDetailMesh == Assets.EdgeGrassMesh ||
        Assets.GroundOverlayMesh == Assets.GrassMicroDetailMesh ||
        Assets.GroundOverlayMesh == Assets.EdgeGrassMesh ||
        Assets.GroundOverlayMesh == Assets.SoilPatchMesh ||
        Assets.SoilPatchMesh == Assets.GrassMicroDetailMesh ||
        Assets.SoilPatchMesh == Assets.EdgeGrassMesh)
    {
        OutError = TEXT("V5D soil/grass roles are not distinct, the layered soil material path drifted, or the edge grass does not use the exact frozen V4 source/owned V5D fade derivative.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::BuildDeterministicLayout(
    const TArray<FTransform>& V5BGrassWorldTransforms,
    const TArray<FTransform>& V4EdgeGrassWorldTransforms,
    const TArray<FTransform>& TreeWorldTransforms,
    const TArray<FTransform>& ExistingV5BSoilWorldTransforms,
    ETRIADIstanaExploreV5DSeasonProfile InSeasonProfile,
    FTRIADIstanaExploreV5DGroundVegetationLayout& OutLayout,
    FString& OutError)
{
    return BuildDeterministicLayoutForPresentationRevision(
        V5BGrassWorldTransforms,
        V4EdgeGrassWorldTransforms,
        TreeWorldTransforms,
        ExistingV5BSoilWorldTransforms,
        InSeasonProfile,
        GrassPresentationRevisionR23,
        OutLayout,
        OutError);
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    BuildDeterministicLayoutForPresentationRevision(
        const TArray<FTransform>& V5BGrassWorldTransforms,
        const TArray<FTransform>& V4EdgeGrassWorldTransforms,
        const TArray<FTransform>& TreeWorldTransforms,
        const TArray<FTransform>& ExistingV5BSoilWorldTransforms,
        ETRIADIstanaExploreV5DSeasonProfile InSeasonProfile,
        int32 PresentationRevision,
        FTRIADIstanaExploreV5DGroundVegetationLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
    const bool bPredecessor =
        PresentationRevision == PredecessorGrassPresentationRevision;
    const bool bR20 = PresentationRevision == GrassPresentationRevisionR20;
    const bool bR23 = PresentationRevision == GrassPresentationRevisionR23;
    if (!bPredecessor && !bR20 && !bR23)
    {
        OutError = FString::Printf(
            TEXT("Unsupported V5D grass presentation revision %d."),
            PresentationRevision);
        return false;
    }
    const int32 LayoutGrassCount = bPredecessor
        ? R14GrassMicroDetailCount
        : (bR20 ? R20GrassMicroDetailCount : GrassMicroDetailCount);
    const int32 LayoutMaximumReuseLayers = bPredecessor
        ? R14MaximumGrassReuseLayers
        : (bR20 ? R20MaximumGrassReuseLayers
                : MaximumGrassReuseLayers);
    const double LayoutOffsetMinimumRadiusCm = bPredecessor
        ? R14GrassLayerOffsetMinimumRadiusCm
        : (bR20 ? R20GrassLayerOffsetMinimumRadiusCm
                : GrassLayerOffsetMinimumRadiusCm);
    const double LayoutOffsetBandWidthCm = bPredecessor
        ? R14GrassLayerOffsetBandWidthCm
        : (bR20 ? R20GrassLayerOffsetBandWidthCm
                : GrassLayerOffsetBandWidthCm);
    const double LayoutOffsetBandStepCm = bPredecessor
        ? R14GrassLayerOffsetBandStepCm
        : (bR20 ? R20GrassLayerOffsetBandStepCm
                : GrassLayerOffsetBandStepCm);
    const double LayoutVerticalSeparationCm = bPredecessor
        ? R14GrassLayerVerticalSeparationCm
        : (bR20 ? R20GrassLayerVerticalSeparationCm
                : GrassLayerVerticalSeparationCm);
    const double LayoutCoverageScaleMin = bPredecessor
        ? R14GrassCoverageScaleMin
        : (bR20 ? R20GrassCoverageScaleMin
                : GrassCoverageScaleMin);
    const double LayoutCoverageScaleMax = bPredecessor
        ? R14GrassCoverageScaleMax
        : (bR20 ? R20GrassCoverageScaleMax
                : GrassCoverageScaleMax);
    const double LayoutHeightScaleMin = bPredecessor
        ? R14GrassHeightScaleMin
        : (bR20 ? R20GrassHeightScaleMin
                : GrassHeightScaleMin);
    const double LayoutHeightScaleMax = bPredecessor
        ? R14GrassHeightScaleMax
        : (bR20 ? R20GrassHeightScaleMax
                : GrassHeightScaleMax);
    if (!ValidateTransformArray(
            V5BGrassWorldTransforms,
            SourceGrassCount,
            TEXT("source grass"),
            OutError) ||
        !ValidateTransformArray(
            V4EdgeGrassWorldTransforms,
            SourceEdgeGrassCount,
            TEXT("source edge grass"),
            OutError) ||
        !ValidateTransformArray(
            TreeWorldTransforms,
            SourceTreeCount,
            TEXT("source tree"),
            OutError) ||
        !ValidateTransformArray(
            ExistingV5BSoilWorldTransforms,
            ExistingSoilCount,
            TEXT("existing V5B soil"),
            OutError) ||
        ExpectedEdgeGrassCount(InSeasonProfile) <= 0)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D season profile is invalid.");
        }
        return false;
    }

    TArray<FScoredIndex> TreeCandidates;
    for (int32 Index = 0; Index < TreeWorldTransforms.Num(); ++Index)
    {
        const FVector Location = TreeWorldTransforms[Index].GetTranslation();
        const double AbsX = FMath::Abs(Location.X);
        if (IsInsideHeroLawn(Location) ||
            AbsX < SupplementalTreeMinimumAbsXCm ||
            AbsX > SupplementalTreeMaximumAbsXCm ||
            Location.Y < SupplementalTreeMinimumYCm ||
            Location.Y > SupplementalTreeMaximumYCm ||
            FMath::Abs(Location.X) < CeremonialAxisHalfWidthCm ||
            IsNearAny(
                Location,
                ExistingV5BSoilWorldTransforms,
                ExistingPatchClearanceCm))
        {
            continue;
        }
        const uint32 Hash = HashTransform(TreeWorldTransforms[Index], 0x51A74A19u);
        const double Macro = SpatialClumpNoise(
            Location.X,
            Location.Y,
            1700.0,
            0x99C4E12Du);
        TreeCandidates.Add({Index, HashUnit(Hash) + 0.18 * (1.0 - Macro), Hash});
    }
    SortScored(TreeCandidates);
    for (const FScoredIndex& Candidate : TreeCandidates)
    {
        if (OutLayout.SupplementalSoil.Num() >= SupplementalSoilCount)
        {
            break;
        }
        const FVector TreeLocation =
            TreeWorldTransforms[Candidate.Index].GetTranslation();
        if (IsNearAny(
                TreeLocation,
                OutLayout.SupplementalSoil,
                SupplementalPatchSpacingCm))
        {
            continue;
        }
        const double PatchScale = FMath::Lerp(
            0.94,
            1.34,
            HashUnit(Candidate.Hash ^ 0xE4A9D725u));
        const FTransform Patch(
            FRotator(0.0, HashUnit(Candidate.Hash ^ 0xB5297A4Du) * 360.0, 0.0),
            TreeLocation + FVector(0.0, 0.0, 0.6),
            FVector(PatchScale, PatchScale, PatchScale));
        OutLayout.SupplementalSoil.Add(Patch);

        const double SeasonalPlantScale =
            InSeasonProfile == ETRIADIstanaExploreV5DSeasonProfile::HumidWet
            ? 1.04
            : (InSeasonProfile ==
                       ETRIADIstanaExploreV5DSeasonProfile::Transition
                   ? 1.0
                   : 0.94);
        for (int32 Plant = 0; Plant < ShrubsPerPatch; ++Plant)
        {
            OutLayout.SupplementalShrubs.Add(BuildPlantTransform(
                TreeLocation,
                Candidate.Hash ^ 0xA8F4713Bu,
                Plant,
                72.0,
                118.0,
                1.08,
                1.52,
                0.74,
                SeasonalPlantScale));
        }
        for (int32 Plant = 0; Plant < UnderstoreyPerPatch; ++Plant)
        {
            OutLayout.SupplementalUnderstorey.Add(BuildPlantTransform(
                TreeLocation,
                Candidate.Hash ^ 0x6D2B79F5u,
                Plant,
                82.0,
                138.0,
                0.90,
                1.28,
                0.68,
                SeasonalPlantScale));
        }
        for (int32 Plant = 0; Plant < FlowersPerPatch; ++Plant)
        {
            OutLayout.SupplementalFlowers.Add(BuildPlantTransform(
                TreeLocation,
                Candidate.Hash ^ 0xD12E4B87u,
                Plant,
                95.0,
                132.0,
                0.78,
                1.05,
                0.82,
                SeasonalPlantScale));
        }
    }
    if (OutLayout.SupplementalSoil.Num() != SupplementalSoilCount)
    {
        const int32 FoundCount = OutLayout.SupplementalSoil.Num();
        OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
        OutError = FString::Printf(
            TEXT("Only %d of %d non-overlapping supplemental tree bases satisfied the V5D hero-lawn and existing-patch boundaries."),
            FoundCount,
            SupplementalSoilCount);
        return false;
    }

    TArray<FScoredIndex> GrassCandidates;
    GrassCandidates.Reserve(V5BGrassWorldTransforms.Num());
    for (int32 Index = 0; Index < V5BGrassWorldTransforms.Num(); ++Index)
    {
        const FVector Location = V5BGrassWorldTransforms[Index].GetTranslation();
        if (IsNearAny(
                Location,
                OutLayout.SupplementalSoil,
                GrassPatchSuppressionRadiusCm) ||
            IsInsideAnyScaledPatchExclusion(
                Location,
                ExistingV5BSoilWorldTransforms,
                ExistingTreeBasePatchSourceRadiusCm,
                TreeBaseGrassSuppressionRadiusFraction,
                TreeBaseGrassCarrierFootprintMarginCm))
        {
            continue;
        }
        const uint32 Hash = HashTransform(
            V5BGrassWorldTransforms[Index],
            0xC73A4D91u);
        GrassCandidates.Add({Index, 0.0, Hash});
    }
    if (GrassCandidates.IsEmpty() ||
        GrassCandidates.Num() * LayoutMaximumReuseLayers <
            LayoutGrassCount)
    {
        OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
        OutError = FString::Printf(
            TEXT("V5D grass suppression left %d candidates; at least %d are required for the bounded %d-layer deterministic micro-detail reuse."),
            GrassCandidates.Num(),
            FMath::DivideAndRoundUp(
                LayoutGrassCount,
                LayoutMaximumReuseLayers),
            LayoutMaximumReuseLayers);
        return false;
    }

    const int32 GrassLayerCount = FMath::DivideAndRoundUp(
        LayoutGrassCount,
        GrassCandidates.Num());
    TMap<int32, FVector> FirstGrassPlacementBySource;
    FirstGrassPlacementBySource.Reserve(GrassCandidates.Num());
    int32 RemainingGrass = LayoutGrassCount;
    for (int32 LayerIndex = 0;
         LayerIndex < GrassLayerCount && RemainingGrass > 0;
         ++LayerIndex)
    {
        TArray<FScoredIndex> LayerCandidates;
        LayerCandidates.Reserve(GrassCandidates.Num());
        for (const FScoredIndex& BaseCandidate : GrassCandidates)
        {
            const FVector Location =
                V5BGrassWorldTransforms[BaseCandidate.Index].GetTranslation();
            const uint32 LayerHash = MixBits(
                BaseCandidate.Hash ^
                (static_cast<uint32>(LayerIndex) + 1u) * 0xD1B54A35u);
            const double Macro = SpatialClumpNoise(
                Location.X,
                Location.Y,
                1150.0,
                0x7A13E5C9u ^
                    (static_cast<uint32>(LayerIndex) + 1u) * 0xA24BAED5u);
            const double DetailNoise = SpatialClumpNoise(
                Location.X,
                Location.Y,
                245.0,
                0x4F2D98B7u ^
                    (static_cast<uint32>(LayerIndex) + 1u) * 0x9FB21C65u);
            const double Clump = 0.66 * Macro + 0.34 * DetailNoise;
            LayerCandidates.Add({
                BaseCandidate.Index,
                HashUnit(LayerHash) * FMath::Lerp(1.42, 0.43, Clump),
                LayerHash});
        }
        SortScored(LayerCandidates);

        const int32 LayerDetailCount = FMath::Min(
            RemainingGrass,
            LayerCandidates.Num());
        for (int32 LayerOrdinal = 0;
             LayerOrdinal < LayerDetailCount;
             ++LayerOrdinal)
        {
            const FScoredIndex& Candidate = LayerCandidates[LayerOrdinal];
            FTransform Detail = V5BGrassWorldTransforms[Candidate.Index];
            FVector Location = Detail.GetTranslation();
            const bool bHero = IsInsideHeroLawn(Location);
            const double EdgeWeight = LawnEdgeWeight(Location);
            const double NearestPatch = NearestDistanceCm(
                Location,
                OutLayout.SupplementalSoil,
                900.0);
            const double ShadeWeight = 1.0 -
                SmoothUnit((NearestPatch - 145.0) / 600.0);
            const double MacroClump = SpatialClumpNoise(
                Location.X,
                Location.Y,
                980.0,
                0x31B78AD5u);
            const double Draw =
                HashUnit(Candidate.Hash ^ 0x8B7D3F21u);

            double DryWeight = 0.02;
            double HumidWeight = 0.22;
            double HeroDryWeight = 0.004;
            if (InSeasonProfile ==
                ETRIADIstanaExploreV5DSeasonProfile::Transition)
            {
                DryWeight = 0.08;
                HumidWeight = 0.18;
                HeroDryWeight = 0.008;
            }
            else if (InSeasonProfile ==
                     ETRIADIstanaExploreV5DSeasonProfile::DryStressPreview)
            {
                DryWeight = 0.18;
                HumidWeight = 0.10;
                HeroDryWeight = 0.014;
            }
            DryWeight = bHero
                ? FMath::Clamp(
                      HeroDryWeight + 0.002 * (1.0 - MacroClump),
                      0.0,
                      0.018)
                : FMath::Clamp(
                      DryWeight + 0.34 * EdgeWeight +
                          0.07 * (1.0 - MacroClump),
                      0.0,
                      0.62);
            const double EffectiveShade = bHero
                ? FMath::Clamp(
                      0.010 + 0.006 * (1.0 - MacroClump),
                      0.0,
                      0.018)
                : FMath::Clamp(
                      0.08 + 0.42 * ShadeWeight,
                      0.0,
                      0.48);

            enum class EGrassRole : uint8
            {
                Manicured,
                Humid,
                Shade,
                Dry
            };
            EGrassRole Role = EGrassRole::Manicured;
            if (Draw < DryWeight)
            {
                Role = EGrassRole::Dry;
            }
            else if (Draw < DryWeight + EffectiveShade)
            {
                Role = EGrassRole::Shade;
            }
            else if (Draw < DryWeight + EffectiveShade + HumidWeight)
            {
                Role = EGrassRole::Humid;
            }

            const FVector SourceLocation = Location;
            const uint32 SourcePlacementHash = HashTransform(
                V5BGrassWorldTransforms[Candidate.Index],
                0xC73A4D91u);
            const double OffsetAngle = bR23
                ? FMath::DegreesToRadians(FRotator::NormalizeAxis(
                      HashUnit(SourcePlacementHash ^ 0x48A65D31u) * 360.0 +
                      static_cast<double>(LayerIndex) *
                          GrassGoldenAngleDegrees))
                : HashUnit(Candidate.Hash ^ 0x48A65D31u) * 2.0 * PI;
            const double OffsetRadius =
                LayoutOffsetMinimumRadiusCm +
                static_cast<double>(LayerIndex) *
                    LayoutOffsetBandStepCm +
                LayoutOffsetBandWidthCm *
                    HashUnit(Candidate.Hash ^ 0xDC3B75E9u);
            Location += FVector(
                FMath::Cos(OffsetAngle) * OffsetRadius,
                FMath::Sin(OffsetAngle) * OffsetRadius,
                0.0);
            if (bR23)
            {
                const double SourceSurfaceDeltaCm = SourceLocation.Z -
                    AnalyticAccentTerrainHeightCm(
                        SourceLocation.X,
                        SourceLocation.Y);
                const double PlacementGapCm = FMath::Lerp(
                    GrassPlacementGapMinimumCm,
                    GrassPlacementGapMaximumCm,
                    HashUnit(Candidate.Hash ^ 0x2A6F1D93u));
                Location.Z = AnalyticAccentTerrainHeightCm(
                    Location.X,
                    Location.Y) + SourceSurfaceDeltaCm + PlacementGapCm;
            }
            else
            {
                Location.Z += 0.10 + static_cast<double>(LayerIndex) *
                    LayoutVerticalSeparationCm;
            }
            if (const FVector* FirstPlacement =
                    FirstGrassPlacementBySource.Find(Candidate.Index))
            {
                if (FirstPlacement->Equals(Location, 0.0001))
                {
                    OutLayout =
                        FTRIADIstanaExploreV5DGroundVegetationLayout();
                    OutError = TEXT("A deterministic grass reuse layer produced a coincident micro-detail transform.");
                    return false;
                }
            }
            else
            {
                FirstGrassPlacementBySource.Add(Candidate.Index, Location);
            }
            Detail.SetTranslation(Location);
            FRotator Rotation = Detail.Rotator();
            Rotation.Yaw = bR23
                ? FRotator::NormalizeAxis(
                      HashUnit(SourcePlacementHash ^ 0xA3C59AC3u) * 360.0 +
                      static_cast<double>(LayerIndex) *
                          GrassGoldenAngleDegrees)
                : Rotation.Yaw + FMath::Lerp(
                      -37.0,
                      37.0,
                      HashUnit(Candidate.Hash ^ 0xA3C59AC3u));
            Detail.SetRotation(Rotation.Quaternion());

            double ScaleMultiplier = 1.0;
            switch (Role)
            {
            case EGrassRole::Manicured:
                ScaleMultiplier = FMath::Lerp(
                    0.91,
                    bHero ? 1.00 : 1.05,
                    HashUnit(Candidate.Hash ^ 0x117A65D9u));
                break;
            case EGrassRole::Humid:
                ScaleMultiplier = FMath::Lerp(
                    1.01,
                    bHero ? 1.08 : 1.14,
                    HashUnit(Candidate.Hash ^ 0x117A65D9u));
                break;
            case EGrassRole::Shade:
                ScaleMultiplier = FMath::Lerp(
                    1.04,
                    1.17,
                    HashUnit(Candidate.Hash ^ 0x117A65D9u));
                break;
            case EGrassRole::Dry:
                ScaleMultiplier = FMath::Lerp(
                    0.82,
                    0.98,
                    HashUnit(Candidate.Hash ^ 0x117A65D9u));
                break;
            }
            const double CoverageSignal = FMath::Clamp(
                0.72 * MacroClump +
                    0.28 * HashUnit(Candidate.Hash ^ 0x7F4A7C15u),
                0.0,
                1.0);
            const double CoverageScale = FMath::Lerp(
                LayoutCoverageScaleMin,
                LayoutCoverageScaleMax,
                SmoothUnit(CoverageSignal));
            const double HeightSignal = FMath::Clamp(
                0.55 * MacroClump +
                    0.45 * HashUnit(Candidate.Hash ^ 0x6B1E29D3u),
                0.0,
                1.0);
            const double HeightScale = FMath::Lerp(
                LayoutHeightScaleMin,
                LayoutHeightScaleMax,
                SmoothUnit(HeightSignal));
            const FVector SourceScale = Detail.GetScale3D();
            FVector DetailScale = SourceScale;
            if (bPredecessor)
            {
                DetailScale *= ScaleMultiplier;
                DetailScale.X *= CoverageScale * FMath::Lerp(
                    0.96,
                    1.04,
                    HashUnit(Candidate.Hash ^ 0x91E10DA5u));
                DetailScale.Y *= CoverageScale * FMath::Lerp(
                    0.96,
                    1.04,
                    HashUnit(Candidate.Hash ^ 0xC2B2AE3Du));
                DetailScale.Z *= HeightScale;
            }
            else
            {
                // The source carrier uses independent XY randomization. R20+
                // first preserves its area with a geometric mean, then applies
                // only a two-percent final anisotropy so elongated carpet-like
                // footprints cannot leak through the inherited transform.
                const double SourceHorizontalScale = FMath::Sqrt(
                    SourceScale.X * SourceScale.Y);
                const double AnisotropyX = FMath::Lerp(
                    GrassAnisotropyScaleMin,
                    GrassAnisotropyScaleMax,
                    HashUnit(Candidate.Hash ^ 0x91E10DA5u));
                const double AnisotropyY = FMath::Lerp(
                    GrassAnisotropyScaleMin,
                    GrassAnisotropyScaleMax,
                    HashUnit(Candidate.Hash ^ 0xC2B2AE3Du));
                double RoleHeightScale = 1.0;
                switch (Role)
                {
                case EGrassRole::Manicured:
                    RoleHeightScale = FMath::Lerp(
                        0.94,
                        1.00,
                        HashUnit(Candidate.Hash ^ 0x117A65D9u));
                    break;
                case EGrassRole::Humid:
                    RoleHeightScale = FMath::Lerp(
                        1.00,
                        1.06,
                        HashUnit(Candidate.Hash ^ 0x117A65D9u));
                    break;
                case EGrassRole::Shade:
                    RoleHeightScale = FMath::Lerp(
                        1.02,
                        1.10,
                        HashUnit(Candidate.Hash ^ 0x117A65D9u));
                    break;
                case EGrassRole::Dry:
                    RoleHeightScale = FMath::Lerp(
                        0.88,
                        0.96,
                        HashUnit(Candidate.Hash ^ 0x117A65D9u));
                    break;
                }
                const double HeightClassDraw =
                    HashUnit(Candidate.Hash ^ 0x5F356495u);
                const double ShortHeightClassScale = bR20
                    ? R20GrassShortHeightClassScale
                    : GrassShortHeightClassScale;
                const double MediumHeightClassScale = bR20
                    ? R20GrassMediumHeightClassScale
                    : GrassMediumHeightClassScale;
                const double TallHeightClassScale = bR20
                    ? R20GrassTallHeightClassScale
                    : GrassTallHeightClassScale;
                const double HeightClassScale =
                    HeightClassDraw < GrassShortHeightClassFraction
                    ? ShortHeightClassScale
                    : (HeightClassDraw <
                              GrassShortHeightClassFraction +
                                  GrassMediumHeightClassFraction
                           ? MediumHeightClassScale
                           : TallHeightClassScale);
                DetailScale.X = SourceHorizontalScale * CoverageScale *
                    AnisotropyX;
                DetailScale.Y = SourceHorizontalScale * CoverageScale *
                    AnisotropyY;
                DetailScale.Z = SourceScale.Z * HeightScale *
                    RoleHeightScale * HeightClassScale;
            }
            Detail.SetScale3D(DetailScale);

            switch (Role)
            {
            case EGrassRole::Manicured:
                OutLayout.GrassManicured.Add(Detail);
                break;
            case EGrassRole::Humid:
                OutLayout.GrassHumid.Add(Detail);
                break;
            case EGrassRole::Shade:
                OutLayout.GrassShade.Add(Detail);
                break;
            case EGrassRole::Dry:
                OutLayout.GrassDryEdge.Add(Detail);
                break;
            }
        }
        RemainingGrass -= LayerDetailCount;
    }

    const auto CountHeroGrass = [](const TArray<FTransform>& Transforms)
    {
        int32 Count = 0;
        for (const FTransform& Transform : Transforms)
        {
            if (IsInsideHeroLawn(Transform.GetTranslation()))
            {
                ++Count;
            }
        }
        return Count;
    };
    const int32 HeroManicured = CountHeroGrass(OutLayout.GrassManicured);
    const int32 HeroHumid = CountHeroGrass(OutLayout.GrassHumid);
    const int32 HeroShade = CountHeroGrass(OutLayout.GrassShade);
    const int32 HeroDry = CountHeroGrass(OutLayout.GrassDryEdge);
    const int32 HeroTotal =
        HeroManicured + HeroHumid + HeroShade + HeroDry;
    if (RemainingGrass != 0 || HeroShade <= 0 || HeroDry <= 0 ||
        HeroTotal <= 0 ||
        static_cast<int64>(HeroManicured + HeroHumid) * 100 <
            static_cast<int64>(HeroTotal) * 95)
    {
        OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
        OutError = TEXT("The formal hero lawn must remain at least 95 percent manicured/humid while retaining deterministic restrained shade and dry micro-detail intrusion.");
        return false;
    }

    const int32 DesiredEdgeGrass = ExpectedEdgeGrassCount(InSeasonProfile);
    TArray<FScoredIndex> EdgeCandidates;
    for (int32 Index = 0; Index < V4EdgeGrassWorldTransforms.Num(); ++Index)
    {
        const FVector Location =
            V4EdgeGrassWorldTransforms[Index].GetTranslation();
        if (IsInsideHeroLawn(Location) ||
            FMath::Abs(Location.X) < CeremonialAxisHalfWidthCm)
        {
            continue;
        }
        const uint32 Hash = HashTransform(
            V4EdgeGrassWorldTransforms[Index],
            0xE51B92A7u);
        const double Macro = SpatialClumpNoise(
            Location.X,
            Location.Y,
            1350.0,
            0x926D78C1u);
        EdgeCandidates.Add({Index, HashUnit(Hash) + 0.20 * (1.0 - Macro), Hash});
    }
    SortScored(EdgeCandidates);
    if (EdgeCandidates.Num() < DesiredEdgeGrass)
    {
        OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
        OutError = FString::Printf(
            TEXT("Only %d of %d required V5D edge-grass transforms remain outside the actual hero lawn and ceremonial axis."),
            EdgeCandidates.Num(),
            DesiredEdgeGrass);
        return false;
    }
    for (int32 Ordinal = 0; Ordinal < DesiredEdgeGrass; ++Ordinal)
    {
        const FScoredIndex& Candidate = EdgeCandidates[Ordinal];
        FTransform Edge = V4EdgeGrassWorldTransforms[Candidate.Index];
        FRotator Rotation = Edge.Rotator();
        Rotation.Yaw += FMath::Lerp(
            -19.0,
            19.0,
            HashUnit(Candidate.Hash ^ 0xD4192F83u));
        Edge.SetRotation(Rotation.Quaternion());
        const double SeasonalScale =
            InSeasonProfile == ETRIADIstanaExploreV5DSeasonProfile::HumidWet
            ? 1.06
            : (InSeasonProfile ==
                       ETRIADIstanaExploreV5DSeasonProfile::Transition
                   ? 1.0
                   : 0.92);
        Edge.SetScale3D(
            Edge.GetScale3D() * SeasonalScale *
            FMath::Lerp(
                0.88,
                1.12,
                HashUnit(Candidate.Hash ^ 0x65B24C17u)));
        OutLayout.EdgeGrass.Add(Edge);
    }

    if (OutLayout.GrassTotal() != LayoutGrassCount ||
        OutLayout.GrassManicured.IsEmpty() ||
        OutLayout.GrassHumid.IsEmpty() ||
        OutLayout.GrassShade.IsEmpty() || OutLayout.GrassDryEdge.IsEmpty() ||
        !ValidateTransformArray(
            OutLayout.EdgeGrass,
            DesiredEdgeGrass,
            TEXT("edge-grass detail"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.SupplementalSoil,
            SupplementalSoilCount,
            TEXT("supplemental soil"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.SupplementalShrubs,
            SupplementalSoilCount * ShrubsPerPatch,
            TEXT("supplemental shrubs"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.SupplementalUnderstorey,
            SupplementalSoilCount * UnderstoreyPerPatch,
            TEXT("supplemental understorey"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.SupplementalFlowers,
            SupplementalSoilCount * FlowersPerPatch,
            TEXT("supplemental flowers"),
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The V5D grass profile census lost its deterministic manicured/humid diversity.");
        }
        OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
        return false;
    }
    for (const FTransform& Transform : OutLayout.SupplementalSoil)
    {
        if (IsInsideHeroLawn(Transform.GetTranslation()))
        {
            OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
            OutError = TEXT("A supplemental soil patch entered the protected V5D hero lawn.");
            return false;
        }
    }
    for (const FTransform& Transform : OutLayout.EdgeGrass)
    {
        if (IsInsideHeroLawn(Transform.GetTranslation()))
        {
            OutLayout = FTRIADIstanaExploreV5DGroundVegetationLayout();
            OutError = TEXT("A taller edge-grass instance entered the protected V5D hero lawn.");
            return false;
        }
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::ExtractSourceTransforms(
    TArray<FTransform>& OutGrass,
    TArray<FTransform>& OutEdgeGrass,
    TArray<FTransform>& OutTrees,
    TArray<FTransform>& OutExistingSoil,
    FString& OutError) const
{
    OutGrass.Reset();
    OutEdgeGrass.Reset();
    OutTrees.Reset();
    OutExistingSoil.Reset();
    if (!V4LandscapeActor || !V5BVisualActor ||
        V4LandscapeActor->GetWorld() != GetWorld() ||
        V5BVisualActor->GetWorld() != GetWorld() ||
        !V4LandscapeActor->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !V5BVisualActor->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !AppendWorldTransforms(
            V5BVisualActor->AccentTurfInstances,
            OutGrass,
            OutError) ||
        !AppendWorldTransforms(
            V4LandscapeActor->GeometryGrassInstances,
            OutEdgeGrass,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D requires identity V4 and V5B source actors in the same world.");
        }
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* TreeComponents[] = {
        V4LandscapeActor->UmbrellaTreeInstances,
        V4LandscapeActor->DomeTreeInstances,
        V4LandscapeActor->HighForkRoundedTreeInstances,
        V4LandscapeActor->ColumnarNarrowTreeInstances,
        V4LandscapeActor->PalmTreeInstances,
        V4LandscapeActor->HeritageUmbrellaInstances,
        V4LandscapeActor->HeritageDomeInstances,
        V4LandscapeActor->HeritageHighForkRoundedInstances,
        V4LandscapeActor->HeritageColumnarNarrowInstances,
        V4LandscapeActor->HeritagePalmInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         TreeComponents)
    {
        if (!AppendWorldTransforms(Component, OutTrees, OutError))
        {
            return false;
        }
    }
    if (!AppendWorldTransforms(
            V5BVisualActor->TreeBaseMulchInstances,
            OutExistingSoil,
            OutError) ||
        OutGrass.Num() != SourceGrassCount ||
        OutEdgeGrass.Num() != SourceEdgeGrassCount ||
        OutTrees.Num() != SourceTreeCount ||
        OutExistingSoil.Num() != ExistingSoilCount)
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("V5D source census drift: grass=%d/%d edge=%d/%d trees=%d/%d soil=%d/%d."),
                OutGrass.Num(),
                SourceGrassCount,
                OutEdgeGrass.Num(),
                SourceEdgeGrassCount,
                OutTrees.Num(),
                SourceTreeCount,
                OutExistingSoil.Num(),
                ExistingSoilCount);
        }
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DGroundVegetationActor::ClearOwnedInstances()
{
    if (GroundMacroVariationOverlay)
    {
        GroundMacroVariationOverlay->SetStaticMesh(nullptr);
        GroundMacroVariationOverlay->EmptyOverrideMaterials();
        GroundMacroVariationOverlay->SetRelativeTransform(FTransform::Identity);
    }
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances,
        EdgeGrassInstances,
        SupplementalSoilInstances,
        SupplementalShrubInstances,
        SupplementalUnderstoreyInstances,
        SupplementalFlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (Component)
        {
            Component->ClearInstances();
            Component->SetStaticMesh(nullptr);
            Component->EmptyOverrideMaterials();
        }
    }
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::ApplyLayout(
    const FTRIADIstanaExploreV5DGroundVegetationLayout& Layout,
    FString& OutError)
{
    const int32 ExpectedGrassCount =
        GrassPresentationRevision == PredecessorGrassPresentationRevision
        ? R14GrassMicroDetailCount
        : (GrassPresentationRevision == GrassPresentationRevisionR20
               ? R20GrassMicroDetailCount
               : (GrassPresentationRevision == GrassPresentationRevisionR23
                      ? GrassMicroDetailCount
                      : INDEX_NONE));
    if (!ValidateAssetRoster(SavedAssets, OutError) ||
        ExpectedGrassCount == INDEX_NONE ||
        Layout.GrassTotal() != ExpectedGrassCount ||
        Layout.EdgeGrass.Num() != ExpectedEdgeGrassCount(SeasonProfile) ||
        Layout.SupplementalSoil.Num() != SupplementalSoilCount ||
        Layout.SupplementalShrubs.Num() != SupplementalSoilCount * ShrubsPerPatch ||
        Layout.SupplementalUnderstorey.Num() !=
            SupplementalSoilCount * UnderstoreyPerPatch ||
        Layout.SupplementalFlowers.Num() != SupplementalSoilCount * FlowersPerPatch)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The saved V5D layout census is incomplete.");
        }
        return false;
    }

    // Repair maps serialized before the overlay mobility contract was added,
    // then restore the saved transform without a static-component move.
    if (GroundMacroVariationOverlay->Mobility != EComponentMobility::Movable)
    {
        GroundMacroVariationOverlay->SetMobility(EComponentMobility::Movable);
    }
    GroundMacroVariationOverlay->SetStaticMesh(SavedAssets.GroundOverlayMesh);
    GroundMacroVariationOverlay->EmptyOverrideMaterials();
    GroundMacroVariationOverlay->SetMaterial(
        0,
        SavedAssets.GroundOverlayMaterial);
    if (!GroundMacroVariationOverlay->GetComponentTransform().Equals(
            SavedGroundOverlayWorldTransform,
            0.0001))
    {
        GroundMacroVariationOverlay->SetWorldTransform(
            SavedGroundOverlayWorldTransform);
    }

    UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    const TArray<FTransform>* GrassTransforms[] = {
        &Layout.GrassManicured,
        &Layout.GrassHumid,
        &Layout.GrassShade,
        &Layout.GrassDryEdge};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassComponents); ++Index)
    {
        GrassComponents[Index]->ClearInstances();
        GrassComponents[Index]->SetStaticMesh(SavedAssets.GrassMicroDetailMesh);
        GrassComponents[Index]->EmptyOverrideMaterials();
        GrassComponents[Index]->SetMaterial(
            0,
            SavedAssets.GrassProfileMaterials[Index]);
        GrassComponents[Index]->AddInstances(
            *GrassTransforms[Index],
            false,
            true,
            false);
    }

    struct FComponentPopulation
    {
        UHierarchicalInstancedStaticMeshComponent* Component;
        UStaticMesh* Mesh;
        UMaterialInterface* Material;
        const TArray<FTransform>* Transforms;
    };
    const FComponentPopulation Populations[] = {
        {EdgeGrassInstances, SavedAssets.EdgeGrassMesh, SavedAssets.EdgeGrassMaterial, &Layout.EdgeGrass},
        {SupplementalSoilInstances, SavedAssets.SoilPatchMesh, SavedAssets.SoilPatchMaterial, &Layout.SupplementalSoil},
        {SupplementalShrubInstances, SavedAssets.ShrubMesh, SavedAssets.ShrubMaterial, &Layout.SupplementalShrubs},
        {SupplementalUnderstoreyInstances, SavedAssets.UnderstoreyMesh, SavedAssets.UnderstoreyMaterial, &Layout.SupplementalUnderstorey},
        {SupplementalFlowerInstances, SavedAssets.FlowerMesh, SavedAssets.FlowerMaterial, &Layout.SupplementalFlowers}};
    for (const FComponentPopulation& Population : Populations)
    {
        Population.Component->ClearInstances();
        Population.Component->SetStaticMesh(Population.Mesh);
        Population.Component->EmptyOverrideMaterials();
        Population.Component->SetMaterial(0, Population.Material);
        Population.Component->AddInstances(
            *Population.Transforms,
            false,
            true,
            false);
    }

    bool bCurrentTreesFullyBuilt = true;
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances,
        EdgeGrassInstances,
        SupplementalSoilInstances,
        SupplementalShrubInstances,
        SupplementalUnderstoreyInstances,
        SupplementalFlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        Component->BuildTreeIfOutdated(
            /*Async*/ false,
            /*ForceUpdate*/ true);
        // UE 5.5 can retain bIsAsyncBuilding for an older registration build
        // after this newer synchronous build has made the current tree valid.
        // ApplyBuildTreeAsync discards that stale result when the tree is no
        // longer out of date, so readiness is the current-tree invariant.
        bCurrentTreesFullyBuilt = bCurrentTreesFullyBuilt &&
            Component->IsTreeFullyBuilt();
    }
    if (!bCurrentTreesFullyBuilt)
    {
        ClearOwnedInstances();
        OutError = TEXT(
            "A V5D render-only HISM did not expose a fully built current "
            "cluster tree after deterministic synchronous build.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ConfigureGroundVegetationRealism(
        ATRIADIstanaPublicViewSceneActor* InPublicViewScene,
        ATRIADIstanaExploreV4LandscapeActor* InV4Landscape,
        ATRIADIstanaExploreV5BVisualActor* InV5BVisual,
        const FTRIADIstanaExploreV5DGroundVegetationAssets& InAssets,
        ETRIADIstanaExploreV5DSeasonProfile InSeasonProfile,
        FString& OutError)
{
    if (bConfigured || !GetWorld() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidateAssetRoster(InAssets, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5D ground/vegetation configuration requires a fresh identity actor and exact assets.");
        }
        return false;
    }

    PublicViewSceneActor = InPublicViewScene;
    V4LandscapeActor = InV4Landscape;
    V5BVisualActor = InV5BVisual;
    SavedAssets = InAssets;
    SeasonProfile = InSeasonProfile;
    GrassPresentationRevision = GrassPresentationRevisionR23;
    ConfigureGrassPresentationComponentsForRevision(
        GrassPresentationRevisionR23);

    if (!PublicViewSceneActor || !PublicViewSceneActor->TerrainComponent ||
        !V4LandscapeActor || !V5BVisualActor ||
        PublicViewSceneActor->GetWorld() != GetWorld() ||
        PublicViewSceneActor->TerrainComponent->GetStaticMesh() !=
            SavedAssets.GroundOverlayMesh ||
        PublicViewSceneActor->TerrainComponent->GetNumMaterials() != 1 ||
        !PublicViewSceneActor->TerrainComponent->GetMaterial(0) ||
        PublicViewSceneActor->TerrainComponent->GetMaterial(0)->GetPathName() !=
            SourceTerrainLawnMaterialPath ||
        PublicViewSceneActor->TerrainComponent->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics ||
        !PublicViewSceneActor->TerrainComponent->IsVisible() ||
        PublicViewSceneActor->TerrainComponent->bHiddenInGame ||
        V5BVisualActor->AccentTurfInstances->GetStaticMesh() !=
            SavedAssets.GrassMicroDetailMesh ||
        V4LandscapeActor->GeometryGrassInstances->GetStaticMesh() !=
            SavedAssets.EdgeGrassMesh ||
        V5BVisualActor->TreeBaseMulchInstances->GetStaticMesh() !=
            SavedAssets.SoilPatchMesh ||
        V4LandscapeActor->ShrubInstances->GetStaticMesh() != SavedAssets.ShrubMesh ||
        V4LandscapeActor->UnderstoreyInstances->GetStaticMesh() !=
            SavedAssets.UnderstoreyMesh ||
        V4LandscapeActor->FlowerInstances->GetStaticMesh() != SavedAssets.FlowerMesh)
    {
        OutError = TEXT("V5D assets must reuse the exact admitted public-view/V4/V5B source meshes without changing the authoritative terrain component.");
        ClearOwnedInstances();
        return false;
    }

    SavedSourceTerrainMaterial =
        PublicViewSceneActor->TerrainComponent->GetMaterial(0);
    SavedSourceTerrainWorldTransform =
        PublicViewSceneActor->TerrainComponent->GetComponentTransform();
    bSavedSourceTerrainVisible =
        PublicViewSceneActor->TerrainComponent->IsVisible();
    bSavedSourceTerrainHiddenInGame =
        PublicViewSceneActor->TerrainComponent->bHiddenInGame;
    SavedGroundOverlayWorldTransform = SavedSourceTerrainWorldTransform;
    FVector GroundOverlayLocation =
        SavedGroundOverlayWorldTransform.GetTranslation();
    GroundOverlayLocation.Z += GroundOverlayOffsetCm;
    SavedGroundOverlayWorldTransform.SetTranslation(GroundOverlayLocation);
    if (!IsFinitePositiveTransform(SavedSourceTerrainWorldTransform) ||
        !IsFinitePositiveTransform(SavedGroundOverlayWorldTransform))
    {
        OutError = TEXT("V5D ground macro overlay requires a finite positive source-terrain transform.");
        ClearOwnedInstances();
        return false;
    }

    TArray<FTransform> Grass;
    TArray<FTransform> EdgeGrass;
    TArray<FTransform> Trees;
    TArray<FTransform> ExistingSoil;
    FTRIADIstanaExploreV5DGroundVegetationLayout Layout;
    if (!ExtractSourceTransforms(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            OutError) ||
        !BuildDeterministicLayout(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            SeasonProfile,
            Layout,
            OutError) ||
        !ApplyLayout(Layout, OutError))
    {
        ClearOwnedInstances();
        return false;
    }

    SavedGrassManicured = Layout.GrassManicured;
    SavedGrassHumid = Layout.GrassHumid;
    SavedGrassShade = Layout.GrassShade;
    SavedGrassDryEdge = Layout.GrassDryEdge;
    SavedEdgeGrass = Layout.EdgeGrass;
    SavedSupplementalSoil = Layout.SupplementalSoil;
    SavedSupplementalShrubs = Layout.SupplementalShrubs;
    SavedSupplementalUnderstorey = Layout.SupplementalUnderstorey;
    SavedSupplementalFlowers = Layout.SupplementalFlowers;
    bSourceTerrainRendererHiddenForReadyProvider = false;
    bSourceTerrainRendererRestoredForFallback = true;
    bLastContextPolicyStateValid = false;
    bConfigured = true;

    FString Report;
    if (!ValidateGroundVegetationRealism(Report))
    {
        bConfigured = false;
        ClearOwnedInstances();
        OutError = TEXT("V5D post-configuration validation failed: ") + Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateR14PredecessorForR20Migration(FString& OutReport) const
{
    FString Error;
    if (!bConfigured || !GetWorld() || HasActorBegunPlay() ||
        GrassPresentationRevision != PredecessorGrassPresentationRevision ||
        ClaimLabel != ExpectedClaimLabel() || !bAppearanceOnly ||
        bSourceTransformsModified || bCollisionOrNavigationAuthority ||
        bSensorOrRfMaterialAuthority || bSurveyOrAsBuiltClaimed ||
        bBotanicalInventoryClaimed || bCurrentSeasonOrWeatherClaimed ||
        bCesiumContentBakedCachedTracedOrAnalysedByGroundPass ||
        bGoogleOrOneMapContentUsed ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        bRuntimeSourceV5BTurfPresentationApplied ||
        bRuntimeSourceV5BTurfSnapshotValid ||
        RuntimeOriginalSourceV5BTurfOwnerActor.IsValid() ||
        RuntimeOriginalSourceV5BTurfComponent.IsValid() ||
        bRuntimeOriginalSourceV5BTurfHadPresentationTag ||
        bRuntimeEdgeGrassFadePresentationApplied ||
        bRuntimeEdgeGrassFadeSnapshotValid ||
        !ValidateAssetRoster(SavedAssets, Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D R20 migration requires one cold, configured, truth-preserving exact R14 predecessor with no transient presentation state.")
            : Error;
        return false;
    }

    if (!PublicViewSceneActor || !PublicViewSceneActor->TerrainComponent ||
        !V4LandscapeActor || !V5BVisualActor ||
        PublicViewSceneActor->GetWorld() != GetWorld() ||
        V4LandscapeActor->GetWorld() != GetWorld() ||
        V5BVisualActor->GetWorld() != GetWorld() ||
        V5BVisualActor->Tags.Contains(RuntimeSourceTurfPresentationTag()) ||
        !V5BVisualActor->AccentTurfInstances ||
        V5BVisualActor->AccentTurfInstances->GetMaterial(0) !=
            V5BVisualActor->SavedAssetRoster.AccentTurfMaterial ||
        !V5BVisualActor->AccentTurfInstances->IsVisible() ||
        V5BVisualActor->AccentTurfInstances->bHiddenInGame)
    {
        OutReport = TEXT("V5D R20 migration requires the exact cold visible/tag-free V5B turf source and exact local source actors.");
        return false;
    }

    TArray<FTransform> Grass;
    TArray<FTransform> EdgeGrass;
    TArray<FTransform> Trees;
    TArray<FTransform> ExistingSoil;
    FTRIADIstanaExploreV5DGroundVegetationLayout RebuiltR14;
    if (!ExtractSourceTransforms(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            Error) ||
        !BuildDeterministicLayoutForPresentationRevision(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            SeasonProfile,
            PredecessorGrassPresentationRevision,
            RebuiltR14,
            Error) ||
        !TransformArraysEqual(SavedGrassManicured, RebuiltR14.GrassManicured) ||
        !TransformArraysEqual(SavedGrassHumid, RebuiltR14.GrassHumid) ||
        !TransformArraysEqual(SavedGrassShade, RebuiltR14.GrassShade) ||
        !TransformArraysEqual(SavedGrassDryEdge, RebuiltR14.GrassDryEdge) ||
        !TransformArraysEqual(SavedEdgeGrass, RebuiltR14.EdgeGrass) ||
        !TransformArraysEqual(
            SavedSupplementalSoil,
            RebuiltR14.SupplementalSoil) ||
        !TransformArraysEqual(
            SavedSupplementalShrubs,
            RebuiltR14.SupplementalShrubs) ||
        !TransformArraysEqual(
            SavedSupplementalUnderstorey,
            RebuiltR14.SupplementalUnderstorey) ||
        !TransformArraysEqual(
            SavedSupplementalFlowers,
            RebuiltR14.SupplementalFlowers))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D R20 migration rejected a predecessor whose saved arrays no longer equal the exact deterministic R14 rebuild.")
            : Error;
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    const TArray<FTransform>* GrassTransforms[] = {
        &SavedGrassManicured,
        &SavedGrassHumid,
        &SavedGrassShade,
        &SavedGrassDryEdge};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassComponents); ++Index)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            GrassComponents[Index];
        int32 CullStart = 0;
        int32 CullEnd = 0;
        if (Component)
        {
            Component->GetCullDistances(CullStart, CullEnd);
        }
        if (!Component || Component->GetOwner() != this ||
            Component->GetStaticMesh() != SavedAssets.GrassMicroDetailMesh ||
            Component->GetMaterial(0) !=
                SavedAssets.GrassProfileMaterials[Index] ||
            Component->GetInstanceCount() != GrassTransforms[Index]->Num() ||
            CullStart != R14GrassCullStartDistanceCm ||
            CullEnd != R14GrassCullEndDistanceCm ||
            Component->WorldPositionOffsetDisableDistance !=
                R14GrassWpoDisableDistanceCm ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                R14GrassLodDistanceScale,
                0.0001f) ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            !HasOnlyIgnoredCollisionResponses(Component) ||
            Component->GetGenerateOverlapEvents() ||
            Component->CanEverAffectNavigation() ||
            Component->bEnableDensityScaling ||
            !Component->IsTreeFullyBuilt())
        {
            OutReport = FString::Printf(
                TEXT("V5D R20 migration rejected R14 grass component %d because its exact old mesh/material/census/cull/WPO/LOD/isolation state drifted."),
                Index);
            return false;
        }
    }

    int32 EdgeCullStart = 0;
    int32 EdgeCullEnd = 0;
    if (EdgeGrassInstances)
    {
        EdgeGrassInstances->GetCullDistances(EdgeCullStart, EdgeCullEnd);
    }
    if (!GroundMacroVariationOverlay ||
        GroundMacroVariationOverlay->GetStaticMesh() !=
            SavedAssets.GroundOverlayMesh ||
        GroundMacroVariationOverlay->GetMaterial(0) !=
            SavedAssets.GroundOverlayMaterial ||
        GroundMacroVariationOverlay->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        GroundMacroVariationOverlay->CanEverAffectNavigation() ||
        !EdgeGrassInstances ||
        EdgeGrassInstances->GetStaticMesh() != SavedAssets.EdgeGrassMesh ||
        EdgeGrassInstances->GetMaterial(0) != SavedAssets.EdgeGrassMaterial ||
        EdgeGrassInstances->GetInstanceCount() != SavedEdgeGrass.Num() ||
        EdgeCullStart != R14EdgeGrassCullStartDistanceCm ||
        EdgeCullEnd != R14EdgeGrassCullEndDistanceCm ||
        EdgeGrassInstances->WorldPositionOffsetDisableDistance !=
            R14EdgeGrassWpoDisableDistanceCm ||
        !FMath::IsNearlyEqual(
            EdgeGrassInstances->InstanceLODDistanceScale,
            R14GrassLodDistanceScale,
            0.0001f) ||
        EdgeGrassInstances->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        EdgeGrassInstances->CanEverAffectNavigation())
    {
        OutReport = TEXT("V5D R20 migration rejected a drifted R14 overlay or edge-grass component policy.");
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5D_R14_GRASS_PREDECESSOR_VALID exactDeterministicArrays=true grassMicroDetail=24576 maximumReuseLayers=2 offsetBandsCm=8..18,28..38 sourceV5BColdVisibleAndTagFree=true sourceMeshTransformsCensusCullWpoMaterialCollisionNavigationRfUntouched=true migrationMutationReady=true");
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateR20PredecessorForR23Migration(FString& OutReport) const
{
    FString Error;
    if (!bConfigured || !GetWorld() || HasActorBegunPlay() ||
        GrassPresentationRevision != GrassPresentationRevisionR20 ||
        ClaimLabel != ExpectedClaimLabel() || !bAppearanceOnly ||
        bSourceTransformsModified || bCollisionOrNavigationAuthority ||
        bSensorOrRfMaterialAuthority || bSurveyOrAsBuiltClaimed ||
        bBotanicalInventoryClaimed || bCurrentSeasonOrWeatherClaimed ||
        bCesiumContentBakedCachedTracedOrAnalysedByGroundPass ||
        bGoogleOrOneMapContentUsed ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        bRuntimeSourceV5BTurfPresentationApplied ||
        bRuntimeSourceV5BTurfSnapshotValid ||
        RuntimeOriginalSourceV5BTurfOwnerActor.IsValid() ||
        RuntimeOriginalSourceV5BTurfComponent.IsValid() ||
        bRuntimeOriginalSourceV5BTurfHadPresentationTag ||
        bRuntimeEdgeGrassFadePresentationApplied ||
        bRuntimeEdgeGrassFadeSnapshotValid ||
        !ValidateAssetRoster(SavedAssets, Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D R23 migration requires one cold, configured, truth-preserving exact R20 predecessor with no transient presentation state.")
            : Error;
        return false;
    }

    if (!PublicViewSceneActor || !PublicViewSceneActor->TerrainComponent ||
        !V4LandscapeActor || !V5BVisualActor ||
        PublicViewSceneActor->GetWorld() != GetWorld() ||
        V4LandscapeActor->GetWorld() != GetWorld() ||
        V5BVisualActor->GetWorld() != GetWorld() ||
        V5BVisualActor->Tags.Contains(RuntimeSourceTurfPresentationTag()) ||
        !V5BVisualActor->AccentTurfInstances ||
        V5BVisualActor->AccentTurfInstances->GetMaterial(0) !=
            V5BVisualActor->SavedAssetRoster.AccentTurfMaterial ||
        !V5BVisualActor->AccentTurfInstances->IsVisible() ||
        V5BVisualActor->AccentTurfInstances->bHiddenInGame)
    {
        OutReport = TEXT("V5D R23 migration requires the exact cold visible/tag-free V5B turf source and exact local source actors.");
        return false;
    }

    TArray<FTransform> Grass;
    TArray<FTransform> EdgeGrass;
    TArray<FTransform> Trees;
    TArray<FTransform> ExistingSoil;
    FTRIADIstanaExploreV5DGroundVegetationLayout RebuiltR20;
    if (!ExtractSourceTransforms(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            Error) ||
        !BuildDeterministicLayoutForPresentationRevision(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            SeasonProfile,
            GrassPresentationRevisionR20,
            RebuiltR20,
            Error) ||
        !TransformArraysEqual(SavedGrassManicured, RebuiltR20.GrassManicured) ||
        !TransformArraysEqual(SavedGrassHumid, RebuiltR20.GrassHumid) ||
        !TransformArraysEqual(SavedGrassShade, RebuiltR20.GrassShade) ||
        !TransformArraysEqual(SavedGrassDryEdge, RebuiltR20.GrassDryEdge) ||
        !TransformArraysEqual(SavedEdgeGrass, RebuiltR20.EdgeGrass) ||
        !TransformArraysEqual(
            SavedSupplementalSoil,
            RebuiltR20.SupplementalSoil) ||
        !TransformArraysEqual(
            SavedSupplementalShrubs,
            RebuiltR20.SupplementalShrubs) ||
        !TransformArraysEqual(
            SavedSupplementalUnderstorey,
            RebuiltR20.SupplementalUnderstorey) ||
        !TransformArraysEqual(
            SavedSupplementalFlowers,
            RebuiltR20.SupplementalFlowers))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D R23 migration rejected a predecessor whose saved arrays no longer equal the exact deterministic R20 rebuild.")
            : Error;
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    const TArray<FTransform>* GrassTransforms[] = {
        &SavedGrassManicured,
        &SavedGrassHumid,
        &SavedGrassShade,
        &SavedGrassDryEdge};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassComponents); ++Index)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            GrassComponents[Index];
        int32 CullStart = 0;
        int32 CullEnd = 0;
        if (Component)
        {
            Component->GetCullDistances(CullStart, CullEnd);
        }
        if (!Component || Component->GetOwner() != this ||
            Component->GetStaticMesh() != SavedAssets.GrassMicroDetailMesh ||
            Component->GetMaterial(0) !=
                SavedAssets.GrassProfileMaterials[Index] ||
            Component->GetInstanceCount() != GrassTransforms[Index]->Num() ||
            CullStart != R20GrassCullStartDistanceCm ||
            CullEnd != R20GrassCullEndDistanceCm ||
            Component->WorldPositionOffsetDisableDistance !=
                R20GrassWpoDisableDistanceCm ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                R20GrassLodDistanceScale,
                0.0001f) ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            !HasOnlyIgnoredCollisionResponses(Component) ||
            Component->GetGenerateOverlapEvents() ||
            Component->CanEverAffectNavigation() ||
            Component->bEnableDensityScaling ||
            Component->CastShadow || Component->bCastContactShadow ||
            Component->bAffectDistanceFieldLighting ||
            !Component->IsTreeFullyBuilt())
        {
            OutReport = FString::Printf(
                TEXT("V5D R23 migration rejected R20 grass component %d because its exact old mesh/material/census/cull/WPO/LOD/isolation/shadow state drifted."),
                Index);
            return false;
        }
    }

    int32 EdgeCullStart = 0;
    int32 EdgeCullEnd = 0;
    if (EdgeGrassInstances)
    {
        EdgeGrassInstances->GetCullDistances(EdgeCullStart, EdgeCullEnd);
    }
    if (!GroundMacroVariationOverlay ||
        GroundMacroVariationOverlay->GetStaticMesh() !=
            SavedAssets.GroundOverlayMesh ||
        GroundMacroVariationOverlay->GetMaterial(0) !=
            SavedAssets.GroundOverlayMaterial ||
        GroundMacroVariationOverlay->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        GroundMacroVariationOverlay->CanEverAffectNavigation() ||
        GroundMacroVariationOverlay->CastShadow ||
        GroundMacroVariationOverlay->bCastContactShadow ||
        GroundMacroVariationOverlay->bAffectDistanceFieldLighting ||
        !EdgeGrassInstances ||
        EdgeGrassInstances->GetStaticMesh() != SavedAssets.EdgeGrassMesh ||
        EdgeGrassInstances->GetMaterial(0) != SavedAssets.EdgeGrassMaterial ||
        EdgeGrassInstances->GetInstanceCount() != SavedEdgeGrass.Num() ||
        EdgeCullStart != R20EdgeGrassCullStartDistanceCm ||
        EdgeCullEnd != R20EdgeGrassCullEndDistanceCm ||
        EdgeGrassInstances->WorldPositionOffsetDisableDistance !=
            R20EdgeGrassWpoDisableDistanceCm ||
        !FMath::IsNearlyEqual(
            EdgeGrassInstances->InstanceLODDistanceScale,
            R20GrassLodDistanceScale,
            0.0001f) ||
        EdgeGrassInstances->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        !HasOnlyIgnoredCollisionResponses(EdgeGrassInstances) ||
        EdgeGrassInstances->GetGenerateOverlapEvents() ||
        EdgeGrassInstances->CanEverAffectNavigation() ||
        EdgeGrassInstances->CastShadow ||
        EdgeGrassInstances->bCastContactShadow ||
        EdgeGrassInstances->bAffectDistanceFieldLighting)
    {
        OutReport = TEXT("V5D R23 migration rejected a drifted R20 overlay or edge-grass component policy.");
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5D_R20_GRASS_PREDECESSOR_VALID exactDeterministicArrays=true grassMicroDetail=12288 maximumReuseLayers=1 offsetBandCm=28..38 sourceV5BColdVisibleAndTagFree=true sourceMeshTransformsCensusCullWpoMaterialCollisionNavigationRfShadowUntouched=true migrationMutationReady=true");
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    RebuildGroundVegetationLayoutToR20(FString& OutError)
{
    if (GrassPresentationRevision == GrassPresentationRevisionR20)
    {
        FString Report;
        const bool bValid = ValidateR20PredecessorForR23Migration(Report);
        OutError = bValid
            ? FString()
            : TEXT("V5D R20 rebuild idempotence requires an already-valid cold R20 actor: ") +
                Report;
        return bValid;
    }

    FString PredecessorReport;
    if (!ValidateR14PredecessorForR20Migration(PredecessorReport))
    {
        OutError = TEXT("V5D R20 rebuild refused its predecessor: ") +
            PredecessorReport;
        return false;
    }

    TArray<FTransform> Grass;
    TArray<FTransform> EdgeGrass;
    TArray<FTransform> Trees;
    TArray<FTransform> ExistingSoil;
    FTRIADIstanaExploreV5DGroundVegetationLayout R20Layout;
    if (!ExtractSourceTransforms(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            OutError) ||
        !BuildDeterministicLayoutForPresentationRevision(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            SeasonProfile,
            GrassPresentationRevisionR20,
            R20Layout,
            OutError))
    {
        return false;
    }

    FTRIADIstanaExploreV5DGroundVegetationLayout R14Layout;
    R14Layout.GrassManicured = SavedGrassManicured;
    R14Layout.GrassHumid = SavedGrassHumid;
    R14Layout.GrassShade = SavedGrassShade;
    R14Layout.GrassDryEdge = SavedGrassDryEdge;
    R14Layout.EdgeGrass = SavedEdgeGrass;
    R14Layout.SupplementalSoil = SavedSupplementalSoil;
    R14Layout.SupplementalShrubs = SavedSupplementalShrubs;
    R14Layout.SupplementalUnderstorey = SavedSupplementalUnderstorey;
    R14Layout.SupplementalFlowers = SavedSupplementalFlowers;

    const auto SaveLayout = [this](
        const FTRIADIstanaExploreV5DGroundVegetationLayout& Layout)
    {
        SavedGrassManicured = Layout.GrassManicured;
        SavedGrassHumid = Layout.GrassHumid;
        SavedGrassShade = Layout.GrassShade;
        SavedGrassDryEdge = Layout.GrassDryEdge;
        SavedEdgeGrass = Layout.EdgeGrass;
        SavedSupplementalSoil = Layout.SupplementalSoil;
        SavedSupplementalShrubs = Layout.SupplementalShrubs;
        SavedSupplementalUnderstorey = Layout.SupplementalUnderstorey;
        SavedSupplementalFlowers = Layout.SupplementalFlowers;
    };
    const auto RestoreR14 = [this, &R14Layout, &SaveLayout](
        FString& OutRollbackReport)
    {
        GrassPresentationRevision = PredecessorGrassPresentationRevision;
        ConfigureGrassPresentationComponentsForRevision(
            PredecessorGrassPresentationRevision);
        SaveLayout(R14Layout);
        FString ApplyError;
        if (!ApplyLayout(R14Layout, ApplyError))
        {
            OutRollbackReport = TEXT("R14 rollback apply failed: ") + ApplyError;
            return false;
        }
        FString ValidateReport;
        if (!ValidateR14PredecessorForR20Migration(ValidateReport))
        {
            OutRollbackReport = TEXT("R14 rollback readback failed: ") +
                ValidateReport;
            return false;
        }
        OutRollbackReport = TEXT("exact R14 rollback validated");
        return true;
    };

    GrassPresentationRevision = GrassPresentationRevisionR20;
    ConfigureGrassPresentationComponentsForRevision(
        GrassPresentationRevisionR20);
    FString ApplyError;
    if (!ApplyLayout(R20Layout, ApplyError))
    {
        FString RollbackReport;
        const bool bRolledBack = RestoreR14(RollbackReport);
        OutError = TEXT("V5D R20 layout apply failed: ") + ApplyError +
            TEXT(" rollback=") +
            (bRolledBack ? RollbackReport
                         : TEXT("FAILED: ") + RollbackReport);
        return false;
    }
    SaveLayout(R20Layout);

    FString R20Report;
    if (!ValidateR20PredecessorForR23Migration(R20Report))
    {
        FString RollbackReport;
        const bool bRolledBack = RestoreR14(RollbackReport);
        OutError = TEXT("V5D R20 layout validation failed: ") + R20Report +
            TEXT(" rollback=") +
            (bRolledBack ? RollbackReport
                         : TEXT("FAILED: ") + RollbackReport);
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    RebuildGroundVegetationLayoutToR23(FString& OutError)
{
    if (GrassPresentationRevision == GrassPresentationRevisionR23)
    {
        FString Report;
        const bool bValid = !HasActorBegunPlay() &&
            ValidateGroundVegetationRealism(Report);
        OutError = bValid
            ? FString()
            : TEXT("V5D R23 rebuild idempotence requires an already-valid cold R23 actor: ") +
                Report;
        return bValid;
    }

    FString PredecessorReport;
    if (!ValidateR20PredecessorForR23Migration(PredecessorReport))
    {
        OutError = TEXT("V5D R23 rebuild refused its predecessor: ") +
            PredecessorReport;
        return false;
    }

    TArray<FTransform> Grass;
    TArray<FTransform> EdgeGrass;
    TArray<FTransform> Trees;
    TArray<FTransform> ExistingSoil;
    FTRIADIstanaExploreV5DGroundVegetationLayout R23Layout;
    if (!ExtractSourceTransforms(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            OutError) ||
        !BuildDeterministicLayoutForPresentationRevision(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            SeasonProfile,
            GrassPresentationRevisionR23,
            R23Layout,
            OutError))
    {
        return false;
    }

    FTRIADIstanaExploreV5DGroundVegetationLayout R20Layout;
    R20Layout.GrassManicured = SavedGrassManicured;
    R20Layout.GrassHumid = SavedGrassHumid;
    R20Layout.GrassShade = SavedGrassShade;
    R20Layout.GrassDryEdge = SavedGrassDryEdge;
    R20Layout.EdgeGrass = SavedEdgeGrass;
    R20Layout.SupplementalSoil = SavedSupplementalSoil;
    R20Layout.SupplementalShrubs = SavedSupplementalShrubs;
    R20Layout.SupplementalUnderstorey = SavedSupplementalUnderstorey;
    R20Layout.SupplementalFlowers = SavedSupplementalFlowers;

    const auto SaveLayout = [this](
        const FTRIADIstanaExploreV5DGroundVegetationLayout& Layout)
    {
        SavedGrassManicured = Layout.GrassManicured;
        SavedGrassHumid = Layout.GrassHumid;
        SavedGrassShade = Layout.GrassShade;
        SavedGrassDryEdge = Layout.GrassDryEdge;
        SavedEdgeGrass = Layout.EdgeGrass;
        SavedSupplementalSoil = Layout.SupplementalSoil;
        SavedSupplementalShrubs = Layout.SupplementalShrubs;
        SavedSupplementalUnderstorey = Layout.SupplementalUnderstorey;
        SavedSupplementalFlowers = Layout.SupplementalFlowers;
    };
    const auto RestoreR20 = [this, &R20Layout, &SaveLayout](
        FString& OutRollbackReport)
    {
        GrassPresentationRevision = GrassPresentationRevisionR20;
        ConfigureGrassPresentationComponentsForRevision(
            GrassPresentationRevisionR20);
        SaveLayout(R20Layout);
        FString ApplyError;
        if (!ApplyLayout(R20Layout, ApplyError))
        {
            OutRollbackReport = TEXT("R20 rollback apply failed: ") +
                ApplyError;
            return false;
        }
        FString ValidateReport;
        if (!ValidateR20PredecessorForR23Migration(ValidateReport))
        {
            OutRollbackReport = TEXT("R20 rollback readback failed: ") +
                ValidateReport;
            return false;
        }
        OutRollbackReport = TEXT("exact R20 rollback validated");
        return true;
    };

    GrassPresentationRevision = GrassPresentationRevisionR23;
    ConfigureGrassPresentationComponentsForRevision(
        GrassPresentationRevisionR23);
    FString ApplyError;
    if (!ApplyLayout(R23Layout, ApplyError))
    {
        FString RollbackReport;
        const bool bRolledBack = RestoreR20(RollbackReport);
        OutError = TEXT("V5D R23 layout apply failed: ") + ApplyError +
            TEXT(" rollback=") +
            (bRolledBack ? RollbackReport
                         : TEXT("FAILED: ") + RollbackReport);
        return false;
    }
    SaveLayout(R23Layout);

    FString R23Report;
    if (!ValidateGroundVegetationRealism(R23Report))
    {
        FString RollbackReport;
        const bool bRolledBack = RestoreR20(RollbackReport);
        OutError = TEXT("V5D R23 layout validation failed: ") + R23Report +
            TEXT(" rollback=") +
            (bRolledBack ? RollbackReport
                         : TEXT("FAILED: ") + RollbackReport);
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ReapplyGroundVegetationRealism(FString& OutError)
{
    if (!bConfigured ||
        GrassPresentationRevision != GrassPresentationRevisionR23)
    {
        OutError = bConfigured
            ? TEXT("The V5D ground/vegetation actor requires the explicit cold staged R14-to-R20-to-R23 layout migration before reapply.")
            : TEXT("The V5D ground/vegetation actor has no persisted configuration.");
        return false;
    }
    const bool bRestoreRuntimeEdgeFadeAfterLayout = HasActorBegunPlay();
    if (bRuntimeEdgeGrassFadePresentationApplied ||
        bRuntimeEdgeGrassFadeSnapshotValid)
    {
        FString RestoreError;
        if (!RestoreRuntimeEdgeGrassFadePresentation(RestoreError))
        {
            OutError = TEXT("V5D edge-grass reapply refused to overwrite a runtime fade snapshot that could not be restored exactly: ") +
                RestoreError;
            return false;
        }
    }

    TArray<TObjectPtr<UMaterialInterface>> ColdEdgeOverrideMaterials;
    TObjectPtr<UMaterialInterface> ColdEdgeResolvedMaterial = nullptr;
    TObjectPtr<UStaticMesh> ColdEdgeMesh = nullptr;
    bool bColdEdgeMaterialStateCaptured = false;
    if (bRestoreRuntimeEdgeFadeAfterLayout)
    {
        if (!EdgeGrassInstances || EdgeGrassInstances->GetOwner() != this ||
            EdgeGrassInstances->GetFName() !=
                FName(TEXT("V5DEdgeGrassSeasonalAccents")) ||
            EdgeGrassInstances->GetStaticMesh() !=
                SavedAssets.EdgeGrassMesh ||
            EdgeGrassInstances->GetMaterial(0) !=
                SavedAssets.EdgeGrassMaterial)
        {
            OutError = TEXT("V5D edge-grass reapply requires the exact cold serialized component/material state before layout mutation.");
            return false;
        }
        ColdEdgeOverrideMaterials = EdgeGrassInstances->OverrideMaterials;
        ColdEdgeResolvedMaterial = EdgeGrassInstances->GetMaterial(0);
        ColdEdgeMesh = EdgeGrassInstances->GetStaticMesh();
        bColdEdgeMaterialStateCaptured = true;
    }
    const auto ReplayExactColdEdgeMaterialState = [
        this,
        &ColdEdgeOverrideMaterials,
        &ColdEdgeResolvedMaterial,
        &ColdEdgeMesh,
        &bColdEdgeMaterialStateCaptured](FString& OutReplayError)
    {
        if (!bColdEdgeMaterialStateCaptured)
        {
            OutReplayError.Reset();
            return true;
        }
        if (!EdgeGrassInstances || EdgeGrassInstances->GetOwner() != this ||
            EdgeGrassInstances->GetFName() !=
                FName(TEXT("V5DEdgeGrassSeasonalAccents")))
        {
            OutReplayError = TEXT("The exact edge-grass component captured before layout no longer resolves for material-array replay.");
            return false;
        }
        if (EdgeGrassInstances->GetStaticMesh() != ColdEdgeMesh)
        {
            EdgeGrassInstances->SetStaticMesh(ColdEdgeMesh);
        }
        EdgeGrassInstances->EmptyOverrideMaterials();
        for (int32 Index = 0;
             Index < ColdEdgeOverrideMaterials.Num();
             ++Index)
        {
            EdgeGrassInstances->SetMaterial(
                Index,
                ColdEdgeOverrideMaterials[Index].Get());
        }
        EdgeGrassInstances->MarkRenderStateDirty();
        if (!OverrideMaterialArraysEqual(
                EdgeGrassInstances->OverrideMaterials,
                ColdEdgeOverrideMaterials) ||
            EdgeGrassInstances->GetStaticMesh() != ColdEdgeMesh ||
            EdgeGrassInstances->GetMaterial(0) !=
                ColdEdgeResolvedMaterial)
        {
            OutReplayError = TEXT("The complete cold edge-grass override array or resolved slot zero did not replay exactly after layout.");
            return false;
        }
        OutReplayError.Reset();
        return true;
    };

    FTRIADIstanaExploreV5DGroundVegetationLayout Layout;
    Layout.GrassManicured = SavedGrassManicured;
    Layout.GrassHumid = SavedGrassHumid;
    Layout.GrassShade = SavedGrassShade;
    Layout.GrassDryEdge = SavedGrassDryEdge;
    Layout.EdgeGrass = SavedEdgeGrass;
    Layout.SupplementalSoil = SavedSupplementalSoil;
    Layout.SupplementalShrubs = SavedSupplementalShrubs;
    Layout.SupplementalUnderstorey = SavedSupplementalUnderstorey;
    Layout.SupplementalFlowers = SavedSupplementalFlowers;
    FString LayoutError;
    const bool bLayoutApplied = ApplyLayout(Layout, LayoutError);
    FString ReplayError;
    const bool bColdEdgeMaterialStateReplayed =
        ReplayExactColdEdgeMaterialState(ReplayError);
    if (!bLayoutApplied || !bColdEdgeMaterialStateReplayed)
    {
        OutError = bLayoutApplied
            ? FString(TEXT("V5D edge-grass post-layout cold material replay failed: ")) +
                ReplayError
            : FString(TEXT("V5D layout reapply failed: ")) + LayoutError +
                (bColdEdgeMaterialStateReplayed
                    ? FString(TEXT(" exactColdEdgeMaterialReplay=true"))
                    : FString(TEXT(" exactColdEdgeMaterialReplay=false ")) +
                        ReplayError);
        return false;
    }
    if (bRestoreRuntimeEdgeFadeAfterLayout &&
        !EnsureRuntimeEdgeGrassFadePresentation(OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateAppliedRuntimeSourceTurfPresentationForSourceActor(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport) const
{
    FString AssetError;
    if (!ValidateAssetRoster(SavedAssets, AssetError))
    {
        OutReport = TEXT("V5D inherited-turf runtime presentation asset roster is invalid: ") +
            AssetError;
        return false;
    }
    const UHierarchicalInstancedStaticMeshComponent* Component = Candidate
        ? Candidate->AccentTurfInstances.Get()
        : nullptr;
    if (!bConfigured || !bRuntimeSourceV5BTurfPresentationApplied ||
        !bRuntimeSourceV5BTurfSnapshotValid || !HasActorBegunPlay() ||
        !Candidate || Candidate != V5BVisualActor ||
        Candidate->GetClass() !=
            ATRIADIstanaExploreV5BVisualActor::StaticClass() ||
        Candidate->GetWorld() != GetWorld() ||
        !Candidate->HasActorBegunPlay() ||
        !Candidate->Tags.Contains(RuntimeSourceTurfPresentationTag()) ||
        RuntimeOriginalSourceV5BTurfOwnerActor.Get() != Candidate ||
        RuntimeOriginalSourceV5BTurfComponent.Get() != Component ||
        !Component || Component->GetOwner() != Candidate ||
        Component->GetFName() != FName(TEXT("V5BAccentTurfInstances")) ||
        Component->GetAttachParent() != Candidate->SceneRoot ||
        !Candidate->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !Component->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        RuntimeOriginalSourceV5BTurfMesh !=
            Candidate->SavedAssetRoster.AccentTurfMesh ||
        RuntimeOriginalSourceV5BTurfMesh !=
            SavedAssets.GrassMicroDetailMesh ||
        RuntimeOriginalSourceV5BTurfResolvedMaterial !=
            Candidate->SavedAssetRoster.AccentTurfMaterial ||
        RuntimeOriginalSourceV5BTurfInstanceCount != SourceGrassCount ||
        RuntimeOriginalSourceV5BTurfWorldTransforms.Num() !=
            SourceGrassCount ||
        RuntimeOriginalSourceV5BTurfCullStartDistanceCm !=
            SourceV5BTurfCullStartDistanceCm ||
        RuntimeOriginalSourceV5BTurfCullEndDistanceCm !=
            SourceV5BTurfCullEndDistanceCm ||
        RuntimeOriginalSourceV5BTurfWpoDisableDistanceCm !=
            SourceV5BTurfWpoDisableDistanceCm ||
        RuntimeOriginalSourceV5BTurfMobility !=
            static_cast<uint8>(EComponentMobility::Static) ||
        RuntimeOriginalSourceV5BTurfCollisionEnabled !=
            static_cast<uint8>(ECollisionEnabled::NoCollision) ||
        RuntimeOriginalSourceV5BTurfCollisionResponses.Num() !=
            CollisionResponseChannelCount ||
        RuntimeOriginalSourceV5BTurfForcedLodModel != 0 ||
        !bRuntimeOriginalSourceV5BTurfOverrideMinLod ||
        RuntimeOriginalSourceV5BTurfMinLod != 0 ||
        !FMath::IsNearlyEqual(
            RuntimeOriginalSourceV5BTurfLodDistanceScale,
            1.0f,
            0.0001f) ||
        RuntimeOriginalSourceV5BTurfNumCustomDataFloats != 0 ||
        !RuntimeOriginalSourceV5BTurfCustomData.IsEmpty() ||
        !bRuntimeOriginalSourceV5BTurfVisible ||
        bRuntimeOriginalSourceV5BTurfHiddenInGame ||
        bRuntimeOriginalSourceV5BTurfHadPresentationTag ||
        bRuntimeOriginalSourceV5BTurfCastShadow ||
        bRuntimeOriginalSourceV5BTurfCastContactShadow ||
        bRuntimeOriginalSourceV5BTurfGenerateOverlapEvents ||
        bRuntimeOriginalSourceV5BTurfCanEverAffectNavigation ||
        bRuntimeOriginalSourceV5BTurfEnableDensityScaling ||
        !FMath::IsNearlyEqual(
            RuntimeOriginalSourceV5BTurfCurrentDensityScaling,
            1.0f,
            0.0001f) ||
        !bRuntimeOriginalSourceV5BTurfAutoRebuildTree)
    {
        OutReport = TEXT("V5D inherited-turf runtime presentation ownership, exact source admission, or complete snapshot is invalid.");
        return false;
    }

    int32 CurrentCullStart = 0;
    int32 CurrentCullEnd = 0;
    Component->GetCullDistances(CurrentCullStart, CurrentCullEnd);
    if (Component->GetStaticMesh() != RuntimeOriginalSourceV5BTurfMesh ||
        Component->GetNumMaterials() != 1 ||
        Component->GetMaterial(0) !=
            RuntimeOriginalSourceV5BTurfResolvedMaterial ||
        !OverrideMaterialArraysEqual(
            Component->OverrideMaterials,
            RuntimeOriginalSourceV5BTurfOverrideMaterials) ||
        Component->GetInstanceCount() !=
            RuntimeOriginalSourceV5BTurfInstanceCount ||
        !Component->GetComponentTransform().Equals(
            RuntimeOriginalSourceV5BTurfComponentTransform,
            0.0001) ||
        CurrentCullStart != CompositeSourceV5BTurfCullStartDistanceCm ||
        CurrentCullEnd != CompositeSourceV5BTurfCullEndDistanceCm ||
        Component->WorldPositionOffsetDisableDistance !=
            RuntimeOriginalSourceV5BTurfWpoDisableDistanceCm ||
        static_cast<uint8>(Component->Mobility) !=
            RuntimeOriginalSourceV5BTurfMobility ||
        static_cast<uint8>(Component->GetCollisionEnabled()) !=
            RuntimeOriginalSourceV5BTurfCollisionEnabled ||
        !Component->IsVisible() || Component->bHiddenInGame ||
        Component->CastShadow != bRuntimeOriginalSourceV5BTurfCastShadow ||
        Component->bCastContactShadow !=
            bRuntimeOriginalSourceV5BTurfCastContactShadow ||
        Component->bAffectDistanceFieldLighting !=
            bRuntimeOriginalSourceV5BTurfAffectDistanceFieldLighting ||
        Component->GetGenerateOverlapEvents() !=
            bRuntimeOriginalSourceV5BTurfGenerateOverlapEvents ||
        Component->CanEverAffectNavigation() !=
            bRuntimeOriginalSourceV5BTurfCanEverAffectNavigation ||
        Component->bEnableDensityScaling !=
            bRuntimeOriginalSourceV5BTurfEnableDensityScaling ||
        !FMath::IsNearlyEqual(
            Component->CurrentDensityScaling,
            RuntimeOriginalSourceV5BTurfCurrentDensityScaling,
            0.0001f) ||
        Component->bAutoRebuildTreeOnInstanceChanges !=
            bRuntimeOriginalSourceV5BTurfAutoRebuildTree ||
        Component->ForcedLodModel !=
            RuntimeOriginalSourceV5BTurfForcedLodModel ||
        Component->bOverrideMinLOD !=
            bRuntimeOriginalSourceV5BTurfOverrideMinLod ||
        Component->MinLOD != RuntimeOriginalSourceV5BTurfMinLod ||
        !FMath::IsNearlyEqual(
            Component->InstanceLODDistanceScale,
            CompositeSourceV5BTurfLodDistanceScale,
            0.0001f) ||
        Component->NumCustomDataFloats !=
            RuntimeOriginalSourceV5BTurfNumCustomDataFloats ||
        Component->PerInstanceSMCustomData !=
            RuntimeOriginalSourceV5BTurfCustomData ||
        Component->IsAsyncBuilding() || !Component->IsTreeFullyBuilt())
    {
        OutReport = TEXT("V5D inherited turf changed outside its bounded visible medium-range cull/LOD presentation slot.");
        return false;
    }
    for (int32 ChannelIndex = 0;
         ChannelIndex < RuntimeOriginalSourceV5BTurfCollisionResponses.Num();
         ++ChannelIndex)
    {
        const uint8 CurrentResponse = static_cast<uint8>(
            Component->GetCollisionResponseToChannel(
                static_cast<ECollisionChannel>(ChannelIndex)));
        if (CurrentResponse !=
                RuntimeOriginalSourceV5BTurfCollisionResponses[ChannelIndex] ||
            CurrentResponse != static_cast<uint8>(ECR_Ignore))
        {
            OutReport = TEXT("V5D inherited turf collision-response isolation changed.");
            return false;
        }
    }

    TArray<FTransform> CurrentTransforms;
    CurrentTransforms.Reserve(SourceGrassCount);
    FString TransformError;
    if (!AppendWorldTransforms(Component, CurrentTransforms, TransformError) ||
        !TransformArraysEqual(
            CurrentTransforms,
            RuntimeOriginalSourceV5BTurfWorldTransforms))
    {
        OutReport = TransformError.IsEmpty()
            ? TEXT("V5D inherited turf transforms, order, or exact census changed.")
            : TransformError;
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5D_SOURCE_TURF_PRESENTATION_VALID inheritedV5BAccentTurf=18432 runtimeCompositeV5BAndV5DGrassOwnership=true sourceRendererVisible=true sourceRendererHiddenInGame=false mediumRangeCullCm=4000,6500 mediumRangeLodScale=1.10 exactSourceActorAndComponentIdentityPinned=true sourceAssetAndComponentMaterialsUntouched=true sourceMeshUntouched=true sourceTransformsAndCensusUntouched=true sourceSerializedCullLodAndWpoUntouched=true runtimeCullAndLodVisualOverrideOnly=true sourceCollisionOverlapNavigationRfUntouched=true sourceSimulationIsolationUntouched=true sourceDistanceFieldLightingStateUntouched=true fullVisibilityMaterialCullLodAndTagSnapshot=true exactFailureSuspendAndEndPlayRestore=true cleanColdTickRetry=true partialStateNeverAppliedOver=true foreignTagNeverRemovedByNonOwner=true v5bRuntimeReapplySuspendColdResumeFresh=true v5bMutationFailureColdAndTagFree=true dormantOverrideSlotsPreserved=true");
    return true;
}

int32 ATRIADIstanaExploreV5DGroundVegetationActor::
    FindLocalRuntimeSourceV5BTurfClaims(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        const ATRIADIstanaExploreV5DGroundVegetationActor* ExcludedOwner,
        ATRIADIstanaExploreV5DGroundVegetationActor*& OutSoleClaim)
{
    OutSoleClaim = nullptr;
    if (!Candidate || !Candidate->GetWorld())
    {
        return 0;
    }

    int32 ClaimCount = 0;
    for (TActorIterator<ATRIADIstanaExploreV5DGroundVegetationActor> It(
             Candidate->GetWorld());
         It;
         ++It)
    {
        if (IsValid(*It) && *It != ExcludedOwner &&
            (It->V5BVisualActor == Candidate ||
             It->RuntimeOriginalSourceV5BTurfOwnerActor.Get() == Candidate) &&
            (It->bRuntimeSourceV5BTurfPresentationApplied ||
             It->bRuntimeSourceV5BTurfSnapshotValid))
        {
            ++ClaimCount;
            OutSoleClaim = *It;
        }
    }
    if (ClaimCount != 1)
    {
        OutSoleClaim = nullptr;
    }
    return ClaimCount;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateActiveRuntimeSourceTurfPresentationForSourceActor(
        const ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport)
{
    if (!Candidate || !Candidate->GetWorld() ||
        !Candidate->Tags.Contains(RuntimeSourceTurfPresentationTag()))
    {
        OutReport = TEXT("V5D active inherited-turf presentation source, world, or exact tag is absent.");
        return false;
    }
    ATRIADIstanaExploreV5DGroundVegetationActor* MutableMatch = nullptr;
    const int32 MatchCount = FindLocalRuntimeSourceV5BTurfClaims(
        Candidate,
        nullptr,
        MutableMatch);
    const ATRIADIstanaExploreV5DGroundVegetationActor* Match = MutableMatch;
    if (MatchCount != 1 || !Match)
    {
        OutReport = FString::Printf(
            TEXT("V5D expected exactly one active inherited-turf presentation owner for the tagged V5B source; found %d."),
            MatchCount);
        return false;
    }
    return Match->ValidateAppliedRuntimeSourceTurfPresentationForSourceActor(
        Candidate,
        OutReport);
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    SuspendActiveRuntimeSourceTurfPresentationForV5BReapply(
        ATRIADIstanaExploreV5BVisualActor* Candidate,
        ATRIADIstanaExploreV5DGroundVegetationActor*& OutSuspendedOwner,
        FString& OutReport)
{
    OutSuspendedOwner = nullptr;
    if (!Candidate || !Candidate->GetWorld())
    {
        OutReport = TEXT("V5D pre-reapply suspension requires an exact V5B source and world.");
        return false;
    }
    ATRIADIstanaExploreV5DGroundVegetationActor* Match = nullptr;
    const int32 MatchCount = FindLocalRuntimeSourceV5BTurfClaims(
        Candidate,
        nullptr,
        Match);
    if (!Candidate->Tags.Contains(RuntimeSourceTurfPresentationTag()) &&
        MatchCount == 0)
    {
        OutReport.Reset();
        return true;
    }
    if (!Candidate->Tags.Contains(RuntimeSourceTurfPresentationTag()))
    {
        OutReport = FString::Printf(
            TEXT("V5D pre-reapply suspension found %d retained local ownership claim(s) despite an absent source tag; V5B mutation is refused."),
            MatchCount);
        return false;
    }
    if (MatchCount != 1 || !Match ||
        !Match->bRuntimeSourceV5BTurfPresentationApplied ||
        !Match->bRuntimeSourceV5BTurfSnapshotValid ||
        Match->RuntimeOriginalSourceV5BTurfOwnerActor.Get() != Candidate)
    {
        OutReport = FString::Printf(
            TEXT("V5D pre-reapply suspension expected one exact active snapshot owner; found %d. No source tag or material was touched."),
            MatchCount);
        return false;
    }

    FString ActivePresentationReport;
    if (!Match->ValidateAppliedRuntimeSourceTurfPresentationForSourceActor(
            Candidate,
            ActivePresentationReport))
    {
        FString RestoreError;
        const bool bRestored =
            Match->RestoreRuntimeSourceV5BTurfPresentation(RestoreError);
        OutReport = TEXT("V5D pre-reapply suspension rejected a drifted active presentation and failed closed: ") +
            ActivePresentationReport + TEXT(" restore=") +
            (bRestored ? FString(TEXT("exact")) : RestoreError);
        return false;
    }

    FString RestoreError;
    if (!Match->RestoreRuntimeSourceV5BTurfPresentation(RestoreError))
    {
        OutReport = TEXT("V5D pre-reapply suspension could not prove an exact cold source-state restore: ") +
            RestoreError;
        return false;
    }
    OutSuspendedOwner = Match;
    OutReport.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply(
        ATRIADIstanaExploreV5DGroundVegetationActor* SuspendedOwner,
        ATRIADIstanaExploreV5BVisualActor* Candidate,
        FString& OutReport)
{
    if (!SuspendedOwner)
    {
        OutReport.Reset();
        return true;
    }
    if (!IsValid(SuspendedOwner) || !Candidate ||
        SuspendedOwner->GetWorld() != Candidate->GetWorld() ||
        SuspendedOwner->V5BVisualActor != Candidate ||
        SuspendedOwner->bRuntimeSourceV5BTurfPresentationApplied ||
        SuspendedOwner->bRuntimeSourceV5BTurfSnapshotValid ||
        SuspendedOwner->RuntimeOriginalSourceV5BTurfOwnerActor.IsValid() ||
        SuspendedOwner->RuntimeOriginalSourceV5BTurfComponent.IsValid() ||
        SuspendedOwner->bRuntimeOriginalSourceV5BTurfHadPresentationTag ||
        Candidate->Tags.Contains(RuntimeSourceTurfPresentationTag()))
    {
        OutReport = TEXT("V5D post-reapply resume requires the exact suspended owner and a clean cold tag-free V5B source.");
        return false;
    }

    FString ApplyError;
    if (!SuspendedOwner->ApplyRuntimeSourceV5BTurfPresentation(ApplyError))
    {
        OutReport = TEXT("V5D post-reapply resume could not take a fresh exact cold baseline: ") +
            ApplyError;
        return false;
    }

    FString TerminalV5BReport;
    if (!Candidate->ValidateExploreV5BVisuals(TerminalV5BReport))
    {
        FString RestoreError;
        const bool bRestored =
            SuspendedOwner->RestoreRuntimeSourceV5BTurfPresentation(
                RestoreError);
        OutReport = TEXT("V5D post-reapply terminal V5B admission failed closed: ") +
            TerminalV5BReport + TEXT(" restore=") +
            (bRestored ? FString(TEXT("exact")) : RestoreError);
        return false;
    }
    OutReport.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateRuntimeSourceV5BTurfPresentation(FString& OutError) const
{
    return ValidateAppliedRuntimeSourceTurfPresentationForSourceActor(
        V5BVisualActor,
        OutError);
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    EnsureRuntimeSourceV5BTurfPresentation(FString& OutError)
{
    const bool bSourceHasPresentationTag = V5BVisualActor &&
        V5BVisualActor->Tags.Contains(RuntimeSourceTurfPresentationTag());
    const bool bCompleteRuntimeState =
        bRuntimeSourceV5BTurfPresentationApplied &&
        bRuntimeSourceV5BTurfSnapshotValid &&
        bSourceHasPresentationTag;
    if (bCompleteRuntimeState)
    {
        return ValidateRuntimeSourceV5BTurfPresentation(OutError);
    }

    const bool bHasLocalPartialRuntimeState =
        bRuntimeSourceV5BTurfPresentationApplied ||
        bRuntimeSourceV5BTurfSnapshotValid ||
        bRuntimeOriginalSourceV5BTurfHadPresentationTag;
    if (!bHasLocalPartialRuntimeState && bSourceHasPresentationTag)
    {
        OutError = TEXT("V5D inherited-turf retry found a foreign presentation tag without a local snapshot; this actor will not remove or apply over it.");
        return false;
    }
    if (bHasLocalPartialRuntimeState)
    {
        FString RestoreError;
        if (!RestoreRuntimeSourceV5BTurfPresentation(RestoreError))
        {
            OutError = TEXT("V5D inherited-turf retry refused to apply over partial or drifted runtime state: ") +
                RestoreError;
            return false;
        }
    }

    // Apply performs the complete cold V5B validation and independently
    // requires both actors to have begun play. A pre-V5B BeginPlay miss thus
    // remains fail-closed for this frame and is safely retried by Tick.
    return ApplyRuntimeSourceV5BTurfPresentation(OutError);
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ApplyRuntimeSourceV5BTurfPresentation(FString& OutError)
{
    ATRIADIstanaExploreV5DGroundVegetationActor* OtherClaim = nullptr;
    const int32 OtherClaimCount = FindLocalRuntimeSourceV5BTurfClaims(
        V5BVisualActor,
        this,
        OtherClaim);
    if (!bConfigured || bRuntimeSourceV5BTurfPresentationApplied ||
        bRuntimeSourceV5BTurfSnapshotValid || !HasActorBegunPlay() ||
        RuntimeOriginalSourceV5BTurfOwnerActor.IsValid() ||
        RuntimeOriginalSourceV5BTurfComponent.IsValid() ||
        bRuntimeOriginalSourceV5BTurfHadPresentationTag ||
        OtherClaimCount != 0 ||
        !V5BVisualActor ||
        V5BVisualActor->GetClass() !=
            ATRIADIstanaExploreV5BVisualActor::StaticClass() ||
        V5BVisualActor->GetWorld() != GetWorld() ||
        !V5BVisualActor->HasActorBegunPlay() ||
        V5BVisualActor->Tags.Contains(RuntimeSourceTurfPresentationTag()))
    {
        OutError = FString::Printf(
            TEXT("V5D inherited-turf presentation requires one fresh begun exact V5B source and no existing tag, local snapshot payload, or other ownership claim; otherClaims=%d."),
            OtherClaimCount);
        return false;
    }

    FString AssetError;
    if (!ValidateAssetRoster(SavedAssets, AssetError))
    {
        OutError = TEXT("V5D inherited-turf presentation requires its exact material/mesh roster: ") +
            AssetError;
        return false;
    }

    FString V5BReport;
    if (!V5BVisualActor->ValidateExploreV5BVisuals(V5BReport))
    {
        OutError = TEXT("V5D inherited-turf presentation refused an invalid cold V5B source: ") +
            V5BReport;
        return false;
    }

    UHierarchicalInstancedStaticMeshComponent* Component =
        V5BVisualActor->AccentTurfInstances.Get();
    int32 CullStart = 0;
    int32 CullEnd = 0;
    if (Component)
    {
        Component->GetCullDistances(CullStart, CullEnd);
    }
    TArray<FTransform> SourceTransforms;
    SourceTransforms.Reserve(SourceGrassCount);
    FString TransformError;
    if (!Component || Component->GetOwner() != V5BVisualActor ||
        Component->GetFName() != FName(TEXT("V5BAccentTurfInstances")) ||
        Component->GetAttachParent() != V5BVisualActor->SceneRoot ||
        !Component->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        Component->GetStaticMesh() != SavedAssets.GrassMicroDetailMesh ||
        Component->GetStaticMesh() !=
            V5BVisualActor->SavedAssetRoster.AccentTurfMesh ||
        Component->GetNumMaterials() != 1 ||
        Component->GetMaterial(0) !=
            V5BVisualActor->SavedAssetRoster.AccentTurfMaterial ||
        Component->GetInstanceCount() != SourceGrassCount ||
        CullStart != SourceV5BTurfCullStartDistanceCm ||
        CullEnd != SourceV5BTurfCullEndDistanceCm ||
        Component->WorldPositionOffsetDisableDistance !=
            SourceV5BTurfWpoDisableDistanceCm ||
        Component->Mobility != EComponentMobility::Static ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        !HasOnlyIgnoredCollisionResponses(Component) ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation() ||
        !Component->IsVisible() || Component->bHiddenInGame ||
        Component->CastShadow || Component->bCastContactShadow ||
        Component->bEnableDensityScaling ||
        !FMath::IsNearlyEqual(
            Component->CurrentDensityScaling,
            1.0f,
            0.0001f) ||
        !Component->bAutoRebuildTreeOnInstanceChanges ||
        Component->ForcedLodModel != 0 ||
        !Component->bOverrideMinLOD || Component->MinLOD != 0 ||
        !FMath::IsNearlyEqual(
            Component->InstanceLODDistanceScale,
            1.0f,
            0.0001f) ||
        Component->NumCustomDataFloats != 0 ||
        !Component->PerInstanceSMCustomData.IsEmpty() ||
        Component->IsAsyncBuilding() || !Component->IsTreeFullyBuilt() ||
        !AppendWorldTransforms(Component, SourceTransforms, TransformError) ||
        SourceTransforms.Num() != SourceGrassCount)
    {
        OutError = TransformError.IsEmpty()
            ? TEXT("V5D inherited-turf presentation requires the exact visible 18,432-instance V5B mesh/census/cull/WPO and render-only simulation isolation.")
            : TransformError;
        return false;
    }

    RuntimeOriginalSourceV5BTurfOwnerActor = V5BVisualActor;
    RuntimeOriginalSourceV5BTurfComponent = Component;
    RuntimeOriginalSourceV5BTurfOverrideMaterials =
        Component->OverrideMaterials;
    RuntimeOriginalSourceV5BTurfResolvedMaterial = Component->GetMaterial(0);
    RuntimeOriginalSourceV5BTurfMesh = Component->GetStaticMesh();
    RuntimeOriginalSourceV5BTurfWorldTransforms = MoveTemp(SourceTransforms);
    RuntimeOriginalSourceV5BTurfComponentTransform =
        Component->GetComponentTransform();
    RuntimeOriginalSourceV5BTurfInstanceCount = Component->GetInstanceCount();
    RuntimeOriginalSourceV5BTurfCullStartDistanceCm = CullStart;
    RuntimeOriginalSourceV5BTurfCullEndDistanceCm = CullEnd;
    RuntimeOriginalSourceV5BTurfWpoDisableDistanceCm =
        Component->WorldPositionOffsetDisableDistance;
    RuntimeOriginalSourceV5BTurfMobility =
        static_cast<uint8>(Component->Mobility);
    RuntimeOriginalSourceV5BTurfCollisionEnabled =
        static_cast<uint8>(Component->GetCollisionEnabled());
    RuntimeOriginalSourceV5BTurfCollisionResponses.Reset();
    RuntimeOriginalSourceV5BTurfCollisionResponses.Reserve(
        CollisionResponseChannelCount);
    for (int32 ChannelIndex = 0;
         ChannelIndex < CollisionResponseChannelCount;
         ++ChannelIndex)
    {
        RuntimeOriginalSourceV5BTurfCollisionResponses.Add(
            static_cast<uint8>(Component->GetCollisionResponseToChannel(
                static_cast<ECollisionChannel>(ChannelIndex))));
    }
    RuntimeOriginalSourceV5BTurfForcedLodModel = Component->ForcedLodModel;
    bRuntimeOriginalSourceV5BTurfOverrideMinLod = Component->bOverrideMinLOD;
    RuntimeOriginalSourceV5BTurfMinLod = Component->MinLOD;
    RuntimeOriginalSourceV5BTurfLodDistanceScale =
        Component->InstanceLODDistanceScale;
    RuntimeOriginalSourceV5BTurfNumCustomDataFloats =
        Component->NumCustomDataFloats;
    RuntimeOriginalSourceV5BTurfCustomData = Component->PerInstanceSMCustomData;
    bRuntimeOriginalSourceV5BTurfVisible = Component->IsVisible();
    bRuntimeOriginalSourceV5BTurfHiddenInGame = Component->bHiddenInGame;
    bRuntimeOriginalSourceV5BTurfHadPresentationTag =
        V5BVisualActor->Tags.Contains(RuntimeSourceTurfPresentationTag());
    bRuntimeOriginalSourceV5BTurfCastShadow = Component->CastShadow;
    bRuntimeOriginalSourceV5BTurfCastContactShadow =
        Component->bCastContactShadow;
    bRuntimeOriginalSourceV5BTurfAffectDistanceFieldLighting =
        Component->bAffectDistanceFieldLighting;
    bRuntimeOriginalSourceV5BTurfGenerateOverlapEvents =
        Component->GetGenerateOverlapEvents();
    bRuntimeOriginalSourceV5BTurfCanEverAffectNavigation =
        Component->CanEverAffectNavigation();
    bRuntimeOriginalSourceV5BTurfEnableDensityScaling =
        Component->bEnableDensityScaling;
    RuntimeOriginalSourceV5BTurfCurrentDensityScaling =
        Component->CurrentDensityScaling;
    bRuntimeOriginalSourceV5BTurfAutoRebuildTree =
        Component->bAutoRebuildTreeOnInstanceChanges;
    bRuntimeSourceV5BTurfSnapshotValid = true;

    Component->SetCullDistances(
        CompositeSourceV5BTurfCullStartDistanceCm,
        CompositeSourceV5BTurfCullEndDistanceCm);
    Component->InstanceLODDistanceScale =
        CompositeSourceV5BTurfLodDistanceScale;
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false, true);
    Component->MarkRenderStateDirty();
    V5BVisualActor->Tags.AddUnique(RuntimeSourceTurfPresentationTag());
    bRuntimeSourceV5BTurfPresentationApplied = true;

    FString PresentationReport;
    if (!ValidateRuntimeSourceV5BTurfPresentation(PresentationReport))
    {
        FString RestoreError;
        const bool bRestored =
            RestoreRuntimeSourceV5BTurfPresentation(RestoreError);
        OutError = TEXT("V5D inherited-turf presentation failed closed validation: ") +
            PresentationReport + TEXT(" restore=") +
            (bRestored ? FString(TEXT("exact")) : RestoreError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    RestoreRuntimeSourceV5BTurfPresentation(FString& OutError)
{
    const bool bHadLocalRuntimeState =
        bRuntimeSourceV5BTurfPresentationApplied ||
        bRuntimeSourceV5BTurfSnapshotValid;
    if (!bRuntimeSourceV5BTurfSnapshotValid)
    {
        // A shared source tag is not proof that this actor owns it. Without
        // this actor's transient full-array snapshot, touching either the tag
        // or component could corrupt another exact V5D owner's presentation.
        bRuntimeSourceV5BTurfPresentationApplied = false;
        RuntimeOriginalSourceV5BTurfOwnerActor.Reset();
        RuntimeOriginalSourceV5BTurfComponent.Reset();
        bRuntimeOriginalSourceV5BTurfHadPresentationTag = false;
        if (bHadLocalRuntimeState)
        {
            OutError = TEXT("V5D inherited-turf presentation had runtime state without its full restoration snapshot.");
        }
        else
        {
            OutError.Reset();
        }
        return !bHadLocalRuntimeState;
    }

    ATRIADIstanaExploreV5BVisualActor* SnapshottedSourceActor =
        RuntimeOriginalSourceV5BTurfOwnerActor.Get();
    UHierarchicalInstancedStaticMeshComponent* Component =
        RuntimeOriginalSourceV5BTurfComponent.Get();
    ATRIADIstanaExploreV5DGroundVegetationActor* OtherLocalOwner = nullptr;
    const int32 OtherLocalOwnerCount =
        FindLocalRuntimeSourceV5BTurfClaims(
            SnapshottedSourceActor,
            this,
            OtherLocalOwner);
    if (OtherLocalOwnerCount != 0)
    {
        bRuntimeSourceV5BTurfPresentationApplied = false;
        OutError = FString::Printf(
            TEXT("V5D inherited-turf restore refused to mutate a source claimed by %d other local runtime owner(s)."),
            OtherLocalOwnerCount);
        return false;
    }
    if (!SnapshottedSourceActor || !Component ||
        Component->GetOwner() != SnapshottedSourceActor)
    {
        if (SnapshottedSourceActor)
        {
            if (bRuntimeOriginalSourceV5BTurfHadPresentationTag)
            {
                SnapshottedSourceActor->Tags.AddUnique(
                    RuntimeSourceTurfPresentationTag());
            }
            else
            {
                SnapshottedSourceActor->Tags.Remove(
                    RuntimeSourceTurfPresentationTag());
            }
        }
        bRuntimeSourceV5BTurfPresentationApplied = false;
        OutError = TEXT("V5D could not restore the exact inherited-turf source actor/component objects captured before presentation apply.");
        return false;
    }

    Component->EmptyOverrideMaterials();
    for (int32 Index = 0;
         Index < RuntimeOriginalSourceV5BTurfOverrideMaterials.Num();
         ++Index)
    {
        Component->SetMaterial(
            Index,
            RuntimeOriginalSourceV5BTurfOverrideMaterials[Index].Get());
    }
    Component->SetVisibility(bRuntimeOriginalSourceV5BTurfVisible, true);
    Component->SetHiddenInGame(
        bRuntimeOriginalSourceV5BTurfHiddenInGame,
        true);
    Component->SetCullDistances(
        RuntimeOriginalSourceV5BTurfCullStartDistanceCm,
        RuntimeOriginalSourceV5BTurfCullEndDistanceCm);
    Component->InstanceLODDistanceScale =
        RuntimeOriginalSourceV5BTurfLodDistanceScale;
    Component->MarkRenderStateDirty();
    if (bRuntimeOriginalSourceV5BTurfHadPresentationTag)
    {
        SnapshottedSourceActor->Tags.AddUnique(
            RuntimeSourceTurfPresentationTag());
    }
    else
    {
        SnapshottedSourceActor->Tags.Remove(
            RuntimeSourceTurfPresentationTag());
    }
    bRuntimeSourceV5BTurfPresentationApplied = false;

    int32 CurrentCullStart = 0;
    int32 CurrentCullEnd = 0;
    Component->GetCullDistances(CurrentCullStart, CurrentCullEnd);
    TArray<FTransform> CurrentTransforms;
    CurrentTransforms.Reserve(SourceGrassCount);
    FString TransformError;
    const bool bTransformsReadable = AppendWorldTransforms(
        Component,
        CurrentTransforms,
        TransformError);
    bool bCollisionResponsesExact =
        RuntimeOriginalSourceV5BTurfCollisionResponses.Num() ==
            CollisionResponseChannelCount;
    for (int32 ChannelIndex = 0;
         bCollisionResponsesExact &&
             ChannelIndex < RuntimeOriginalSourceV5BTurfCollisionResponses.Num();
         ++ChannelIndex)
    {
        bCollisionResponsesExact = static_cast<uint8>(
            Component->GetCollisionResponseToChannel(
                static_cast<ECollisionChannel>(ChannelIndex))) ==
            RuntimeOriginalSourceV5BTurfCollisionResponses[ChannelIndex];
    }
    const bool bExactRestore =
        OverrideMaterialArraysEqual(
            Component->OverrideMaterials,
            RuntimeOriginalSourceV5BTurfOverrideMaterials) &&
        Component->GetMaterial(0) ==
            RuntimeOriginalSourceV5BTurfResolvedMaterial &&
        Component->GetStaticMesh() == RuntimeOriginalSourceV5BTurfMesh &&
        Component->GetInstanceCount() ==
            RuntimeOriginalSourceV5BTurfInstanceCount &&
        bTransformsReadable &&
        TransformArraysEqual(
            CurrentTransforms,
            RuntimeOriginalSourceV5BTurfWorldTransforms) &&
        Component->GetComponentTransform().Equals(
            RuntimeOriginalSourceV5BTurfComponentTransform,
            0.0001) &&
        CurrentCullStart == RuntimeOriginalSourceV5BTurfCullStartDistanceCm &&
        CurrentCullEnd == RuntimeOriginalSourceV5BTurfCullEndDistanceCm &&
        Component->WorldPositionOffsetDisableDistance ==
            RuntimeOriginalSourceV5BTurfWpoDisableDistanceCm &&
        static_cast<uint8>(Component->Mobility) ==
            RuntimeOriginalSourceV5BTurfMobility &&
        static_cast<uint8>(Component->GetCollisionEnabled()) ==
            RuntimeOriginalSourceV5BTurfCollisionEnabled &&
        bCollisionResponsesExact &&
        Component->ForcedLodModel ==
            RuntimeOriginalSourceV5BTurfForcedLodModel &&
        Component->bOverrideMinLOD ==
            bRuntimeOriginalSourceV5BTurfOverrideMinLod &&
        Component->MinLOD == RuntimeOriginalSourceV5BTurfMinLod &&
        FMath::IsNearlyEqual(
            Component->InstanceLODDistanceScale,
            RuntimeOriginalSourceV5BTurfLodDistanceScale,
            0.0001f) &&
        Component->NumCustomDataFloats ==
            RuntimeOriginalSourceV5BTurfNumCustomDataFloats &&
        Component->PerInstanceSMCustomData ==
            RuntimeOriginalSourceV5BTurfCustomData &&
        Component->IsVisible() == bRuntimeOriginalSourceV5BTurfVisible &&
        Component->bHiddenInGame ==
            bRuntimeOriginalSourceV5BTurfHiddenInGame &&
        Component->CastShadow == bRuntimeOriginalSourceV5BTurfCastShadow &&
        Component->bCastContactShadow ==
            bRuntimeOriginalSourceV5BTurfCastContactShadow &&
        Component->bAffectDistanceFieldLighting ==
            bRuntimeOriginalSourceV5BTurfAffectDistanceFieldLighting &&
        Component->GetGenerateOverlapEvents() ==
            bRuntimeOriginalSourceV5BTurfGenerateOverlapEvents &&
        Component->CanEverAffectNavigation() ==
            bRuntimeOriginalSourceV5BTurfCanEverAffectNavigation &&
        Component->bEnableDensityScaling ==
            bRuntimeOriginalSourceV5BTurfEnableDensityScaling &&
        FMath::IsNearlyEqual(
            Component->CurrentDensityScaling,
            RuntimeOriginalSourceV5BTurfCurrentDensityScaling,
            0.0001f) &&
        Component->bAutoRebuildTreeOnInstanceChanges ==
            bRuntimeOriginalSourceV5BTurfAutoRebuildTree &&
        SnapshottedSourceActor->Tags.Contains(
            RuntimeSourceTurfPresentationTag()) ==
            bRuntimeOriginalSourceV5BTurfHadPresentationTag;
    if (!bExactRestore)
    {
        OutError = TransformError.IsEmpty()
            ? TEXT("V5D restored the inherited-turf override array but exact source-state readback drifted.")
            : TransformError;
        return false;
    }

    RuntimeOriginalSourceV5BTurfOverrideMaterials.Reset();
    RuntimeOriginalSourceV5BTurfOwnerActor.Reset();
    RuntimeOriginalSourceV5BTurfComponent.Reset();
    RuntimeOriginalSourceV5BTurfResolvedMaterial = nullptr;
    RuntimeOriginalSourceV5BTurfMesh = nullptr;
    RuntimeOriginalSourceV5BTurfWorldTransforms.Reset();
    RuntimeOriginalSourceV5BTurfComponentTransform = FTransform::Identity;
    RuntimeOriginalSourceV5BTurfInstanceCount = 0;
    RuntimeOriginalSourceV5BTurfCullStartDistanceCm = 0;
    RuntimeOriginalSourceV5BTurfCullEndDistanceCm = 0;
    RuntimeOriginalSourceV5BTurfWpoDisableDistanceCm = 0;
    RuntimeOriginalSourceV5BTurfMobility = 0;
    RuntimeOriginalSourceV5BTurfCollisionEnabled = 0;
    RuntimeOriginalSourceV5BTurfCollisionResponses.Reset();
    RuntimeOriginalSourceV5BTurfForcedLodModel = 0;
    bRuntimeOriginalSourceV5BTurfOverrideMinLod = false;
    RuntimeOriginalSourceV5BTurfMinLod = 0;
    RuntimeOriginalSourceV5BTurfLodDistanceScale = 1.0f;
    RuntimeOriginalSourceV5BTurfNumCustomDataFloats = 0;
    RuntimeOriginalSourceV5BTurfCustomData.Reset();
    bRuntimeOriginalSourceV5BTurfVisible = true;
    bRuntimeOriginalSourceV5BTurfHiddenInGame = false;
    bRuntimeOriginalSourceV5BTurfHadPresentationTag = false;
    bRuntimeOriginalSourceV5BTurfCastShadow = false;
    bRuntimeOriginalSourceV5BTurfCastContactShadow = false;
    bRuntimeOriginalSourceV5BTurfAffectDistanceFieldLighting = false;
    bRuntimeOriginalSourceV5BTurfGenerateOverlapEvents = false;
    bRuntimeOriginalSourceV5BTurfCanEverAffectNavigation = false;
    bRuntimeOriginalSourceV5BTurfEnableDensityScaling = false;
    RuntimeOriginalSourceV5BTurfCurrentDensityScaling = 1.0f;
    bRuntimeOriginalSourceV5BTurfAutoRebuildTree = false;
    bRuntimeSourceV5BTurfSnapshotValid = false;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    EnsureRuntimeEdgeGrassFadePresentation(FString& OutError)
{
    const bool bCompleteRuntimeState =
        bRuntimeEdgeGrassFadePresentationApplied &&
        bRuntimeEdgeGrassFadeSnapshotValid;
    if (bCompleteRuntimeState)
    {
        return ValidateRuntimeEdgeGrassFadePresentation(OutError);
    }

    const bool bHasPartialRuntimeState =
        bRuntimeEdgeGrassFadePresentationApplied ||
        bRuntimeEdgeGrassFadeSnapshotValid;
    if (bHasPartialRuntimeState)
    {
        FString RestoreError;
        if (!RestoreRuntimeEdgeGrassFadePresentation(RestoreError))
        {
            OutError = TEXT("V5D edge-grass fade retry refused to apply over partial or drifted runtime state: ") +
                RestoreError;
            return false;
        }
    }
    return ApplyRuntimeEdgeGrassFadePresentation(OutError);
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ApplyRuntimeEdgeGrassFadePresentation(FString& OutError)
{
    if (!bConfigured || !HasActorBegunPlay() ||
        bRuntimeEdgeGrassFadePresentationApplied ||
        bRuntimeEdgeGrassFadeSnapshotValid ||
        RuntimeOriginalEdgeGrassComponent.IsValid() ||
        !EdgeGrassInstances || EdgeGrassInstances->GetOwner() != this ||
        EdgeGrassInstances->GetFName() !=
            FName(TEXT("V5DEdgeGrassSeasonalAccents")))
    {
        OutError = TEXT("V5D edge-grass fade requires one fresh begun configured owner/component and no retained runtime snapshot.");
        return false;
    }

    FString AssetError;
    if (!ValidateAssetRoster(SavedAssets, AssetError))
    {
        OutError = TEXT("V5D edge-grass fade requires the exact admitted cold asset roster: ") +
            AssetError;
        return false;
    }

    UMaterialInterface* FadeMaterial = EdgeGrassFadeMaterialCookReference;
    if (!FadeMaterial ||
        FadeMaterial->GetPathName() != EdgeGrassFadeMaterialPath)
    {
        // The native CDO ConstructorHelpers reference is the cook dependency.
        // This exact-path load is only a same-editor-session fallback when the
        // R11 package was created after this class's CDO was constructed.
        FadeMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            *EdgeGrassFadeMaterialPath);
        if (FadeMaterial &&
            FadeMaterial->GetPathName() == EdgeGrassFadeMaterialPath)
        {
            EdgeGrassFadeMaterialCookReference = FadeMaterial;
        }
    }

    int32 CullStart = 0;
    int32 CullEnd = 0;
    EdgeGrassInstances->GetCullDistances(CullStart, CullEnd);
    TArray<FTransform> CurrentTransforms;
    CurrentTransforms.Reserve(SavedEdgeGrass.Num());
    FString TransformError;
    if (!FadeMaterial ||
        FadeMaterial->GetPathName() != EdgeGrassFadeMaterialPath ||
        EdgeGrassInstances->GetStaticMesh() != SavedAssets.EdgeGrassMesh ||
        EdgeGrassInstances->GetMaterial(0) != SavedAssets.EdgeGrassMaterial ||
        EdgeGrassInstances->GetInstanceCount() != SavedEdgeGrass.Num() ||
        CullStart != EdgeGrassCullStartDistanceCm ||
        CullEnd != EdgeGrassCullEndDistanceCm ||
        EdgeGrassInstances->WorldPositionOffsetDisableDistance !=
            EdgeGrassWpoDisableDistanceCm ||
        EdgeGrassInstances->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        !HasOnlyIgnoredCollisionResponses(EdgeGrassInstances) ||
        EdgeGrassInstances->GetGenerateOverlapEvents() ||
        EdgeGrassInstances->CanEverAffectNavigation() ||
        EdgeGrassInstances->bEnableDensityScaling ||
        !AppendWorldTransforms(
            EdgeGrassInstances,
            CurrentTransforms,
            TransformError) ||
        !TransformArraysEqual(CurrentTransforms, SavedEdgeGrass))
    {
        OutError = TransformError.IsEmpty()
            ? TEXT("V5D edge-grass fade requires the exact cold mesh/material/transforms/census/cull/WPO/collision/navigation state and the cooked R11 derivative.")
            : TransformError;
        return false;
    }

    RuntimeOriginalEdgeGrassComponent = EdgeGrassInstances;
    RuntimeOriginalEdgeGrassOverrideMaterials =
        EdgeGrassInstances->OverrideMaterials;
    RuntimeOriginalEdgeGrassResolvedMaterial =
        EdgeGrassInstances->GetMaterial(0);
    bRuntimeEdgeGrassFadeSnapshotValid = true;

    EdgeGrassInstances->SetMaterial(0, FadeMaterial);
    EdgeGrassInstances->MarkRenderStateDirty();
    bRuntimeEdgeGrassFadePresentationApplied = true;

    FString PresentationError;
    if (!ValidateRuntimeEdgeGrassFadePresentation(PresentationError))
    {
        FString RestoreError;
        const bool bRestored =
            RestoreRuntimeEdgeGrassFadePresentation(RestoreError);
        OutError = TEXT("V5D edge-grass fade failed closed validation: ") +
            PresentationError + TEXT(" restore=") +
            (bRestored ? FString(TEXT("exact")) : RestoreError);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateRuntimeEdgeGrassFadePresentation(FString& OutError) const
{
    const UHierarchicalInstancedStaticMeshComponent* Component =
        RuntimeOriginalEdgeGrassComponent.Get();
    int32 CullStart = 0;
    int32 CullEnd = 0;
    if (Component)
    {
        Component->GetCullDistances(CullStart, CullEnd);
    }
    TArray<FTransform> CurrentTransforms;
    CurrentTransforms.Reserve(SavedEdgeGrass.Num());
    FString TransformError;
    if (!bConfigured || !HasActorBegunPlay() ||
        !bRuntimeEdgeGrassFadePresentationApplied ||
        !bRuntimeEdgeGrassFadeSnapshotValid ||
        !Component || Component != EdgeGrassInstances ||
        Component->GetOwner() != this ||
        Component->GetFName() !=
            FName(TEXT("V5DEdgeGrassSeasonalAccents")) ||
        !EdgeGrassFadeMaterialCookReference ||
        EdgeGrassFadeMaterialCookReference->GetPathName() !=
            EdgeGrassFadeMaterialPath ||
        !RuntimeOriginalEdgeGrassResolvedMaterial ||
        RuntimeOriginalEdgeGrassResolvedMaterial !=
            SavedAssets.EdgeGrassMaterial ||
        Component->GetStaticMesh() != SavedAssets.EdgeGrassMesh ||
        Component->GetMaterial(0) !=
            EdgeGrassFadeMaterialCookReference ||
        !HasExactPresentedOverrideArray(
            Component,
            RuntimeOriginalEdgeGrassOverrideMaterials,
            EdgeGrassFadeMaterialCookReference) ||
        Component->GetInstanceCount() != SavedEdgeGrass.Num() ||
        CullStart != EdgeGrassCullStartDistanceCm ||
        CullEnd != EdgeGrassCullEndDistanceCm ||
        Component->WorldPositionOffsetDisableDistance !=
            EdgeGrassWpoDisableDistanceCm ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        !HasOnlyIgnoredCollisionResponses(Component) ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation() ||
        Component->bEnableDensityScaling ||
        !AppendWorldTransforms(Component, CurrentTransforms, TransformError) ||
        !TransformArraysEqual(CurrentTransforms, SavedEdgeGrass))
    {
        OutError = TransformError.IsEmpty()
            ? TEXT("V5D edge grass changed outside its single transient slot-0 fade presentation or lost its exact restoration snapshot.")
            : TransformError;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    RestoreRuntimeEdgeGrassFadePresentation(FString& OutError)
{
    const bool bHadRuntimeState =
        bRuntimeEdgeGrassFadePresentationApplied ||
        bRuntimeEdgeGrassFadeSnapshotValid;
    if (!bRuntimeEdgeGrassFadeSnapshotValid)
    {
        bRuntimeEdgeGrassFadePresentationApplied = false;
        RuntimeOriginalEdgeGrassComponent.Reset();
        if (bHadRuntimeState)
        {
            OutError = TEXT("V5D edge-grass fade had runtime state without its complete material-array restoration snapshot.");
        }
        else
        {
            OutError.Reset();
        }
        return !bHadRuntimeState;
    }

    UHierarchicalInstancedStaticMeshComponent* Component =
        RuntimeOriginalEdgeGrassComponent.Get();
    if (!Component || Component->GetOwner() != this ||
        Component->GetFName() !=
            FName(TEXT("V5DEdgeGrassSeasonalAccents")))
    {
        bRuntimeEdgeGrassFadePresentationApplied = false;
        OutError = TEXT("V5D edge-grass fade could not recover the exact owned component captured before the transient swap.");
        return false;
    }

    Component->EmptyOverrideMaterials();
    for (int32 Index = 0;
         Index < RuntimeOriginalEdgeGrassOverrideMaterials.Num();
         ++Index)
    {
        Component->SetMaterial(
            Index,
            RuntimeOriginalEdgeGrassOverrideMaterials[Index].Get());
    }
    Component->MarkRenderStateDirty();
    bRuntimeEdgeGrassFadePresentationApplied = false;

    int32 CullStart = 0;
    int32 CullEnd = 0;
    Component->GetCullDistances(CullStart, CullEnd);
    TArray<FTransform> CurrentTransforms;
    CurrentTransforms.Reserve(SavedEdgeGrass.Num());
    FString TransformError;
    const bool bTransformsReadable = AppendWorldTransforms(
        Component,
        CurrentTransforms,
        TransformError);
    const bool bExactRestore =
        OverrideMaterialArraysEqual(
            Component->OverrideMaterials,
            RuntimeOriginalEdgeGrassOverrideMaterials) &&
        Component->GetMaterial(0) ==
            RuntimeOriginalEdgeGrassResolvedMaterial &&
        Component->GetStaticMesh() == SavedAssets.EdgeGrassMesh &&
        Component->GetInstanceCount() == SavedEdgeGrass.Num() &&
        bTransformsReadable &&
        TransformArraysEqual(CurrentTransforms, SavedEdgeGrass) &&
        CullStart == EdgeGrassCullStartDistanceCm &&
        CullEnd == EdgeGrassCullEndDistanceCm &&
        Component->WorldPositionOffsetDisableDistance ==
            EdgeGrassWpoDisableDistanceCm &&
        Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        HasOnlyIgnoredCollisionResponses(Component) &&
        !Component->GetGenerateOverlapEvents() &&
        !Component->CanEverAffectNavigation() &&
        !Component->bEnableDensityScaling;
    if (!bExactRestore)
    {
        OutError = TransformError.IsEmpty()
            ? TEXT("V5D edge-grass fade restored its material array but could not prove the exact untouched mesh/transforms/census/cull/WPO/collision/navigation state.")
            : TransformError;
        return false;
    }

    RuntimeOriginalEdgeGrassOverrideMaterials.Reset();
    RuntimeOriginalEdgeGrassComponent.Reset();
    RuntimeOriginalEdgeGrassResolvedMaterial = nullptr;
    bRuntimeEdgeGrassFadeSnapshotValid = false;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::ResolveExactContextPolicy(
    ATRIADIstanaExploreV5DContextPolicyActor*& OutPolicy,
    FString& OutError) const
{
    OutPolicy = nullptr;
    UWorld* World = GetWorld();
    if (!World)
    {
        OutError = TEXT("V5D terrain presentation has no world.");
        return false;
    }

    int32 PolicyCount = 0;
    for (TActorIterator<ATRIADIstanaExploreV5DContextPolicyActor> It(World);
         It;
         ++It)
    {
        if (IsValid(*It))
        {
            OutPolicy = *It;
            ++PolicyCount;
        }
    }

    AActor* SiteClipActor = nullptr;
    int32 TaggedSiteClipCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (IsValid(*It) && It->Tags.Contains(HybridSiteClipTag))
        {
            SiteClipActor = *It;
            ++TaggedSiteClipCount;
        }
    }

    FString PolicyReport;
    if (PolicyCount != 1 || !OutPolicy ||
        !OutPolicy->Tags.Contains(HybridContextPolicyTag) ||
        !OutPolicy->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !OutPolicy->ProviderSiteClipCenterMeters.Equals(
            ExpectedProviderSiteClipCenterMeters(),
            0.000001) ||
        !OutPolicy->ProviderSiteClipSemiAxesMeters.Equals(
            ExpectedProviderSiteClipSemiAxesMeters(),
            0.000001) ||
        OutPolicy->ProviderSiteClipSplinePoints !=
            ExpectedProviderSiteClipSplinePoints() ||
        TaggedSiteClipCount != 1 || !SiteClipActor ||
        !SiteClipActor->GetActorTransform().Equals(
            FTransform::Identity,
            0.001) ||
        !OutPolicy->ValidateHybridContext(PolicyReport))
    {
        OutError = FString::Printf(
            TEXT("V5D terrain presentation requires one valid identity context policy and one identity 64-point compact irregular-ellipse site clip; policy=%d siteClip=%d %s"),
            PolicyCount,
            TaggedSiteClipCount,
            *PolicyReport);
        OutPolicy = nullptr;
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    RestoreSourceTerrainRendering(FString& OutError)
{
    UStaticMeshComponent* Terrain = PublicViewSceneActor
        ? PublicViewSceneActor->TerrainComponent.Get()
        : nullptr;
    if (!bSavedSourceTerrainVisible || bSavedSourceTerrainHiddenInGame ||
        !ApplySourceTerrainRendererVisibility(Terrain, true))
    {
        OutError = TEXT("V5D fail-closed terrain restore requires the saved source renderer to be visible and its QueryAndPhysics collision to remain live.");
        bSourceTerrainRendererHiddenForReadyProvider = false;
        bSourceTerrainRendererRestoredForFallback = false;
        return false;
    }
    bSourceTerrainRendererHiddenForReadyProvider = false;
    bSourceTerrainRendererRestoredForFallback = true;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    SynchronizeSourceTerrainRendererWithProviderPolicy(FString& OutError)
{
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    FString PolicyError;
    const bool bPolicyValid = ResolveExactContextPolicy(Policy, PolicyError);
    const bool bPolicyHasBegunPlay = bPolicyValid &&
        Policy && Policy->HasActorBegunPlay();

    if (Policy && RuntimeContextPolicyActor.Get() != Policy)
    {
        if (ATRIADIstanaExploreV5DContextPolicyActor* Previous =
                RuntimeContextPolicyActor.Get())
        {
            RemoveTickPrerequisiteActor(Previous);
        }
        RuntimeContextPolicyActor = Policy;
        AddTickPrerequisiteActor(Policy);
    }

    const bool bHideSourceTerrain = ShouldHideSourceTerrainRenderer(
        bPolicyValid,
        bPolicyHasBegunPlay,
        bPolicyValid && Policy &&
            (Policy->bR33DualCesiumContextConfigured
                 ? Policy->ShouldHideSourceTerrainRendererForVisualContext()
                 : Policy->bLocalBuildingFallbackCurrentlyHidden));
    if (!bPolicyValid || !bPolicyHasBegunPlay)
    {
        FString RestoreError;
        const bool bRestored = RestoreSourceTerrainRendering(RestoreError);
        bLastContextPolicyStateValid = false;
        OutError = PolicyError;
        if (!bPolicyHasBegunPlay && bPolicyValid)
        {
            OutError = TEXT("V5D context policy has not begun play; source terrain renderer remains fail-closed visible.");
        }
        if (!bRestored)
        {
            OutError += TEXT(" ") + RestoreError;
        }
        return false;
    }

    UStaticMeshComponent* Terrain = PublicViewSceneActor
        ? PublicViewSceneActor->TerrainComponent.Get()
        : nullptr;
    if (!ApplySourceTerrainRendererVisibility(
            Terrain,
            !bHideSourceTerrain))
    {
        FString RestoreError;
        RestoreSourceTerrainRendering(RestoreError);
        bLastContextPolicyStateValid = false;
        OutError = TEXT("V5D terrain renderer transition was refused because QueryAndPhysics collision or visibility readback drifted. ") +
            RestoreError;
        return false;
    }

    bSourceTerrainRendererHiddenForReadyProvider = bHideSourceTerrain;
    bSourceTerrainRendererRestoredForFallback = !bHideSourceTerrain;
    bLastContextPolicyStateValid = true;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateGroundVegetationRealism(FString& OutReport) const
{
    FString Error;
    if (!bConfigured || !GetWorld() ||
        GrassPresentationRevision != GrassPresentationRevisionR23 ||
        ClaimLabel != ExpectedClaimLabel() || !bAppearanceOnly ||
        bSourceTransformsModified || bCollisionOrNavigationAuthority ||
        bSensorOrRfMaterialAuthority || bSurveyOrAsBuiltClaimed ||
        bBotanicalInventoryClaimed || bCurrentSeasonOrWeatherClaimed ||
        bCesiumContentBakedCachedTracedOrAnalysedByGroundPass ||
        bGoogleOrOneMapContentUsed ||
        !GroundOverlayCoreCenterMeters.Equals(
            ExpectedProviderSiteClipCenterMeters(),
            0.000001) ||
        !GroundOverlayCoreSemiAxesMeters.Equals(
            ExpectedProviderSiteClipSemiAxesMeters(),
            0.000001) ||
        GroundOverlayClipSplinePoints !=
            ExpectedProviderSiteClipSplinePoints() ||
        !FMath::IsNearlyEqual(
            GroundOverlayOpaqueCollarMeters,
            RequiredGroundOverlayOpaqueCollarMeters,
            0.000001) ||
        !FMath::IsNearlyEqual(
            GroundOverlayOutwardFeatherMeters,
            RequiredGroundOverlayOutwardFeatherMeters,
            0.000001) ||
        !FMath::IsNearlyEqual(
            GroundOverlayDitherCellMeters,
            RequiredGroundOverlayDitherCellMeters,
            0.000001) ||
        !bGroundOverlayUsesStableWorldSpaceDitheredCoreMask ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidateAssetRoster(SavedAssets, Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("V5D ground/vegetation truth or identity state drifted.")
            : Error;
        return false;
    }

    EGrassMaterialRuntimeRevision GrassMaterialRuntimeRevision =
        EGrassMaterialRuntimeRevision::Unsupported;
    if (!ResolveGrassMaterialRuntimeRevision(
            SavedAssets.GrassProfileMaterials,
            GrassMaterialRuntimeRevision,
            Error))
    {
        OutReport = TEXT("V5D grass-material runtime revision validation failed: ") +
            Error;
        return false;
    }
    int32 GroundOverlayMaterialRevision = 0;
    int32 EdgeGrassMaterialRevision = 0;
    int32 GroundOverlayMaterialCalibrationRevision = 0;
    int32 EdgeGrassMaterialCalibrationRevision = 0;
    if (!ResolveOptionalRuntimeMaterialRevision(
            SavedAssets.GroundOverlayMaterial,
            GroundOverlayMaterialRevision,
            Error) ||
        !ResolveOptionalRuntimeMaterialRevision(
            EdgeGrassFadeMaterialCookReference,
            EdgeGrassMaterialRevision,
            Error) ||
        !ResolveOptionalRuntimeMaterialCalibrationRevision(
            SavedAssets.GroundOverlayMaterial,
            GroundOverlayMaterialCalibrationRevision,
            Error) ||
        !ResolveOptionalRuntimeMaterialCalibrationRevision(
            EdgeGrassFadeMaterialCookReference,
            EdgeGrassMaterialCalibrationRevision,
            Error))
    {
        OutReport = TEXT("V5D grass-system runtime revision validation failed: ") +
            Error;
        return false;
    }
    const int32 GrassProfileMaterialRevision =
        (GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R23 ||
         GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R23B)
        ? 23
        : (GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R22
               ? 22
               : 0);
    const int32 GrassProfileMaterialCalibrationRevision =
        GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R23B
        ? 1
        : 0;
    if (GroundOverlayMaterialRevision != EdgeGrassMaterialRevision ||
        GrassProfileMaterialRevision != GroundOverlayMaterialRevision ||
        GroundOverlayMaterialCalibrationRevision !=
            EdgeGrassMaterialCalibrationRevision ||
        GrassProfileMaterialCalibrationRevision !=
            GroundOverlayMaterialCalibrationRevision)
    {
        OutReport = TEXT("V5D grass-system runtime revision validation rejected a mixed blade/overlay/edge material roster.");
        return false;
    }

    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    if (!ResolveExactContextPolicy(Policy, Error))
    {
        OutReport = TEXT("V5D ground/provider seam policy is invalid: ") + Error;
        return false;
    }
    const bool bPolicyHasBegunPlay = Policy->HasActorBegunPlay();
    const bool bExpectedSourceTerrainHidden =
        ShouldHideSourceTerrainRenderer(
            true,
            HasActorBegunPlay() && bPolicyHasBegunPlay,
            Policy->bR33DualCesiumContextConfigured
                ? Policy->ShouldHideSourceTerrainRendererForVisualContext()
                : Policy->bLocalBuildingFallbackCurrentlyHidden);

    FVector ExpectedOverlayLocation =
        SavedSourceTerrainWorldTransform.GetTranslation();
    ExpectedOverlayLocation.Z += GroundOverlayOffsetCm;
    FTransform ExpectedOverlayTransform = SavedSourceTerrainWorldTransform;
    ExpectedOverlayTransform.SetTranslation(ExpectedOverlayLocation);
    if (!PublicViewSceneActor || !PublicViewSceneActor->TerrainComponent ||
        PublicViewSceneActor->GetWorld() != GetWorld() ||
        PublicViewSceneActor->TerrainComponent->GetStaticMesh() !=
            SavedAssets.GroundOverlayMesh ||
        PublicViewSceneActor->TerrainComponent->GetMaterial(0) !=
            SavedSourceTerrainMaterial ||
        !SavedSourceTerrainMaterial ||
        SavedSourceTerrainMaterial->GetPathName() !=
            SourceTerrainLawnMaterialPath ||
        !PublicViewSceneActor->TerrainComponent->GetComponentTransform().Equals(
            SavedSourceTerrainWorldTransform,
            0.0001) ||
        PublicViewSceneActor->TerrainComponent->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics ||
        !bSavedSourceTerrainVisible || bSavedSourceTerrainHiddenInGame ||
        PublicViewSceneActor->TerrainComponent->IsVisible() ==
            bExpectedSourceTerrainHidden ||
        PublicViewSceneActor->TerrainComponent->bHiddenInGame !=
            bExpectedSourceTerrainHidden ||
        bSourceTerrainRendererHiddenForReadyProvider !=
            bExpectedSourceTerrainHidden ||
        bSourceTerrainRendererRestoredForFallback ==
            bExpectedSourceTerrainHidden ||
        (HasActorBegunPlay() && bPolicyHasBegunPlay &&
            !bLastContextPolicyStateValid) ||
        !SavedGroundOverlayWorldTransform.Equals(
            ExpectedOverlayTransform,
            0.0001) ||
        !GroundMacroVariationOverlay ||
        GroundMacroVariationOverlay->GetOwner() != this ||
        GroundMacroVariationOverlay->GetStaticMesh() !=
            SavedAssets.GroundOverlayMesh ||
        GroundMacroVariationOverlay->GetMaterial(0) !=
            SavedAssets.GroundOverlayMaterial ||
        !GroundMacroVariationOverlay->GetComponentTransform().Equals(
            SavedGroundOverlayWorldTransform,
            0.0001) ||
        GroundMacroVariationOverlay->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision ||
        GroundMacroVariationOverlay->Mobility != EComponentMobility::Movable ||
        GroundMacroVariationOverlay->CanEverAffectNavigation() ||
        !GroundMacroVariationOverlay->IsVisible() ||
        GroundMacroVariationOverlay->bHiddenInGame ||
        GroundMacroVariationOverlay->CastShadow ||
        GroundMacroVariationOverlay->bAffectDistanceFieldLighting)
    {
        OutReport = TEXT("The V5D macro-ground overlay or untouched source-terrain readback drifted.");
        return false;
    }

    FString SourceTurfPresentationReport;
    if (HasActorBegunPlay())
    {
        if (!ValidateRuntimeSourceV5BTurfPresentation(
                SourceTurfPresentationReport))
        {
            OutReport = TEXT("The V5D runtime inherited-turf presentation failed its exact source/isolation gate: ") +
                SourceTurfPresentationReport;
            return false;
        }
    }
    else if (!V5BVisualActor || !V5BVisualActor->AccentTurfInstances ||
        bRuntimeSourceV5BTurfPresentationApplied ||
        bRuntimeSourceV5BTurfSnapshotValid ||
        RuntimeOriginalSourceV5BTurfOwnerActor.IsValid() ||
        RuntimeOriginalSourceV5BTurfComponent.IsValid() ||
        bRuntimeOriginalSourceV5BTurfHadPresentationTag ||
        V5BVisualActor->Tags.Contains(RuntimeSourceTurfPresentationTag()) ||
        V5BVisualActor->AccentTurfInstances->GetMaterial(0) !=
            V5BVisualActor->SavedAssetRoster.AccentTurfMaterial ||
        !V5BVisualActor->AccentTurfInstances->IsVisible() ||
        V5BVisualActor->AccentTurfInstances->bHiddenInGame)
    {
        OutReport = TEXT("The cold V5D map leaked or inherited a runtime turf presentation; original V5B material binding is required before Play.");
        return false;
    }

    FString EdgeGrassFadeReport;
    if (HasActorBegunPlay())
    {
        if (!ValidateRuntimeEdgeGrassFadePresentation(
                EdgeGrassFadeReport))
        {
            OutReport = TEXT("The V5D runtime edge-grass fade failed its exact transient-material/isolation gate: ") +
                EdgeGrassFadeReport;
            return false;
        }
    }
    else if (!EdgeGrassInstances ||
        bRuntimeEdgeGrassFadePresentationApplied ||
        bRuntimeEdgeGrassFadeSnapshotValid ||
        RuntimeOriginalEdgeGrassComponent.IsValid() ||
        RuntimeOriginalEdgeGrassResolvedMaterial ||
        !RuntimeOriginalEdgeGrassOverrideMaterials.IsEmpty() ||
        EdgeGrassInstances->GetMaterial(0) != SavedAssets.EdgeGrassMaterial)
    {
        OutReport = TEXT("The cold V5D map leaked a runtime edge-grass fade presentation; its exact serialized source/owned material binding is required before Play.");
        return false;
    }

    TArray<FTransform> Grass;
    TArray<FTransform> EdgeGrass;
    TArray<FTransform> Trees;
    TArray<FTransform> ExistingSoil;
    FTRIADIstanaExploreV5DGroundVegetationLayout Rebuilt;
    if (!ExtractSourceTransforms(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            Error) ||
        !BuildDeterministicLayout(
            Grass,
            EdgeGrass,
            Trees,
            ExistingSoil,
            SeasonProfile,
            Rebuilt,
            Error) ||
        !TransformArraysEqual(SavedGrassManicured, Rebuilt.GrassManicured) ||
        !TransformArraysEqual(SavedGrassHumid, Rebuilt.GrassHumid) ||
        !TransformArraysEqual(SavedGrassShade, Rebuilt.GrassShade) ||
        !TransformArraysEqual(SavedGrassDryEdge, Rebuilt.GrassDryEdge) ||
        !TransformArraysEqual(SavedEdgeGrass, Rebuilt.EdgeGrass) ||
        !TransformArraysEqual(SavedSupplementalSoil, Rebuilt.SupplementalSoil) ||
        !TransformArraysEqual(SavedSupplementalShrubs, Rebuilt.SupplementalShrubs) ||
        !TransformArraysEqual(
            SavedSupplementalUnderstorey,
            Rebuilt.SupplementalUnderstorey) ||
        !TransformArraysEqual(SavedSupplementalFlowers, Rebuilt.SupplementalFlowers))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("Saved V5D transforms no longer equal the pure deterministic rebuild.")
            : Error;
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    const int32 ExpectedGrassCounts[] = {
        SavedGrassManicured.Num(),
        SavedGrassHumid.Num(),
        SavedGrassShade.Num(),
        SavedGrassDryEdge.Num()};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassComponents); ++Index)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            GrassComponents[Index];
        int32 StartCullDistance = 0;
        int32 EndCullDistance = 0;
        if (Component)
        {
            Component->GetCullDistances(StartCullDistance, EndCullDistance);
        }
        if (!Component || Component->GetOwner() != this ||
            Component->GetStaticMesh() != SavedAssets.GrassMicroDetailMesh ||
            Component->GetMaterial(0) != SavedAssets.GrassProfileMaterials[Index] ||
            Component->GetInstanceCount() != ExpectedGrassCounts[Index] ||
            StartCullDistance != GrassCullStartDistanceCm ||
            EndCullDistance != GrassCullEndDistanceCm ||
            Component->WorldPositionOffsetDisableDistance !=
                GrassWpoDisableDistanceCm ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                GrassLodDistanceScale,
                0.0001f) ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->CanEverAffectNavigation() ||
            Component->bEnableDensityScaling || Component->CastShadow ||
            Component->bCastContactShadow ||
            Component->bAffectDistanceFieldLighting)
        {
            OutReport = FString::Printf(
                TEXT("V5D grass component %d lost its exact visual-only mesh/material/count/policy state."),
                Index);
            return false;
        }
    }

    int32 EdgeGrassCullStart = 0;
    int32 EdgeGrassCullEnd = 0;
    if (EdgeGrassInstances)
    {
        EdgeGrassInstances->GetCullDistances(
            EdgeGrassCullStart,
            EdgeGrassCullEnd);
    }
    if (!EdgeGrassInstances ||
        EdgeGrassCullStart != EdgeGrassCullStartDistanceCm ||
        EdgeGrassCullEnd != EdgeGrassCullEndDistanceCm ||
        EdgeGrassInstances->WorldPositionOffsetDisableDistance !=
            EdgeGrassWpoDisableDistanceCm ||
        !FMath::IsNearlyEqual(
            EdgeGrassInstances->InstanceLODDistanceScale,
            GrassLodDistanceScale,
            0.0001f) || EdgeGrassInstances->CastShadow ||
        EdgeGrassInstances->bCastContactShadow ||
        EdgeGrassInstances->bAffectDistanceFieldLighting)
    {
        OutReport = TEXT("V5D edge grass lost its bounded 30-45 m cull, 24 m WPO, early-LOD, or no-shadow policy.");
        return false;
    }

    struct FExpectedComponent
    {
        const UHierarchicalInstancedStaticMeshComponent* Component;
        const UStaticMesh* Mesh;
        const UMaterialInterface* Material;
        int32 Count;
    };
    const UMaterialInterface* ExpectedEdgeGrassMaterial = HasActorBegunPlay()
        ? EdgeGrassFadeMaterialCookReference.Get()
        : SavedAssets.EdgeGrassMaterial.Get();
    const FExpectedComponent Expected[] = {
        {EdgeGrassInstances, SavedAssets.EdgeGrassMesh, ExpectedEdgeGrassMaterial, SavedEdgeGrass.Num()},
        {SupplementalSoilInstances, SavedAssets.SoilPatchMesh, SavedAssets.SoilPatchMaterial, SavedSupplementalSoil.Num()},
        {SupplementalShrubInstances, SavedAssets.ShrubMesh, SavedAssets.ShrubMaterial, SavedSupplementalShrubs.Num()},
        {SupplementalUnderstoreyInstances, SavedAssets.UnderstoreyMesh, SavedAssets.UnderstoreyMaterial, SavedSupplementalUnderstorey.Num()},
        {SupplementalFlowerInstances, SavedAssets.FlowerMesh, SavedAssets.FlowerMaterial, SavedSupplementalFlowers.Num()}};
    for (const FExpectedComponent& Row : Expected)
    {
        if (!Row.Component || Row.Component->GetOwner() != this ||
            Row.Component->GetStaticMesh() != Row.Mesh ||
            Row.Component->GetMaterial(0) != Row.Material ||
            Row.Component->GetInstanceCount() != Row.Count ||
            Row.Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Row.Component->CanEverAffectNavigation() ||
            Row.Component->bEnableDensityScaling)
        {
            OutReport = TEXT("A V5D edge/soil/understorey component lost its exact visual-only mesh/material/count/policy state.");
            return false;
        }
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID groundMacroVariationOverlay=1 fineTurfAppearanceProxy=true currentBotanicalOrSpeciesClaim=false groundCoreMask=irregularEllipse64 coreCenterMeters=(%.1f,%.1f) coreSemiAxesMeters=(%.1f,%.1f) opaqueCoverageCollarMeters=%.1f outwardCoverageFeatherMeters=%.1f maximumOutsideProviderClipMeters=%.1f stableDitherCellMeters=%.2f coverageGapAtProviderBoundary=false dedicatedOverlayTextureSamples=4 mowingBandMeters=3.2 mowingAngleDegrees=7 lowFrequencyVariationMeters=11,48 farDetailFadeMeters=30,55 farNormalStrength=revisionValidated grassMicroDetail=%d inheritedSourceGrass=%d inheritedSourceGrassSerializedUntouched=true runtimeCompositeV5BAndV5DGrassOwnership=true maximumDeterministicReuseLayers=%d grassLayerOffsetRadiusBandCm=18..42 grassLayerGoldenAngleDecorrelationDegrees=137.507764 grassYaw=fullHashed360WithGoldenAngleLayerDecorrelation grassTerrainHeight=exactCopiedV5BAnalyticFormula sourceSurfaceDeltaPreserved=true addedPlacementGapCm=0.01..0.05 grassCoverageScale=0.72..0.92 grassSourceXyNormalizedByGeometricMean=true grassFinalAnisotropy=0.98..1.02 grassHeightScale=1.05..1.25 grassHeightClasses=0.88,1.02,1.18 horizontalAndVerticalScaleSplit=true correlatedMesoClumps=true grassPatchSuppressionRadiusCm=350 heroLawnProfile=predominantlyManicuredHumid restrainedHeroShadeDryIntrusion=true v5dGrassCullStartMeters=%.1f v5dGrassCullEndMeters=%.1f v5dGrassWpoDisableMeters=%.1f grassMaterialVisibilityMeters=20..28 grassLodDistanceScale=0.60 compositeSourceGrassCullMeters=40..65 compositeSourceGrassLodDistanceScale=1.10 edgeGrassCullStartMeters=%.1f edgeGrassCullEndMeters=%.1f edgeGrassWpoDisableMeters=%.1f edgeGrassFadeAsset=M_IPV5D_GrassMedium_EdgeFade edgeGrassRuntimeMaterial=%s edgeGrassFade=revisionValidated sourceOpacityMaskOutput=1_red sourceV4MaterialAssetUntouched=true edgeGrassFullOverrideArraySnapshotRestore=true grassAndEdgeShadowsUnchangedDisabled=true edgeGrassTransformsCensusCollisionNavigationSensorRfUntouched=true manicured=%d humid=%d shade=%d dryEdge=%d tallerEdgeGrass=%d tallerEdgeGrassSeasonCensus=%d,%d,%d edgeGrassCandidateMask=actualHeroLawnPlusCeremonialAxis supplementalSoil=%d shrubs=%d understorey=%d flowers=%d heroLawnPreserved=true deterministicSpatialClumps=true continuousEdgeTransition=true sourceTerrainMeshMaterialTransformCollisionUntouched=true sourceTerrainRendererHiddenForReadyProvider=%s sourceTerrainRendererRestoredForFallback=%s inheritedV5BSourceMaterialAssetAndComponentBindingUntouched=true inheritedV5BSourceMeshTransformsCensusUntouched=true inheritedV5BSerializedCullLodWpoUntouched=true inheritedV5BRuntimeCullLodVisualOverrideOnly=true inheritedV5BCollisionOverlapNavigationRfSimulationUntouched=true inheritedV5BSourceRendererPresentation=%s inheritedV5BFullVisibilityMaterialCullLodTagSnapshotRestore=true sourceV4VegetationUntouched=true renderOnly=true collision=false navigation=false sensorRfAuthority=false survey=false botanicalInventory=false currentWeatherClaim=false cesiumBakeCacheTraceAnalysis=false"),
        GroundOverlayCoreCenterMeters.X,
        GroundOverlayCoreCenterMeters.Y,
        GroundOverlayCoreSemiAxesMeters.X,
        GroundOverlayCoreSemiAxesMeters.Y,
        RequiredGroundOverlayOpaqueCollarMeters,
        RequiredGroundOverlayOutwardFeatherMeters,
        RequiredGroundOverlayOpaqueCollarMeters +
            RequiredGroundOverlayOutwardFeatherMeters,
        RequiredGroundOverlayDitherCellMeters,
        GrassMicroDetailCount,
        SourceGrassCount,
        MaximumGrassReuseLayers,
        GrassCullStartDistanceCm / CentimetersPerMeter,
        GrassCullEndDistanceCm / CentimetersPerMeter,
        GrassWpoDisableDistanceCm / CentimetersPerMeter,
        EdgeGrassCullStartDistanceCm / CentimetersPerMeter,
        EdgeGrassCullEndDistanceCm / CentimetersPerMeter,
        EdgeGrassWpoDisableDistanceCm / CentimetersPerMeter,
        HasActorBegunPlay()
            ? TEXT("M_IPV5D_GrassMedium_EdgeFade")
            : TEXT("inactive_cold_serialized_material"),
        SavedGrassManicured.Num(),
        SavedGrassHumid.Num(),
        SavedGrassShade.Num(),
        SavedGrassDryEdge.Num(),
        SavedEdgeGrass.Num(),
        ExpectedEdgeGrassCount(
            ETRIADIstanaExploreV5DSeasonProfile::HumidWet),
        ExpectedEdgeGrassCount(
            ETRIADIstanaExploreV5DSeasonProfile::Transition),
        ExpectedEdgeGrassCount(
            ETRIADIstanaExploreV5DSeasonProfile::DryStressPreview),
        SavedSupplementalSoil.Num(),
        SavedSupplementalShrubs.Num(),
        SavedSupplementalUnderstorey.Num(),
        SavedSupplementalFlowers.Num(),
        bSourceTerrainRendererHiddenForReadyProvider
            ? TEXT("true") : TEXT("false"),
        bSourceTerrainRendererRestoredForFallback
            ? TEXT("true") : TEXT("false"),
        HasActorBegunPlay()
            ? TEXT("visible_runtime_composite_40m_to65m")
            : TEXT("inactive_cold_visible_V5B_original"));
    OutReport += TEXT(" grassPresentationRevision=R23 grassPredecessorRevision=R20 stagedLegacyMigration=R14ToR20ToR23 ");
    if (GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R23B)
    {
        OutReport += TEXT("grassMaterialRevision=R23B grassR23BPhotographicCalibration=true grassR23StableSpatialVisibility=true grassR23DefaultWindStrengthCm=0 grassMaterialCalibrationRevision=R23B groundOverlayMaterialRevision=R23 groundOverlayMaterialCalibrationRevision=R23B edgeGrassMaterialRevision=R23 edgeGrassMaterialCalibrationRevision=R23B runtimeMaterialRevisionMarker=23 runtimeMaterialCalibrationRevisionMarker=1 edgeGrassFade=StableSpatialVisibility20m28m ");
    }
    else if (GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R23)
    {
        OutReport += TEXT("grassMaterialRevision=R23 grassR23StableSpatialVisibility=true grassR23DefaultWindStrengthCm=0 groundOverlayMaterialRevision=R23 edgeGrassMaterialRevision=R23 runtimeMaterialRevisionMarker=23 edgeGrassFade=StableSpatialVisibility20m28m ");
    }
    else if (GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R22)
    {
        OutReport += TEXT("grassMaterialRevision=R22 grassR22StableSpatialVisibility=true grassR22DefaultWindStrengthCm=0 groundOverlayMaterialRevision=R22 edgeGrassMaterialRevision=R22 runtimeMaterialRevisionMarker=22 edgeGrassFade=StableSpatialVisibility12m18m ");
    }
    else if (GrassMaterialRuntimeRevision == EGrassMaterialRuntimeRevision::R21)
    {
        OutReport += TEXT("grassMaterialRevision=R21 grassR21CalibratedColorNormalAndSurfaceResponse=true groundOverlayMaterialRevision=R18 edgeGrassMaterialRevision=R11 edgeGrassFade=DitherTemporalAA_PerInstanceFade ");
    }
    else
    {
        OutReport += TEXT("grassMaterialRevision=R19 grassR19DistanceFilteredColorNormalAndSurfaceResponse=true groundOverlayMaterialRevision=R18 edgeGrassMaterialRevision=R11 edgeGrassFade=DitherTemporalAA_PerInstanceFade ");
    }
    OutReport += TEXT("materialRevisionDerivedFromExactRuntimeParameterSignature=true materialRevisionDerivedFromUniformBladeOverlayEdgeMarkers=true validatedGrassRevisionSelection=R23B_FIRST_R23_THEN_R22_THEN_R21_THEN_R19_FALLBACK groundOverlayR16CoreMaskPreserved=true groundOverlayCoreMaskFingerprint=V5 opaqueCollarMeters=50 groundOverlayOpaqueCollarMeters=50 groundOverlayDitherFeatherMeters=8 groundOverlayMaximumOutsideProviderClipMeters=58 providerSiteClipRemainsActive=true grassCoverageSignalWeights=0.72/0.28 grassHeightSignalWeights=0.55/0.45 accentCarrierRevision=R11 accentCarrierLodTriangles=2550/680/204 treeBaseGrassSuppressionRadiusFraction=1.0 treeBaseGrassCarrierFootprintMarginCm=220");
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    ValidateOwnedGrassInstanceTransforms(FString& OutReport) const
{
    // UE 5.5's CPU GetInstanceTransform path is double-backed FMatrix, but
    // the proof's observable HISM render tree converts instances to
    // FMatrix44f. At this map's roughly 138 m coordinates, 0.001 cm is the
    // renderer-representability floor and remains below authored displacement.
    constexpr double SerializedHismTransformToleranceCm = 0.001;
    const UHierarchicalInstancedStaticMeshComponent* Components[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances,
        EdgeGrassInstances};
    const TArray<FTransform>* ExpectedTransforms[] = {
        &SavedGrassManicured,
        &SavedGrassHumid,
        &SavedGrassShade,
        &SavedGrassDryEdge,
        &SavedEdgeGrass};
    static_assert(
        UE_ARRAY_COUNT(Components) == 5 &&
            UE_ARRAY_COUNT(ExpectedTransforms) == 5,
        "The exact grass transform roster must remain five rows.");

    int32 VerifiedTransforms = 0;
    for (int32 ComponentIndex = 0;
         ComponentIndex < UE_ARRAY_COUNT(Components);
         ++ComponentIndex)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            Components[ComponentIndex];
        const TArray<FTransform>& Expected = *ExpectedTransforms[ComponentIndex];
        if (!IsValid(Component) ||
            Component->GetInstanceCount() != Expected.Num())
        {
            OutReport = FString::Printf(
                TEXT("Grass transform roster row %d has an invalid component/count."),
                ComponentIndex);
            return false;
        }
        for (int32 InstanceIndex = 0;
             InstanceIndex < Expected.Num();
             ++InstanceIndex)
        {
            FTransform Actual = FTransform::Identity;
            if (!Component->GetInstanceTransform(
                    InstanceIndex,
                    Actual,
                    true) ||
                !Actual.Equals(
                    Expected[InstanceIndex],
                    SerializedHismTransformToleranceCm))
            {
                OutReport = FString::Printf(
                    TEXT("Grass transform roster row %d instance %d drifted from the exact deterministic saved layout."),
                    ComponentIndex,
                    InstanceIndex);
                return false;
            }
            ++VerifiedTransforms;
        }
    }
    if (VerifiedTransforms != GrassMicroDetailCount + SavedEdgeGrass.Num())
    {
        OutReport = FString::Printf(
            TEXT("Expected %d exact grass transforms, validated %d."),
            GrassMicroDetailCount + SavedEdgeGrass.Num(),
            VerifiedTransforms);
        return false;
    }
    OutReport = FString::Printf(
        TEXT("exactDeterministicSavedGrassTransforms=true verifiedGrassTransforms=%d"),
        VerifiedTransforms);
    return true;
}

bool ATRIADIstanaExploreV5DGroundVegetationActor::
    GetMediumDistanceTurfSourceProfilesR32(
        TArray<FTransform>& OutManicured,
        TArray<FTransform>& OutHumid,
        TArray<FTransform>& OutShade,
        TArray<FTransform>& OutDryEdge,
        FString& OutReport) const
{
    OutManicured.Reset();
    OutHumid.Reset();
    OutShade.Reset();
    OutDryEdge.Reset();

    constexpr int32 ExpectedManicuredCount = 12460;
    constexpr int32 ExpectedHumidCount = 3976;
    constexpr int32 ExpectedShadeCount = 1152;
    constexpr int32 ExpectedDryEdgeCount = 844;

    if (!bConfigured ||
        GrassPresentationRevision != GrassPresentationRevisionR23 ||
        SavedGrassManicured.Num() != ExpectedManicuredCount ||
        SavedGrassHumid.Num() != ExpectedHumidCount ||
        SavedGrassShade.Num() != ExpectedShadeCount ||
        SavedGrassDryEdge.Num() != ExpectedDryEdgeCount ||
        SavedGrassManicured.Num() + SavedGrassHumid.Num() +
                SavedGrassShade.Num() + SavedGrassDryEdge.Num() !=
            GrassMicroDetailCount)
    {
        OutReport = FString::Printf(
            TEXT("R32 turf source profiles require configured exact R23 arrays; configured=%s revision=%d manicured=%d humid=%d shade=%d dryEdge=%d."),
            bConfigured ? TEXT("true") : TEXT("false"),
            GrassPresentationRevision,
            SavedGrassManicured.Num(),
            SavedGrassHumid.Num(),
            SavedGrassShade.Num(),
            SavedGrassDryEdge.Num());
        return false;
    }

    FString FullGroundReport;
    if (!ValidateGroundVegetationRealism(FullGroundReport))
    {
        OutReport = FString::Printf(
            TEXT("R32 turf source profiles rejected an invalid ground/vegetation owner: %s"),
            *FullGroundReport);
        return false;
    }

    FString ExactTransformReport;
    if (!ValidateOwnedGrassInstanceTransforms(ExactTransformReport))
    {
        OutReport = FString::Printf(
            TEXT("R32 turf source profiles rejected source transform drift: %s"),
            *ExactTransformReport);
        return false;
    }

    // Copy-only handoff: the source actor and its component roster remain
    // untouched. R32 may select from these transforms but cannot mutate them.
    OutManicured = SavedGrassManicured;
    OutHumid = SavedGrassHumid;
    OutShade = SavedGrassShade;
    OutDryEdge = SavedGrassDryEdge;
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R32_TURF_SOURCE_PROFILES_VALID sourceTransforms=%d manicured=%d humid=%d shade=%d dryEdge=%d fullGroundVegetationValid=true sourceReadOnly=true %s"),
        OutManicured.Num() + OutHumid.Num() + OutShade.Num() +
            OutDryEdge.Num(),
        OutManicured.Num(),
        OutHumid.Num(),
        OutShade.Num(),
        OutDryEdge.Num(),
        *ExactTransformReport);
    return true;
}

void ATRIADIstanaExploreV5DGroundVegetationActor::BeginPlay()
{
    Super::BeginPlay();
    GetWorldTimerManager().SetTimerForNextTick(
        this,
        &ATRIADIstanaExploreV5DGroundVegetationActor::
            ApplyDeferredRuntimeGroundVegetationPresentation);
}

void ATRIADIstanaExploreV5DGroundVegetationActor::
    ApplyDeferredRuntimeGroundVegetationPresentation()
{
    FString Error;
    FString Report;
    if (!ReapplyGroundVegetationRealism(Error) ||
        !EnsureRuntimeEdgeGrassFadePresentation(Error) ||
        !EnsureRuntimeSourceV5BTurfPresentation(Error) ||
        !SynchronizeSourceTerrainRendererWithProviderPolicy(Error) ||
        !ValidateGroundVegetationRealism(Report))
    {
        FString TurfRestoreError;
        FString EdgeRestoreError;
        FString TerrainRestoreError;
        RestoreRuntimeEdgeGrassFadePresentation(EdgeRestoreError);
        RestoreRuntimeSourceV5BTurfPresentation(TurfRestoreError);
        RestoreSourceTerrainRendering(TerrainRestoreError);
        bRuntimePolicyFailureWasLogged = true;
        UE_LOG(
            LogTRIADIstanaExploreV5DGroundVegetation,
            Warning,
            TEXT("V5D ground/vegetation runtime failed closed with serialized edge grass, the inherited turf renderer/material/tag restored, and source terrain visible: %s %s edgeRestore=%s turfRestore=%s terrainRestore=%s"),
            *Error,
            *Report,
            *EdgeRestoreError,
            *TurfRestoreError,
            *TerrainRestoreError);
        return;
    }
    UE_LOG(
        LogTRIADIstanaExploreV5DGroundVegetation,
        Display,
        TEXT("%s"),
        *Report);
}

void ATRIADIstanaExploreV5DGroundVegetationActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bConfigured)
    {
        return;
    }

    FString Error;
    FString Report;
    if (!EnsureRuntimeEdgeGrassFadePresentation(Error) ||
        !EnsureRuntimeSourceV5BTurfPresentation(Error) ||
        !SynchronizeSourceTerrainRendererWithProviderPolicy(Error) ||
        !ValidateGroundVegetationRealism(Report))
    {
        FString TurfRestoreError;
        FString EdgeRestoreError;
        FString TerrainRestoreError;
        RestoreRuntimeEdgeGrassFadePresentation(EdgeRestoreError);
        RestoreRuntimeSourceV5BTurfPresentation(TurfRestoreError);
        RestoreSourceTerrainRendering(TerrainRestoreError);
        if (!bRuntimePolicyFailureWasLogged)
        {
            UE_LOG(
                LogTRIADIstanaExploreV5DGroundVegetation,
                Warning,
                TEXT("V5D terrain/provider/turf/edge presentation failed closed with serialized edge grass, the inherited turf renderer/material/tag restored, and source terrain visible: %s %s edgeRestore=%s turfRestore=%s terrainRestore=%s"),
                *Error,
                *Report,
                *EdgeRestoreError,
                *TurfRestoreError,
                *TerrainRestoreError);
            bRuntimePolicyFailureWasLogged = true;
        }
        return;
    }

    if (bRuntimePolicyFailureWasLogged)
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DGroundVegetation,
            Display,
            TEXT("V5D terrain/provider/turf/edge presentation recovered: %s"),
            *Report);
        bRuntimePolicyFailureWasLogged = false;
    }
}

void ATRIADIstanaExploreV5DGroundVegetationActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (ATRIADIstanaExploreV5DContextPolicyActor* Policy =
            RuntimeContextPolicyActor.Get())
    {
        RemoveTickPrerequisiteActor(Policy);
    }
    RuntimeContextPolicyActor.Reset();
    FString EdgeRestoreError;
    if (!RestoreRuntimeEdgeGrassFadePresentation(EdgeRestoreError))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DGroundVegetation,
            Error,
            TEXT("V5D edge-grass fade EndPlay restore failed: %s"),
            *EdgeRestoreError);
    }
    FString TurfRestoreError;
    if (!RestoreRuntimeSourceV5BTurfPresentation(TurfRestoreError))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DGroundVegetation,
            Error,
            TEXT("V5D inherited-turf renderer/material/tag EndPlay restore failed: %s"),
            *TurfRestoreError);
    }
    FString RestoreError;
    if (bConfigured && !RestoreSourceTerrainRendering(RestoreError))
    {
        UE_LOG(
            LogTRIADIstanaExploreV5DGroundVegetation,
            Error,
            TEXT("V5D source terrain renderer EndPlay restore failed: %s"),
            *RestoreError);
    }
    bLastContextPolicyStateValid = false;
    Super::EndPlay(EndPlayReason);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DEdgeGrassFadeCookDependencyTest,
    "TRIAD.Istana.ExploreV5D.GroundVegetation.EdgeGrassFadeCookDependency",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DEdgeGrassFadeCookDependencyTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    FString Report;
    const bool bValid = ATRIADIstanaExploreV5DGroundVegetationActor::
        ValidateNativeEdgeGrassFadeCookDependency(Report);
    TestTrue(TEXT("Native CDO/package edge-fade dependency"), bValid);
    if (!bValid)
    {
        AddError(Report);
    }
    else
    {
        TestTrue(
            TEXT("Cook report names the exact derivative"),
            Report.Contains(TEXT("M_IPV5D_GrassMedium_EdgeFade")));
    }
    return bValid;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DGroundVegetationDeterministicLayoutTest,
    "TRIAD.Istana.ExploreV5D.GroundVegetation.DeterministicLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DGroundVegetationDeterministicLayoutTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TArray<FTransform> Grass;
    Grass.Reserve(SourceGrassCount);
    for (int32 Y = 0; Y < 96; ++Y)
    {
        for (int32 X = 0; X < 192; ++X)
        {
            Grass.Add(FTransform(
                FRotator(0.0, static_cast<double>((X * 17 + Y * 29) % 360), 0.0),
                FVector(
                    AccentMinimumXCm + 100.0 * static_cast<double>(X),
                    AccentMinimumYCm + 220.0 * static_cast<double>(Y),
                    40.0),
                FVector(0.55, 0.55, 0.55)));
        }
    }
    TArray<FTransform> Edge;
    Edge.Reserve(SourceEdgeGrassCount);
    for (int32 Y = 0; Y < 24; ++Y)
    {
        for (int32 X = 0; X < 64; ++X)
        {
            const double Side = X < 32 ? -1.0 : 1.0;
            const double Across = static_cast<double>(X % 32);
            Edge.Add(FTransform(
                FRotator(0.0, static_cast<double>((X * 11 + Y * 7) % 360), 0.0),
                FVector(
                    Side * (5600.0 + Across * 390.0),
                    1800.0 + static_cast<double>(Y) * 930.0,
                    42.0),
                FVector(0.8, 0.8, 0.8)));
        }
    }
    TArray<FTransform> Trees;
    Trees.Reserve(SourceTreeCount);
    for (int32 Y = 0; Y < 27; ++Y)
    {
        for (int32 X = 0; X < 27; ++X)
        {
            Trees.Add(FTransform(
                FRotator(0.0, static_cast<double>((X * 31 + Y * 13) % 360), 0.0),
                FVector(
                    -17500.0 + static_cast<double>(X) * 1350.0,
                    2100.0 + static_cast<double>(Y) * 840.0,
                    39.0),
                FVector(1.0, 1.0, 1.0)));
        }
    }
    TArray<FTransform> ExistingSoil;
    ExistingSoil.Reserve(ExistingSoilCount);
    for (int32 Index = 0; Index < ExistingSoilCount; ++Index)
    {
        const double Side = Index < 32 ? -1.0 : 1.0;
        ExistingSoil.Add(FTransform(
            FRotator::ZeroRotator,
            FVector(
                Side * (6900.0 + static_cast<double>(Index % 8) * 850.0),
                3600.0 + static_cast<double>(Index % 32) * 570.0,
                39.5),
            FVector::OneVector));
    }

    FTRIADIstanaExploreV5DGroundVegetationLayout WetA;
    FTRIADIstanaExploreV5DGroundVegetationLayout WetB;
    FTRIADIstanaExploreV5DGroundVegetationLayout Transition;
    FTRIADIstanaExploreV5DGroundVegetationLayout Dry;
    FString Error;
    TestTrue(
        TEXT("Humid-wet layout builds"),
        ATRIADIstanaExploreV5DGroundVegetationActor::BuildDeterministicLayout(
            Grass,
            Edge,
            Trees,
            ExistingSoil,
            ETRIADIstanaExploreV5DSeasonProfile::HumidWet,
            WetA,
            Error));
    TestTrue(
        TEXT("Repeated humid-wet layout builds"),
        ATRIADIstanaExploreV5DGroundVegetationActor::BuildDeterministicLayout(
            Grass,
            Edge,
            Trees,
            ExistingSoil,
            ETRIADIstanaExploreV5DSeasonProfile::HumidWet,
            WetB,
            Error));
    TestTrue(
        TEXT("Transition layout builds"),
        ATRIADIstanaExploreV5DGroundVegetationActor::BuildDeterministicLayout(
            Grass,
            Edge,
            Trees,
            ExistingSoil,
            ETRIADIstanaExploreV5DSeasonProfile::Transition,
            Transition,
            Error));
    TestTrue(
        TEXT("Dry-stress preview layout builds"),
        ATRIADIstanaExploreV5DGroundVegetationActor::BuildDeterministicLayout(
            Grass,
            Edge,
            Trees,
            ExistingSoil,
            ETRIADIstanaExploreV5DSeasonProfile::DryStressPreview,
            Dry,
            Error));
    TestEqual(
        TEXT("Exact R23 grass micro-detail census"),
        WetA.GrassTotal(),
        GrassMicroDetailCount);
    TestEqual(
        TEXT("R23 fills the exact source-sized modeled-blade census"),
        WetA.GrassTotal(),
        Grass.Num());
    TestEqual(TEXT("Exact humid-wet edge census"), WetA.EdgeGrass.Num(), 512);
    TestEqual(
        TEXT("Exact transition edge census"),
        Transition.EdgeGrass.Num(),
        640);
    TestEqual(TEXT("Exact dry preview edge census"), Dry.EdgeGrass.Num(), 768);
    TestEqual(TEXT("Exact supplemental soil census"), WetA.SupplementalSoil.Num(), 24);
    TestEqual(TEXT("Exact shrub census"), WetA.SupplementalShrubs.Num(), 48);
    TestEqual(TEXT("Exact understorey census"), WetA.SupplementalUnderstorey.Num(), 96);
    TestEqual(TEXT("Exact flower census"), WetA.SupplementalFlowers.Num(), 24);
    TestTrue(
        TEXT("Repeated grass layout is byte-order deterministic"),
        TransformArraysEqual(WetA.GrassManicured, WetB.GrassManicured) &&
            TransformArraysEqual(WetA.GrassHumid, WetB.GrassHumid) &&
            TransformArraysEqual(WetA.GrassShade, WetB.GrassShade) &&
            TransformArraysEqual(WetA.GrassDryEdge, WetB.GrassDryEdge));
    TestTrue(
        TEXT("Repeated soil layout is deterministic"),
        TransformArraysEqual(WetA.SupplementalSoil, WetB.SupplementalSoil));
    TestTrue(
        TEXT("Season preview changes the profile partition"),
        WetA.GrassDryEdge.Num() != Dry.GrassDryEdge.Num());
    const auto CountHeroProfile = [](const TArray<FTransform>& Transforms)
    {
        int32 Count = 0;
        for (const FTransform& Transform : Transforms)
        {
            if (IsInsideHeroLawn(Transform.GetTranslation()))
            {
                ++Count;
            }
        }
        return Count;
    };
    const int32 HeroManicured = CountHeroProfile(WetA.GrassManicured);
    const int32 HeroHumid = CountHeroProfile(WetA.GrassHumid);
    const int32 HeroShade = CountHeroProfile(WetA.GrassShade);
    const int32 HeroDry = CountHeroProfile(WetA.GrassDryEdge);
    const int32 HeroTotal =
        HeroManicured + HeroHumid + HeroShade + HeroDry;
    TestTrue(
        TEXT("Hero lawn keeps restrained deterministic shade intrusion"),
        HeroShade > 0);
    TestTrue(
        TEXT("Hero lawn keeps restrained deterministic dry intrusion"),
        HeroDry > 0);
    TestTrue(
        TEXT("Hero lawn remains at least 95 percent manicured or humid"),
        HeroTotal > 0 &&
            static_cast<int64>(HeroManicured + HeroHumid) * 100 >=
                static_cast<int64>(HeroTotal) * 95);
    double MinimumGrassCoverageScale = TNumericLimits<double>::Max();
    double MaximumGrassCoverageScale = 0.0;
    double MinimumGrassHeightScale = TNumericLimits<double>::Max();
    double MaximumGrassHeightScale = 0.0;
    double MinimumPlacementGapCm = TNumericLimits<double>::Max();
    double MaximumPlacementGapCm = -TNumericLimits<double>::Max();
    double MinimumGrassYawDegrees = TNumericLimits<double>::Max();
    double MaximumGrassYawDegrees = -TNumericLimits<double>::Max();
    double MinimumSupplementalPatchCenterDistanceCm =
        TNumericLimits<double>::Max();
    double MinimumExistingTreeBasePatchCenterDistanceCm =
        TNumericLimits<double>::Max();
    bool bGrassAnisotropyRatioIsBounded = true;
    bool bGrassReuseLayerRadialBandsAreExact = true;
    bool bGrassSourceMappingValid = true;
    bool bGoldenAngleLayerDecorrelationExact = true;
    TMap<int32, int32> GrassReuseCountBySource;
    TMap<int32, double> FirstGrassYawBySource;
    const TArray<FTransform>* GrassProfiles[] = {
        &WetA.GrassManicured,
        &WetA.GrassHumid,
        &WetA.GrassShade,
        &WetA.GrassDryEdge};
    for (const TArray<FTransform>* Profile : GrassProfiles)
    {
        for (const FTransform& Transform : *Profile)
        {
            const FVector Scale = Transform.GetScale3D();
            bGrassAnisotropyRatioIsBounded =
                bGrassAnisotropyRatioIsBounded && Scale.Y > 0.0 &&
                FMath::IsWithinInclusive(
                    Scale.X / Scale.Y,
                    GrassAnisotropyScaleMin /
                        GrassAnisotropyScaleMax,
                    GrassAnisotropyScaleMax /
                        GrassAnisotropyScaleMin);
            const FVector Location = Transform.GetTranslation();
            const int32 SourceX = FMath::RoundToInt(
                (Location.X - AccentMinimumXCm) / 100.0);
            const int32 SourceY = FMath::RoundToInt(
                (Location.Y - AccentMinimumYCm) / 220.0);
            bGrassSourceMappingValid = bGrassSourceMappingValid &&
                SourceX >= 0 && SourceX < 192 &&
                SourceY >= 0 && SourceY < 96;
            const int32 SourceKey = SourceY * 192 + SourceX;
            int32& ReuseCount = GrassReuseCountBySource.FindOrAdd(SourceKey);
            ++ReuseCount;
            const double NormalizedYaw = FRotator::NormalizeAxis(
                Transform.Rotator().Yaw);
            MinimumGrassYawDegrees = FMath::Min(
                MinimumGrassYawDegrees,
                NormalizedYaw);
            MaximumGrassYawDegrees = FMath::Max(
                MaximumGrassYawDegrees,
                NormalizedYaw);
            if (const double* FirstYaw = FirstGrassYawBySource.Find(SourceKey))
            {
                bGoldenAngleLayerDecorrelationExact =
                    bGoldenAngleLayerDecorrelationExact &&
                    FMath::IsNearlyEqual(
                        FMath::Abs(FMath::FindDeltaAngleDegrees(
                            *FirstYaw,
                            NormalizedYaw)),
                        GrassGoldenAngleDegrees,
                        0.001);
            }
            else
            {
                FirstGrassYawBySource.Add(SourceKey, NormalizedYaw);
            }
            const FVector SourceLocation(
                AccentMinimumXCm + 100.0 * SourceX,
                AccentMinimumYCm + 220.0 * SourceY,
                40.0);
            const double ReuseLayerRadiusCm = FVector2D::Distance(
                FVector2D(Location),
                FVector2D(SourceLocation));
            const double SourceSurfaceDeltaCm = SourceLocation.Z -
                AnalyticAccentTerrainHeightCm(
                    SourceLocation.X,
                    SourceLocation.Y);
            const double PlacementGapCm = Location.Z -
                AnalyticAccentTerrainHeightCm(Location.X, Location.Y) -
                SourceSurfaceDeltaCm;
            MinimumPlacementGapCm = FMath::Min(
                MinimumPlacementGapCm,
                PlacementGapCm);
            MaximumPlacementGapCm = FMath::Max(
                MaximumPlacementGapCm,
                PlacementGapCm);
            bGrassReuseLayerRadialBandsAreExact =
                bGrassReuseLayerRadialBandsAreExact &&
                FMath::IsWithinInclusive(
                    ReuseLayerRadiusCm,
                    GrassLayerOffsetMinimumRadiusCm,
                    GrassLayerOffsetMinimumRadiusCm +
                        GrassLayerOffsetBandWidthCm);
            const double CoverageScale = FMath::Sqrt(Scale.X * Scale.Y);
            MinimumGrassCoverageScale = FMath::Min(
                MinimumGrassCoverageScale,
                CoverageScale);
            MaximumGrassCoverageScale = FMath::Max(
                MaximumGrassCoverageScale,
                CoverageScale);
            MinimumGrassHeightScale = FMath::Min(
                MinimumGrassHeightScale,
                Scale.Z);
            MaximumGrassHeightScale = FMath::Max(
                MaximumGrassHeightScale,
                Scale.Z);
            for (const FTransform& Patch : WetA.SupplementalSoil)
            {
                MinimumSupplementalPatchCenterDistanceCm = FMath::Min(
                    MinimumSupplementalPatchCenterDistanceCm,
                    FVector2D::Distance(
                        FVector2D(Transform.GetTranslation()),
                        FVector2D(Patch.GetTranslation())));
            }
            for (const FTransform& Patch : ExistingSoil)
            {
                MinimumExistingTreeBasePatchCenterDistanceCm = FMath::Min(
                    MinimumExistingTreeBasePatchCenterDistanceCm,
                    FVector2D::Distance(
                        FVector2D(Transform.GetTranslation()),
                        FVector2D(Patch.GetTranslation())));
            }
        }
    }
    int32 MaximumObservedReuseCount = 0;
    int32 ReusedSourceCount = 0;
    for (const TPair<int32, int32>& Row : GrassReuseCountBySource)
    {
        MaximumObservedReuseCount = FMath::Max(
            MaximumObservedReuseCount,
            Row.Value);
        ReusedSourceCount += Row.Value > 1 ? 1 : 0;
    }
    TestTrue(TEXT("R23 source-grid mapping remains exact"),
        bGrassSourceMappingValid);
    TestTrue(
        TEXT("R23 uses at most two deterministic layers and exercises its second layer"),
        MaximumObservedReuseCount <= MaximumGrassReuseLayers &&
            ReusedSourceCount > 0 &&
            GrassReuseCountBySource.Num() < WetA.GrassTotal());
    TestTrue(
        TEXT("R23 reused carriers use exact golden-angle yaw decorrelation"),
        bGoldenAngleLayerDecorrelationExact);
    TestTrue(
        TEXT("R23 yaw spans the full hashed circle"),
        MaximumGrassYawDegrees - MinimumGrassYawDegrees > 350.0);
    TestTrue(
        TEXT("R23 terrain reprojection preserves source surface delta and adds only 0.01..0.05 cm"),
        MinimumPlacementGapCm >= GrassPlacementGapMinimumCm - 0.0001 &&
            MaximumPlacementGapCm <= GrassPlacementGapMaximumCm + 0.0001 &&
            MaximumPlacementGapCm - MinimumPlacementGapCm > 0.035);
    TestTrue(
        TEXT("R23 grass keeps bounded separated horizontal coverage variation"),
        MinimumGrassCoverageScale >=
                0.55 * GrassCoverageScaleMin * GrassAnisotropyScaleMin -
                    0.001 &&
            MaximumGrassCoverageScale <=
                0.55 * GrassCoverageScaleMax * GrassAnisotropyScaleMax +
                    0.001 &&
            MaximumGrassCoverageScale / MinimumGrassCoverageScale > 1.20);
    TestTrue(
        TEXT("R23 grass keeps the exact final bounded anisotropy ratio"),
        bGrassAnisotropyRatioIsBounded);
    TestTrue(
        TEXT("R23 grass uses the exact shared 18..42 cm offset band"),
        bGrassReuseLayerRadialBandsAreExact);
    TestTrue(
        TEXT("R23 grass has three-class bounded upright silhouette variation"),
        MinimumGrassHeightScale >=
                0.55 * GrassHeightScaleMin * 0.88 *
                        GrassShortHeightClassScale -
                    0.001 &&
            MaximumGrassHeightScale <=
                0.55 * GrassHeightScaleMax * 1.10 *
                        GrassTallHeightClassScale +
                    0.001 &&
            MaximumGrassHeightScale / MinimumGrassHeightScale > 1.35);
    TestTrue(
        TEXT("R23 grass carrier footprints remain outside supplemental patches"),
        MinimumSupplementalPatchCenterDistanceCm >=
            GrassPatchSuppressionRadiusCm -
                (GrassLayerOffsetMinimumRadiusCm +
                    GrassLayerOffsetBandWidthCm) -
                0.001);
    TestTrue(
        TEXT("R23 grass carrier footprints remain outside inherited V5B mulch"),
        MinimumExistingTreeBasePatchCenterDistanceCm >=
            ExistingTreeBasePatchSourceRadiusCm *
                    TreeBaseGrassSuppressionRadiusFraction +
                TreeBaseGrassCarrierFootprintMarginCm -
                (GrassLayerOffsetMinimumRadiusCm +
                    GrassLayerOffsetBandWidthCm) -
                0.001);
    bool bHeroClear = true;
    for (const FTransform& Transform : WetA.SupplementalSoil)
    {
        bHeroClear = bHeroClear &&
            !IsInsideHeroLawn(Transform.GetTranslation());
    }
    for (const FTransform& Transform : WetA.EdgeGrass)
    {
        bHeroClear = bHeroClear &&
            !IsInsideHeroLawn(Transform.GetTranslation());
    }
    TestTrue(TEXT("Hero lawn excludes soil and tall edge grass"), bHeroClear);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DGroundVegetationTerrainSeamPolicyTest,
    "TRIAD.Istana.ExploreV5D.GroundVegetation.TerrainSeamPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DGroundVegetationTerrainSeamPolicyTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestTrue(
        TEXT("Provider clip center remains exact"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedProviderSiteClipCenterMeters().Equals(
                FVector2D(0.0, 55.0),
                0.000001));
    TestTrue(
        TEXT("Provider clip semi-axes remain compact and exact"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedProviderSiteClipSemiAxesMeters().Equals(
                FVector2D(185.0, 245.0),
                0.000001));
    TestEqual(
        TEXT("Provider clip remains the exact 64-point polygon"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedProviderSiteClipSplinePoints(),
        64);
    TestEqual(
        TEXT("Opaque authored seam collar remains exactly 50 m"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayOpaqueCollarMeters(),
        50.0);
    TestEqual(
        TEXT("Coverage-safe dither transition remains eight metres"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayOutwardFeatherMeters(),
        8.0);
    TestEqual(
        TEXT("Dither cell remains stable world-space 25 cm"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayDitherCellMeters(),
        0.25);

    const FVector2D ClipCenter =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipCenterCentimeters();
    const FVector2D BoundaryPoint =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipPointCentimeters(0);
    const FVector2D OutwardRay = (BoundaryPoint - ClipCenter).GetSafeNormal();
    const auto BruteForceBoundaryRayDistanceCm = [&ClipCenter](
        const FVector2D& UnitRay,
        double& OutDistanceCm) -> bool
    {
        OutDistanceCm = TNumericLimits<double>::Max();
        const int32 PointCount =
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedProviderSiteClipSplinePoints();
        for (int32 EdgeIndex = 0; EdgeIndex < PointCount; ++EdgeIndex)
        {
            const FVector2D P0 =
                ATRIADIstanaExploreV5DContextPolicyActor::
                    ExpectedProviderSiteClipPointCentimeters(EdgeIndex) -
                ClipCenter;
            const FVector2D P1 =
                ATRIADIstanaExploreV5DContextPolicyActor::
                    ExpectedProviderSiteClipPointCentimeters(EdgeIndex + 1) -
                ClipCenter;
            const FVector2D Edge = P1 - P0;
            const double Denominator =
                UnitRay.X * Edge.Y - UnitRay.Y * Edge.X;
            if (FMath::Abs(Denominator) <= UE_DOUBLE_SMALL_NUMBER)
            {
                continue;
            }
            const double SegmentParameter =
                (P0.X * UnitRay.Y - P0.Y * UnitRay.X) / Denominator;
            const double RayDistance =
                (P0.X * Edge.Y - P0.Y * Edge.X) / Denominator;
            if (RayDistance > 0.0 && SegmentParameter >= 0.0 &&
                SegmentParameter <= 1.0)
            {
                OutDistanceCm = FMath::Min(OutDistanceCm, RayDistance);
            }
        }
        return FMath::IsFinite(OutDistanceCm) &&
            OutDistanceCm < TNumericLimits<double>::Max();
    };

    const double FortyFiveRadians = FMath::DegreesToRadians(45.0);
    const FVector2D FortyFiveRay(
        FMath::Cos(FortyFiveRadians),
        FMath::Sin(FortyFiveRadians));
    double FortyFiveBoundaryCm = 0.0;
    const bool bFoundFortyFiveBoundary = BruteForceBoundaryRayDistanceCm(
        FortyFiveRay,
        FortyFiveBoundaryCm);
    TestTrue(
        TEXT("Brute-force 45-degree ray intersects the exact linear polygon"),
        bFoundFortyFiveBoundary);
    TestTrue(
        TEXT("Normalized ellipse-angle bracket matches the exact 45-degree edge"),
        bFoundFortyFiveBoundary && FMath::IsNearlyZero(
            ATRIADIstanaExploreV5DContextPolicyActor::
                EvaluateProviderSiteClipSignedInwardDistanceCentimeters(
                    ClipCenter + FortyFiveRay * FortyFiveBoundaryCm),
            0.001));

    bool bSweepFoundEveryBoundary = true;
    double MaximumSweepSignedDistanceErrorCm = 0.0;
    double MaximumSweepBoundaryCoverageError = 0.0;
    for (int32 Sample = 0; Sample < 1440; ++Sample)
    {
        const double AngleRadians = FMath::DegreesToRadians(
            static_cast<double>(Sample) * 0.25);
        const FVector2D Ray(
            FMath::Cos(AngleRadians),
            FMath::Sin(AngleRadians));
        double ExactBoundaryCm = 0.0;
        if (!BruteForceBoundaryRayDistanceCm(Ray, ExactBoundaryCm))
        {
            bSweepFoundEveryBoundary = false;
            break;
        }
        const FVector2D ExactBoundaryLocation =
            ClipCenter + Ray * ExactBoundaryCm;
        MaximumSweepSignedDistanceErrorCm = FMath::Max(
            MaximumSweepSignedDistanceErrorCm,
            FMath::Abs(
                ATRIADIstanaExploreV5DContextPolicyActor::
                    EvaluateProviderSiteClipSignedInwardDistanceCentimeters(
                        ExactBoundaryLocation)));
        MaximumSweepBoundaryCoverageError = FMath::Max(
            MaximumSweepBoundaryCoverageError,
            FMath::Abs(
                1.0 - ATRIADIstanaExploreV5DGroundVegetationActor::
                    EvaluateGroundOverlayCoreCoverage(
                        ExactBoundaryLocation)));
    }
    TestTrue(
        TEXT("0.25-degree sweep finds the exact edge on every ray"),
        bSweepFoundEveryBoundary);
    TestTrue(
        TEXT("0.25-degree sweep matches brute-force edge selection within 0.001 cm"),
        MaximumSweepSignedDistanceErrorCm <= 0.001);
    TestTrue(
        TEXT("0.25-degree sweep keeps full coverage at every provider boundary"),
        MaximumSweepBoundaryCoverageError <= 0.000001);
    TestTrue(
        TEXT("Deep authored core is fully covered"),
        FMath::IsNearlyEqual(
            ATRIADIstanaExploreV5DGroundVegetationActor::
                EvaluateGroundOverlayCoreCoverage(ClipCenter),
            1.0,
            0.000001));
    TestTrue(
        TEXT("Exact provider boundary remains fully covered"),
        FMath::IsNearlyEqual(
            ATRIADIstanaExploreV5DGroundVegetationActor::
                EvaluateGroundOverlayCoreCoverage(BoundaryPoint),
            1.0,
            0.000001));
    TestTrue(
        TEXT("Shared linear polygon signed distance is zero at a vertex"),
        FMath::IsNearlyZero(
            ATRIADIstanaExploreV5DContextPolicyActor::
                EvaluateProviderSiteClipSignedInwardDistanceCentimeters(
                    BoundaryPoint),
            0.001));
    const FVector2D CollarOuterPoint = BoundaryPoint + OutwardRay *
        (RequiredGroundOverlayOpaqueCollarMeters * CentimetersPerMeter);
    TestTrue(
        TEXT("Authored overlay remains fully opaque through the 50 m collar"),
        FMath::IsNearlyEqual(
            ATRIADIstanaExploreV5DGroundVegetationActor::
                EvaluateGroundOverlayCoreCoverage(CollarOuterPoint),
            1.0,
            0.000001));
    const FVector2D MidFeatherPoint = BoundaryPoint + OutwardRay *
        ((RequiredGroundOverlayOpaqueCollarMeters +
            0.5 * RequiredGroundOverlayOutwardFeatherMeters) *
            CentimetersPerMeter);
    const double MidFeatherCoverage =
        ATRIADIstanaExploreV5DGroundVegetationActor::
            EvaluateGroundOverlayCoreCoverage(MidFeatherPoint);
    TestTrue(
        TEXT("Transition begins only after the full-opacity collar"),
        MidFeatherCoverage > 0.49 && MidFeatherCoverage < 0.51);
    const FVector2D BeyondFeatherPoint = BoundaryPoint + OutwardRay *
        ((RequiredGroundOverlayOpaqueCollarMeters +
            RequiredGroundOverlayOutwardFeatherMeters + 0.01) *
            CentimetersPerMeter);
    TestFalse(
        TEXT("Overlay never renders beyond the exact 58 m collar and feather"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            EvaluateGroundOverlayStableDitherMask(BeyondFeatherPoint));
    TestTrue(
        TEXT("Continuous overlay coverage is zero beyond the exact 58 m collar and feather"),
        FMath::IsNearlyZero(
            ATRIADIstanaExploreV5DGroundVegetationActor::
                EvaluateGroundOverlayCoreCoverage(BeyondFeatherPoint),
            0.000001));
    TestEqual(
        TEXT("World-space dither is deterministic"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            EvaluateGroundOverlayStableDitherMask(MidFeatherPoint),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            EvaluateGroundOverlayStableDitherMask(MidFeatherPoint));

    TestFalse(
        TEXT("Invalid policy cannot hide source terrain"),
        ShouldHideSourceTerrainRenderer(false, true, true));
    TestFalse(
        TEXT("Unbegun policy cannot hide source terrain"),
        ShouldHideSourceTerrainRenderer(true, false, true));
    TestFalse(
        TEXT("Visible fallback keeps source terrain visible"),
        ShouldHideSourceTerrainRenderer(true, true, false));
    TestTrue(
        TEXT("Only valid begun hidden-fallback state hides source terrain"),
        ShouldHideSourceTerrainRenderer(true, true, true));

    const ATRIADIstanaExploreV5DGroundVegetationActor* Defaults =
        GetDefault<ATRIADIstanaExploreV5DGroundVegetationActor>();
    TestTrue(
        TEXT("Owned overlay is movable for exact PIE transform reapply"),
        Defaults && Defaults->GroundMacroVariationOverlay &&
            Defaults->GroundMacroVariationOverlay->Mobility ==
                EComponentMobility::Movable &&
            Defaults->GroundMacroVariationOverlay->GetCollisionEnabled() ==
                ECollisionEnabled::NoCollision &&
            !Defaults->GroundMacroVariationOverlay->CanEverAffectNavigation());
    const UHierarchicalInstancedStaticMeshComponent* DefaultHisms[] = {
        Defaults ? Defaults->GrassManicuredInstances.Get() : nullptr,
        Defaults ? Defaults->GrassHumidInstances.Get() : nullptr,
        Defaults ? Defaults->GrassShadeInstances.Get() : nullptr,
        Defaults ? Defaults->GrassDryEdgeInstances.Get() : nullptr,
        Defaults ? Defaults->EdgeGrassInstances.Get() : nullptr,
        Defaults ? Defaults->SupplementalSoilInstances.Get() : nullptr,
        Defaults ? Defaults->SupplementalShrubInstances.Get() : nullptr,
        Defaults ? Defaults->SupplementalUnderstoreyInstances.Get() : nullptr,
        Defaults ? Defaults->SupplementalFlowerInstances.Get() : nullptr};
    bool bDefaultHismPoliciesValid = true;
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         DefaultHisms)
    {
        bDefaultHismPoliciesValid = bDefaultHismPoliciesValid && Component &&
            Component->Mobility == EComponentMobility::Static &&
            !Component->bAutoRebuildTreeOnInstanceChanges &&
            Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
            !Component->CanEverAffectNavigation();
    }
    TestTrue(
        TEXT("Owned HISMs retain static render-only explicit-build policy"),
        bDefaultHismPoliciesValid);
    const UHierarchicalInstancedStaticMeshComponent* DefaultGrassHisms[] = {
        Defaults ? Defaults->GrassManicuredInstances.Get() : nullptr,
        Defaults ? Defaults->GrassHumidInstances.Get() : nullptr,
        Defaults ? Defaults->GrassShadeInstances.Get() : nullptr,
        Defaults ? Defaults->GrassDryEdgeInstances.Get() : nullptr};
    bool bDefaultGrassCullPolicyValid = true;
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         DefaultGrassHisms)
    {
        int32 StartCullDistance = 0;
        int32 EndCullDistance = 0;
        if (Component)
        {
            Component->GetCullDistances(StartCullDistance, EndCullDistance);
        }
        bDefaultGrassCullPolicyValid = bDefaultGrassCullPolicyValid &&
            Component && StartCullDistance == R14GrassCullStartDistanceCm &&
            EndCullDistance == R14GrassCullEndDistanceCm &&
            Component->WorldPositionOffsetDisableDistance ==
                R14GrassWpoDisableDistanceCm &&
            FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                R14GrassLodDistanceScale,
                0.0001f);
    }
    TestTrue(
        TEXT("R14 CDO keeps the exact cold predecessor grass policy for migration admission"),
        bDefaultGrassCullPolicyValid);
    int32 DefaultEdgeCullStart = 0;
    int32 DefaultEdgeCullEnd = 0;
    if (Defaults && Defaults->EdgeGrassInstances)
    {
        Defaults->EdgeGrassInstances->GetCullDistances(
            DefaultEdgeCullStart,
            DefaultEdgeCullEnd);
    }
    TestTrue(
        TEXT("R14 CDO keeps the exact cold predecessor edge-grass policy for migration admission"),
        Defaults && Defaults->EdgeGrassInstances &&
            DefaultEdgeCullStart == R14EdgeGrassCullStartDistanceCm &&
            DefaultEdgeCullEnd == R14EdgeGrassCullEndDistanceCm &&
            Defaults->EdgeGrassInstances->WorldPositionOffsetDisableDistance ==
                R14EdgeGrassWpoDisableDistanceCm &&
            FMath::IsNearlyEqual(
                Defaults->EdgeGrassInstances->InstanceLODDistanceScale,
                R14GrassLodDistanceScale,
                0.0001f));
    TestTrue(
        TEXT("R20 predecessor and R23 target policies remain explicit and distinct from the R14 CDO"),
        R20GrassCullStartDistanceCm == 2400 &&
            R20GrassCullEndDistanceCm == 3800 &&
            R20GrassWpoDisableDistanceCm == 2400 &&
            R20EdgeGrassCullStartDistanceCm == 3000 &&
            R20EdgeGrassCullEndDistanceCm == 4500 &&
            R20EdgeGrassWpoDisableDistanceCm == 2400 &&
            FMath::IsNearlyEqual(
                R20GrassLodDistanceScale,
                0.60f,
                0.0001f) &&
            GrassCullStartDistanceCm == 2600 &&
            GrassCullEndDistanceCm == 3400 &&
            GrassWpoDisableDistanceCm == 2400 &&
            EdgeGrassCullStartDistanceCm == 3000 &&
            EdgeGrassCullEndDistanceCm == 4500 &&
            EdgeGrassWpoDisableDistanceCm == 2400 &&
            FMath::IsNearlyEqual(GrassLodDistanceScale, 0.60f, 0.0001f) &&
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGrassPresentationRevision() ==
                GrassPresentationRevisionR23 &&
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedR23PredecessorGrassPresentationRevision() ==
                GrassPresentationRevisionR20);

    UStaticMeshComponent* Terrain = NewObject<UStaticMeshComponent>(
        GetTransientPackage());
    Terrain->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Terrain->SetVisibility(true, true);
    Terrain->SetHiddenInGame(false, true);
    TestTrue(
        TEXT("Provider-ready transition hides only the renderer"),
        ApplySourceTerrainRendererVisibility(Terrain, false));
    TestFalse(TEXT("Renderer hidden for provider"), Terrain->IsVisible());
    TestTrue(TEXT("Hidden-in-game agrees"), Terrain->bHiddenInGame);
    TestEqual(
        TEXT("Collision remains live while renderer is hidden"),
        Terrain->GetCollisionEnabled(),
        ECollisionEnabled::QueryAndPhysics);
    TestTrue(
        TEXT("Fallback/EndPlay transition restores renderer"),
        ApplySourceTerrainRendererVisibility(Terrain, true));
    TestTrue(TEXT("Renderer restored"), Terrain->IsVisible());
    TestFalse(TEXT("Hidden-in-game restored"), Terrain->bHiddenInGame);
    TestEqual(
        TEXT("Collision remains live after restore"),
        Terrain->GetCollisionEnabled(),
        ECollisionEnabled::QueryAndPhysics);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DGrassMaterialRuntimeRevisionTest,
    "TRIAD.Istana.ExploreV5D.GroundVegetation.GrassMaterialRuntimeRevision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DGrassMaterialRuntimeRevisionTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    const TArray<float> R19Roughness = {0.70f, 0.66f, 0.72f, 0.76f};
    const TArray<float> R19Specular = {0.30f, 0.32f, 0.28f, 0.26f};
    const TArray<float> R21Roughness = {0.68f, 0.66f, 0.72f, 0.75f};
    const TArray<float> R21Specular = {0.30f, 0.31f, 0.28f, 0.26f};
    const TArray<float> AnimatedWind = {0.48f, 0.48f, 0.48f, 0.48f};
    const TArray<float> StableWind = {0.0f, 0.0f, 0.0f, 0.0f};
    const TArray<float> NoRevision = {0.0f, 0.0f, 0.0f, 0.0f};
    const TArray<float> R22Revision = {22.0f, 22.0f, 22.0f, 22.0f};
    const TArray<float> R23Revision = {23.0f, 23.0f, 23.0f, 23.0f};
    const TArray<float> R23BRoughness = {0.71f, 0.70f, 0.75f, 0.78f};
    const TArray<float> R23BSpecular = {0.27f, 0.28f, 0.25f, 0.23f};
    const TArray<float> R23BCalibration = {1.0f, 1.0f, 1.0f, 1.0f};
    TestEqual(
        TEXT("Exact R21 runtime parameter signature is admitted"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R21Roughness, R21Specular, AnimatedWind, NoRevision)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::R21));
    TestEqual(
        TEXT("Exact backward R19 runtime parameter signature is admitted"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R19Roughness, R19Specular, AnimatedWind, NoRevision)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::R19));
    TestEqual(
        TEXT("Exact uniform R22 runtime parameter signature is admitted"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R21Roughness, R21Specular, StableWind, R22Revision)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::R22));
    TestEqual(
        TEXT("Exact uniform R23 runtime parameter signature is admitted"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R21Roughness, R21Specular, StableWind, R23Revision)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::R23));
    TestEqual(
        TEXT("Exact uniform R23B runtime parameter signature is admitted"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R23BRoughness,
            R23BSpecular,
            StableWind,
            R23Revision,
            R23BCalibration)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::R23B));
    TestEqual(
        TEXT("Mixed R23/R23B calibration marker roster is rejected"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R23BRoughness,
            R23BSpecular,
            StableWind,
            R23Revision,
            TArray<float>{1.0f, 1.0f, 0.0f, 1.0f})),
        static_cast<int32>(EGrassMaterialRuntimeRevision::Unsupported));
    TestEqual(
        TEXT("R23B roughness/specular without calibration marker is rejected"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R23BRoughness,
            R23BSpecular,
            StableWind,
            R23Revision,
            NoRevision)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::Unsupported));
    TestEqual(
        TEXT("Zero-wind R21 without a revision marker is rejected"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R21Roughness, R21Specular, StableWind, NoRevision)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::Unsupported));
    TestEqual(
        TEXT("Mixed R19/R21 runtime parameter signature is rejected"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R21Roughness, R19Specular, AnimatedWind, NoRevision)),
        static_cast<int32>(EGrassMaterialRuntimeRevision::Unsupported));
    TestEqual(
        TEXT("Mixed R22/R23 marker roster is rejected"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R21Roughness,
            R21Specular,
            StableWind,
            TArray<float>{23.0f, 23.0f, 22.0f, 23.0f})),
        static_cast<int32>(EGrassMaterialRuntimeRevision::Unsupported));
    TestEqual(
        TEXT("Wrong-size R22 marker roster is rejected"),
        static_cast<int32>(ClassifyGrassMaterialRuntimeSignature(
            R21Roughness,
            R21Specular,
            StableWind,
            TArray<float>{22.0f, 22.0f, 22.0f})),
        static_cast<int32>(EGrassMaterialRuntimeRevision::Unsupported));
    return true;
}
#endif
