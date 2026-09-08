#include "TRIADIstanaExploreV2LandscapeActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr double CentimetersPerMeter = 100.0;

constexpr int32 ExpectedNearUmbrellaBroadleafCount = 48;
constexpr int32 ExpectedNearColumnarBroadleafCount = 64;
constexpr int32 ExpectedNearDomeBroadleafCount = 42;
constexpr int32 ExpectedNearPalmCount = 6;
constexpr int32 ExpectedOuterUmbrellaBroadleafCount = 224;
constexpr int32 ExpectedOuterColumnarBroadleafCount = 168;
constexpr int32 ExpectedOuterDomeBroadleafCount = 140;
constexpr int32 ExpectedOuterPalmCount = 28;
constexpr int32 ExpectedOuterTreeCount = 560;
constexpr int32 ExpectedUmbrellaBroadleafCount =
    ExpectedNearUmbrellaBroadleafCount + ExpectedOuterUmbrellaBroadleafCount;
constexpr int32 ExpectedColumnarBroadleafCount =
    ExpectedNearColumnarBroadleafCount + ExpectedOuterColumnarBroadleafCount;
constexpr int32 ExpectedDomeBroadleafCount =
    ExpectedNearDomeBroadleafCount + ExpectedOuterDomeBroadleafCount;
constexpr int32 ExpectedPalmCount =
    ExpectedNearPalmCount + ExpectedOuterPalmCount;
constexpr int32 ExpectedShrubACount = 512;
constexpr int32 ExpectedShrubBCount = 320;
constexpr int32 ExpectedHedgeCount = 192;
constexpr int32 ExpectedGroundcoverCount = 768;
constexpr int32 ExpectedNearTurfCount = 6144;
constexpr int32 ExpectedMeadowSedgeCount = 1536;
constexpr int32 ExpectedTreeCount =
    ExpectedUmbrellaBroadleafCount + ExpectedColumnarBroadleafCount +
    ExpectedDomeBroadleafCount + ExpectedPalmCount;
constexpr int32 BroadleafWpoDisableDistanceCm = 60000;
constexpr int32 UnderstoryWpoDisableDistanceCm = 30000;
constexpr int32 GrassWpoDisableDistanceCm = 14000;

static_assert(ExpectedTreeCount == 720);
static_assert(
    ExpectedOuterUmbrellaBroadleafCount +
        ExpectedOuterColumnarBroadleafCount +
        ExpectedOuterDomeBroadleafCount + ExpectedOuterPalmCount ==
    ExpectedOuterTreeCount);

const FName WindStrengthParameter(TEXT("TRIAD_WindStrengthCm"));
const FName WindSpeedParameter(TEXT("TRIAD_WindSpeed"));
const FName WindDirectionParameter(TEXT("TRIAD_WindDirection"));

bool IgnoresAllChannels(const UPrimitiveComponent* Component)
{
    return Component &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore);
}

float CyclicFraction(int32 Index, int32 Multiplier, int32 Denominator)
{
    check(Denominator > 0);
    return static_cast<float>((Index * Multiplier) % (Denominator + 1)) /
        static_cast<float>(Denominator);
}
}

ATRIADIstanaExploreV2LandscapeActor::ATRIADIstanaExploreV2LandscapeActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ExploreV2LandscapeRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    UmbrellaBroadleafInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("UmbrellaBroadleafFrame"));
    ColumnarBroadleafInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ColumnarBroadleafAllee"));
    DomeBroadleafInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DomeSpecimenBroadleaf"));
    PalmInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SparseFormalPalms"));
    ShrubInstancesA = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LayeredShrubsA"));
    ShrubInstancesB = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("LayeredShrubsB"));
    HedgeInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ClippedHedges"));
    GroundcoverInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FoundationGroundcover"));
    NearTurfInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DenseNearFieldTurf"));
    MeadowSedgeInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("OuterMeadowAndSedge"));
    TreeTrunkPawnBlockers = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HiddenTreeTrunkPawnBlockers"));

    const TArray<UHierarchicalInstancedStaticMeshComponent*> Components = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances,
        PalmInstances,
        ShrubInstancesA,
        ShrubInstancesB,
        HedgeInstances,
        GroundcoverInstances,
        NearTurfInstances,
        MeadowSedgeInstances,
        TreeTrunkPawnBlockers};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }

    ConfigureVisualInstances(UmbrellaBroadleafInstances, 22000, 185000, true);
    ConfigureVisualInstances(ColumnarBroadleafInstances, 18000, 155000, true);
    ConfigureVisualInstances(DomeBroadleafInstances, 20000, 165000, true);
    for (UHierarchicalInstancedStaticMeshComponent* Component :
        {UmbrellaBroadleafInstances.Get(),
         ColumnarBroadleafInstances.Get(),
         DomeBroadleafInstances.Get()})
    {
        Component->ForcedLodModel = 0;
        Component->bOverrideMinLOD = true;
        Component->MinLOD = 1;
        Component->SetWorldPositionOffsetDisableDistance(
            BroadleafWpoDisableDistanceCm);
    }
    ConfigureVisualInstances(PalmInstances, 18000, 150000, true);
    ConfigureVisualInstances(ShrubInstancesA, 6500, 65000, true);
    ConfigureVisualInstances(ShrubInstancesB, 6500, 65000, true);
    ConfigureVisualInstances(HedgeInstances, 7000, 70000, true);
    ConfigureVisualInstances(GroundcoverInstances, 4500, 45000, true);
    ConfigureVisualInstances(NearTurfInstances, 2500, 18000, false);
    ConfigureVisualInstances(MeadowSedgeInstances, 5000, 52000, false);

    // Palm materials are intentionally static in V2; a zero distance preserves
    // their authored appearance.  Animated understory and grass stop evaluating
    // WPO beyond these bounded distances even though their geometry can remain
    // visible until the independently configured HISM cull distance.
    PalmInstances->SetWorldPositionOffsetDisableDistance(0);
    for (UHierarchicalInstancedStaticMeshComponent* Component :
        {ShrubInstancesA.Get(),
         ShrubInstancesB.Get(),
         HedgeInstances.Get(),
         GroundcoverInstances.Get()})
    {
        Component->SetWorldPositionOffsetDisableDistance(
            UnderstoryWpoDisableDistanceCm);
    }
    NearTurfInstances->SetWorldPositionOffsetDisableDistance(
        GrassWpoDisableDistanceCm);
    MeadowSedgeInstances->SetWorldPositionOffsetDisableDistance(
        GrassWpoDisableDistanceCm);

    TreeTrunkPawnBlockers->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TreeTrunkPawnBlockers->SetCollisionResponseToAllChannels(ECR_Ignore);
    TreeTrunkPawnBlockers->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    TreeTrunkPawnBlockers->SetVisibility(false, true);
    TreeTrunkPawnBlockers->SetHiddenInGame(true, true);
    TreeTrunkPawnBlockers->SetCastShadow(false);

    GustRandom.Initialize(DeterministicPlacementSeed ^ 0x57494E44);
}

void ATRIADIstanaExploreV2LandscapeActor::ConfigureVisualInstances(
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

double ATRIADIstanaExploreV2LandscapeActor::TerrainHeightMeters(
    double X,
    double Y)
{
    const double Radius = FMath::Sqrt(X * X + Y * Y);
    return 0.72 * FMath::Sin(X / 185.0) +
        0.48 * FMath::Cos(Y / 230.0) +
        0.22 * FMath::Sin((X + Y) / 97.0) +
        0.0000011 * Radius * Radius - 0.48;
}

bool ATRIADIstanaExploreV2LandscapeActor::IsFiniteMesh(
    const UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return false;
    }
    const FVector Extent = Mesh->GetBounds().BoxExtent;
    return !Extent.ContainsNaN() && Extent.GetMin() > 0.5;
}

bool ATRIADIstanaExploreV2LandscapeActor::ConfigureExploreV2Assets(
    UStaticMesh* InUmbrellaBroadleaf,
    UStaticMesh* InColumnarBroadleaf,
    UStaticMesh* InDomeBroadleaf,
    UStaticMesh* InPalm,
    UStaticMesh* InShrubA,
    UStaticMesh* InShrubB,
    UStaticMesh* InHedge,
    UStaticMesh* InGroundcover,
    UStaticMesh* InNearTurf,
    UStaticMesh* InMeadowSedge,
    UStaticMesh* InTrunkCollisionProxy,
    FString& OutError)
{
    const TArray<UStaticMesh*> RequiredMeshes = {
        InUmbrellaBroadleaf,
        InColumnarBroadleaf,
        InDomeBroadleaf,
        InPalm,
        InShrubA,
        InShrubB,
        InHedge,
        InGroundcover,
        InNearTurf,
        InMeadowSedge,
        InTrunkCollisionProxy};
    for (const UStaticMesh* Mesh : RequiredMeshes)
    {
        if (!IsFiniteMesh(Mesh))
        {
            OutError = TEXT("All eleven Explore V2 vegetation/collision meshes require finite, non-zero 3D bounds.");
            return false;
        }
    }

    UmbrellaBroadleafInstances->SetStaticMesh(InUmbrellaBroadleaf);
    ColumnarBroadleafInstances->SetStaticMesh(InColumnarBroadleaf);
    DomeBroadleafInstances->SetStaticMesh(InDomeBroadleaf);
    PalmInstances->SetStaticMesh(InPalm);
    ShrubInstancesA->SetStaticMesh(InShrubA);
    ShrubInstancesB->SetStaticMesh(InShrubB);
    HedgeInstances->SetStaticMesh(InHedge);
    GroundcoverInstances->SetStaticMesh(InGroundcover);
    NearTurfInstances->SetStaticMesh(InNearTurf);
    MeadowSedgeInstances->SetStaticMesh(InMeadowSedge);
    TreeTrunkPawnBlockers->SetStaticMesh(InTrunkCollisionProxy);

    WindMaterialInstances.Reset();
    TreeTrunkWindMaterial = nullptr;
    TreeBranchWindMaterial = nullptr;
    TreeLeafWindMaterial = nullptr;
    GrassWindMaterial = nullptr;
    TreeTrunkWindSlotIndex = INDEX_NONE;
    TreeBranchWindSlotIndex = INDEX_NONE;
    TreeLeafWindSlotIndex = INDEX_NONE;
    GrassWindSlotIndex = INDEX_NONE;
    bWindMaterialsConfigured = false;
    PreservedOuterTreeWorldTransforms.Reset();
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV2LandscapeActor::ConfigureWindMaterials(
    UMaterialInterface* InTreeTrunkWindMaterial,
    int32 InTreeTrunkSlotIndex,
    UMaterialInterface* InTreeBranchWindMaterial,
    int32 InTreeBranchSlotIndex,
    UMaterialInterface* InTreeLeafWindMaterial,
    int32 InTreeLeafSlotIndex,
    UMaterialInterface* InGrassWindMaterial,
    int32 InGrassWindSlotIndex,
    FString& OutError)
{
    if (!InTreeTrunkWindMaterial || !InTreeBranchWindMaterial ||
        !InTreeLeafWindMaterial || !InGrassWindMaterial ||
        InTreeTrunkSlotIndex < 0 || InTreeBranchSlotIndex < 0 ||
        InTreeLeafSlotIndex < 0 || InGrassWindSlotIndex < 0 ||
        InTreeTrunkSlotIndex == InTreeBranchSlotIndex ||
        InTreeTrunkSlotIndex == InTreeLeafSlotIndex ||
        InTreeBranchSlotIndex == InTreeLeafSlotIndex ||
        InTreeTrunkWindMaterial == InTreeBranchWindMaterial ||
        InTreeTrunkWindMaterial == InTreeLeafWindMaterial ||
        InTreeBranchWindMaterial == InTreeLeafWindMaterial ||
        InGrassWindMaterial == InTreeTrunkWindMaterial ||
        InGrassWindMaterial == InTreeBranchWindMaterial ||
        InGrassWindMaterial == InTreeLeafWindMaterial)
    {
        OutError = TEXT("Explore V2 requires four distinct wind materials, three distinct non-negative broadleaf slots, and one non-negative grass slot.");
        return false;
    }

    const TArray<UHierarchicalInstancedStaticMeshComponent*> WindTrees = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : WindTrees)
    {
        if (!Component || !Component->GetStaticMesh() ||
            InTreeTrunkSlotIndex >= Component->GetNumMaterials() ||
            InTreeBranchSlotIndex >= Component->GetNumMaterials() ||
            InTreeLeafSlotIndex >= Component->GetNumMaterials())
        {
            OutError = TEXT("Every animated broadleaf mesh must expose the configured V2 trunk, branch and leaf material slots.");
            return false;
        }
    }
    if (!NearTurfInstances || !NearTurfInstances->GetStaticMesh() ||
        InGrassWindSlotIndex >= NearTurfInstances->GetNumMaterials() ||
        !MeadowSedgeInstances || !MeadowSedgeInstances->GetStaticMesh() ||
        InGrassWindSlotIndex >= MeadowSedgeInstances->GetNumMaterials())
    {
        OutError = TEXT("Near-turf and meadow/sedge meshes must each expose the exact configured grass material slot.");
        return false;
    }

    TreeTrunkWindMaterial = InTreeTrunkWindMaterial;
    TreeBranchWindMaterial = InTreeBranchWindMaterial;
    TreeLeafWindMaterial = InTreeLeafWindMaterial;
    GrassWindMaterial = InGrassWindMaterial;
    TreeTrunkWindSlotIndex = InTreeTrunkSlotIndex;
    TreeBranchWindSlotIndex = InTreeBranchSlotIndex;
    TreeLeafWindSlotIndex = InTreeLeafSlotIndex;
    GrassWindSlotIndex = InGrassWindSlotIndex;
    WindMaterialInstances.Reset();
    for (UHierarchicalInstancedStaticMeshComponent* Component : WindTrees)
    {
        Component->SetMaterial(TreeTrunkWindSlotIndex, TreeTrunkWindMaterial);
        Component->SetMaterial(TreeBranchWindSlotIndex, TreeBranchWindMaterial);
        Component->SetMaterial(TreeLeafWindSlotIndex, TreeLeafWindMaterial);
    }
    NearTurfInstances->SetMaterial(GrassWindSlotIndex, GrassWindMaterial);
    MeadowSedgeInstances->SetMaterial(GrassWindSlotIndex, GrassWindMaterial);
    bWindMaterialsConfigured = true;

    if (!ValidateWindConfiguration(OutError))
    {
        bWindMaterialsConfigured = false;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV2LandscapeActor::RecordPreservedDistantContext(
    const UStaticMeshComponent* Component,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh() ||
        Component->GetStaticMesh()->GetPathName() !=
            TEXT("/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_OSMContextBuildings.SM_IstanaPublicView_OSMContextBuildings"))
    {
        OutError = TEXT("The exact inherited V5 OSM/HDB context mesh is absent before Explore V2 preservation.");
        return false;
    }

    PreservedDistantContextMeshPath = Component->GetStaticMesh()->GetPathName();
    PreservedDistantContextMaterialPaths.Reset();
    for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material)
        {
            OutError = TEXT("The inherited OSM/HDB context has a null material slot.");
            return false;
        }
        PreservedDistantContextMaterialPaths.Add(Material->GetPathName());
    }
    if (PreservedDistantContextMaterialPaths.IsEmpty())
    {
        OutError = TEXT("The inherited OSM/HDB context has no material slots to preserve.");
        return false;
    }
    PreservedDistantContextRelativeTransform = Component->GetRelativeTransform();
    PreservedDistantContextWorldTransform = Component->GetComponentTransform();
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV2LandscapeActor::ValidatePreservedDistantContext(
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
        OutError = TEXT("The inherited distant OSM/HDB component no longer matches its persisted mesh/transform/state record.");
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

void ATRIADIstanaExploreV2LandscapeActor::ClearAllInstances()
{
    UmbrellaBroadleafInstances->ClearInstances();
    ColumnarBroadleafInstances->ClearInstances();
    DomeBroadleafInstances->ClearInstances();
    PalmInstances->ClearInstances();
    ShrubInstancesA->ClearInstances();
    ShrubInstancesB->ClearInstances();
    HedgeInstances->ClearInstances();
    GroundcoverInstances->ClearInstances();
    NearTurfInstances->ClearInstances();
    MeadowSedgeInstances->ClearInstances();
    TreeTrunkPawnBlockers->ClearInstances();
}

bool ATRIADIstanaExploreV2LandscapeActor::PopulateDeterministicLandscape(
    const TArray<FTransform>& OuterTreeTransforms,
    FString& OutError)
{
    if (!GetActorTransform().Equals(FTransform::Identity, 0.001f) ||
        OuterTreeTransforms.Num() != ExpectedOuterTreeCount)
    {
        OutError = TEXT("Explore V2 population requires an identity actor and exactly 560 inherited outer-tree world transforms.");
        return false;
    }

    const auto IntrudesProtectedAreaMeters = [this](const FVector& LocationMeters)
    {
        const bool bInsideAxis =
            FMath::Abs(LocationMeters.X) < ClearCeremonialAxisHalfWidthMeters &&
            LocationMeters.Y >= 0.0 && LocationMeters.Y < 138.0;
        const bool bInsideFountain = FVector2D(
            LocationMeters.X - FountainCenterMeters.X,
            LocationMeters.Y - FountainCenterMeters.Y).SizeSquared() <
            FMath::Square(FountainClearanceRadiusMeters);
        return bInsideAxis || bInsideFountain;
    };

    TArray<FTransform> SortedOuterTransforms = OuterTreeTransforms;
    for (const FTransform& Transform : SortedOuterTransforms)
    {
        const FVector LocationMeters =
            Transform.GetTranslation() / CentimetersPerMeter;
        const FVector Scale = Transform.GetScale3D();
        const double RadiusMeters = LocationMeters.Size2D();
        if (Transform.ContainsNaN() || !Transform.GetRotation().IsNormalized() ||
            Scale.GetMin() <= 0.001 || RadiusMeters < 250.0 ||
            RadiusMeters > 1000.0 ||
            IntrudesProtectedAreaMeters(LocationMeters))
        {
            OutError = TEXT("Every inherited outer-tree transform must be finite, positively scaled, normalized, outside 250 m, inside 1 km and clear of the formal axis/fountain.");
            return false;
        }
    }
    SortedOuterTransforms.Sort([](const FTransform& A, const FTransform& B)
    {
        const FVector LA = A.GetTranslation();
        const FVector LB = B.GetTranslation();
        const double AngleA = FMath::Atan2(LA.Y, LA.X);
        const double AngleB = FMath::Atan2(LB.Y, LB.X);
        if (AngleA != AngleB)
        {
            return AngleA < AngleB;
        }
        const double RadiusA = LA.SizeSquared2D();
        const double RadiusB = LB.SizeSquared2D();
        if (RadiusA != RadiusB)
        {
            return RadiusA < RadiusB;
        }
        if (LA.Z != LB.Z)
        {
            return LA.Z < LB.Z;
        }
        const FVector SA = A.GetScale3D();
        const FVector SB = B.GetScale3D();
        if (SA.X != SB.X)
        {
            return SA.X < SB.X;
        }
        if (SA.Y != SB.Y)
        {
            return SA.Y < SB.Y;
        }
        if (SA.Z != SB.Z)
        {
            return SA.Z < SB.Z;
        }
        const FQuat RA = A.GetRotation();
        const FQuat RB = B.GetRotation();
        if (RA.X != RB.X)
        {
            return RA.X < RB.X;
        }
        if (RA.Y != RB.Y)
        {
            return RA.Y < RB.Y;
        }
        if (RA.Z != RB.Z)
        {
            return RA.Z < RB.Z;
        }
        return RA.W < RB.W;
    });
    // Canonicalize first, then deterministically mix the exact position roster.
    // Slicing the polar-angle order directly would create implausible contiguous
    // umbrella/columnar/dome/palm sectors around the estate.
    FRandomStream OuterMixRandom(DeterministicPlacementSeed ^ 0x4D495845);
    for (int32 Index = SortedOuterTransforms.Num() - 1; Index > 0; --Index)
    {
        SortedOuterTransforms.Swap(
            Index,
            OuterMixRandom.RandRange(0, Index));
    }

    const TArray<UHierarchicalInstancedStaticMeshComponent*> Required = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances,
        PalmInstances,
        ShrubInstancesA,
        ShrubInstancesB,
        HedgeInstances,
        GroundcoverInstances,
        NearTurfInstances,
        MeadowSedgeInstances,
        TreeTrunkPawnBlockers};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Required)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            OutError = TEXT("Explore V2 assets must be configured before deterministic planting is populated.");
            return false;
        }
    }
    if (!ValidateWindConfiguration(OutError))
    {
        return false;
    }

    for (UHierarchicalInstancedStaticMeshComponent* Component : Required)
    {
        // Population adds thousands of deterministic instances. Defer the
        // HISM cluster tree only for this mutation window, then restore the
        // engine's non-serialized true runtime state before validation/save.
        Component->bAutoRebuildTreeOnInstanceChanges = false;
    }
    ClearAllInstances();
    PreservedOuterTreeWorldTransforms = SortedOuterTransforms;
    FRandomStream Random(DeterministicPlacementSeed);

    auto AddTree = [this](
        UHierarchicalInstancedStaticMeshComponent* Component,
        const FVector2D& PositionMeters,
        float YawDegrees,
        const FVector& VisualScale,
        float BlockerHeightMeters,
        float BlockerRadiusScale)
    {
        const double GroundZ = TerrainHeightMeters(PositionMeters.X, PositionMeters.Y);
        Component->AddInstance(FTransform(
            FRotator(0.0f, YawDegrees, 0.0f),
            FVector(PositionMeters.X, PositionMeters.Y, GroundZ) *
                CentimetersPerMeter,
            VisualScale));

        // Engine Cylinder is 1 m high and 1 m wide.  These conservative,
        // invisible proxies affect Pawn queries only, never rendered/sensor truth.
        TreeTrunkPawnBlockers->AddInstance(FTransform(
            FRotator::ZeroRotator,
            FVector(
                PositionMeters.X,
                PositionMeters.Y,
                GroundZ + BlockerHeightMeters * 0.5) * CentimetersPerMeter,
            FVector(
                BlockerRadiusScale,
                BlockerRadiusScale,
                BlockerHeightMeters)));
    };

    auto AddOuterTrees = [this, &SortedOuterTransforms](
        UHierarchicalInstancedStaticMeshComponent* Component,
        int32 SourceStartIndex,
        int32 SourceCount,
        float MinimumScaleXY,
        float MaximumScaleXY,
        float MinimumScaleZ,
        float MaximumScaleZ,
        float BaseBlockerHeightMeters,
        float BaseBlockerRadiusScale)
    {
        for (int32 Offset = 0; Offset < SourceCount; ++Offset)
        {
            const FTransform& Source =
                SortedOuterTransforms[SourceStartIndex + Offset];
            const float Fraction = CyclicFraction(
                Offset + SourceStartIndex, 37, 100);
            const float ScaleXY = FMath::Lerp(
                MinimumScaleXY, MaximumScaleXY, Fraction);
            const float ScaleZ = FMath::Lerp(
                MinimumScaleZ,
                MaximumScaleZ,
                CyclicFraction(Offset + SourceStartIndex, 53, 100));
            const float SourceYaw = Source.GetRotation().Rotator().Yaw;
            const FTransform VisualTransform(
                FRotator(0.0f, SourceYaw, 0.0f),
                Source.GetTranslation(),
                FVector(
                    ScaleXY,
                    ScaleXY * FMath::Lerp(
                        0.92f,
                        1.08f,
                        CyclicFraction(Offset + SourceStartIndex, 29, 100)),
                    ScaleZ));
            Component->AddInstance(VisualTransform, true);

            const float BlockerHeightMeters =
                BaseBlockerHeightMeters * ScaleZ;
            const FVector SourceLocation = Source.GetTranslation();
            TreeTrunkPawnBlockers->AddInstance(FTransform(
                FRotator::ZeroRotator,
                SourceLocation + FVector(
                    0.0,
                    0.0,
                    BlockerHeightMeters * 0.5 * CentimetersPerMeter),
                FVector(
                    BaseBlockerRadiusScale * ScaleXY,
                    BaseBlockerRadiusScale * ScaleXY,
                    BlockerHeightMeters)),
                true);
        }
    };

    // Large umbrella-canopy frame, extending well beyond the formal inner lawn.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Index = 0; Index < 24; ++Index)
        {
            const int32 Lane = Index % 3;
            const int32 Row = Index / 3;
            const FVector2D Position(
                Side * (101.0f + Lane * 32.0f + Random.FRandRange(-4.5f, 4.5f)),
                14.0f + Row * 40.0f + Lane * 6.0f +
                    Random.FRandRange(-4.0f, 4.0f));
            const float ScaleZ = 1.00f + 0.58f * CyclicFraction(Index, 7, 10);
            const float ScaleXY = 0.82f + 0.50f * CyclicFraction(Index, 5, 12);
            AddTree(
                UmbrellaBroadleafInstances,
                Position,
                Random.FRandRange(0.0f, 360.0f),
                FVector(ScaleXY, ScaleXY * Random.FRandRange(0.92f, 1.08f), ScaleZ),
                12.0f * ScaleZ,
                1.35f * ScaleXY);
        }
    }
    AddOuterTrees(
        UmbrellaBroadleafInstances,
        0,
        ExpectedOuterUmbrellaBroadleafCount,
        0.82f,
        1.35f,
        1.00f,
        1.65f,
        12.0f,
        1.35f);

    // Tall columnar allee rhythm.  It frames, but never enters, the central axis.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Index = 0; Index < 32; ++Index)
        {
            const int32 Lane = Index % 2;
            const int32 Row = Index / 2;
            const FVector2D Position(
                Side * (51.0f + Lane * 18.0f + Random.FRandRange(-1.7f, 1.7f)),
                34.0f + Row * 13.5f + Lane * 4.0f +
                    Random.FRandRange(-1.4f, 1.4f));
            const float ScaleZ = 1.24f + 0.66f * CyclicFraction(Index, 5, 8);
            const float ScaleXY = 0.68f + 0.38f * CyclicFraction(Index, 7, 10);
            AddTree(
                ColumnarBroadleafInstances,
                Position,
                Random.FRandRange(0.0f, 360.0f),
                FVector(ScaleXY, ScaleXY * Random.FRandRange(0.94f, 1.06f), ScaleZ),
                13.5f * ScaleZ,
                1.10f * ScaleXY);
        }
    }
    AddOuterTrees(
        ColumnarBroadleafInstances,
        ExpectedOuterUmbrellaBroadleafCount,
        ExpectedOuterColumnarBroadleafCount,
        0.65f,
        1.05f,
        1.20f,
        1.90f,
        13.5f,
        1.10f);

    // Irregular dome specimens break up the allee and outer-canopy silhouette.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Index = 0; Index < 21; ++Index)
        {
            const int32 Lane = Index % 3;
            const int32 Row = Index / 3;
            const FVector2D Position(
                Side * (79.0f + Lane * 28.0f + Random.FRandRange(-3.5f, 3.5f)),
                29.0f + Row * 42.0f + (Lane - 1) * 5.0f +
                    Random.FRandRange(-3.5f, 3.5f));
            const float ScaleZ = 0.95f + 0.61f * CyclicFraction(Index, 9, 10);
            const float ScaleXY = 0.78f + 0.62f * CyclicFraction(Index, 4, 12);
            AddTree(
                DomeBroadleafInstances,
                Position,
                Random.FRandRange(0.0f, 360.0f),
                FVector(ScaleXY, ScaleXY * Random.FRandRange(0.88f, 1.12f), ScaleZ),
                10.5f * ScaleZ,
                1.25f * ScaleXY);
        }
    }
    AddOuterTrees(
        DomeBroadleafInstances,
        ExpectedOuterUmbrellaBroadleafCount +
            ExpectedOuterColumnarBroadleafCount,
        ExpectedOuterDomeBroadleafCount,
        0.80f,
        1.40f,
        0.95f,
        1.60f,
        10.5f,
        1.25f);

    // Palms remain sparse accents rather than replacing the broadleaf canopy.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Index = 0; Index < 3; ++Index)
        {
            const FVector2D Position(
                Side * (75.0f + (Index % 3) * 31.0f +
                    Random.FRandRange(-2.5f, 2.5f)),
                45.0f + Index * 35.0f + Random.FRandRange(-2.5f, 2.5f));
            const float ScaleZ = 1.12f + 0.58f * CyclicFraction(Index, 1, 2);
            const float ScaleXY = 0.82f + 0.38f * CyclicFraction(Index, 1, 2);
            AddTree(
                PalmInstances,
                Position,
                Random.FRandRange(0.0f, 360.0f),
                FVector(ScaleXY, ScaleXY, ScaleZ),
                15.0f * ScaleZ,
                0.92f * ScaleXY);
        }
    }
    AddOuterTrees(
        PalmInstances,
        ExpectedOuterUmbrellaBroadleafCount +
            ExpectedOuterColumnarBroadleafCount +
            ExpectedOuterDomeBroadleafCount,
        ExpectedOuterPalmCount,
        0.90f,
        1.25f,
        1.12f,
        1.70f,
        15.0f,
        0.92f);

    // Dense, staggered tropical shrub layers in paired formal beds.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Row = 0; Row < 16; ++Row)
        {
            for (int32 Column = 0; Column < 16; ++Column)
            {
                const float X = Side * (23.0f + Column * 2.75f +
                    Random.FRandRange(-0.42f, 0.42f));
                const float Y = 39.0f + Row * 4.8f +
                    Random.FRandRange(-0.52f, 0.52f) + SideIndex * 0.31f;
                const double Z = TerrainHeightMeters(X, Y);
                const float ScaleXY = Random.FRandRange(0.72f, 1.24f);
                ShrubInstancesA->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(ScaleXY, ScaleXY, Random.FRandRange(0.75f, 1.42f))));
            }
        }

        for (int32 Row = 0; Row < 10; ++Row)
        {
            for (int32 Column = 0; Column < 16; ++Column)
            {
                const float X = Side * (24.0f + Column * 2.65f +
                    Random.FRandRange(-0.45f, 0.45f));
                const float Y = 43.0f + Row * 7.4f +
                    Random.FRandRange(-0.58f, 0.58f) - SideIndex * 0.29f;
                const double Z = TerrainHeightMeters(X, Y);
                const float ScaleXY = Random.FRandRange(0.70f, 1.20f);
                ShrubInstancesB->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(ScaleXY, ScaleXY, Random.FRandRange(0.82f, 1.55f))));
            }
        }
    }

    // Four continuous clipped borders around the paired beds.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (const float Y : {36.0f, 116.0f})
        {
            for (int32 Segment = 0; Segment < 48; ++Segment)
            {
                const float X = Side * (19.0f + Segment * 1.15f);
                const double Z = TerrainHeightMeters(X, Y);
                HedgeInstances->AddInstance(FTransform(
                    FRotator(0.0f, Side < 0.0f ? 180.0f : 0.0f, 0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(0.58f, 0.46f, 0.60f)));
            }
        }
    }

    // Low foundation foliage leaves the entrance, fountain and sightline open.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Row = 0; Row < 12; ++Row)
        {
            for (int32 Column = 0; Column < 32; ++Column)
            {
                const float X = Side * (19.5f + Column * 1.75f +
                    Random.FRandRange(-0.28f, 0.28f));
                const float Y = 28.0f + Row * 7.2f +
                    Random.FRandRange(-0.35f, 0.35f);
                const double Z = TerrainHeightMeters(X, Y);
                const float Scale = Random.FRandRange(0.38f, 0.72f);
                GroundcoverInstances->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                    FVector(X, Y, Z) * CentimetersPerMeter,
                    FVector(Scale, Scale, Random.FRandRange(0.35f, 0.68f))));
            }
        }
    }

    // Dense close turf on both sides of the ceremonial axis.  Material-level
    // striping/micro-detail supplies distant coverage; these clusters are the
    // near-camera parallax layer and are therefore tightly culled.
    const double NearTurfLocalHeightCm =
        NearTurfInstances->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.0;
    const double MeadowLocalHeightCm =
        MeadowSedgeInstances->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.0;
    if (!FMath::IsFinite(NearTurfLocalHeightCm) ||
        NearTurfLocalHeightCm <= 0.0 ||
        !FMath::IsFinite(MeadowLocalHeightCm) ||
        MeadowLocalHeightCm <= 0.0)
    {
        OutError = TEXT("Explore V2 grass meshes require finite positive local Z bounds before metric-height planting.");
        return false;
    }
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Row = 0; Row < 48; ++Row)
        {
            for (int32 Column = 0; Column < 64; ++Column)
            {
                const float X = Side * (18.75f + Column * 1.05f +
                    Random.FRandRange(-0.16f, 0.16f));
                const float Y = 28.0f + Row * 2.30f +
                    Random.FRandRange(-0.20f, 0.20f);
                const double Z = TerrainHeightMeters(X, Y);
                const double TargetHeightCm = Random.FRandRange(3.0f, 6.0f);
                NearTurfInstances->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                    FVector(X, Y, Z + 0.012) * CentimetersPerMeter,
                    FVector(
                        Random.FRandRange(0.76f, 1.22f),
                        Random.FRandRange(0.76f, 1.22f),
                        TargetHeightCm / NearTurfLocalHeightCm)));
            }
        }
    }

    // Looser outer meadow/sedge pockets bridge the formal lawn into the mature
    // tree wall without pretending to identify real individual plants.
    for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
    {
        const float Side = SideIndex == 0 ? -1.0f : 1.0f;
        for (int32 Row = 0; Row < 24; ++Row)
        {
            for (int32 Column = 0; Column < 32; ++Column)
            {
                const float X = Side * (90.0f + Column * 3.20f +
                    Random.FRandRange(-0.85f, 0.85f));
                const float Y = 12.0f + Row * 13.0f +
                    Random.FRandRange(-1.25f, 1.25f);
                const double Z = TerrainHeightMeters(X, Y);
                const float ScaleXY = Random.FRandRange(0.72f, 1.38f);
                const double TargetHeightCm = Random.FRandRange(18.0f, 80.0f);
                MeadowSedgeInstances->AddInstance(FTransform(
                    FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f),
                    FVector(X, Y, Z + 0.018) * CentimetersPerMeter,
                    FVector(
                        ScaleXY,
                        ScaleXY,
                        TargetHeightCm / MeadowLocalHeightCm)));
            }
        }
    }

    for (UHierarchicalInstancedStaticMeshComponent* Component : Required)
    {
        Component->bAutoRebuildTreeOnInstanceChanges = true;
        if (!Component->BuildTreeIfOutdated(false, true))
        {
            OutError = TEXT("Explore V2 could not build a final deterministic HISM cluster tree.");
            return false;
        }
    }

    FString Report;
    if (!ValidateExploreV2Landscape(Report))
    {
        OutError = Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV2LandscapeActor::ValidateVisualComponent(
    const UHierarchicalInstancedStaticMeshComponent* Component,
    int32 ExpectedInstances,
    const TCHAR* Label,
    FString& OutError) const
{
    if (!Component || !Component->GetStaticMesh() ||
        Component->GetInstanceCount() != ExpectedInstances ||
        !Component->bAutoRebuildTreeOnInstanceChanges ||
        !Component->GetRelativeTransform().Equals(FTransform::Identity, 0.001f) ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetGenerateOverlapEvents() || !IgnoresAllChannels(Component))
    {
        OutError = FString::Printf(
            TEXT("Explore V2 component '%s' is absent, has a changed exact census, or is not visual-only (expected=%d, actual=%d)."),
            Label,
            ExpectedInstances,
            Component ? Component->GetInstanceCount() : -1);
        return false;
    }
    const bool bBroadleaf =
        Component == UmbrellaBroadleafInstances ||
        Component == ColumnarBroadleafInstances ||
        Component == DomeBroadleafInstances;
    if (bBroadleaf &&
        (Component->ForcedLodModel != 0 || !Component->bOverrideMinLOD ||
         Component->MinLOD != 1 ||
         Component->GetStaticMesh()->GetMinLODIdx() != 1))
    {
        OutError = FString::Printf(
            TEXT("Explore V2 broadleaf component '%s' can select non-runtime source LOD0."),
            Label);
        return false;
    }

    int32 ExpectedWpoDisableDistanceCm = 0;
    if (bBroadleaf)
    {
        ExpectedWpoDisableDistanceCm = BroadleafWpoDisableDistanceCm;
    }
    else if (Component == ShrubInstancesA || Component == ShrubInstancesB ||
             Component == HedgeInstances || Component == GroundcoverInstances)
    {
        ExpectedWpoDisableDistanceCm = UnderstoryWpoDisableDistanceCm;
    }
    else if (Component == NearTurfInstances || Component == MeadowSedgeInstances)
    {
        ExpectedWpoDisableDistanceCm = GrassWpoDisableDistanceCm;
    }
    if (Component->WorldPositionOffsetDisableDistance !=
        ExpectedWpoDisableDistanceCm)
    {
        OutError = FString::Printf(
            TEXT("Explore V2 component '%s' changed its exact WPO-disable distance (expected=%d cm, actual=%d cm)."),
            Label,
            ExpectedWpoDisableDistanceCm,
            Component->WorldPositionOffsetDisableDistance);
        return false;
    }
    return true;
}

bool ATRIADIstanaExploreV2LandscapeActor::ValidateWindConfiguration(
    FString& OutError) const
{
    const bool bDirectionValid =
        FMath::IsFinite(PrevailingWindDirection.X) &&
        FMath::IsFinite(PrevailingWindDirection.Y) &&
        PrevailingWindDirection.SizeSquared() > 0.01f;
    const bool bMaterialsAndSlotsExact =
        TreeTrunkWindMaterial && TreeBranchWindMaterial &&
        TreeLeafWindMaterial && GrassWindMaterial &&
        TreeTrunkWindMaterial != TreeBranchWindMaterial &&
        TreeTrunkWindMaterial != TreeLeafWindMaterial &&
        TreeBranchWindMaterial != TreeLeafWindMaterial &&
        GrassWindMaterial != TreeTrunkWindMaterial &&
        GrassWindMaterial != TreeBranchWindMaterial &&
        GrassWindMaterial != TreeLeafWindMaterial &&
        TreeTrunkWindSlotIndex >= 0 && TreeBranchWindSlotIndex >= 0 &&
        TreeLeafWindSlotIndex >= 0 && GrassWindSlotIndex >= 0 &&
        TreeTrunkWindSlotIndex != TreeBranchWindSlotIndex &&
        TreeTrunkWindSlotIndex != TreeLeafWindSlotIndex &&
        TreeBranchWindSlotIndex != TreeLeafWindSlotIndex;
    if (!bWindMaterialsConfigured || !bMaterialsAndSlotsExact ||
        !bDirectionValid || !FMath::IsFinite(BaseWindStrengthCm) ||
        BaseWindStrengthCm < 0.0f || BaseWindStrengthCm > 30.0f ||
        !FMath::IsFinite(GustPeakStrengthCm) ||
        GustPeakStrengthCm <= BaseWindStrengthCm ||
        GustPeakStrengthCm > 150.0f || !FMath::IsFinite(WindSpeed) ||
        WindSpeed < 0.05f || WindSpeed > 10.0f ||
        !FMath::IsNearlyEqual(GustIntervalMinimumSeconds, 7.0f, 0.0001f) ||
        !FMath::IsNearlyEqual(GustIntervalMaximumSeconds, 14.0f, 0.0001f) ||
        !FMath::IsNearlyEqual(GustDurationMinimumSeconds, 1.5f, 0.0001f) ||
        !FMath::IsNearlyEqual(GustDurationMaximumSeconds, 3.0f, 0.0001f) ||
        !FMath::IsFinite(RecoveryFrequencyHz) ||
        !FMath::IsNearlyEqual(RecoveryFrequencyHz, 0.65f, 0.0001f) ||
        !FMath::IsFinite(RecoveryDampingRatio) ||
        !FMath::IsNearlyEqual(RecoveryDampingRatio, 0.32f, 0.0001f))
    {
        OutError = TEXT("Explore V2 wind materials/slots or exact 7-14 s interval, 1.5-3 s duration and 0.65 Hz/0.32 underdamped configuration changed.");
        return false;
    }

    const TArray<const UHierarchicalInstancedStaticMeshComponent*> WindTrees = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : WindTrees)
    {
        if (!Component || !Component->GetStaticMesh() ||
            TreeTrunkWindSlotIndex >= Component->GetNumMaterials() ||
            TreeBranchWindSlotIndex >= Component->GetNumMaterials() ||
            TreeLeafWindSlotIndex >= Component->GetNumMaterials())
        {
            OutError = TEXT("An animated broadleaf no longer exposes its exact trunk/branch/leaf wind slots.");
            return false;
        }
    }
    if (!NearTurfInstances || !NearTurfInstances->GetStaticMesh() ||
        GrassWindSlotIndex >= NearTurfInstances->GetNumMaterials() ||
        !MeadowSedgeInstances || !MeadowSedgeInstances->GetStaticMesh() ||
        GrassWindSlotIndex >= MeadowSedgeInstances->GetNumMaterials())
    {
        OutError = TEXT("A wind-animated grass component no longer exposes its exact configured slot.");
        return false;
    }

    if (WindMaterialInstances.IsEmpty())
    {
        for (const UHierarchicalInstancedStaticMeshComponent* Component : WindTrees)
        {
            if (Component->GetMaterial(TreeTrunkWindSlotIndex) !=
                    TreeTrunkWindMaterial ||
                Component->GetMaterial(TreeBranchWindSlotIndex) !=
                    TreeBranchWindMaterial ||
                Component->GetMaterial(TreeLeafWindSlotIndex) !=
                    TreeLeafWindMaterial)
            {
                OutError = TEXT("A saved broadleaf override no longer uses its exact V2 trunk/branch/leaf wind materials.");
                return false;
            }
        }
        if (NearTurfInstances->GetMaterial(GrassWindSlotIndex) !=
                GrassWindMaterial ||
            MeadowSedgeInstances->GetMaterial(GrassWindSlotIndex) !=
                GrassWindMaterial)
        {
            OutError = TEXT("A saved grass override no longer uses the configured V2 wind material.");
            return false;
        }
    }
    else
    {
        if (WindMaterialInstances.Num() != 11)
        {
            OutError = TEXT("Runtime Explore V2 wind must own exactly eleven per-component/material-slot MIDs.");
            return false;
        }
        for (const UMaterialInstanceDynamic* MID : WindMaterialInstances)
        {
            if (!MID)
            {
                OutError = TEXT("A runtime Explore V2 wind MID is null.");
                return false;
            }
        }
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV2LandscapeActor::CreateRuntimeWindMaterialInstances(
    FString& OutError)
{
    if (!ValidateWindConfiguration(OutError))
    {
        return false;
    }
    if (!WindMaterialInstances.IsEmpty())
    {
        OutError.Reset();
        return true;
    }

    const TArray<UHierarchicalInstancedStaticMeshComponent*> WindTrees = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances};
    const TArray<UMaterialInterface*> TreeMaterials = {
        TreeTrunkWindMaterial,
        TreeBranchWindMaterial,
        TreeLeafWindMaterial};
    const TArray<int32> TreeSlots = {
        TreeTrunkWindSlotIndex,
        TreeBranchWindSlotIndex,
        TreeLeafWindSlotIndex};
    for (UHierarchicalInstancedStaticMeshComponent* Component : WindTrees)
    {
        for (int32 RoleIndex = 0; RoleIndex < TreeMaterials.Num(); ++RoleIndex)
        {
            UMaterialInstanceDynamic* MID =
                Component->CreateDynamicMaterialInstance(
                    TreeSlots[RoleIndex],
                    TreeMaterials[RoleIndex]);
            if (!MID)
            {
                OutError = TEXT("Failed to create a runtime MID for a V2 broadleaf trunk/branch/leaf slot.");
                WindMaterialInstances.Reset();
                return false;
            }
            WindMaterialInstances.Add(MID);
        }
    }

    for (UHierarchicalInstancedStaticMeshComponent* Component :
        {NearTurfInstances.Get(), MeadowSedgeInstances.Get()})
    {
        UMaterialInstanceDynamic* MID = Component->CreateDynamicMaterialInstance(
            GrassWindSlotIndex,
            GrassWindMaterial);
        if (!MID)
        {
            OutError = TEXT("Failed to create a runtime MID for a V2 grass component.");
            WindMaterialInstances.Reset();
            return false;
        }
        WindMaterialInstances.Add(MID);
    }

    if (WindMaterialInstances.Num() != 11)
    {
        OutError = TEXT("Explore V2 runtime wind did not create the exact eleven required MIDs.");
        WindMaterialInstances.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV2LandscapeActor::IsWindRuntimeActive() const
{
    const float DirectionMagnitudeSquared = CurrentWindDirection.SizeSquared();
    const float MaximumStrengthCm =
        FMath::Max(150.0f, ActiveGustPeakStrengthCm * 1.25f);
    if (!HasActorBegunPlay() || WindMaterialInstances.Num() != 11 ||
        !FMath::IsFinite(CurrentWindStrengthCm) ||
        CurrentWindStrengthCm < 0.0f ||
        CurrentWindStrengthCm > MaximumStrengthCm + 0.01f ||
        !FMath::IsFinite(WindStrengthVelocityCmPerSecond) ||
        FMath::Abs(WindStrengthVelocityCmPerSecond) > 1000.0f ||
        !FMath::IsFinite(CurrentWindDirection.X) ||
        !FMath::IsFinite(CurrentWindDirection.Y) ||
        DirectionMagnitudeSquared < 0.16f ||
        DirectionMagnitudeSquared > 2.56f ||
        !FMath::IsFinite(WindDirectionVelocityPerSecond.X) ||
        !FMath::IsFinite(WindDirectionVelocityPerSecond.Y) ||
        WindDirectionVelocityPerSecond.SizeSquared() > 25.0f ||
        !FMath::IsFinite(ActiveGustDirection.X) ||
        !FMath::IsFinite(ActiveGustDirection.Y) ||
        ActiveGustDirection.SizeSquared() < 0.99f ||
        ActiveGustDirection.SizeSquared() > 1.01f ||
        !FMath::IsFinite(TimeUntilNextGustSeconds) ||
        !FMath::IsFinite(ActiveGustElapsedSeconds) ||
        !FMath::IsFinite(ActiveGustDurationSeconds) ||
        ActiveGustDurationSeconds < GustDurationMinimumSeconds ||
        ActiveGustDurationSeconds > GustDurationMaximumSeconds ||
        !FMath::IsFinite(ActiveGustPeakStrengthCm))
    {
        return false;
    }

    const TArray<const UHierarchicalInstancedStaticMeshComponent*> TreeComponents = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances};
    const TArray<int32> TreeSlots = {
        TreeTrunkWindSlotIndex,
        TreeBranchWindSlotIndex,
        TreeLeafWindSlotIndex};
    for (int32 ComponentIndex = 0;
         ComponentIndex < TreeComponents.Num();
         ++ComponentIndex)
    {
        for (int32 RoleIndex = 0; RoleIndex < TreeSlots.Num(); ++RoleIndex)
        {
            const int32 MIDIndex = ComponentIndex * 3 + RoleIndex;
            const UMaterialInstanceDynamic* MID =
                WindMaterialInstances[MIDIndex];
            if (!TreeComponents[ComponentIndex] || !MID ||
                TreeComponents[ComponentIndex]->GetMaterial(
                    TreeSlots[RoleIndex]) != MID)
            {
                return false;
            }
        }
    }
    if (!NearTurfInstances || !MeadowSedgeInstances ||
        !WindMaterialInstances[9] || !WindMaterialInstances[10] ||
        NearTurfInstances->GetMaterial(GrassWindSlotIndex) !=
            WindMaterialInstances[9] ||
        MeadowSedgeInstances->GetMaterial(GrassWindSlotIndex) !=
            WindMaterialInstances[10])
    {
        return false;
    }
    return true;
}

void ATRIADIstanaExploreV2LandscapeActor::ApplyWindParameters()
{
    FVector2D Direction = CurrentWindDirection.GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        Direction = PrevailingWindDirection.GetSafeNormal();
    }
    const FLinearColor DirectionValue(Direction.X, Direction.Y, 0.0f, 0.0f);
    for (UMaterialInstanceDynamic* MID : WindMaterialInstances)
    {
        if (!MID)
        {
            continue;
        }
        MID->SetScalarParameterValue(WindStrengthParameter, CurrentWindStrengthCm);
        MID->SetScalarParameterValue(WindSpeedParameter, WindSpeed);
        MID->SetVectorParameterValue(WindDirectionParameter, DirectionValue);
    }
}

void ATRIADIstanaExploreV2LandscapeActor::BeginPlay()
{
    Super::BeginPlay();

    GustRandom.Initialize(DeterministicPlacementSeed ^ 0x57494E44);
    CurrentWindDirection = PrevailingWindDirection.GetSafeNormal();
    WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    ActiveGustDirection = CurrentWindDirection;
    CurrentWindStrengthCm = BaseWindStrengthCm;
    WindStrengthVelocityCmPerSecond = 0.0f;
    TimeUntilNextGustSeconds = GustRandom.FRandRange(
        GustIntervalMinimumSeconds,
        GustIntervalMaximumSeconds);
    ActiveGustElapsedSeconds = -1.0f;
    ActiveGustDurationSeconds = GustRandom.FRandRange(
        GustDurationMinimumSeconds,
        GustDurationMaximumSeconds);
    ActiveGustPeakStrengthCm = GustPeakStrengthCm;

    FString Error;
    if (!CreateRuntimeWindMaterialInstances(Error))
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("ISTANA_EXPLORE_V2_WIND_DISABLED: %s"),
            *Error);
        SetActorTickEnabled(false);
        return;
    }
    ApplyWindParameters();
}

void ATRIADIstanaExploreV2LandscapeActor::TriggerWindGust(
    float PeakStrengthCm)
{
    const float RequestedPeak = PeakStrengthCm >= 0.0f
        ? PeakStrengthCm
        : GustPeakStrengthCm * GustRandom.FRandRange(0.90f, 1.08f);
    ActiveGustPeakStrengthCm = FMath::Clamp(
        RequestedPeak,
        BaseWindStrengthCm + 1.0f,
        150.0f);
    ActiveGustElapsedSeconds = 0.0f;
    ActiveGustDurationSeconds = GustRandom.FRandRange(
        GustDurationMinimumSeconds,
        GustDurationMaximumSeconds);

    const FVector2D Prevailing = PrevailingWindDirection.GetSafeNormal();
    const float BaseAngle = FMath::Atan2(Prevailing.Y, Prevailing.X);
    const float DeflectionRadians = FMath::DegreesToRadians(
        GustRandom.FRandRange(-22.0f, 22.0f));
    ActiveGustDirection = FVector2D(
        FMath::Cos(BaseAngle + DeflectionRadians),
        FMath::Sin(BaseAngle + DeflectionRadians));
}

void ATRIADIstanaExploreV2LandscapeActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (WindMaterialInstances.Num() != 11 || !FMath::IsFinite(DeltaSeconds) ||
        DeltaSeconds <= 0.0f)
    {
        return;
    }

    const float ClampedDelta = FMath::Min(DeltaSeconds, 0.25f);
    float TargetStrengthCm = BaseWindStrengthCm;
    FVector2D TargetDirection = PrevailingWindDirection.GetSafeNormal();
    if (ActiveGustElapsedSeconds >= 0.0f)
    {
        ActiveGustElapsedSeconds += ClampedDelta;
        const float GustAlpha = FMath::Clamp(
            ActiveGustElapsedSeconds / ActiveGustDurationSeconds,
            0.0f,
            1.0f);
        if (GustAlpha >= 1.0f)
        {
            ActiveGustElapsedSeconds = -1.0f;
            TimeUntilNextGustSeconds = GustRandom.FRandRange(
                GustIntervalMinimumSeconds,
                GustIntervalMaximumSeconds);
        }
        else
        {
            // Smooth transient force.  The spring's retained velocity produces
            // the requested after-sway after this external force has ended.
            const float Pulse = FMath::Pow(
                FMath::Sin(PI * GustAlpha),
                0.72f);
            TargetStrengthCm = FMath::Lerp(
                BaseWindStrengthCm,
                ActiveGustPeakStrengthCm,
                Pulse);
            TargetDirection = ActiveGustDirection;
        }
    }
    else
    {
        TimeUntilNextGustSeconds -= ClampedDelta;
        if (TimeUntilNextGustSeconds <= 0.0f)
        {
            TriggerWindGust();
            TargetDirection = ActiveGustDirection;
        }
    }

    // Stable sub-stepped integration of an explicitly underdamped second-order
    // response.  It overshoots and settles instead of snapping back after gusts.
    const float AngularFrequency = 2.0f * PI * RecoveryFrequencyHz;
    float Remaining = ClampedDelta;
    while (Remaining > KINDA_SMALL_NUMBER)
    {
        const float Step = FMath::Min(Remaining, 1.0f / 60.0f);
        const float Acceleration =
            AngularFrequency * AngularFrequency *
                (TargetStrengthCm - CurrentWindStrengthCm) -
            2.0f * RecoveryDampingRatio * AngularFrequency *
                WindStrengthVelocityCmPerSecond;
        WindStrengthVelocityCmPerSecond += Acceleration * Step;
        CurrentWindStrengthCm += WindStrengthVelocityCmPerSecond * Step;

        const FVector2D DirectionAcceleration =
            AngularFrequency * AngularFrequency *
                (TargetDirection - CurrentWindDirection) -
            2.0f * RecoveryDampingRatio * AngularFrequency *
                WindDirectionVelocityPerSecond;
        WindDirectionVelocityPerSecond += DirectionAcceleration * Step;
        CurrentWindDirection += WindDirectionVelocityPerSecond * Step;
        Remaining -= Step;
    }
    CurrentWindStrengthCm = FMath::Clamp(
        CurrentWindStrengthCm,
        0.0f,
        FMath::Max(150.0f, ActiveGustPeakStrengthCm * 1.25f));
    WindStrengthVelocityCmPerSecond = FMath::Clamp(
        WindStrengthVelocityCmPerSecond,
        -1000.0f,
        1000.0f);
    const double DirectionMagnitude = CurrentWindDirection.Size();
    if (!FMath::IsFinite(DirectionMagnitude) || DirectionMagnitude < 0.4)
    {
        CurrentWindDirection = PrevailingWindDirection.GetSafeNormal();
        WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    }
    else if (DirectionMagnitude > 1.6)
    {
        CurrentWindDirection *= 1.6 / DirectionMagnitude;
    }
    const double DirectionVelocity = WindDirectionVelocityPerSecond.Size();
    if (!FMath::IsFinite(DirectionVelocity))
    {
        WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    }
    else if (DirectionVelocity > 5.0)
    {
        WindDirectionVelocityPerSecond *= 5.0 / DirectionVelocity;
    }
    ApplyWindParameters();
}

bool ATRIADIstanaExploreV2LandscapeActor::ValidateExploreV2Landscape(
    FString& OutReport) const
{
    FString Error;
    if (PreservedOuterTreeWorldTransforms.Num() != ExpectedOuterTreeCount)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: the exact 560-transform inherited outer-tree record is absent.");
        return false;
    }
    if (ClaimLabel != TEXT("ISTANA_PUBLIC_REFERENCE_EXPLORE_V2_QUALITATIVE_COMPOSITION_NOT_BOTANICAL_INVENTORY_NOT_SURVEY_CONTROLLED_NOT_ONE_TO_ONE_1KM_REPLICA") ||
        bExactSpeciesOrCultivarsClaimed ||
        bExactIndividualTreePlacementClaimed ||
        bOneToOneKilometerReplicaClaimed ||
        bVegetationUsedForSensorTruth ||
        !bPublicGroundsImagesUsedAsCompositionReferenceOnly ||
        DeterministicPlacementSeed != 0x2757A6A5 ||
        !FMath::IsNearlyEqual(ClearCeremonialAxisHalfWidthMeters, 17.0f, 0.0001f) ||
        !FountainCenterMeters.Equals(FVector2D(0.0f, 95.0f), 0.0001f) ||
        !FMath::IsNearlyEqual(FountainClearanceRadiusMeters, 18.0f, 0.0001f) ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001f) ||
        !ValidateVisualComponent(UmbrellaBroadleafInstances, ExpectedUmbrellaBroadleafCount, TEXT("UmbrellaBroadleaf"), Error) ||
        !ValidateVisualComponent(ColumnarBroadleafInstances, ExpectedColumnarBroadleafCount, TEXT("ColumnarBroadleaf"), Error) ||
        !ValidateVisualComponent(DomeBroadleafInstances, ExpectedDomeBroadleafCount, TEXT("DomeBroadleaf"), Error) ||
        !ValidateVisualComponent(PalmInstances, ExpectedPalmCount, TEXT("Palm"), Error) ||
        !ValidateVisualComponent(ShrubInstancesA, ExpectedShrubACount, TEXT("ShrubA"), Error) ||
        !ValidateVisualComponent(ShrubInstancesB, ExpectedShrubBCount, TEXT("ShrubB"), Error) ||
        !ValidateVisualComponent(HedgeInstances, ExpectedHedgeCount, TEXT("Hedge"), Error) ||
        !ValidateVisualComponent(GroundcoverInstances, ExpectedGroundcoverCount, TEXT("Groundcover"), Error) ||
        !ValidateVisualComponent(NearTurfInstances, ExpectedNearTurfCount, TEXT("NearTurf"), Error) ||
        !ValidateVisualComponent(MeadowSedgeInstances, ExpectedMeadowSedgeCount, TEXT("MeadowSedge"), Error) ||
        !ValidateWindConfiguration(Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: ") + Error;
        return false;
    }

    if (!TreeTrunkPawnBlockers || !TreeTrunkPawnBlockers->GetStaticMesh() ||
        TreeTrunkPawnBlockers->GetStaticMesh()->GetPathName() !=
            TEXT("/Engine/BasicShapes/Cylinder.Cylinder") ||
        TreeTrunkPawnBlockers->GetInstanceCount() != ExpectedTreeCount ||
        TreeTrunkPawnBlockers->GetCollisionEnabled() != ECollisionEnabled::QueryOnly ||
        TreeTrunkPawnBlockers->GetGenerateOverlapEvents() ||
        TreeTrunkPawnBlockers->GetCollisionResponseToChannel(ECC_Pawn) != ECR_Block ||
        TreeTrunkPawnBlockers->IsVisible() || !TreeTrunkPawnBlockers->bHiddenInGame)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: hidden trunk blockers are not the exact Pawn-only proxy census.");
        return false;
    }
    FCollisionResponseContainer ExpectedPawnOnlyResponses(ECR_Ignore);
    ExpectedPawnOnlyResponses.SetResponse(ECC_Pawn, ECR_Block);
    if (TreeTrunkPawnBlockers->GetCollisionResponseToChannels() !=
        ExpectedPawnOnlyResponses)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: hidden trunk blockers are not exact Pawn-only query collision.");
        return false;
    }

    const auto IntrudesProtectedArea = [this](const FVector& LocationCm)
    {
        const FVector LocationMeters = LocationCm / CentimetersPerMeter;
        const bool bInsideAxis =
            FMath::Abs(LocationMeters.X) < ClearCeremonialAxisHalfWidthMeters &&
            LocationMeters.Y >= 0.0 && LocationMeters.Y < 138.0;
        const bool bInsideFountain = FVector2D(
            LocationMeters.X - FountainCenterMeters.X,
            LocationMeters.Y - FountainCenterMeters.Y).SizeSquared() <
            FMath::Square(FountainClearanceRadiusMeters);
        return bInsideAxis || bInsideFountain;
    };

    const TArray<const UHierarchicalInstancedStaticMeshComponent*> VisibleComponents = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances,
        PalmInstances,
        ShrubInstancesA,
        ShrubInstancesB,
        HedgeInstances,
        GroundcoverInstances,
        NearTurfInstances,
        MeadowSedgeInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : VisibleComponents)
    {
        for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
        {
            FTransform Transform;
            if (!Component->GetInstanceTransform(Index, Transform, false) ||
                Transform.ContainsNaN() || IntrudesProtectedArea(Transform.GetTranslation()))
            {
                OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: visible vegetation is missing, non-finite, or intrudes into the formal axis/fountain clearance.");
                return false;
            }
        }
    }

    auto ValidateMetricPlantHeight = [](
        const UHierarchicalInstancedStaticMeshComponent* Component,
        double MinimumHeightCm,
        double MaximumHeightCm)
    {
        const double LocalHeightCm =
            Component->GetStaticMesh()->GetBounds().BoxExtent.Z * 2.0;
        if (!FMath::IsFinite(LocalHeightCm) || LocalHeightCm <= 0.0)
        {
            return false;
        }
        for (int32 Index = 0; Index < Component->GetInstanceCount(); ++Index)
        {
            FTransform Transform;
            if (!Component->GetInstanceTransform(Index, Transform, false))
            {
                return false;
            }
            const double WorldHeightCm =
                LocalHeightCm * FMath::Abs(Transform.GetScale3D().Z);
            if (!FMath::IsFinite(WorldHeightCm) ||
                WorldHeightCm < MinimumHeightCm - 0.01 ||
                WorldHeightCm > MaximumHeightCm + 0.01)
            {
                return false;
            }
        }
        return true;
    };
    if (!ValidateMetricPlantHeight(NearTurfInstances, 3.0, 6.0) ||
        !ValidateMetricPlantHeight(MeadowSedgeInstances, 18.0, 80.0))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: near turf must remain 3-6 cm and meadow/sedge 18-80 cm from exact local mesh bounds.");
        return false;
    }

    struct FOuterTreeRange
    {
        const UHierarchicalInstancedStaticMeshComponent* Component;
        int32 NearCount;
        int32 SourceStart;
        int32 Count;
    };
    const TArray<FOuterTreeRange> OuterRanges = {
        {UmbrellaBroadleafInstances,
         ExpectedNearUmbrellaBroadleafCount,
         0,
         ExpectedOuterUmbrellaBroadleafCount},
        {ColumnarBroadleafInstances,
         ExpectedNearColumnarBroadleafCount,
         ExpectedOuterUmbrellaBroadleafCount,
         ExpectedOuterColumnarBroadleafCount},
        {DomeBroadleafInstances,
         ExpectedNearDomeBroadleafCount,
         ExpectedOuterUmbrellaBroadleafCount +
             ExpectedOuterColumnarBroadleafCount,
         ExpectedOuterDomeBroadleafCount},
        {PalmInstances,
         ExpectedNearPalmCount,
         ExpectedOuterUmbrellaBroadleafCount +
             ExpectedOuterColumnarBroadleafCount +
             ExpectedOuterDomeBroadleafCount,
         ExpectedOuterPalmCount}};
    for (const FOuterTreeRange& Range : OuterRanges)
    {
        for (int32 Offset = 0; Offset < Range.Count; ++Offset)
        {
            const FTransform& Source =
                PreservedOuterTreeWorldTransforms[Range.SourceStart + Offset];
            FTransform Live;
            const FVector SourceMeters =
                Source.GetTranslation() / CentimetersPerMeter;
            const double RadiusMeters = SourceMeters.Size2D();
            if (!Range.Component->GetInstanceTransform(
                    Range.NearCount + Offset, Live, true) ||
                Live.ContainsNaN() || Source.ContainsNaN() ||
                !Live.GetTranslation().Equals(
                    Source.GetTranslation(), 0.05f) ||
                Source.GetScale3D().GetMin() <= 0.001 ||
                RadiusMeters < 250.0 || RadiusMeters > 1000.0 ||
                IntrudesProtectedArea(Source.GetTranslation()))
            {
                OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: an inherited outer-tree position changed, left its 250-1000 m band, or entered the protected formal axis.");
                return false;
            }
        }
    }

    const TArray<const UHierarchicalInstancedStaticMeshComponent*> TreeComponents = {
        UmbrellaBroadleafInstances,
        ColumnarBroadleafInstances,
        DomeBroadleafInstances,
        PalmInstances};
    const TArray<double> BlockerBaseHeightsMeters = {
        12.0, 13.5, 10.5, 15.0};
    const TArray<double> BlockerBaseRadiusScales = {
        1.35, 1.10, 1.25, 0.92};
    int32 BlockerIndex = 0;
    float MinimumTreeScaleZ = TNumericLimits<float>::Max();
    float MaximumTreeScaleZ = TNumericLimits<float>::Lowest();
    double MinimumTreeHeightMeters = TNumericLimits<double>::Max();
    double MaximumTreeHeightMeters = TNumericLimits<double>::Lowest();
    for (int32 ComponentIndex = 0;
         ComponentIndex < TreeComponents.Num();
         ++ComponentIndex)
    {
        const UHierarchicalInstancedStaticMeshComponent* TreeComponent =
            TreeComponents[ComponentIndex];
        const UStaticMesh* TreeMesh = TreeComponent->GetStaticMesh();
        const double LocalTreeHeightMeters = TreeMesh
            ? TreeMesh->GetBoundingBox().GetSize().Z / CentimetersPerMeter
            : 0.0;
        if (!FMath::IsFinite(LocalTreeHeightMeters) ||
            LocalTreeHeightMeters <= 0.0)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: a tree mesh has an empty/non-finite local height.");
            return false;
        }
        for (int32 TreeIndex = 0; TreeIndex < TreeComponent->GetInstanceCount(); ++TreeIndex)
        {
            FTransform TreeTransform;
            FTransform BlockerTransform;
            if (!TreeComponent->GetInstanceTransform(
                    TreeIndex, TreeTransform, false) ||
                !TreeTrunkPawnBlockers->GetInstanceTransform(
                    BlockerIndex, BlockerTransform, false) ||
                TreeTransform.ContainsNaN() || BlockerTransform.ContainsNaN())
            {
                OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: a tree or its hidden blocker transform is missing/non-finite.");
                return false;
            }
            const FVector TreeLocation = TreeTransform.GetTranslation();
            const FVector TreeScale = TreeTransform.GetScale3D();
            const double ExpectedBlockerHeightMeters =
                BlockerBaseHeightsMeters[ComponentIndex] * TreeScale.Z;
            const FVector ExpectedBlockerLocation =
                TreeLocation + FVector(
                    0.0,
                    0.0,
                    ExpectedBlockerHeightMeters *
                        0.5 * CentimetersPerMeter);
            const FVector ExpectedBlockerScale(
                BlockerBaseRadiusScales[ComponentIndex] * TreeScale.X,
                BlockerBaseRadiusScales[ComponentIndex] * TreeScale.X,
                ExpectedBlockerHeightMeters);
            if (!BlockerTransform.GetTranslation().Equals(
                    ExpectedBlockerLocation, 0.01) ||
                !BlockerTransform.GetScale3D().Equals(
                    ExpectedBlockerScale, 0.0001) ||
                !BlockerTransform.GetRotation().Equals(
                    FQuat::Identity, 0.0001))
            {
                OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: a hidden Cylinder trunk blocker no longer has the exact visible-tree-aligned location, scale or identity rotation.");
                return false;
            }
            const float ScaleZ = TreeScale.Z;
            if (!FMath::IsFinite(ScaleZ) || ScaleZ <= 0.0f)
            {
                OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: a tree has a non-positive/non-finite scale.");
                return false;
            }
            MinimumTreeScaleZ = FMath::Min(MinimumTreeScaleZ, ScaleZ);
            MaximumTreeScaleZ = FMath::Max(MaximumTreeScaleZ, ScaleZ);
            const double TreeHeightMeters =
                LocalTreeHeightMeters * static_cast<double>(ScaleZ);
            MinimumTreeHeightMeters = FMath::Min(
                MinimumTreeHeightMeters, TreeHeightMeters);
            MaximumTreeHeightMeters = FMath::Max(
                MaximumTreeHeightMeters, TreeHeightMeters);
            ++BlockerIndex;
        }
    }
    if (BlockerIndex != ExpectedTreeCount || MinimumTreeScaleZ > 0.96f ||
        MaximumTreeScaleZ < 1.88f ||
        MaximumTreeScaleZ - MinimumTreeScaleZ < 0.90f ||
        MinimumTreeHeightMeters < 17.95 ||
        MinimumTreeHeightMeters > 19.60 ||
        MaximumTreeHeightMeters < 36.50 ||
        MaximumTreeHeightMeters > 37.05)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V2_LANDSCAPE_INVALID: the mixed tree census or required 18-37 m physical height/scale range changed.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("Validated additive Explore V2 seed 0x2757A6A5: %d umbrella, %d columnar, %d dome/specimen and %d palm trees, including 560 exact inherited outer positions (%d Pawn-only hidden trunk proxies); %d layered shrubs, %d hedge modules, %d groundcover clumps, %d metric 3-6 cm near-turf and %d metric 18-80 cm meadow/sedge clusters. The axis/fountain are clear, visible vegetation is non-colliding/non-sensor truth, tree vertical scales span %.2f-%.2f and physical heights span %.2f-%.2f m, and eleven runtime MIDs drive trunk/branch/leaf/grass sway with underdamped strength and direction recovery. Exact botanical inventory, individual placement, survey and one-to-one 1 km claims remain false."),
        ExpectedUmbrellaBroadleafCount,
        ExpectedColumnarBroadleafCount,
        ExpectedDomeBroadleafCount,
        ExpectedPalmCount,
        ExpectedTreeCount,
        ExpectedShrubACount + ExpectedShrubBCount,
        ExpectedHedgeCount,
        ExpectedGroundcoverCount,
        ExpectedNearTurfCount,
        ExpectedMeadowSedgeCount,
        MinimumTreeScaleZ,
        MaximumTreeScaleZ,
        MinimumTreeHeightMeters,
        MaximumTreeHeightMeters);
    return true;
}
