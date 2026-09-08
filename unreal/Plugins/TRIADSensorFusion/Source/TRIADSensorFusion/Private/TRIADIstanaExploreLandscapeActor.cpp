#include "TRIADIstanaExploreLandscapeActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr double CentimetersPerMeter = 100.0;
constexpr float InnerTreeMinimumRadiusMeters = 90.0f;
constexpr int32 ExpectedTreeCount = 36;
constexpr int32 ExpectedShrubACount = 304;
constexpr int32 ExpectedShrubBCount = 152;
constexpr int32 ExpectedHedgeCount = 124;
constexpr int32 ExpectedGroundcoverCount = 228;
constexpr int32 ExpectedGrassCount = 2325;

const TArray<FVector2D>& BroadleafFramePositionsMeters()
{
    static const TArray<FVector2D> Positions = {
        {-118.0, 18.0}, {-105.0, 28.0}, {-91.0, 39.0}, {-122.0, 51.0},
        {-101.0, 62.0}, {-84.0, 75.0}, {-116.0, 88.0}, {-96.0, 101.0},
        {-128.0, 119.0}, {-105.0, 132.0}, {-82.0, 145.0}, {-134.0, 157.0},
        {-93.0, 169.0}, {-121.0, 184.0}, {-103.0, 201.0}, {-139.0, 219.0},
        {-112.0, 236.0}, {-87.0, 224.0},
        {114.0, 16.0}, {98.0, 31.0}, {126.0, 43.0}, {88.0, 57.0},
        {111.0, 71.0}, {132.0, 86.0}, {94.0, 99.0}, {119.0, 114.0},
        {85.0, 129.0}, {127.0, 143.0}, {101.0, 158.0}, {137.0, 176.0},
        {92.0, 188.0}, {116.0, 204.0}, {143.0, 222.0}, {104.0, 237.0},
        {82.0, 211.0}, {128.0, 252.0},
    };
    return Positions;
}

bool IgnoresAllChannels(const UPrimitiveComponent* Component)
{
    return Component &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore);
}
}

ATRIADIstanaExploreLandscapeActor::ATRIADIstanaExploreLandscapeActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ExploreLandscapeRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    BroadleafTreeInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MatureBroadleafFrame"));
    ShrubInstancesA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LayeredShrubsA"));
    ShrubInstancesB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LayeredShrubsB"));
    HedgeInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ClippedHedges"));
    GroundcoverInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FoundationGroundcover"));
    NearGrassInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("NearFieldTurfDetail"));
    TreeTrunkPawnBlockers = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HiddenTreeTrunkPawnBlockers"));

    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        BroadleafTreeInstances, ShrubInstancesA, ShrubInstancesB,
        HedgeInstances, GroundcoverInstances, NearGrassInstances,
        TreeTrunkPawnBlockers};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }

    ConfigureVisualInstances(BroadleafTreeInstances, 25000, 180000, true);
    BroadleafTreeInstances->ForcedLodModel = 0;
    BroadleafTreeInstances->bOverrideMinLOD = true;
    BroadleafTreeInstances->MinLOD = 1;
    ConfigureVisualInstances(ShrubInstancesA, 8000, 65000, true);
    ConfigureVisualInstances(ShrubInstancesB, 8000, 65000, true);
    ConfigureVisualInstances(HedgeInstances, 8000, 65000, true);
    ConfigureVisualInstances(GroundcoverInstances, 5000, 45000, true);
    ConfigureVisualInstances(NearGrassInstances, 3500, 15000, false);

    TreeTrunkPawnBlockers->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TreeTrunkPawnBlockers->SetCollisionResponseToAllChannels(ECR_Ignore);
    TreeTrunkPawnBlockers->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    TreeTrunkPawnBlockers->SetVisibility(false, true);
    TreeTrunkPawnBlockers->SetHiddenInGame(true, true);
    TreeTrunkPawnBlockers->SetCastShadow(false);
}

void ATRIADIstanaExploreLandscapeActor::ConfigureVisualInstances(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 StartCullDistance,
    int32 EndCullDistance,
    bool bCastShadow)
{
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetCullDistances(StartCullDistance, EndCullDistance);
    Component->SetCastShadow(bCastShadow);
}

double ATRIADIstanaExploreLandscapeActor::TerrainHeightMeters(double X, double Y)
{
    const double Radius = FMath::Sqrt(X * X + Y * Y);
    return 0.72 * FMath::Sin(X / 185.0) +
        0.48 * FMath::Cos(Y / 230.0) +
        0.22 * FMath::Sin((X + Y) / 97.0) +
        0.0000011 * Radius * Radius - 0.48;
}

bool ATRIADIstanaExploreLandscapeActor::IsFiniteMesh(const UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return false;
    }
    const FVector Extent = Mesh->GetBounds().BoxExtent;
    return !Extent.ContainsNaN() && Extent.GetMin() > 0.5;
}

bool ATRIADIstanaExploreLandscapeActor::ConfigureExploreAssets(
    UStaticMesh* InBroadleafTree,
    UStaticMesh* InShrubA,
    UStaticMesh* InShrubB,
    UStaticMesh* InGroundcover,
    UStaticMesh* InGrassClump,
    UStaticMesh* InTrunkCollisionProxy,
    FString& OutError)
{
    if (!IsFiniteMesh(InBroadleafTree) || !IsFiniteMesh(InShrubA) ||
        !IsFiniteMesh(InShrubB) || !IsFiniteMesh(InGroundcover) ||
        !IsFiniteMesh(InGrassClump) || !IsFiniteMesh(InTrunkCollisionProxy))
    {
        OutError = TEXT("All six Explore V1 vegetation/collision meshes require non-zero 3D bounds.");
        return false;
    }
    BroadleafTreeInstances->SetStaticMesh(InBroadleafTree);
    ShrubInstancesA->SetStaticMesh(InShrubA);
    ShrubInstancesB->SetStaticMesh(InShrubB);
    HedgeInstances->SetStaticMesh(InShrubA);
    GroundcoverInstances->SetStaticMesh(InGroundcover);
    NearGrassInstances->SetStaticMesh(InGrassClump);
    TreeTrunkPawnBlockers->SetStaticMesh(InTrunkCollisionProxy);
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreLandscapeActor::RecordPreservedDistantContext(
    const UStaticMeshComponent* Component,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh() ||
        Component->GetStaticMesh()->GetPathName() !=
            TEXT("/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_OSMContextBuildings.SM_IstanaPublicView_OSMContextBuildings"))
    {
        OutError = TEXT("The exact V5 OSM/HDB context mesh is absent before Explore preservation.");
        return false;
    }
    PreservedDistantContextMeshPath = Component->GetStaticMesh()->GetPathName();
    PreservedDistantContextMaterialPaths.Reset();
    for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material)
        {
            OutError = TEXT("The V5 OSM/HDB context has a null material slot.");
            return false;
        }
        PreservedDistantContextMaterialPaths.Add(Material->GetPathName());
    }
    if (PreservedDistantContextMaterialPaths.IsEmpty())
    {
        OutError = TEXT("The V5 OSM/HDB context has no material slots to preserve.");
        return false;
    }
    PreservedDistantContextRelativeTransform = Component->GetRelativeTransform();
    PreservedDistantContextWorldTransform = Component->GetComponentTransform();
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreLandscapeActor::ValidatePreservedDistantContext(
    const UStaticMeshComponent* Component,
    FString& OutError) const
{
    if (!Component || !Component->GetStaticMesh() ||
        PreservedDistantContextMeshPath.IsEmpty() ||
        Component->GetStaticMesh()->GetPathName() != PreservedDistantContextMeshPath ||
        !Component->GetRelativeTransform().Equals(
            PreservedDistantContextRelativeTransform, 0.001f) ||
        !Component->GetComponentTransform().Equals(
            PreservedDistantContextWorldTransform, 0.001f) ||
        !Component->IsVisible() || Component->bHiddenInGame ||
        !Component->IsActive() || !Component->bAutoActivate ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetGenerateOverlapEvents() || !IgnoresAllChannels(Component) ||
        Component->GetNumMaterials() != PreservedDistantContextMaterialPaths.Num())
    {
        OutError = TEXT("The inherited distant OSM/HDB component no longer matches its persisted V5 mesh/transform/state record.");
        return false;
    }
    for (int32 Index = 0; Index < PreservedDistantContextMaterialPaths.Num(); ++Index)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material || Material->GetPathName() !=
                PreservedDistantContextMaterialPaths[Index])
        {
            OutError = TEXT("An inherited distant OSM/HDB material slot changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreLandscapeActor::ClearAllInstances()
{
    BroadleafTreeInstances->ClearInstances();
    ShrubInstancesA->ClearInstances();
    ShrubInstancesB->ClearInstances();
    HedgeInstances->ClearInstances();
    GroundcoverInstances->ClearInstances();
    NearGrassInstances->ClearInstances();
    TreeTrunkPawnBlockers->ClearInstances();
}

bool ATRIADIstanaExploreLandscapeActor::PopulateDeterministicLandscape(
    FString& OutError)
{
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Required = {
        BroadleafTreeInstances, ShrubInstancesA, ShrubInstancesB,
        HedgeInstances, GroundcoverInstances, NearGrassInstances,
        TreeTrunkPawnBlockers};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Required)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            OutError = TEXT("Explore V1 assets must be configured before deterministic planting is populated.");
            return false;
        }
    }

    ClearAllInstances();
    FRandomStream Random(DeterministicPlacementSeed);

    for (const FVector2D& Position : BroadleafFramePositionsMeters())
    {
        const float Scale = Random.FRandRange(0.82f, 1.13f);
        const float Yaw = Random.FRandRange(0.0f, 360.0f);
        const double GroundZ = TerrainHeightMeters(Position.X, Position.Y);
        BroadleafTreeInstances->AddInstance(FTransform(
            FRotator(0.0f, Yaw, 0.0f),
            FVector(Position.X, Position.Y, GroundZ) * CentimetersPerMeter,
            FVector(Scale)));

        // Engine Cylinder is 1 m high with 0.5 m radius.  Keep this proxy
        // hidden and conservative: it is only for comfortable pawn movement.
        TreeTrunkPawnBlockers->AddInstance(FTransform(
            FRotator::ZeroRotator,
            FVector(Position.X, Position.Y, GroundZ + 5.5) * CentimetersPerMeter,
            FVector(1.25f * Scale, 1.25f * Scale, 11.0f * Scale)));
    }

    // Deep paired beds: balanced at a distance, deliberately not clone-mirrored.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Row = 0; Row < 16; ++Row)
        {
            for (int32 Column = 0; Column < 12; ++Column)
            {
                const float UnsignedX = 19.0f + Column * 3.15f +
                    Random.FRandRange(-0.45f, 0.45f);
                const float X = Side * UnsignedX;
                const float Y = 36.0f + Row * 3.15f +
                    Random.FRandRange(-0.50f, 0.50f) + SideIndex * 0.37f;
                const double Z = TerrainHeightMeters(X, Y);
                UHierarchicalInstancedStaticMeshComponent* Target =
                    ((Row + Column + SideIndex) % 3 == 0)
                    ? ShrubInstancesB
                    : ShrubInstancesA;
                const float Scale = Random.FRandRange(0.72f, 1.18f);
                Target->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(Scale)));
            }
        }
    }

    // Fill the two inherited 28 m x 18 m planting beds around y=88 m so
    // their old exposed-soil boxes read as planted terraces in Explore.
    FRandomStream FormalBedRandom(0x0B3D2024);
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Row = 0; Row < 3; ++Row)
        {
            for (int32 Column = 0; Column < 12; ++Column)
            {
                const float X = Side * (21.0f + Column * 2.35f +
                    FormalBedRandom.FRandRange(-0.24f, 0.24f));
                const float Y = 83.0f + Row * 5.0f +
                    FormalBedRandom.FRandRange(-0.34f, 0.34f);
                const double Z = TerrainHeightMeters(X, Y) + 0.24;
                UHierarchicalInstancedStaticMeshComponent* Target =
                    ((Row + Column + SideIndex) % 3 == 0)
                    ? ShrubInstancesB
                    : ShrubInstancesA;
                Target->AddInstance(FTransform(
                    FRotator(
                        0.0f,
                        FormalBedRandom.FRandRange(0.0f, 360.0f),
                        0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(FormalBedRandom.FRandRange(0.75f, 1.12f))));
            }
        }
    }

    // Low clipped borders around the paired beds.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Index = 0; Index < 31; ++Index)
        {
            const float X = Side * (19.0f + Index * 1.2f);
            for (const float Y : {34.2f, 85.8f})
            {
                const double Z = TerrainHeightMeters(X, Y);
                HedgeInstances->AddInstance(FTransform(
                    FRotator(0.0f, Side < 0.0f ? 180.0f : 0.0f, 0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(0.55f, 0.42f, 0.52f)));
            }
        }
    }

    // Foundation foliage stays low and leaves the main entrance open.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Index = 0; Index < 24; ++Index)
        {
            const float X = Side * (15.0f + Index * 1.85f);
            const float Y = 25.8f + Random.FRandRange(-0.35f, 0.35f);
            const double Z = TerrainHeightMeters(X, Y);
            GroundcoverInstances->AddInstance(FTransform(
                FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                FVector(X, Y, Z) * CentimetersPerMeter,
                FVector(Random.FRandRange(0.42f, 0.68f))));
        }
    }
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Row = 0; Row < 5; ++Row)
        {
            for (int32 Column = 0; Column < 18; ++Column)
            {
                const float X = Side * (18.0f + Column * 2.1f +
                    Random.FRandRange(-0.28f, 0.28f));
                const float Y = 31.0f + Row * 11.6f +
                    Random.FRandRange(-0.35f, 0.35f);
                const double Z = TerrainHeightMeters(X, Y);
                GroundcoverInstances->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(Random.FRandRange(0.35f, 0.58f))));
            }
        }
    }

    // Near-field blades are intentionally absent from the fountain, the
    // ceremonial centerline and immediately under the building frontage.
    for (int32 XIndex = -28; XIndex <= 28; ++XIndex)
    {
        for (int32 YIndex = 0; YIndex <= 48; ++YIndex)
        {
            const float X = XIndex * 2.45f + Random.FRandRange(-0.34f, 0.34f);
            const float Y = 29.0f + YIndex * 2.65f +
                Random.FRandRange(-0.34f, 0.34f);
            const bool bInsideCeremonialAxis =
                FMath::Abs(X) < ClearCeremonialAxisHalfWidthMeters && Y < 122.0f;
            const bool bInsideFountain =
                FVector2D(X, Y - 95.0f).SizeSquared() < FMath::Square(17.0f);
            if (bInsideCeremonialAxis || bInsideFountain)
            {
                continue;
            }
            const double Z = TerrainHeightMeters(X, Y);
            NearGrassInstances->AddInstance(FTransform(
                FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                FVector(X, Y, Z + 0.015) * CentimetersPerMeter,
                FVector(Random.FRandRange(0.82f, 1.18f))));
        }
    }

    FString Report;
    if (!ValidateExploreLandscape(Report))
    {
        OutError = Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreLandscapeActor::ValidateVisualComponent(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    int32 ExpectedInstances,
    const TCHAR* Label,
    FString& OutError) const
{
    if (!Component || !Component->GetStaticMesh() ||
        Component->GetInstanceCount() != ExpectedInstances ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetGenerateOverlapEvents() || !IgnoresAllChannels(Component))
    {
        OutError = FString::Printf(
            TEXT("Explore landscape component '%s' is absent, has a changed exact census, or is not visual-only (expected=%d, actual=%d)."),
            Label,
            ExpectedInstances,
            Component ? Component->GetInstanceCount() : -1);
        return false;
    }
    if (Component == BroadleafTreeInstances &&
        (Component->ForcedLodModel != 0 || !Component->bOverrideMinLOD ||
            Component->MinLOD != 1 ||
            Component->GetStaticMesh()->GetMinLODIdx() != 1))
    {
        OutError = TEXT("The broadleaf HISM can select non-runtime source LOD0.");
        return false;
    }
    return true;
}

bool ATRIADIstanaExploreLandscapeActor::ValidateExploreLandscape(
    FString& OutReport) const
{
    FString Error;
    const int32 TreeCount = BroadleafTreeInstances
        ? BroadleafTreeInstances->GetInstanceCount()
        : 0;
    if (ClaimLabel != TEXT("ISTANA_LIKE_PUBLIC_REFERENCE_LANDSCAPE_APPROXIMATION_NOT_BOTANICAL_INVENTORY_NOT_SURVEY_CONTROLLED") ||
        bExactSpeciesOrCultivarsClaimed || bExactIndividualTreePlacementClaimed ||
        bVegetationUsedForSensorTruth ||
        DeterministicPlacementSeed != 0x1757A6A5 ||
        !FMath::IsNearlyEqual(ClearCeremonialAxisHalfWidthMeters, 16.0f, 0.0001f) ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001f) ||
        !ValidateVisualComponent(ShrubInstancesA, ExpectedShrubACount, TEXT("ShrubA"), Error) ||
        !ValidateVisualComponent(ShrubInstancesB, ExpectedShrubBCount, TEXT("ShrubB"), Error) ||
        !ValidateVisualComponent(HedgeInstances, ExpectedHedgeCount, TEXT("Hedge"), Error) ||
        !ValidateVisualComponent(GroundcoverInstances, ExpectedGroundcoverCount, TEXT("Groundcover"), Error) ||
        !ValidateVisualComponent(NearGrassInstances, ExpectedGrassCount, TEXT("NearGrass"), Error) ||
        !ValidateVisualComponent(BroadleafTreeInstances, ExpectedTreeCount, TEXT("BroadleafTree"), Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_LANDSCAPE_INVALID: ") + Error;
        return false;
    }

    if (!TreeTrunkPawnBlockers || !TreeTrunkPawnBlockers->GetStaticMesh() ||
        TreeTrunkPawnBlockers->GetInstanceCount() != TreeCount ||
        TreeTrunkPawnBlockers->GetCollisionEnabled() != ECollisionEnabled::QueryOnly ||
        TreeTrunkPawnBlockers->GetGenerateOverlapEvents() ||
        TreeTrunkPawnBlockers->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block ||
        TreeTrunkPawnBlockers->IsVisible() || !TreeTrunkPawnBlockers->bHiddenInGame)
    {
        OutReport = TEXT("ISTANA_EXPLORE_LANDSCAPE_INVALID: hidden trunk blockers are not exact Pawn-only collision proxies.");
        return false;
    }
    FCollisionResponseContainer ExpectedPawnOnlyResponses(ECR_Ignore);
    ExpectedPawnOnlyResponses.SetResponse(ECC_Pawn, ECR_Block);
    if (TreeTrunkPawnBlockers->GetCollisionResponseToChannels() !=
        ExpectedPawnOnlyResponses)
    {
        OutReport = TEXT("ISTANA_EXPLORE_LANDSCAPE_INVALID: hidden trunk blockers are not exact Pawn-only query collision.");
        return false;
    }

    for (int32 Index = 0; Index < TreeCount; ++Index)
    {
        FTransform TreeTransform;
        FTransform BlockerTransform;
        if (!BroadleafTreeInstances->GetInstanceTransform(
                Index, TreeTransform, false) ||
            !TreeTrunkPawnBlockers->GetInstanceTransform(
                Index, BlockerTransform, false) ||
            TreeTransform.ContainsNaN() || BlockerTransform.ContainsNaN() ||
            TreeTransform.GetTranslation().Size2D() <
                InnerTreeMinimumRadiusMeters * CentimetersPerMeter)
        {
            OutReport = TEXT("ISTANA_EXPLORE_LANDSCAPE_INVALID: a broadleaf frame tree is invalid or intrudes into the formal inner lawn.");
            return false;
        }
        const FVector2D TreeXY(TreeTransform.GetTranslation());
        const FVector2D BlockerXY(BlockerTransform.GetTranslation());
        if (!TreeXY.Equals(BlockerXY, 0.01))
        {
            OutReport = TEXT("ISTANA_EXPLORE_LANDSCAPE_INVALID: a hidden trunk blocker no longer aligns with its visible tree.");
            return false;
        }
    }

    for (int32 Index = 0; Index < ExpectedGrassCount; ++Index)
    {
        FTransform Transform;
        if (!NearGrassInstances->GetInstanceTransform(Index, Transform, false) ||
            Transform.ContainsNaN())
        {
            OutReport = TEXT("ISTANA_EXPLORE_LANDSCAPE_INVALID: a near-grass transform is missing or non-finite.");
            return false;
        }
        const FVector LocationMeters = Transform.GetTranslation() / CentimetersPerMeter;
        const bool bIntrudesIntoCeremonialAxis =
            FMath::Abs(LocationMeters.X) < ClearCeremonialAxisHalfWidthMeters &&
            LocationMeters.Y < 122.0;
        const bool bIntrudesIntoFountain =
            FVector2D(LocationMeters.X, LocationMeters.Y - 95.0).SizeSquared() <
            FMath::Square(17.0);
        if (bIntrudesIntoCeremonialAxis || bIntrudesIntoFountain)
        {
            OutReport = TEXT("ISTANA_EXPLORE_LANDSCAPE_INVALID: near grass intrudes into the ceremonial axis or fountain clearance.");
            return false;
        }
    }

    OutReport = FString::Printf(
        TEXT("Validated additive Explore V1 qualitative landscape seed 0x1757A6A5: %d mature broadleaf trees with Pawn-only hidden trunk proxies, %d layered shrubs, %d clipped hedge modules, %d groundcover clumps and %d near-field grass clusters; the formal ceremonial axis and fountain are clear, all visible vegetation is non-colliding/non-sensor truth, and exact species, cultivar, inventory, survey and April-2024 planting claims remain false."),
        TreeCount,
        ShrubInstancesA->GetInstanceCount() + ShrubInstancesB->GetInstanceCount(),
        HedgeInstances->GetInstanceCount(),
        GroundcoverInstances->GetInstanceCount(),
        NearGrassInstances->GetInstanceCount());
    return true;
}
