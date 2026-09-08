#include "TRIADIstanaExploreV5DR29VegetationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"

namespace
{
constexpr int32 GrassVariantCount = 3;
constexpr int32 GrassProfileCount = 4;
constexpr int32 GrassBucketCount = GrassVariantCount * GrassProfileCount;
constexpr int32 GrassInstanceCount = 6144;
constexpr int32 TreeMorphologyCount = 5;
constexpr int32 TreeInstanceCount = 7;
constexpr int32 ShrubInstanceCount = 36;
constexpr int32 UnderstoreyInstanceCount = 44;
constexpr int32 FlowerInstanceCount = 64;
constexpr int32 GrassCullStartDistanceCm = 6500;
constexpr int32 GrassCullEndDistanceCm = 9000;
constexpr int32 GrassWpoDisableDistanceCm = 6000;
constexpr int32 NormalResponseEndDistanceCm = 6000;
constexpr int32 RoughnessResponseEndDistanceCm = 6500;
constexpr int32 PlantingCullStartDistanceCm = 9000;
constexpr int32 PlantingCullEndDistanceCm = 18000;
constexpr int32 TreeCullStartDistanceCm = 18000;
constexpr int32 TreeCullEndDistanceCm = 80000;
constexpr float GrassLodDistanceScale = 1.35f;
constexpr float TreeLodDistanceScale = 1.80f;

static_assert(GrassBucketCount == 12);
static_assert(GrassInstanceCount == 3072 * 2);

const FString R29ClaimLabel(
    TEXT("VISUAL_ASSUMPTION_BOUND_R29_MODELED_BLADE_TROPICAL_MORPHOLOGY_VARIATION"));
const FName ActorTag(
    TEXT("TRIAD_IstanaExploreV5D_R29Vegetation_RenderOnly"));

const FString GrassMeshPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/SM_IPV5D_R29_GrassFineCluster_Render.SM_IPV5D_R29_GrassFineCluster_Render"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/SM_IPV5D_R29_GrassBroadCluster_Render.SM_IPV5D_R29_GrassBroadCluster_Render"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Meshes/SM_IPV5D_R29_GrassMixedCluster_Render.SM_IPV5D_R29_GrassMixedCluster_Render")};
const FString GrassMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_Manicured.M_IPV5D_R29_Turf_Manicured"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_Humid.M_IPV5D_R29_Turf_Humid"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_Shade.M_IPV5D_R29_Turf_Shade"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/VegetationR29/Materials/M_IPV5D_R29_Turf_DryEdge.M_IPV5D_R29_Turf_DryEdge")};
const FString TreeMeshPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_ColumnarNarrow_NearLOD0.SM_IPV5D_Tree_ColumnarNarrow_NearLOD0"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Palm_NearLOD0.SM_IPV5D_Tree_Palm_NearLOD0")};
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

uint32 HashTransform(const FTransform& Transform, int32 Ordinal, uint32 Salt)
{
    const FVector Location = Transform.GetTranslation();
    uint32 Value = MixBits(static_cast<uint32>(FMath::RoundToInt(Location.X * 10.0)) ^ Salt);
    Value = MixBits(Value ^ static_cast<uint32>(FMath::RoundToInt(Location.Y * 10.0)));
    Value = MixBits(Value ^ static_cast<uint32>(FMath::RoundToInt(Location.Z * 10.0)));
    return MixBits(Value ^ static_cast<uint32>(Ordinal));
}

bool IsExactPath(const UObject* Object, const FString& ExpectedPath)
{
    return IsValid(Object) && Object->GetPathName() == ExpectedPath;
}

void ConfigureRenderOnly(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 CullStart,
    int32 CullEnd,
    int32 WpoDisable,
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
    Component->SetCastContactShadow(false);
    Component->SetAffectDistanceFieldLighting(bCastShadow);
    Component->SetCullDistances(CullStart, CullEnd);
    Component->WorldPositionOffsetDisableDistance = WpoDisable;
    Component->bEnableDensityScaling = false;
    Component->bAutoRebuildTreeOnInstanceChanges = false;
    Component->ComponentTags.AddUnique(ActorTag);
}

FTransform MakeGrassPresentationTransform(
    const FTransform& Source,
    int32 Ordinal,
    int32 Profile)
{
    const uint32 Hash = HashTransform(
        Source,
        Ordinal,
        0xD1B54A35u ^ static_cast<uint32>(Profile * 0x45D9F3Bu));
    FRotator Rotation = Source.Rotator();
    Rotation.Yaw += FMath::Lerp(-8.0, 8.0, HashUnit(Hash ^ 0xA3C59AC3u));
    Rotation.Pitch = FMath::Lerp(-1.8, 1.8, HashUnit(Hash ^ 0x3C6EF372u));
    Rotation.Roll = FMath::Lerp(-2.2, 2.2, HashUnit(Hash ^ 0x9E3779B9u));
    FVector Scale = Source.GetScale3D();
    Scale.X *= FMath::Lerp(0.95, 1.05, HashUnit(Hash ^ 0x85EBCA6Bu));
    Scale.Y *= FMath::Lerp(0.94, 1.06, HashUnit(Hash ^ 0xC2B2AE35u));
    // The new carriers contain 7--19 cm geometry instead of the 4.4 cm R11
    // source; this bounded correction keeps managed-turf tips below 22 cm.
    Scale.Z *= FMath::Lerp(0.50, 0.56, HashUnit(Hash ^ 0x27D4EB2Fu));
    return FTransform(Rotation, Source.GetTranslation(), Scale);
}

FTransform MakeTreePresentationTransform(
    const FTransform& Source,
    int32 Ordinal)
{
    const uint32 Hash = HashTransform(Source, Ordinal, 0x94D049BBu);
    FRotator Rotation = Source.Rotator();
    Rotation.Yaw += FMath::Lerp(-6.0, 6.0, HashUnit(Hash ^ 0x165667B1u));
    Rotation.Pitch = FMath::Lerp(-0.9, 0.9, HashUnit(Hash ^ 0xD3A2646Cu));
    Rotation.Roll = FMath::Lerp(-1.2, 1.2, HashUnit(Hash ^ 0xFD7046C5u));
    FVector Scale = Source.GetScale3D();
    Scale.X *= FMath::Lerp(0.97, 1.04, HashUnit(Hash ^ 0xB55A4F09u));
    Scale.Y *= FMath::Lerp(0.96, 1.05, HashUnit(Hash ^ 0x9E3779B9u));
    Scale.Z *= FMath::Lerp(0.98, 1.03, HashUnit(Hash ^ 0x7F4A7C15u));
    return FTransform(Rotation, Source.GetTranslation(), Scale);
}

void AppendGrassProfile(
    const TArray<FTransform>& First,
    const TArray<FTransform>& Second,
    int32 Profile,
    FTRIADIstanaExploreV5DR29VegetationLayout& OutLayout)
{
    int32 Ordinal = 0;
    auto Append = [&](const TArray<FTransform>& Source)
    {
        for (const FTransform& Transform : Source)
        {
            const uint32 Hash = HashTransform(
                Transform,
                Ordinal,
                0x5297A4D1u ^ static_cast<uint32>(Profile * 0x68E31DA4u));
            const uint32 Bucket = MixBits(Hash ^ 0xA511E9B3u) % 10u;
            const int32 Variant = Bucket < 5u ? 0 : (Bucket < 8u ? 1 : 2);
            OutLayout.GrassBuckets[Profile * GrassVariantCount + Variant]
                .WorldTransforms.Add(
                    MakeGrassPresentationTransform(Transform, Ordinal, Profile));
            ++Ordinal;
        }
    };
    Append(First);
    Append(Second);
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

bool ComponentMatches(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    UStaticMesh* Mesh,
    UMaterialInterface* Material,
    const TArray<FTransform>& Expected)
{
    if (!Component || Component->GetStaticMesh() != Mesh ||
        Component->GetInstanceCount() != Expected.Num() ||
        (Material && Component->GetMaterial(0) != Material))
    {
        return false;
    }
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        FTransform Actual;
        if (!Component->GetInstanceTransform(Index, Actual, true) ||
            !Actual.Equals(Expected[Index], 0.001))
        {
            return false;
        }
    }
    return true;
}

bool PopulateComponent(
    UHierarchicalInstancedStaticMeshComponent* Component,
    UStaticMesh* Mesh,
    UMaterialInterface* Material,
    const TArray<FTransform>& Transforms,
    FString& OutError)
{
    if (!Component || !Mesh)
    {
        OutError = TEXT("R29 vegetation component or mesh is missing.");
        return false;
    }
    Component->ClearInstances();
    Component->SetStaticMesh(Mesh);
    Component->EmptyOverrideMaterials();
    if (Material)
    {
        Component->SetMaterial(0, Material);
    }
    Component->AddInstances(Transforms, false, true, false);
    Component->BuildTreeIfOutdated(false, true);
    if (!ComponentMatches(Component, Mesh, Material, Transforms) ||
        !Component->IsTreeFullyBuilt())
    {
        OutError = TEXT("R29 vegetation HISM population failed exact synchronous validation.");
        return false;
    }
    return true;
}

bool ValidateLayoutInternal(
    const FTRIADIstanaExploreV5DR29VegetationLayout& Layout,
    FString& OutError)
{
    if (Layout.GrassBuckets.Num() != GrassBucketCount ||
        Layout.TreeBuckets.Num() != TreeMorphologyCount ||
        Layout.GrassTotal() != GrassInstanceCount ||
        Layout.TreeTotal() != TreeInstanceCount ||
        Layout.Shrubs.Num() != ShrubInstanceCount ||
        Layout.Understorey.Num() != UnderstoreyInstanceCount ||
        Layout.Flowers.Num() != FlowerInstanceCount)
    {
        OutError = TEXT("R29 vegetation layout lost its exact inherited census or bucket roster.");
        return false;
    }
    for (const FTRIADIstanaExploreV5DR29VegetationBucket& Bucket :
         Layout.GrassBuckets)
    {
        if (Bucket.WorldTransforms.IsEmpty())
        {
            OutError = TEXT("R29 grass did not retain all twelve profile/silhouette combinations.");
            return false;
        }
    }
    for (const FTRIADIstanaExploreV5DR29VegetationBucket& Bucket :
         Layout.TreeBuckets)
    {
        if (Bucket.WorldTransforms.IsEmpty())
        {
            OutError = TEXT("R29 tree palette did not retain all five tropical morphology cues.");
            return false;
        }
    }

    FTRIADIstanaExploreV5DLandmarkVegetationLayout Source;
    if (!ATRIADIstanaExploreV5DLandmarkVegetationActor::
            BuildDeterministicLayoutR28(Source, OutError))
    {
        return false;
    }
    TArray<FVector> ExpectedTreeLocations;
    auto AppendLocations = [&](const TArray<FTransform>& Transforms)
    {
        for (const FTransform& Transform : Transforms)
        {
            ExpectedTreeLocations.Add(Transform.GetTranslation());
        }
    };
    AppendLocations(Source.MacDonaldHouse.TreesUmbrella);
    AppendLocations(Source.MacDonaldHouse.TreesDome);
    AppendLocations(Source.MacDonaldHouse.TreesHighFork);
    AppendLocations(Source.TemasekShophouse.TreesUmbrella);
    AppendLocations(Source.TemasekShophouse.TreesDome);
    AppendLocations(Source.TemasekShophouse.TreesHighFork);
    TArray<FVector> ActualTreeLocations;
    for (const FTRIADIstanaExploreV5DR29VegetationBucket& Bucket :
         Layout.TreeBuckets)
    {
        for (const FTransform& Transform : Bucket.WorldTransforms)
        {
            ActualTreeLocations.Add(Transform.GetTranslation());
        }
    }
    for (const FVector& Expected : ExpectedTreeLocations)
    {
        if (!ActualTreeLocations.ContainsByPredicate(
                [&](const FVector& Actual)
                {
                    return Actual.Equals(Expected, 0.001);
                }))
        {
            OutError = TEXT("R29 tree morphology variation moved an inherited R28 anchor.");
            return false;
        }
    }

    TMap<FIntVector, int32> ExpectedGrassLocations;
    auto CountGrassLocations = [](
        const TArray<FTransform>& Transforms,
        TMap<FIntVector, int32>& Counts)
    {
        for (const FTransform& Transform : Transforms)
        {
            const FVector Location = Transform.GetTranslation();
            const FIntVector Key(
                FMath::RoundToInt(Location.X * 1000.0),
                FMath::RoundToInt(Location.Y * 1000.0),
                FMath::RoundToInt(Location.Z * 1000.0));
            ++Counts.FindOrAdd(Key);
        }
    };
    CountGrassLocations(
        Source.MacDonaldHouse.GrassManicured, ExpectedGrassLocations);
    CountGrassLocations(Source.MacDonaldHouse.GrassHumid, ExpectedGrassLocations);
    CountGrassLocations(Source.MacDonaldHouse.GrassShade, ExpectedGrassLocations);
    CountGrassLocations(Source.MacDonaldHouse.GrassDryEdge, ExpectedGrassLocations);
    CountGrassLocations(
        Source.TemasekShophouse.GrassManicured, ExpectedGrassLocations);
    CountGrassLocations(Source.TemasekShophouse.GrassHumid, ExpectedGrassLocations);
    CountGrassLocations(Source.TemasekShophouse.GrassShade, ExpectedGrassLocations);
    CountGrassLocations(Source.TemasekShophouse.GrassDryEdge, ExpectedGrassLocations);
    TMap<FIntVector, int32> ActualGrassLocations;
    for (const FTRIADIstanaExploreV5DR29VegetationBucket& Bucket :
         Layout.GrassBuckets)
    {
        CountGrassLocations(Bucket.WorldTransforms, ActualGrassLocations);
    }
    bool bGrassLocationsMatch =
        ActualGrassLocations.Num() == ExpectedGrassLocations.Num();
    for (const TPair<FIntVector, int32>& Expected : ExpectedGrassLocations)
    {
        const int32* Actual = ActualGrassLocations.Find(Expected.Key);
        if (!Actual || *Actual != Expected.Value)
        {
            bGrassLocationsMatch = false;
            break;
        }
    }
    if (!bGrassLocationsMatch)
    {
        OutError = TEXT("R29 grass silhouette variation moved an inherited R28 world position.");
        return false;
    }
    OutError.Reset();
    return true;
}
} // namespace

int32 FTRIADIstanaExploreV5DR29VegetationLayout::GrassTotal() const
{
    int32 Total = 0;
    for (const FTRIADIstanaExploreV5DR29VegetationBucket& Bucket : GrassBuckets)
    {
        Total += Bucket.WorldTransforms.Num();
    }
    return Total;
}

int32 FTRIADIstanaExploreV5DR29VegetationLayout::TreeTotal() const
{
    int32 Total = 0;
    for (const FTRIADIstanaExploreV5DR29VegetationBucket& Bucket : TreeBuckets)
    {
        Total += Bucket.WorldTransforms.Num();
    }
    return Total;
}

ATRIADIstanaExploreV5DR29VegetationActor::
    ATRIADIstanaExploreV5DR29VegetationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("R29VegetationRoot"));
    SetRootComponent(SceneRoot);

    GrassComponents.Reserve(GrassBucketCount);
    for (int32 Index = 0; Index < GrassBucketCount; ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
                FName(*FString::Printf(TEXT("R29Grass_%02d"), Index)));
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        ConfigureRenderOnly(
            Component,
            GrassCullStartDistanceCm,
            GrassCullEndDistanceCm,
            GrassWpoDisableDistanceCm,
            false);
        Component->InstanceLODDistanceScale = GrassLodDistanceScale;
        GrassComponents.Add(Component);
    }

    TreeComponents.Reserve(TreeMorphologyCount);
    for (int32 Index = 0; Index < TreeMorphologyCount; ++Index)
    {
        UHierarchicalInstancedStaticMeshComponent* Component =
            CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
                FName(*FString::Printf(TEXT("R29TreeMorphology_%02d"), Index)));
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        ConfigureRenderOnly(
            Component,
            TreeCullStartDistanceCm,
            TreeCullEndDistanceCm,
            0,
            true);
        Component->bOverrideMinLOD = true;
        Component->MinLOD = 0;
        Component->ForcedLodModel = 0;
        Component->InstanceLODDistanceScale = TreeLodDistanceScale;
        TreeComponents.Add(Component);
    }

    ShrubInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("R29Shrubs"));
    UnderstoreyInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("R29Understorey"));
    FlowerInstances =
        CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(
            TEXT("R29Flowers"));
    UHierarchicalInstancedStaticMeshComponent* Planting[] = {
        ShrubInstances, UnderstoreyInstances, FlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Planting)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        ConfigureRenderOnly(
            Component,
            PlantingCullStartDistanceCm,
            PlantingCullEndDistanceCm,
            GrassWpoDisableDistanceCm,
            true);
    }
}

bool ATRIADIstanaExploreV5DR29VegetationActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DR29VegetationAssets& Assets,
    FString& OutError)
{
    if (Assets.GrassMeshVariants.Num() != GrassVariantCount ||
        Assets.GrassProfileMaterials.Num() != GrassProfileCount ||
        Assets.TropicalTreeMeshes.Num() != TreeMorphologyCount)
    {
        OutError = TEXT("R29 vegetation asset arrays are incomplete.");
        return false;
    }
    for (int32 Index = 0; Index < GrassVariantCount; ++Index)
    {
        if (!IsExactPath(Assets.GrassMeshVariants[Index], GrassMeshPaths[Index]))
        {
            OutError = TEXT("R29 vegetation requires the exact isolated modeled-blade mesh roster.");
            return false;
        }
    }
    for (int32 Index = 0; Index < GrassProfileCount; ++Index)
    {
        if (!IsExactPath(
                Assets.GrassProfileMaterials[Index],
                GrassMaterialPaths[Index]))
        {
            OutError = TEXT("R29 vegetation requires the exact isolated long-range grass material roster.");
            return false;
        }
    }
    for (int32 Index = 0; Index < TreeMorphologyCount; ++Index)
    {
        if (!IsExactPath(Assets.TropicalTreeMeshes[Index], TreeMeshPaths[Index]))
        {
            OutError = TEXT("R29 vegetation requires the exact admitted five-form tree derivative roster.");
            return false;
        }
    }
    if (!IsExactPath(Assets.ShrubMesh, ShrubMeshPath) ||
        !IsExactPath(Assets.ShrubMaterial, ShrubMaterialPath) ||
        !IsExactPath(Assets.UnderstoreyMesh, UnderstoreyMeshPath) ||
        !IsExactPath(Assets.UnderstoreyMaterial, UnderstoreyMaterialPath) ||
        !IsExactPath(Assets.FlowerMesh, FlowerMeshPath) ||
        !IsExactPath(Assets.FlowerMaterial, FlowerMaterialPath))
    {
        OutError = TEXT("R29 vegetation requires the exact reusable V4 planting roster.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29VegetationActor::BuildDeterministicLayout(
    FTRIADIstanaExploreV5DR29VegetationLayout& OutLayout,
    FString& OutError)
{
    OutLayout = FTRIADIstanaExploreV5DR29VegetationLayout{};
    OutLayout.GrassBuckets.SetNum(GrassBucketCount);
    OutLayout.TreeBuckets.SetNum(TreeMorphologyCount);

    FTRIADIstanaExploreV5DLandmarkVegetationLayout Source;
    if (!ATRIADIstanaExploreV5DLandmarkVegetationActor::
            BuildDeterministicLayoutR28(Source, OutError))
    {
        return false;
    }
    const TArray<FTransform>* MacGrass[] = {
        &Source.MacDonaldHouse.GrassManicured,
        &Source.MacDonaldHouse.GrassHumid,
        &Source.MacDonaldHouse.GrassShade,
        &Source.MacDonaldHouse.GrassDryEdge};
    const TArray<FTransform>* TemasekGrass[] = {
        &Source.TemasekShophouse.GrassManicured,
        &Source.TemasekShophouse.GrassHumid,
        &Source.TemasekShophouse.GrassShade,
        &Source.TemasekShophouse.GrassDryEdge};
    for (int32 Profile = 0; Profile < GrassProfileCount; ++Profile)
    {
        AppendGrassProfile(
            *MacGrass[Profile],
            *TemasekGrass[Profile],
            Profile,
            OutLayout);
    }

    TArray<FTransform> SourceTrees;
    SourceTrees.Append(Source.MacDonaldHouse.TreesUmbrella);
    SourceTrees.Append(Source.MacDonaldHouse.TreesDome);
    SourceTrees.Append(Source.MacDonaldHouse.TreesHighFork);
    SourceTrees.Append(Source.TemasekShophouse.TreesUmbrella);
    SourceTrees.Append(Source.TemasekShophouse.TreesDome);
    SourceTrees.Append(Source.TemasekShophouse.TreesHighFork);
    // With seven inherited anchors this exact palette guarantees that every
    // admitted form is visible while avoiding repeated adjacent silhouettes.
    const int32 MorphologyByOrdinal[] = {0, 1, 2, 3, 4, 0, 1};
    if (SourceTrees.Num() != UE_ARRAY_COUNT(MorphologyByOrdinal))
    {
        OutError = TEXT("R29 vegetation refused an unexpected R28 tree census.");
        return false;
    }
    for (int32 Ordinal = 0; Ordinal < SourceTrees.Num(); ++Ordinal)
    {
        OutLayout.TreeBuckets[MorphologyByOrdinal[Ordinal]]
            .WorldTransforms.Add(
                MakeTreePresentationTransform(SourceTrees[Ordinal], Ordinal));
    }

    OutLayout.Shrubs.Append(Source.MacDonaldHouse.Shrubs);
    OutLayout.Shrubs.Append(Source.TemasekShophouse.Shrubs);
    OutLayout.Understorey.Append(Source.MacDonaldHouse.Understorey);
    OutLayout.Understorey.Append(Source.TemasekShophouse.Understorey);
    OutLayout.Flowers.Append(Source.MacDonaldHouse.Flowers);
    OutLayout.Flowers.Append(Source.TemasekShophouse.Flowers);
    if (!ValidateLayoutInternal(OutLayout, OutError))
    {
        OutLayout = FTRIADIstanaExploreV5DR29VegetationLayout{};
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DR29VegetationActor::ClearOwnedInstances()
{
    for (UHierarchicalInstancedStaticMeshComponent* Component : GrassComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
    for (UHierarchicalInstancedStaticMeshComponent* Component : TreeComponents)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
    UHierarchicalInstancedStaticMeshComponent* Planting[] = {
        ShrubInstances, UnderstoreyInstances, FlowerInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Planting)
    {
        if (Component)
        {
            Component->ClearInstances();
        }
    }
}

bool ATRIADIstanaExploreV5DR29VegetationActor::PopulateSavedLayout(
    FString& OutError)
{
    for (int32 Bucket = 0; Bucket < GrassBucketCount; ++Bucket)
    {
        const int32 Profile = Bucket / GrassVariantCount;
        const int32 Variant = Bucket % GrassVariantCount;
        if (!PopulateComponent(
                GrassComponents[Bucket],
                SavedAssets.GrassMeshVariants[Variant],
                SavedAssets.GrassProfileMaterials[Profile],
                SavedLayout.GrassBuckets[Bucket].WorldTransforms,
                OutError))
        {
            return false;
        }
    }
    for (int32 Morphology = 0;
         Morphology < TreeMorphologyCount;
         ++Morphology)
    {
        if (!PopulateComponent(
                TreeComponents[Morphology],
                SavedAssets.TropicalTreeMeshes[Morphology],
                nullptr,
                SavedLayout.TreeBuckets[Morphology].WorldTransforms,
                OutError))
        {
            return false;
        }
    }
    return PopulateComponent(
               ShrubInstances,
               SavedAssets.ShrubMesh,
               SavedAssets.ShrubMaterial,
               SavedLayout.Shrubs,
               OutError) &&
        PopulateComponent(
               UnderstoreyInstances,
               SavedAssets.UnderstoreyMesh,
               SavedAssets.UnderstoreyMaterial,
               SavedLayout.Understorey,
               OutError) &&
        PopulateComponent(
               FlowerInstances,
               SavedAssets.FlowerMesh,
               SavedAssets.FlowerMaterial,
               SavedLayout.Flowers,
               OutError);
}

bool ATRIADIstanaExploreV5DR29VegetationActor::ConfigureR29Vegetation(
    const FTRIADIstanaExploreV5DR29VegetationAssets& InAssets,
    FString& OutError)
{
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0001))
    {
        OutError = TEXT("R29 vegetation actor must remain at identity because its saved transforms are world-space.");
        return false;
    }
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ATRIADIstanaExploreV5DLandmarkVegetationActor>
                 Iterator(World);
             Iterator;
             ++Iterator)
        {
            const ATRIADIstanaExploreV5DLandmarkVegetationActor* Candidate =
                *Iterator;
            if (IsValid(Candidate) &&
                Candidate->ClaimLabel ==
                    ATRIADIstanaExploreV5DLandmarkVegetationActor::
                        ExpectedR28ClaimLabel())
            {
                OutError = TEXT("R29 vegetation refused a co-resident configured R28 render owner; the trusted builder must choose exactly one successor.");
                return false;
            }
        }
    }
    if (!ValidateAssetRoster(InAssets, OutError))
    {
        return false;
    }
    FTRIADIstanaExploreV5DR29VegetationLayout Layout;
    if (!BuildDeterministicLayout(Layout, OutError))
    {
        return false;
    }

    ClearOwnedInstances();
    SavedAssets = InAssets;
    SavedLayout = MoveTemp(Layout);
    ClaimLabel = ExpectedClaimLabel();
    bAppearanceOnly = true;
    bSourceActorsOrAssetsModified = false;
    bCollisionOrNavigationAuthority = false;
    bSensorRfOrGeospatialAuthority = false;
    bBotanicalSurveyAsBuiltOrCurrentSeasonClaimed = false;
    SetActorEnableCollision(false);
    Tags.AddUnique(ExpectedActorTag());

    if (!PopulateSavedLayout(OutError))
    {
        ClearOwnedInstances();
        return false;
    }
    FString Report;
    if (!ValidateR29Vegetation(Report))
    {
        ClearOwnedInstances();
        OutError = TEXT("R29 vegetation failed closed post-population validation: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29VegetationActor::ValidateR29Vegetation(
    FString& OutReport) const
{
    FString Error;
    if (!ValidateAssetRoster(SavedAssets, Error) ||
        !ValidateLayoutInternal(SavedLayout, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_INVALID_LAYOUT_OR_ASSETS: ") +
            Error;
        return false;
    }
    if (!GetActorTransform().Equals(FTransform::Identity, 0.0001) ||
        GetActorEnableCollision() || !Tags.Contains(ExpectedActorTag()) ||
        ClaimLabel != ExpectedClaimLabel() || !bAppearanceOnly ||
        bSourceActorsOrAssetsModified || bCollisionOrNavigationAuthority ||
        bSensorRfOrGeospatialAuthority ||
        bBotanicalSurveyAsBuiltOrCurrentSeasonClaimed)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_INVALID_TRUTH_BOUNDARY");
        return false;
    }
    if (GrassComponents.Num() != GrassBucketCount ||
        TreeComponents.Num() != TreeMorphologyCount)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_INVALID_COMPONENT_ROSTER");
        return false;
    }
    for (int32 Bucket = 0; Bucket < GrassBucketCount; ++Bucket)
    {
        const int32 Profile = Bucket / GrassVariantCount;
        const int32 Variant = Bucket % GrassVariantCount;
        const UHierarchicalInstancedStaticMeshComponent* Component =
            GrassComponents[Bucket];
        if (!ComponentMatches(
                Component,
                SavedAssets.GrassMeshVariants[Variant],
                SavedAssets.GrassProfileMaterials[Profile],
                SavedLayout.GrassBuckets[Bucket].WorldTransforms) ||
            !HasRenderOnlyPolicy(
                Component,
                GrassCullStartDistanceCm,
                GrassCullEndDistanceCm,
                GrassWpoDisableDistanceCm) ||
            Component->CastShadow || Component->bCastContactShadow ||
            Component->InstanceLODDistanceScale != GrassLodDistanceScale)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_INVALID_GRASS_PRESENTATION");
            return false;
        }
    }
    for (int32 Morphology = 0;
         Morphology < TreeMorphologyCount;
         ++Morphology)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component =
            TreeComponents[Morphology];
        if (!ComponentMatches(
                Component,
                SavedAssets.TropicalTreeMeshes[Morphology],
                nullptr,
                SavedLayout.TreeBuckets[Morphology].WorldTransforms) ||
            !HasRenderOnlyPolicy(
                Component,
                TreeCullStartDistanceCm,
                TreeCullEndDistanceCm,
                0) ||
            !Component->CastShadow || Component->bCastContactShadow ||
            Component->MinLOD != 0 || Component->ForcedLodModel != 0 ||
            Component->InstanceLODDistanceScale != TreeLodDistanceScale)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_INVALID_TREE_PRESENTATION");
            return false;
        }
    }
    if (!ComponentMatches(
            ShrubInstances,
            SavedAssets.ShrubMesh,
            SavedAssets.ShrubMaterial,
            SavedLayout.Shrubs) ||
        !HasRenderOnlyPolicy(
            ShrubInstances,
            PlantingCullStartDistanceCm,
            PlantingCullEndDistanceCm,
            GrassWpoDisableDistanceCm) ||
        !ComponentMatches(
            UnderstoreyInstances,
            SavedAssets.UnderstoreyMesh,
            SavedAssets.UnderstoreyMaterial,
            SavedLayout.Understorey) ||
        !HasRenderOnlyPolicy(
            UnderstoreyInstances,
            PlantingCullStartDistanceCm,
            PlantingCullEndDistanceCm,
            GrassWpoDisableDistanceCm) ||
        !ComponentMatches(
            FlowerInstances,
            SavedAssets.FlowerMesh,
            SavedAssets.FlowerMaterial,
            SavedLayout.Flowers) ||
        !HasRenderOnlyPolicy(
            FlowerInstances,
            PlantingCullStartDistanceCm,
            PlantingCullEndDistanceCm,
            GrassWpoDisableDistanceCm))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_INVALID_PLANTING_PRESENTATION");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R29_VEGETATION_VALID actorContractVersion=1 r28WorldPositionsAndCensusPreserved=true grassInstances=%d modeledBladeMeshVariants=%d grassProfileVariantBuckets=%d meshVariantSelectionWeightsPercent=50,30,20 exactRealizedVariantRatioClaimed=false normalResponseEndMeters=%.1f roughnessResponseEndMeters=%.1f materialVisibilityEndMeters=90 treeInstances=%d treeMorphologyCues=%d treeRoster=spreading,dome,highFork,columnar,palm originalTreeMaterialsPreserved=true sourceActorsModified=false sourceAssetsModified=false mutuallyExclusiveWithR28RenderOwner=true visualCaptureAccepted=false captureRevalidationRequired=true appearanceOnly=true collision=false navigation=false sensorAuthority=false rfAuthority=false geospatialAuthority=false surveyClaim=false botanicalClaim=false seasonalClaim=false"),
        GrassInstanceCount,
        GrassVariantCount,
        GrassBucketCount,
        NormalResponseEndDistanceCm / 100.0,
        RoughnessResponseEndDistanceCm / 100.0,
        TreeInstanceCount,
        TreeMorphologyCount);
    return true;
}

const FString& ATRIADIstanaExploreV5DR29VegetationActor::ExpectedClaimLabel()
{
    return R29ClaimLabel;
}

const FName& ATRIADIstanaExploreV5DR29VegetationActor::ExpectedActorTag()
{
    return ActorTag;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::ExpectedGrassVariantCount()
{
    return GrassVariantCount;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::ExpectedGrassProfileCount()
{
    return GrassProfileCount;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::ExpectedGrassBucketCount()
{
    return GrassBucketCount;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::ExpectedGrassInstanceCount()
{
    return GrassInstanceCount;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::ExpectedTreeMorphologyCount()
{
    return TreeMorphologyCount;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::ExpectedTreeInstanceCount()
{
    return TreeInstanceCount;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::
    ExpectedNormalResponseEndDistanceCm()
{
    return NormalResponseEndDistanceCm;
}

int32 ATRIADIstanaExploreV5DR29VegetationActor::
    ExpectedRoughnessResponseEndDistanceCm()
{
    return RoughnessResponseEndDistanceCm;
}
