#include "TRIADIstanaExploreV5BVisualActor.h"

#include "Components/BillboardComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#endif
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TRIADIstanaExploreV2LandscapeActor.h"
#include "TRIADIstanaExploreV3SupplementActor.h"
#include "TRIADIstanaExploreV4LandscapeActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DFountainRealismActor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogTRIADIstanaExploreV5BVisuals, Log, All);

namespace
{
const FString V5BClaimLabel(
    TEXT("ISTANA_EXPLORE_V5B_RENDER_ONLY_PUBLIC_DATA_VISUAL_APPROXIMATION_NOT_ONE_TO_ONE_NOT_SURVEY_NOT_AS_BUILT_NOT_BOTANICAL_INVENTORY_NOT_SENSOR_OR_RF_TRUTH"));
const FString V5DPhotographicToneMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));

constexpr int32 CloseTurfSourceCount = 18432;
constexpr int32 AccentTurfCount = 18432;
constexpr int32 AccentSourceMultiplier = 101;
constexpr int32 AccentSourceOffset = 37;
constexpr int32 RuntimeFacadeMaterialCount = 6;
constexpr int32 InheritedV4RenderSourceCount = 5;
constexpr int32 LegacyGroundPlantRenderSourceCount = 8;
constexpr int32 InheritedRenderSourceCount =
    InheritedV4RenderSourceCount + LegacyGroundPlantRenderSourceCount;
constexpr int32 LegacyGroundPlantInstanceCount = 2096;
// Keep the inherited V5B authored/default value intact.  Only the exact V5D
// runtime presentation selects the reduced photographic-look fill below.
constexpr float DefaultFacadeFillIntensityLux = 2500.0f;
constexpr float V5DPhotographicFacadeFillIntensityLux = 1100.0f;
const FName FacadeTintParameter(TEXT("LookdevTint"));
const FName FacadeRoughnessParameter(TEXT("RoughnessBias"));

bool IsExactV5DPhotographicToneWorld(const UWorld* World)
{
    return World && World->GetOutermost() &&
        UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) ==
            V5DPhotographicToneMapPackage;
}

float ExpectedFacadeFillIntensityLux(
    const UWorld* World,
    bool bRuntimeFacadePresentationApplied)
{
    return bRuntimeFacadePresentationApplied &&
            IsExactV5DPhotographicToneWorld(World)
        ? V5DPhotographicFacadeFillIntensityLux
        : DefaultFacadeFillIntensityLux;
}

struct FRuntimeFacadeMaterialSpec
{
    int32 Slot;
    FLinearColor Tint;
    float RoughnessBias;
};

const FRuntimeFacadeMaterialSpec RuntimeFacadeMaterialSpecs[] = {
    {1, FLinearColor(0.620000f, 0.650000f, 0.700000f), 0.12f},
    {6, FLinearColor(0.840000f, 0.820000f, 0.780000f), 0.14f},
    {7, FLinearColor(0.780000f, 0.800000f, 0.820000f), 0.16f},
    {8, FLinearColor(0.600000f, 0.680000f, 0.780000f), 0.15f},
    {9, FLinearColor(0.780000f, 0.770000f, 0.730000f), 0.16f},
    {10, FLinearColor(0.900000f, 0.880000f, 0.840000f), 0.12f}};
constexpr int32 FormalBedShrubSourceCount = 512;
constexpr int32 FormalBedFlowerSourceCount = 192;
constexpr int32 FormalBedUnderstoreySourceCount = 384;
constexpr int32 FormalBedShrubInfillCount = 240;
constexpr int32 FormalBedFlowerInfillCount = 720;
constexpr int32 FormalBedUnderstoreyInfillCount = 1920;
constexpr int32 FormalBedShrubRenderCount =
    FormalBedShrubSourceCount + FormalBedShrubInfillCount;
constexpr int32 FormalBedFlowerRenderCount =
    FormalBedFlowerSourceCount + FormalBedFlowerInfillCount;
constexpr int32 FormalBedUnderstoreyRenderCount =
    FormalBedUnderstoreySourceCount + FormalBedUnderstoreyInfillCount;
// R9 relocates the complete borrowed planting roster into the two formal beds.
// R8 retained the suffix at its V4 flank transforms, which read as hundreds of
// isolated weeds across the lawn in the fixed acceptance views.
constexpr int32 FormalBedShrubRelocatedSourceCount =
    FormalBedShrubSourceCount;
constexpr int32 FormalBedFlowerRelocatedSourceCount =
    FormalBedFlowerSourceCount;
constexpr int32 FormalBedUnderstoreyRelocatedSourceCount =
    FormalBedUnderstoreySourceCount;
constexpr int32 FormalBedRelocatedSourcePlantCount =
    FormalBedShrubRelocatedSourceCount +
    FormalBedFlowerRelocatedSourceCount +
    FormalBedUnderstoreyRelocatedSourceCount;
constexpr int32 FormalBedPreservedFlankPlantCount =
    (FormalBedShrubSourceCount - FormalBedShrubRelocatedSourceCount) +
    (FormalBedFlowerSourceCount - FormalBedFlowerRelocatedSourceCount) +
    (FormalBedUnderstoreySourceCount -
     FormalBedUnderstoreyRelocatedSourceCount);
constexpr int32 FormalBedRenderOnlyInfillPlantCount =
    FormalBedShrubInfillCount + FormalBedFlowerInfillCount +
    FormalBedUnderstoreyInfillCount;
constexpr double FormalBedCenterAbsXCm = 3400.0;
constexpr double FormalBedCenterYCm = 8800.0;
// The low planting reaches beyond the exact 1400 x 900 cm collision core far
// enough to cover the inner apron, while remaining comfortably inside the
// minimum 304 cm generated veneer drape. The rounded mapping below keeps the
// diagonal placements inside that conservative envelope as well.
constexpr double FormalBedPlantHalfSizeXCm = 1540.0;
constexpr double FormalBedPlantHalfSizeYCm = 1000.0;
constexpr double FormalBedRoundedMappingCoefficient = 0.38;
constexpr double FormalBedOuterPlantBandMinimumRadius = 0.66;
constexpr double FormalBedCoreHalfSizeXCm = 1400.0;
constexpr double FormalBedCoreHalfSizeYCm = 900.0;
constexpr double FormalBedMinimumApronWidthCm = 304.0;
constexpr double FormalBedConservativeVeneerExponent = 2.65;
constexpr double FormalBedCoreDatumZCm = 39.7194726;
constexpr double FormalBedMaximumReliefCm = 4.8;
constexpr double FormalBedRootInsetCm = 2.0;
constexpr double FormalBedTurfSuppressionHalfSizeXCm = 1610.0;
constexpr double FormalBedTurfSuppressionHalfSizeYCm = 1110.0;
constexpr double FormalBedTurfSuppressionDepthCm = 10.0;
constexpr int32 TreeGroundingAnchorCount = 64;
constexpr int32 TreeGroundingAnchorsPerSide = TreeGroundingAnchorCount / 2;
constexpr int32 TreeGroundingPlantedAnchorCount = 24;
constexpr int32 TreeGroundingPlantedAnchorsPerSide =
    TreeGroundingPlantedAnchorCount / 2;
constexpr int32 TreeGroundingShrubsPerPlantedAnchor = 2;
constexpr int32 TreeGroundingUnderstoreyPerPlantedAnchor = 6;
constexpr int32 TreeGroundingShrubCount =
    TreeGroundingPlantedAnchorCount * TreeGroundingShrubsPerPlantedAnchor;
constexpr int32 TreeGroundingUnderstoreyCount =
    TreeGroundingPlantedAnchorCount * TreeGroundingUnderstoreyPerPlantedAnchor;
constexpr int32 ExpectedV4MainTreeCount = 720;
constexpr int32 ExpectedV4HeritageTreeCount = 9;
constexpr int32 ExpectedV4TreeSourceCount =
    ExpectedV4MainTreeCount + ExpectedV4HeritageTreeCount;
constexpr double TreeGroundingMinimumAbsXCm = 5200.0;
constexpr double TreeGroundingMaximumAbsXCm = 17000.0;
constexpr double TreeGroundingMinimumYCm = 3500.0;
constexpr double TreeGroundingMaximumYCm = 22000.0;
constexpr double TreeGroundingDuplicateRootToleranceCm = 75.0;
constexpr double TreeBaseRadiusHeightRatio = 0.043;
constexpr double TreeBaseRadiusMinCm = 110.0;
constexpr double TreeBaseRadiusMaxCm = 180.0;
constexpr double TreeGroundingMinimumMulchEdgeClearanceCm = 90.0;
// Preserve the R19 source-selection guard exactly even though the smaller R20
// patches now exceed the minimum requested edge clearance by a wider margin.
constexpr double TreeGroundingMinimumSpacingCm = 550.0;
constexpr double TreeBasePatchSourceRadiusCm = 100.0;
constexpr double TreeBasePatchZOffsetCm = 0.6;
// Frozen persisted V5B layout compatibility. V5D owns the wider R14 carrier
// exclusion without mutating this source map's deterministic transforms.
constexpr double TreeBaseGrassSuppressionRadiusFraction = 0.84;
constexpr int32 TreeBasePlantForcedLodModel = 1;
constexpr double TreeBaseShrubRootInsetCm = 3.0;
constexpr double TreeBaseUnderstoreyRootInsetCm = 4.5;
constexpr double TreeBaseShrubMinimumRadialFraction = 0.42;
constexpr double TreeBaseShrubMaximumRadialFraction = 0.56;
constexpr double TreeBaseShrubMinimumUniformScale = 1.35;
constexpr double TreeBaseShrubMaximumUniformScale = 1.85;
constexpr double TreeBaseShrubHorizontalScaleMultiplier = 0.72;
constexpr double TreeBaseUnderstoreyMinimumRadialFraction = 0.44;
constexpr double TreeBaseUnderstoreyMaximumRadialFraction = 0.58;
constexpr double TreeBaseUnderstoreyMinimumUniformScale = 1.15;
constexpr double TreeBaseUnderstoreyMaximumUniformScale = 1.60;
constexpr double TreeBaseUnderstoreyHorizontalScaleMultiplier = 0.60;
constexpr double TreeBaseUnderstoreyTriadAngleStepRadians = 0.14;
static_assert(TreeGroundingAnchorCount % 2 == 0);
static_assert(TreeGroundingPlantedAnchorCount % 2 == 0);
static_assert(TreeGroundingMinimumSpacingCm == 550.0);
static_assert(TreeBaseGrassSuppressionRadiusFraction == 0.84);
static_assert(
    TreeGroundingMinimumSpacingCm >=
    2.0 * TreeBaseRadiusMaxCm +
        TreeGroundingMinimumMulchEdgeClearanceCm);
const FName V5BRenderSuccessorTag(TEXT("TRIADIstanaExploreV5BRenderSuccessor"));
const FString ExpectedSourceHardscapeObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Ground/SM_IstanaPublicView_Hardscape.SM_IstanaPublicView_Hardscape"));
const FName ExpectedHardscapeMaterialSlotNames[] = {
    FName(TEXT("M_IPV_Stone")),
    FName(TEXT("M_IPV_Water")),
    FName(TEXT("M_IPV_Metal"))};
const FName ExpectedHardscapePlantingSlotName(TEXT("M_IPV_Planting"));
constexpr double AccentLawnMinimumXCm = -9600.0;
constexpr double AccentLawnMaximumXCm = 9600.0;
constexpr double AccentLawnMinimumYCm = 800.0;
constexpr double AccentLawnMaximumYCm = 22000.0;
constexpr double AccentTerrainGapCm = 0.15;
// Irrational increments from the two-dimensional R2 sequence give every
// prefix a spatially even, non-lattice distribution. Rejected points advance
// the sequence without changing the exact ordered source-provenance roster.
constexpr double AccentR2IncrementX = 0.7548776662466927;
constexpr double AccentR2IncrementY = 0.5698402909980532;
constexpr double AccentBaseYawDegrees = 47.0;
constexpr double AccentYawJitterDegrees = 33.0;
// The deterministic R11 carrier groups 680 unique blade roots around 272 R2
// pair/triad tuft centres inside the frozen 160 x 140 cm authored footprint.
// Keep it near source scale so the fine-turf proxy fills the mean sample.
constexpr double AccentMinXScale = 0.86;
constexpr double AccentMaxXScale = 1.14;
constexpr double AccentMinYScale = 0.82;
constexpr double AccentMaxYScale = 1.18;
// The serialized map's instance transforms were authored against R8's 4.8 cm
// carrier. Keep that denominator frozen in this asset-only migration so no map
// transform changes are required. It is compatibility state, not current
// source-height evidence: R11's exact 4.4 cm source tip is presented at
// 1.833..2.933 cm.
constexpr double AccentFrozenTransformHeightDenominatorCm = 4.8;
constexpr double AccentMinZScale =
    2.0 / AccentFrozenTransformHeightDenominatorCm;
constexpr double AccentMaxZScale =
    3.2 / AccentFrozenTransformHeightDenominatorCm;

double SmoothStep01(double Value)
{
    const double Unit = FMath::Clamp(Value, 0.0, 1.0);
    return Unit * Unit * (3.0 - 2.0 * Unit);
}

double TreeBaseMoundSurfaceZCm(double RadialFraction)
{
    const double Radius = FMath::Clamp(RadialFraction, 0.0, 1.0);
    if (Radius <= 0.30)
    {
        return FMath::Lerp(0.70, 1.80, Radius / 0.30);
    }
    if (Radius <= 0.65)
    {
        return FMath::Lerp(1.80, 0.90, (Radius - 0.30) / 0.35);
    }
    if (Radius <= 0.88)
    {
        return FMath::Lerp(0.90, 0.20, (Radius - 0.65) / 0.23);
    }
    return FMath::Lerp(0.20, -0.45, (Radius - 0.88) / 0.12);
}

FVector2D OrganicFormalBedLocalXY(double U, double V)
{
    const double MappedU = U * FMath::Sqrt(FMath::Max(
        0.0,
        1.0 - FormalBedRoundedMappingCoefficient * V * V));
    const double MappedV = V * FMath::Sqrt(FMath::Max(
        0.0,
        1.0 - FormalBedRoundedMappingCoefficient * U * U));
    const double Angle = FMath::Atan2(MappedV, MappedU);
    const double EdgeWeight = FMath::Pow(
        FMath::Max(FMath::Abs(U), FMath::Abs(V)),
        2.35);
    const double Scallop = 1.0 + EdgeWeight * (
        0.026 * FMath::Sin(5.0 * Angle + 0.63) +
        0.015 * FMath::Cos(8.0 * Angle - 0.21) +
        0.009 * FMath::Sin(13.0 * Angle + 1.17));
    return FVector2D(
        FormalBedPlantHalfSizeXCm * MappedU * Scallop,
        FormalBedPlantHalfSizeYCm * MappedV * Scallop);
}

bool IsInsideConservativeFormalBedVeneer(const FVector& Location)
{
    const double LocalX = FMath::Abs(
        FMath::Abs(Location.X) - FormalBedCenterAbsXCm);
    const double LocalY = FMath::Abs(Location.Y - FormalBedCenterYCm);
    const double NormalizedX = LocalX /
        (FormalBedCoreHalfSizeXCm + FormalBedMinimumApronWidthCm);
    const double NormalizedY = LocalY /
        (FormalBedCoreHalfSizeYCm + FormalBedMinimumApronWidthCm);
    return FMath::Pow(NormalizedX, FormalBedConservativeVeneerExponent) +
            FMath::Pow(NormalizedY, FormalBedConservativeVeneerExponent) <=
        1.0;
}

double FormalBedReliefCm(double U, double V)
{
    const double Signal =
        0.38 * FMath::Sin(7.31 * U + 5.17 * V + 0.37) +
        0.27 * FMath::Sin(15.13 * U - 9.71 * V + 1.11) +
        0.21 * FMath::Cos(24.97 * U + 17.39 * V - 0.73) +
        0.14 * FMath::Sin(39.17 * U - 31.07 * V + 2.03);
    const double Unit = FMath::Clamp(0.5 * (Signal + 1.0), 0.0, 1.0);
    const double EdgeDistance = 1.0 - FMath::Max(FMath::Abs(U), FMath::Abs(V));
    return FormalBedMaximumReliefCm * Unit * SmoothStep01(EdgeDistance / 0.24);
}

constexpr double FountainCenterXCm = 0.0;
constexpr double FountainCenterYCm = 9500.0;
constexpr double FountainSurfaceZCm = 117.0194726;
constexpr double FountainFoamZCm = 117.4194726;
constexpr double OuterPlumeRadiusCm = 560.0;
constexpr double OuterPlumeOriginZCm = 117.2194726;
constexpr double CentralPlumeOriginZCm = 215.2194726;
constexpr int32 OuterPlumeCount = 12;
constexpr int32 ImpactRingCount = 12;

// Five staggered 54 cm radial courses replace the two 1.5 m "pizza-slice"
// bands. The two HISM rosters alternate rings while sharing only two meshes.
constexpr int32 InnerPaverCount = 112 + 128 + 144;
constexpr int32 OuterPaverCount = 120 + 136;
constexpr double PaverZCm = 37.8194726;

constexpr int32 PachiraVariantCount = 4;
constexpr int32 PachiraInstancesPerVariant = 12;
constexpr int32 LogicalPachiraCount =
    PachiraVariantCount * PachiraInstancesPerVariant;
constexpr double PachiraRootInsetCm = 1.5;

const FName ExpectedCloseTurfComponentName(
    TEXT("V4AnimatedCloseTurfGeometryCardReplacement"));

uint32 MixStableHash(uint32 Value)
{
    Value ^= Value >> 16;
    Value *= 0x7FEB352Du;
    Value ^= Value >> 15;
    Value *= 0x846CA68Bu;
    Value ^= Value >> 16;
    return Value;
}

double HashUnitInterval(uint32 Value)
{
    return static_cast<double>(MixStableHash(Value) & 0x00FFFFFFu) /
        static_cast<double>(0x00FFFFFFu);
}

double FractionalPart(double Value)
{
    return Value - FMath::FloorToDouble(Value);
}

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

bool IsInsideAccentLawnExclusion(double XCm, double YCm)
{
    const bool bBuildingOrApron =
        YCm < 1400.0 && FMath::Abs(XCm) < 8200.0;
    const bool bEntranceWalk =
        FMath::Abs(XCm) < 320.0 && YCm < 7000.0;
    const bool bFountainOrDrive = FVector2D(
        XCm,
        YCm - FountainCenterYCm).SizeSquared() < FMath::Square(2500.0);
    return bBuildingOrApron || bEntranceWalk || bFountainOrDrive;
}

bool IsInsideOrganicFormalBedSuppression(double XCm, double YCm)
{
    const double BedCenterXCm = XCm < 0.0
        ? -FormalBedCenterAbsXCm
        : FormalBedCenterAbsXCm;
    const double U =
        (XCm - BedCenterXCm) / FormalBedTurfSuppressionHalfSizeXCm;
    const double V =
        (YCm - FormalBedCenterYCm) / FormalBedTurfSuppressionHalfSizeYCm;
    const double Angle = FMath::Atan2(V, U);
    const double Boundary = FMath::Max(
        0.82,
        1.0 +
            0.055 * FMath::Sin(5.0 * Angle + 0.63) +
            0.025 * FMath::Cos(8.0 * Angle - 0.21) +
            0.012 * FMath::Sin(13.0 * Angle + 1.17));
    constexpr double SuperellipseExponent = 2.65;
    return FMath::Pow(FMath::Abs(U), SuperellipseExponent) +
            FMath::Pow(FMath::Abs(V), SuperellipseExponent) <=
        FMath::Pow(Boundary, SuperellipseExponent);
}

bool IsFinitePositiveTransform(const FTransform& Transform)
{
    const FVector Scale = Transform.GetScale3D();
    return !Transform.ContainsNaN() &&
        FMath::IsFinite(Scale.X) &&
        FMath::IsFinite(Scale.Y) &&
        FMath::IsFinite(Scale.Z) &&
        Scale.X > 0.0001 &&
        Scale.Y > 0.0001 &&
        Scale.Z > 0.0001;
}

void ConfigureRenderOnlyPrimitive(
    UPrimitiveComponent* Component,
    bool bCastShadow)
{
    check(Component);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false);
    Component->SetCastShadow(bCastShadow);
    Component->bCastContactShadow = bCastShadow;
}

void ConfigureRenderOnlyHism(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 StartCullDistance,
    int32 EndCullDistance,
    int32 WpoDisableDistance,
    bool bCastShadow)
{
    check(Component);
    ConfigureRenderOnlyPrimitive(Component, bCastShadow);
    Component->bEnableDensityScaling = false;
    Component->CurrentDensityScaling = 1.0f;
#if WITH_EDITOR
    Component->bCanEnableDensityScaling = false;
#endif
    Component->SetCullDistances(StartCullDistance, EndCullDistance);
    Component->SetWorldPositionOffsetDisableDistance(WpoDisableDistance);
    Component->ForcedLodModel = 0;
    Component->bOverrideMinLOD = true;
    Component->MinLOD = 0;
}

bool ValidateRenderOnlyPrimitive(
    const UPrimitiveComponent* Component,
    const TCHAR* Label,
    FString& OutError)
{
    if (!Component)
    {
        OutError = FString::Printf(TEXT("V5B component '%s' is null."), Label);
        return false;
    }
    if (Component->Mobility != EComponentMobility::Static ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation())
    {
        OutError = FString::Printf(
            TEXT("V5B component '%s' violates Static/NoCollision/no-overlap/no-navigation policy."),
            Label);
        return false;
    }
    if (Component->GetCollisionResponseToChannels() !=
        FCollisionResponseContainer(ECR_Ignore))
    {
        OutError = FString::Printf(
            TEXT("V5B component '%s' does not ignore every collision channel."),
            Label);
        return false;
    }
    OutError.Reset();
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
            TEXT("V5B %s count drift: expected %d, found %d."),
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
                TEXT("V5B %s transform %d is not finite with positive scale."),
                Label,
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool TransformArraysEqual(
    const TArray<FTransform>& A,
    const TArray<FTransform>& B,
    double Tolerance = 0.001)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (!A[Index].Equals(B[Index], Tolerance))
        {
            return false;
        }
    }
    return true;
}

bool ValidateComponentInstances(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<FTransform>& ExpectedWorldTransforms,
    const TCHAR* Label,
    FString& OutError)
{
    if (!Component ||
        Component->GetInstanceCount() != ExpectedWorldTransforms.Num())
    {
        OutError = FString::Printf(
            TEXT("V5B %s instance count drift: expected %d, found %d."),
            Label,
            ExpectedWorldTransforms.Num(),
            Component ? Component->GetInstanceCount() : -1);
        return false;
    }
    for (int32 Index = 0; Index < ExpectedWorldTransforms.Num(); ++Index)
    {
        FTransform Actual = FTransform::Identity;
        if (!Component->GetInstanceTransform(Index, Actual, true) ||
            !Actual.Equals(ExpectedWorldTransforms[Index], 0.001))
        {
            OutError = FString::Printf(
                TEXT("V5B %s instance transform %d drifted."),
                Label,
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshAndMaterial(
    const UStaticMeshComponent* Component,
    const UStaticMesh* Mesh,
    const UMaterialInterface* Material,
    const TCHAR* Label,
    FString& OutError)
{
    if (!Component ||
        Component->GetStaticMesh() != Mesh ||
        Component->GetMaterial(0) != Material)
    {
        OutError = FString::Printf(
            TEXT("V5B %s mesh/material binding drifted."),
            Label);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateHismMeshAndMaterial(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const UStaticMesh* Mesh,
    const UMaterialInterface* Material,
    const TCHAR* Label,
    FString& OutError)
{
    return ValidateMeshAndMaterial(
        Component,
        Mesh,
        Material,
        Label,
        OutError);
}

bool ValidateMeshAndMaterials(
    const UStaticMeshComponent* Component,
    const UStaticMesh* Mesh,
    const TArray<TObjectPtr<UMaterialInterface>>& Materials,
    const TCHAR* Label,
    FString& OutError)
{
    if (!Component || Component->GetStaticMesh() != Mesh ||
        Materials.Num() != UE_ARRAY_COUNT(ExpectedHardscapeMaterialSlotNames) ||
        Component->GetNumOverrideMaterials() != Materials.Num())
    {
        OutError = FString::Printf(
            TEXT("V5B %s mesh/material roster binding drifted."),
            Label);
        return false;
    }
    for (int32 MaterialIndex = 0;
         MaterialIndex < Materials.Num();
         ++MaterialIndex)
    {
        if (Component->GetMaterial(MaterialIndex) !=
            Materials[MaterialIndex].Get())
        {
            OutError = FString::Printf(
                TEXT("V5B %s material binding %d drifted."),
                Label,
                MaterialIndex);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool HasExactBaseOrRuntimeMidParent(
    const UMaterialInterface* Material,
    const UMaterialInterface* ExpectedBase)
{
    if (!Material || !ExpectedBase)
    {
        return false;
    }
    if (Material == ExpectedBase)
    {
        return true;
    }
    const UMaterialInstanceDynamic* Mid =
        Cast<UMaterialInstanceDynamic>(Material);
    return Mid && Mid->Parent == ExpectedBase;
}

void AssignMeshAndMaterial(
    UStaticMeshComponent* Component,
    UStaticMesh* Mesh,
    UMaterialInterface* Material)
{
    check(Component && Mesh && Material);
    Component->SetStaticMesh(Mesh);
    Component->SetMaterial(0, Material);
}

template <typename TActor>
TActor* FindExactlyOneActor(UWorld* World)
{
    TActor* Result = nullptr;
    if (!World)
    {
        return nullptr;
    }
    for (TActorIterator<TActor> It(World); It; ++It)
    {
        if (Result)
        {
            return nullptr;
        }
        Result = *It;
    }
    return Result;
}

TArray<UPrimitiveComponent*> LegacyGroundPlantRenderSources(UWorld* World)
{
    ATRIADIstanaExploreV2LandscapeActor* V2 =
        FindExactlyOneActor<ATRIADIstanaExploreV2LandscapeActor>(World);
    ATRIADIstanaExploreV3SupplementActor* V3 =
        FindExactlyOneActor<ATRIADIstanaExploreV3SupplementActor>(World);
    if (!V2 || !V3)
    {
        return {};
    }

    TArray<UPrimitiveComponent*> Components = {
        V2->ShrubInstancesA,
        V2->ShrubInstancesB,
        V2->HedgeInstances,
        V2->GroundcoverInstances,
        V3->Shrub02Instances,
        V3->Fern02Instances,
        V3->Moss01Instances,
        V3->BermudaGrassInstances};
    const FName ExpectedNames[] = {
        FName(TEXT("LayeredShrubsA")),
        FName(TEXT("LayeredShrubsB")),
        FName(TEXT("ClippedHedges")),
        FName(TEXT("FoundationGroundcover")),
        FName(TEXT("CC0Shrub02")),
        FName(TEXT("CC0Fern02")),
        FName(TEXT("CC0Moss01ShadedMicroAreas")),
        FName(TEXT("CC0BermudaGrassEdgePatches"))};
    const int32 ExpectedCounts[] = {
        512, 320, 192, 768, 96, 160, 24, 24};
    static_assert(
        UE_ARRAY_COUNT(ExpectedNames) == LegacyGroundPlantRenderSourceCount &&
            UE_ARRAY_COUNT(ExpectedCounts) ==
                LegacyGroundPlantRenderSourceCount,
        "The exact inherited legacy ground-plant roster changed.");
    if (Components.Num() != LegacyGroundPlantRenderSourceCount)
    {
        return {};
    }
    int32 TotalInstances = 0;
    for (int32 Index = 0; Index < Components.Num(); ++Index)
    {
        const UHierarchicalInstancedStaticMeshComponent* Hism =
            Cast<UHierarchicalInstancedStaticMeshComponent>(
                Components[Index]);
        if (!Hism || Hism->GetFName() != ExpectedNames[Index] ||
            Hism->GetInstanceCount() != ExpectedCounts[Index])
        {
            return {};
        }
        TotalInstances += Hism->GetInstanceCount();
    }
    return TotalInstances == LegacyGroundPlantInstanceCount
        ? Components
        : TArray<UPrimitiveComponent*>();
}

TArray<UPrimitiveComponent*> InheritedRenderSources(
    ATRIADIstanaExploreV4LandscapeActor* Actor)
{
    if (!Actor)
    {
        return {};
    }
    TArray<UPrimitiveComponent*> Components = {
        Actor->CloseTurfInstances,
        Actor->GeometryGrassInstances,
        Actor->ShrubInstances,
        Actor->FlowerInstances,
        Actor->UnderstoreyInstances};
    const TArray<UPrimitiveComponent*> LegacyGroundPlants =
        LegacyGroundPlantRenderSources(Actor->GetWorld());
    if (Components.Num() != InheritedV4RenderSourceCount ||
        LegacyGroundPlants.Num() != LegacyGroundPlantRenderSourceCount)
    {
        return {};
    }
    Components.Append(LegacyGroundPlants);
    return Components;
}

struct FInheritedRenderVisibilitySnapshot
{
    TArray<bool> Visibility;
    TArray<bool> HiddenInGame;
};

bool CaptureInheritedRenderVisibility(
    ATRIADIstanaExploreV4LandscapeActor* Actor,
    FInheritedRenderVisibilitySnapshot& OutSnapshot)
{
    OutSnapshot = FInheritedRenderVisibilitySnapshot();
    const TArray<UPrimitiveComponent*> Components =
        InheritedRenderSources(Actor);
    if (Components.Num() != InheritedRenderSourceCount)
    {
        return false;
    }
    for (UPrimitiveComponent* Component : Components)
    {
        if (!Component)
        {
            OutSnapshot = FInheritedRenderVisibilitySnapshot();
            return false;
        }
        OutSnapshot.Visibility.Add(Component->IsVisible());
        OutSnapshot.HiddenInGame.Add(Component->bHiddenInGame);
    }
    return true;
}

void RestoreInheritedRenderVisibility(
    ATRIADIstanaExploreV4LandscapeActor* Actor,
    const FInheritedRenderVisibilitySnapshot& Snapshot)
{
    const TArray<UPrimitiveComponent*> Components =
        InheritedRenderSources(Actor);
    if (Components.Num() != InheritedRenderSourceCount ||
        Snapshot.Visibility.Num() != InheritedRenderSourceCount ||
        Snapshot.HiddenInGame.Num() != InheritedRenderSourceCount)
    {
        return;
    }
    for (int32 Index = 0; Index < Components.Num(); ++Index)
    {
        if (Components[Index])
        {
            Components[Index]->SetVisibility(
                Snapshot.Visibility[Index],
                true);
            Components[Index]->SetHiddenInGame(
                Snapshot.HiddenInGame[Index]);
        }
    }
}

bool SetInheritedRenderSuccessorState(
    ATRIADIstanaExploreV4LandscapeActor* Actor,
    bool bSuccessorOwnsRendering)
{
    const TArray<UPrimitiveComponent*> Components =
        InheritedRenderSources(Actor);
    if (Components.Num() != InheritedRenderSourceCount)
    {
        return false;
    }
    for (UPrimitiveComponent* Component : Components)
    {
        if (!Component)
        {
            return false;
        }
    }
    for (UPrimitiveComponent* Component : Components)
    {
        Component->SetVisibility(!bSuccessorOwnsRendering, true);
        Component->SetHiddenInGame(bSuccessorOwnsRendering);
    }
    return true;
}

bool HasExactInheritedRenderSuccessorState(
    const ATRIADIstanaExploreV4LandscapeActor* Actor)
{
    const TArray<UPrimitiveComponent*> Components =
        InheritedRenderSources(
            const_cast<ATRIADIstanaExploreV4LandscapeActor*>(Actor));
    if (Components.Num() != InheritedRenderSourceCount)
    {
        return false;
    }
    for (const UPrimitiveComponent* Component : Components)
    {
        if (!Component || Component->IsVisible() || !Component->bHiddenInGame)
        {
            return false;
        }
    }
    return true;
}

struct FHardscapeRenderVisibilitySnapshot
{
    bool bVisible = false;
    bool bHiddenInGame = false;
};

bool CaptureHardscapeRenderVisibility(
    ATRIADIstanaPublicViewSceneActor* Actor,
    FHardscapeRenderVisibilitySnapshot& OutSnapshot)
{
    OutSnapshot = FHardscapeRenderVisibilitySnapshot();
    if (!Actor || !Actor->HardscapeComponent)
    {
        return false;
    }
    OutSnapshot.bVisible = Actor->HardscapeComponent->IsVisible();
    OutSnapshot.bHiddenInGame = Actor->HardscapeComponent->bHiddenInGame;
    return true;
}

void RestoreHardscapeRenderVisibility(
    ATRIADIstanaPublicViewSceneActor* Actor,
    const FHardscapeRenderVisibilitySnapshot& Snapshot)
{
    if (!Actor || !Actor->HardscapeComponent)
    {
        return;
    }
    Actor->HardscapeComponent->SetVisibility(Snapshot.bVisible, true);
    Actor->HardscapeComponent->SetHiddenInGame(Snapshot.bHiddenInGame);
}

bool SetHardscapeRenderSuccessorState(
    ATRIADIstanaPublicViewSceneActor* Actor,
    bool bSuccessorOwnsRendering)
{
    if (!Actor || !Actor->HardscapeComponent ||
        Actor->HardscapeComponent->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics)
    {
        return false;
    }
    Actor->HardscapeComponent->SetVisibility(
        !bSuccessorOwnsRendering,
        true);
    Actor->HardscapeComponent->SetHiddenInGame(bSuccessorOwnsRendering);
    return Actor->HardscapeComponent->GetCollisionEnabled() ==
        ECollisionEnabled::QueryAndPhysics;
}

bool HasExactHardscapeRenderSuccessorState(
    const ATRIADIstanaPublicViewSceneActor* Actor)
{
    return Actor && Actor->HardscapeComponent &&
        !Actor->HardscapeComponent->IsVisible() &&
        Actor->HardscapeComponent->bHiddenInGame &&
        Actor->HardscapeComponent->GetCollisionEnabled() ==
            ECollisionEnabled::QueryAndPhysics;
}

TArray<UHierarchicalInstancedStaticMeshComponent*> OwnedV5BHismComponents(
    ATRIADIstanaExploreV5BVisualActor* Actor)
{
    if (!Actor)
    {
        return {};
    }
    return {
        Actor->AccentTurfInstances,
        Actor->FormalBedVeneerComponent,
        Actor->FormalBedShrubCorrections,
        Actor->FormalBedFlowerCorrections,
        Actor->FormalBedUnderstoreyCorrections,
        Actor->TreeBaseMulchInstances,
        Actor->TreeBaseShrubInstances,
        Actor->TreeBaseUnderstoreyInstances,
        Actor->FountainSurfaceComponent,
        Actor->FountainEdgeFoamComponent,
        Actor->OuterPlumeInstances,
        Actor->ImpactRingInstances,
        Actor->CentralPlumeComponent,
        Actor->InnerPaverInstances,
        Actor->OuterPaverInstances,
        Actor->PachiraBarkAInstances,
        Actor->PachiraBarkBInstances,
        Actor->PachiraBarkCInstances,
        Actor->PachiraBarkDInstances,
        Actor->PachiraLeavesAInstances,
        Actor->PachiraLeavesBInstances,
        Actor->PachiraLeavesCInstances,
        Actor->PachiraLeavesDInstances};
}

struct FV5BOwnedHismStateSnapshot
{
    UHierarchicalInstancedStaticMeshComponent* Component = nullptr;
    UStaticMesh* Mesh = nullptr;
    TArray<TObjectPtr<UMaterialInterface>> OverrideMaterials;
    TArray<FTransform> LocalInstanceTransforms;
    int32 NumCustomDataFloats = 0;
    TArray<float> PerInstanceCustomData;
    int32 ForcedLodModel = 0;
    bool bOverrideMinLOD = false;
    int32 MinLOD = 0;
    bool bAutoRebuildTreeOnInstanceChanges = true;
};

struct FV5BOwnedVisualStateSnapshot
{
    UStaticMesh* HardscapeMesh = nullptr;
    TArray<TObjectPtr<UMaterialInterface>> HardscapeOverrideMaterials;
    TArray<FV5BOwnedHismStateSnapshot> HismStates;
};

bool CaptureOwnedV5BVisualState(
    ATRIADIstanaExploreV5BVisualActor* Actor,
    FV5BOwnedVisualStateSnapshot& OutSnapshot,
    FString& OutError)
{
    OutSnapshot = FV5BOwnedVisualStateSnapshot();
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components =
        OwnedV5BHismComponents(Actor);
    if (!Actor || !Actor->HardscapeRenderSuccessorComponent ||
        Components.Num() != 23)
    {
        OutError = TEXT("V5B failure-atomic snapshot found an incomplete owned component roster.");
        return false;
    }
    OutSnapshot.HardscapeMesh =
        Actor->HardscapeRenderSuccessorComponent->GetStaticMesh();
    OutSnapshot.HardscapeOverrideMaterials =
        Actor->HardscapeRenderSuccessorComponent->OverrideMaterials;
    OutSnapshot.HismStates.Reserve(Components.Num());
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component)
        {
            OutSnapshot = FV5BOwnedVisualStateSnapshot();
            OutError = TEXT("V5B failure-atomic snapshot found a null owned HISM.");
            return false;
        }
        FV5BOwnedHismStateSnapshot State;
        State.Component = Component;
        State.Mesh = Component->GetStaticMesh();
        State.OverrideMaterials = Component->OverrideMaterials;
        State.NumCustomDataFloats = Component->NumCustomDataFloats;
        State.PerInstanceCustomData = Component->PerInstanceSMCustomData;
        State.ForcedLodModel = Component->ForcedLodModel;
        State.bOverrideMinLOD = Component->bOverrideMinLOD;
        State.MinLOD = Component->MinLOD;
        State.bAutoRebuildTreeOnInstanceChanges =
            Component->bAutoRebuildTreeOnInstanceChanges;
        State.LocalInstanceTransforms.Reserve(Component->GetInstanceCount());
        for (int32 InstanceIndex = 0;
             InstanceIndex < Component->GetInstanceCount();
             ++InstanceIndex)
        {
            FTransform LocalTransform;
            if (!Component->GetInstanceTransform(
                    InstanceIndex,
                    LocalTransform,
                    false))
            {
                OutSnapshot = FV5BOwnedVisualStateSnapshot();
                OutError = FString::Printf(
                    TEXT("V5B failure-atomic snapshot could not read instance %d from '%s'."),
                    InstanceIndex,
                    *Component->GetName());
                return false;
            }
            State.LocalInstanceTransforms.Add(LocalTransform);
        }
        OutSnapshot.HismStates.Add(MoveTemp(State));
    }
    OutError.Reset();
    return true;
}

bool RestoreOwnedV5BVisualState(
    ATRIADIstanaExploreV5BVisualActor* Actor,
    const FV5BOwnedVisualStateSnapshot& Snapshot,
    FString& OutError)
{
    if (!Actor || !Actor->HardscapeRenderSuccessorComponent ||
        Snapshot.HismStates.Num() != 23)
    {
        OutError = TEXT("V5B failure-atomic restore found an incomplete owned snapshot.");
        return false;
    }

    Actor->HardscapeRenderSuccessorComponent->SetStaticMesh(
        Snapshot.HardscapeMesh);
    Actor->HardscapeRenderSuccessorComponent->OverrideMaterials =
        Snapshot.HardscapeOverrideMaterials;
    Actor->HardscapeRenderSuccessorComponent->MarkRenderStateDirty();

    bool bRestoredExactly = true;
    FString FirstFailure;
    for (const FV5BOwnedHismStateSnapshot& State : Snapshot.HismStates)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            State.Component;
        if (!Component)
        {
            bRestoredExactly = false;
            if (FirstFailure.IsEmpty())
            {
                FirstFailure = TEXT("a captured owned HISM no longer exists");
            }
            continue;
        }

        Component->bAutoRebuildTreeOnInstanceChanges = false;
        Component->ClearInstances();
        Component->SetStaticMesh(State.Mesh);
        Component->OverrideMaterials = State.OverrideMaterials;
        Component->NumCustomDataFloats = State.NumCustomDataFloats;
        Component->ForcedLodModel = State.ForcedLodModel;
        Component->bOverrideMinLOD = State.bOverrideMinLOD;
        Component->MinLOD = State.MinLOD;
        if (!State.LocalInstanceTransforms.IsEmpty())
        {
            Component->AddInstances(
                State.LocalInstanceTransforms,
                false,
                false,
                false);
        }
        Component->PerInstanceSMCustomData = State.PerInstanceCustomData;
        Component->MarkRenderStateDirty();

        const bool bTreeBuildCompleted =
            Component->BuildTreeIfOutdated(false, true);
        const bool bTreeReady = !Component->IsAsyncBuilding() &&
            (!Component->IsRegistered() ||
             State.LocalInstanceTransforms.IsEmpty() ||
             (bTreeBuildCompleted && Component->IsTreeFullyBuilt()));
        Component->bAutoRebuildTreeOnInstanceChanges =
            State.bAutoRebuildTreeOnInstanceChanges;

        bool bInstancesMatch =
            Component->GetInstanceCount() ==
                State.LocalInstanceTransforms.Num();
        for (int32 InstanceIndex = 0;
             bInstancesMatch &&
             InstanceIndex < State.LocalInstanceTransforms.Num();
             ++InstanceIndex)
        {
            FTransform RestoredTransform;
            bInstancesMatch = Component->GetInstanceTransform(
                    InstanceIndex,
                    RestoredTransform,
                    false) &&
                RestoredTransform.Equals(
                    State.LocalInstanceTransforms[InstanceIndex],
                    KINDA_SMALL_NUMBER);
        }
        const bool bComponentRestored = bTreeReady && bInstancesMatch &&
            Component->GetStaticMesh() == State.Mesh &&
            Component->OverrideMaterials == State.OverrideMaterials &&
            Component->NumCustomDataFloats == State.NumCustomDataFloats &&
            Component->PerInstanceSMCustomData ==
                State.PerInstanceCustomData &&
            Component->ForcedLodModel == State.ForcedLodModel &&
            Component->bOverrideMinLOD == State.bOverrideMinLOD &&
            Component->MinLOD == State.MinLOD &&
            Component->bAutoRebuildTreeOnInstanceChanges ==
                State.bAutoRebuildTreeOnInstanceChanges;
        if (!bComponentRestored)
        {
            bRestoredExactly = false;
            if (FirstFailure.IsEmpty())
            {
                FirstFailure = FString::Printf(
                    TEXT("owned HISM '%s' did not restore exactly or rebuild synchronously (registered=%s async=%s buildCompleted=%s treeFullyBuilt=%s treeReady=%s instancesMatch=%s meshMatch=%s materialsMatch=%s customFloatCountMatch=%s customDataMatch=%s forcedLodMatch=%s minLodOverrideMatch=%s minLodMatch=%s autoRebuildMatch=%s)"),
                    *Component->GetName(),
                    Component->IsRegistered() ? TEXT("true") : TEXT("false"),
                    Component->IsAsyncBuilding() ? TEXT("true") : TEXT("false"),
                    bTreeBuildCompleted ? TEXT("true") : TEXT("false"),
                    Component->IsTreeFullyBuilt() ? TEXT("true") : TEXT("false"),
                    bTreeReady ? TEXT("true") : TEXT("false"),
                    bInstancesMatch ? TEXT("true") : TEXT("false"),
                    Component->GetStaticMesh() == State.Mesh ? TEXT("true") : TEXT("false"),
                    Component->OverrideMaterials == State.OverrideMaterials ? TEXT("true") : TEXT("false"),
                    Component->NumCustomDataFloats == State.NumCustomDataFloats ? TEXT("true") : TEXT("false"),
                    Component->PerInstanceSMCustomData == State.PerInstanceCustomData ? TEXT("true") : TEXT("false"),
                    Component->ForcedLodModel == State.ForcedLodModel ? TEXT("true") : TEXT("false"),
                    Component->bOverrideMinLOD == State.bOverrideMinLOD ? TEXT("true") : TEXT("false"),
                    Component->MinLOD == State.MinLOD ? TEXT("true") : TEXT("false"),
                    Component->bAutoRebuildTreeOnInstanceChanges == State.bAutoRebuildTreeOnInstanceChanges ? TEXT("true") : TEXT("false"));
            }
        }
    }

    const bool bHardscapeRestored =
        Actor->HardscapeRenderSuccessorComponent->GetStaticMesh() ==
            Snapshot.HardscapeMesh &&
        Actor->HardscapeRenderSuccessorComponent->OverrideMaterials ==
            Snapshot.HardscapeOverrideMaterials;
    if (!bHardscapeRestored)
    {
        bRestoredExactly = false;
        if (FirstFailure.IsEmpty())
        {
            FirstFailure = TEXT("the owned hardscape mesh/material overrides did not restore exactly");
        }
    }
    if (!bRestoredExactly)
    {
        OutError = TEXT("V5B failure-atomic rollback failed: ") +
            FirstFailure + TEXT(".");
        return false;
    }
    OutError.Reset();
    return true;
}

struct FV5BSceneRenderStateSnapshot
{
    ATRIADIstanaPublicViewSceneActor* Actor = nullptr;
    TArray<FName> Tags;
    FHardscapeRenderVisibilitySnapshot HardscapeVisibility;
};

struct FV5BSourceRenderStateSnapshot
{
    ATRIADIstanaExploreV4LandscapeActor* Actor = nullptr;
    TArray<FName> Tags;
    FInheritedRenderVisibilitySnapshot InheritedVisibility;
};

bool CaptureSceneRenderState(
    ATRIADIstanaPublicViewSceneActor* Actor,
    FV5BSceneRenderStateSnapshot& OutSnapshot)
{
    OutSnapshot = FV5BSceneRenderStateSnapshot();
    if (!Actor || !CaptureHardscapeRenderVisibility(
            Actor,
            OutSnapshot.HardscapeVisibility))
    {
        return false;
    }
    OutSnapshot.Actor = Actor;
    OutSnapshot.Tags = Actor->Tags;
    return true;
}

bool CaptureSourceRenderState(
    ATRIADIstanaExploreV4LandscapeActor* Actor,
    FV5BSourceRenderStateSnapshot& OutSnapshot)
{
    OutSnapshot = FV5BSourceRenderStateSnapshot();
    if (!Actor || !CaptureInheritedRenderVisibility(
            Actor,
            OutSnapshot.InheritedVisibility))
    {
        return false;
    }
    OutSnapshot.Actor = Actor;
    OutSnapshot.Tags = Actor->Tags;
    return true;
}

bool RestoreSceneRenderState(
    const FV5BSceneRenderStateSnapshot& Snapshot)
{
    if (!Snapshot.Actor || !Snapshot.Actor->HardscapeComponent)
    {
        return false;
    }
    Snapshot.Actor->Tags = Snapshot.Tags;
    RestoreHardscapeRenderVisibility(
        Snapshot.Actor,
        Snapshot.HardscapeVisibility);
    return Snapshot.Actor->Tags == Snapshot.Tags &&
        Snapshot.Actor->HardscapeComponent->IsVisible() ==
            Snapshot.HardscapeVisibility.bVisible &&
        Snapshot.Actor->HardscapeComponent->bHiddenInGame ==
            Snapshot.HardscapeVisibility.bHiddenInGame;
}

bool RestoreSourceRenderState(
    const FV5BSourceRenderStateSnapshot& Snapshot)
{
    if (!Snapshot.Actor)
    {
        return false;
    }
    Snapshot.Actor->Tags = Snapshot.Tags;
    RestoreInheritedRenderVisibility(
        Snapshot.Actor,
        Snapshot.InheritedVisibility);
    FInheritedRenderVisibilitySnapshot RestoredVisibility;
    return Snapshot.Actor->Tags == Snapshot.Tags &&
        CaptureInheritedRenderVisibility(
            Snapshot.Actor,
            RestoredVisibility) &&
        RestoredVisibility.Visibility ==
            Snapshot.InheritedVisibility.Visibility &&
        RestoredVisibility.HiddenInGame ==
            Snapshot.InheritedVisibility.HiddenInGame;
}
}

ATRIADIstanaExploreV5BVisualActor::ATRIADIstanaExploreV5BVisualActor()
{
    PrimaryActorTick.bCanEverTick = false;
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("ExploreV5BVisualRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    AccentTurfInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BAccentTurfInstances"));
    HardscapeRenderSuccessorComponent =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("V5BHardscapeNoLegacyBedsRenderSuccessor"));
    FacadeFillLightComponent =
        CreateDefaultSubobject<UDirectionalLightComponent>(
            TEXT("V5BFacadeFillLight"));
    FormalBedVeneerComponent =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BFormalBedsOrganicVeneer"));
    FormalBedShrubCorrections =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BFormalBedShrubCorrections"));
    FormalBedFlowerCorrections =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BFormalBedFlowerCorrections"));
    FormalBedUnderstoreyCorrections =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BFormalBedUnderstoreyCorrections"));
    TreeBaseMulchInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BTreeBaseMulchInstances"));
    TreeBaseShrubInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BTreeBaseShrubInstances"));
    TreeBaseUnderstoreyInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BTreeBaseUnderstoreyInstances"));
    FountainSurfaceComponent =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BFountainSurface"));
    FountainEdgeFoamComponent =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BFountainEdgeFoam"));
    OuterPlumeInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BOuterPlumeInstances"));
    ImpactRingInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BImpactRingInstances"));
    CentralPlumeComponent =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BCentralPlume"));
    InnerPaverInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BInnerPaverInstances"));
    OuterPaverInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BOuterPaverInstances"));
    PachiraBarkAInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraBarkAInstances"));
    PachiraBarkBInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraBarkBInstances"));
    PachiraBarkCInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraBarkCInstances"));
    PachiraBarkDInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraBarkDInstances"));
    PachiraLeavesAInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraLeavesAInstances"));
    PachiraLeavesBInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraLeavesBInstances"));
    PachiraLeavesCInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraLeavesCInstances"));
    PachiraLeavesDInstances =
        CreateDefaultSubobject<UTRIADIstanaExploreV4SynchronousHismComponent>(
            TEXT("V5BPachiraLeavesDInstances"));

    const TArray<USceneComponent*> VisualComponents = {
        AccentTurfInstances,
        HardscapeRenderSuccessorComponent,
        FormalBedVeneerComponent,
        FormalBedShrubCorrections,
        FormalBedFlowerCorrections,
        FormalBedUnderstoreyCorrections,
        TreeBaseMulchInstances,
        TreeBaseShrubInstances,
        TreeBaseUnderstoreyInstances,
        FountainSurfaceComponent,
        FountainEdgeFoamComponent,
        OuterPlumeInstances,
        ImpactRingInstances,
        CentralPlumeComponent,
        InnerPaverInstances,
        OuterPaverInstances,
        PachiraBarkAInstances,
        PachiraBarkBInstances,
        PachiraBarkCInstances,
        PachiraBarkDInstances,
        PachiraLeavesAInstances,
        PachiraLeavesBInstances,
        PachiraLeavesCInstances,
        PachiraLeavesDInstances};
    for (USceneComponent* Component : VisualComponents)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        Component->SetMobility(EComponentMobility::Static);
    }

    FacadeFillLightComponent->SetupAttachment(SceneRoot);
    FacadeFillLightComponent->SetRelativeLocation(FVector::ZeroVector);
    FacadeFillLightComponent->SetRelativeRotation(FRotator(-12.0, -90.0, 0.0));
    FacadeFillLightComponent->SetMobility(EComponentMobility::Movable);
    FacadeFillLightComponent->SetIntensity(DefaultFacadeFillIntensityLux);
    FacadeFillLightComponent->SetLightColor(FLinearColor(0.90f, 0.95f, 1.0f));
    FacadeFillLightComponent->SetLightingChannels(false, true, false);
    FacadeFillLightComponent->SetCastShadows(false);
    FacadeFillLightComponent->SetCastVolumetricShadow(false);
    FacadeFillLightComponent->SetAffectReflection(false);
    FacadeFillLightComponent->SetAffectGlobalIllumination(false);
    FacadeFillLightComponent->SetIndirectLightingIntensity(0.0f);
    FacadeFillLightComponent->SetVolumetricScatteringIntensity(0.0f);
    FacadeFillLightComponent->SetVisibility(false);

    ConfigureRenderOnlyHism(AccentTurfInstances, 3000, 5200, 3200, false);
    ConfigureRenderOnlyPrimitive(HardscapeRenderSuccessorComponent, true);
    ConfigureRenderOnlyHism(FormalBedVeneerComponent, 0, 90000, 0, true);
    ConfigureRenderOnlyHism(FormalBedShrubCorrections, 10000, 65000, 30000, true);
    ConfigureRenderOnlyHism(FormalBedFlowerCorrections, 10000, 45000, 24000, true);
    ConfigureRenderOnlyHism(FormalBedUnderstoreyCorrections, 10000, 55000, 28000, true);
    ConfigureRenderOnlyHism(TreeBaseMulchInstances, 0, 65000, 0, true);
    ConfigureRenderOnlyHism(TreeBaseShrubInstances, 10000, 65000, 30000, true);
    ConfigureRenderOnlyHism(TreeBaseUnderstoreyInstances, 10000, 55000, 28000, true);
    // UE 5.5's HISM traversal consumes ForcedLodModel as the direct zero-based
    // LOD index, so 1 selects source LOD1. Keeping only these 192 tree-base
    // plants off the 12-triangle LOD2 avoids detached leaf-card silhouettes
    // without changing the inherited V4 renderers.
    TreeBaseShrubInstances->ForcedLodModel = TreeBasePlantForcedLodModel;
    TreeBaseUnderstoreyInstances->ForcedLodModel =
        TreeBasePlantForcedLodModel;
    ConfigureRenderOnlyHism(FountainSurfaceComponent, 0, 90000, 70000, false);
    ConfigureRenderOnlyHism(FountainEdgeFoamComponent, 0, 75000, 60000, false);
    ConfigureRenderOnlyHism(OuterPlumeInstances, 0, 75000, 60000, false);
    ConfigureRenderOnlyHism(ImpactRingInstances, 0, 65000, 55000, false);
    ConfigureRenderOnlyHism(CentralPlumeComponent, 0, 90000, 70000, false);
    ConfigureRenderOnlyHism(InnerPaverInstances, 0, 85000, 0, true);
    ConfigureRenderOnlyHism(OuterPaverInstances, 0, 85000, 0, true);

    const TArray<UHierarchicalInstancedStaticMeshComponent*> PachiraComponents = {
        PachiraBarkAInstances,
        PachiraBarkBInstances,
        PachiraBarkCInstances,
        PachiraBarkDInstances,
        PachiraLeavesAInstances,
        PachiraLeavesBInstances,
        PachiraLeavesCInstances,
        PachiraLeavesDInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : PachiraComponents)
    {
        ConfigureRenderOnlyHism(Component, 2500, 80000, 50000, true);
    }
}

const FString& ATRIADIstanaExploreV5BVisualActor::ExpectedClaimLabel()
{
    return V5BClaimLabel;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedCloseTurfSourceCount()
{
    return CloseTurfSourceCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedAccentTurfCount()
{
    return AccentTurfCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedFormalBedShrubCorrectionCount()
{
    return FormalBedShrubRenderCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedFormalBedFlowerCorrectionCount()
{
    return FormalBedFlowerRenderCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedFormalBedUnderstoreyCorrectionCount()
{
    return FormalBedUnderstoreyRenderCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedTreeBaseMulchCount()
{
    return TreeGroundingAnchorCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedTreeBaseShrubCount()
{
    return TreeGroundingShrubCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedTreeBaseUnderstoreyCount()
{
    return TreeGroundingUnderstoreyCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedInnerPaverCount()
{
    return InnerPaverCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedOuterPaverCount()
{
    return OuterPaverCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedOuterPlumeCount()
{
    return OuterPlumeCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedImpactRingCount()
{
    return ImpactRingCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedLogicalPachiraCount()
{
    return LogicalPachiraCount;
}

int32 ATRIADIstanaExploreV5BVisualActor::ExpectedPachiraInstancesPerVariant()
{
    return PachiraInstancesPerVariant;
}

int32 ATRIADIstanaExploreV5BVisualActor::AccentSourceIndexForOrdinal(
    int32 AccentOrdinal)
{
    if (AccentOrdinal < 0 || AccentOrdinal >= AccentTurfCount)
    {
        return INDEX_NONE;
    }
    return static_cast<int32>(
        (static_cast<int64>(AccentSourceMultiplier) * AccentOrdinal +
         AccentSourceOffset) %
        CloseTurfSourceCount);
}

double ATRIADIstanaExploreV5BVisualActor::ExpectedFountainSurfaceZCm()
{
    return FountainSurfaceZCm;
}

double ATRIADIstanaExploreV5BVisualActor::ExpectedOuterPlumeOriginZCm()
{
    return OuterPlumeOriginZCm;
}

double ATRIADIstanaExploreV5BVisualActor::ExpectedCentralPlumeOriginZCm()
{
    return CentralPlumeOriginZCm;
}

double ATRIADIstanaExploreV5BVisualActor::ExpectedPaverZCm()
{
    return PaverZCm;
}

bool ATRIADIstanaExploreV5BVisualActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5BAssetRoster& Assets,
    FString& OutError)
{
    struct FNamedMesh
    {
        const TCHAR* Name = nullptr;
        const UStaticMesh* Mesh = nullptr;
    };
    TArray<FNamedMesh> Meshes = {
        {TEXT("AccentTurfMesh"), Assets.AccentTurfMesh},
        {TEXT("HardscapeRenderSuccessorMesh"), Assets.HardscapeRenderSuccessorMesh},
        {TEXT("FormalBedVeneerMesh"), Assets.FormalBedVeneerMesh},
        {TEXT("FormalBedShrubMesh"), Assets.FormalBedShrubMesh},
        {TEXT("FormalBedFlowerMesh"), Assets.FormalBedFlowerMesh},
        {TEXT("FormalBedUnderstoreyMesh"), Assets.FormalBedUnderstoreyMesh},
        {TEXT("TreeBaseMulchMesh"), Assets.TreeBaseMulchMesh},
        {TEXT("FountainSurfaceMesh"), Assets.FountainSurfaceMesh},
        {TEXT("FountainEdgeFoamMesh"), Assets.FountainEdgeFoamMesh},
        {TEXT("OuterPlumeMesh"), Assets.OuterPlumeMesh},
        {TEXT("ImpactRingMesh"), Assets.ImpactRingMesh},
        {TEXT("CentralPlumeMesh"), Assets.CentralPlumeMesh},
        {TEXT("InnerPaverWedgeMesh"), Assets.InnerPaverWedgeMesh},
        {TEXT("OuterPaverWedgeMesh"), Assets.OuterPaverWedgeMesh}};

    if (Assets.HardscapeRenderSuccessorMaterials.Num() !=
        UE_ARRAY_COUNT(ExpectedHardscapeMaterialSlotNames))
    {
        OutError = TEXT("V5B requires exactly three ordered hardscape render-successor materials.");
        return false;
    }

    if (Assets.PachiraBarkMeshes.Num() != PachiraVariantCount ||
        Assets.PachiraLeavesMeshes.Num() != PachiraVariantCount ||
        Assets.PachiraBarkMaterials.Num() != PachiraVariantCount ||
        Assets.PachiraLeavesMaterials.Num() != PachiraVariantCount)
    {
        OutError = TEXT("V5B requires exactly four ordered Pachira bark meshes, leaf meshes, bark materials and leaf materials.");
        return false;
    }
    for (int32 Variant = 0; Variant < PachiraVariantCount; ++Variant)
    {
        Meshes.Add({TEXT("PachiraBarkMesh"), Assets.PachiraBarkMeshes[Variant]});
        Meshes.Add({TEXT("PachiraLeavesMesh"), Assets.PachiraLeavesMeshes[Variant]});
        if (!Assets.PachiraBarkMaterials[Variant] ||
            !Assets.PachiraLeavesMaterials[Variant] ||
            Assets.PachiraBarkMaterials[Variant]->HasAnyFlags(
                RF_BeginDestroyed | RF_FinishDestroyed) ||
            Assets.PachiraLeavesMaterials[Variant]->HasAnyFlags(
                RF_BeginDestroyed | RF_FinishDestroyed))
        {
            OutError = FString::Printf(
                TEXT("V5B Pachira material variant %d is incomplete."),
                Variant);
            return false;
        }
    }

    TSet<const UStaticMesh*> UniqueMeshes;
    for (const FNamedMesh& Entry : Meshes)
    {
        if (!Entry.Mesh)
        {
            OutError = FString::Printf(
                TEXT("V5B required asset %s is null."),
                Entry.Name);
            return false;
        }
        if (Entry.Mesh->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
            Entry.Mesh->IsCompiling())
        {
            OutError = FString::Printf(
                TEXT("V5B required mesh %s is destroying or still compiling."),
                Entry.Name);
            return false;
        }
        if (UniqueMeshes.Contains(Entry.Mesh))
        {
            OutError = FString::Printf(
                TEXT("V5B required mesh %s reuses another mesh pointer; combined or aliased imports are rejected."),
                Entry.Name);
            return false;
        }
        UniqueMeshes.Add(Entry.Mesh);
    }

    TArray<const UMaterialInterface*> RequiredMaterials = {
        Assets.AccentTurfMaterial,
        Assets.FormalBedVeneerMaterial,
        Assets.FormalBedShrubMaterial,
        Assets.FormalBedFlowerMaterial,
        Assets.FormalBedUnderstoreyMaterial,
        Assets.FountainSurfaceMaterial,
        Assets.FountainEdgeFoamMaterial,
        Assets.OuterPlumeMaterial,
        Assets.ImpactRingMaterial,
        Assets.CentralPlumeMaterial,
        Assets.PaverMaterial};
    for (const TObjectPtr<UMaterialInterface>& Material :
         Assets.HardscapeRenderSuccessorMaterials)
    {
        RequiredMaterials.Add(Material.Get());
    }
    for (int32 Index = 0; Index < RequiredMaterials.Num(); ++Index)
    {
        if (!RequiredMaterials[Index])
        {
            OutError = FString::Printf(
                TEXT("V5B required primary material %d is null."),
                Index);
            return false;
        }
        if (RequiredMaterials[Index]->HasAnyFlags(
                RF_BeginDestroyed | RF_FinishDestroyed))
        {
            OutError = FString::Printf(
                TEXT("V5B required primary material %d is destroying."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::BuildDeterministicLayout(
    const TArray<FTransform>& CloseTurfWorldTransforms,
    FTRIADIstanaExploreV5BDeterministicLayout& OutLayout,
    FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5BDeterministicLayout();
    if (CloseTurfWorldTransforms.Num() != CloseTurfSourceCount)
    {
        OutError = FString::Printf(
            TEXT("V5B requires exactly %d ordered V4 close-turf transforms; found %d."),
            CloseTurfSourceCount,
            CloseTurfWorldTransforms.Num());
        return false;
    }
    for (int32 Index = 0; Index < CloseTurfWorldTransforms.Num(); ++Index)
    {
        if (!IsFinitePositiveTransform(CloseTurfWorldTransforms[Index]))
        {
            OutError = FString::Printf(
                TEXT("V5B source close-turf transform %d is invalid."),
                Index);
            return false;
        }
    }

    OutLayout.AccentTurfSourceIndices.Reserve(AccentTurfCount);
    OutLayout.AccentTurfWorldTransforms.Reserve(AccentTurfCount);
    TSet<int32> UniqueAccentSourceIndices;
    int32 AccentOrdinal = 0;
    int32 CandidateIndex = 0;
    constexpr int32 MaximumAccentCandidates = AccentTurfCount * 4;
    while (AccentOrdinal < AccentTurfCount &&
           CandidateIndex < MaximumAccentCandidates)
    {
        const double SequenceIndex = static_cast<double>(CandidateIndex + 1);
        const double LawnU = FractionalPart(
            0.5 + SequenceIndex * AccentR2IncrementX);
        const double LawnV = FractionalPart(
            0.5 + SequenceIndex * AccentR2IncrementY);
        ++CandidateIndex;
        const double XCm = FMath::Lerp(
            AccentLawnMinimumXCm,
            AccentLawnMaximumXCm,
            LawnU);
        const double YCm = FMath::Lerp(
            AccentLawnMinimumYCm,
            AccentLawnMaximumYCm,
            LawnV);
        if (IsInsideAccentLawnExclusion(XCm, YCm))
        {
            continue;
        }

        const int32 SourceIndex = AccentSourceIndexForOrdinal(AccentOrdinal);
        if (!CloseTurfWorldTransforms.IsValidIndex(SourceIndex) ||
            UniqueAccentSourceIndices.Contains(SourceIndex))
        {
            OutLayout = FTRIADIstanaExploreV5BDeterministicLayout();
            OutError = FString::Printf(
                TEXT("V5B accent source formula produced invalid or duplicate index %d at ordinal %d."),
                SourceIndex,
                AccentOrdinal);
            return false;
        }
        UniqueAccentSourceIndices.Add(SourceIndex);

        const FTransform& Source = CloseTurfWorldTransforms[SourceIndex];
        FVector Location(
            XCm,
            YCm,
            AnalyticAccentTerrainHeightCm(XCm, YCm) +
                AccentTerrainGapCm);
        // Keep the exact one-to-one successor/provenance roster, but suppress
        // only the organic superellipse below each formal-bed veneer so no
        // 4-5.8 cm blade can punch through its irregular soil edge.
        if (IsInsideOrganicFormalBedSuppression(XCm, YCm))
        {
            Location.Z -= FormalBedTurfSuppressionDepthCm;
        }
        FRotator Rotation = Source.Rotator();
        const double YawUnit = HashUnitInterval(
            static_cast<uint32>(AccentOrdinal) ^ 0xA17C3E5Du);
        const double XScaleUnit = HashUnitInterval(
            static_cast<uint32>(AccentOrdinal) ^ 0x51C4D2B7u);
        const double YScaleUnit = HashUnitInterval(
            static_cast<uint32>(AccentOrdinal) ^ 0xE42A916Cu);
        const double ZScaleUnit = HashUnitInterval(
            static_cast<uint32>(AccentOrdinal) ^ 0xC98E741Bu);
        Rotation.Yaw = FRotator::NormalizeAxis(
            Rotation.Yaw + AccentBaseYawDegrees +
            FMath::Lerp(
                -AccentYawJitterDegrees,
                AccentYawJitterDegrees,
                YawUnit));
        const double XScale = FMath::Lerp(
            AccentMinXScale,
            AccentMaxXScale,
            XScaleUnit);
        const double YScale = FMath::Lerp(
            AccentMinYScale,
            AccentMaxYScale,
            YScaleUnit);
        const double ZScale = FMath::Lerp(
            AccentMinZScale,
            AccentMaxZScale,
            ZScaleUnit);
        const FTransform AccentTransform(
            Rotation,
            Location,
            FVector(XScale, YScale, ZScale));
        if (!IsFinitePositiveTransform(AccentTransform))
        {
            OutLayout = FTRIADIstanaExploreV5BDeterministicLayout();
            OutError = FString::Printf(
                TEXT("V5B generated accent transform %d is invalid."),
                AccentOrdinal);
            return false;
        }
        OutLayout.AccentTurfSourceIndices.Add(SourceIndex);
        OutLayout.AccentTurfWorldTransforms.Add(AccentTransform);
        ++AccentOrdinal;
    }
    if (AccentOrdinal != AccentTurfCount)
    {
        OutLayout = FTRIADIstanaExploreV5BDeterministicLayout();
        OutError = FString::Printf(
            TEXT("V5B low-discrepancy lawn distribution produced only %d/%d admitted accent transforms."),
            AccentOrdinal,
            AccentTurfCount);
        return false;
    }

    OutLayout.FountainSurfaceWorldTransform = FTransform(
        FRotator::ZeroRotator,
        FVector(FountainCenterXCm, FountainCenterYCm, FountainSurfaceZCm),
        FVector::OneVector);
    OutLayout.FountainEdgeFoamWorldTransform = FTransform(
        FRotator::ZeroRotator,
        FVector(FountainCenterXCm, FountainCenterYCm, FountainFoamZCm),
        FVector::OneVector);
    OutLayout.OuterPlumeWorldTransforms.Reserve(OuterPlumeCount);
    OutLayout.ImpactRingWorldTransforms.Reserve(ImpactRingCount);
    for (int32 Index = 0; Index < OuterPlumeCount; ++Index)
    {
        const double AngleDegrees = 30.0 * static_cast<double>(Index);
        const double AngleRadians = FMath::DegreesToRadians(AngleDegrees);
        const FVector Location(
            FountainCenterXCm + OuterPlumeRadiusCm * FMath::Cos(AngleRadians),
            FountainCenterYCm + OuterPlumeRadiusCm * FMath::Sin(AngleRadians),
            OuterPlumeOriginZCm);
        const double WidthScale = FMath::Lerp(
            0.66,
            0.86,
            HashUnitInterval(static_cast<uint32>(Index) ^ 0x7A3C51D9u));
        const double HeightScale = FMath::Lerp(
            0.90,
            1.10,
            HashUnitInterval(static_cast<uint32>(Index) ^ 0x1F8BD42Eu));
        const double PitchDegrees = FMath::Lerp(
            -2.8,
            2.8,
            HashUnitInterval(static_cast<uint32>(Index) ^ 0xD621A94Bu));
        const double RollDegrees = FMath::Lerp(
            -2.2,
            2.2,
            HashUnitInterval(static_cast<uint32>(Index) ^ 0x4C95E713u));
        OutLayout.OuterPlumeWorldTransforms.Add(FTransform(
            FRotator(PitchDegrees, AngleDegrees, RollDegrees),
            Location,
            FVector(WidthScale, WidthScale, HeightScale)));
        const double RippleScale = FMath::Lerp(
            0.82,
            1.08,
            HashUnitInterval(static_cast<uint32>(Index) ^ 0x96F2C0A5u));
        OutLayout.ImpactRingWorldTransforms.Add(FTransform(
            FRotator(0.0, AngleDegrees, 0.0),
            Location,
            FVector(RippleScale, RippleScale, 1.0)));
    }
    OutLayout.CentralPlumeWorldTransform = FTransform(
        FRotator::ZeroRotator,
        FVector(
            FountainCenterXCm,
            FountainCenterYCm,
            CentralPlumeOriginZCm),
        FVector(0.82, 0.82, 1.06));

    // The paver meshes are local-centred 54 cm stones. Five staggered rings
    // use an approximately constant 1.2 cm tangential joint, with tiny
    // deterministic per-piece radial-width and height variation.
    OutLayout.InnerPaverWorldTransforms.Reserve(InnerPaverCount);
    OutLayout.OuterPaverWorldTransforms.Reserve(OuterPaverCount);
    struct FPaverRing
    {
        double RadiusCm;
        int32 Count;
        bool bInnerMesh;
        double Phase;
    };
    const FPaverRing Rings[] = {
        {1065.0, 112, true, 0.0},
        {1122.0, 120, false, 0.5},
        {1179.0, 128, true, 0.0},
        {1236.0, 136, false, 0.5},
        {1293.0, 144, true, 0.0}};
    int32 GlobalPaverIndex = 0;
    for (const FPaverRing& Ring : Rings)
    {
        TArray<FTransform>& Target = Ring.bInnerMesh
            ? OutLayout.InnerPaverWorldTransforms
            : OutLayout.OuterPaverWorldTransforms;
        const double MeshTangentCm = Ring.bInnerMesh ? 55.0 : 54.0;
        const double TangentScale =
            (((2.0 * PI) * Ring.RadiusCm / Ring.Count) - 1.2) /
            MeshTangentCm;
        for (int32 Index = 0; Index < Ring.Count; ++Index)
        {
            const double AngleDegrees =
                360.0 * (static_cast<double>(Index) + Ring.Phase) /
                Ring.Count;
            const double AngleRadians = FMath::DegreesToRadians(AngleDegrees);
            const double RadialScale = FMath::Lerp(
                0.985,
                1.015,
                HashUnitInterval(
                    static_cast<uint32>(GlobalPaverIndex) ^ 0x3847A2D1u));
            const double ZJitterCm = FMath::Lerp(
                -0.10,
                0.10,
                HashUnitInterval(
                    static_cast<uint32>(GlobalPaverIndex) ^ 0xB79C150Eu));
            Target.Add(FTransform(
                FRotator(0.0, AngleDegrees, 0.0),
                FVector(
                    FountainCenterXCm + Ring.RadiusCm * FMath::Cos(AngleRadians),
                    FountainCenterYCm + Ring.RadiusCm * FMath::Sin(AngleRadians),
                    PaverZCm + ZJitterCm),
                FVector(RadialScale, TangentScale, 1.0)));
            ++GlobalPaverIndex;
        }
    }

    // Preserve the exact logical and per-variant census, but distribute each
    // side independently through three deterministic groves. This retains
    // formal bilateral massing without repeated rows or exact mirror clones.
    struct FPachiraGrove
    {
        double CenterU;
        double CenterV;
        double RadiusU;
        double RadiusV;
    };
    const FPachiraGrove PachiraGroves[] = {
        {-0.38, -0.32, 0.21, 0.19},
        {0.34, -0.05, 0.19, 0.25},
        {-0.06, 0.40, 0.26, 0.16}};
    int32 LogicalPlantIndex = 0;
    for (; LogicalPlantIndex < LogicalPachiraCount; ++LogicalPlantIndex)
    {
        const int32 SideIndex = LogicalPlantIndex & 1;
        const int32 PairOrdinal = LogicalPlantIndex / 2;
        const int32 Variant = PairOrdinal % PachiraVariantCount;
        const FPachiraGrove& Grove =
            PachiraGroves[(PairOrdinal / PachiraVariantCount) % 3];
        const double Radius = FMath::Sqrt(HashUnitInterval(
            static_cast<uint32>(LogicalPlantIndex) ^ 0xA41E73C9u));
        const double Angle = 2.0 * PI * HashUnitInterval(
            static_cast<uint32>(LogicalPlantIndex) ^ 0x5BC219E7u);
        const double BedU = FMath::Clamp(
            Grove.CenterU + Radius * Grove.RadiusU * FMath::Cos(Angle),
            -0.82,
            0.82);
        const double BedV = FMath::Clamp(
            Grove.CenterV + Radius * Grove.RadiusV * FMath::Sin(Angle),
            -0.82,
            0.82);
        const double XCm = SideIndex == 0
            ? -FormalBedCenterAbsXCm + 1400.0 * BedU
            : FormalBedCenterAbsXCm - 1400.0 * BedU;
        const double YCm = FormalBedCenterYCm + 900.0 * BedV;
        const double ZCm = FormalBedCoreDatumZCm +
            FormalBedReliefCm(BedU, BedV) - PachiraRootInsetCm;
        const double YawDegrees = 360.0 * HashUnitInterval(
            static_cast<uint32>(LogicalPlantIndex) ^ 0xB451D92Fu);
        const double UniformScale = FMath::Lerp(
            0.78,
            1.18,
            HashUnitInterval(
                static_cast<uint32>(LogicalPlantIndex) ^ 0x61C8F2B5u));
        OutLayout.PachiraWorldTransformsByVariant[Variant].Add(FTransform(
            FRotator(0.0, YawDegrees, 0.0),
            FVector(XCm, YCm, ZCm),
            FVector(UniformScale)));
    }

    if (LogicalPlantIndex != LogicalPachiraCount ||
        !ValidateTransformArray(
            OutLayout.AccentTurfWorldTransforms,
            AccentTurfCount,
            TEXT("accent-turf"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.OuterPlumeWorldTransforms,
            OuterPlumeCount,
            TEXT("outer-plume"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.ImpactRingWorldTransforms,
            ImpactRingCount,
            TEXT("impact-ring"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.InnerPaverWorldTransforms,
            InnerPaverCount,
            TEXT("inner-paver"),
            OutError) ||
        !ValidateTransformArray(
            OutLayout.OuterPaverWorldTransforms,
            OuterPaverCount,
            TEXT("outer-paver"),
            OutError))
    {
        OutLayout = FTRIADIstanaExploreV5BDeterministicLayout();
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5B logical Pachira census drifted.");
        }
        return false;
    }
    for (int32 Variant = 0; Variant < PachiraVariantCount; ++Variant)
    {
        if (!ValidateTransformArray(
                OutLayout.PachiraWorldTransformsByVariant[Variant],
                PachiraInstancesPerVariant,
                TEXT("Pachira morphology proxy variant"),
                OutError))
        {
            OutLayout = FTRIADIstanaExploreV5BDeterministicLayout();
            return false;
        }
    }
    if (!IsFinitePositiveTransform(OutLayout.FountainSurfaceWorldTransform) ||
        !IsFinitePositiveTransform(OutLayout.FountainEdgeFoamWorldTransform) ||
        !IsFinitePositiveTransform(OutLayout.CentralPlumeWorldTransform))
    {
        OutLayout = FTRIADIstanaExploreV5BDeterministicLayout();
        OutError = TEXT("V5B generated a non-finite fountain transform.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::SelectCanonicalMaximinCandidateIndices(
    const TArray<FVector2D>& CanonicalCandidateRoots,
    int32 SelectionCount,
    TArray<int32>& OutBestSelection,
    double& OutBestMinimumDistanceSquared)
{
    OutBestSelection.Reset();
    OutBestMinimumDistanceSquared = -1.0;
    if (SelectionCount <= 0 ||
        CanonicalCandidateRoots.Num() < SelectionCount)
    {
        return false;
    }

    const auto DistanceSquared = [&CanonicalCandidateRoots](
        int32 A,
        int32 B)
    {
        return FVector2D(
            CanonicalCandidateRoots[A].X - CanonicalCandidateRoots[B].X,
            CanonicalCandidateRoots[A].Y - CanonicalCandidateRoots[B].Y).
            SizeSquared();
    };

    for (int32 SeedIndex = 0;
         SeedIndex < CanonicalCandidateRoots.Num();
         ++SeedIndex)
    {
        TArray<int32> TrialSelection;
        TrialSelection.Reserve(SelectionCount);
        TArray<bool> bChosen;
        bChosen.Init(false, CanonicalCandidateRoots.Num());
        TArray<double> NearestDistanceSquared;
        NearestDistanceSquared.Init(
            TNumericLimits<double>::Max(),
            CanonicalCandidateRoots.Num());

        TrialSelection.Add(SeedIndex);
        bChosen[SeedIndex] = true;
        for (int32 CandidateIndex = 0;
             CandidateIndex < CanonicalCandidateRoots.Num();
             ++CandidateIndex)
        {
            if (!bChosen[CandidateIndex])
            {
                NearestDistanceSquared[CandidateIndex] = DistanceSquared(
                    CandidateIndex,
                    SeedIndex);
            }
        }

        double TrialMinimumDistanceSquared =
            TNumericLimits<double>::Max();
        while (TrialSelection.Num() < SelectionCount)
        {
            int32 FarthestIndex = INDEX_NONE;
            double FarthestNearestDistanceSquared = -1.0;
            for (int32 CandidateIndex = 0;
                 CandidateIndex < CanonicalCandidateRoots.Num();
                 ++CandidateIndex)
            {
                if (!bChosen[CandidateIndex] &&
                    NearestDistanceSquared[CandidateIndex] >
                        FarthestNearestDistanceSquared)
                {
                    FarthestIndex = CandidateIndex;
                    FarthestNearestDistanceSquared =
                        NearestDistanceSquared[CandidateIndex];
                }
            }
            if (FarthestIndex == INDEX_NONE)
            {
                OutBestSelection.Reset();
                OutBestMinimumDistanceSquared = -1.0;
                return false;
            }

            TrialSelection.Add(FarthestIndex);
            bChosen[FarthestIndex] = true;
            TrialMinimumDistanceSquared = FMath::Min(
                TrialMinimumDistanceSquared,
                FarthestNearestDistanceSquared);

            // The legacy traversal recomputed the minimum in selected-index
            // order. Updating once for the newly appended index performs the
            // same sequence of FMath::Min operations and therefore preserves
            // both floating-point results and strict canonical tie behaviour.
            if (TrialSelection.Num() < SelectionCount)
            {
                for (int32 CandidateIndex = 0;
                     CandidateIndex < CanonicalCandidateRoots.Num();
                     ++CandidateIndex)
                {
                    if (!bChosen[CandidateIndex])
                    {
                        NearestDistanceSquared[CandidateIndex] = FMath::Min(
                            NearestDistanceSquared[CandidateIndex],
                            DistanceSquared(CandidateIndex, FarthestIndex));
                    }
                }
            }
        }

        if (TrialMinimumDistanceSquared >
            OutBestMinimumDistanceSquared)
        {
            OutBestMinimumDistanceSquared = TrialMinimumDistanceSquared;
            OutBestSelection = MoveTemp(TrialSelection);
        }
    }
    return OutBestSelection.Num() == SelectionCount;
}

bool ATRIADIstanaExploreV5BVisualActor::BuildTreeGrounding(
    const TArray<FTRIADIstanaExploreV5BTreeSource>& TreeSources,
    FTRIADIstanaExploreV5BDeterministicLayout& InOutLayout,
    FString& OutError)
{
    InOutLayout.TreeBaseSourceComponentIndices.Reset();
    InOutLayout.TreeBaseSourceInstanceIndices.Reset();
    InOutLayout.TreeBaseMulchWorldTransforms.Reset();
    InOutLayout.TreeBaseShrubWorldTransforms.Reset();
    InOutLayout.TreeBaseUnderstoreyWorldTransforms.Reset();

    if (TreeSources.Num() != ExpectedV4TreeSourceCount ||
        InOutLayout.AccentTurfSourceIndices.Num() != AccentTurfCount ||
        InOutLayout.AccentTurfWorldTransforms.Num() != AccentTurfCount)
    {
        OutError = FString::Printf(
            TEXT("V5B tree grounding requires the exact %d-tree V4 census and %d ordered accent transforms."),
            ExpectedV4TreeSourceCount,
            AccentTurfCount);
        return false;
    }

    struct FGroundingCandidate
    {
        int32 SourceComponentIndex = INDEX_NONE;
        int32 SourceInstanceIndex = INDEX_NONE;
        FVector Root = FVector::ZeroVector;
        double RadiusCm = 0.0;
        uint32 OrderKey = 0u;
    };

    TArray<FGroundingCandidate> CandidatesBySide[2];
    TArray<FVector> UniqueRoots;
    UniqueRoots.Reserve(TreeSources.Num());
    TSet<uint64> UniqueSourceKeys;
    for (int32 SourceArrayIndex = 0;
         SourceArrayIndex < TreeSources.Num();
         ++SourceArrayIndex)
    {
        const FTRIADIstanaExploreV5BTreeSource& Source =
            TreeSources[SourceArrayIndex];
        if (!IsFinitePositiveTransform(Source.WorldTransform) ||
            !FMath::IsFinite(Source.SourceMeshHeightCm) ||
            Source.SourceMeshHeightCm <= 1.0 ||
            Source.SourceComponentIndex < 0 ||
            Source.SourceComponentIndex >= 10 ||
            Source.SourceInstanceIndex < 0)
        {
            OutError = FString::Printf(
                TEXT("V5B tree source %d is incomplete or non-finite."),
                SourceArrayIndex);
            return false;
        }
        const uint64 SourceKey =
            (static_cast<uint64>(static_cast<uint32>(Source.SourceComponentIndex)) << 32) |
            static_cast<uint32>(Source.SourceInstanceIndex);
        if (UniqueSourceKeys.Contains(SourceKey))
        {
            OutError = TEXT("V5B tree grounding found a duplicate component/instance source key.");
            return false;
        }
        UniqueSourceKeys.Add(SourceKey);

        const FVector Root = Source.WorldTransform.GetTranslation();
        bool bDuplicateRoot = false;
        for (const FVector& ExistingRoot : UniqueRoots)
        {
            if (FVector2D(Root.X - ExistingRoot.X, Root.Y - ExistingRoot.Y).
                    SizeSquared() <=
                FMath::Square(TreeGroundingDuplicateRootToleranceCm))
            {
                bDuplicateRoot = true;
                break;
            }
        }
        if (bDuplicateRoot)
        {
            continue;
        }
        UniqueRoots.Add(Root);

        const double AbsX = FMath::Abs(Root.X);
        if (AbsX < TreeGroundingMinimumAbsXCm ||
            AbsX > TreeGroundingMaximumAbsXCm ||
            Root.Y < TreeGroundingMinimumYCm ||
            Root.Y > TreeGroundingMaximumYCm ||
            IsInsideConservativeFormalBedVeneer(Root))
        {
            continue;
        }

        const int32 SideIndex = Root.X < 0.0 ? 0 : 1;
        const uint32 IdentityHash =
            static_cast<uint32>(Source.SourceComponentIndex) * 0x9E3779B9u ^
            static_cast<uint32>(Source.SourceInstanceIndex) * 0x85EBCA6Bu;
        FGroundingCandidate Candidate;
        Candidate.SourceComponentIndex = Source.SourceComponentIndex;
        Candidate.SourceInstanceIndex = Source.SourceInstanceIndex;
        Candidate.Root = Root;
        Candidate.RadiusCm = FMath::Clamp(
            Source.SourceMeshHeightCm *
                Source.WorldTransform.GetScale3D().Z *
                TreeBaseRadiusHeightRatio,
            TreeBaseRadiusMinCm,
            TreeBaseRadiusMaxCm);
        Candidate.OrderKey = MixStableHash(IdentityHash ^ 0xB45E91D3u);
        CandidatesBySide[SideIndex].Add(Candidate);
    }

    TArray<FGroundingCandidate> Selected;
    Selected.Reserve(TreeGroundingAnchorCount);
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        CandidatesBySide[SideIndex].Sort(
            [](const FGroundingCandidate& A, const FGroundingCandidate& B)
            {
                if (A.OrderKey != B.OrderKey)
                {
                    return A.OrderKey < B.OrderKey;
                }
                if (A.SourceComponentIndex != B.SourceComponentIndex)
                {
                    return A.SourceComponentIndex < B.SourceComponentIndex;
                }
                return A.SourceInstanceIndex < B.SourceInstanceIndex;
            });
        const TArray<FGroundingCandidate>& SideCandidates =
            CandidatesBySide[SideIndex];
        if (SideCandidates.Num() < TreeGroundingAnchorsPerSide)
        {
            OutError = FString::Printf(
                TEXT("V5B tree grounding has only %d/%d bounded candidates on the %s side."),
                SideCandidates.Num(),
                TreeGroundingAnchorsPerSide,
                SideIndex == 0 ? TEXT("left") : TEXT("right"));
            return false;
        }

        // Hash-first first-fit can strand a dense early candidate even when a
        // well-spaced 32-tree solution exists. Run one deterministic farthest-
        // point traversal from every canonical seed, then retain the trial
        // with the largest measured pairwise minimum. Canonical hash/component/
        // instance order and strict comparisons resolve every tie and keep the
        // result byte-stable. Cache each candidate's nearest selected distance
        // so every newly selected point is compared exactly once per candidate.
        TArray<FVector2D> CanonicalCandidateRoots;
        CanonicalCandidateRoots.Reserve(SideCandidates.Num());
        for (const FGroundingCandidate& Candidate : SideCandidates)
        {
            CanonicalCandidateRoots.Emplace(
                Candidate.Root.X,
                Candidate.Root.Y);
        }
        TArray<int32> BestSelection;
        double BestMinimumDistanceSquared = -1.0;
        if (!SelectCanonicalMaximinCandidateIndices(
                CanonicalCandidateRoots,
                TreeGroundingAnchorsPerSide,
                BestSelection,
                BestMinimumDistanceSquared))
        {
            OutError = TEXT("V5B tree grounding maximin selection exhausted its bounded candidates.");
            return false;
        }
        if (BestSelection.Num() != TreeGroundingAnchorsPerSide ||
            BestMinimumDistanceSquared <
                FMath::Square(TreeGroundingMinimumSpacingCm))
        {
            InOutLayout.TreeBaseSourceComponentIndices.Reset();
            InOutLayout.TreeBaseSourceInstanceIndices.Reset();
            InOutLayout.TreeBaseMulchWorldTransforms.Reset();
            InOutLayout.TreeBaseShrubWorldTransforms.Reset();
            InOutLayout.TreeBaseUnderstoreyWorldTransforms.Reset();
            OutError = FString::Printf(
                TEXT("V5B tree grounding maximin selection achieved only %.3f/%.3f cm minimum spacing for %d anchors on the %s side."),
                BestMinimumDistanceSquared > 0.0
                    ? FMath::Sqrt(BestMinimumDistanceSquared)
                    : 0.0,
                TreeGroundingMinimumSpacingCm,
                TreeGroundingAnchorsPerSide,
                SideIndex == 0 ? TEXT("left") : TEXT("right"));
            return false;
        }
        for (const int32 CandidateIndex : BestSelection)
        {
            Selected.Add(SideCandidates[CandidateIndex]);
        }
    }

    InOutLayout.TreeBaseSourceComponentIndices.Reserve(TreeGroundingAnchorCount);
    InOutLayout.TreeBaseSourceInstanceIndices.Reserve(TreeGroundingAnchorCount);
    InOutLayout.TreeBaseMulchWorldTransforms.Reserve(TreeGroundingAnchorCount);
    InOutLayout.TreeBaseShrubWorldTransforms.Reserve(TreeGroundingShrubCount);
    InOutLayout.TreeBaseUnderstoreyWorldTransforms.Reserve(
        TreeGroundingUnderstoreyCount);
    for (int32 AnchorIndex = 0;
         AnchorIndex < Selected.Num();
         ++AnchorIndex)
    {
        const FGroundingCandidate& Candidate = Selected[AnchorIndex];
        const uint32 IdentityHash =
            static_cast<uint32>(Candidate.SourceComponentIndex) * 0x9E3779B9u ^
            static_cast<uint32>(Candidate.SourceInstanceIndex) * 0x85EBCA6Bu;
        const double PatchYaw = 360.0 * HashUnitInterval(
            IdentityHash ^ 0xC31A78E5u);
        const FVector PatchLocation(
            Candidate.Root.X,
            Candidate.Root.Y,
            Candidate.Root.Z + TreeBasePatchZOffsetCm);
        InOutLayout.TreeBaseSourceComponentIndices.Add(
            Candidate.SourceComponentIndex);
        InOutLayout.TreeBaseSourceInstanceIndices.Add(
            Candidate.SourceInstanceIndex);
        InOutLayout.TreeBaseMulchWorldTransforms.Add(FTransform(
            FRotator(0.0, PatchYaw, 0.0),
            PatchLocation,
            FVector(
                Candidate.RadiusCm / TreeBasePatchSourceRadiusCm,
                Candidate.RadiusCm / TreeBasePatchSourceRadiusCm,
                1.0)));

        const int32 SideLocalIndex =
            AnchorIndex % TreeGroundingAnchorsPerSide;
        if (SideLocalIndex >= TreeGroundingPlantedAnchorsPerSide)
        {
            continue;
        }
        const auto ShrubClusterAngle = [IdentityHash](int32 ClusterIndex)
        {
            const uint32 ClusterHash = IdentityHash ^
                (0x37B4A2D1u +
                 static_cast<uint32>(ClusterIndex) * 0x9E3779B9u);
            return 2.0 * PI *
                (static_cast<double>(ClusterIndex) /
                     TreeGroundingShrubsPerPlantedAnchor +
                 0.12 *
                     (HashUnitInterval(ClusterHash ^ 0x194DE31Bu) - 0.5));
        };
        for (int32 PlantIndex = 0;
             PlantIndex < TreeGroundingShrubsPerPlantedAnchor;
             ++PlantIndex)
        {
            const uint32 PlantHash = IdentityHash ^
                (0x37B4A2D1u + static_cast<uint32>(PlantIndex) * 0x9E3779B9u);
            const double Angle = ShrubClusterAngle(PlantIndex);
            const double RadialFraction = FMath::Lerp(
                TreeBaseShrubMinimumRadialFraction,
                TreeBaseShrubMaximumRadialFraction,
                HashUnitInterval(PlantHash ^ 0x8AC1745Du));
            const double UniformScale = FMath::Lerp(
                TreeBaseShrubMinimumUniformScale,
                TreeBaseShrubMaximumUniformScale,
                HashUnitInterval(PlantHash ^ 0xE713C429u));
            InOutLayout.TreeBaseShrubWorldTransforms.Add(FTransform(
                FRotator(
                    0.0,
                    FMath::RadiansToDegrees(Angle) +
                        360.0 * HashUnitInterval(PlantHash ^ 0x63AE190Fu),
                    0.0),
                PatchLocation + FVector(
                    Candidate.RadiusCm * RadialFraction * FMath::Cos(Angle),
                    Candidate.RadiusCm * RadialFraction * FMath::Sin(Angle),
                    TreeBaseMoundSurfaceZCm(RadialFraction) -
                        TreeBaseShrubRootInsetCm),
                FVector(
                    UniformScale * TreeBaseShrubHorizontalScaleMultiplier,
                    UniformScale * TreeBaseShrubHorizontalScaleMultiplier,
                    UniformScale)));
        }
        for (int32 PlantIndex = 0;
             PlantIndex < TreeGroundingUnderstoreyPerPlantedAnchor;
             ++PlantIndex)
        {
            const uint32 PlantHash = IdentityHash ^
                (0x91E4C753u + static_cast<uint32>(PlantIndex) * 0x85EBCA6Bu);
            const int32 ClusterIndex = PlantIndex / 3;
            const int32 MemberIndex = PlantIndex % 3;
            const double Angle = ShrubClusterAngle(ClusterIndex) +
                static_cast<double>(MemberIndex - 1) *
                    TreeBaseUnderstoreyTriadAngleStepRadians;
            const double RadialFraction = FMath::Lerp(
                TreeBaseUnderstoreyMinimumRadialFraction,
                TreeBaseUnderstoreyMaximumRadialFraction,
                HashUnitInterval(PlantHash ^ 0xC56248EBu));
            const double UniformScale = FMath::Lerp(
                TreeBaseUnderstoreyMinimumUniformScale,
                TreeBaseUnderstoreyMaximumUniformScale,
                HashUnitInterval(PlantHash ^ 0x2AD91E47u));
            InOutLayout.TreeBaseUnderstoreyWorldTransforms.Add(FTransform(
                FRotator(
                    0.0,
                    360.0 * HashUnitInterval(PlantHash ^ 0xA73B50E9u),
                    0.0),
                PatchLocation + FVector(
                    Candidate.RadiusCm * RadialFraction * FMath::Cos(Angle),
                    Candidate.RadiusCm * RadialFraction * FMath::Sin(Angle),
                    TreeBaseMoundSurfaceZCm(RadialFraction) -
                        TreeBaseUnderstoreyRootInsetCm),
                FVector(
                    UniformScale *
                        TreeBaseUnderstoreyHorizontalScaleMultiplier,
                    UniformScale *
                        TreeBaseUnderstoreyHorizontalScaleMultiplier,
                    UniformScale)));
        }
    }

    // Recompute only Z so the exact accent count, source-index ordering, XY,
    // rotation and scale remain unchanged and repeated pure rebuilds are safe.
    for (FTransform& AccentTransform :
         InOutLayout.AccentTurfWorldTransforms)
    {
        FVector AccentLocation = AccentTransform.GetTranslation();
        AccentLocation.Z =
            AnalyticAccentTerrainHeightCm(AccentLocation.X, AccentLocation.Y) +
            AccentTerrainGapCm;
        if (IsInsideOrganicFormalBedSuppression(
                AccentLocation.X,
                AccentLocation.Y))
        {
            AccentLocation.Z -= FormalBedTurfSuppressionDepthCm;
        }
        for (const FTransform& PatchTransform :
             InOutLayout.TreeBaseMulchWorldTransforms)
        {
            const FVector PatchLocation = PatchTransform.GetTranslation();
            const double SuppressionRadiusCm =
                PatchTransform.GetScale3D().X *
                TreeBasePatchSourceRadiusCm *
                TreeBaseGrassSuppressionRadiusFraction;
            if (FVector2D(
                    AccentLocation.X - PatchLocation.X,
                    AccentLocation.Y - PatchLocation.Y).SizeSquared() <=
                FMath::Square(SuppressionRadiusCm))
            {
                AccentLocation.Z -= FormalBedTurfSuppressionDepthCm;
                break;
            }
        }
        AccentTransform.SetTranslation(AccentLocation);
    }

    if (!ValidateTransformArray(
            InOutLayout.TreeBaseMulchWorldTransforms,
            TreeGroundingAnchorCount,
            TEXT("tree-base mulch"),
            OutError) ||
        !ValidateTransformArray(
            InOutLayout.TreeBaseShrubWorldTransforms,
            TreeGroundingShrubCount,
            TEXT("tree-base shrub"),
            OutError) ||
        !ValidateTransformArray(
            InOutLayout.TreeBaseUnderstoreyWorldTransforms,
            TreeGroundingUnderstoreyCount,
            TEXT("tree-base understorey"),
            OutError) ||
        InOutLayout.TreeBaseSourceComponentIndices.Num() !=
            TreeGroundingAnchorCount ||
        InOutLayout.TreeBaseSourceInstanceIndices.Num() !=
            TreeGroundingAnchorCount)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5B tree-grounding source or render census drifted.");
        }
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::BuildFormalBedCorrections(
    const TArray<FTransform>& ShrubSourceWorldTransforms,
    const TArray<FTransform>& FlowerSourceWorldTransforms,
    const TArray<FTransform>& UnderstoreySourceWorldTransforms,
    FTRIADIstanaExploreV5BDeterministicLayout& InOutLayout,
    FString& OutError)
{
    InOutLayout.FormalBedVeneerWorldTransform = FTransform::Identity;
    InOutLayout.FormalBedShrubCorrectionWorldTransforms.Reset();
    InOutLayout.FormalBedFlowerCorrectionWorldTransforms.Reset();
    InOutLayout.FormalBedUnderstoreyCorrectionWorldTransforms.Reset();
    if (ShrubSourceWorldTransforms.Num() != FormalBedShrubSourceCount ||
        FlowerSourceWorldTransforms.Num() != FormalBedFlowerSourceCount ||
        UnderstoreySourceWorldTransforms.Num() != FormalBedUnderstoreySourceCount)
    {
        OutError = FString::Printf(
            TEXT("V5B full borrowed planting census must be shrub=%d flower=%d understorey=%d; actual %d/%d/%d."),
            FormalBedShrubSourceCount,
            FormalBedFlowerSourceCount,
            FormalBedUnderstoreySourceCount,
            ShrubSourceWorldTransforms.Num(),
            FlowerSourceWorldTransforms.Num(),
            UnderstoreySourceWorldTransforms.Num());
        return false;
    }

    const auto BuildCategory = [&OutError](
        const TArray<FTransform>& SourceTransforms,
        uint32 Salt,
        const TCHAR* Label,
        int32 RelocatedSourceCount,
        int32 InfillCount,
        int32 ClusterCount,
        double ClusterCenterExtentU,
        double ClusterCenterExtentV,
        double MinimumClusterRadiusU,
        double MaximumClusterRadiusU,
        double MinimumClusterRadiusV,
        double MaximumClusterRadiusV,
        double MaximumClusterSkew,
        double MinimumRadialFraction,
        double MaximumRadialFraction,
        double OuterBandPlacementFraction,
        double OuterRadialBiasExponent,
        double MinimumUniformScale,
        double MaximumUniformScale,
        double HorizontalScaleMultiplier,
        double MaximumPitchDegrees,
        double MaximumRollDegrees,
        TArray<FTransform>& OutTransforms) -> bool
    {
        if ((SourceTransforms.Num() % 2) != 0 ||
            (RelocatedSourceCount % 2) != 0 ||
            RelocatedSourceCount <= 0 ||
            RelocatedSourceCount > SourceTransforms.Num() ||
            InfillCount < 0 ||
            ClusterCount <= 0 ||
            MinimumRadialFraction < 0.0 ||
            MaximumRadialFraction > 0.98 ||
            MinimumRadialFraction >= MaximumRadialFraction ||
            OuterBandPlacementFraction < 0.0 ||
            OuterBandPlacementFraction > 1.0 ||
            OuterRadialBiasExponent <= 0.0 ||
            HorizontalScaleMultiplier < 1.0)
        {
            OutError = FString::Printf(
                TEXT("V5B formal-bed %s requires an even bilateral census and positive cluster count."),
                Label);
            return false;
        }
        OutTransforms.Reserve(SourceTransforms.Num() + InfillCount);
        TArray<FVector2D> AcceptedLocalPositionsBySide[2];
        const int32 PlacedCountPerSide =
            (RelocatedSourceCount + InfillCount) / 2;
        AcceptedLocalPositionsBySide[0].Reserve(PlacedCountPerSide);
        AcceptedLocalPositionsBySide[1].Reserve(PlacedCountPerSide);

        const auto AppendPlacedTransform = [
            &OutTransforms,
            &OutError,
            &AcceptedLocalPositionsBySide,
            Label,
            Salt,
            ClusterCount,
            ClusterCenterExtentU,
            ClusterCenterExtentV,
            MinimumClusterRadiusU,
            MaximumClusterRadiusU,
            MinimumClusterRadiusV,
            MaximumClusterRadiusV,
            MaximumClusterSkew,
            MinimumRadialFraction,
            MaximumRadialFraction,
            OuterBandPlacementFraction,
            OuterRadialBiasExponent,
            MinimumUniformScale,
            MaximumUniformScale,
            HorizontalScaleMultiplier,
            MaximumPitchDegrees,
            MaximumRollDegrees](
                const FTransform& Source,
                int32 PlacementIndex,
                int32 SideIndex,
                int32 BilateralOrdinal) -> bool
        {
            // A side-specific Cranley-Patterson rotation breaks exact mirror
            // clones while preserving the exact bilateral census. R2 cluster
            // centres and an eight-candidate best-choice search keep the mass
            // clustered but avoid the nursery rows produced by independent
            // hash radii and hard boundary clamping.
            const double SidePhase = SideIndex == 0
                ? 0.1732050807568877
                : 0.6180339887498948;
            const double ClusterSelector = FractionalPart(
                (static_cast<double>(BilateralOrdinal) + 0.5) *
                    0.6180339887498948 +
                SidePhase +
                HashUnitInterval(Salt + 0x6C8E9CF5u));
            const int32 ClusterIndex = static_cast<int32>(
                FMath::FloorToDouble(
                    ClusterSelector * static_cast<double>(ClusterCount)));
            const double ClusterU = FMath::Lerp(
                -ClusterCenterExtentU,
                ClusterCenterExtentU,
                FractionalPart(
                    (static_cast<double>(ClusterIndex) + 0.5) *
                        AccentR2IncrementX +
                    SidePhase +
                    HashUnitInterval(Salt + 0x19A7C5D3u)));
            const double ClusterV = FMath::Lerp(
                -ClusterCenterExtentV,
                ClusterCenterExtentV,
                FractionalPart(
                    (static_cast<double>(ClusterIndex) + 0.5) *
                        AccentR2IncrementY +
                    0.5 * SidePhase +
                    HashUnitInterval(Salt + 0xA6E31B47u)));
            const double ClusterRadiusU = FMath::Lerp(
                MinimumClusterRadiusU,
                MaximumClusterRadiusU,
                HashUnitInterval(
                    static_cast<uint32>(ClusterIndex) ^
                    (Salt + 0x4D82F1A9u)));
            const double ClusterRadiusV = FMath::Lerp(
                MinimumClusterRadiusV,
                MaximumClusterRadiusV,
                HashUnitInterval(
                    static_cast<uint32>(ClusterIndex) ^
                    (Salt + 0xC17B609Du)));
            const double ClusterSkew = FMath::Lerp(
                -MaximumClusterSkew,
                MaximumClusterSkew,
                HashUnitInterval(
                    static_cast<uint32>(ClusterIndex) ^
                    (Salt + 0xE2584A6Bu)));
            const double LobePhase = 2.0 * PI * HashUnitInterval(
                static_cast<uint32>(ClusterIndex) ^
                (Salt + 0x91D4C37Fu));

            constexpr int32 CandidateCount = 8;
            bool bHasCandidate = false;
            double BestNearestDistanceSquared = -1.0;
            double U = 0.0;
            double V = 0.0;
            FVector2D Local = FVector2D::ZeroVector;
            for (int32 CandidateIndex = 0;
                 CandidateIndex < CandidateCount;
                 ++CandidateIndex)
            {
                const double SequenceIndex =
                    static_cast<double>(PlacementIndex * CandidateCount +
                        CandidateIndex) +
                    0.5;
                const double RadiusUnit = FractionalPart(
                    SequenceIndex * AccentR2IncrementY +
                    SidePhase +
                    HashUnitInterval(Salt + 0x73E9A10Bu));
                double Radius = FMath::Sqrt(FMath::Lerp(
                    0.035,
                    0.98,
                    RadiusUnit));
                const double Angle = 2.0 * PI * FractionalPart(
                    SequenceIndex * AccentR2IncrementX +
                    0.5 * SidePhase +
                    HashUnitInterval(Salt + 0x2F54D687u));
                Radius *= 0.76 + 0.24 *
                    (0.5 + 0.5 * FMath::Sin(
                        3.0 * Angle + LobePhase));
                const double OffsetU =
                    Radius * ClusterRadiusU * FMath::Cos(Angle);
                const double OffsetV =
                    Radius * ClusterRadiusV * FMath::Sin(Angle);
                double RawU = ClusterU + OffsetU + ClusterSkew * OffsetV;
                double RawV = ClusterV + OffsetV - 0.12 * OffsetU;
                double RawRadius = FMath::Max(
                    FMath::Abs(RawU),
                    FMath::Abs(RawV));
                if (RawRadius < 0.000001)
                {
                    RawU = FMath::Cos(Angle);
                    RawV = FMath::Sin(Angle);
                    RawRadius = FMath::Max(
                        FMath::Abs(RawU),
                        FMath::Abs(RawV));
                }

                // Keep the cluster-derived direction, but explicitly assign
                // most low planting to the outer 34% radial band. A separate
                // side phase makes edge holes and spill-outs asymmetric while
                // the best-choice pass retains bounded blue-noise spacing.
                const double EdgeSelector = FractionalPart(
                    (static_cast<double>(PlacementIndex) + 0.5) *
                        0.6180339887498948 +
                    SidePhase +
                    HashUnitInterval(Salt + 0x5B17D3E9u));
                const bool bUseOuterBand =
                    EdgeSelector < OuterBandPlacementFraction &&
                    MaximumRadialFraction >
                        FormalBedOuterPlantBandMinimumRadius;
                const double InteriorMaximumRadius = FMath::Min(
                    MaximumRadialFraction,
                    FormalBedOuterPlantBandMinimumRadius - 0.025);
                const double TargetRadius = bUseOuterBand
                    ? FMath::Lerp(
                        FormalBedOuterPlantBandMinimumRadius,
                        MaximumRadialFraction,
                        FMath::Pow(
                            RadiusUnit,
                            OuterRadialBiasExponent))
                    : FMath::Lerp(
                        MinimumRadialFraction,
                        InteriorMaximumRadius,
                        FMath::Sqrt(RadiusUnit));
                const double CandidateU =
                    RawU * TargetRadius / RawRadius;
                const double CandidateV =
                    RawV * TargetRadius / RawRadius;
                const FVector2D CandidateLocal =
                    OrganicFormalBedLocalXY(CandidateU, CandidateV);
                double NearestDistanceSquared =
                    TNumericLimits<double>::Max();
                for (const FVector2D& AcceptedLocal :
                     AcceptedLocalPositionsBySide[SideIndex])
                {
                    NearestDistanceSquared = FMath::Min(
                        NearestDistanceSquared,
                        FVector2D::DistSquared(
                            CandidateLocal,
                            AcceptedLocal));
                }
                if (!bHasCandidate ||
                    NearestDistanceSquared > BestNearestDistanceSquared)
                {
                    bHasCandidate = true;
                    BestNearestDistanceSquared = NearestDistanceSquared;
                    U = CandidateU;
                    V = CandidateV;
                    Local = CandidateLocal;
                }
            }
            AcceptedLocalPositionsBySide[SideIndex].Add(Local);
            FVector Location(
                SideIndex == 0
                    ? -FormalBedCenterAbsXCm + Local.X
                    : FormalBedCenterAbsXCm - Local.X,
                FormalBedCenterYCm + Local.Y,
                0.0);
            Location.Z = FormalBedCoreDatumZCm +
                FormalBedReliefCm(U, V) - FormalBedRootInsetCm;
            FRotator Rotation = Source.Rotator();
            Rotation.Yaw = FRotator::NormalizeAxis(
                Rotation.Yaw + FMath::Lerp(
                    -178.0,
                    178.0,
                    HashUnitInterval(
                        static_cast<uint32>(PlacementIndex) ^
                        (Salt + 0x7C5E291Du))));
            Rotation.Pitch = FRotator::NormalizeAxis(
                Rotation.Pitch + FMath::Lerp(
                    -MaximumPitchDegrees,
                    MaximumPitchDegrees,
                    HashUnitInterval(
                        static_cast<uint32>(PlacementIndex) ^
                        (Salt + 0x38B4D62Fu))));
            Rotation.Roll = FRotator::NormalizeAxis(
                Rotation.Roll + FMath::Lerp(
                    -MaximumRollDegrees,
                    MaximumRollDegrees,
                    HashUnitInterval(
                        static_cast<uint32>(PlacementIndex) ^
                        (Salt + 0xF15A097Bu))));
            const double UniformScale = FMath::Lerp(
                MinimumUniformScale,
                MaximumUniformScale,
                HashUnitInterval(
                    static_cast<uint32>(PlacementIndex) ^
                    (Salt + 0xB4956E27u)));
            const FVector SourceScale = Source.GetScale3D();
            const FVector ScaleShape = SourceScale / SourceScale.X;
            FVector PlacedScale = ScaleShape * UniformScale;
            PlacedScale.X *= HorizontalScaleMultiplier;
            PlacedScale.Y *= HorizontalScaleMultiplier;
            const FTransform Correction(
                Rotation,
                Location,
                PlacedScale);
            const double AbsX = FMath::Abs(Location.X);
            if (!IsFinitePositiveTransform(Correction) ||
                AbsX < 1700.0 || AbsX > 5100.0 ||
                Location.Y < 7650.0 || Location.Y > 9950.0 ||
                !IsInsideConservativeFormalBedVeneer(Location))
            {
                OutTransforms.Reset();
                OutError = FString::Printf(
                    TEXT("V5B formal-bed %s correction %d escaped its render-only footprint."),
                    Label,
                    PlacementIndex);
                return false;
            }
            OutTransforms.Add(Correction);
            return true;
        };

        for (int32 Index = 0; Index < SourceTransforms.Num(); ++Index)
        {
            const FTransform& Source = SourceTransforms[Index];
            if (!IsFinitePositiveTransform(Source))
            {
                OutTransforms.Reset();
                OutError = FString::Printf(
                    TEXT("V5B formal-bed %s source transform %d is invalid."),
                    Label,
                    Index);
                return false;
            }
            // Every source plant is retained in the exact source-array order.
            // R9 moves the complete roster into the beds so none of the former
            // flank suffix remains as isolated lawn litter.
            if (Index >= RelocatedSourceCount)
            {
                OutTransforms.Add(Source);
                continue;
            }
            const int32 SideIndex = Index & 1;
            const int32 BilateralOrdinal = Index / 2;
            if (!AppendPlacedTransform(
                    Source,
                    Index,
                    SideIndex,
                    BilateralOrdinal))
            {
                return false;
            }
        }

        // Render-only density is appended after the complete borrowed roster.
        // All source indices remain stable while the additional instances fill
        // the sparse bed interiors.
        const int32 PrefixPairCount = RelocatedSourceCount / 2;
        for (int32 InfillIndex = 0; InfillIndex < InfillCount; ++InfillIndex)
        {
            const int32 SideIndex = InfillIndex & 1;
            const int32 SourcePairIndex = static_cast<int32>(
                MixStableHash(
                    static_cast<uint32>(InfillIndex) ^
                    (Salt + 0xD36F18A5u)) %
                static_cast<uint32>(PrefixPairCount));
            const int32 SourceIndex = 2 * SourcePairIndex + SideIndex;
            const int32 PlacementIndex =
                SourceTransforms.Num() + InfillIndex;
            const int32 BilateralOrdinal =
                RelocatedSourceCount / 2 + InfillIndex / 2;
            if (!AppendPlacedTransform(
                    SourceTransforms[SourceIndex],
                    PlacementIndex,
                    SideIndex,
                    BilateralOrdinal))
            {
                return false;
            }
        }
        OutError.Reset();
        return true;
    };

    // Tall shrubs form the interior backbone. Flowers and low Calathea fill
    // both the centre and edge instead of starving the bed interior; R11 adds
    // only small, symmetric understorey crowns to close the sunlit gaps
    // without enlarging the repeated source morphology. Modest edge-biased
    // subsets still interrupt the rendered soil seam. Source-array ordering
    // remains unchanged and the added density stays render-only inside the
    // conservative organic veneer.
    if (!BuildCategory(
            ShrubSourceWorldTransforms,
            0x1286A4D3u,
            TEXT("shrub"),
            FormalBedShrubRelocatedSourceCount,
            FormalBedShrubInfillCount,
            18,
            0.48,
            0.50,
            0.15,
            0.30,
            0.12,
            0.25,
            0.18,
            0.08,
            0.62,
            0.0,
            1.0,
            5.2,
            7.8,
            1.10,
            4.0,
            5.0,
            InOutLayout.FormalBedShrubCorrectionWorldTransforms) ||
        !BuildCategory(
            FlowerSourceWorldTransforms,
            0x64B9E217u,
            TEXT("flower"),
            FormalBedFlowerRelocatedSourceCount,
            FormalBedFlowerInfillCount,
            28,
            0.60,
            0.62,
            0.11,
            0.25,
            0.09,
            0.21,
            0.18,
            0.20,
            0.98,
            0.42,
            0.62,
            3.8,
            5.8,
            1.35,
            6.0,
            7.0,
            InOutLayout.FormalBedFlowerCorrectionWorldTransforms) ||
        !BuildCategory(
            UnderstoreySourceWorldTransforms,
            0xA73C508Du,
            TEXT("understorey"),
            FormalBedUnderstoreyRelocatedSourceCount,
            FormalBedUnderstoreyInfillCount,
            32,
            0.70,
            0.70,
            0.07,
            0.17,
            0.06,
            0.15,
            0.16,
            0.14,
            0.98,
            0.48,
            0.50,
            2.6,
            4.4,
            1.60,
            8.0,
            10.0,
            InOutLayout.FormalBedUnderstoreyCorrectionWorldTransforms) ||
        !ValidateTransformArray(
            InOutLayout.FormalBedShrubCorrectionWorldTransforms,
            FormalBedShrubRenderCount,
            TEXT("formal-bed shrub correction"),
            OutError) ||
        !ValidateTransformArray(
            InOutLayout.FormalBedFlowerCorrectionWorldTransforms,
            FormalBedFlowerRenderCount,
            TEXT("formal-bed flower correction"),
            OutError) ||
        !ValidateTransformArray(
            InOutLayout.FormalBedUnderstoreyCorrectionWorldTransforms,
            FormalBedUnderstoreyRenderCount,
            TEXT("formal-bed understorey correction"),
            OutError))
    {
        InOutLayout.FormalBedShrubCorrectionWorldTransforms.Reset();
        InOutLayout.FormalBedFlowerCorrectionWorldTransforms.Reset();
        InOutLayout.FormalBedUnderstoreyCorrectionWorldTransforms.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5BVisualActor::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!ApplyRuntimeFacadePresentation(Error))
    {
        UE_LOG(LogTRIADIstanaExploreV5BVisuals, Error,
            TEXT("ISTANA_EXPLORE_V5B_RUNTIME_FACADE_INVALID: %s"), *Error);
    }
}

bool ATRIADIstanaExploreV5BVisualActor::ApplyRuntimeFacadePresentation(FString& OutError)
{
    UStaticMeshComponent* Hero = PublicViewSceneActor
        ? PublicViewSceneActor->BuildingHeroVisualComponent.Get() : nullptr;
    const UStaticMesh* HeroMesh = Hero ? Hero->GetStaticMesh() : nullptr;
    if (!Hero || !HeroMesh || HeroMesh->GetStaticMaterials().Num() != 11 ||
        !FacadeFillLightComponent)
    {
        OutError = TEXT("exact eleven-slot V5B hero or facade fill is absent.");
        return false;
    }
    Hero->EmptyOverrideMaterials();
    RuntimeFacadeMaterialInstances.Reset(RuntimeFacadeMaterialCount);
    for (const FRuntimeFacadeMaterialSpec& Spec : RuntimeFacadeMaterialSpecs)
    {
        UMaterialInterface* Parent = HeroMesh->GetMaterial(Spec.Slot);
        UMaterialInstanceDynamic* Mid = Parent
            ? UMaterialInstanceDynamic::Create(Parent, this) : nullptr;
        if (!Mid)
        {
            Hero->EmptyOverrideMaterials();
            RuntimeFacadeMaterialInstances.Reset();
            OutError = FString::Printf(TEXT("failed to create transient facade MID for slot %d."), Spec.Slot);
            return false;
        }
        Mid->SetFlags(RF_Transient);
        Mid->SetVectorParameterValue(FacadeTintParameter, Spec.Tint);
        Mid->SetScalarParameterValue(FacadeRoughnessParameter, Spec.RoughnessBias);
        Hero->SetMaterial(Spec.Slot, Mid);
        RuntimeFacadeMaterialInstances.Add(Mid);
    }
    Hero->SetLightingChannels(true, true, false);
    FacadeFillLightComponent->SetIntensity(
        IsExactV5DPhotographicToneWorld(GetWorld())
            ? V5DPhotographicFacadeFillIntensityLux
            : DefaultFacadeFillIntensityLux);
    FacadeFillLightComponent->SetVisibility(true);
    bRuntimeFacadePresentationApplied = true;
    FString RuntimeReport;
    if (!ValidateRuntimeFacadePresentation(RuntimeReport))
    {
        bRuntimeFacadePresentationApplied = false;
        FacadeFillLightComponent->SetVisibility(false);
        FacadeFillLightComponent->SetIntensity(DefaultFacadeFillIntensityLux);
        Hero->SetLightingChannels(true, false, false);
        Hero->EmptyOverrideMaterials();
        RuntimeFacadeMaterialInstances.Reset();
        OutError = RuntimeReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ValidateRuntimeFacadePresentation(FString& OutReport) const
{
    const auto Fail = [&OutReport](const FString& Message) -> bool
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5B_RUNTIME_FACADE_INVALID: ") + Message;
        return false;
    };
    const UStaticMeshComponent* Hero = PublicViewSceneActor
        ? PublicViewSceneActor->BuildingHeroVisualComponent.Get() : nullptr;
    const UStaticMesh* HeroMesh = Hero ? Hero->GetStaticMesh() : nullptr;
    const bool bExactV5DPhotographicToneWorld =
        IsExactV5DPhotographicToneWorld(GetWorld());
    const float RequiredFacadeFillIntensityLux =
        ExpectedFacadeFillIntensityLux(GetWorld(), true);
    if (!bRuntimeFacadePresentationApplied || !Hero || !HeroMesh ||
        HeroMesh->GetStaticMaterials().Num() != 11 ||
        RuntimeFacadeMaterialInstances.Num() != RuntimeFacadeMaterialCount ||
        Hero->OverrideMaterials.Num() != 11 ||
        !Hero->LightingChannels.bChannel0 || !Hero->LightingChannels.bChannel1 ||
        Hero->LightingChannels.bChannel2)
    {
        return Fail(TEXT("runtime state, exact six-MID roster, or hero channels drifted."));
    }
    int32 MidIndex = 0;
    for (const FRuntimeFacadeMaterialSpec& Spec : RuntimeFacadeMaterialSpecs)
    {
        const UMaterialInstanceDynamic* Mid = Cast<UMaterialInstanceDynamic>(Hero->OverrideMaterials[Spec.Slot]);
        FLinearColor Tint;
        float RoughnessBias = 0.0f;
        if (!Mid || Mid != RuntimeFacadeMaterialInstances[MidIndex] ||
            Mid->Parent != HeroMesh->GetMaterial(Spec.Slot) || !Mid->HasAnyFlags(RF_Transient) ||
            !Mid->GetVectorParameterValue(
                FHashedMaterialParameterInfo(FacadeTintParameter), Tint, true) ||
            !Tint.Equals(Spec.Tint, 0.00001f) ||
            !Mid->GetScalarParameterValue(
                FHashedMaterialParameterInfo(FacadeRoughnessParameter),
                RoughnessBias,
                true) ||
            !FMath::IsNearlyEqual(RoughnessBias, Spec.RoughnessBias, 0.00001f))
        {
            return Fail(FString::Printf(TEXT("facade MID binding or parameter drifted at slot %d."), Spec.Slot));
        }
        ++MidIndex;
    }
    for (int32 Slot = 0; Slot < 11; ++Slot)
    {
        bool bRuntimeSlot = false;
        for (const FRuntimeFacadeMaterialSpec& Spec : RuntimeFacadeMaterialSpecs)
        {
            bRuntimeSlot |= Spec.Slot == Slot;
        }
        if (!bRuntimeSlot && Hero->OverrideMaterials[Slot] != nullptr)
        {
            return Fail(FString::Printf(TEXT("non-facade hero slot %d gained an override."), Slot));
        }
    }
    if (!FacadeFillLightComponent || !FacadeFillLightComponent->IsVisible() ||
        !FacadeFillLightComponent->LightingChannels.bChannel1 ||
        FacadeFillLightComponent->LightingChannels.bChannel0 ||
        FacadeFillLightComponent->LightingChannels.bChannel2 ||
        !FMath::IsNearlyEqual(
            FacadeFillLightComponent->Intensity,
            RequiredFacadeFillIntensityLux) ||
        FacadeFillLightComponent->CastShadows ||
        FacadeFillLightComponent->bCastVolumetricShadow ||
        FacadeFillLightComponent->bAffectReflection ||
        FacadeFillLightComponent->bAffectGlobalIllumination ||
        !FMath::IsNearlyZero(FacadeFillLightComponent->IndirectLightingIntensity) ||
        !FMath::IsNearlyZero(FacadeFillLightComponent->VolumetricScatteringIntensity))
    {
        return Fail(FString::Printf(
            TEXT("channel-isolated shadowless fill drifted; expected %.0f lux for exactV5DMap=%s."),
            RequiredFacadeFillIntensityLux,
            bExactV5DPhotographicToneWorld ? TEXT("true") : TEXT("false")));
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5B_RUNTIME_FACADE_VALID heroMids=6 slots=1,6,7,8,9,10 directFrozenV5Parents=true transient=true heroLightingChannels=0+1 fillLights=1 fillLux=%.0f fillIntensityPolicy=%s fillLightingChannel=1 fillShadows=false fillGI=false fillVolumetrics=false"),
        RequiredFacadeFillIntensityLux,
        bExactV5DPhotographicToneWorld
            ? TEXT("exactV5DMapScoped1100")
            : TEXT("crossMapDefault2500"));
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ValidateOwnedComponentTopology(
    FString& OutError) const
{
    const float RequiredFacadeFillIntensityLux =
        ExpectedFacadeFillIntensityLux(
            GetWorld(),
            bRuntimeFacadePresentationApplied);
    if (!SceneRoot || GetRootComponent() != SceneRoot ||
        SceneRoot->Mobility != EComponentMobility::Static ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001))
    {
        OutError = TEXT("V5B requires its exact static root and identity actor transform.");
        return false;
    }

    struct FNamedPrimitive
    {
        const TCHAR* ExpectedName = nullptr;
        const UPrimitiveComponent* Component = nullptr;
    };
    const TArray<FNamedPrimitive> Components = {
        {TEXT("V5BAccentTurfInstances"), AccentTurfInstances},
        {TEXT("V5BHardscapeNoLegacyBedsRenderSuccessor"), HardscapeRenderSuccessorComponent},
        {TEXT("V5BFormalBedsOrganicVeneer"), FormalBedVeneerComponent},
        {TEXT("V5BFormalBedShrubCorrections"), FormalBedShrubCorrections},
        {TEXT("V5BFormalBedFlowerCorrections"), FormalBedFlowerCorrections},
        {TEXT("V5BFormalBedUnderstoreyCorrections"), FormalBedUnderstoreyCorrections},
        {TEXT("V5BTreeBaseMulchInstances"), TreeBaseMulchInstances},
        {TEXT("V5BTreeBaseShrubInstances"), TreeBaseShrubInstances},
        {TEXT("V5BTreeBaseUnderstoreyInstances"), TreeBaseUnderstoreyInstances},
        {TEXT("V5BFountainSurface"), FountainSurfaceComponent},
        {TEXT("V5BFountainEdgeFoam"), FountainEdgeFoamComponent},
        {TEXT("V5BOuterPlumeInstances"), OuterPlumeInstances},
        {TEXT("V5BImpactRingInstances"), ImpactRingInstances},
        {TEXT("V5BCentralPlume"), CentralPlumeComponent},
        {TEXT("V5BInnerPaverInstances"), InnerPaverInstances},
        {TEXT("V5BOuterPaverInstances"), OuterPaverInstances},
        {TEXT("V5BPachiraBarkAInstances"), PachiraBarkAInstances},
        {TEXT("V5BPachiraBarkBInstances"), PachiraBarkBInstances},
        {TEXT("V5BPachiraBarkCInstances"), PachiraBarkCInstances},
        {TEXT("V5BPachiraBarkDInstances"), PachiraBarkDInstances},
        {TEXT("V5BPachiraLeavesAInstances"), PachiraLeavesAInstances},
        {TEXT("V5BPachiraLeavesBInstances"), PachiraLeavesBInstances},
        {TEXT("V5BPachiraLeavesCInstances"), PachiraLeavesCInstances},
        {TEXT("V5BPachiraLeavesDInstances"), PachiraLeavesDInstances}};
    if (Components.Num() != 24)
    {
        OutError = TEXT("V5B exact primitive roster must contain 24 components.");
        return false;
    }
    TSet<const UPrimitiveComponent*> UniqueComponents;
    for (const FNamedPrimitive& Row : Components)
    {
        if (!Row.Component ||
            Row.Component->GetFName() != FName(Row.ExpectedName) ||
            Row.Component->GetAttachParent() != SceneRoot ||
            !Row.Component->GetRelativeTransform().Equals(
                FTransform::Identity,
                0.001) ||
            Row.Component->GetOwner() != this ||
            UniqueComponents.Contains(Row.Component) ||
            !ValidateRenderOnlyPrimitive(
                Row.Component,
                Row.ExpectedName,
                OutError))
        {
            if (OutError.IsEmpty())
            {
                OutError = FString::Printf(
                    TEXT("V5B exact component topology drifted at '%s'."),
                    Row.ExpectedName);
            }
            return false;
        }
        UniqueComponents.Add(Row.Component);
    }

    TInlineComponentArray<UPrimitiveComponent*> ActualPrimitives(this);
    TArray<const UPrimitiveComponent*> ActualRenderPrimitives;
    TArray<const UPrimitiveComponent*> VisualizationPrimitives;
    for (const UPrimitiveComponent* Primitive : ActualPrimitives)
    {
        if (!Primitive)
        {
            continue;
        }
#if WITH_EDITORONLY_DATA
        if (Primitive->IsVisualizationComponent())
        {
            VisualizationPrimitives.Add(Primitive);
            continue;
        }
#endif
        ActualRenderPrimitives.Add(Primitive);
    }
    if (ActualRenderPrimitives.Num() != Components.Num())
    {
        OutError = FString::Printf(
            TEXT("V5B owns an unexpected primitive: expected %d, found %d."),
            Components.Num(),
            ActualRenderPrimitives.Num());
        return false;
    }
    for (const UPrimitiveComponent* Primitive : ActualRenderPrimitives)
    {
        if (!UniqueComponents.Contains(Primitive))
        {
            OutError = FString::Printf(
                TEXT("V5B owns an unexpected primitive '%s'."),
                Primitive ? *Primitive->GetName() : TEXT("<null>"));
            return false;
        }
    }
    // In a registered non-game editor world USceneComponent creates one
    // transient billboard for a visualized light.  It is absent otherwise and
    // is not render geometry.  Admit that exact engine-owned sprite only.
    const bool bRequiresEditorFillSprite =
        FacadeFillLightComponent && FacadeFillLightComponent->IsRegistered() &&
        GetWorld() && !GetWorld()->IsGameWorld();
    const int32 ExpectedVisualizationCount = bRequiresEditorFillSprite ? 1 : 0;
    if (VisualizationPrimitives.Num() != ExpectedVisualizationCount ||
        (ExpectedVisualizationCount == 1 &&
         (VisualizationPrimitives[0]->GetClass() !=
              UBillboardComponent::StaticClass() ||
          VisualizationPrimitives[0]->GetOwner() != this ||
          VisualizationPrimitives[0]->GetAttachParent() !=
              FacadeFillLightComponent.Get() ||
          !VisualizationPrimitives[0]->HasAllFlags(
              RF_Transient | RF_TextExportTransient))))
    {
        OutError = TEXT("V5B editor-only facade-fill visualization topology drifted.");
        return false;
    }
    TInlineComponentArray<UDirectionalLightComponent*> DirectionalLights(this);
    if (DirectionalLights.Num() != 1 ||
        DirectionalLights[0] != FacadeFillLightComponent ||
        !FacadeFillLightComponent ||
        FacadeFillLightComponent->GetFName() != FName(TEXT("V5BFacadeFillLight")) ||
        FacadeFillLightComponent->GetAttachParent() != SceneRoot ||
        FacadeFillLightComponent->GetOwner() != this ||
        !FacadeFillLightComponent->LightingChannels.bChannel1 ||
        FacadeFillLightComponent->LightingChannels.bChannel0 ||
        FacadeFillLightComponent->LightingChannels.bChannel2 ||
        !FMath::IsNearlyEqual(
            FacadeFillLightComponent->Intensity,
            RequiredFacadeFillIntensityLux) ||
        FacadeFillLightComponent->CastShadows ||
        FacadeFillLightComponent->bCastVolumetricShadow ||
        FacadeFillLightComponent->bAffectReflection ||
        FacadeFillLightComponent->bAffectGlobalIllumination ||
        !FMath::IsNearlyZero(FacadeFillLightComponent->IndirectLightingIntensity) ||
        !FMath::IsNearlyZero(FacadeFillLightComponent->VolumetricScatteringIntensity) ||
        FacadeFillLightComponent->IsVisible() != bRuntimeFacadePresentationApplied)
    {
        OutError = FString::Printf(
            TEXT("V5B exact cold/runtime facade fill topology drifted; expected %.0f lux for runtimeApplied=%s exactV5DMap=%s."),
            RequiredFacadeFillIntensityLux,
            bRuntimeFacadePresentationApplied ? TEXT("true") : TEXT("false"),
            IsExactV5DPhotographicToneWorld(GetWorld())
                ? TEXT("true")
                : TEXT("false"));
        return false;
    }
    int32 AccentStartCull = 0;
    int32 AccentEndCull = 0;
    AccentTurfInstances->GetCullDistances(AccentStartCull, AccentEndCull);
    const bool bV5DSourceTurfPresentationTag = Tags.Contains(
        ATRIADIstanaExploreV5DGroundVegetationActor::
            RuntimeSourceTurfPresentationTag());
    FString V5DSourceTurfPresentationReport;
    const bool bV5DSourceTurfPresentation =
        bV5DSourceTurfPresentationTag &&
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ValidateActiveRuntimeSourceTurfPresentationForSourceActor(
                this,
                V5DSourceTurfPresentationReport);
    if (bV5DSourceTurfPresentationTag && !bV5DSourceTurfPresentation)
    {
        OutError = TEXT("V5B component topology rejected an invalid tagged V5D inherited-turf presentation: ") +
            V5DSourceTurfPresentationReport;
        return false;
    }
    const int32 ExpectedAccentStartCull = bV5DSourceTurfPresentation
        ? ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedRuntimeSourceTurfCullStartDistanceCm()
        : 3000;
    const int32 ExpectedAccentEndCull = bV5DSourceTurfPresentation
        ? ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedRuntimeSourceTurfCullEndDistanceCm()
        : 5200;
    const float ExpectedAccentLodDistanceScale = bV5DSourceTurfPresentation
        ? ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedRuntimeSourceTurfLodDistanceScale()
        : 1.0f;
    if (AccentStartCull != ExpectedAccentStartCull ||
        AccentEndCull != ExpectedAccentEndCull ||
        !FMath::IsNearlyEqual(
            AccentTurfInstances->InstanceLODDistanceScale,
            ExpectedAccentLodDistanceScale,
            0.0001f) ||
        AccentTurfInstances->WorldPositionOffsetDisableDistance != 3200 ||
        AccentTurfInstances->bEnableDensityScaling)
    {
        OutError = TEXT("V5B accent turf cold/composite culling, LOD-distance, WPO-disable, or density-scaling policy drifted.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ExtractCloseTurfWorldTransforms(
    const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
    TArray<FTransform>& OutTransforms,
    FString& OutError) const
{
    OutTransforms.Reset();
    if (!SourceActor ||
        SourceActor->GetClass() !=
            ATRIADIstanaExploreV4LandscapeActor::StaticClass() ||
        !SourceActor->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SourceActor->CloseTurfInstances ||
        SourceActor->CloseTurfInstances->GetFName() !=
            ExpectedCloseTurfComponentName ||
        SourceActor->CloseTurfInstances->Mobility !=
            EComponentMobility::Static ||
        SourceActor->CloseTurfInstances->IsAsyncBuilding() ||
        !SourceActor->CloseTurfInstances->IsTreeFullyBuilt() ||
        SourceActor->CloseTurfInstances->GetInstanceCount() !=
            CloseTurfSourceCount)
    {
        OutError = TEXT("V5B requires the exact identity native V4 actor and its ordered 18,432-instance close-turf component.");
        return false;
    }
    OutTransforms.Reserve(CloseTurfSourceCount);
    for (int32 Index = 0; Index < CloseTurfSourceCount; ++Index)
    {
        FTransform WorldTransform = FTransform::Identity;
        if (!SourceActor->CloseTurfInstances->GetInstanceTransform(
                Index,
                WorldTransform,
                true) ||
            !IsFinitePositiveTransform(WorldTransform))
        {
            OutTransforms.Reset();
            OutError = FString::Printf(
                TEXT("V5B failed to read finite V4 close-turf instance %d."),
                Index);
            return false;
        }
        OutTransforms.Add(WorldTransform);
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ExtractFormalBedSourceWorldTransforms(
    const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
    TArray<FTransform>& OutShrubs,
    TArray<FTransform>& OutFlowers,
    TArray<FTransform>& OutUnderstorey,
    FString& OutError) const
{
    OutShrubs.Reset();
    OutFlowers.Reset();
    OutUnderstorey.Reset();
    if (!SourceActor ||
        !SourceActor->ShrubInstances ||
        !SourceActor->FlowerInstances ||
        !SourceActor->UnderstoreyInstances ||
        SourceActor->ShrubInstances->GetFName() != FName(TEXT("V4LayeredShrubs")) ||
        SourceActor->FlowerInstances->GetFName() != FName(TEXT("V4FloweringAccents")) ||
        SourceActor->UnderstoreyInstances->GetFName() != FName(TEXT("V4TropicalUnderstorey")) ||
        SourceActor->ShrubInstances->GetInstanceCount() !=
            FormalBedShrubSourceCount ||
        SourceActor->FlowerInstances->GetInstanceCount() !=
            FormalBedFlowerSourceCount ||
        SourceActor->UnderstoreyInstances->GetInstanceCount() !=
            FormalBedUnderstoreySourceCount ||
        SourceActor->ShrubInstances->IsAsyncBuilding() ||
        SourceActor->FlowerInstances->IsAsyncBuilding() ||
        SourceActor->UnderstoreyInstances->IsAsyncBuilding() ||
        !SourceActor->ShrubInstances->IsTreeFullyBuilt() ||
        !SourceActor->FlowerInstances->IsTreeFullyBuilt() ||
        !SourceActor->UnderstoreyInstances->IsTreeFullyBuilt())
    {
        OutError = TEXT("V5B requires the exact ordered full V4 shrub/flower/understorey HISM rosters.");
        return false;
    }

    const auto ExtractRoster = [&OutError](
        const UHierarchicalInstancedStaticMeshComponent* Component,
        int32 Count,
        const TCHAR* Label,
        TArray<FTransform>& OutTransforms) -> bool
    {
        OutTransforms.Reserve(Count);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            FTransform Transform = FTransform::Identity;
            if (!Component->GetInstanceTransform(Index, Transform, true) ||
                !IsFinitePositiveTransform(Transform))
            {
                OutTransforms.Reset();
                OutError = FString::Printf(
                    TEXT("V5B failed to read finite V4 %s source transform %d."),
                    Label,
                    Index);
                return false;
            }
            OutTransforms.Add(Transform);
        }
        return true;
    };
    if (!ExtractRoster(
            SourceActor->ShrubInstances,
            FormalBedShrubSourceCount,
            TEXT("shrub"),
            OutShrubs) ||
        !ExtractRoster(
            SourceActor->FlowerInstances,
            FormalBedFlowerSourceCount,
            TEXT("flower"),
            OutFlowers) ||
        !ExtractRoster(
            SourceActor->UnderstoreyInstances,
            FormalBedUnderstoreySourceCount,
            TEXT("understorey"),
            OutUnderstorey))
    {
        OutShrubs.Reset();
        OutFlowers.Reset();
        OutUnderstorey.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ExtractTreeSources(
    const ATRIADIstanaExploreV4LandscapeActor* SourceActor,
    TArray<FTRIADIstanaExploreV5BTreeSource>& OutSources,
    FString& OutError) const
{
    OutSources.Reset();
    FString V4Report;
    if (!SourceActor ||
        SourceActor->GetClass() !=
            ATRIADIstanaExploreV4LandscapeActor::StaticClass() ||
        !SourceActor->ValidateExploreV4Landscape(V4Report))
    {
        OutError = FString::Printf(
            TEXT("V5B tree grounding requires the fully valid, exact V4 tree census: %s"),
            V4Report.IsEmpty() ? TEXT("source actor missing") : *V4Report);
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* Components[] = {
        SourceActor->UmbrellaTreeInstances,
        SourceActor->DomeTreeInstances,
        SourceActor->HighForkRoundedTreeInstances,
        SourceActor->ColumnarNarrowTreeInstances,
        SourceActor->PalmTreeInstances,
        SourceActor->HeritageUmbrellaInstances,
        SourceActor->HeritageDomeInstances,
        SourceActor->HeritageHighForkRoundedInstances,
        SourceActor->HeritageColumnarNarrowInstances,
        SourceActor->HeritagePalmInstances};
    const FName ExpectedNames[] = {
        FName(TEXT("V4UmbrellaTreeReclassification")),
        FName(TEXT("V4DomeTreeReclassification")),
        FName(TEXT("V4HighForkRoundedTreeReclassification")),
        FName(TEXT("V4ColumnarNarrowTreeReclassification")),
        FName(TEXT("V4PalmTreeReclassification")),
        FName(TEXT("V4HeritageUmbrellaSilhouetteProxies")),
        FName(TEXT("V4HeritageDomeSilhouetteProxies")),
        FName(TEXT("V4HeritageHighForkSilhouetteProxies")),
        FName(TEXT("V4HeritageColumnarSilhouetteProxies")),
        FName(TEXT("V4HeritagePalmSilhouetteProxies"))};
    static_assert(UE_ARRAY_COUNT(Components) == UE_ARRAY_COUNT(ExpectedNames));

    const bool bV5DTreeRuntimeOverride = SourceActor->Tags.Contains(
        ATRIADIstanaExploreV5DTreeRealismActor::RuntimeOverrideTag());
    const int32 ExpectedTreeMinLod = bV5DTreeRuntimeOverride
        ? ATRIADIstanaExploreV5DTreeRealismActor::
            ExpectedRuntimeTreeMinimumLod()
        : 1;
    const int32 ExpectedTreeForcedLodModel = bV5DTreeRuntimeOverride
        ? ATRIADIstanaExploreV5DTreeRealismActor::
            ExpectedRuntimeTreeForcedLodModel()
        : 0;

    int32 MainCount = 0;
    int32 HeritageCount = 0;
    OutSources.Reserve(ExpectedV4TreeSourceCount);
    for (int32 ComponentIndex = 0;
         ComponentIndex < UE_ARRAY_COUNT(Components);
         ++ComponentIndex)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            Components[ComponentIndex];
        const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
        if (!Component || Component->GetFName() != ExpectedNames[ComponentIndex] ||
            !Mesh || Mesh->IsCompiling() ||
            Component->Mobility != EComponentMobility::Static ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->GetGenerateOverlapEvents() ||
            Component->CanEverAffectNavigation() ||
            !Component->IsVisible() || Component->bHiddenInGame ||
            !Component->bOverrideMinLOD ||
            Component->MinLOD != ExpectedTreeMinLod ||
            Component->ForcedLodModel != ExpectedTreeForcedLodModel ||
            Component->IsAsyncBuilding() || !Component->IsTreeFullyBuilt() ||
            !Component->bAutoRebuildTreeOnInstanceChanges)
        {
            OutSources.Reset();
            OutError = FString::Printf(
                TEXT("V5B read-only tree source component %d drifted from its exact V4 render/collision/navigation/LOD contract."),
                ComponentIndex);
            return false;
        }
        const double MeshHeightCm = Mesh->GetBounds().BoxExtent.Z * 2.0;
        if (!FMath::IsFinite(MeshHeightCm) || MeshHeightCm <= 1.0)
        {
            OutSources.Reset();
            OutError = TEXT("V5B tree grounding found an invalid V4 source mesh height.");
            return false;
        }
        const int32 InstanceCount = Component->GetInstanceCount();
        if (ComponentIndex < 5)
        {
            MainCount += InstanceCount;
        }
        else
        {
            HeritageCount += InstanceCount;
        }
        for (int32 InstanceIndex = 0;
             InstanceIndex < InstanceCount;
             ++InstanceIndex)
        {
            FTransform WorldTransform = FTransform::Identity;
            if (!Component->GetInstanceTransform(
                    InstanceIndex,
                    WorldTransform,
                    true) ||
                !IsFinitePositiveTransform(WorldTransform))
            {
                OutSources.Reset();
                OutError = FString::Printf(
                    TEXT("V5B could not read exact V4 tree component %d instance %d."),
                    ComponentIndex,
                    InstanceIndex);
                return false;
            }
            FTRIADIstanaExploreV5BTreeSource Source;
            Source.WorldTransform = WorldTransform;
            Source.SourceMeshHeightCm = MeshHeightCm;
            Source.SourceComponentIndex = ComponentIndex;
            Source.SourceInstanceIndex = InstanceIndex;
            OutSources.Add(Source);
        }
    }
    if (MainCount != ExpectedV4MainTreeCount ||
        HeritageCount != ExpectedV4HeritageTreeCount ||
        OutSources.Num() != ExpectedV4TreeSourceCount)
    {
        OutSources.Reset();
        OutError = FString::Printf(
            TEXT("V5B exact V4 tree census drifted: main=%d/%d heritage=%d/%d."),
            MainCount,
            ExpectedV4MainTreeCount,
            HeritageCount,
            ExpectedV4HeritageTreeCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ValidateHardscapeRenderSuccessorSource(
    const ATRIADIstanaPublicViewSceneActor* SourceActor,
    const FTRIADIstanaExploreV5BAssetRoster& Assets,
    FString& OutError) const
{
    const UStaticMeshComponent* SourceComponent =
        SourceActor ? SourceActor->HardscapeComponent.Get() : nullptr;
    const UStaticMesh* SourceMesh =
        SourceComponent ? SourceComponent->GetStaticMesh() : nullptr;
    const UStaticMesh* SuccessorMesh = Assets.HardscapeRenderSuccessorMesh;
    if (!SourceActor ||
        SourceActor->GetClass() !=
            ATRIADIstanaPublicViewSceneActor::StaticClass() ||
        !SourceActor->SceneRoot ||
        SourceActor->GetRootComponent() != SourceActor->SceneRoot ||
        !SourceActor->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SourceComponent ||
        SourceComponent->GetFName() !=
            FName(TEXT("PublicForecourtHardscape")) ||
        SourceComponent->GetOwner() != SourceActor ||
        SourceComponent->GetAttachParent() != SourceActor->SceneRoot ||
        !SourceComponent->GetRelativeTransform().Equals(
            FTransform::Identity,
            0.001) ||
        SourceComponent->Mobility != EComponentMobility::Static ||
        SourceComponent->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics ||
        !SourceMesh ||
        SourceMesh->GetPathName() != ExpectedSourceHardscapeObjectPath ||
        SourceMesh->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) ||
        SourceMesh->IsCompiling())
    {
        OutError = TEXT("V5B requires the exact identity public-view scene and PublicForecourtHardscape source mesh with QueryAndPhysics collision authority.");
        return false;
    }
    if (!SuccessorMesh || SuccessorMesh == SourceMesh ||
        SuccessorMesh->GetStaticMaterials().Num() !=
            UE_ARRAY_COUNT(ExpectedHardscapeMaterialSlotNames) ||
        SourceMesh->GetStaticMaterials().Num() != 4 ||
        Assets.HardscapeRenderSuccessorMaterials.Num() !=
            UE_ARRAY_COUNT(ExpectedHardscapeMaterialSlotNames))
    {
        OutError = TEXT("V5B hardscape successor/source material topology must be distinct and exactly three/four slots.");
        return false;
    }

    const int32 PlantingIndex =
        SourceMesh->GetMaterialIndex(ExpectedHardscapePlantingSlotName);
    bool bPlantingImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
    bPlantingImportedSlotNameMatches =
        PlantingIndex != INDEX_NONE &&
        SourceMesh->GetStaticMaterials().IsValidIndex(PlantingIndex) &&
        SourceMesh->GetStaticMaterials()[PlantingIndex].
            ImportedMaterialSlotName == ExpectedHardscapePlantingSlotName;
#endif
    if (PlantingIndex == INDEX_NONE ||
        !SourceMesh->GetStaticMaterials().IsValidIndex(PlantingIndex) ||
        SourceMesh->GetStaticMaterials()[PlantingIndex].MaterialSlotName !=
            ExpectedHardscapePlantingSlotName ||
        !bPlantingImportedSlotNameMatches)
    {
        OutError = TEXT("V5B frozen source hardscape lost its exact legacy planting slot identity.");
        return false;
    }

    for (int32 ExpectedIndex = 0;
         ExpectedIndex < UE_ARRAY_COUNT(ExpectedHardscapeMaterialSlotNames);
         ++ExpectedIndex)
    {
        const FName ExpectedName =
            ExpectedHardscapeMaterialSlotNames[ExpectedIndex];
        const FStaticMaterial& SuccessorMaterial =
            SuccessorMesh->GetStaticMaterials()[ExpectedIndex];
        const int32 SourceIndex =
            SourceMesh->GetMaterialIndex(ExpectedName);
        if (SourceIndex == INDEX_NONE ||
            !SourceMesh->GetStaticMaterials().IsValidIndex(SourceIndex))
        {
            OutError = FString::Printf(
                TEXT("V5B frozen source hardscape is missing required slot '%s'."),
                *ExpectedName.ToString());
            return false;
        }
        const FStaticMaterial& SourceMaterial =
            SourceMesh->GetStaticMaterials()[SourceIndex];
        const UMaterialInterface* EffectiveSourceMaterial =
            SourceComponent->GetMaterial(SourceIndex);
        bool bImportedSlotNamesMatch = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNamesMatch =
            SuccessorMaterial.ImportedMaterialSlotName == ExpectedName &&
            SourceMaterial.ImportedMaterialSlotName == ExpectedName;
#endif
        if (SuccessorMaterial.MaterialSlotName != ExpectedName ||
            SourceMaterial.MaterialSlotName != ExpectedName ||
            !bImportedSlotNamesMatch ||
            !SourceMaterial.MaterialInterface ||
            SuccessorMaterial.MaterialInterface !=
                SourceMaterial.MaterialInterface ||
            !EffectiveSourceMaterial ||
            Assets.HardscapeRenderSuccessorMaterials[ExpectedIndex].Get() !=
                EffectiveSourceMaterial)
        {
            OutError = FString::Printf(
                TEXT("V5B hardscape successor slot '%s' no longer preserves its exact frozen/effective source binding."),
                *ExpectedName.ToString());
            return false;
        }
    }

    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5BVisualActor::ClearOwnedVisuals()
{
    const TArray<UHierarchicalInstancedStaticMeshComponent*> HismComponents = {
        AccentTurfInstances,
        FormalBedVeneerComponent,
        FormalBedShrubCorrections,
        FormalBedFlowerCorrections,
        FormalBedUnderstoreyCorrections,
        TreeBaseMulchInstances,
        TreeBaseShrubInstances,
        TreeBaseUnderstoreyInstances,
        FountainSurfaceComponent,
        FountainEdgeFoamComponent,
        OuterPlumeInstances,
        ImpactRingInstances,
        CentralPlumeComponent,
        InnerPaverInstances,
        OuterPaverInstances,
        PachiraBarkAInstances,
        PachiraBarkBInstances,
        PachiraBarkCInstances,
        PachiraBarkDInstances,
        PachiraLeavesAInstances,
        PachiraLeavesBInstances,
        PachiraLeavesCInstances,
        PachiraLeavesDInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : HismComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
            Component->SetStaticMesh(nullptr);
            Component->EmptyOverrideMaterials();
        }
    }
    if (HardscapeRenderSuccessorComponent)
    {
        HardscapeRenderSuccessorComponent->SetStaticMesh(nullptr);
        HardscapeRenderSuccessorComponent->EmptyOverrideMaterials();
    }
}

bool ATRIADIstanaExploreV5BVisualActor::ApplyLayout(
    const FTRIADIstanaExploreV5BAssetRoster& Assets,
    const FTRIADIstanaExploreV5BDeterministicLayout& Layout,
    FString& OutError)
{
    if (!ValidateOwnedComponentTopology(OutError) ||
        !ValidateAssetRoster(Assets, OutError) ||
        !ValidateTransformArray(
            Layout.AccentTurfWorldTransforms,
            AccentTurfCount,
            TEXT("accent-turf"),
            OutError) ||
        !ValidateTransformArray(
            Layout.FormalBedShrubCorrectionWorldTransforms,
            FormalBedShrubRenderCount,
            TEXT("formal-bed shrub correction"),
            OutError) ||
        !ValidateTransformArray(
            Layout.FormalBedFlowerCorrectionWorldTransforms,
            FormalBedFlowerRenderCount,
            TEXT("formal-bed flower correction"),
            OutError) ||
        !ValidateTransformArray(
            Layout.FormalBedUnderstoreyCorrectionWorldTransforms,
            FormalBedUnderstoreyRenderCount,
            TEXT("formal-bed understorey correction"),
            OutError) ||
        !ValidateTransformArray(
            Layout.TreeBaseMulchWorldTransforms,
            TreeGroundingAnchorCount,
            TEXT("tree-base mulch"),
            OutError) ||
        !ValidateTransformArray(
            Layout.TreeBaseShrubWorldTransforms,
            TreeGroundingShrubCount,
            TEXT("tree-base shrub"),
            OutError) ||
        !ValidateTransformArray(
            Layout.TreeBaseUnderstoreyWorldTransforms,
            TreeGroundingUnderstoreyCount,
            TEXT("tree-base understorey"),
            OutError) ||
        !ValidateTransformArray(
            Layout.OuterPlumeWorldTransforms,
            OuterPlumeCount,
            TEXT("outer-plume"),
            OutError) ||
        !ValidateTransformArray(
            Layout.ImpactRingWorldTransforms,
            ImpactRingCount,
            TEXT("impact-ring"),
            OutError) ||
        !ValidateTransformArray(
            Layout.InnerPaverWorldTransforms,
            InnerPaverCount,
            TEXT("inner-paver"),
            OutError) ||
        !ValidateTransformArray(
            Layout.OuterPaverWorldTransforms,
            OuterPaverCount,
            TEXT("outer-paver"),
            OutError))
    {
        return false;
    }
    if (Layout.AccentTurfSourceIndices.Num() != AccentTurfCount ||
        Layout.TreeBaseSourceComponentIndices.Num() !=
            TreeGroundingAnchorCount ||
        Layout.TreeBaseSourceInstanceIndices.Num() !=
            TreeGroundingAnchorCount ||
        !IsFinitePositiveTransform(Layout.FormalBedVeneerWorldTransform) ||
        !IsFinitePositiveTransform(Layout.FountainSurfaceWorldTransform) ||
        !IsFinitePositiveTransform(Layout.FountainEdgeFoamWorldTransform) ||
        !IsFinitePositiveTransform(Layout.CentralPlumeWorldTransform))
    {
        OutError = TEXT("V5B layout preflight found incomplete source indices or fountain transforms.");
        return false;
    }
    for (int32 Variant = 0; Variant < PachiraVariantCount; ++Variant)
    {
        if (!ValidateTransformArray(
                Layout.PachiraWorldTransformsByVariant[Variant],
                PachiraInstancesPerVariant,
                TEXT("Pachira morphology proxy variant"),
                OutError))
        {
            return false;
        }
    }
    if (Tags.Contains(
            ATRIADIstanaExploreV5DGroundVegetationActor::
                RuntimeSourceTurfPresentationTag()))
    {
        OutError = TEXT("V5B ApplyLayout refuses to mutate while an admitted V5D inherited-turf presentation is active; ReapplyExploreV5BVisuals must suspend it first.");
        return false;
    }

    UHierarchicalInstancedStaticMeshComponent* BarkComponents[] = {
        PachiraBarkAInstances,
        PachiraBarkBInstances,
        PachiraBarkCInstances,
        PachiraBarkDInstances};
    UHierarchicalInstancedStaticMeshComponent* LeavesComponents[] = {
        PachiraLeavesAInstances,
        PachiraLeavesBInstances,
        PachiraLeavesCInstances,
        PachiraLeavesDInstances};
    const TArray<UHierarchicalInstancedStaticMeshComponent*> HismComponents = {
        AccentTurfInstances,
        FormalBedVeneerComponent,
        FormalBedShrubCorrections,
        FormalBedFlowerCorrections,
        FormalBedUnderstoreyCorrections,
        TreeBaseMulchInstances,
        TreeBaseShrubInstances,
        TreeBaseUnderstoreyInstances,
        FountainSurfaceComponent,
        FountainEdgeFoamComponent,
        OuterPlumeInstances,
        ImpactRingInstances,
        CentralPlumeComponent,
        InnerPaverInstances,
        OuterPaverInstances,
        PachiraBarkAInstances,
        PachiraBarkBInstances,
        PachiraBarkCInstances,
        PachiraBarkDInstances,
        PachiraLeavesAInstances,
        PachiraLeavesBInstances,
        PachiraLeavesCInstances,
        PachiraLeavesDInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : HismComponents)
    {
        Component->bAutoRebuildTreeOnInstanceChanges = false;
    }

    AssignMeshAndMaterial(
        AccentTurfInstances,
        Assets.AccentTurfMesh,
        Assets.AccentTurfMaterial);
    HardscapeRenderSuccessorComponent->SetStaticMesh(
        Assets.HardscapeRenderSuccessorMesh);
    HardscapeRenderSuccessorComponent->EmptyOverrideMaterials();
    for (int32 MaterialIndex = 0;
         MaterialIndex < Assets.HardscapeRenderSuccessorMaterials.Num();
         ++MaterialIndex)
    {
        HardscapeRenderSuccessorComponent->SetMaterial(
            MaterialIndex,
            Assets.HardscapeRenderSuccessorMaterials[MaterialIndex].Get());
    }
    AssignMeshAndMaterial(
        FormalBedVeneerComponent,
        Assets.FormalBedVeneerMesh,
        Assets.FormalBedVeneerMaterial);
    AssignMeshAndMaterial(
        FormalBedShrubCorrections,
        Assets.FormalBedShrubMesh,
        Assets.FormalBedShrubMaterial);
    AssignMeshAndMaterial(
        FormalBedFlowerCorrections,
        Assets.FormalBedFlowerMesh,
        Assets.FormalBedFlowerMaterial);
    AssignMeshAndMaterial(
        FormalBedUnderstoreyCorrections,
        Assets.FormalBedUnderstoreyMesh,
        Assets.FormalBedUnderstoreyMaterial);
    AssignMeshAndMaterial(
        TreeBaseMulchInstances,
        Assets.TreeBaseMulchMesh,
        Assets.FormalBedVeneerMaterial);
    AssignMeshAndMaterial(
        TreeBaseShrubInstances,
        Assets.FormalBedShrubMesh,
        Assets.FormalBedShrubMaterial);
    AssignMeshAndMaterial(
        TreeBaseUnderstoreyInstances,
        Assets.FormalBedUnderstoreyMesh,
        Assets.FormalBedUnderstoreyMaterial);
    TreeBaseShrubInstances->ForcedLodModel = TreeBasePlantForcedLodModel;
    TreeBaseUnderstoreyInstances->ForcedLodModel =
        TreeBasePlantForcedLodModel;
    AssignMeshAndMaterial(
        FountainSurfaceComponent,
        Assets.FountainSurfaceMesh,
        Assets.FountainSurfaceMaterial);
    AssignMeshAndMaterial(
        FountainEdgeFoamComponent,
        Assets.FountainEdgeFoamMesh,
        Assets.FountainEdgeFoamMaterial);
    AssignMeshAndMaterial(
        OuterPlumeInstances,
        Assets.OuterPlumeMesh,
        Assets.OuterPlumeMaterial);
    AssignMeshAndMaterial(
        ImpactRingInstances,
        Assets.ImpactRingMesh,
        Assets.ImpactRingMaterial);
    AssignMeshAndMaterial(
        CentralPlumeComponent,
        Assets.CentralPlumeMesh,
        Assets.CentralPlumeMaterial);
    AssignMeshAndMaterial(
        InnerPaverInstances,
        Assets.InnerPaverWedgeMesh,
        Assets.PaverMaterial);
    AssignMeshAndMaterial(
        OuterPaverInstances,
        Assets.OuterPaverWedgeMesh,
        Assets.PaverMaterial);
    for (int32 Variant = 0; Variant < PachiraVariantCount; ++Variant)
    {
        AssignMeshAndMaterial(
            BarkComponents[Variant],
            Assets.PachiraBarkMeshes[Variant],
            Assets.PachiraBarkMaterials[Variant]);
        AssignMeshAndMaterial(
            LeavesComponents[Variant],
            Assets.PachiraLeavesMeshes[Variant],
            Assets.PachiraLeavesMaterials[Variant]);
    }

    for (UHierarchicalInstancedStaticMeshComponent* Component : HismComponents)
    {
        Component->ClearInstances();
    }
    AccentTurfInstances->AddInstances(
        Layout.AccentTurfWorldTransforms,
        false,
        true,
        false);
    FormalBedVeneerComponent->AddInstance(
        Layout.FormalBedVeneerWorldTransform,
        true);
    FormalBedShrubCorrections->AddInstances(
        Layout.FormalBedShrubCorrectionWorldTransforms,
        false,
        true,
        false);
    FormalBedFlowerCorrections->AddInstances(
        Layout.FormalBedFlowerCorrectionWorldTransforms,
        false,
        true,
        false);
    FormalBedUnderstoreyCorrections->AddInstances(
        Layout.FormalBedUnderstoreyCorrectionWorldTransforms,
        false,
        true,
        false);
    TreeBaseMulchInstances->AddInstances(
        Layout.TreeBaseMulchWorldTransforms,
        false,
        true,
        false);
    TreeBaseShrubInstances->AddInstances(
        Layout.TreeBaseShrubWorldTransforms,
        false,
        true,
        false);
    TreeBaseUnderstoreyInstances->AddInstances(
        Layout.TreeBaseUnderstoreyWorldTransforms,
        false,
        true,
        false);
    FountainSurfaceComponent->AddInstance(
        Layout.FountainSurfaceWorldTransform,
        true);
    FountainEdgeFoamComponent->AddInstance(
        Layout.FountainEdgeFoamWorldTransform,
        true);
    OuterPlumeInstances->AddInstances(
        Layout.OuterPlumeWorldTransforms,
        false,
        true,
        false);
    ImpactRingInstances->AddInstances(
        Layout.ImpactRingWorldTransforms,
        false,
        true,
        false);
    CentralPlumeComponent->AddInstance(
        Layout.CentralPlumeWorldTransform,
        true);
    InnerPaverInstances->AddInstances(
        Layout.InnerPaverWorldTransforms,
        false,
        true,
        false);
    OuterPaverInstances->AddInstances(
        Layout.OuterPaverWorldTransforms,
        false,
        true,
        false);
    for (int32 Variant = 0; Variant < PachiraVariantCount; ++Variant)
    {
        BarkComponents[Variant]->AddInstances(
            Layout.PachiraWorldTransformsByVariant[Variant],
            false,
            true,
            false);
        LeavesComponents[Variant]->AddInstances(
            Layout.PachiraWorldTransformsByVariant[Variant],
            false,
            true,
            false);
    }

    bool bTreesBuiltSynchronously = true;
    for (UHierarchicalInstancedStaticMeshComponent* Component : HismComponents)
    {
        bTreesBuiltSynchronously =
            Component->BuildTreeIfOutdated(false, true) &&
            !Component->IsAsyncBuilding() &&
            Component->IsTreeFullyBuilt() &&
            bTreesBuiltSynchronously;
        Component->bAutoRebuildTreeOnInstanceChanges = true;
    }
    if (!bTreesBuiltSynchronously)
    {
        OutError = TEXT("A V5B HISM failed its forced synchronous tree build.");
        return false;
    }
    return ValidateAppliedLayout(Assets, Layout, OutError);
}

bool ATRIADIstanaExploreV5BVisualActor::ValidateAppliedLayout(
    const FTRIADIstanaExploreV5BAssetRoster& Assets,
    const FTRIADIstanaExploreV5BDeterministicLayout& Layout,
    FString& OutError) const
{
    if (!ValidateOwnedComponentTopology(OutError) ||
        !ValidateAssetRoster(Assets, OutError))
    {
        return false;
    }
    const bool bV5DSourceTurfPresentationTag = Tags.Contains(
        ATRIADIstanaExploreV5DGroundVegetationActor::
            RuntimeSourceTurfPresentationTag());
    FString V5DSourceTurfPresentationReport;
    const bool bV5DSourceTurfPresentation =
        bV5DSourceTurfPresentationTag &&
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ValidateActiveRuntimeSourceTurfPresentationForSourceActor(
                this,
                V5DSourceTurfPresentationReport);
    if (bV5DSourceTurfPresentationTag && !bV5DSourceTurfPresentation)
    {
        OutError = TEXT("A tagged V5D inherited-turf material presentation failed its exact owner/source/census/simulation-isolation gate: ") +
            V5DSourceTurfPresentationReport;
        return false;
    }
    const bool bV5DFountainHardscapeWaterPresentationTag = Tags.Contains(
        ATRIADIstanaExploreV5DFountainRealismActor::
            SourceRendererReplacementTag());
    FString V5DFountainHardscapeWaterPresentationReport;
    const bool bV5DFountainHardscapeWaterPresentation =
        bV5DFountainHardscapeWaterPresentationTag &&
        ATRIADIstanaExploreV5DFountainRealismActor::
            ValidateActiveHardscapeWaterPresentationForSourceActor(
                this,
                V5DFountainHardscapeWaterPresentationReport);
    if (bV5DFountainHardscapeWaterPresentationTag &&
        !bV5DFountainHardscapeWaterPresentation)
    {
        OutError = TEXT("A tagged V5D fountain hardscape-water presentation failed its exact sole-owner/source/three-slot/simulation-isolation gate: ") +
            V5DFountainHardscapeWaterPresentationReport;
        return false;
    }
    if ((!bV5DSourceTurfPresentation && !ValidateHismMeshAndMaterial(
            AccentTurfInstances,
            Assets.AccentTurfMesh,
            Assets.AccentTurfMaterial,
            TEXT("accent turf"),
            OutError)) ||
        (!bV5DFountainHardscapeWaterPresentation &&
         !ValidateMeshAndMaterials(
            HardscapeRenderSuccessorComponent,
            Assets.HardscapeRenderSuccessorMesh,
            Assets.HardscapeRenderSuccessorMaterials,
            TEXT("hardscape render successor"),
            OutError)) ||
        !ValidateHismMeshAndMaterial(
            FormalBedVeneerComponent,
            Assets.FormalBedVeneerMesh,
            Assets.FormalBedVeneerMaterial,
            TEXT("formal-bed organic veneer"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            FormalBedShrubCorrections,
            Assets.FormalBedShrubMesh,
            Assets.FormalBedShrubMaterial,
            TEXT("formal-bed shrub corrections"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            FormalBedFlowerCorrections,
            Assets.FormalBedFlowerMesh,
            Assets.FormalBedFlowerMaterial,
            TEXT("formal-bed flower corrections"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            FormalBedUnderstoreyCorrections,
            Assets.FormalBedUnderstoreyMesh,
            Assets.FormalBedUnderstoreyMaterial,
            TEXT("formal-bed understorey corrections"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            TreeBaseMulchInstances,
            Assets.TreeBaseMulchMesh,
            Assets.FormalBedVeneerMaterial,
            TEXT("tree-base mulch"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            TreeBaseShrubInstances,
            Assets.FormalBedShrubMesh,
            Assets.FormalBedShrubMaterial,
            TEXT("tree-base shrubs"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            TreeBaseUnderstoreyInstances,
            Assets.FormalBedUnderstoreyMesh,
            Assets.FormalBedUnderstoreyMaterial,
            TEXT("tree-base understorey"),
            OutError) ||
        !ValidateMeshAndMaterial(
            FountainSurfaceComponent,
            Assets.FountainSurfaceMesh,
            Assets.FountainSurfaceMaterial,
            TEXT("fountain surface"),
            OutError) ||
        !ValidateMeshAndMaterial(
            FountainEdgeFoamComponent,
            Assets.FountainEdgeFoamMesh,
            Assets.FountainEdgeFoamMaterial,
            TEXT("fountain edge foam"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            OuterPlumeInstances,
            Assets.OuterPlumeMesh,
            Assets.OuterPlumeMaterial,
            TEXT("outer plumes"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            ImpactRingInstances,
            Assets.ImpactRingMesh,
            Assets.ImpactRingMaterial,
            TEXT("impact rings"),
            OutError) ||
        !ValidateMeshAndMaterial(
            CentralPlumeComponent,
            Assets.CentralPlumeMesh,
            Assets.CentralPlumeMaterial,
            TEXT("central plume"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            InnerPaverInstances,
            Assets.InnerPaverWedgeMesh,
            Assets.PaverMaterial,
            TEXT("inner pavers"),
            OutError) ||
        !ValidateHismMeshAndMaterial(
            OuterPaverInstances,
            Assets.OuterPaverWedgeMesh,
            Assets.PaverMaterial,
            TEXT("outer pavers"),
            OutError))
    {
        return false;
    }
    if (TreeBaseShrubInstances->ForcedLodModel !=
            TreeBasePlantForcedLodModel ||
        TreeBaseUnderstoreyInstances->ForcedLodModel !=
            TreeBasePlantForcedLodModel ||
        TreeBaseShrubInstances->MinLOD != 0 ||
        TreeBaseUnderstoreyInstances->MinLOD != 0)
    {
        OutError = TEXT("The tree-base plant HISMs lost their exact source-LOD1-only presentation contract.");
        return false;
    }

    const TArray<FTransform> FountainSurfaceTransforms = {
        Layout.FountainSurfaceWorldTransform};
    const TArray<FTransform> FormalBedVeneerTransforms = {
        Layout.FormalBedVeneerWorldTransform};
    const TArray<FTransform> FountainEdgeFoamTransforms = {
        Layout.FountainEdgeFoamWorldTransform};
    const TArray<FTransform> CentralPlumeTransforms = {
        Layout.CentralPlumeWorldTransform};
    if (!ValidateComponentInstances(
            AccentTurfInstances,
            Layout.AccentTurfWorldTransforms,
            TEXT("accent turf"),
            OutError) ||
        !ValidateComponentInstances(
            FormalBedVeneerComponent,
            FormalBedVeneerTransforms,
            TEXT("formal-bed organic veneer"),
            OutError) ||
        !ValidateComponentInstances(
            FormalBedShrubCorrections,
            Layout.FormalBedShrubCorrectionWorldTransforms,
            TEXT("formal-bed shrub corrections"),
            OutError) ||
        !ValidateComponentInstances(
            FormalBedFlowerCorrections,
            Layout.FormalBedFlowerCorrectionWorldTransforms,
            TEXT("formal-bed flower corrections"),
            OutError) ||
        !ValidateComponentInstances(
            FormalBedUnderstoreyCorrections,
            Layout.FormalBedUnderstoreyCorrectionWorldTransforms,
            TEXT("formal-bed understorey corrections"),
            OutError) ||
        !ValidateComponentInstances(
            TreeBaseMulchInstances,
            Layout.TreeBaseMulchWorldTransforms,
            TEXT("tree-base mulch"),
            OutError) ||
        !ValidateComponentInstances(
            TreeBaseShrubInstances,
            Layout.TreeBaseShrubWorldTransforms,
            TEXT("tree-base shrubs"),
            OutError) ||
        !ValidateComponentInstances(
            TreeBaseUnderstoreyInstances,
            Layout.TreeBaseUnderstoreyWorldTransforms,
            TEXT("tree-base understorey"),
            OutError) ||
        !ValidateComponentInstances(
            FountainSurfaceComponent,
            FountainSurfaceTransforms,
            TEXT("fountain surface"),
            OutError) ||
        !ValidateComponentInstances(
            FountainEdgeFoamComponent,
            FountainEdgeFoamTransforms,
            TEXT("fountain edge foam"),
            OutError) ||
        !ValidateComponentInstances(
            OuterPlumeInstances,
            Layout.OuterPlumeWorldTransforms,
            TEXT("outer plumes"),
            OutError) ||
        !ValidateComponentInstances(
            ImpactRingInstances,
            Layout.ImpactRingWorldTransforms,
            TEXT("impact rings"),
            OutError) ||
        !ValidateComponentInstances(
            CentralPlumeComponent,
            CentralPlumeTransforms,
            TEXT("central plume"),
            OutError) ||
        !ValidateComponentInstances(
            InnerPaverInstances,
            Layout.InnerPaverWorldTransforms,
            TEXT("inner pavers"),
            OutError) ||
        !ValidateComponentInstances(
            OuterPaverInstances,
            Layout.OuterPaverWorldTransforms,
            TEXT("outer pavers"),
            OutError))
    {
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* BarkComponents[] = {
        PachiraBarkAInstances,
        PachiraBarkBInstances,
        PachiraBarkCInstances,
        PachiraBarkDInstances};
    const UHierarchicalInstancedStaticMeshComponent* LeavesComponents[] = {
        PachiraLeavesAInstances,
        PachiraLeavesBInstances,
        PachiraLeavesCInstances,
        PachiraLeavesDInstances};
    for (int32 Variant = 0; Variant < PachiraVariantCount; ++Variant)
    {
        if (!ValidateHismMeshAndMaterial(
                BarkComponents[Variant],
                Assets.PachiraBarkMeshes[Variant],
                Assets.PachiraBarkMaterials[Variant],
                TEXT("Pachira bark morphology proxy"),
                OutError) ||
            !ValidateHismMeshAndMaterial(
                LeavesComponents[Variant],
                Assets.PachiraLeavesMeshes[Variant],
                Assets.PachiraLeavesMaterials[Variant],
                TEXT("Pachira leaves morphology proxy"),
                OutError) ||
            !ValidateComponentInstances(
                BarkComponents[Variant],
                Layout.PachiraWorldTransformsByVariant[Variant],
                TEXT("Pachira bark morphology proxy"),
                OutError) ||
            !ValidateComponentInstances(
                LeavesComponents[Variant],
                Layout.PachiraWorldTransformsByVariant[Variant],
                TEXT("Pachira leaves morphology proxy"),
                OutError))
        {
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5BVisualActor::SaveLayout(
    const FTRIADIstanaExploreV5BDeterministicLayout& Layout)
{
    PreservedAccentSourceIndices = Layout.AccentTurfSourceIndices;
    SavedAccentTurfWorldTransforms = Layout.AccentTurfWorldTransforms;
    SavedFormalBedVeneerWorldTransform =
        Layout.FormalBedVeneerWorldTransform;
    SavedFormalBedShrubCorrectionWorldTransforms =
        Layout.FormalBedShrubCorrectionWorldTransforms;
    SavedFormalBedFlowerCorrectionWorldTransforms =
        Layout.FormalBedFlowerCorrectionWorldTransforms;
    SavedFormalBedUnderstoreyCorrectionWorldTransforms =
        Layout.FormalBedUnderstoreyCorrectionWorldTransforms;
    SavedTreeBaseSourceComponentIndices =
        Layout.TreeBaseSourceComponentIndices;
    SavedTreeBaseSourceInstanceIndices =
        Layout.TreeBaseSourceInstanceIndices;
    SavedTreeBaseMulchWorldTransforms =
        Layout.TreeBaseMulchWorldTransforms;
    SavedTreeBaseShrubWorldTransforms =
        Layout.TreeBaseShrubWorldTransforms;
    SavedTreeBaseUnderstoreyWorldTransforms =
        Layout.TreeBaseUnderstoreyWorldTransforms;
    SavedFountainSurfaceWorldTransform =
        Layout.FountainSurfaceWorldTransform;
    SavedFountainEdgeFoamWorldTransform =
        Layout.FountainEdgeFoamWorldTransform;
    SavedOuterPlumeWorldTransforms = Layout.OuterPlumeWorldTransforms;
    SavedImpactRingWorldTransforms = Layout.ImpactRingWorldTransforms;
    SavedCentralPlumeWorldTransform = Layout.CentralPlumeWorldTransform;
    SavedInnerPaverWorldTransforms = Layout.InnerPaverWorldTransforms;
    SavedOuterPaverWorldTransforms = Layout.OuterPaverWorldTransforms;
    SavedPachiraVariantAWorldTransforms =
        Layout.PachiraWorldTransformsByVariant[0];
    SavedPachiraVariantBWorldTransforms =
        Layout.PachiraWorldTransformsByVariant[1];
    SavedPachiraVariantCWorldTransforms =
        Layout.PachiraWorldTransformsByVariant[2];
    SavedPachiraVariantDWorldTransforms =
        Layout.PachiraWorldTransformsByVariant[3];
}

FTRIADIstanaExploreV5BDeterministicLayout
ATRIADIstanaExploreV5BVisualActor::LoadSavedLayout() const
{
    FTRIADIstanaExploreV5BDeterministicLayout Layout;
    Layout.AccentTurfSourceIndices = PreservedAccentSourceIndices;
    Layout.AccentTurfWorldTransforms = SavedAccentTurfWorldTransforms;
    Layout.FormalBedVeneerWorldTransform =
        SavedFormalBedVeneerWorldTransform;
    Layout.FormalBedShrubCorrectionWorldTransforms =
        SavedFormalBedShrubCorrectionWorldTransforms;
    Layout.FormalBedFlowerCorrectionWorldTransforms =
        SavedFormalBedFlowerCorrectionWorldTransforms;
    Layout.FormalBedUnderstoreyCorrectionWorldTransforms =
        SavedFormalBedUnderstoreyCorrectionWorldTransforms;
    Layout.TreeBaseSourceComponentIndices =
        SavedTreeBaseSourceComponentIndices;
    Layout.TreeBaseSourceInstanceIndices =
        SavedTreeBaseSourceInstanceIndices;
    Layout.TreeBaseMulchWorldTransforms =
        SavedTreeBaseMulchWorldTransforms;
    Layout.TreeBaseShrubWorldTransforms =
        SavedTreeBaseShrubWorldTransforms;
    Layout.TreeBaseUnderstoreyWorldTransforms =
        SavedTreeBaseUnderstoreyWorldTransforms;
    Layout.FountainSurfaceWorldTransform =
        SavedFountainSurfaceWorldTransform;
    Layout.FountainEdgeFoamWorldTransform =
        SavedFountainEdgeFoamWorldTransform;
    Layout.OuterPlumeWorldTransforms = SavedOuterPlumeWorldTransforms;
    Layout.ImpactRingWorldTransforms = SavedImpactRingWorldTransforms;
    Layout.CentralPlumeWorldTransform = SavedCentralPlumeWorldTransform;
    Layout.InnerPaverWorldTransforms = SavedInnerPaverWorldTransforms;
    Layout.OuterPaverWorldTransforms = SavedOuterPaverWorldTransforms;
    Layout.PachiraWorldTransformsByVariant[0] =
        SavedPachiraVariantAWorldTransforms;
    Layout.PachiraWorldTransformsByVariant[1] =
        SavedPachiraVariantBWorldTransforms;
    Layout.PachiraWorldTransformsByVariant[2] =
        SavedPachiraVariantCWorldTransforms;
    Layout.PachiraWorldTransformsByVariant[3] =
        SavedPachiraVariantDWorldTransforms;
    return Layout;
}

bool ATRIADIstanaExploreV5BVisualActor::ConfigureExploreV5BVisuals(
    ATRIADIstanaPublicViewSceneActor* InPublicViewSceneActor,
    ATRIADIstanaExploreV4LandscapeActor* InExploreV4LandscapeActor,
    const FTRIADIstanaExploreV5BAssetRoster& InAssets,
    FString& OutError)
{
    if (!ValidateOwnedComponentTopology(OutError) ||
        !ValidateAssetRoster(InAssets, OutError) ||
        !ValidateHardscapeRenderSuccessorSource(
            InPublicViewSceneActor,
            InAssets,
            OutError))
    {
        return false;
    }

    TArray<FTransform> CloseTurfWorldTransforms;
    if (!ExtractCloseTurfWorldTransforms(
            InExploreV4LandscapeActor,
            CloseTurfWorldTransforms,
            OutError))
    {
        return false;
    }
    TArray<FTransform> FormalShrubPrefixWorldTransforms;
    TArray<FTransform> FormalFlowerPrefixWorldTransforms;
    TArray<FTransform> FormalUnderstoreyPrefixWorldTransforms;
    if (!ExtractFormalBedSourceWorldTransforms(
            InExploreV4LandscapeActor,
            FormalShrubPrefixWorldTransforms,
            FormalFlowerPrefixWorldTransforms,
            FormalUnderstoreyPrefixWorldTransforms,
            OutError) ||
        InExploreV4LandscapeActor->ShrubInstances->GetStaticMesh() !=
            InAssets.FormalBedShrubMesh ||
        !HasExactBaseOrRuntimeMidParent(
            InExploreV4LandscapeActor->ShrubInstances->GetMaterial(0),
            InAssets.FormalBedShrubMaterial) ||
        InExploreV4LandscapeActor->FlowerInstances->GetStaticMesh() !=
            InAssets.FormalBedFlowerMesh ||
        !HasExactBaseOrRuntimeMidParent(
            InExploreV4LandscapeActor->FlowerInstances->GetMaterial(0),
            InAssets.FormalBedFlowerMaterial) ||
        InExploreV4LandscapeActor->UnderstoreyInstances->GetStaticMesh() !=
            InAssets.FormalBedUnderstoreyMesh ||
        !HasExactBaseOrRuntimeMidParent(
            InExploreV4LandscapeActor->UnderstoreyInstances->GetMaterial(0),
            InAssets.FormalBedUnderstoreyMaterial))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5B formal-bed corrections must borrow the exact effective V4 mesh/material pointers.");
        }
        return false;
    }
    TArray<FTRIADIstanaExploreV5BTreeSource> TreeSources;
    if (!ExtractTreeSources(
            InExploreV4LandscapeActor,
            TreeSources,
            OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5BDeterministicLayout NewLayout;
    if (!BuildDeterministicLayout(
            CloseTurfWorldTransforms,
            NewLayout,
            OutError) ||
        !BuildFormalBedCorrections(
            FormalShrubPrefixWorldTransforms,
            FormalFlowerPrefixWorldTransforms,
            FormalUnderstoreyPrefixWorldTransforms,
            NewLayout,
            OutError) ||
        !BuildTreeGrounding(
            TreeSources,
            NewLayout,
            OutError))
    {
        return false;
    }

    TArray<FTransform> NewPreservedAccentSources;
    NewPreservedAccentSources.Reserve(AccentTurfCount);
    for (int32 SourceIndex : NewLayout.AccentTurfSourceIndices)
    {
        if (!CloseTurfWorldTransforms.IsValidIndex(SourceIndex))
        {
            OutError = TEXT("V5B selected-source snapshot preflight failed before mutation.");
            return false;
        }
        NewPreservedAccentSources.Add(
            CloseTurfWorldTransforms[SourceIndex]);
    }

    const bool bHadPreviousConfiguration = bConfigured;
    ATRIADIstanaPublicViewSceneActor* PreviousSceneActor =
        PublicViewSceneActor;
    ATRIADIstanaExploreV4LandscapeActor* PreviousSourceActor =
        ExploreV4LandscapeActor;
    const FTRIADIstanaExploreV5BAssetRoster PreviousAssets =
        SavedAssetRoster;
    const FTRIADIstanaExploreV5BDeterministicLayout PreviousLayout =
        LoadSavedLayout();
    const TArray<FTransform> PreviousPreservedAccentSources =
        PreservedAccentSourceWorldTransforms;
    FV5BOwnedVisualStateSnapshot PreviousOwnedVisualState;
    FString SnapshotError;
    if (!CaptureOwnedV5BVisualState(
            this,
            PreviousOwnedVisualState,
            SnapshotError))
    {
        OutError = SnapshotError;
        return false;
    }

    TArray<FV5BSceneRenderStateSnapshot> SceneRenderSnapshots;
    TArray<FV5BSourceRenderStateSnapshot> SourceRenderSnapshots;
    FV5BSceneRenderStateSnapshot IncomingSceneRenderState;
    FV5BSourceRenderStateSnapshot IncomingSourceRenderState;
    if (!CaptureSceneRenderState(
            InPublicViewSceneActor,
            IncomingSceneRenderState) ||
        !CaptureSourceRenderState(
            InExploreV4LandscapeActor,
            IncomingSourceRenderState))
    {
        OutError = TEXT("V5B could not snapshot the incoming public hardscape, exact 13-component inherited render-source visibility roster, and full actor tags before mutation.");
        return false;
    }
    SceneRenderSnapshots.Add(MoveTemp(IncomingSceneRenderState));
    SourceRenderSnapshots.Add(MoveTemp(IncomingSourceRenderState));
    if (PreviousSceneActor &&
        PreviousSceneActor != InPublicViewSceneActor)
    {
        FV5BSceneRenderStateSnapshot PreviousSceneRenderState;
        if (!CaptureSceneRenderState(
                PreviousSceneActor,
                PreviousSceneRenderState))
        {
            OutError = TEXT("V5B could not snapshot the different previous public hardscape and full actor tags before mutation.");
            return false;
        }
        SceneRenderSnapshots.Add(MoveTemp(PreviousSceneRenderState));
    }
    if (PreviousSourceActor &&
        PreviousSourceActor != InExploreV4LandscapeActor)
    {
        FV5BSourceRenderStateSnapshot PreviousSourceRenderState;
        if (!CaptureSourceRenderState(
                PreviousSourceActor,
                PreviousSourceRenderState))
        {
            OutError = TEXT("V5B could not snapshot the different previous exact 13-component inherited render-source visibility roster and full actor tags before mutation.");
            return false;
        }
        SourceRenderSnapshots.Add(MoveTemp(PreviousSourceRenderState));
    }

    const auto RestorePreviousConfiguration = [&]() -> FString
    {
        FString RollbackError;
        FString OwnedRestoreError;
        const bool bOwnedStateRestored = RestoreOwnedV5BVisualState(
            this,
            PreviousOwnedVisualState,
            OwnedRestoreError);

        PublicViewSceneActor = PreviousSceneActor;
        ExploreV4LandscapeActor = PreviousSourceActor;
        SavedAssetRoster = PreviousAssets;
        SaveLayout(PreviousLayout);
        PreservedAccentSourceWorldTransforms =
            PreviousPreservedAccentSources;
        bConfigured = bHadPreviousConfiguration;

        bool bExternalStateRestored = true;
        for (const FV5BSceneRenderStateSnapshot& Snapshot :
             SceneRenderSnapshots)
        {
            bExternalStateRestored = RestoreSceneRenderState(Snapshot) &&
                bExternalStateRestored;
        }
        for (const FV5BSourceRenderStateSnapshot& Snapshot :
             SourceRenderSnapshots)
        {
            bExternalStateRestored = RestoreSourceRenderState(Snapshot) &&
                bExternalStateRestored;
        }

        if (!bOwnedStateRestored)
        {
            RollbackError += OwnedRestoreError;
        }
        if (!bExternalStateRestored)
        {
            if (!RollbackError.IsEmpty())
            {
                RollbackError += TEXT(" ");
            }
            RollbackError += TEXT("V5B failure-atomic rollback could not restore all captured actor tags and inherited render visibility exactly.");
        }
        return RollbackError;
    };
    const auto FailAfterMutation = [&](const FString& Failure) -> bool
    {
        const FString RollbackError = RestorePreviousConfiguration();
        OutError = Failure;
        if (!RollbackError.IsEmpty())
        {
            OutError += TEXT(" Rollback also failed: ") + RollbackError;
        }
        return false;
    };

    if (!ApplyLayout(InAssets, NewLayout, OutError))
    {
        const FString ApplyError = OutError;
        return FailAfterMutation(ApplyError);
    }

    bool bRenderOwnershipTransitioned = true;
    if (bHadPreviousConfiguration && PreviousSourceActor &&
        PreviousSourceActor != InExploreV4LandscapeActor &&
        PreviousSourceActor->CloseTurfInstances)
    {
        PreviousSourceActor->Tags.Remove(V5BRenderSuccessorTag);
        bRenderOwnershipTransitioned =
            SetInheritedRenderSuccessorState(PreviousSourceActor, false) &&
            bRenderOwnershipTransitioned;
    }
    if (bHadPreviousConfiguration && PreviousSceneActor &&
        PreviousSceneActor != InPublicViewSceneActor &&
        PreviousSceneActor->HardscapeComponent)
    {
        PreviousSceneActor->Tags.Remove(V5BRenderSuccessorTag);
        bRenderOwnershipTransitioned =
            SetHardscapeRenderSuccessorState(PreviousSceneActor, false) &&
            bRenderOwnershipTransitioned;
    }
    InPublicViewSceneActor->Tags.AddUnique(V5BRenderSuccessorTag);
    bRenderOwnershipTransitioned = SetHardscapeRenderSuccessorState(
            InPublicViewSceneActor,
            true) &&
        bRenderOwnershipTransitioned;
    InExploreV4LandscapeActor->Tags.AddUnique(V5BRenderSuccessorTag);
    bRenderOwnershipTransitioned = SetInheritedRenderSuccessorState(
            InExploreV4LandscapeActor,
            true) &&
        bRenderOwnershipTransitioned;
    if (!bRenderOwnershipTransitioned)
    {
        return FailAfterMutation(
            TEXT("V5B could not atomically transfer hardscape and inherited render ownership."));
    }

    PublicViewSceneActor = InPublicViewSceneActor;
    ExploreV4LandscapeActor = InExploreV4LandscapeActor;
    SavedAssetRoster = InAssets;
    SaveLayout(NewLayout);
    PreservedAccentSourceWorldTransforms =
        MoveTemp(NewPreservedAccentSources);
    bConfigured = true;

    FString ValidationReport;
    if (!ValidateExploreV5BVisuals(ValidationReport))
    {
        return FailAfterMutation(ValidationReport);
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ReapplyExploreV5BVisuals(
    FString& OutError)
{
    if (!bConfigured || !PublicViewSceneActor || !ExploreV4LandscapeActor)
    {
        OutError = TEXT("V5B visuals cannot be reapplied before a complete saved configuration exists.");
        return false;
    }
    if (!ValidateAssetRoster(SavedAssetRoster, OutError) ||
        !ValidateOwnedComponentTopology(OutError) ||
        !ValidateHardscapeRenderSuccessorSource(
            PublicViewSceneActor,
            SavedAssetRoster,
            OutError))
    {
        return false;
    }

    TArray<FTransform> CurrentCloseTurfWorldTransforms;
    if (!ExtractCloseTurfWorldTransforms(
            ExploreV4LandscapeActor,
            CurrentCloseTurfWorldTransforms,
            OutError))
    {
        return false;
    }
    if (PreservedAccentSourceIndices.Num() != AccentTurfCount ||
        PreservedAccentSourceWorldTransforms.Num() != AccentTurfCount)
    {
        OutError = TEXT("V5B persisted accent source snapshot is incomplete.");
        return false;
    }
    for (int32 Ordinal = 0; Ordinal < AccentTurfCount; ++Ordinal)
    {
        const int32 ExpectedSourceIndex = AccentSourceIndexForOrdinal(Ordinal);
        if (PreservedAccentSourceIndices[Ordinal] != ExpectedSourceIndex ||
            !CurrentCloseTurfWorldTransforms[ExpectedSourceIndex].Equals(
                PreservedAccentSourceWorldTransforms[Ordinal],
                0.001))
        {
            OutError = FString::Printf(
                TEXT("V5B source snapshot drifted at accent ordinal %d; reapply rejected before mutation."),
                Ordinal);
            return false;
        }
    }

    TArray<FTransform> FormalShrubPrefixWorldTransforms;
    TArray<FTransform> FormalFlowerPrefixWorldTransforms;
    TArray<FTransform> FormalUnderstoreyPrefixWorldTransforms;
    if (!ExtractFormalBedSourceWorldTransforms(
            ExploreV4LandscapeActor,
            FormalShrubPrefixWorldTransforms,
            FormalFlowerPrefixWorldTransforms,
            FormalUnderstoreyPrefixWorldTransforms,
            OutError) ||
        ExploreV4LandscapeActor->ShrubInstances->GetStaticMesh() !=
            SavedAssetRoster.FormalBedShrubMesh ||
        !HasExactBaseOrRuntimeMidParent(
            ExploreV4LandscapeActor->ShrubInstances->GetMaterial(0),
            SavedAssetRoster.FormalBedShrubMaterial) ||
        ExploreV4LandscapeActor->FlowerInstances->GetStaticMesh() !=
            SavedAssetRoster.FormalBedFlowerMesh ||
        !HasExactBaseOrRuntimeMidParent(
            ExploreV4LandscapeActor->FlowerInstances->GetMaterial(0),
            SavedAssetRoster.FormalBedFlowerMaterial) ||
        ExploreV4LandscapeActor->UnderstoreyInstances->GetStaticMesh() !=
            SavedAssetRoster.FormalBedUnderstoreyMesh ||
        !HasExactBaseOrRuntimeMidParent(
            ExploreV4LandscapeActor->UnderstoreyInstances->GetMaterial(0),
            SavedAssetRoster.FormalBedUnderstoreyMaterial))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5B formal-bed borrowed source bindings drifted before reapply.");
        }
        return false;
    }
    TArray<FTRIADIstanaExploreV5BTreeSource> CurrentTreeSources;
    if (!ExtractTreeSources(
            ExploreV4LandscapeActor,
            CurrentTreeSources,
            OutError))
    {
        return false;
    }
    const FTRIADIstanaExploreV5BDeterministicLayout SavedLayout =
        LoadSavedLayout();
    FTRIADIstanaExploreV5BDeterministicLayout RebuiltLayout;
    if (!BuildDeterministicLayout(
            CurrentCloseTurfWorldTransforms,
            RebuiltLayout,
            OutError) ||
        !BuildFormalBedCorrections(
            FormalShrubPrefixWorldTransforms,
            FormalFlowerPrefixWorldTransforms,
            FormalUnderstoreyPrefixWorldTransforms,
            RebuiltLayout,
            OutError) ||
        !BuildTreeGrounding(
            CurrentTreeSources,
            RebuiltLayout,
            OutError) ||
        !TransformArraysEqual(
            SavedLayout.FormalBedShrubCorrectionWorldTransforms,
            RebuiltLayout.FormalBedShrubCorrectionWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.FormalBedFlowerCorrectionWorldTransforms,
            RebuiltLayout.FormalBedFlowerCorrectionWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.FormalBedUnderstoreyCorrectionWorldTransforms,
            RebuiltLayout.FormalBedUnderstoreyCorrectionWorldTransforms) ||
        SavedLayout.TreeBaseSourceComponentIndices !=
            RebuiltLayout.TreeBaseSourceComponentIndices ||
        SavedLayout.TreeBaseSourceInstanceIndices !=
            RebuiltLayout.TreeBaseSourceInstanceIndices ||
        !TransformArraysEqual(
            SavedLayout.TreeBaseMulchWorldTransforms,
            RebuiltLayout.TreeBaseMulchWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.TreeBaseShrubWorldTransforms,
            RebuiltLayout.TreeBaseShrubWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.TreeBaseUnderstoreyWorldTransforms,
            RebuiltLayout.TreeBaseUnderstoreyWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.AccentTurfWorldTransforms,
            RebuiltLayout.AccentTurfWorldTransforms))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("V5B full borrowed formal-bed source rosters drifted before reapply.");
        }
        return false;
    }
    FHardscapeRenderVisibilitySnapshot HardscapeVisibilitySnapshot;
    FInheritedRenderVisibilitySnapshot SourceVisibilitySnapshot;
    if (!CaptureHardscapeRenderVisibility(
            PublicViewSceneActor,
            HardscapeVisibilitySnapshot) ||
        !CaptureInheritedRenderVisibility(
            ExploreV4LandscapeActor,
            SourceVisibilitySnapshot))
    {
        OutError = TEXT("V5B could not snapshot source rendering states before reapply.");
        return false;
    }
    const bool bSceneHadSuccessorTag =
        PublicViewSceneActor->Tags.Contains(V5BRenderSuccessorTag);
    const bool bSourceHadSuccessorTag =
        ExploreV4LandscapeActor->Tags.Contains(V5BRenderSuccessorTag);
    ATRIADIstanaExploreV5DGroundVegetationActor* SuspendedV5DOwner = nullptr;
    FString V5DSuspensionReport;
    if (!ATRIADIstanaExploreV5DGroundVegetationActor::
            SuspendActiveRuntimeSourceTurfPresentationForV5BReapply(
                this,
                SuspendedV5DOwner,
                V5DSuspensionReport))
    {
        OutError = TEXT("V5B reapply refused to enter mutation because the active V5D inherited-turf presentation could not be suspended exactly: ") +
            V5DSuspensionReport;
        return false;
    }
    if (!ApplyLayout(SavedAssetRoster, SavedLayout, OutError))
    {
        if (SuspendedV5DOwner)
        {
            OutError += TEXT(" The V5D turf presentation remains suspended in exact cold tag-free state.");
        }
        return false;
    }
    PublicViewSceneActor->Tags.AddUnique(V5BRenderSuccessorTag);
    SetHardscapeRenderSuccessorState(PublicViewSceneActor, true);
    ExploreV4LandscapeActor->Tags.AddUnique(V5BRenderSuccessorTag);
    SetInheritedRenderSuccessorState(ExploreV4LandscapeActor, true);
    FString ValidationReport;
    if (!ValidateExploreV5BVisuals(ValidationReport))
    {
        if (!bSceneHadSuccessorTag)
        {
            PublicViewSceneActor->Tags.Remove(V5BRenderSuccessorTag);
        }
        if (!bSourceHadSuccessorTag)
        {
            ExploreV4LandscapeActor->Tags.Remove(V5BRenderSuccessorTag);
        }
        RestoreHardscapeRenderVisibility(
            PublicViewSceneActor,
            HardscapeVisibilitySnapshot);
        RestoreInheritedRenderVisibility(
            ExploreV4LandscapeActor,
            SourceVisibilitySnapshot);
        OutError = ValidationReport;
        return false;
    }
    FString V5DResumeReport;
    if (!ATRIADIstanaExploreV5DGroundVegetationActor::
            ResumeSuspendedRuntimeSourceTurfPresentationAfterV5BReapply(
                SuspendedV5DOwner,
                this,
                V5DResumeReport))
    {
        OutError = TEXT("V5B reapply reached a valid cold state but could not resume the exact V5D inherited-turf presentation; the source remains cold and tag-free: ") +
            V5DResumeReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5BVisualActor::ValidateExploreV5BVisuals(
    FString& OutReport) const
{
    const auto Fail = [&OutReport](const FString& Message) -> bool
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5B_VISUALS_INVALID: ") + Message;
        return false;
    };

    if (!bConfigured ||
        ClaimLabel != ExpectedClaimLabel() ||
        !bAppearanceOnly ||
        !bRenderOnlyGeometry ||
        bCollisionOrNavigationAuthority ||
        bSensorOrRfMaterialAuthority ||
        bSurveyOrAsBuiltClaimed ||
        bBotanicalInventoryClaimed ||
        !bPachiraUsedOnlyAsSyntheticMorphologyProxy ||
        bSyntheticBedLocationsClaimedObserved ||
        bFountainHydraulicSimulationClaimed ||
        bGoogleOrOneMapContentUsed)
    {
        return Fail(TEXT("truth/provenance flags or configured state drifted."));
    }

    FString Error;
    if (!ValidateOwnedComponentTopology(Error) ||
        !ValidateAssetRoster(SavedAssetRoster, Error) ||
        !ValidateHardscapeRenderSuccessorSource(
            PublicViewSceneActor,
            SavedAssetRoster,
            Error))
    {
        return Fail(Error);
    }
    if (!PublicViewSceneActor ||
        !PublicViewSceneActor->Tags.Contains(V5BRenderSuccessorTag) ||
        !HasExactHardscapeRenderSuccessorState(PublicViewSceneActor) ||
        !ExploreV4LandscapeActor ||
        !ExploreV4LandscapeActor->Tags.Contains(V5BRenderSuccessorTag) ||
        !HasExactInheritedRenderSuccessorState(ExploreV4LandscapeActor) ||
        PreservedAccentSourceIndices.Num() != AccentTurfCount ||
        PreservedAccentSourceWorldTransforms.Num() != AccentTurfCount)
    {
        return Fail(TEXT("source actors, hardscape/exact 13-component inherited successor visibility states, collision preservation, or exact accent-source snapshot is incomplete."));
    }

    TArray<FTransform> CurrentCloseTurfWorldTransforms;
    if (!ExtractCloseTurfWorldTransforms(
            ExploreV4LandscapeActor,
            CurrentCloseTurfWorldTransforms,
            Error))
    {
        return Fail(Error);
    }
    for (int32 Ordinal = 0; Ordinal < AccentTurfCount; ++Ordinal)
    {
        const int32 ExpectedSourceIndex = AccentSourceIndexForOrdinal(Ordinal);
        if (PreservedAccentSourceIndices[Ordinal] != ExpectedSourceIndex ||
            !CurrentCloseTurfWorldTransforms[ExpectedSourceIndex].Equals(
                PreservedAccentSourceWorldTransforms[Ordinal],
                0.001))
        {
            return Fail(FString::Printf(
                TEXT("source transform snapshot drifted at accent ordinal %d."),
                Ordinal));
        }
    }

    TArray<FTransform> FormalShrubPrefixWorldTransforms;
    TArray<FTransform> FormalFlowerPrefixWorldTransforms;
    TArray<FTransform> FormalUnderstoreyPrefixWorldTransforms;
    if (!ExtractFormalBedSourceWorldTransforms(
            ExploreV4LandscapeActor,
            FormalShrubPrefixWorldTransforms,
            FormalFlowerPrefixWorldTransforms,
            FormalUnderstoreyPrefixWorldTransforms,
            Error) ||
        ExploreV4LandscapeActor->ShrubInstances->GetStaticMesh() !=
            SavedAssetRoster.FormalBedShrubMesh ||
        !HasExactBaseOrRuntimeMidParent(
            ExploreV4LandscapeActor->ShrubInstances->GetMaterial(0),
            SavedAssetRoster.FormalBedShrubMaterial) ||
        ExploreV4LandscapeActor->FlowerInstances->GetStaticMesh() !=
            SavedAssetRoster.FormalBedFlowerMesh ||
        !HasExactBaseOrRuntimeMidParent(
            ExploreV4LandscapeActor->FlowerInstances->GetMaterial(0),
            SavedAssetRoster.FormalBedFlowerMaterial) ||
        ExploreV4LandscapeActor->UnderstoreyInstances->GetStaticMesh() !=
            SavedAssetRoster.FormalBedUnderstoreyMesh ||
        !HasExactBaseOrRuntimeMidParent(
            ExploreV4LandscapeActor->UnderstoreyInstances->GetMaterial(0),
            SavedAssetRoster.FormalBedUnderstoreyMaterial))
    {
        return Fail(Error.IsEmpty()
            ? TEXT("formal-bed borrowed source bindings drifted.")
            : Error);
    }
    TArray<FTRIADIstanaExploreV5BTreeSource> CurrentTreeSources;
    if (!ExtractTreeSources(
            ExploreV4LandscapeActor,
            CurrentTreeSources,
            Error))
    {
        return Fail(Error);
    }

    FTRIADIstanaExploreV5BDeterministicLayout RebuiltLayout;
    if (!BuildDeterministicLayout(
            CurrentCloseTurfWorldTransforms,
            RebuiltLayout,
            Error) ||
        !BuildFormalBedCorrections(
            FormalShrubPrefixWorldTransforms,
            FormalFlowerPrefixWorldTransforms,
            FormalUnderstoreyPrefixWorldTransforms,
            RebuiltLayout,
            Error) ||
        !BuildTreeGrounding(
            CurrentTreeSources,
            RebuiltLayout,
            Error))
    {
        return Fail(Error);
    }
    const FTRIADIstanaExploreV5BDeterministicLayout SavedLayout =
        LoadSavedLayout();
    if (SavedLayout.AccentTurfSourceIndices !=
            RebuiltLayout.AccentTurfSourceIndices ||
        !TransformArraysEqual(
            SavedLayout.AccentTurfWorldTransforms,
            RebuiltLayout.AccentTurfWorldTransforms) ||
        !SavedLayout.FormalBedVeneerWorldTransform.Equals(
            RebuiltLayout.FormalBedVeneerWorldTransform,
            0.001) ||
        !TransformArraysEqual(
            SavedLayout.FormalBedShrubCorrectionWorldTransforms,
            RebuiltLayout.FormalBedShrubCorrectionWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.FormalBedFlowerCorrectionWorldTransforms,
            RebuiltLayout.FormalBedFlowerCorrectionWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.FormalBedUnderstoreyCorrectionWorldTransforms,
            RebuiltLayout.FormalBedUnderstoreyCorrectionWorldTransforms) ||
        SavedLayout.TreeBaseSourceComponentIndices !=
            RebuiltLayout.TreeBaseSourceComponentIndices ||
        SavedLayout.TreeBaseSourceInstanceIndices !=
            RebuiltLayout.TreeBaseSourceInstanceIndices ||
        !TransformArraysEqual(
            SavedLayout.TreeBaseMulchWorldTransforms,
            RebuiltLayout.TreeBaseMulchWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.TreeBaseShrubWorldTransforms,
            RebuiltLayout.TreeBaseShrubWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.TreeBaseUnderstoreyWorldTransforms,
            RebuiltLayout.TreeBaseUnderstoreyWorldTransforms) ||
        !SavedLayout.FountainSurfaceWorldTransform.Equals(
            RebuiltLayout.FountainSurfaceWorldTransform,
            0.001) ||
        !SavedLayout.FountainEdgeFoamWorldTransform.Equals(
            RebuiltLayout.FountainEdgeFoamWorldTransform,
            0.001) ||
        !TransformArraysEqual(
            SavedLayout.OuterPlumeWorldTransforms,
            RebuiltLayout.OuterPlumeWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.ImpactRingWorldTransforms,
            RebuiltLayout.ImpactRingWorldTransforms) ||
        !SavedLayout.CentralPlumeWorldTransform.Equals(
            RebuiltLayout.CentralPlumeWorldTransform,
            0.001) ||
        !TransformArraysEqual(
            SavedLayout.InnerPaverWorldTransforms,
            RebuiltLayout.InnerPaverWorldTransforms) ||
        !TransformArraysEqual(
            SavedLayout.OuterPaverWorldTransforms,
            RebuiltLayout.OuterPaverWorldTransforms))
    {
        return Fail(TEXT("persisted deterministic layout differs from a pure rebuild."));
    }
    for (int32 Variant = 0; Variant < PachiraVariantCount; ++Variant)
    {
        if (!TransformArraysEqual(
                SavedLayout.PachiraWorldTransformsByVariant[Variant],
                RebuiltLayout.PachiraWorldTransformsByVariant[Variant]))
        {
            return Fail(FString::Printf(
                TEXT("persisted Pachira morphology-proxy variant %d drifted."),
                Variant));
        }
    }
    if (!ValidateAppliedLayout(SavedAssetRoster, SavedLayout, Error))
    {
        return Fail(Error);
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5B_VISUALS_VALID ownedPrimitives=24 facadeFillComponents=1 runtimeFacadeApplied=%s runtimeFacadeMids=%d accentTurf=%d fineTurfAppearanceProxy=true botanicalSpeciesClaim=false frozenCompatibilityAssetName=BermudaTurfCluster accentCarrierTuftCenters=272 accentCarrierPairTufts=136 accentCarrierTriadTufts=136 accentCarrierBlades=680 accentCarrierRoots=680 accentCarrierClippedJuvenileBlades=510/170 accentCarrierPostureMix=306/306/68 accentCarrierCShapeCounterBend=612/68 accentCarrierHeightMix=462/184/34 accentCarrierTotalTriangleSurfaceAreaCm2=505.174458 accentCarrierTotalTriangleSurfaceAreaBoundsCm2=500/535 accentCarrierProjectedAreaRatioToR10Bounds=0.92/1.08 accentCarrierProjectedAreaMaxRatiosAtDownwardElevation0/10/20/30=1.064873/1.070234/1.076868/1.072940 accentCarrierProjectedAreaApplicability=orthographicDownwardElevation0/10/20/30Azimuth0..359NotOverhead accentCarrierLodTriangles=2550/680/204 accentCarrierVertices=3910/2040/612 accentCarrierLodScreenSizes=1.0/0.10/0.040 accentSourceHeightCm=1.6..4.4 accentFrozenTransformHeightDenominatorCm=4.8(serializedMapCompatibilityNotSourceEvidence) accentPlacedBladeHeightEnvelopeCm=0.666667..2.933333 accentPlacedMaximumHeightCm=1.833333..2.933333 accentMaterialWindHeightNormalizerCm=4.8(frozenCompatibilityNotSourceEvidence) accentDistribution=r2LowDiscrepancy accentTerrainGapCm=0.15 accentAnisotropicXY=true accentSerializedCullCm=3000/5200 accentWpoDisableCm=3200 accentBladeTonality=physicalNormalReadabilityV7 accentNormal=importedCurvatureTwoSidedFadeMatched accentWindFadeCm=3000..3200 organicBedTurfSuppression=true treeBaseMulch=%d treeBaseShrubs=%d treeBaseUnderstorey=%d treeBaseMulchUv=radialSentinelV1 treeBaseMulchFinish=darkFeatheredV12 treeBaseRadiusCm=110..180 treeBaseShrubScale=1.35..1.85x0.72XY treeBaseUnderstoreyScale=1.15..1.60x0.60XY treeBasePlantRadial=shrubs0.42..0.56/understorey0.44..0.58 treeBaseRootInsetCm=shrubs3.0/understorey4.5 treeBaseUnderstoreyLayout=twoTriads treeBasePlantSourceLod=1 treeBaseSelection=deterministicMultiSeedMaximin treeBaseFlankEnvelopeCm=5200..17000/3500..22000 treeBaseMinimumSpacingCm=550 treeBaseMinimumMulchEdgeClearanceCm=90 treeBaseGrassSuppressionRadiusFraction=0.84 treeBaseGrassCarrierFootprintMarginCm=0 treeBaseCentralLawnExcluded=true v4TreeCensus=720+9 v4TreeRenderersUnchanged=true hardscapeSuccessor=true inheritedHardscapeHidden=true originalHardscapeCollisionPreserved=true formalBedVeneers=1 formalBedShrubs=%d formalBedFlowers=%d formalBedUnderstorey=%d formalRelocatedSourcePlants=%d renderOnlyBedInfillPlants=%d preservedFlankPlants=%d formalBedMaximumReliefCm=4.8 inheritedCloseTurfAndGeometryGrassHidden=true inheritedV4PlantingRenderersHidden=true inheritedLegacyGroundPlantRenderersHidden=8 inheritedLegacyGroundPlantInstancesHidden=2096 inheritedRenderSourcesHidden=13 innerPavers=%d outerPavers=%d outerPlumes=%d impactRings=%d logicalPachira=%d renderOnly=true collision=false navigation=false sensorRfAuthority=false survey=false botanicalInventory=false"),
        bRuntimeFacadePresentationApplied ? TEXT("true") : TEXT("false"),
        RuntimeFacadeMaterialInstances.Num(),
        AccentTurfCount,
        TreeGroundingAnchorCount,
        TreeGroundingShrubCount,
        TreeGroundingUnderstoreyCount,
        FormalBedShrubRenderCount,
        FormalBedFlowerRenderCount,
        FormalBedUnderstoreyRenderCount,
        FormalBedRelocatedSourcePlantCount,
        FormalBedRenderOnlyInfillPlantCount,
        FormalBedPreservedFlankPlantCount,
        InnerPaverCount,
        OuterPaverCount,
        OuterPlumeCount,
        ImpactRingCount,
        LogicalPachiraCount);
    int32 ActiveAccentCullStart = 0;
    int32 ActiveAccentCullEnd = 0;
    AccentTurfInstances->GetCullDistances(
        ActiveAccentCullStart,
        ActiveAccentCullEnd);
    OutReport += FString::Printf(
        TEXT(" accentCarrierRevision=R11 activeAccentCullCm=%d/%d activeAccentLodDistanceScale=%.2f activeAccentPresentation=%s"),
        ActiveAccentCullStart,
        ActiveAccentCullEnd,
        AccentTurfInstances->InstanceLODDistanceScale,
        Tags.Contains(
            ATRIADIstanaExploreV5DGroundVegetationActor::
                RuntimeSourceTurfPresentationTag())
            ? TEXT("v5d_visible_medium_range_composite")
            : TEXT("cold_serialized_source"));
    OutReport += FString::Printf(
        TEXT(" facadeFillLux=%.0f facadeFillIntensityPolicy=%s"),
        FacadeFillLightComponent
            ? FacadeFillLightComponent->Intensity
            : -1.0f,
        bRuntimeFacadePresentationApplied &&
                IsExactV5DPhotographicToneWorld(GetWorld())
            ? TEXT("exactV5DMapScoped1100")
            : IsExactV5DPhotographicToneWorld(GetWorld())
                ? TEXT("exactV5DColdDefault2500AwaitingRuntime")
                : TEXT("crossMapDefault2500"));
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5BOldSchemaRollbackTest,
    "TRIAD.Istana.ExploreV5B.OldSchemaFailureAtomicRollback",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5BOldSchemaRollbackTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    ATRIADIstanaExploreV5BVisualActor* Actor =
        NewObject<ATRIADIstanaExploreV5BVisualActor>(
            GetTransientPackage(),
            NAME_None,
            RF_Transient);
    TestNotNull(TEXT("Transient V5B rollback actor exists"), Actor);
    if (!Actor)
    {
        return false;
    }

    UMaterialInterface* DefaultSurfaceMaterial =
        UMaterial::GetDefaultMaterial(MD_Surface);
    const auto NewTransientMesh = []() -> UStaticMesh*
    {
        return NewObject<UStaticMesh>(
            GetTransientPackage(),
            NAME_None,
            RF_Transient);
    };

    UStaticMesh* LegacyHardscapeMesh = NewTransientMesh();
    UStaticMesh* LegacyAccentMesh = LoadObject<UStaticMesh>(
        nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    TestNotNull(
        TEXT("Legacy accent rollback probe has renderable mesh data"),
        LegacyAccentMesh);
    if (!LegacyAccentMesh)
    {
        return false;
    }
    Actor->HardscapeRenderSuccessorComponent->SetStaticMesh(
        LegacyHardscapeMesh);
    Actor->HardscapeRenderSuccessorComponent->EmptyOverrideMaterials();
    Actor->HardscapeRenderSuccessorComponent->SetMaterial(
        0,
        DefaultSurfaceMaterial);
    Actor->HardscapeRenderSuccessorComponent->SetMaterial(
        2,
        DefaultSurfaceMaterial);
    Actor->AccentTurfInstances->SetStaticMesh(LegacyAccentMesh);
    Actor->AccentTurfInstances->EmptyOverrideMaterials();
    Actor->AccentTurfInstances->SetMaterial(0, DefaultSurfaceMaterial);
    Actor->AccentTurfInstances->SetMaterial(2, DefaultSurfaceMaterial);
    Actor->AccentTurfInstances->NumCustomDataFloats = 2;
    // Keep this raw rollback fixture deterministic: adding an instance while
    // auto-rebuild is enabled launches an asynchronous HISM tree task, which
    // can race the mutation/restore sequence below.
    Actor->AccentTurfInstances->bAutoRebuildTreeOnInstanceChanges = false;
    Actor->AccentTurfInstances->AddInstance(
        FTransform(
            FRotator(0.0, 27.0, 0.0),
            FVector(11.0, 22.0, 33.0),
            FVector(1.1, 0.9, 1.0)),
        false);
    Actor->AccentTurfInstances->PerInstanceSMCustomData = {0.25f, 0.75f};

    FV5BOwnedVisualStateSnapshot LegacySnapshot;
    FString SnapshotError;
    TestTrue(
        TEXT("Legacy raw owned state snapshots before mutation"),
        CaptureOwnedV5BVisualState(
            Actor,
            LegacySnapshot,
            SnapshotError));
    TestTrue(TEXT("Legacy snapshot clears error"), SnapshotError.IsEmpty());

    FTRIADIstanaExploreV5BAssetRoster OldSchemaAssets;
    OldSchemaAssets.HardscapeRenderSuccessorMesh = NewTransientMesh();
    OldSchemaAssets.HardscapeRenderSuccessorMaterials = {
        DefaultSurfaceMaterial,
        DefaultSurfaceMaterial,
        DefaultSurfaceMaterial};
    OldSchemaAssets.AccentTurfMesh = NewTransientMesh();
    OldSchemaAssets.AccentTurfMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.FormalBedVeneerMesh = NewTransientMesh();
    OldSchemaAssets.FormalBedVeneerMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.FormalBedShrubMesh = NewTransientMesh();
    OldSchemaAssets.FormalBedShrubMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.FormalBedFlowerMesh = NewTransientMesh();
    OldSchemaAssets.FormalBedFlowerMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.FormalBedUnderstoreyMesh = NewTransientMesh();
    OldSchemaAssets.FormalBedUnderstoreyMaterial = DefaultSurfaceMaterial;
    // The saved 36-asset schema predates this one mesh pointer.
    OldSchemaAssets.TreeBaseMulchMesh = nullptr;
    OldSchemaAssets.FountainSurfaceMesh = NewTransientMesh();
    OldSchemaAssets.FountainSurfaceMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.FountainEdgeFoamMesh = NewTransientMesh();
    OldSchemaAssets.FountainEdgeFoamMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.OuterPlumeMesh = NewTransientMesh();
    OldSchemaAssets.OuterPlumeMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.ImpactRingMesh = NewTransientMesh();
    OldSchemaAssets.ImpactRingMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.CentralPlumeMesh = NewTransientMesh();
    OldSchemaAssets.CentralPlumeMaterial = DefaultSurfaceMaterial;
    OldSchemaAssets.InnerPaverWedgeMesh = NewTransientMesh();
    OldSchemaAssets.OuterPaverWedgeMesh = NewTransientMesh();
    OldSchemaAssets.PaverMaterial = DefaultSurfaceMaterial;
    for (int32 Variant = 0; Variant < 4; ++Variant)
    {
        OldSchemaAssets.PachiraBarkMeshes.Add(NewTransientMesh());
        OldSchemaAssets.PachiraLeavesMeshes.Add(NewTransientMesh());
        OldSchemaAssets.PachiraBarkMaterials.Add(DefaultSurfaceMaterial);
        OldSchemaAssets.PachiraLeavesMaterials.Add(DefaultSurfaceMaterial);
    }

    Actor->ClearOwnedVisuals();
    Actor->AccentTurfInstances->NumCustomDataFloats = 0;
    Actor->AccentTurfInstances->PerInstanceSMCustomData.Reset();
    Actor->AccentTurfInstances->bAutoRebuildTreeOnInstanceChanges = true;
    Actor->TreeBaseShrubInstances->ForcedLodModel = 0;
    Actor->TreeBaseShrubInstances->bOverrideMinLOD = false;
    Actor->TreeBaseShrubInstances->MinLOD = 1;
    FString OldApplyError;
    TestFalse(
        TEXT("Old saved 36-asset schema cannot re-enter current ApplyLayout"),
        Actor->ApplyLayout(
            OldSchemaAssets,
            FTRIADIstanaExploreV5BDeterministicLayout(),
            OldApplyError));
    TestTrue(
        TEXT("Old-schema ApplyLayout failure is specifically the absent tree-base asset"),
        OldApplyError.Contains(TEXT("TreeBaseMulchMesh")));

    FString RestoreError;
    const bool bRawRollbackRestored = RestoreOwnedV5BVisualState(
        Actor,
        LegacySnapshot,
        RestoreError);
    TestTrue(
        *FString::Printf(
            TEXT("Raw rollback restores despite old-schema ApplyLayout rejection: %s"),
            *RestoreError),
        bRawRollbackRestored);
    TestTrue(TEXT("Raw rollback clears error"), RestoreError.IsEmpty());
    TestTrue(
        TEXT("Raw rollback restores hardscape mesh"),
        Actor->HardscapeRenderSuccessorComponent->GetStaticMesh() ==
            LegacyHardscapeMesh);
    TestEqual(
        TEXT("Raw rollback restores the full hardscape override array"),
        Actor->HardscapeRenderSuccessorComponent->OverrideMaterials.Num(),
        3);
    TestTrue(
        TEXT("Raw rollback restores accent mesh"),
        Actor->AccentTurfInstances->GetStaticMesh() ==
            LegacyAccentMesh);
    TestEqual(
        TEXT("Raw rollback restores the full HISM override array"),
        Actor->AccentTurfInstances->OverrideMaterials.Num(),
        3);
    TestEqual(
        TEXT("Raw rollback restores instance transforms"),
        Actor->AccentTurfInstances->GetInstanceCount(),
        1);
    TestEqual(
        TEXT("Raw rollback restores NumCustomDataFloats"),
        Actor->AccentTurfInstances->NumCustomDataFloats,
        2);
    const TArray<float> ExpectedCustomData = {0.25f, 0.75f};
    TestTrue(
        TEXT("Raw rollback restores all per-instance custom data"),
        Actor->AccentTurfInstances->PerInstanceSMCustomData ==
            ExpectedCustomData);
    TestFalse(
        TEXT("Raw rollback restores bAutoRebuildTreeOnInstanceChanges"),
        Actor->AccentTurfInstances->bAutoRebuildTreeOnInstanceChanges);
    TestEqual(
        TEXT("Raw rollback restores tree-base forced source LOD1"),
        Actor->TreeBaseShrubInstances->ForcedLodModel,
        TreeBasePlantForcedLodModel);
    TestTrue(
        TEXT("Raw rollback restores tree-base minimum-LOD override"),
        Actor->TreeBaseShrubInstances->bOverrideMinLOD);
    TestEqual(
        TEXT("Raw rollback restores tree-base minimum LOD"),
        Actor->TreeBaseShrubInstances->MinLOD,
        0);
    return true;
}

#endif
