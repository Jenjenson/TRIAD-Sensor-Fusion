#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr int32 MaximumGrassInstancesPerLandmark = 2048;
constexpr int32 MacDonaldGrassCount = 1536;
constexpr int32 TemasekGrassCount = 1536;
constexpr int32 CandidateAttemptsPerGrassInstance = 32;
// Keep the bounded 3,072-instance layer readable from the 72.8 m landmark
// evidence view while disabling all grass WPO beyond the 24 m near field.
constexpr int32 GrassCullStartDistanceCm = 6500;
constexpr int32 GrassCullEndDistanceCm = 9000;
constexpr int32 GrassWpoDisableDistanceCm = 2400;
constexpr float GrassLodDistanceScale = 0.60f;
// R26 reused a material whose opacity was hard-gated to zero after 28 m. R27
// uses isolated material derivatives with this same component envelope and a
// denser, still lawn-scale carrier. The source carrier is only 4.4 cm tall;
// these bounded scales put its tallest tips at 7.57--9.33 cm rather than
// manufacturing implausible waist-high blades.
constexpr double GrassCoverageScaleMinimum = 1.12;
constexpr double GrassCoverageScaleMaximum = 1.38;
constexpr double GrassHeightScaleMinimum = 1.72;
constexpr double GrassHeightScaleMaximum = 2.12;
constexpr double EvidenceViewDistanceCm = 7280.0;
constexpr double EvidenceViewHorizontalFovDegrees = 80.0;
constexpr double EvidenceViewAspectRatio = 16.0 / 9.0;
constexpr double EvidenceViewHeightPixels = 1440.0;
constexpr double GrassCarrierMaximumSourceHeightCm = 4.4;
constexpr double MinimumAcceptedTallestCarrierTipProjectionPixels = 1.50;
constexpr int32 TreeCullStartDistanceCm = 18000;
constexpr int32 TreeCullEndDistanceCm = 80000;
constexpr int32 PlantingCullStartDistanceCm = 7000;
constexpr int32 PlantingCullEndDistanceCm = 42000;
constexpr int32 MacDonaldShrubCount = 18;
constexpr int32 MacDonaldUnderstoreyCount = 22;
constexpr int32 MacDonaldFlowerCount = 32;
constexpr int32 TemasekShrubCount = 18;
constexpr int32 TemasekUnderstoreyCount = 22;
constexpr int32 TemasekFlowerCount = 32;
constexpr int32 RuntimeTreeMinimumLod = 0;
constexpr int32 RuntimeTreeForcedLodModel = 0;
constexpr float RuntimeTreeLodDistanceScale = 1.8f;
constexpr double GrassPlacementGapMinimumCm = 1.0;
constexpr double GrassPlacementGapMaximumCm = 3.0;

// R28 is an additive successor to the committed R27 state. It doubles the
// deterministic carrier census to approximately one nominal carrier footprint
// per visible square metre, removes only the artificial three-metre clearance
// around the authoritative landmark footprint, and leaves collision,
// navigation, sensor, RF and geospatial authority untouched.
constexpr int32 MaximumGrassInstancesPerLandmarkR28 = 4096;
constexpr int32 MacDonaldGrassCountR28 = 3072;
constexpr int32 TemasekGrassCountR28 = 3072;
constexpr double R28HardscapeExclusionClearanceCm = 35.0;
constexpr double GrassCarrierWidthCm = 157.07;
constexpr double GrassCarrierDepthCm = 136.84;
constexpr double R28MinimumNominalCarrierCoverage = 0.75;
constexpr uint32 R28GrassSequenceOffset = 65537u;
constexpr uint32 R28GrassProfileSalt = 0x528C91E7u;

static_assert(
    MacDonaldGrassCountR28 <= MaximumGrassInstancesPerLandmarkR28);
static_assert(
    TemasekGrassCountR28 <= MaximumGrassInstancesPerLandmarkR28);

static_assert(MacDonaldGrassCount <= MaximumGrassInstancesPerLandmark);
static_assert(TemasekGrassCount <= MaximumGrassInstancesPerLandmark);
static_assert(GrassWpoDisableDistanceCm <= GrassCullStartDistanceCm);

const FString LandmarkVegetationClaimLabel(
    TEXT("VISUAL_ASSUMPTION_BOUND_R27_LANDMARK_VEGETATION_CORRECTION"));
const FString LandmarkVegetationR28ClaimLabel(
    TEXT("VISUAL_ASSUMPTION_BOUND_R28_DENSE_TURF_MATURE_TROPICAL_CANOPY"));
const FName ActorTag(
    TEXT("TRIAD_IstanaExploreV5D_LandmarkVegetation_RenderOnly"));
const FString TemasekFoliageLayoutSchema(
    TEXT("triad.istana_explore_v5d.r24_temasek_shophouse."
         "foliage_layout.v1"));
const FString TemasekFoliageRenderOwnerClass(
    TEXT("ATRIADIstanaExploreV5DLandmarkVegetationActor"));
const TArray<FVector> TemasekTreeAnchorsLocalMeters = {
    FVector(-18.5, -19.0, 0.0),
    FVector(18.5, -18.8, 0.0),
    FVector(-13.2, -16.2, 0.0)};

const FString GrassCarrierMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5B/Vegetation/Meshes/SM_IPV5B_BermudaTurfCluster.SM_IPV5B_BermudaTurfCluster"));
const FString GrassMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Manicured.M_IPV5D_LandmarkTurf_R27_Manicured"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Humid.M_IPV5D_LandmarkTurf_R27_Humid"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Shade.M_IPV5D_LandmarkTurf_R27_Shade"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_DryEdge.M_IPV5D_LandmarkTurf_R27_DryEdge")};
const FString GrassMaterialPathsR28[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_Manicured.M_IPV5D_LandmarkTurf_R28_Manicured"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_Humid.M_IPV5D_LandmarkTurf_R28_Humid"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_Shade.M_IPV5D_LandmarkTurf_R28_Shade"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR28/Materials/M_IPV5D_LandmarkTurf_R28_DryEdge.M_IPV5D_LandmarkTurf_R28_DryEdge")};
const FString UmbrellaTreeMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"));
const FString DomeTreeMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0"));
const FString HighForkTreeMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0"));
const FString ShrubMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Shrub04_A.SM_IPV4_Shrub04_A"));
const FString ShrubMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Shrub04_Wind.M_IPV4_Shrub04_Wind"));
const FString UnderstoreyMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Calathea_D.SM_IPV4_Calathea_D"));
const FString UnderstoreyMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Calathea_Wind.M_IPV4_Calathea_Wind"));
const FString FlowerMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Periwinkle06_F.SM_IPV4_Periwinkle06_F"));
const FString FlowerMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Periwinkle_Wind.M_IPV4_Periwinkle_Wind"));

struct FLandmarkSiteSpec
{
    ETRIADIstanaExploreV5DLandmarkVegetationSite Site;
    FVector AnchorCentimeters;
    double YawDegrees;
    FVector2D GrassSemiAxesCentimeters;
    FVector2D GrassHardscapeExclusionHalfExtentsCentimeters;
    int32 GrassCount;
    uint32 Seed;
};

FLandmarkSiteSpec GetSiteSpec(
    ETRIADIstanaExploreV5DLandmarkVegetationSite Site)
{
    if (Site ==
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse)
    {
        return {
            Site,
            FVector(36320.390052826, 87155.365772797, 0.0),
            -161.885822122792,
            FVector2D(7200.0, 5000.0),
            // 30 x 16 m contract footprint plus a 3 m visual-only margin.
            FVector2D(1800.0, 1100.0),
            MacDonaldGrassCount,
            0x4D414344u};
    }
    return {
        Site,
        FVector(40411.657951, 88424.311139, 0.0),
        -162.5152283523459,
        FVector2D(6900.0, 4700.0),
        // 40 x 25 m contract footprint plus a 3 m visual-only margin.
        FVector2D(2300.0, 1550.0),
        TemasekGrassCount,
        0x54454D41u};
}

FLandmarkSiteSpec GetR28SiteSpec(
    ETRIADIstanaExploreV5DLandmarkVegetationSite Site)
{
    if (Site ==
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse)
    {
        return {
            Site,
            FVector(36320.390052826, 87155.365772797, 0.0),
            -161.885822122792,
            FVector2D(7200.0, 5000.0),
            // Preserve the exact 30 x 16 m building footprint and only a
            // 35 cm render clearance. R27's extra three-metre visual margin
            // was the source of the observed central bald stripe.
            FVector2D(
                1500.0 + R28HardscapeExclusionClearanceCm,
                800.0 + R28HardscapeExclusionClearanceCm),
            MacDonaldGrassCountR28,
            0x4D414344u ^ 0x0000001Cu};
    }
    return {
        Site,
        FVector(40411.657951, 88424.311139, 0.0),
        -162.5152283523459,
        FVector2D(6900.0, 4700.0),
        // Preserve the exact 40 x 25 m source footprint with the same narrow
        // render clearance; no grass is admitted beneath building geometry.
        FVector2D(
            2000.0 + R28HardscapeExclusionClearanceCm,
            1250.0 + R28HardscapeExclusionClearanceCm),
        TemasekGrassCountR28,
        0x54454D41u ^ 0x0000001Cu};
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

double HashUnit(uint32 Value)
{
    return static_cast<double>(MixBits(Value) & 0x00FFFFFFu) /
        static_cast<double>(0x01000000u);
}

double RadicalInverse(uint32 Index, uint32 Base)
{
    const double InverseBase = 1.0 / static_cast<double>(Base);
    double Fraction = InverseBase;
    double Result = 0.0;
    while (Index > 0u)
    {
        Result += static_cast<double>(Index % Base) * Fraction;
        Index /= Base;
        Fraction *= InverseBase;
    }
    return Result;
}

// Exact copy of the V5B/V5D analytic accent-terrain surface. The landmark
// patch never traces collision and therefore cannot acquire geometry authority.
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

FVector2D LocalToWorldXY(
    const FLandmarkSiteSpec& Spec,
    const FVector2D& LocalCentimeters)
{
    const double YawRadians = FMath::DegreesToRadians(Spec.YawDegrees);
    const double CosYaw = FMath::Cos(YawRadians);
    const double SinYaw = FMath::Sin(YawRadians);
    return FVector2D(
        Spec.AnchorCentimeters.X +
            LocalCentimeters.X * CosYaw - LocalCentimeters.Y * SinYaw,
        Spec.AnchorCentimeters.Y +
            LocalCentimeters.X * SinYaw + LocalCentimeters.Y * CosYaw);
}

FVector2D WorldToLocalXY(
    const FLandmarkSiteSpec& Spec,
    const FVector& WorldCentimeters)
{
    const double YawRadians = FMath::DegreesToRadians(Spec.YawDegrees);
    const double CosYaw = FMath::Cos(YawRadians);
    const double SinYaw = FMath::Sin(YawRadians);
    const double DX = WorldCentimeters.X - Spec.AnchorCentimeters.X;
    const double DY = WorldCentimeters.Y - Spec.AnchorCentimeters.Y;
    return FVector2D(
        DX * CosYaw + DY * SinYaw,
        -DX * SinYaw + DY * CosYaw);
}

bool IsInsideGrassPatch(
    const FLandmarkSiteSpec& Spec,
    const FVector2D& LocalCentimeters)
{
    const double Ellipse =
        FMath::Square(
            LocalCentimeters.X / Spec.GrassSemiAxesCentimeters.X) +
        FMath::Square(
            LocalCentimeters.Y / Spec.GrassSemiAxesCentimeters.Y);
    const FVector2D World = LocalToWorldXY(Spec, LocalCentimeters);
    const ETRIADIstanaExploreV5DLandmarkVegetationSite OtherSite =
        Spec.Site ==
            ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse
        ? ETRIADIstanaExploreV5DLandmarkVegetationSite::TemasekShophouse
        : ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse;
    const FVector OtherAnchor = GetSiteSpec(OtherSite).AnchorCentimeters;
    const double OwnDistanceSquared =
        FMath::Square(World.X - Spec.AnchorCentimeters.X) +
        FMath::Square(World.Y - Spec.AnchorCentimeters.Y);
    const double OtherDistanceSquared =
        FMath::Square(World.X - OtherAnchor.X) +
        FMath::Square(World.Y - OtherAnchor.Y);
    return Ellipse <= 1.0 &&
        OwnDistanceSquared <= OtherDistanceSquared &&
        (FMath::Abs(LocalCentimeters.X) >
             Spec.GrassHardscapeExclusionHalfExtentsCentimeters.X ||
         FMath::Abs(LocalCentimeters.Y) >
             Spec.GrassHardscapeExclusionHalfExtentsCentimeters.Y);
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

FTransform MakeWorldTransform(
    const FLandmarkSiteSpec& Spec,
    const FVector2D& LocalCentimeters,
    double YawOffsetDegrees,
    const FVector& Scale,
    double PlacementGapCm)
{
    const FVector2D WorldXY = LocalToWorldXY(Spec, LocalCentimeters);
    return FTransform(
        FRotator(0.0, Spec.YawDegrees + YawOffsetDegrees, 0.0),
        FVector(
            WorldXY.X,
            WorldXY.Y,
            AnalyticAccentTerrainHeightCm(WorldXY.X, WorldXY.Y) +
                PlacementGapCm),
        Scale);
}

void AppendGrassTransform(
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Layout,
    int32 Profile,
    const FTransform& Transform)
{
    switch (Profile)
    {
    case 0:
        Layout.GrassManicured.Add(Transform);
        break;
    case 1:
        Layout.GrassHumid.Add(Transform);
        break;
    case 2:
        Layout.GrassShade.Add(Transform);
        break;
    default:
        Layout.GrassDryEdge.Add(Transform);
        break;
    }
}

bool BuildGrassLayout(
    const FLandmarkSiteSpec& Spec,
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& OutLayout,
    FString& OutError)
{
    const int32 CandidateCeiling =
        Spec.GrassCount * CandidateAttemptsPerGrassInstance;
    for (int32 Candidate = 1;
         Candidate <= CandidateCeiling &&
             OutLayout.GrassTotal() < Spec.GrassCount;
         ++Candidate)
    {
        const FVector2D Local(
            (2.0 * RadicalInverse(
                       static_cast<uint32>(Candidate),
                       2u) -
                1.0) *
                Spec.GrassSemiAxesCentimeters.X,
            (2.0 * RadicalInverse(
                       static_cast<uint32>(Candidate),
                       3u) -
                1.0) *
                Spec.GrassSemiAxesCentimeters.Y);
        if (!IsInsideGrassPatch(Spec, Local))
        {
            continue;
        }

        const uint32 Stable = MixBits(
            static_cast<uint32>(Candidate) ^ Spec.Seed);
        const uint32 ProfileBucket = MixBits(Stable ^ 0xA17C9E21u) % 100u;
        const int32 Profile = ProfileBucket < 55u
            ? 0
            : (ProfileBucket < 75u ? 1 : (ProfileBucket < 90u ? 2 : 3));
        const double CoverageScale = FMath::Lerp(
            GrassCoverageScaleMinimum,
            GrassCoverageScaleMaximum,
            HashUnit(Stable ^ 0x10C0A7E5u));
        const double HeightScale = FMath::Lerp(
            GrassHeightScaleMinimum,
            GrassHeightScaleMaximum,
            HashUnit(Stable ^ 0x73A5B91Du));
        const double GapCm = FMath::Lerp(
            GrassPlacementGapMinimumCm,
            GrassPlacementGapMaximumCm,
            HashUnit(Stable ^ 0xC5EED123u));
        AppendGrassTransform(
            OutLayout,
            Profile,
            MakeWorldTransform(
                Spec,
                Local,
                360.0 * HashUnit(Stable ^ 0xB5297A4Du),
                FVector(CoverageScale, CoverageScale, HeightScale),
                GapCm));
    }

    if (OutLayout.GrassTotal() != Spec.GrassCount)
    {
        OutError = FString::Printf(
            TEXT("Landmark site %d generated %d of %d grass transforms within its deterministic candidate ceiling %d."),
            static_cast<int32>(Spec.Site),
            OutLayout.GrassTotal(),
            Spec.GrassCount,
            CandidateCeiling);
        return false;
    }
    OutError.Reset();
    return true;
}

double NominalR28CarrierCoverage(const FLandmarkSiteSpec& Spec);
bool ComponentMatchesTransforms(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<FTransform>& First,
    const TArray<FTransform>& Second);
bool HasRenderOnlyPolicy(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    int32 ExpectedCullStart,
    int32 ExpectedCullEnd,
    int32 ExpectedWpoDisable);
} // namespace

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ValidateLandmarkVegetationR28(FString& OutReport) const
{
    FString Error;
    if (!ValidateAssetRosterR28(SavedAssets, Error) ||
        !ValidateLayoutR28(SavedLayout, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_INVALID_LAYOUT_OR_ASSETS: ") +
            Error;
        return false;
    }
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0001) ||
        GetActorEnableCollision() ||
        !Tags.Contains(ExpectedActorTag()) ||
        ClaimLabel != ExpectedR28ClaimLabel() ||
        !bAppearanceOnly || bSourceAssetPackagesModified ||
        bCollisionOrNavigationAuthority || bSensorOrRfAuthority ||
        bBotanicalSurveyOrCurrentSeasonClaimed)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_INVALID_TRUTH_BOUNDARY");
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         GrassComponents)
    {
        if (!HasRenderOnlyPolicy(
                Component,
                GrassCullStartDistanceCm,
                GrassCullEndDistanceCm,
                GrassWpoDisableDistanceCm) ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                GrassLodDistanceScale,
                0.0001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_INVALID_GRASS_RENDER_POLICY");
            return false;
        }
    }

    const UHierarchicalInstancedStaticMeshComponent* TreeComponents[] = {
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkTreeInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         TreeComponents)
    {
        if (!HasRenderOnlyPolicy(
                Component,
                TreeCullStartDistanceCm,
                TreeCullEndDistanceCm,
                GrassWpoDisableDistanceCm) ||
            !Component->bOverrideMinLOD ||
            Component->MinLOD != RuntimeTreeMinimumLod ||
            Component->ForcedLodModel != RuntimeTreeForcedLodModel ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                RuntimeTreeLodDistanceScale,
                0.0001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_INVALID_TREE_RENDER_POLICY");
            return false;
        }
    }

    const UHierarchicalInstancedStaticMeshComponent* PlantingComponents[] = {
        ShrubInstances,
        UnderstoreyInstances,
        FlowerInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         PlantingComponents)
    {
        if (!HasRenderOnlyPolicy(
                Component,
                PlantingCullStartDistanceCm,
                PlantingCullEndDistanceCm,
                GrassWpoDisableDistanceCm))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_INVALID_PLANTING_RENDER_POLICY");
            return false;
        }
    }

    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& MacDonald =
        SavedLayout.MacDonaldHouse;
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Temasek =
        SavedLayout.TemasekShophouse;
    const bool bTransformsMatch =
        ComponentMatchesTransforms(
            GrassManicuredInstances,
            MacDonald.GrassManicured,
            Temasek.GrassManicured) &&
        ComponentMatchesTransforms(
            GrassHumidInstances,
            MacDonald.GrassHumid,
            Temasek.GrassHumid) &&
        ComponentMatchesTransforms(
            GrassShadeInstances,
            MacDonald.GrassShade,
            Temasek.GrassShade) &&
        ComponentMatchesTransforms(
            GrassDryEdgeInstances,
            MacDonald.GrassDryEdge,
            Temasek.GrassDryEdge) &&
        ComponentMatchesTransforms(
            UmbrellaTreeInstances,
            MacDonald.TreesUmbrella,
            Temasek.TreesUmbrella) &&
        ComponentMatchesTransforms(
            DomeTreeInstances,
            MacDonald.TreesDome,
            Temasek.TreesDome) &&
        ComponentMatchesTransforms(
            HighForkTreeInstances,
            MacDonald.TreesHighFork,
            Temasek.TreesHighFork) &&
        ComponentMatchesTransforms(
            ShrubInstances,
            MacDonald.Shrubs,
            Temasek.Shrubs) &&
        ComponentMatchesTransforms(
            UnderstoreyInstances,
            MacDonald.Understorey,
            Temasek.Understorey) &&
        ComponentMatchesTransforms(
            FlowerInstances,
            MacDonald.Flowers,
            Temasek.Flowers);
    if (!bTransformsMatch)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_INVALID_WORLD_TRANSFORMS");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R28_VALID actorContractVersion=3 actorMapIntegrationAuthority=false worldSpace=true deterministic=true nearestLandmarkPartition=true r27StatePreserved=true r28DenseTurfConfigured=true r28HealthyTropicalColourConfigured=true r28MatureTemasekCanopyConfigured=true evidenceRangeMeters=72.8 materialVisibilityAtEvidenceRange=%.3f grassScaleXY=%.2f,%.2f grassScaleZ=%.2f,1.98 grassProfilePercent=70,20,9,1 hardscapeRenderClearanceCm=%.1f macDonaldNominalCarrierCoverage=%.3f temasekNominalCarrierCoverage=%.3f macDonaldGrass=%d temasekGrass=%d maximumGrassPerSite=%d temasekTreeRoster=umbrella,dome,umbrella temasekTreeScalesAnisotropic=true sourceTreeAnchorTranslationsPreserved=true visualCaptureAccepted=false captureRevalidationRequired=true renderOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false sourceAssetsModified=false surveyClaim=false botanicalClaim=false seasonalClaim=false"),
        ExpectedMaterialVisibilityAtEvidenceView(),
        GrassCoverageScaleMinimum,
        GrassCoverageScaleMaximum,
        GrassHeightScaleMinimum,
        R28HardscapeExclusionClearanceCm,
        NominalR28CarrierCoverage(GetR28SiteSpec(
            ETRIADIstanaExploreV5DLandmarkVegetationSite::
                MacDonaldHouse)),
        NominalR28CarrierCoverage(GetR28SiteSpec(
            ETRIADIstanaExploreV5DLandmarkVegetationSite::
                TemasekShophouse)),
        MacDonald.GrassTotal(),
        Temasek.GrassTotal(),
        MaximumGrassInstancesPerLandmarkR28);
    return true;
}

namespace
{
double NominalR28CarrierCoverage(const FLandmarkSiteSpec& Spec)
{
    const double EllipseAreaSquareCentimeters = PI *
        Spec.GrassSemiAxesCentimeters.X *
        Spec.GrassSemiAxesCentimeters.Y;
    const double ExclusionAreaSquareCentimeters = 4.0 *
        Spec.GrassHardscapeExclusionHalfExtentsCentimeters.X *
        Spec.GrassHardscapeExclusionHalfExtentsCentimeters.Y;
    const double ConservativeOwnedAreaSquareCentimeters = FMath::Max(
        1.0,
        EllipseAreaSquareCentimeters - ExclusionAreaSquareCentimeters);
    const double MinimumCarrierFootprintSquareCentimeters =
        GrassCarrierWidthCm * GrassCarrierDepthCm *
        FMath::Square(GrassCoverageScaleMinimum);
    return Spec.GrassCount * MinimumCarrierFootprintSquareCentimeters /
        ConservativeOwnedAreaSquareCentimeters;
}

bool BuildGrassLayoutR28(
    const FLandmarkSiteSpec& Spec,
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& OutLayout,
    FString& OutError)
{
    const int32 CandidateCeiling =
        Spec.GrassCount * CandidateAttemptsPerGrassInstance;
    for (int32 Candidate = 1;
         Candidate <= CandidateCeiling &&
             OutLayout.GrassTotal() < Spec.GrassCount;
         ++Candidate)
    {
        const uint32 SequenceIndex =
            static_cast<uint32>(Candidate) + R28GrassSequenceOffset;
        const FVector2D Local(
            (2.0 * RadicalInverse(SequenceIndex, 2u) - 1.0) *
                Spec.GrassSemiAxesCentimeters.X,
            (2.0 * RadicalInverse(SequenceIndex, 3u) - 1.0) *
                Spec.GrassSemiAxesCentimeters.Y);
        if (!IsInsideGrassPatch(Spec, Local))
        {
            continue;
        }

        const uint32 Stable = MixBits(SequenceIndex ^ Spec.Seed);
        const uint32 ProfileBucket =
            MixBits(Stable ^ R28GrassProfileSalt) % 100u;
        // Managed tropical lawn: the dry-edge material remains represented for
        // variation but is deliberately rare instead of painting the whole
        // near field with repeated straw-coloured silhouettes.
        const int32 Profile = ProfileBucket < 70u
            ? 0
            : (ProfileBucket < 90u ? 1 : (ProfileBucket < 99u ? 2 : 3));
        const double CoverageScale = FMath::Lerp(
            GrassCoverageScaleMinimum,
            GrassCoverageScaleMaximum,
            HashUnit(Stable ^ 0x10C0A7E5u));
        const double HeightScale = FMath::Lerp(
            GrassHeightScaleMinimum,
            1.98,
            HashUnit(Stable ^ 0x73A5B91Du));
        const double GapCm = FMath::Lerp(
            GrassPlacementGapMinimumCm,
            GrassPlacementGapMaximumCm,
            HashUnit(Stable ^ 0xC5EED123u));
        AppendGrassTransform(
            OutLayout,
            Profile,
            MakeWorldTransform(
                Spec,
                Local,
                360.0 * HashUnit(Stable ^ 0xB5297A4Du),
                FVector(CoverageScale, CoverageScale, HeightScale),
                GapCm));
    }

    if (OutLayout.GrassTotal() != Spec.GrassCount)
    {
        OutError = FString::Printf(
            TEXT("R28 landmark site %d generated %d of %d dense-grass transforms within candidate ceiling %d."),
            static_cast<int32>(Spec.Site),
            OutLayout.GrassTotal(),
            Spec.GrassCount,
            CandidateCeiling);
        return false;
    }
    OutError.Reset();
    return true;
}

void AppendTree(
    const FLandmarkSiteSpec& Spec,
    const FVector2D& LocalCentimeters,
    double YawOffsetDegrees,
    double UniformScale,
    TArray<FTransform>& OutTransforms)
{
    OutTransforms.Add(MakeWorldTransform(
        Spec,
        LocalCentimeters,
        YawOffsetDegrees,
        FVector(UniformScale),
        0.0));
}

FTransform MakeTemasekContractTreeWorldTransform(
    int32 AnchorIndex,
    double YawOffsetDegrees,
    double UniformScale)
{
    const FTransform Placement =
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTemasekSourcePlacementTransform();
    const TArray<FVector>& Anchors =
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTemasekTreeAnchorsLocalMeters();
    const FVector FlatWorld = Placement.TransformPosition(
        Anchors[AnchorIndex] * 100.0);
    return FTransform(
        FRotator(
            0.0,
            Placement.Rotator().Yaw + YawOffsetDegrees,
            0.0),
        FVector(
            FlatWorld.X,
            FlatWorld.Y,
            AnalyticAccentTerrainHeightCm(FlatWorld.X, FlatWorld.Y)),
        FVector(UniformScale));
}

void BuildTrees(
    const FLandmarkSiteSpec& Spec,
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& OutLayout)
{
    if (Spec.Site ==
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse)
    {
        // Assumption-bound screening positions outside the house hardscape.
        AppendTree(Spec, FVector2D(3900.0, -2800.0), 14.0, 0.98, OutLayout.TreesUmbrella);
        AppendTree(Spec, FVector2D(4100.0, 2500.0), -21.0, 1.04, OutLayout.TreesUmbrella);
        AppendTree(Spec, FVector2D(3000.0, -3000.0), 37.0, 0.96, OutLayout.TreesDome);
        AppendTree(Spec, FVector2D(-1500.0, 3200.0), -32.0, 1.02, OutLayout.TreesHighFork);
        return;
    }

    // Exact R24 source-local anchors, transformed by the same pinned
    // translation/yaw/non-uniform plan scale as the source render mesh.
    OutLayout.TreesUmbrella.Add(
        MakeTemasekContractTreeWorldTransform(0, 8.0, 0.98));
    OutLayout.TreesDome.Add(
        MakeTemasekContractTreeWorldTransform(1, -13.0, 1.01));
    OutLayout.TreesHighFork.Add(
        MakeTemasekContractTreeWorldTransform(2, 29.0, 1.00));
}

FTransform MakeTemasekContractTreeWorldTransformR28(
    int32 AnchorIndex,
    double YawOffsetDegrees,
    const FVector& Scale)
{
    const FTransform Placement =
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTemasekSourcePlacementTransform();
    const TArray<FVector>& Anchors =
        ATRIADIstanaExploreV5DLandmarkVegetationActor::
            ExpectedTemasekTreeAnchorsLocalMeters();
    const FVector FlatWorld = Placement.TransformPosition(
        Anchors[AnchorIndex] * 100.0);
    return FTransform(
        FRotator(
            0.0,
            Placement.Rotator().Yaw + YawOffsetDegrees,
            0.0),
        FVector(
            FlatWorld.X,
            FlatWorld.Y,
            AnalyticAccentTerrainHeightCm(FlatWorld.X, FlatWorld.Y)),
        Scale);
}

void BuildTreesR28(
    const FLandmarkSiteSpec& Spec,
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& OutLayout)
{
    if (Spec.Site ==
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse)
    {
        // The R27 MacDonald canopy already passed visual inspection.
        BuildTrees(Spec, OutLayout);
        return;
    }

    // Preserve the three Phase-2 anchor translations exactly. Replace the
    // visibly juvenile/high-fork silhouette with a second irregular umbrella
    // crown, and use anisotropic scale so no crown becomes a spherical blob.
    OutLayout.TreesUmbrella.Add(
        MakeTemasekContractTreeWorldTransformR28(
            0, 8.0, FVector(2.05, 1.78, 1.72)));
    OutLayout.TreesDome.Add(
        MakeTemasekContractTreeWorldTransformR28(
            1, -13.0, FVector(1.82, 2.00, 1.68)));
    OutLayout.TreesUmbrella.Add(
        MakeTemasekContractTreeWorldTransformR28(
            2, 29.0, FVector(1.92, 1.72, 1.58)));
}

bool AppendPlantingBeds(
    const FLandmarkSiteSpec& Spec,
    int32 Count,
    double BedRadiusXCentimeters,
    double BedRadiusYCentimeters,
    double PhaseRadians,
    double MinimumScale,
    double MaximumScale,
    uint32 Salt,
    TArray<FTransform>& OutTransforms,
    FString& OutError)
{
    const FVector2D BedCentersNormalized[] = {
        FVector2D(-0.50, -0.43),
        FVector2D(0.52, -0.40),
        FVector2D(-0.52, 0.43),
        FVector2D(0.50, 0.44)};
    constexpr double GoldenAngleRadians = 2.39996322972865332;
    OutTransforms.Reserve(Count);
    const int32 CandidateCeiling = Count * 24;
    for (int32 Candidate = 1;
         Candidate <= CandidateCeiling && OutTransforms.Num() < Count;
         ++Candidate)
    {
        const uint32 Stable = MixBits(
            Spec.Seed ^ Salt ^ static_cast<uint32>(Candidate));
        const int32 BedIndex = static_cast<int32>(
            MixBits(Stable ^ 0x6D2B79F5u) %
            UE_ARRAY_COUNT(BedCentersNormalized));
        const FVector2D BedCenter(
            BedCentersNormalized[BedIndex].X *
                Spec.GrassSemiAxesCentimeters.X,
            BedCentersNormalized[BedIndex].Y *
                Spec.GrassSemiAxesCentimeters.Y);
        const double Angle = PhaseRadians + GoldenAngleRadians * Candidate +
            FMath::DegreesToRadians(
                -7.0 + 14.0 * HashUnit(Stable ^ 0x38B42F11u));
        const double Radius = FMath::Sqrt(
            RadicalInverse(static_cast<uint32>(Candidate), 2u));
        const FVector2D Local(
            BedCenter.X +
                BedRadiusXCentimeters * Radius * FMath::Cos(Angle),
            BedCenter.Y +
                BedRadiusYCentimeters * Radius * FMath::Sin(Angle));
        if (!IsInsideGrassPatch(Spec, Local))
        {
            continue;
        }
        const double UniformScale = FMath::Lerp(
            MinimumScale,
            MaximumScale,
            HashUnit(Stable ^ 0xE31D76A9u));
        OutTransforms.Add(MakeWorldTransform(
            Spec,
            Local,
            360.0 * HashUnit(Stable ^ 0xA42D930Bu),
            FVector(UniformScale),
            0.5));
    }
    if (OutTransforms.Num() != Count)
    {
        OutError = FString::Printf(
            TEXT("Landmark site %d planting bed generated %d of %d transforms within candidate ceiling %d."),
            static_cast<int32>(Spec.Site),
            OutTransforms.Num(),
            Count,
            CandidateCeiling);
        return false;
    }
    return true;
}

bool BuildPlanting(
    const FLandmarkSiteSpec& Spec,
    FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& OutLayout,
    FString& OutError)
{
    const bool bMacDonald =
        Spec.Site ==
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse;
    return AppendPlantingBeds(
        Spec,
        bMacDonald ? MacDonaldShrubCount : TemasekShrubCount,
        1150.0,
        760.0,
        0.17,
        0.82,
        1.14,
        0x51A5D00Du,
        OutLayout.Shrubs,
        OutError) &&
    AppendPlantingBeds(
        Spec,
        bMacDonald
            ? MacDonaldUnderstoreyCount
            : TemasekUnderstoreyCount,
        950.0,
        690.0,
        0.49,
        0.84,
        1.18,
        0x71C44E29u,
        OutLayout.Understorey,
        OutError) &&
    AppendPlantingBeds(
        Spec,
        bMacDonald ? MacDonaldFlowerCount : TemasekFlowerCount,
        820.0,
        620.0,
        0.31,
        0.86,
        1.16,
        0x942B10F3u,
        OutLayout.Flowers,
        OutError);
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
            TEXT("Landmark %s expected %d transforms; found %d."),
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
                TEXT("Landmark %s transform %d is invalid."),
                Label,
                Index);
            return false;
        }
    }
    return true;
}

bool ValidatePatchOwnedTransformArray(
    const FLandmarkSiteSpec& Spec,
    const TArray<FTransform>& Transforms,
    int32 ExpectedCount,
    const TCHAR* Label,
    FString& OutError)
{
    if (!ValidateTransformArray(
            Transforms,
            ExpectedCount,
            Label,
            OutError))
    {
        return false;
    }
    for (int32 Index = 0; Index < Transforms.Num(); ++Index)
    {
        if (!IsInsideGrassPatch(
                Spec,
                WorldToLocalXY(
                    Spec,
                    Transforms[Index].GetTranslation())))
        {
            OutError = FString::Printf(
                TEXT("Landmark %s transform %d left its site-owned patch or entered the contract-footprint exclusion."),
                Label,
                Index);
            return false;
        }
    }
    return true;
}

bool ValidateGrassTransforms(
    const FLandmarkSiteSpec& Spec,
    const TArray<FTransform>* Profiles,
    FString& OutError)
{
    int32 Total = 0;
    for (int32 Profile = 0; Profile < 4; ++Profile)
    {
        if (Profiles[Profile].IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("Landmark site %d grass profile %d is empty."),
                static_cast<int32>(Spec.Site),
                Profile);
            return false;
        }
        Total += Profiles[Profile].Num();
        for (int32 Index = 0; Index < Profiles[Profile].Num(); ++Index)
        {
            const FTransform& Transform = Profiles[Profile][Index];
            const FVector Location = Transform.GetTranslation();
            const FVector2D Local = WorldToLocalXY(Spec, Location);
            const double GroundZ = AnalyticAccentTerrainHeightCm(
                Location.X,
                Location.Y);
            const double GapCm = Location.Z - GroundZ;
            const FVector Scale = Transform.GetScale3D();
            if (!IsFinitePositiveTransform(Transform) ||
                !IsInsideGrassPatch(Spec, Local) ||
                GapCm < GrassPlacementGapMinimumCm - 0.0001 ||
                GapCm > GrassPlacementGapMaximumCm + 0.0001 ||
                Scale.X < GrassCoverageScaleMinimum - 0.0001 ||
                Scale.X > GrassCoverageScaleMaximum + 0.0001 ||
                !FMath::IsNearlyEqual(Scale.X, Scale.Y, 0.0001) ||
                Scale.Z < GrassHeightScaleMinimum - 0.0001 ||
                Scale.Z > GrassHeightScaleMaximum + 0.0001)
            {
                OutError = FString::Printf(
                    TEXT("Landmark site %d grass profile %d transform %d left its deterministic world-space patch, analytic terrain gap, or R27 distance-readable scale envelope."),
                    static_cast<int32>(Spec.Site),
                    Profile,
                    Index);
                return false;
            }
        }
    }
    if (Total != Spec.GrassCount ||
        Total > MaximumGrassInstancesPerLandmark)
    {
        OutError = FString::Printf(
            TEXT("Landmark site %d grass census %d violates exact %d / maximum %d."),
            static_cast<int32>(Spec.Site),
            Total,
            Spec.GrassCount,
            MaximumGrassInstancesPerLandmark);
        return false;
    }
    return true;
}

bool ValidateSiteLayout(
    const FLandmarkSiteSpec& Spec,
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Layout,
    FString& OutError)
{
    const TArray<FTransform> GrassProfiles[] = {
        Layout.GrassManicured,
        Layout.GrassHumid,
        Layout.GrassShade,
        Layout.GrassDryEdge};
    if (!ValidateGrassTransforms(Spec, GrassProfiles, OutError))
    {
        return false;
    }

    const bool bMacDonald =
        Spec.Site ==
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse;
    return
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.TreesUmbrella,
            bMacDonald ? 2 : 1,
            TEXT("umbrella trees"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.TreesDome,
            1,
            TEXT("dome trees"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.TreesHighFork,
            1,
            TEXT("high-fork trees"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.Shrubs,
            bMacDonald ? MacDonaldShrubCount : TemasekShrubCount,
            TEXT("shrubs"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.Understorey,
            bMacDonald
                ? MacDonaldUnderstoreyCount
                : TemasekUnderstoreyCount,
            TEXT("understorey"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.Flowers,
            bMacDonald ? MacDonaldFlowerCount : TemasekFlowerCount,
            TEXT("flowers"),
            OutError);
}

bool ValidateTemasekTreeAnchorContract(
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Layout,
    FString& OutError)
{
    const FTransform Expected[] = {
        MakeTemasekContractTreeWorldTransform(0, 8.0, 0.98),
        MakeTemasekContractTreeWorldTransform(1, -13.0, 1.01),
        MakeTemasekContractTreeWorldTransform(2, 29.0, 1.00)};
    if (Layout.TreesUmbrella.Num() != 1 ||
        Layout.TreesDome.Num() != 1 ||
        Layout.TreesHighFork.Num() != 1 ||
        !Layout.TreesUmbrella[0].Equals(Expected[0], 0.001) ||
        !Layout.TreesDome[0].Equals(Expected[1], 0.001) ||
        !Layout.TreesHighFork[0].Equals(Expected[2], 0.001))
    {
        OutError = TEXT("Temasek foliage handoff lost one of its exact three R24 source-local tree anchors or pinned source placement transform.");
        return false;
    }
    return true;
}

bool ValidateGrassTransformsR28(
    const FLandmarkSiteSpec& Spec,
    const TArray<FTransform>* Profiles,
    FString& OutError)
{
    int32 Total = 0;
    for (int32 Profile = 0; Profile < 4; ++Profile)
    {
        if (Profiles[Profile].IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("R28 landmark site %d grass profile %d is empty."),
                static_cast<int32>(Spec.Site),
                Profile);
            return false;
        }
        Total += Profiles[Profile].Num();
        for (int32 Index = 0; Index < Profiles[Profile].Num(); ++Index)
        {
            const FTransform& Transform = Profiles[Profile][Index];
            const FVector Location = Transform.GetTranslation();
            const FVector2D Local = WorldToLocalXY(Spec, Location);
            const double GroundZ = AnalyticAccentTerrainHeightCm(
                Location.X,
                Location.Y);
            const double GapCm = Location.Z - GroundZ;
            const FVector Scale = Transform.GetScale3D();
            if (!IsFinitePositiveTransform(Transform) ||
                !IsInsideGrassPatch(Spec, Local) ||
                GapCm < GrassPlacementGapMinimumCm - 0.0001 ||
                GapCm > GrassPlacementGapMaximumCm + 0.0001 ||
                Scale.X < GrassCoverageScaleMinimum - 0.0001 ||
                Scale.X > GrassCoverageScaleMaximum + 0.0001 ||
                !FMath::IsNearlyEqual(Scale.X, Scale.Y, 0.0001) ||
                Scale.Z < GrassHeightScaleMinimum - 0.0001 ||
                Scale.Z > 1.98 + 0.0001)
            {
                OutError = FString::Printf(
                    TEXT("R28 landmark site %d grass profile %d transform %d left its deterministic world-space patch, analytic terrain gap, or dense-turf scale envelope."),
                    static_cast<int32>(Spec.Site),
                    Profile,
                    Index);
                return false;
            }
        }
    }
    if (Total != Spec.GrassCount ||
        Total > MaximumGrassInstancesPerLandmarkR28)
    {
        OutError = FString::Printf(
            TEXT("R28 landmark site %d grass census %d violates exact %d / maximum %d."),
            static_cast<int32>(Spec.Site),
            Total,
            Spec.GrassCount,
            MaximumGrassInstancesPerLandmarkR28);
        return false;
    }
    if (NominalR28CarrierCoverage(Spec) <
        R28MinimumNominalCarrierCoverage)
    {
        OutError = FString::Printf(
            TEXT("R28 landmark site %d nominal minimum-scale carrier coverage %.3f is below %.3f."),
            static_cast<int32>(Spec.Site),
            NominalR28CarrierCoverage(Spec),
            R28MinimumNominalCarrierCoverage);
        return false;
    }
    return true;
}

bool ValidateSiteLayoutR28(
    const FLandmarkSiteSpec& Spec,
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Layout,
    FString& OutError)
{
    const TArray<FTransform> GrassProfiles[] = {
        Layout.GrassManicured,
        Layout.GrassHumid,
        Layout.GrassShade,
        Layout.GrassDryEdge};
    if (!ValidateGrassTransformsR28(Spec, GrassProfiles, OutError))
    {
        return false;
    }

    const bool bMacDonald =
        Spec.Site ==
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse;
    return
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.TreesUmbrella,
            2,
            TEXT("R28 umbrella trees"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.TreesDome,
            1,
            TEXT("R28 dome trees"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.TreesHighFork,
            bMacDonald ? 1 : 0,
            TEXT("R28 high-fork trees"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.Shrubs,
            bMacDonald ? MacDonaldShrubCount : TemasekShrubCount,
            TEXT("R28 shrubs"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.Understorey,
            bMacDonald
                ? MacDonaldUnderstoreyCount
                : TemasekUnderstoreyCount,
            TEXT("R28 understorey"),
            OutError) &&
        ValidatePatchOwnedTransformArray(
            Spec,
            Layout.Flowers,
            bMacDonald ? MacDonaldFlowerCount : TemasekFlowerCount,
            TEXT("R28 flowers"),
            OutError);
}

bool ValidateTemasekTreeAnchorContractR28(
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Layout,
    FString& OutError)
{
    const FTransform ExpectedUmbrella[] = {
        MakeTemasekContractTreeWorldTransformR28(
            0, 8.0, FVector(2.05, 1.78, 1.72)),
        MakeTemasekContractTreeWorldTransformR28(
            2, 29.0, FVector(1.92, 1.72, 1.58))};
    const FTransform ExpectedDome =
        MakeTemasekContractTreeWorldTransformR28(
            1, -13.0, FVector(1.82, 2.00, 1.68));
    if (Layout.TreesUmbrella.Num() != 2 ||
        Layout.TreesDome.Num() != 1 ||
        !Layout.TreesHighFork.IsEmpty() ||
        !Layout.TreesUmbrella[0].Equals(ExpectedUmbrella[0], 0.001) ||
        !Layout.TreesUmbrella[1].Equals(ExpectedUmbrella[1], 0.001) ||
        !Layout.TreesDome[0].Equals(ExpectedDome, 0.001))
    {
        OutError = TEXT("Temasek R28 canopy lost an exact Phase-2 anchor, its mature anisotropic scale, or its umbrella/dome/umbrella roster.");
        return false;
    }
    return true;
}

bool IsExactPath(const UObject* Object, const FString& ExpectedPath)
{
    return IsValid(Object) && Object->GetPathName() == ExpectedPath;
}

void ConfigureRenderOnlyHism(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 CullStartDistanceCm,
    int32 CullEndDistanceCm,
    int32 WpoDisableDistanceCm,
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
    Component->SetCullDistances(CullStartDistanceCm, CullEndDistanceCm);
    Component->WorldPositionOffsetDisableDistance = WpoDisableDistanceCm;
    Component->bEnableDensityScaling = false;
    Component->bAutoRebuildTreeOnInstanceChanges = false;
    Component->ComponentTags.AddUnique(ActorTag);
}

void AppendTransforms(
    const TArray<FTransform>& First,
    const TArray<FTransform>& Second,
    TArray<FTransform>& OutCombined)
{
    OutCombined.Reset(First.Num() + Second.Num());
    OutCombined.Append(First);
    OutCombined.Append(Second);
}

bool PopulateComponent(
    UHierarchicalInstancedStaticMeshComponent* Component,
    UStaticMesh* Mesh,
    UMaterialInterface* Material,
    const TArray<FTransform>& First,
    const TArray<FTransform>& Second,
    const TCHAR* Label,
    FString& OutError)
{
    if (!Component || !Mesh)
    {
        OutError = FString::Printf(
            TEXT("Landmark %s component or mesh is missing."),
            Label);
        return false;
    }
    TArray<FTransform> Combined;
    AppendTransforms(First, Second, Combined);
    Component->ClearInstances();
    Component->SetStaticMesh(Mesh);
    Component->EmptyOverrideMaterials();
    if (Material)
    {
        Component->SetMaterial(0, Material);
    }
    Component->AddInstances(
        Combined,
        false,
        true,
        false);
    Component->BuildTreeIfOutdated(false, true);
    if (Component->GetInstanceCount() != Combined.Num() ||
        !Component->IsTreeFullyBuilt())
    {
        OutError = FString::Printf(
            TEXT("Landmark %s population did not synchronously build its exact %d world-space instances."),
            Label,
            Combined.Num());
        return false;
    }
    return true;
}

bool ComponentMatchesTransforms(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const TArray<FTransform>& First,
    const TArray<FTransform>& Second)
{
    if (!Component ||
        Component->GetInstanceCount() != First.Num() + Second.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
    {
        FTransform Actual;
        const FTransform& Expected = Index < First.Num()
            ? First[Index]
            : Second[Index - First.Num()];
        if (!Component->GetInstanceTransform(Index, Actual, true) ||
            !Actual.Equals(Expected, 0.0001))
        {
            return false;
        }
    }
    return true;
}

bool HasRenderOnlyPolicy(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    int32 ExpectedCullStart,
    int32 ExpectedCullEnd,
    int32 ExpectedWpoDisable)
{
    int32 ActualCullStart = 0;
    int32 ActualCullEnd = 0;
    if (Component)
    {
        Component->GetCullDistances(ActualCullStart, ActualCullEnd);
    }
    return Component &&
        Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore) &&
        !Component->GetGenerateOverlapEvents() &&
        !Component->CanEverAffectNavigation() &&
        !Component->bEnableDensityScaling &&
        !Component->bAutoRebuildTreeOnInstanceChanges &&
        ActualCullStart == ExpectedCullStart &&
        ActualCullEnd == ExpectedCullEnd &&
        Component->WorldPositionOffsetDisableDistance == ExpectedWpoDisable &&
        Component->ComponentTags.Contains(ActorTag);
}
} // namespace

ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ATRIADIstanaExploreV5DLandmarkVegetationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);
    Tags.AddUnique(ActorTag);
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("V5DLandmarkVegetationRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    GrassManicuredInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkGrassManicured"));
    GrassHumidInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkGrassHumid"));
    GrassShadeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkGrassShade"));
    GrassDryEdgeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkGrassDryEdge"));
    UmbrellaTreeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkTreesUmbrella"));
    DomeTreeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkTreesDome"));
    HighForkTreeInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkTreesHighFork"));
    ShrubInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkShrubs"));
    UnderstoreyInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkUnderstorey"));
    FlowerInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("V5DLandmarkFlowers"));

    UHierarchicalInstancedStaticMeshComponent* Components[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances,
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkTreeInstances,
        ShrubInstances,
        UnderstoreyInstances,
        FlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
    }

    UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
    {
        ConfigureRenderOnlyHism(
            Component,
            GrassCullStartDistanceCm,
            GrassCullEndDistanceCm,
            GrassWpoDisableDistanceCm,
            false);
        Component->InstanceLODDistanceScale = GrassLodDistanceScale;
    }

    UHierarchicalInstancedStaticMeshComponent* TreeComponents[] = {
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkTreeInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : TreeComponents)
    {
        ConfigureRenderOnlyHism(
            Component,
            TreeCullStartDistanceCm,
            TreeCullEndDistanceCm,
            GrassWpoDisableDistanceCm,
            true);
        Component->bOverrideMinLOD = true;
        Component->MinLOD = RuntimeTreeMinimumLod;
        Component->ForcedLodModel = RuntimeTreeForcedLodModel;
        Component->InstanceLODDistanceScale = RuntimeTreeLodDistanceScale;
    }

    UHierarchicalInstancedStaticMeshComponent* PlantingComponents[] = {
        ShrubInstances,
        UnderstoreyInstances,
        FlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component :
         PlantingComponents)
    {
        ConfigureRenderOnlyHism(
            Component,
            PlantingCullStartDistanceCm,
            PlantingCullEndDistanceCm,
            GrassWpoDisableDistanceCm,
            true);
    }
}

const FString& ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedClaimLabel()
{
    return LandmarkVegetationClaimLabel;
}

const FName& ATRIADIstanaExploreV5DLandmarkVegetationActor::ExpectedActorTag()
{
    return ActorTag;
}

const FString& ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedTemasekFoliageLayoutSchema()
{
    return TemasekFoliageLayoutSchema;
}

const FString& ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedTemasekFoliageRenderOwnerClass()
{
    return TemasekFoliageRenderOwnerClass;
}

const TArray<FVector>& ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedTemasekTreeAnchorsLocalMeters()
{
    return TemasekTreeAnchorsLocalMeters;
}

FTransform ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedTemasekSourcePlacementTransform()
{
    return FTransform(
        FRotator(0.0, -162.5152283523459, 0.0),
        FVector(40411.657951, 88424.311139, 0.0),
        FVector(1.053891, 1.221655, 1.0));
}

FVector ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedSiteAnchorCentimeters(
        ETRIADIstanaExploreV5DLandmarkVegetationSite Site)
{
    return GetSiteSpec(Site).AnchorCentimeters;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::ExpectedSiteYawDegrees(
    ETRIADIstanaExploreV5DLandmarkVegetationSite Site)
{
    return GetSiteSpec(Site).YawDegrees;
}

int32 ATRIADIstanaExploreV5DLandmarkVegetationActor::ExpectedGrassCount(
    ETRIADIstanaExploreV5DLandmarkVegetationSite Site)
{
    return GetSiteSpec(Site).GrassCount;
}

int32 ATRIADIstanaExploreV5DLandmarkVegetationActor::
    MaximumGrassInstancesPerSite()
{
    return MaximumGrassInstancesPerLandmark;
}

int32 ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassCullStartDistanceCm()
{
    return GrassCullStartDistanceCm;
}

int32 ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassCullEndDistanceCm()
{
    return GrassCullEndDistanceCm;
}

int32 ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassWpoDisableDistanceCm()
{
    return GrassWpoDisableDistanceCm;
}

float ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassLodDistanceScale()
{
    return GrassLodDistanceScale;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassCoverageScaleMinimum()
{
    return GrassCoverageScaleMinimum;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassCoverageScaleMaximum()
{
    return GrassCoverageScaleMaximum;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassHeightScaleMinimum()
{
    return GrassHeightScaleMinimum;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedGrassHeightScaleMaximum()
{
    return GrassHeightScaleMaximum;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedEvidenceViewDistanceCm()
{
    return EvidenceViewDistanceCm;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedMaterialVisibilityAtEvidenceView()
{
    const double Alpha = FMath::Clamp(
        (EvidenceViewDistanceCm - GrassCullStartDistanceCm) /
            static_cast<double>(
                GrassCullEndDistanceCm - GrassCullStartDistanceCm),
        0.0,
        1.0);
    const double ComponentVisibility = 1.0 - Alpha;
    const double CalibratedVisibility =
        1.0 - Alpha * Alpha * (3.0 - 2.0 * Alpha);
    return FMath::Max(ComponentVisibility, CalibratedVisibility);
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView()
{
    const double HorizontalHalfRadians = FMath::DegreesToRadians(
        EvidenceViewHorizontalFovDegrees * 0.5);
    const double VerticalHalfRadians = FMath::Atan(
        FMath::Tan(HorizontalHalfRadians) / EvidenceViewAspectRatio);
    const double VerticalFocalPixels = EvidenceViewHeightPixels /
        (2.0 * FMath::Tan(VerticalHalfRadians));
    return GrassCarrierMaximumSourceHeightCm * GrassHeightScaleMinimum /
        EvidenceViewDistanceCm * VerticalFocalPixels;
}

const FString& ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedR28ClaimLabel()
{
    return LandmarkVegetationR28ClaimLabel;
}

int32 ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedR28GrassCount(
        ETRIADIstanaExploreV5DLandmarkVegetationSite Site)
{
    return GetR28SiteSpec(Site).GrassCount;
}

int32 ATRIADIstanaExploreV5DLandmarkVegetationActor::
    MaximumR28GrassInstancesPerSite()
{
    return MaximumGrassInstancesPerLandmarkR28;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedR28HardscapeClearanceCm()
{
    return R28HardscapeExclusionClearanceCm;
}

double ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ExpectedR28NominalCarrierCoverageMinimum()
{
    return R28MinimumNominalCarrierCoverage;
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DLandmarkVegetationAssets& Assets,
    FString& OutError)
{
    if (!IsExactPath(Assets.GrassCarrierMesh, GrassCarrierMeshPath) ||
        Assets.GrassProfileMaterials.Num() != 4)
    {
        OutError = TEXT("R27 landmark vegetation requires the exact V5B grass carrier and four isolated R27 far-readable grass materials.");
        return false;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(GrassMaterialPaths); ++Index)
    {
        if (!IsExactPath(
                Assets.GrassProfileMaterials[Index],
                GrassMaterialPaths[Index]))
        {
            OutError = FString::Printf(
                TEXT("Landmark vegetation grass material %d is not the exact isolated R27 profile."),
                Index);
            return false;
        }
    }
    const FBoxSphereBounds GrassCarrierBounds =
        Assets.GrassCarrierMesh->GetBounds();
    const FVector GrassCarrierMinimum =
        GrassCarrierBounds.Origin - GrassCarrierBounds.BoxExtent;
    const FVector GrassCarrierMaximum =
        GrassCarrierBounds.Origin + GrassCarrierBounds.BoxExtent;
    if (!FMath::IsWithinInclusive(
            GrassCarrierMinimum.X, -78.05, -77.72) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMinimum.Y, -68.62, -68.29) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMinimum.Z, -0.05, 0.05) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMaximum.X, 79.02, 79.35) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMaximum.Y, 68.22, 68.55) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMaximum.Z, 4.33, 4.47))
    {
        OutError = TEXT("The R27 projection assumption requires the exact R11 Bermuda carrier centimetre bounds, including its 4.4 cm tallest source tip.");
        return false;
    }
    if (!IsExactPath(Assets.UmbrellaTreeMesh, UmbrellaTreeMeshPath) ||
        !IsExactPath(Assets.DomeTreeMesh, DomeTreeMeshPath) ||
        !IsExactPath(Assets.HighForkTreeMesh, HighForkTreeMeshPath) ||
        !IsExactPath(Assets.ShrubMesh, ShrubMeshPath) ||
        !IsExactPath(Assets.ShrubMaterial, ShrubMaterialPath) ||
        !IsExactPath(Assets.UnderstoreyMesh, UnderstoreyMeshPath) ||
        !IsExactPath(Assets.UnderstoreyMaterial, UnderstoreyMaterialPath) ||
        !IsExactPath(Assets.FlowerMesh, FlowerMeshPath) ||
        !IsExactPath(Assets.FlowerMaterial, FlowerMaterialPath))
    {
        OutError = TEXT("Landmark vegetation requires the exact reusable V4 planting and V5D non-palm tree roster.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::ValidateAssetRosterR28(
    const FTRIADIstanaExploreV5DLandmarkVegetationAssets& Assets,
    FString& OutError)
{
    if (!IsExactPath(Assets.GrassCarrierMesh, GrassCarrierMeshPath) ||
        Assets.GrassProfileMaterials.Num() != 4)
    {
        OutError = TEXT("R28 landmark vegetation requires the exact V5B grass carrier and four isolated R28 healthy-tropical grass materials.");
        return false;
    }
    for (int32 Index = 0;
         Index < UE_ARRAY_COUNT(GrassMaterialPathsR28);
         ++Index)
    {
        if (!IsExactPath(
                Assets.GrassProfileMaterials[Index],
                GrassMaterialPathsR28[Index]))
        {
            OutError = FString::Printf(
                TEXT("Landmark vegetation grass material %d is not the exact isolated R28 profile."),
                Index);
            return false;
        }
    }
    const FBoxSphereBounds GrassCarrierBounds =
        Assets.GrassCarrierMesh->GetBounds();
    const FVector GrassCarrierMinimum =
        GrassCarrierBounds.Origin - GrassCarrierBounds.BoxExtent;
    const FVector GrassCarrierMaximum =
        GrassCarrierBounds.Origin + GrassCarrierBounds.BoxExtent;
    if (!FMath::IsWithinInclusive(
            GrassCarrierMinimum.X, -78.05, -77.72) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMinimum.Y, -68.62, -68.29) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMinimum.Z, -0.05, 0.05) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMaximum.X, 79.02, 79.35) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMaximum.Y, 68.22, 68.55) ||
        !FMath::IsWithinInclusive(
            GrassCarrierMaximum.Z, 4.33, 4.47))
    {
        OutError = TEXT("R28 density coverage requires the exact R11 Bermuda carrier centimetre bounds; substituting a larger mesh would invalidate the coverage calculation.");
        return false;
    }
    if (!IsExactPath(Assets.UmbrellaTreeMesh, UmbrellaTreeMeshPath) ||
        !IsExactPath(Assets.DomeTreeMesh, DomeTreeMeshPath) ||
        !IsExactPath(Assets.HighForkTreeMesh, HighForkTreeMeshPath) ||
        !IsExactPath(Assets.ShrubMesh, ShrubMeshPath) ||
        !IsExactPath(Assets.ShrubMaterial, ShrubMaterialPath) ||
        !IsExactPath(Assets.UnderstoreyMesh, UnderstoreyMeshPath) ||
        !IsExactPath(Assets.UnderstoreyMaterial, UnderstoreyMaterialPath) ||
        !IsExactPath(Assets.FlowerMesh, FlowerMeshPath) ||
        !IsExactPath(Assets.FlowerMaterial, FlowerMaterialPath))
    {
        OutError = TEXT("R28 landmark vegetation requires the exact reusable V4 planting and V5D non-palm tree roster.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::BuildDeterministicLayout(
    FTRIADIstanaExploreV5DLandmarkVegetationLayout& OutLayout,
    FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
    const FLandmarkSiteSpec MacDonald = GetSiteSpec(
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse);
    const FLandmarkSiteSpec Temasek = GetSiteSpec(
        ETRIADIstanaExploreV5DLandmarkVegetationSite::TemasekShophouse);
    if (!BuildGrassLayout(MacDonald, OutLayout.MacDonaldHouse, OutError) ||
        !BuildGrassLayout(Temasek, OutLayout.TemasekShophouse, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
        return false;
    }
    BuildTrees(MacDonald, OutLayout.MacDonaldHouse);
    BuildTrees(Temasek, OutLayout.TemasekShophouse);
    if (!BuildPlanting(MacDonald, OutLayout.MacDonaldHouse, OutError) ||
        !BuildPlanting(Temasek, OutLayout.TemasekShophouse, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
        return false;
    }
    if (!ValidateLayout(OutLayout, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::ValidateLayout(
    const FTRIADIstanaExploreV5DLandmarkVegetationLayout& Layout,
    FString& OutError)
{
    if (ExpectedMaterialVisibilityAtEvidenceView() < 0.75 ||
        ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView() <
            MinimumAcceptedTallestCarrierTipProjectionPixels)
    {
        OutError = TEXT("R27 landmark grass lost its bounded 72.8 m material-visibility or tallest-carrier-tip projection assumption.");
        return false;
    }
    return ValidateSiteLayout(
               GetSiteSpec(
                   ETRIADIstanaExploreV5DLandmarkVegetationSite::
                       MacDonaldHouse),
               Layout.MacDonaldHouse,
               OutError) &&
        ValidateSiteLayout(
               GetSiteSpec(
                   ETRIADIstanaExploreV5DLandmarkVegetationSite::
                       TemasekShophouse),
               Layout.TemasekShophouse,
               OutError) &&
        ValidateTemasekTreeAnchorContract(
               Layout.TemasekShophouse,
               OutError);
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::
    BuildDeterministicLayoutR28(
        FTRIADIstanaExploreV5DLandmarkVegetationLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
    const FLandmarkSiteSpec MacDonald = GetR28SiteSpec(
        ETRIADIstanaExploreV5DLandmarkVegetationSite::MacDonaldHouse);
    const FLandmarkSiteSpec Temasek = GetR28SiteSpec(
        ETRIADIstanaExploreV5DLandmarkVegetationSite::TemasekShophouse);
    if (!BuildGrassLayoutR28(
            MacDonald,
            OutLayout.MacDonaldHouse,
            OutError) ||
        !BuildGrassLayoutR28(
            Temasek,
            OutLayout.TemasekShophouse,
            OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
        return false;
    }
    BuildTreesR28(MacDonald, OutLayout.MacDonaldHouse);
    BuildTreesR28(Temasek, OutLayout.TemasekShophouse);
    if (!BuildPlanting(MacDonald, OutLayout.MacDonaldHouse, OutError) ||
        !BuildPlanting(Temasek, OutLayout.TemasekShophouse, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
        return false;
    }
    if (!ValidateLayoutR28(OutLayout, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DLandmarkVegetationLayout();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::ValidateLayoutR28(
    const FTRIADIstanaExploreV5DLandmarkVegetationLayout& Layout,
    FString& OutError)
{
    if (ExpectedMaterialVisibilityAtEvidenceView() < 0.75 ||
        ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView() <
            MinimumAcceptedTallestCarrierTipProjectionPixels)
    {
        OutError = TEXT("R28 landmark grass lost the inherited bounded 72.8 m material-visibility or tallest-carrier-tip projection assumption.");
        return false;
    }
    return ValidateSiteLayoutR28(
               GetR28SiteSpec(
                   ETRIADIstanaExploreV5DLandmarkVegetationSite::
                       MacDonaldHouse),
               Layout.MacDonaldHouse,
               OutError) &&
        ValidateSiteLayoutR28(
               GetR28SiteSpec(
                   ETRIADIstanaExploreV5DLandmarkVegetationSite::
                       TemasekShophouse),
               Layout.TemasekShophouse,
               OutError) &&
        ValidateTemasekTreeAnchorContractR28(
               Layout.TemasekShophouse,
               OutError);
}

void ATRIADIstanaExploreV5DLandmarkVegetationActor::ClearOwnedInstances()
{
    UHierarchicalInstancedStaticMeshComponent* Components[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances,
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkTreeInstances,
        ShrubInstances,
        UnderstoreyInstances,
        FlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::PopulateSavedLayout(
    FString& OutError)
{
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& MacDonald =
        SavedLayout.MacDonaldHouse;
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Temasek =
        SavedLayout.TemasekShophouse;
    return
        PopulateComponent(
            GrassManicuredInstances,
            SavedAssets.GrassCarrierMesh,
            SavedAssets.GrassProfileMaterials[0],
            MacDonald.GrassManicured,
            Temasek.GrassManicured,
            TEXT("manicured grass"),
            OutError) &&
        PopulateComponent(
            GrassHumidInstances,
            SavedAssets.GrassCarrierMesh,
            SavedAssets.GrassProfileMaterials[1],
            MacDonald.GrassHumid,
            Temasek.GrassHumid,
            TEXT("humid grass"),
            OutError) &&
        PopulateComponent(
            GrassShadeInstances,
            SavedAssets.GrassCarrierMesh,
            SavedAssets.GrassProfileMaterials[2],
            MacDonald.GrassShade,
            Temasek.GrassShade,
            TEXT("shade grass"),
            OutError) &&
        PopulateComponent(
            GrassDryEdgeInstances,
            SavedAssets.GrassCarrierMesh,
            SavedAssets.GrassProfileMaterials[3],
            MacDonald.GrassDryEdge,
            Temasek.GrassDryEdge,
            TEXT("dry-edge grass"),
            OutError) &&
        PopulateComponent(
            UmbrellaTreeInstances,
            SavedAssets.UmbrellaTreeMesh,
            nullptr,
            MacDonald.TreesUmbrella,
            Temasek.TreesUmbrella,
            TEXT("umbrella trees"),
            OutError) &&
        PopulateComponent(
            DomeTreeInstances,
            SavedAssets.DomeTreeMesh,
            nullptr,
            MacDonald.TreesDome,
            Temasek.TreesDome,
            TEXT("dome trees"),
            OutError) &&
        PopulateComponent(
            HighForkTreeInstances,
            SavedAssets.HighForkTreeMesh,
            nullptr,
            MacDonald.TreesHighFork,
            Temasek.TreesHighFork,
            TEXT("high-fork trees"),
            OutError) &&
        PopulateComponent(
            ShrubInstances,
            SavedAssets.ShrubMesh,
            SavedAssets.ShrubMaterial,
            MacDonald.Shrubs,
            Temasek.Shrubs,
            TEXT("shrubs"),
            OutError) &&
        PopulateComponent(
            UnderstoreyInstances,
            SavedAssets.UnderstoreyMesh,
            SavedAssets.UnderstoreyMaterial,
            MacDonald.Understorey,
            Temasek.Understorey,
            TEXT("understorey"),
            OutError) &&
        PopulateComponent(
            FlowerInstances,
            SavedAssets.FlowerMesh,
            SavedAssets.FlowerMaterial,
            MacDonald.Flowers,
            Temasek.Flowers,
            TEXT("flowers"),
            OutError);
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ConfigureLandmarkVegetation(
        const FTRIADIstanaExploreV5DLandmarkVegetationAssets& InAssets,
        FString& OutError)
{
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0001))
    {
        OutError = TEXT("Landmark vegetation actor must remain at the identity transform because every saved instance is world-space.");
        return false;
    }
    if (!ValidateAssetRoster(InAssets, OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationLayout Layout;
    if (!BuildDeterministicLayout(Layout, OutError))
    {
        return false;
    }

    ClearOwnedInstances();
    SavedAssets = InAssets;
    SavedLayout = MoveTemp(Layout);
    ClaimLabel = ExpectedClaimLabel();
    bAppearanceOnly = true;
    bSourceAssetPackagesModified = false;
    bCollisionOrNavigationAuthority = false;
    bSensorOrRfAuthority = false;
    bBotanicalSurveyOrCurrentSeasonClaimed = false;
    SetActorEnableCollision(false);
    Tags.AddUnique(ExpectedActorTag());

    if (!PopulateSavedLayout(OutError))
    {
        ClearOwnedInstances();
        return false;
    }
    FString ValidationReport;
    if (!ValidateLandmarkVegetation(ValidationReport))
    {
        ClearOwnedInstances();
        OutError = TEXT("Landmark vegetation failed closed post-population validation: ") +
            ValidationReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ConfigureLandmarkVegetationR28(
        const FTRIADIstanaExploreV5DLandmarkVegetationAssets& InAssets,
        FString& OutError)
{
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0001))
    {
        OutError = TEXT("R28 landmark vegetation actor must remain at the identity transform because every saved instance is world-space.");
        return false;
    }
    if (!ValidateAssetRosterR28(InAssets, OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5DLandmarkVegetationLayout Layout;
    if (!BuildDeterministicLayoutR28(Layout, OutError))
    {
        return false;
    }

    ClearOwnedInstances();
    SavedAssets = InAssets;
    SavedLayout = MoveTemp(Layout);
    ClaimLabel = ExpectedR28ClaimLabel();
    bAppearanceOnly = true;
    bSourceAssetPackagesModified = false;
    bCollisionOrNavigationAuthority = false;
    bSensorOrRfAuthority = false;
    bBotanicalSurveyOrCurrentSeasonClaimed = false;
    SetActorEnableCollision(false);
    Tags.AddUnique(ExpectedActorTag());

    if (!PopulateSavedLayout(OutError))
    {
        ClearOwnedInstances();
        return false;
    }
    FString ValidationReport;
    if (!ValidateLandmarkVegetationR28(ValidationReport))
    {
        ClearOwnedInstances();
        OutError = TEXT("R28 landmark vegetation failed closed post-population validation: ") +
            ValidationReport;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DLandmarkVegetationActor::
    ValidateLandmarkVegetation(FString& OutReport) const
{
    FString Error;
    if (!ValidateAssetRoster(SavedAssets, Error) ||
        !ValidateLayout(SavedLayout, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_INVALID_LAYOUT_OR_ASSETS: ") +
            Error;
        return false;
    }
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0001) ||
        GetActorEnableCollision() ||
        !Tags.Contains(ExpectedActorTag()) ||
        ClaimLabel != ExpectedClaimLabel() ||
        !bAppearanceOnly || bSourceAssetPackagesModified ||
        bCollisionOrNavigationAuthority || bSensorOrRfAuthority ||
        bBotanicalSurveyOrCurrentSeasonClaimed)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_INVALID_TRUTH_BOUNDARY");
        return false;
    }

    const UHierarchicalInstancedStaticMeshComponent* GrassComponents[] = {
        GrassManicuredInstances,
        GrassHumidInstances,
        GrassShadeInstances,
        GrassDryEdgeInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         GrassComponents)
    {
        if (!HasRenderOnlyPolicy(
                Component,
                GrassCullStartDistanceCm,
                GrassCullEndDistanceCm,
                GrassWpoDisableDistanceCm) ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                GrassLodDistanceScale,
                0.0001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_INVALID_GRASS_RENDER_POLICY");
            return false;
        }
    }

    const UHierarchicalInstancedStaticMeshComponent* TreeComponents[] = {
        UmbrellaTreeInstances,
        DomeTreeInstances,
        HighForkTreeInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         TreeComponents)
    {
        if (!HasRenderOnlyPolicy(
                Component,
                TreeCullStartDistanceCm,
                TreeCullEndDistanceCm,
                GrassWpoDisableDistanceCm) ||
            !Component->bOverrideMinLOD ||
            Component->MinLOD != RuntimeTreeMinimumLod ||
            Component->ForcedLodModel != RuntimeTreeForcedLodModel ||
            !FMath::IsNearlyEqual(
                Component->InstanceLODDistanceScale,
                RuntimeTreeLodDistanceScale,
                0.0001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_INVALID_TREE_RENDER_POLICY");
            return false;
        }
    }

    const UHierarchicalInstancedStaticMeshComponent* PlantingComponents[] = {
        ShrubInstances,
        UnderstoreyInstances,
        FlowerInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         PlantingComponents)
    {
        if (!HasRenderOnlyPolicy(
                Component,
                PlantingCullStartDistanceCm,
                PlantingCullEndDistanceCm,
                GrassWpoDisableDistanceCm))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_INVALID_PLANTING_RENDER_POLICY");
            return false;
        }
    }

    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& MacDonald =
        SavedLayout.MacDonaldHouse;
    const FTRIADIstanaExploreV5DLandmarkVegetationSiteLayout& Temasek =
        SavedLayout.TemasekShophouse;
    const bool bTransformsMatch =
        ComponentMatchesTransforms(
            GrassManicuredInstances,
            MacDonald.GrassManicured,
            Temasek.GrassManicured) &&
        ComponentMatchesTransforms(
            GrassHumidInstances,
            MacDonald.GrassHumid,
            Temasek.GrassHumid) &&
        ComponentMatchesTransforms(
            GrassShadeInstances,
            MacDonald.GrassShade,
            Temasek.GrassShade) &&
        ComponentMatchesTransforms(
            GrassDryEdgeInstances,
            MacDonald.GrassDryEdge,
            Temasek.GrassDryEdge) &&
        ComponentMatchesTransforms(
            UmbrellaTreeInstances,
            MacDonald.TreesUmbrella,
            Temasek.TreesUmbrella) &&
        ComponentMatchesTransforms(
            DomeTreeInstances,
            MacDonald.TreesDome,
            Temasek.TreesDome) &&
        ComponentMatchesTransforms(
            HighForkTreeInstances,
            MacDonald.TreesHighFork,
            Temasek.TreesHighFork) &&
        ComponentMatchesTransforms(
            ShrubInstances,
            MacDonald.Shrubs,
            Temasek.Shrubs) &&
        ComponentMatchesTransforms(
            UnderstoreyInstances,
            MacDonald.Understorey,
            Temasek.Understorey) &&
        ComponentMatchesTransforms(
            FlowerInstances,
            MacDonald.Flowers,
            Temasek.Flowers);
    if (!bTransformsMatch)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_INVALID_WORLD_TRANSFORMS");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_VALID actorContractVersion=2 actorMapIntegrationAuthority=false worldSpace=true deterministic=true nearestLandmarkPartition=true r26VisualAcceptanceFailed=true r27FarCameraCorrectionConfigured=true evidenceRangeMeters=72.8 materialVisibilityAtEvidenceRange=%.3f visibilityGateCount=1 componentFadeIntegrated=true tallestCarrierTipProjectionPixelsAtEvidenceRange=%.3f projectionAssumption=perpendicularPinholeMaxSourceTip grassScaleXY=%.2f,%.2f grassScaleZ=%.2f,%.2f visualCaptureAccepted=false captureRevalidationRequired=true macDonaldGrass=%d temasekGrass=%d maximumGrassPerSite=%d grassCullCm=%d,%d wpoDisableCm=%d treeAutomaticLod=%d,%d,%.1f renderOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false sourceAssetsModified=false surveyClaim=false botanicalClaim=false seasonalClaim=false"),
        ExpectedMaterialVisibilityAtEvidenceView(),
        ExpectedTallestCarrierTipProjectionPixelsAtEvidenceView(),
        GrassCoverageScaleMinimum,
        GrassCoverageScaleMaximum,
        GrassHeightScaleMinimum,
        GrassHeightScaleMaximum,
        MacDonald.GrassTotal(),
        Temasek.GrassTotal(),
        MaximumGrassInstancesPerLandmark,
        GrassCullStartDistanceCm,
        GrassCullEndDistanceCm,
        GrassWpoDisableDistanceCm,
        RuntimeTreeMinimumLod,
        RuntimeTreeForcedLodModel,
        RuntimeTreeLodDistanceScale);
    return true;
}
