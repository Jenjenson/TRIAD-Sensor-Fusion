#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"

namespace
{
constexpr int32 SourceInstanceCount = 18432;
constexpr int32 SelectionDenominator = 4;
constexpr int32 ProfileCount = 4;
constexpr int32 MeshVariantCount = 3;
constexpr int32 BucketCount = ProfileCount * MeshVariantCount;
constexpr int32 SelectedInstanceCount =
    SourceInstanceCount / SelectionDenominator;
constexpr int32 CullStartDistanceCm = 6500;
constexpr int32 CullEndDistanceCm = 9000;
constexpr int32 WpoDisableDistanceCm = 6000;
constexpr double MinimumTipHeightCm = 5.0;
constexpr double MaximumTipHeightCm = 9.0;
constexpr double MinimumXyScaleMultiplier = 0.88;
constexpr double MaximumXyScaleMultiplier = 1.12;
constexpr double MaximumAddedYawDegrees = 180.0;
constexpr double MeshBoundsToleranceCm = 0.01;
constexpr float LodDistanceScale = 1.0f;
constexpr double TransformToleranceCm = 0.001;

// Changing any salt intentionally changes the versioned visual distribution.
constexpr int32 DistributionPolicyVersion = 2;
constexpr uint32 SelectionHashSalt = 0xA511E9B3u;
constexpr uint32 VariantHashSalt = 0x63D83595u;
constexpr uint32 YawHashSalt = 0x9E3779B9u;
constexpr uint32 XScaleHashSalt = 0xC2B2AE35u;
constexpr uint32 YScaleHashSalt = 0x27D4EB2Fu;
constexpr uint32 TipHeightHashSalt = 0x85EBCA6Bu;

constexpr int32 SourceProfileCounts[] = {12460, 3976, 1152, 844};
constexpr int32 ProfileQuotas[] = {3115, 994, 288, 211};
constexpr int32 VariantInstanceCounts[] = {2310, 1380, 918};
constexpr int32 ProfileVariantQuotas[ProfileCount][MeshVariantCount] = {
    {1560, 933, 622},
    {499, 297, 198},
    {145, 87, 56},
    {106, 63, 42}};
constexpr int32 MeshTriangleCounts[] = {1024, 896, 1152};
constexpr double MeshMaximumHeightCm[] = {13.844719, 18.459288, 19.605375};
constexpr int32 WorstCaseVisibleTriangleCount =
    VariantInstanceCounts[0] * MeshTriangleCounts[0] +
    VariantInstanceCounts[1] * MeshTriangleCounts[1] +
    VariantInstanceCounts[2] * MeshTriangleCounts[2];

static_assert(SelectedInstanceCount == 4608);
static_assert(BucketCount == 12);
static_assert(UE_ARRAY_COUNT(SourceProfileCounts) == ProfileCount);
static_assert(UE_ARRAY_COUNT(ProfileQuotas) == ProfileCount);
static_assert(UE_ARRAY_COUNT(VariantInstanceCounts) == MeshVariantCount);
static_assert(UE_ARRAY_COUNT(MeshMaximumHeightCm) == MeshVariantCount);
static_assert(
    ProfileQuotas[0] + ProfileQuotas[1] + ProfileQuotas[2] +
            ProfileQuotas[3] ==
        SelectedInstanceCount);
static_assert(
    VariantInstanceCounts[0] + VariantInstanceCounts[1] +
            VariantInstanceCounts[2] ==
        SelectedInstanceCount);
static_assert(1560 + 933 + 622 == ProfileQuotas[0]);
static_assert(499 + 297 + 198 == ProfileQuotas[1]);
static_assert(145 + 87 + 56 == ProfileQuotas[2]);
static_assert(106 + 63 + 42 == ProfileQuotas[3]);
static_assert(1560 + 499 + 145 + 106 == VariantInstanceCounts[0]);
static_assert(933 + 297 + 87 + 63 == VariantInstanceCounts[1]);
static_assert(622 + 198 + 56 + 42 == VariantInstanceCounts[2]);
static_assert(WorstCaseVisibleTriangleCount == 4659456);

const FString R32ClaimLabel(
    TEXT("VISUAL_ASSUMPTION_BOUND_R32_MEDIUM_DISTANCE_MODELED_TURF_BRIDGE"));
const FName ActorTag(
    TEXT("TRIAD_IstanaExploreV5D_R32MediumDistanceTurf_RenderOnly"));
const FName ComponentTag(
    TEXT("TRIAD_IstanaExploreV5D_R32MediumDistanceTurf_VisualCameraPresentationOnly"));

const FString GrassMeshPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/SM_IPV5D_R29_GrassFineCluster_Render.SM_IPV5D_R29_GrassFineCluster_Render"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/SM_IPV5D_R29_GrassBroadCluster_Render.SM_IPV5D_R29_GrassBroadCluster_Render"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/SM_IPV5D_R29_GrassMixedCluster_Render.SM_IPV5D_R29_GrassMixedCluster_Render")};
const FString GrassMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/MediumDistanceTurfR32/Materials/M_IPV5D_R32_Turf_Manicured.M_IPV5D_R32_Turf_Manicured"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/MediumDistanceTurfR32/Materials/M_IPV5D_R32_Turf_Humid.M_IPV5D_R32_Turf_Humid"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/MediumDistanceTurfR32/Materials/M_IPV5D_R32_Turf_Shade.M_IPV5D_R32_Turf_Shade"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/MediumDistanceTurfR32/Materials/M_IPV5D_R32_Turf_DryEdge.M_IPV5D_R32_Turf_DryEdge")};

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

uint32 HashTransform(
    const FTransform& Transform,
    int32 SourceIndex,
    int32 Profile)
{
    const FVector Location = Transform.GetTranslation();
    uint32 Value = MixBits(
        static_cast<uint32>(FMath::RoundToInt(Location.X * 10.0)) ^
        0x52333254u);
    Value = MixBits(
        Value ^ static_cast<uint32>(FMath::RoundToInt(Location.Y * 10.0)));
    Value = MixBits(
        Value ^ static_cast<uint32>(FMath::RoundToInt(Location.Z * 10.0)));
    Value = MixBits(Value ^ static_cast<uint32>(SourceIndex));
    return MixBits(Value ^ static_cast<uint32>(Profile * 0x45D9F3Bu));
}

uint32 HashForPurpose(
    const FTransform& Transform,
    int32 SourceIndex,
    int32 Profile,
    uint32 Salt)
{
    return MixBits(HashTransform(Transform, SourceIndex, Profile) ^ Salt);
}

bool IsExactPath(const UObject* Object, const FString& ExpectedPath)
{
    return IsValid(Object) && Object->GetPathName() == ExpectedPath;
}

double TargetTipHeightCm(
    const FTransform& Source,
    int32 SourceIndex,
    int32 Profile)
{
    const uint32 Hash = HashForPurpose(
        Source,
        SourceIndex,
        Profile,
        TipHeightHashSalt);
    switch (Profile)
    {
    case 0: // manicured
        return FMath::Lerp(5.0, 6.5, HashUnit(Hash ^ 0xA3C59AC3u));
    case 1: // humid
        return FMath::Lerp(6.2, 8.2, HashUnit(Hash ^ 0x3C6EF372u));
    case 2: // shade
        return FMath::Lerp(6.5, 9.0, HashUnit(Hash ^ 0x9E3779B9u));
    default: // dry edge
        return FMath::Lerp(5.0, 7.0, HashUnit(Hash ^ 0x85EBCA6Bu));
    }
}

FTransform MakePresentationTransform(
    const FTransform& Source,
    int32 SourceIndex,
    int32 Profile,
    int32 Variant)
{
    FVector Scale = Source.GetScale3D();
    Scale.X *= FMath::Lerp(
        MinimumXyScaleMultiplier,
        MaximumXyScaleMultiplier,
        HashUnit(HashForPurpose(
            Source, SourceIndex, Profile, XScaleHashSalt)));
    Scale.Y *= FMath::Lerp(
        MinimumXyScaleMultiplier,
        MaximumXyScaleMultiplier,
        HashUnit(HashForPurpose(
            Source, SourceIndex, Profile, YScaleHashSalt)));
    Scale.Z = TargetTipHeightCm(Source, SourceIndex, Profile) /
        MeshMaximumHeightCm[Variant];

    const double AddedYawDegrees = FMath::Lerp(
        -MaximumAddedYawDegrees,
        MaximumAddedYawDegrees,
        HashUnit(HashForPurpose(
            Source, SourceIndex, Profile, YawHashSalt)));
    FQuat PresentationRotation =
        FQuat(
            FVector::UpVector,
            FMath::DegreesToRadians(AddedYawDegrees)) *
        Source.GetRotation();
    PresentationRotation.Normalize();
    return FTransform(
        PresentationRotation,
        Source.GetTranslation(),
        Scale);
}

void ConfigureRenderOnly(
    UHierarchicalInstancedStaticMeshComponent* Component)
{
    if (!Component)
    {
        return;
    }
    Component->SetRelativeTransform(FTransform::Identity);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    Component->SetCastContactShadow(false);
    Component->SetAffectDistanceFieldLighting(false);
    Component->SetRenderInMainPass(true);
    Component->SetRenderCustomDepth(false);
    Component->bHiddenInSceneCapture = false;
    Component->SetVisibility(true, true);
    Component->SetHiddenInGame(false, true);
    Component->SetOverlayMaterial(nullptr);
    Component->SetCullDistances(CullStartDistanceCm, CullEndDistanceCm);
    Component->WorldPositionOffsetDisableDistance = WpoDisableDistanceCm;
    Component->InstanceLODDistanceScale = LodDistanceScale;
    Component->bEnableDensityScaling = false;
    Component->bAutoRebuildTreeOnInstanceChanges = false;
    Component->ComponentTags.AddUnique(ComponentTag);
}

bool HasRenderOnlyPolicy(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const USceneComponent* Parent)
{
    int32 ActualCullStart = 0;
    int32 ActualCullEnd = 0;
    if (Component)
    {
        Component->GetCullDistances(ActualCullStart, ActualCullEnd);
    }
    return Component && Component->GetAttachParent() == Parent &&
        Component->GetRelativeTransform().Equals(
            FTransform::Identity, TransformToleranceCm) &&
        Component->Mobility == EComponentMobility::Static &&
        Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore) &&
        !Component->GetGenerateOverlapEvents() &&
        !Component->CanEverAffectNavigation() && !Component->CastShadow &&
        !Component->bCastContactShadow &&
        !Component->bAffectDistanceFieldLighting &&
        Component->bRenderInMainPass && !Component->bRenderCustomDepth &&
        !Component->bHiddenInSceneCapture && !Component->GetOverlayMaterial() &&
        Component->IsVisible() && !Component->bHiddenInGame &&
        ActualCullStart == CullStartDistanceCm &&
        ActualCullEnd == CullEndDistanceCm &&
        Component->WorldPositionOffsetDisableDistance ==
            WpoDisableDistanceCm &&
        Component->InstanceLODDistanceScale == LodDistanceScale &&
        !Component->bEnableDensityScaling &&
        !Component->bAutoRebuildTreeOnInstanceChanges &&
        Component->ComponentTags.Num() == 1 &&
        Component->ComponentTags.Contains(ComponentTag);
}

bool TransformsEqual(
    const TArray<FTransform>& A,
    const TArray<FTransform>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (!A[Index].Equals(B[Index], TransformToleranceCm))
        {
            return false;
        }
    }
    return true;
}

bool LayoutsEqual(
    const FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout& A,
    const FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout& B)
{
    if (A.GrassBuckets.Num() != B.GrassBuckets.Num())
    {
        return false;
    }
    for (int32 Bucket = 0; Bucket < A.GrassBuckets.Num(); ++Bucket)
    {
        if (!TransformsEqual(
                A.GrassBuckets[Bucket].WorldTransforms,
                B.GrassBuckets[Bucket].WorldTransforms))
        {
            return false;
        }
    }
    return true;
}

bool ValidateLayoutStructure(
    const FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout& Layout,
    FString& OutError)
{
    if (Layout.GrassBuckets.Num() != BucketCount ||
        Layout.Total() != SelectedInstanceCount)
    {
        OutError = TEXT("R32 turf lost its exact twelve-bucket or 4,608-instance census.");
        return false;
    }
    int32 VariantTotals[MeshVariantCount] = {};
    for (int32 Profile = 0; Profile < ProfileCount; ++Profile)
    {
        int32 ProfileTotal = 0;
        for (int32 Variant = 0; Variant < MeshVariantCount; ++Variant)
        {
            const TArray<FTransform>& Transforms =
                Layout.GrassBuckets[Profile * MeshVariantCount + Variant]
                    .WorldTransforms;
            if (Transforms.IsEmpty())
            {
                OutError = TEXT("R32 turf must retain all twelve profile/silhouette buckets.");
                return false;
            }
            ProfileTotal += Transforms.Num();
            VariantTotals[Variant] += Transforms.Num();
            for (const FTransform& Transform : Transforms)
            {
                const double TipHeightCm =
                    FMath::Abs(Transform.GetScale3D().Z) *
                    MeshMaximumHeightCm[Variant];
                if (Transform.ContainsNaN() ||
                    TipHeightCm < MinimumTipHeightCm - 0.0001 ||
                    TipHeightCm > MaximumTipHeightCm + 0.0001)
                {
                    OutError = TEXT("R32 turf contains a non-finite transform or a local tip height outside 5--9 cm.");
                    return false;
                }
            }
        }
        if (ProfileTotal != ProfileQuotas[Profile])
        {
            OutError = TEXT("R32 turf lost an exact per-profile quota.");
            return false;
        }
    }
    for (int32 Variant = 0; Variant < MeshVariantCount; ++Variant)
    {
        if (VariantTotals[Variant] != VariantInstanceCounts[Variant])
        {
            OutError = TEXT("R32 turf lost its deterministic 50/30/20 realized variant census.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ComponentMatches(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    const AActor* ExpectedOwner,
    const USceneComponent* Parent,
    const UStaticMesh* Mesh,
    const UMaterialInterface* Material,
    const TArray<FTransform>& Expected)
{
    if (!HasRenderOnlyPolicy(Component, Parent) ||
        Component->GetOwner() != ExpectedOwner ||
        Component->GetStaticMesh() != Mesh ||
        Component->GetMaterial(0) != Material ||
        Component->GetNumOverrideMaterials() != 1 ||
        Component->GetInstanceCount() != Expected.Num() ||
        !Component->IsTreeFullyBuilt())
    {
        return false;
    }
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        FTransform Actual = FTransform::Identity;
        if (!Component->GetInstanceTransform(Index, Actual, true) ||
            !Actual.Equals(Expected[Index], TransformToleranceCm))
        {
            return false;
        }
    }
    return true;
}

bool PopulateComponent(
    UHierarchicalInstancedStaticMeshComponent* Component,
    const AActor* ExpectedOwner,
    const USceneComponent* ExpectedParent,
    UStaticMesh* Mesh,
    UMaterialInterface* Material,
    const TArray<FTransform>& Transforms,
    FString& OutError)
{
    if (!Component || !Mesh || !Material || Transforms.IsEmpty())
    {
        OutError = TEXT("R32 turf component, mesh, material, or transform bucket is missing.");
        return false;
    }
    Component->ClearInstances();
    Component->SetStaticMesh(Mesh);
    Component->EmptyOverrideMaterials();
    Component->SetMaterial(0, Material);
    Component->AddInstances(Transforms, false, true, false);
    Component->BuildTreeIfOutdated(false, true);
    if (!ComponentMatches(
            Component,
            ExpectedOwner,
            ExpectedParent,
            Mesh,
            Material,
            Transforms))
    {
        OutError = TEXT("R32 turf HISM population failed exact synchronous validation.");
        return false;
    }
    return true;
}
} // namespace

int32 FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout::Total() const
{
    int32 TotalInstances = 0;
    for (const FTRIADIstanaExploreV5DR32MediumDistanceTurfBucket& Bucket :
         GrassBuckets)
    {
        TotalInstances += Bucket.WorldTransforms.Num();
    }
    return TotalInstances;
}

ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ATRIADIstanaExploreV5DR32MediumDistanceTurfActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetActorEnableCollision(false);

    SceneRoot =
        CreateDefaultSubobject<USceneComponent>(TEXT("R32MediumDistanceTurfRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    SceneRoot->SetRelativeTransform(FTransform::Identity);

    GrassComponents.Reserve(BucketCount);
    for (int32 Bucket = 0; Bucket < BucketCount; ++Bucket)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
                FName(*FString::Printf(
                    TEXT("R32MediumDistanceTurf_%02d"), Bucket)));
        Component->SetupAttachment(SceneRoot);
        ConfigureRenderOnly(Component);
        GrassComponents.Add(Component);
    }
}

bool ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& Assets,
    FString& OutError)
{
    if (Assets.GrassMeshVariants.Num() != MeshVariantCount ||
        Assets.GrassProfileMaterials.Num() != ProfileCount)
    {
        OutError = TEXT("R32 turf requires exactly three mesh variants and four profile materials.");
        return false;
    }
    for (int32 Variant = 0; Variant < MeshVariantCount; ++Variant)
    {
        const UStaticMesh* Mesh = Assets.GrassMeshVariants[Variant];
        const FBox Bounds = Mesh ? Mesh->GetBoundingBox() : FBox(ForceInit);
        const FStaticMeshRenderData* RenderData =
            Mesh ? Mesh->GetRenderData() : nullptr;
        if (!IsExactPath(Mesh, GrassMeshPaths[Variant]) ||
            !FMath::IsNearlyEqual(
                Bounds.Min.Z, 0.0, MeshBoundsToleranceCm) ||
            !FMath::IsNearlyEqual(
                Bounds.Max.Z,
                MeshMaximumHeightCm[Variant],
                MeshBoundsToleranceCm) ||
            Mesh->GetNumLODs() != 1 || !RenderData ||
            RenderData->LODResources.Num() != 1 ||
            RenderData->LODResources[0].GetNumTriangles() !=
                MeshTriangleCounts[Variant] ||
            Mesh->GetStaticMaterials().Num() != 1 ||
            Mesh->GetStaticMaterials()[0].MaterialSlotName != FName(TEXT("R29Grass")))
        {
            OutError = TEXT("R32 turf requires the exact admitted one-LOD R29 modeled-blade mesh roster, material slot, and height bounds.");
            return false;
        }
    }
    for (int32 Profile = 0; Profile < ProfileCount; ++Profile)
    {
        if (!IsExactPath(
                Assets.GrassProfileMaterials[Profile],
                GrassMaterialPaths[Profile]))
        {
            OutError = TEXT("R32 turf requires the exact isolated R32 profile-material derivative roster.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    BuildDeterministicLayout(
        const TArray<FTransform>& Manicured,
        const TArray<FTransform>& Humid,
        const TArray<FTransform>& Shade,
        const TArray<FTransform>& DryEdge,
        FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout& OutLayout,
        FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout{};
    const TArray<FTransform>* Profiles[] = {
        &Manicured, &Humid, &Shade, &DryEdge};
    for (int32 Profile = 0; Profile < ProfileCount; ++Profile)
    {
        if (Profiles[Profile]->Num() != SourceProfileCounts[Profile])
        {
            OutError = TEXT("R32 turf refused a source profile with a non-R23 census.");
            return false;
        }
    }

    OutLayout.GrassBuckets.SetNum(BucketCount);
    for (int32 Profile = 0; Profile < ProfileCount; ++Profile)
    {
        const TArray<FTransform>* CurrentProfile = Profiles[Profile];
        TArray<int32> RankedSourceIndices;
        RankedSourceIndices.Reserve(CurrentProfile->Num());
        for (int32 SourceIndex = 0;
             SourceIndex < CurrentProfile->Num();
             ++SourceIndex)
        {
            const FTransform& Source = (*CurrentProfile)[SourceIndex];
            if (Source.ContainsNaN())
            {
                OutLayout = FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout{};
                OutError = TEXT("R32 turf refused a non-finite source transform.");
                return false;
            }
            RankedSourceIndices.Add(SourceIndex);
        }

        // Select the exact one-quarter quota by a spatial/source hash rather
        // than the visible ordinal 0,4,8 pattern. SourceIndex is only the
        // deterministic collision tie-breaker after the versioned hash.
        RankedSourceIndices.Sort(
            [CurrentProfile, Profile](int32 Left, int32 Right)
            {
                const uint32 LeftHash = HashForPurpose(
                    (*CurrentProfile)[Left],
                    Left,
                    Profile,
                    SelectionHashSalt);
                const uint32 RightHash = HashForPurpose(
                    (*CurrentProfile)[Right],
                    Right,
                    Profile,
                    SelectionHashSalt);
                return LeftHash == RightHash
                    ? Left < Right
                    : LeftHash < RightHash;
            });
        RankedSourceIndices.SetNum(
            ProfileQuotas[Profile],
            EAllowShrinking::No);

        // Independently rank the selected set for mesh assignment. Fixed
        // per-profile quotas preserve all twelve non-empty buckets and the
        // exact global 50/30/20 realized census without a modulo schedule.
        RankedSourceIndices.Sort(
            [CurrentProfile, Profile](int32 Left, int32 Right)
            {
                const uint32 LeftHash = HashForPurpose(
                    (*CurrentProfile)[Left],
                    Left,
                    Profile,
                    VariantHashSalt);
                const uint32 RightHash = HashForPurpose(
                    (*CurrentProfile)[Right],
                    Right,
                    Profile,
                    VariantHashSalt);
                return LeftHash == RightHash
                    ? Left < Right
                    : LeftHash < RightHash;
            });

        int32 SelectedCursor = 0;
        for (int32 Variant = 0; Variant < MeshVariantCount; ++Variant)
        {
            const int32 VariantQuota =
                ProfileVariantQuotas[Profile][Variant];
            for (int32 VariantOrdinal = 0;
                 VariantOrdinal < VariantQuota;
                 ++VariantOrdinal)
            {
                const int32 SourceIndex =
                    RankedSourceIndices[SelectedCursor++];
                const FTransform& Source =
                    (*CurrentProfile)[SourceIndex];
                OutLayout
                    .GrassBuckets[
                        Profile * MeshVariantCount + Variant]
                    .WorldTransforms.Add(MakePresentationTransform(
                        Source,
                        SourceIndex,
                        Profile,
                        Variant));
            }
        }
        if (SelectedCursor != ProfileQuotas[Profile])
        {
            OutLayout = FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout{};
            OutError = TEXT("R32 turf hash-ranked selection lost its exact profile quota.");
            return false;
        }
    }

    if (!ValidateLayoutStructure(OutLayout, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout{};
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::ClearOwnedInstances()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
}

bool ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::PopulateSavedLayout(
    FString& OutError)
{
    for (int32 Bucket = 0; Bucket < BucketCount; ++Bucket)
    {
        const int32 Profile = Bucket / MeshVariantCount;
        const int32 Variant = Bucket % MeshVariantCount;
        if (!PopulateComponent(
                GrassComponents[Bucket],
                this,
                SceneRoot,
                SavedAssets.GrassMeshVariants[Variant],
                SavedAssets.GrassProfileMaterials[Profile],
                SavedLayout.GrassBuckets[Bucket].WorldTransforms,
                OutError))
        {
            return false;
        }
    }
    return true;
}

bool ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ConfigureR32MediumDistanceTurf(
        ATRIADIstanaExploreV5DGroundVegetationActor*
            InSourceGroundVegetation,
        const FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& InAssets,
        FString& OutError)
{
    if (bConfigured)
    {
        OutError = TEXT("R32 turf refuses in-place reconfiguration.");
        return false;
    }
    UWorld* World = GetWorld();
    if (!World || !IsValid(InSourceGroundVegetation) ||
        InSourceGroundVegetation->GetWorld() != World ||
        !Tags.IsEmpty() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.0001))
    {
        OutError = TEXT("R32 turf requires one valid same-world R23 source actor, an identity world transform, and an initially empty actor-tag roster.");
        return false;
    }
    if (World)
    {
        for (TActorIterator<ATRIADIstanaExploreV5DR32MediumDistanceTurfActor>
                 Iterator(World);
             Iterator;
             ++Iterator)
        {
            const ATRIADIstanaExploreV5DR32MediumDistanceTurfActor* Candidate =
                *Iterator;
            if (Candidate != this && IsValid(Candidate) &&
                Candidate->bConfigured)
            {
                OutError = TEXT("R32 turf refused a second configured render owner.");
                return false;
            }
        }
    }
    if (!ValidateAssetRoster(InAssets, OutError))
    {
        return false;
    }

    TArray<FTransform> Manicured;
    TArray<FTransform> Humid;
    TArray<FTransform> Shade;
    TArray<FTransform> DryEdge;
    FString SourceReport;
    if (!InSourceGroundVegetation->GetMediumDistanceTurfSourceProfilesR32(
            Manicured, Humid, Shade, DryEdge, SourceReport))
    {
        OutError = TEXT("R32 turf source admission failed: ") + SourceReport;
        return false;
    }
    FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout NewLayout;
    if (!BuildDeterministicLayout(
            Manicured,
            Humid,
            Shade,
            DryEdge,
            NewLayout,
            OutError))
    {
        return false;
    }

    ClearOwnedInstances();
    SourceGroundVegetation = InSourceGroundVegetation;
    SavedAssets = InAssets;
    SavedLayout = MoveTemp(NewLayout);
    ClaimLabel = ExpectedClaimLabel();
    bConfigured = true;
    bAppearanceOnly = true;
    bSourceActorsOrAssetsModified = false;
    bCollisionOrNavigationAuthority = false;
    bSensorRfOrGeospatialAuthority = false;
    bBotanicalSurveyAsBuiltOrCurrentConditionClaimed = false;
    bVisualCaptureAccepted = false;
    SetActorEnableCollision(false);
    SetActorHiddenInGame(false);
    Tags.AddUnique(ExpectedActorTag());

    if (!PopulateSavedLayout(OutError))
    {
        ClearOwnedInstances();
        SourceGroundVegetation = nullptr;
        SavedAssets = FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets{};
        SavedLayout = FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout{};
        ClaimLabel.Reset();
        bConfigured = false;
        Tags.Remove(ExpectedActorTag());
        return false;
    }
    FString Report;
    if (!ValidateR32MediumDistanceTurf(Report))
    {
        ClearOwnedInstances();
        SourceGroundVegetation = nullptr;
        SavedAssets = FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets{};
        SavedLayout = FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout{};
        ClaimLabel.Reset();
        bConfigured = false;
        Tags.Remove(ExpectedActorTag());
        OutError = TEXT("R32 turf failed closed post-population validation: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ValidateR32MediumDistanceTurf(FString& OutReport) const
{
    FString Error;
    if (!bConfigured || !IsValid(SourceGroundVegetation) ||
        !GetWorld() || SourceGroundVegetation->GetWorld() != GetWorld() ||
        !ValidateAssetRoster(SavedAssets, Error) ||
        !ValidateLayoutStructure(SavedLayout, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_TURF_INVALID_LAYOUT_ASSETS_OR_SOURCE: ") +
            Error;
        return false;
    }
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0001) ||
        GetActorEnableCollision() || IsHidden() || Tags.Num() != 1 ||
        !Tags.Contains(ExpectedActorTag()) ||
        ClaimLabel != ExpectedClaimLabel() || !bAppearanceOnly ||
        bSourceActorsOrAssetsModified || bCollisionOrNavigationAuthority ||
        bSensorRfOrGeospatialAuthority ||
        bBotanicalSurveyAsBuiltOrCurrentConditionClaimed ||
        bVisualCaptureAccepted)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_TURF_INVALID_TRUTH_BOUNDARY");
        return false;
    }
    if (!SceneRoot || GetRootComponent() != SceneRoot ||
        SceneRoot->GetOwner() != this || SceneRoot->GetAttachParent() ||
        SceneRoot->Mobility != EComponentMobility::Static ||
        !SceneRoot->GetRelativeTransform().Equals(
            FTransform::Identity, TransformToleranceCm) ||
        GrassComponents.Num() != BucketCount)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_TURF_INVALID_COMPONENT_ROSTER");
        return false;
    }

    TArray<FTransform> Manicured;
    TArray<FTransform> Humid;
    TArray<FTransform> Shade;
    TArray<FTransform> DryEdge;
    FString SourceReport;
    if (!SourceGroundVegetation->GetMediumDistanceTurfSourceProfilesR32(
            Manicured, Humid, Shade, DryEdge, SourceReport))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_TURF_INVALID_SOURCE_HANDOFF: ") +
            SourceReport;
        return false;
    }
    FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout ExpectedLayout;
    if (!BuildDeterministicLayout(
            Manicured,
            Humid,
            Shade,
            DryEdge,
            ExpectedLayout,
            Error) ||
        !LayoutsEqual(SavedLayout, ExpectedLayout))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_TURF_INVALID_DETERMINISTIC_SOURCE_BINDING: ") +
            Error;
        return false;
    }

    for (int32 Bucket = 0; Bucket < BucketCount; ++Bucket)
    {
        const int32 Profile = Bucket / MeshVariantCount;
        const int32 Variant = Bucket % MeshVariantCount;
        if (!ComponentMatches(
                GrassComponents[Bucket],
                this,
                SceneRoot,
                SavedAssets.GrassMeshVariants[Variant],
                SavedAssets.GrassProfileMaterials[Profile],
                SavedLayout.GrassBuckets[Bucket].WorldTransforms))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_TURF_INVALID_RENDER_PRESENTATION");
            return false;
        }
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_VALID actorContractVersion=3 sourceR23Transforms=%d deterministicHashSelectionDenominator=%d distributionPolicyVersion=%d selectedTransforms=%d profileQuotas=%d,%d,%d,%d buckets=%d meshVariantHashTargetWeights=50,30,20 realizedVariantCensus=%d,%d,%d worstCaseVisibleTriangles=%d singleLodMeshes=true exactLod0TriangleCensusValidated=true targetHardwarePerformanceGateRequired=true translationsPreservedExactly=true sourceRotationsPreservedExactly=false deterministicAddedYawDegrees=%.1f-%.1f xyScaleMultiplier=%.2f-%.2f localTipHeightCm=%.1f-%.1f cullMeters=%.1f-%.1f isolatedR32MaterialDerivatives=4 continuousMacroScaleMeters=11.0 continuousMesoScaleMeters=3.4 mediumResponseMeters=20-65 sourceActorsModified=false sourceAssetsModified=false humanMainPass=true visibleInSceneCapture=true rgbSimulationPresentationOnly=true pbrTruthClaimed=false siteMeasurementClaimed=false visualCaptureAccepted=false nativeMapApplicationClaimed=false appearanceOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false geospatialAuthority=false surveyClaim=false botanicalClaim=false currentConditionClaim=false"),
        SourceInstanceCount,
        SelectionDenominator,
        DistributionPolicyVersion,
        SelectedInstanceCount,
        ProfileQuotas[0],
        ProfileQuotas[1],
        ProfileQuotas[2],
        ProfileQuotas[3],
        BucketCount,
        VariantInstanceCounts[0],
        VariantInstanceCounts[1],
        VariantInstanceCounts[2],
        WorstCaseVisibleTriangleCount,
        -MaximumAddedYawDegrees,
        MaximumAddedYawDegrees,
        MinimumXyScaleMultiplier,
        MaximumXyScaleMultiplier,
        MinimumTipHeightCm,
        MaximumTipHeightCm,
        CullStartDistanceCm / 100.0,
        CullEndDistanceCm / 100.0);
    return true;
}

const FString&
ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::ExpectedClaimLabel()
{
    return R32ClaimLabel;
}

const FName& ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedActorTag()
{
    return ActorTag;
}

const FName& ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedComponentTag()
{
    return ComponentTag;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedSourceInstanceCount()
{
    return SourceInstanceCount;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::ExpectedInstanceCount()
{
    return SelectedInstanceCount;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedSelectionDenominator()
{
    return SelectionDenominator;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::ExpectedProfileCount()
{
    return ProfileCount;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedMeshVariantCount()
{
    return MeshVariantCount;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::ExpectedBucketCount()
{
    return BucketCount;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedProfileQuota(int32 ProfileIndex)
{
    return ProfileIndex >= 0 && ProfileIndex < ProfileCount
        ? ProfileQuotas[ProfileIndex]
        : 0;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedCullStartDistanceCm()
{
    return CullStartDistanceCm;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedCullEndDistanceCm()
{
    return CullEndDistanceCm;
}

int32 ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedWpoDisableDistanceCm()
{
    return WpoDisableDistanceCm;
}

double ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedMinimumTipHeightCm()
{
    return MinimumTipHeightCm;
}

double ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
    ExpectedMaximumTipHeightCm()
{
    return MaximumTipHeightCm;
}
