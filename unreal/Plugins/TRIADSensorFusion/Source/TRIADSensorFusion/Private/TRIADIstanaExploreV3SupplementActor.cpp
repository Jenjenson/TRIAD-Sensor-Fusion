#include "TRIADIstanaExploreV3SupplementActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"

namespace
{
constexpr double CentimetersPerMeter = 100.0;
constexpr int32 ExpectedIslandTreeCount = 18;
constexpr int32 ExpectedDarkColumnarBroadleafCount = 232;
constexpr int32 ExpectedShrubCount = 96;
constexpr int32 ExpectedFernCount = 160;
constexpr int32 ExpectedMossCount = 24;
constexpr int32 ExpectedIslandImportedLod0Triangles = 1599403;
constexpr int32 ExpectedShrubImportedLod0Triangles = 27254;
constexpr int32 ExpectedFernImportedLod0Triangles = 6232;
constexpr int32 ExpectedMossImportedLod0Triangles = 204;
constexpr int32 ExpectedBermudaImportedLod0Triangles = 941;
constexpr float ExpectedBermudaImportedUpAxisSpanCm = 15.825569f;
constexpr float MinimumBermudaPlacedHeightCm = 3.0f;
constexpr float MaximumBermudaPlacedHeightCm = 6.0f;
// Bermuda's 223,596 Poly Haven catalog polycount is provenance only; its exact
// UE5.5 imported LOD0 is 941 triangles and remains a sparse accent. The
// 8-triangle Prepared Foliage008 card is the sole mass close-turf path.
constexpr int32 ExpectedBermudaGrassCount = 24;
// Sole safe mass-grass path: 18,432 x 8-triangle six-centimetre cards remain
// bounded to the camera-near 100 m cull range (147,456 source triangles).
constexpr int32 ExpectedNearTurfCount = 18432;

const FString IslandTreeObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_IslandTree01.SM_IPVExploreV3_IslandTree01"));
const FString ShrubObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_Shrub02.SM_IPVExploreV3_Shrub02"));
const FString FernObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_Fern02.SM_IPVExploreV3_Fern02"));
const FString MossObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_Moss01.SM_IPVExploreV3_Moss01"));
const FString BermudaObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_BermudaGrass.SM_IPVExploreV3_BermudaGrass"));
const FString TurfCardObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Vegetation/SM_IPVExploreV3_CloseTurfCards.SM_IPVExploreV3_CloseTurfCards"));
const FString V2BroadleafObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV1/Vegetation/SM_IstanaPublicViewExploreV1_Broadleaf_A.SM_IstanaPublicViewExploreV1_Broadleaf_A"));
const FString V2TrunkWindMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafTrunk_Wind.M_IPVExploreV2_BroadleafTrunk_Wind"));
const FString V2BranchWindMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV2/Vegetation/M_IPVExploreV2_BroadleafBranches_Wind.M_IPVExploreV2_BroadleafBranches_Wind"));
const FString V3DarkLeafWindMaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Materials/M_IPVExploreV3_ColumnarLeavesDark_Wind.M_IPVExploreV3_ColumnarLeavesDark_Wind"));
const FString V3MaterialRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Materials/"));
const FString PorticoObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV3/Portico/SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live.SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live"));

struct FPorticoImportedMaterialSpec
{
    FName MaterialName;
    int32 TriangleFaces;
};

const TArray<FPorticoImportedMaterialSpec>& PorticoImportedMaterialSpecs()
{
    static const TArray<FPorticoImportedMaterialSpec> Specs = {
        {TEXT("M_IPV7_Portico_Soffit"), 948},
        {TEXT("M_IPV7_Portico_Trim"), 3176},
        {TEXT("M_IPV7_Portico_Recess"), 72},
        {TEXT("M_IPV7_Portico_Metal"), 252},
        {TEXT("M_IPV7_Portico_Glass"), 576},
        {TEXT("M_IPV7_Portico_Louvre"), 792}};
    return Specs;
}

const FName WindStrengthParameter(TEXT("TRIAD_WindStrengthCm"));
const FName WindSpeedParameter(TEXT("TRIAD_WindSpeed"));
const FName WindDirectionParameter(TEXT("TRIAD_WindDirection"));

bool IgnoresAllChannels(const UPrimitiveComponent* Component)
{
    return Component &&
        Component->GetCollisionResponseToChannels() ==
            FCollisionResponseContainer(ECR_Ignore);
}

int32 CountTriangles(const UStaticMesh* Mesh, int32 LodIndex)
{
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    if (!RenderData || !RenderData->LODResources.IsValidIndex(LodIndex))
    {
        return INDEX_NONE;
    }
    uint64 Triangles = 0;
    for (const FStaticMeshSection& Section :
         RenderData->LODResources[LodIndex].Sections)
    {
        Triangles += Section.NumTriangles;
    }
    return Triangles <= static_cast<uint64>(MAX_int32)
        ? static_cast<int32>(Triangles)
        : INDEX_NONE;
}

bool HasExactMeshPath(const UStaticMeshComponent* Component, const FString& Path)
{
    return Component && Component->GetStaticMesh() &&
        Component->GetStaticMesh()->GetPathName() == Path;
}

bool HasExactMaterialOrRuntimeMidParent(
    const UMaterialInterface* Material,
    const FString& ExactPath)
{
    if (!Material)
    {
        return false;
    }
    if (Material->GetPathName() == ExactPath)
    {
        return true;
    }
    const UMaterialInstanceDynamic* Mid =
        Cast<UMaterialInstanceDynamic>(Material);
    return Mid && Mid->Parent && Mid->Parent->GetPathName() == ExactPath;
}

FString V3MaterialPath(const FString& Name)
{
    return V3MaterialRoot + Name + TEXT(".") + Name;
}

bool AllSlotsHaveExactMaterialOrMidParent(
    const UStaticMeshComponent* Component,
    const FString& ExactPath)
{
    if (!Component || Component->GetNumMaterials() <= 0)
    {
        return false;
    }
    for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
    {
        if (!HasExactMaterialOrRuntimeMidParent(
                Component->GetMaterial(Slot), ExactPath))
        {
            return false;
        }
    }
    return true;
}

bool ImportedSlotHasExactMaterialOrMidParent(
    const UStaticMeshComponent* Component,
    const FName ImportedSlot,
    const FString& ExactPath)
{
    const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    int32 Slot = Mesh
        ? Mesh->GetMaterialIndexFromImportedMaterialSlotName(ImportedSlot)
        : INDEX_NONE;
    if (Slot == INDEX_NONE && Mesh)
    {
        Slot = Mesh->GetMaterialIndex(ImportedSlot);
    }
    return Slot != INDEX_NONE &&
        HasExactMaterialOrRuntimeMidParent(
            Component->GetMaterial(Slot), ExactPath);
}

bool ValidatePorticoMaterialTopology(
    const UStaticMeshComponent* Component,
    FString& OutError)
{
    const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
    const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
    const TArray<FPorticoImportedMaterialSpec>& Specs =
        PorticoImportedMaterialSpecs();
    if (!Component || !Mesh || !RenderData ||
        RenderData->LODResources.IsEmpty() ||
        Component->GetNumMaterials() != Specs.Num() ||
        Mesh->GetStaticMaterials().Num() != Specs.Num() ||
        RenderData->LODResources[0].Sections.Num() != Specs.Num())
    {
        OutError = TEXT("V7 portico must retain exactly six used/imported material slots and six LOD0 sections; the two zero-face palette definitions are standalone only.");
        return false;
    }

    for (int32 SlotIndex = 0; SlotIndex < Specs.Num(); ++SlotIndex)
    {
        const FPorticoImportedMaterialSpec& Spec = Specs[SlotIndex];
        const FStaticMaterial& StaticMaterial =
            Mesh->GetStaticMaterials()[SlotIndex];
        bool bImportedSlotNameMatches = true;
#if WITH_EDITORONLY_DATA
        bImportedSlotNameMatches =
            StaticMaterial.ImportedMaterialSlotName == Spec.MaterialName;
#endif
        if (StaticMaterial.MaterialSlotName != Spec.MaterialName ||
            !bImportedSlotNameMatches ||
            !HasExactMaterialOrRuntimeMidParent(
                Component->GetMaterial(SlotIndex),
                V3MaterialPath(Spec.MaterialName.ToString())))
        {
            OutError = TEXT("V7 portico UE5.5 FStaticMaterial order/name or exact V3 material binding changed.");
            return false;
        }
    }

    TSet<int32> SeenMaterialIndices;
    int32 TriangleTotal = 0;
    for (const FStaticMeshSection& Section :
         RenderData->LODResources[0].Sections)
    {
        const int32 MaterialIndex = Section.MaterialIndex;
        if (!Specs.IsValidIndex(MaterialIndex) ||
            SeenMaterialIndices.Contains(MaterialIndex) ||
            Section.NumTriangles != Specs[MaterialIndex].TriangleFaces)
        {
            OutError = TEXT("V7 portico six-section material-index/triangle-face census changed.");
            return false;
        }
        SeenMaterialIndices.Add(MaterialIndex);
        TriangleTotal += Section.NumTriangles;
    }
    if (SeenMaterialIndices.Num() != Specs.Num() || TriangleTotal != 5816 ||
        Mesh->GetMaterialIndex(TEXT("M_IPV7_Portico_Render")) != INDEX_NONE ||
        Mesh->GetMaterialIndexFromImportedMaterialSlotName(
            TEXT("M_IPV7_Portico_Render")) != INDEX_NONE ||
        Mesh->GetMaterialIndex(TEXT("M_IPV7_Portico_Stone")) != INDEX_NONE ||
        Mesh->GetMaterialIndexFromImportedMaterialSlotName(
            TEXT("M_IPV7_Portico_Stone")) != INDEX_NONE)
    {
        OutError = TEXT("V7 portico face coverage changed or zero-face Render/Stone definitions were fabricated as mesh slots.");
        return false;
    }
    OutError.Reset();
    return true;
}
}

ATRIADIstanaExploreV3SupplementActor::ATRIADIstanaExploreV3SupplementActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bAllowTickOnDedicatedServer = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ExploreV3SupplementRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    IslandTree01Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CC0IslandTree01Midstorey"));
    DarkColumnarBroadleafInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("V3DarkColumnarBroadleafExactTransformReplacement"));
    Shrub02Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CC0Shrub02"));
    Fern02Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CC0Fern02"));
    Moss01Instances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CC0Moss01ShadedMicroAreas"));
    BermudaGrassInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("CC0BermudaGrassEdgePatches"));
    AmbientCgNearTurfInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("AmbientCgFoliage008NearTurf"));
    SupplementalTreePawnBlockers = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HiddenV3TreePawnOnlyBlockers"));

    const TArray<UHierarchicalInstancedStaticMeshComponent*> Hisms = {
        IslandTree01Instances,
        DarkColumnarBroadleafInstances,
        Shrub02Instances,
        Fern02Instances,
        Moss01Instances,
        BermudaGrassInstances,
        AmbientCgNearTurfInstances,
        SupplementalTreePawnBlockers};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Hisms)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetMobility(EComponentMobility::Static);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
    }

    ConfigureVisualHism(IslandTree01Instances, 5000, 60000, 30000, true);
    ConfigureVisualHism(DarkColumnarBroadleafInstances, 18000, 155000, 72000, true);
    ConfigureVisualHism(Shrub02Instances, 3000, 36000, 22000, true);
    ConfigureVisualHism(Fern02Instances, 2500, 26000, 18000, true);
    ConfigureVisualHism(Moss01Instances, 1200, 12000, 8000, false);
    ConfigureVisualHism(BermudaGrassInstances, 1200, 14000, 10000, false);
    ConfigureVisualHism(AmbientCgNearTurfInstances, 800, 10000, 8000, false);

    // The upstream 3,729,692 catalog polycount is provenance only. The exact
    // UE5.5 imported Island Tree LOD0 is 1,599,403 render triangles; both the
    // mesh asset and this component enforce LOD1 or coarser at runtime.
    IslandTree01Instances->bOverrideMinLOD = true;
    IslandTree01Instances->MinLOD = 1;
    IslandTree01Instances->ForcedLodModel = 0;
    DarkColumnarBroadleafInstances->bOverrideMinLOD = true;
    DarkColumnarBroadleafInstances->MinLOD = 1;
    DarkColumnarBroadleafInstances->ForcedLodModel = 0;
    Moss01Instances->bOverrideMinLOD = true;
    Moss01Instances->MinLOD = 1;
    Moss01Instances->ForcedLodModel = 0;
    BermudaGrassInstances->bOverrideMinLOD = true;
    BermudaGrassInstances->MinLOD = 1;
    BermudaGrassInstances->ForcedLodModel = 0;

    SupplementalTreePawnBlockers->SetCollisionEnabled(
        ECollisionEnabled::QueryOnly);
    SupplementalTreePawnBlockers->SetCollisionObjectType(ECC_WorldStatic);
    SupplementalTreePawnBlockers->SetCollisionResponseToAllChannels(ECR_Ignore);
    SupplementalTreePawnBlockers->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    SupplementalTreePawnBlockers->SetVisibility(false, true);
    SupplementalTreePawnBlockers->SetHiddenInGame(true, true);
    SupplementalTreePawnBlockers->SetCastShadow(false);

    PorticoV7RenderOnlyComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("V7PorticoV5LiveRenderOnlyIdentitySibling"));
    LowFrequencyTerrainComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoarseDsmLowFrequencyVisualPrior"));
    OsmPublicRoadsComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OsmApproximatePublicRoadVisuals"));
    UraIndicativeRoadsComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("UraIndicativePlanningRoadVisuals"));
    OsmWaterComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OsmApproximateWaterVisuals"));
    const TArray<UStaticMeshComponent*> StaticVisuals = {
        PorticoV7RenderOnlyComponent,
        LowFrequencyTerrainComponent,
        OsmPublicRoadsComponent,
        UraIndicativeRoadsComponent,
        OsmWaterComponent};
    for (UStaticMeshComponent* Component : StaticVisuals)
    {
        Component->SetupAttachment(SceneRoot);
        Component->SetRelativeTransform(FTransform::Identity);
        ConfigureVisualStaticMesh(Component);
    }
    // All coarse generated context remains a bound, hidden diagnostic until a
    // separate registration/collision revision proves alignment with the exact
    // inherited V2 terrain, hardscape, Pawn collision and OSM/HDB state.
    for (UStaticMeshComponent* Component : {
             LowFrequencyTerrainComponent.Get(),
             OsmPublicRoadsComponent.Get(),
             UraIndicativeRoadsComponent.Get(),
             OsmWaterComponent.Get()})
    {
        Component->SetVisibility(false, true);
        Component->SetHiddenInGame(true, true);
    }

    GustRandom.Initialize(DeterministicPlacementSeed ^ 0x57494E44);
}

void ATRIADIstanaExploreV3SupplementActor::PostLoad()
{
    Super::PostLoad();
    FString IgnoredError;
    RefreshSavedAssetRuntimeBindings(IgnoredError);
}

bool ATRIADIstanaExploreV3SupplementActor::RefreshSavedAssetRuntimeBindings(
    FString& OutError)
{
    UStaticMesh* Broadleaf = DarkColumnarBroadleafInstances
        ? DarkColumnarBroadleafInstances->GetStaticMesh()
        : nullptr;
    if (!Broadleaf)
    {
        ExpectedRuntimeWindMidCount = 0;
        OutError = TEXT("The saved dark-columnar broadleaf mesh is absent.");
        return false;
    }
    ColumnarTrunkMaterialSlot = Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(
        TEXT("jacaranda_tree_trunk"));
    ColumnarBranchMaterialSlot = Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(
        TEXT("jacaranda_tree_branches"));
    ColumnarLeafMaterialSlot = Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(
        TEXT("jacaranda_tree_leaves"));
    if (ColumnarTrunkMaterialSlot == INDEX_NONE)
    {
        ColumnarTrunkMaterialSlot = Broadleaf->GetMaterialIndex(TEXT("jacaranda_tree_trunk"));
    }
    if (ColumnarBranchMaterialSlot == INDEX_NONE)
    {
        ColumnarBranchMaterialSlot = Broadleaf->GetMaterialIndex(TEXT("jacaranda_tree_branches"));
    }
    if (ColumnarLeafMaterialSlot == INDEX_NONE)
    {
        ColumnarLeafMaterialSlot = Broadleaf->GetMaterialIndex(TEXT("jacaranda_tree_leaves"));
    }
    ExpectedRuntimeWindMidCount = 0;
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Animated = {
        IslandTree01Instances,
        DarkColumnarBroadleafInstances,
        Shrub02Instances,
        Fern02Instances,
        Moss01Instances,
        BermudaGrassInstances,
        AmbientCgNearTurfInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Animated)
    {
        if (!Component || !Component->GetStaticMesh() ||
            Component->GetNumMaterials() <= 0)
        {
            ExpectedRuntimeWindMidCount = 0;
            OutError = TEXT("A saved V3 animated HISM mesh/material roster is incomplete.");
            return false;
        }
        ExpectedRuntimeWindMidCount += Component->GetNumMaterials();
    }
    if (ColumnarTrunkMaterialSlot == INDEX_NONE ||
        ColumnarBranchMaterialSlot == INDEX_NONE ||
        ColumnarLeafMaterialSlot == INDEX_NONE)
    {
        ExpectedRuntimeWindMidCount = 0;
        OutError = TEXT("Saved Jacaranda trunk/branch/leaf slot identity is incomplete.");
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV3SupplementActor::ConfigureVisualHism(
    UHierarchicalInstancedStaticMeshComponent* Component,
    int32 StartCullDistance,
    int32 EndCullDistance,
    int32 WpoDisableDistance,
    bool bCastShadow)
{
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetCullDistances(StartCullDistance, EndCullDistance);
    Component->SetWorldPositionOffsetDisableDistance(WpoDisableDistance);
    Component->SetCastShadow(bCastShadow);
}

void ATRIADIstanaExploreV3SupplementActor::ConfigureVisualStaticMesh(
    UStaticMeshComponent* Component)
{
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
}

double ATRIADIstanaExploreV3SupplementActor::TerrainHeightMeters(
    double X,
    double Y)
{
    const double Radius = FMath::Sqrt(X * X + Y * Y);
    return 0.72 * FMath::Sin(X / 185.0) +
        0.48 * FMath::Cos(Y / 230.0) +
        0.22 * FMath::Sin((X + Y) / 97.0) +
        0.0000011 * Radius * Radius - 0.48;
}

bool ATRIADIstanaExploreV3SupplementActor::IsFiniteMesh(
    const UStaticMesh* Mesh)
{
    if (!Mesh)
    {
        return false;
    }
    const FVector Extent = Mesh->GetBounds().BoxExtent;
    return !Extent.ContainsNaN() && Extent.GetMin() > 0.01;
}

bool ATRIADIstanaExploreV3SupplementActor::ConfigureRequiredAssets(
    UStaticMesh* InIslandTree01,
    UStaticMesh* InShrub02,
    UStaticMesh* InFern02,
    UStaticMesh* InMoss01,
    UStaticMesh* InBermudaGrass,
    UStaticMesh* InAmbientCgNearTurfCard,
    UStaticMesh* InBoundedV2Broadleaf,
    UMaterialInterface* InV2TreeTrunkWindMaterial,
    UMaterialInterface* InV2TreeBranchWindMaterial,
    UMaterialInterface* InV3DarkColumnarLeafWindMaterial,
    UStaticMesh* InPawnBlockerCylinder,
    UStaticMesh* InPorticoV7V5Live,
    FString& OutError)
{
    const TArray<UStaticMesh*> Required = {
        InIslandTree01,
        InShrub02,
        InFern02,
        InMoss01,
        InBermudaGrass,
        InAmbientCgNearTurfCard,
        InBoundedV2Broadleaf,
        InPawnBlockerCylinder,
        InPorticoV7V5Live};
    for (const UStaticMesh* Mesh : Required)
    {
        if (!IsFiniteMesh(Mesh))
        {
            OutError = TEXT("All nine Explore V3 required meshes need finite non-zero 3D bounds.");
            return false;
        }
    }
    if (InIslandTree01->GetPathName() != IslandTreeObjectPath ||
        InShrub02->GetPathName() != ShrubObjectPath ||
        InFern02->GetPathName() != FernObjectPath ||
        InMoss01->GetPathName() != MossObjectPath ||
        InBermudaGrass->GetPathName() != BermudaObjectPath ||
        InAmbientCgNearTurfCard->GetPathName() != TurfCardObjectPath ||
        InBoundedV2Broadleaf->GetPathName() != V2BroadleafObjectPath ||
        InPawnBlockerCylinder->GetPathName() !=
            TEXT("/Engine/BasicShapes/Cylinder.Cylinder") ||
        InPorticoV7V5Live->GetPathName() != PorticoObjectPath)
    {
        OutError = TEXT("A V3 required mesh does not have its exact additive object path.");
        return false;
    }
    if (!InV2TreeTrunkWindMaterial || !InV2TreeBranchWindMaterial ||
        !InV3DarkColumnarLeafWindMaterial ||
        InV2TreeTrunkWindMaterial->GetPathName() != V2TrunkWindMaterialPath ||
        InV2TreeBranchWindMaterial->GetPathName() != V2BranchWindMaterialPath ||
        InV3DarkColumnarLeafWindMaterial->GetPathName() != V3DarkLeafWindMaterialPath)
    {
        OutError = TEXT("The exact protected V2 trunk/branch and V3-owned dark leaf wind materials are required.");
        return false;
    }

    IslandTree01Instances->SetStaticMesh(InIslandTree01);
    DarkColumnarBroadleafInstances->SetStaticMesh(InBoundedV2Broadleaf);
    Shrub02Instances->SetStaticMesh(InShrub02);
    Fern02Instances->SetStaticMesh(InFern02);
    Moss01Instances->SetStaticMesh(InMoss01);
    BermudaGrassInstances->SetStaticMesh(InBermudaGrass);
    AmbientCgNearTurfInstances->SetStaticMesh(InAmbientCgNearTurfCard);
    SupplementalTreePawnBlockers->SetStaticMesh(InPawnBlockerCylinder);
    PorticoV7RenderOnlyComponent->SetStaticMesh(InPorticoV7V5Live);

    ColumnarTrunkMaterialSlot = InBoundedV2Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(
        TEXT("jacaranda_tree_trunk"));
    ColumnarBranchMaterialSlot = InBoundedV2Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(
        TEXT("jacaranda_tree_branches"));
    ColumnarLeafMaterialSlot = InBoundedV2Broadleaf->GetMaterialIndexFromImportedMaterialSlotName(
        TEXT("jacaranda_tree_leaves"));
    if (ColumnarTrunkMaterialSlot == INDEX_NONE)
    {
        ColumnarTrunkMaterialSlot = InBoundedV2Broadleaf->GetMaterialIndex(TEXT("jacaranda_tree_trunk"));
    }
    if (ColumnarBranchMaterialSlot == INDEX_NONE)
    {
        ColumnarBranchMaterialSlot = InBoundedV2Broadleaf->GetMaterialIndex(TEXT("jacaranda_tree_branches"));
    }
    if (ColumnarLeafMaterialSlot == INDEX_NONE)
    {
        ColumnarLeafMaterialSlot = InBoundedV2Broadleaf->GetMaterialIndex(TEXT("jacaranda_tree_leaves"));
    }
    if (ColumnarTrunkMaterialSlot == INDEX_NONE ||
        ColumnarBranchMaterialSlot == INDEX_NONE ||
        ColumnarLeafMaterialSlot == INDEX_NONE ||
        ColumnarTrunkMaterialSlot == ColumnarBranchMaterialSlot ||
        ColumnarTrunkMaterialSlot == ColumnarLeafMaterialSlot ||
        ColumnarBranchMaterialSlot == ColumnarLeafMaterialSlot ||
        InBoundedV2Broadleaf->GetMinLODIdx() != 1)
    {
        OutError = TEXT("The inherited bounded Jacaranda mesh lost its distinct trunk/branch/leaf slots or asset MinLOD1 gate.");
        return false;
    }
    DarkColumnarBroadleafInstances->SetMaterial(
        ColumnarTrunkMaterialSlot, InV2TreeTrunkWindMaterial);
    DarkColumnarBroadleafInstances->SetMaterial(
        ColumnarBranchMaterialSlot, InV2TreeBranchWindMaterial);
    DarkColumnarBroadleafInstances->SetMaterial(
        ColumnarLeafMaterialSlot, InV3DarkColumnarLeafWindMaterial);

    ExpectedRuntimeWindMidCount = 0;
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Animated = {
        IslandTree01Instances,
        DarkColumnarBroadleafInstances,
        Shrub02Instances,
        Fern02Instances,
        Moss01Instances,
        BermudaGrassInstances,
        AmbientCgNearTurfInstances};
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Animated)
    {
        if (Component->GetNumMaterials() <= 0)
        {
            OutError = TEXT("Every visible V3 HISM must expose at least one saved wind material slot.");
            ExpectedRuntimeWindMidCount = 0;
            return false;
        }
        ExpectedRuntimeWindMidCount += Component->GetNumMaterials();
    }
    WindMaterialInstances.Reset();
    PreservedV2ColumnarWorldTransforms.Reset();
    bV2ColumnarVisualsReplacedWithExactTransforms = false;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV3SupplementActor::ConfigureOptionalVisualContext(
    UStaticMesh* InLowFrequencyTerrain,
    UStaticMesh* InOsmPublicRoads,
    UStaticMesh* InUraIndicativeRoads,
    UStaticMesh* InOsmWater,
    const FString& InGeneratedManifestSha256,
    FString& OutError)
{
    const int32 Present =
        (InLowFrequencyTerrain ? 1 : 0) +
        (InOsmPublicRoads ? 1 : 0) +
        (InUraIndicativeRoads ? 1 : 0) +
        (InOsmWater ? 1 : 0);
    if (Present != 0 && Present != 4)
    {
        OutError = TEXT("Generated geospatial context is all-or-none; partial meshes are refused.");
        return false;
    }
    if (Present == 0)
    {
        if (!InGeneratedManifestSha256.IsEmpty())
        {
            OutError = TEXT("A generated-context manifest SHA cannot be recorded without all four meshes.");
            return false;
        }
        LowFrequencyTerrainComponent->SetStaticMesh(nullptr);
        OsmPublicRoadsComponent->SetStaticMesh(nullptr);
        UraIndicativeRoadsComponent->SetStaticMesh(nullptr);
        OsmWaterComponent->SetStaticMesh(nullptr);
        GeneratedContextManifestSha256.Reset();
        bCompleteGeneratedContextConfigured = false;
        OutError.Reset();
        return true;
    }
    if (!IsFiniteMesh(InLowFrequencyTerrain) ||
        !IsFiniteMesh(InOsmPublicRoads) ||
        !IsFiniteMesh(InUraIndicativeRoads) ||
        !IsFiniteMesh(InOsmWater) ||
        InGeneratedManifestSha256 !=
            TEXT("FBC1032E60591AA8CEA0C1B3E16BDB9E9776C84692533CABC11D8EB7A79D6A56"))
    {
        OutError = TEXT("Complete context requires four finite meshes and one uppercase 64-hex manifest SHA.");
        return false;
    }
    for (int32 Index = 0; Index < InGeneratedManifestSha256.Len(); ++Index)
    {
        const TCHAR Character = InGeneratedManifestSha256[Index];
        if (!FChar::IsHexDigit(Character))
        {
            OutError = TEXT("Generated-context manifest SHA contains a non-hex character.");
            return false;
        }
    }
    LowFrequencyTerrainComponent->SetStaticMesh(InLowFrequencyTerrain);
    OsmPublicRoadsComponent->SetStaticMesh(InOsmPublicRoads);
    UraIndicativeRoadsComponent->SetStaticMesh(InUraIndicativeRoads);
    OsmWaterComponent->SetStaticMesh(InOsmWater);
    GeneratedContextManifestSha256 = InGeneratedManifestSha256;
    bCompleteGeneratedContextConfigured = true;
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV3SupplementActor::ClearAllInstances()
{
    IslandTree01Instances->ClearInstances();
    DarkColumnarBroadleafInstances->ClearInstances();
    Shrub02Instances->ClearInstances();
    Fern02Instances->ClearInstances();
    Moss01Instances->ClearInstances();
    BermudaGrassInstances->ClearInstances();
    AmbientCgNearTurfInstances->ClearInstances();
    SupplementalTreePawnBlockers->ClearInstances();
}

bool ATRIADIstanaExploreV3SupplementActor::PopulateDeterministicSupplement(
    const TArray<FTransform>& InheritedV2ColumnarWorldTransforms,
    FString& OutError)
{
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Required = {
        IslandTree01Instances,
        DarkColumnarBroadleafInstances,
        Shrub02Instances,
        Fern02Instances,
        Moss01Instances,
        BermudaGrassInstances,
        AmbientCgNearTurfInstances,
        SupplementalTreePawnBlockers};
    if (!GetActorTransform().Equals(FTransform::Identity, 0.001f) ||
        ExpectedRuntimeWindMidCount <= 0 ||
        InheritedV2ColumnarWorldTransforms.Num() != ExpectedDarkColumnarBroadleafCount)
    {
        OutError = TEXT("V3 population requires an identity actor and a configured complete asset/material roster.");
        return false;
    }
    for (UHierarchicalInstancedStaticMeshComponent* Component : Required)
    {
        if (!Component || !Component->GetStaticMesh())
        {
            OutError = TEXT("V3 population was requested before all required meshes were configured.");
            return false;
        }
        Component->bAutoRebuildTreeOnInstanceChanges = false;
    }
    ClearAllInstances();
    PreservedV2ColumnarWorldTransforms.Reset();
    bV2ColumnarVisualsReplacedWithExactTransforms = false;
    FRandomStream Random(DeterministicPlacementSeed);

    for (const FTransform& WorldTransform : InheritedV2ColumnarWorldTransforms)
    {
        if (WorldTransform.ContainsNaN() || WorldTransform.GetScale3D().GetMin() <= 0.001)
        {
            OutError = TEXT("A V2 columnar transform is missing, non-finite, or non-positive.");
            return false;
        }
        const FVector Meters = WorldTransform.GetTranslation() / CentimetersPerMeter;
        if (FMath::Abs(Meters.X) < 17.0 && Meters.Y >= 0.0 && Meters.Y < 138.0)
        {
            OutError = TEXT("A V2 columnar transform enters the protected formal front axis; replacement refused.");
            return false;
        }
        DarkColumnarBroadleafInstances->AddInstance(WorldTransform, true);
        PreservedV2ColumnarWorldTransforms.Add(WorldTransform);
    }
    bV2ColumnarVisualsReplacedWithExactTransforms = true;

    auto AddAt = [](UHierarchicalInstancedStaticMeshComponent* Component,
                    double X,
                    double Y,
                    double Z,
                    float Yaw,
                    const FVector& Scale)
    {
        Component->AddInstance(FTransform(
            FRotator(0.0f, Yaw, 0.0f),
            FVector(X, Y, Z) * CentimetersPerMeter,
            Scale));
    };
    auto SideX = [&Random](double Minimum, double Maximum)
    {
        return (Random.RandRange(0, 1) == 0 ? -1.0 : 1.0) *
            Random.FRandRange(Minimum, Maximum);
    };

    // The 5 m scan remains a small secondary mid-storey form, never a scaled
    // substitute for the 23-36 m official heritage-tree silhouettes.
    for (int32 Index = 0; Index < ExpectedIslandTreeCount; ++Index)
    {
        const double Angle = 2.0 * PI *
            (static_cast<double>(Index) + 0.35) /
            static_cast<double>(ExpectedIslandTreeCount);
        const double Radius = 165.0 + 18.0 * (Index % 5);
        const double X = FMath::Cos(Angle) * Radius;
        const double Y = FMath::Sin(Angle) * Radius + 35.0;
        const double Z = TerrainHeightMeters(X, Y);
        const float Scale = 0.82f + 0.07f * static_cast<float>(Index % 7);
        AddAt(IslandTree01Instances, X, Y, Z,
            Random.FRandRange(-180.0f, 180.0f),
            FVector(Scale, Scale * Random.FRandRange(0.92f, 1.08f), Scale));
        const float BlockerHeight = 3.6f * Scale;
        AddAt(SupplementalTreePawnBlockers, X, Y,
            Z + BlockerHeight * 0.5,
            0.0f,
            FVector(0.56f * Scale, 0.56f * Scale, BlockerHeight));
    }

    for (int32 Index = 0; Index < ExpectedShrubCount; ++Index)
    {
        const double X = SideX(23.0, 88.0);
        const double Y = Random.FRandRange(-25.0, 190.0);
        const double Z = TerrainHeightMeters(X, Y);
        const float Scale = Random.FRandRange(0.42f, 0.82f);
        AddAt(Shrub02Instances, X, Y, Z,
            Random.FRandRange(-180.0f, 180.0f),
            FVector(Scale, Scale * Random.FRandRange(0.86f, 1.12f), Scale));
    }
    for (int32 Index = 0; Index < ExpectedFernCount; ++Index)
    {
        const double X = SideX(24.0, 105.0);
        const double Y = Random.FRandRange(-20.0, 215.0);
        const double Z = TerrainHeightMeters(X, Y);
        const float Scale = Random.FRandRange(0.55f, 1.05f);
        AddAt(Fern02Instances, X, Y, Z,
            Random.FRandRange(-180.0f, 180.0f), FVector(Scale));
    }
    for (int32 Index = 0; Index < ExpectedMossCount; ++Index)
    {
        const int32 TreeIndex = Index % ExpectedIslandTreeCount;
        FTransform TreeTransform;
        IslandTree01Instances->GetInstanceTransform(TreeIndex, TreeTransform, false);
        const FVector TreeMeters = TreeTransform.GetTranslation() / CentimetersPerMeter;
        const double Angle = Random.FRandRange(-PI, PI);
        const double Radius = Random.FRandRange(1.2, 3.8);
        const double X = TreeMeters.X + FMath::Cos(Angle) * Radius;
        const double Y = TreeMeters.Y + FMath::Sin(Angle) * Radius;
        AddAt(Moss01Instances, X, Y, TerrainHeightMeters(X, Y),
            Random.FRandRange(-180.0f, 180.0f),
            FVector(Random.FRandRange(0.72f, 1.24f)));
    }
    for (int32 Index = 0; Index < ExpectedBermudaGrassCount; ++Index)
    {
        const double X = SideX(20.0, 82.0);
        const double Y = Random.FRandRange(8.0, 210.0);
        const double Z = TerrainHeightMeters(X, Y);
        const float TargetHeightCm = Random.FRandRange(
            MinimumBermudaPlacedHeightCm,
            MaximumBermudaPlacedHeightCm);
        AddAt(BermudaGrassInstances, X, Y, Z,
            Random.FRandRange(-180.0f, 180.0f),
            FVector(Random.FRandRange(0.25f, 0.55f),
                    Random.FRandRange(0.25f, 0.55f),
                    TargetHeightCm /
                        ExpectedBermudaImportedUpAxisSpanCm));
    }
    constexpr int32 TurfGridColumns = 144;
    constexpr int32 TurfGridRows = 192;
    constexpr int32 TurfGridCandidates = TurfGridColumns * TurfGridRows;
    int32 TurfAdded = 0;
    for (int32 Candidate = 0;
         Candidate < TurfGridCandidates && TurfAdded < ExpectedNearTurfCount;
         ++Candidate)
    {
        // Coprime permutation spreads early accepted cells across the whole
        // lawn instead of filling a near-to-far strip before exclusions.
        const int32 Permuted = (Candidate * 7919) % TurfGridCandidates;
        const int32 Column = Permuted % TurfGridColumns;
        const int32 Row = Permuted / TurfGridColumns;
        const double CellX = 192.0 / TurfGridColumns;
        const double CellY = 212.0 / TurfGridRows;
        const double X = -96.0 +
            (Column + 0.5 + Random.FRandRange(-0.32, 0.32)) * CellX;
        const double Y = 8.0 +
            (Row + 0.5 + Random.FRandRange(-0.32, 0.32)) * CellY;
        const bool bBuildingOrApron = Y < 14.0 && FMath::Abs(X) < 82.0;
        const bool bEntranceWalk = FMath::Abs(X) < 3.2 && Y < 70.0;
        const bool bFountainOrDrive = FVector2D(
            X, Y - 95.0).SizeSquared() < FMath::Square(25.0);
        if (bBuildingOrApron || bEntranceWalk || bFountainOrDrive)
        {
            continue;
        }
        const double Z = TerrainHeightMeters(X, Y) + 0.012;
        const float Scale = Random.FRandRange(0.82f, 1.28f);
        AddAt(AmbientCgNearTurfInstances, X, Y, Z,
            Random.FRandRange(-180.0f, 180.0f),
            FVector(Scale, Scale, Random.FRandRange(0.50f, 1.00f)));
        ++TurfAdded;
    }
    if (TurfAdded != ExpectedNearTurfCount)
    {
        OutError = TEXT("Deterministic close-turf grid/exclusions could not produce the exact bounded census.");
        return false;
    }

    for (UHierarchicalInstancedStaticMeshComponent* Component : Required)
    {
        Component->bAutoRebuildTreeOnInstanceChanges = true;
        if (!Component->BuildTreeIfOutdated(false, true))
        {
            OutError = TEXT("A V3 HISM cluster tree could not be rebuilt synchronously before validation/save.");
            return false;
        }
    }
    FString Report;
    if (!ValidateExploreV3Supplement(Report))
    {
        OutError = Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV3SupplementActor::RecordPreservedDistantContext(
    const UStaticMeshComponent* Component,
    FString& OutError)
{
    if (!Component || !Component->GetStaticMesh() ||
        Component->GetStaticMesh()->GetPathName() !=
            TEXT("/Game/TRIAD/IstanaPublicView/Context/SM_IstanaPublicView_OSMContextBuildings.SM_IstanaPublicView_OSMContextBuildings"))
    {
        OutError = TEXT("The exact inherited Explore V2 OSM/HDB context mesh is absent.");
        return false;
    }
    PreservedDistantContextMeshPath = Component->GetStaticMesh()->GetPathName();
    PreservedDistantContextMaterialPaths.Reset();
    for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material)
        {
            OutError = TEXT("The inherited OSM/HDB component has a null material.");
            return false;
        }
        PreservedDistantContextMaterialPaths.Add(Material->GetPathName());
    }
    if (PreservedDistantContextMaterialPaths.IsEmpty())
    {
        OutError = TEXT("The inherited OSM/HDB component has no materials.");
        return false;
    }
    PreservedDistantContextRelativeTransform = Component->GetRelativeTransform();
    PreservedDistantContextWorldTransform = Component->GetComponentTransform();
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV3SupplementActor::ValidatePreservedDistantContext(
    const UStaticMeshComponent* Component,
    FString& OutError) const
{
    if (!Component || !Component->GetStaticMesh() ||
        Component->GetStaticMesh()->GetPathName() != PreservedDistantContextMeshPath ||
        !Component->GetRelativeTransform().Equals(PreservedDistantContextRelativeTransform, 0.001f) ||
        !Component->GetComponentTransform().Equals(PreservedDistantContextWorldTransform, 0.001f) ||
        !Component->IsVisible() || Component->bHiddenInGame ||
        !Component->IsActive() || !Component->bAutoActivate ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetGenerateOverlapEvents() || !IgnoresAllChannels(Component) ||
        Component->GetNumMaterials() != PreservedDistantContextMaterialPaths.Num())
    {
        OutError = TEXT("The inherited OSM/HDB mesh/material/transform/state snapshot changed.");
        return false;
    }
    for (int32 Index = 0; Index < PreservedDistantContextMaterialPaths.Num(); ++Index)
    {
        const UMaterialInterface* Material = Component->GetMaterial(Index);
        if (!Material || Material->GetPathName() != PreservedDistantContextMaterialPaths[Index])
        {
            OutError = TEXT("An inherited OSM/HDB material path changed.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV3SupplementActor::CreateRuntimeWindMaterialInstances(
    FString& OutError)
{
    WindMaterialInstances.Reset();
    const TArray<UHierarchicalInstancedStaticMeshComponent*> Animated = {
        IslandTree01Instances,
        DarkColumnarBroadleafInstances,
        Shrub02Instances,
        Fern02Instances,
        Moss01Instances,
        BermudaGrassInstances,
        AmbientCgNearTurfInstances};
    for (UHierarchicalInstancedStaticMeshComponent* Component : Animated)
    {
        for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
        {
            UMaterialInstanceDynamic* Mid =
                Component->CreateDynamicMaterialInstance(Slot, Component->GetMaterial(Slot));
            if (!Mid)
            {
                OutError = TEXT("Could not create every V3 per-component wind MID.");
                WindMaterialInstances.Reset();
                return false;
            }
            WindMaterialInstances.Add(Mid);
        }
    }
    if (WindMaterialInstances.Num() != ExpectedRuntimeWindMidCount)
    {
        OutError = TEXT("The V3 runtime wind MID roster is incomplete.");
        WindMaterialInstances.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV3SupplementActor::ApplyWindParameters()
{
    const FVector2D Direction = CurrentWindDirection.GetSafeNormal();
    for (UMaterialInstanceDynamic* Mid : WindMaterialInstances)
    {
        Mid->SetScalarParameterValue(WindStrengthParameter, CurrentWindStrengthCm);
        Mid->SetScalarParameterValue(WindSpeedParameter, WindSpeed);
        Mid->SetVectorParameterValue(
            WindDirectionParameter,
            FLinearColor(Direction.X, Direction.Y, 0.0f, 0.0f));
    }
}

void ATRIADIstanaExploreV3SupplementActor::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!RefreshSavedAssetRuntimeBindings(Error))
    {
        UE_LOG(LogTemp, Error, TEXT("ISTANA_EXPLORE_V3_WIND_DISABLED: %s"), *Error);
        SetActorTickEnabled(false);
        return;
    }
    GustRandom.Initialize(DeterministicPlacementSeed ^ 0x57494E44);
    CurrentWindStrengthCm = BaseWindStrengthCm;
    CurrentWindDirection = PrevailingWindDirection.GetSafeNormal();
    WindStrengthVelocityCmPerSecond = 0.0f;
    WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    ActiveGustDirection = CurrentWindDirection;
    ActiveGustElapsedSeconds = -1.0f;
    TimeUntilNextGustSeconds = GustRandom.FRandRange(7.0f, 13.0f);
    if (!CreateRuntimeWindMaterialInstances(Error))
    {
        UE_LOG(LogTemp, Error, TEXT("ISTANA_EXPLORE_V3_WIND_DISABLED: %s"), *Error);
        SetActorTickEnabled(false);
        return;
    }
    ApplyWindParameters();
}

void ATRIADIstanaExploreV3SupplementActor::TriggerWindGust(float PeakStrengthCm)
{
    const float RequestedPeak = PeakStrengthCm >= 0.0f
        ? PeakStrengthCm
        : GustPeakStrengthCm * GustRandom.FRandRange(0.90f, 1.08f);
    ActiveGustPeakStrengthCm = FMath::Clamp(
        RequestedPeak, BaseWindStrengthCm + 1.0f, 150.0f);
    ActiveGustElapsedSeconds = 0.0f;
    ActiveGustDurationSeconds = GustRandom.FRandRange(1.6f, 3.2f);
    const FVector2D Prevailing = PrevailingWindDirection.GetSafeNormal();
    const float BaseAngle = FMath::Atan2(Prevailing.Y, Prevailing.X);
    const float Deflection = FMath::DegreesToRadians(
        GustRandom.FRandRange(-24.0f, 24.0f));
    ActiveGustDirection = FVector2D(
        FMath::Cos(BaseAngle + Deflection),
        FMath::Sin(BaseAngle + Deflection));
}

void ATRIADIstanaExploreV3SupplementActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!IsWindRuntimeActive() || !FMath::IsFinite(DeltaSeconds) ||
        DeltaSeconds <= 0.0f)
    {
        return;
    }
    const float ClampedDelta = FMath::Min(DeltaSeconds, 0.25f);
    float TargetStrength = BaseWindStrengthCm;
    FVector2D TargetDirection = PrevailingWindDirection.GetSafeNormal();
    if (ActiveGustElapsedSeconds >= 0.0f)
    {
        ActiveGustElapsedSeconds += ClampedDelta;
        const float Alpha = FMath::Clamp(
            ActiveGustElapsedSeconds / ActiveGustDurationSeconds, 0.0f, 1.0f);
        if (Alpha >= 1.0f)
        {
            ActiveGustElapsedSeconds = -1.0f;
            TimeUntilNextGustSeconds = GustRandom.FRandRange(7.0f, 13.0f);
        }
        else
        {
            const float Pulse = FMath::Pow(FMath::Sin(PI * Alpha), 0.72f);
            TargetStrength = FMath::Lerp(BaseWindStrengthCm, ActiveGustPeakStrengthCm, Pulse);
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

    // Explicitly underdamped strength and direction springs retain velocity,
    // producing the requested visible recovery/after-sway after a gust ends.
    const float AngularFrequency = 2.0f * PI * RecoveryFrequencyHz;
    float Remaining = ClampedDelta;
    while (Remaining > KINDA_SMALL_NUMBER)
    {
        const float Step = FMath::Min(Remaining, 1.0f / 60.0f);
        const float Acceleration =
            AngularFrequency * AngularFrequency * (TargetStrength - CurrentWindStrengthCm) -
            2.0f * RecoveryDampingRatio * AngularFrequency * WindStrengthVelocityCmPerSecond;
        WindStrengthVelocityCmPerSecond += Acceleration * Step;
        CurrentWindStrengthCm += WindStrengthVelocityCmPerSecond * Step;
        const FVector2D DirectionAcceleration =
            AngularFrequency * AngularFrequency * (TargetDirection - CurrentWindDirection) -
            2.0f * RecoveryDampingRatio * AngularFrequency * WindDirectionVelocityPerSecond;
        WindDirectionVelocityPerSecond += DirectionAcceleration * Step;
        CurrentWindDirection += WindDirectionVelocityPerSecond * Step;
        Remaining -= Step;
    }
    CurrentWindStrengthCm = FMath::Clamp(CurrentWindStrengthCm, 0.0f, 160.0f);
    WindStrengthVelocityCmPerSecond = FMath::Clamp(
        WindStrengthVelocityCmPerSecond, -1000.0f, 1000.0f);
    if (!FMath::IsFinite(CurrentWindDirection.Size()) ||
        CurrentWindDirection.Size() < 0.4)
    {
        CurrentWindDirection = PrevailingWindDirection.GetSafeNormal();
        WindDirectionVelocityPerSecond = FVector2D::ZeroVector;
    }
    ApplyWindParameters();
}

bool ATRIADIstanaExploreV3SupplementActor::IsWindRuntimeActive() const
{
    if (!HasActorBegunPlay() || ExpectedRuntimeWindMidCount <= 0 ||
        WindMaterialInstances.Num() != ExpectedRuntimeWindMidCount)
    {
        return false;
    }
    for (const UMaterialInstanceDynamic* Mid : WindMaterialInstances)
    {
        if (!Mid)
        {
            return false;
        }
    }
    return true;
}

FString ATRIADIstanaExploreV3SupplementActor::BuildWindRuntimeStateReport() const
{
    return FString::Printf(
        TEXT("windActive=%s strengthCm=%.4f strengthVelocityCmPerSecond=%.4f direction=(%.6f,%.6f) directionVelocity=(%.6f,%.6f) gustElapsedSeconds=%.4f gustDurationSeconds=%.4f timeUntilNextGustSeconds=%.4f recoveryFrequencyHz=%.4f recoveryDampingRatio=%.4f runtimeMids=%d/%d"),
        IsWindRuntimeActive() ? TEXT("true") : TEXT("false"),
        CurrentWindStrengthCm,
        WindStrengthVelocityCmPerSecond,
        CurrentWindDirection.X,
        CurrentWindDirection.Y,
        WindDirectionVelocityPerSecond.X,
        WindDirectionVelocityPerSecond.Y,
        ActiveGustElapsedSeconds,
        ActiveGustDurationSeconds,
        TimeUntilNextGustSeconds,
        RecoveryFrequencyHz,
        RecoveryDampingRatio,
        WindMaterialInstances.Num(),
        ExpectedRuntimeWindMidCount);
}

bool ATRIADIstanaExploreV3SupplementActor::ValidateExploreV3Supplement(
    FString& OutReport) const
{
    const TArray<const UHierarchicalInstancedStaticMeshComponent*> VisualHisMs = {
        IslandTree01Instances,
        DarkColumnarBroadleafInstances,
        Shrub02Instances,
        Fern02Instances,
        Moss01Instances,
        BermudaGrassInstances,
        AmbientCgNearTurfInstances};
    const TArray<int32> Counts = {
        ExpectedIslandTreeCount,
        ExpectedDarkColumnarBroadleafCount,
        ExpectedShrubCount,
        ExpectedFernCount,
        ExpectedMossCount,
        ExpectedBermudaGrassCount,
        ExpectedNearTurfCount};
    const TArray<FString> Paths = {
        IslandTreeObjectPath,
        V2BroadleafObjectPath,
        ShrubObjectPath,
        FernObjectPath,
        MossObjectPath,
        BermudaObjectPath,
        TurfCardObjectPath};
    if (!GetActorTransform().Equals(FTransform::Identity, 0.001f) ||
        ClaimLabel != TEXT("ISTANA_EXPLORE_V3_LAWFUL_PUBLIC_SOURCE_VISUAL_APPROXIMATION_NOT_ONE_TO_ONE_NOT_SURVEY_NOT_SENSOR_TRUTH") ||
        bOneToOneOneKilometerClaimed || bSurveyAccurateTerrainClaimed ||
        bExactBotanicalInventoryClaimed || bSensorTruthAuthority ||
        bGoogleOrOneMapPixelsUsed || bPorticoCollisionAuthority ||
        !bV2ColumnarVisualsReplacedWithExactTransforms ||
        bPublicDistributionReady ||
        DeterministicPlacementSeed != 0x3757A6A5 ||
        !FMath::IsFinite(BaseWindStrengthCm) ||
        !FMath::IsFinite(GustPeakStrengthCm) ||
        !FMath::IsFinite(WindSpeed) ||
        !FMath::IsFinite(RecoveryFrequencyHz) ||
        !FMath::IsFinite(RecoveryDampingRatio) ||
        !FMath::IsNearlyEqual(BaseWindStrengthCm, 5.0f, 0.001f) ||
        !FMath::IsNearlyEqual(GustPeakStrengthCm, 46.0f, 0.001f) ||
        !FMath::IsNearlyEqual(WindSpeed, 1.42f, 0.001f) ||
        !PrevailingWindDirection.Equals(FVector2D(0.93f, 0.37f), 0.001f) ||
        !FMath::IsNearlyEqual(RecoveryFrequencyHz, 0.62f, 0.001f) ||
        !FMath::IsNearlyEqual(RecoveryDampingRatio, 0.30f, 0.001f) ||
        WindStrengthParameter != FName(TEXT("TRIAD_WindStrengthCm")) ||
        WindSpeedParameter != FName(TEXT("TRIAD_WindSpeed")) ||
        WindDirectionParameter != FName(TEXT("TRIAD_WindDirection")) ||
        FrozenLawfulSourceManifestSha256 !=
            TEXT("CA26023F45BAA1344ED0DC031766355D734EC9B89C8B25F089ADFA18FEA12290") ||
        FrozenPorticoObjSha256 !=
            TEXT("164624CABCF0B093A7D53F87F0A3ACA5B009A001ABCA9CBD3950ECFC6336FFFD"))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: identity, source binding, seed, or truthful claim boundary changed.");
        return false;
    }
    const TArray<FString> ExpectedAttribution = {
        TEXT("© OpenStreetMap contributors; https://www.openstreetmap.org/copyright"),
        TEXT("Contains information from Heritage Trees and Master Plan 2019 Road Graphic accessed on 25 August 2026 from data.gov.sg under the Singapore Open Data Licence 1.0."),
        TEXT("Terrain derivative produced using Copernicus WorldDEM-30 © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights reserved."),
        TEXT("The organisations in charge of the Copernicus programme by law or by delegation do not incur any liability for any use of the Copernicus WorldDEM-30.")};
    if (RequiredDistributionAttribution != ExpectedAttribution)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: exact OSM/Singapore Open Data/Copernicus attribution roster changed.");
        return false;
    }

    if (PreservedV2ColumnarWorldTransforms.Num() != ExpectedDarkColumnarBroadleafCount ||
        !DarkColumnarBroadleafInstances->bOverrideMinLOD ||
        DarkColumnarBroadleafInstances->MinLOD != 1 ||
        DarkColumnarBroadleafInstances->ForcedLodModel != 0 ||
        !DarkColumnarBroadleafInstances->GetStaticMesh() ||
        DarkColumnarBroadleafInstances->GetStaticMesh()->GetMinLODIdx() != 1 ||
        ColumnarTrunkMaterialSlot == INDEX_NONE ||
        ColumnarBranchMaterialSlot == INDEX_NONE ||
        ColumnarLeafMaterialSlot == INDEX_NONE ||
        !DarkColumnarBroadleafInstances->GetMaterial(ColumnarTrunkMaterialSlot) ||
        !DarkColumnarBroadleafInstances->GetMaterial(ColumnarBranchMaterialSlot) ||
        !DarkColumnarBroadleafInstances->GetMaterial(ColumnarLeafMaterialSlot) ||
        !HasExactMaterialOrRuntimeMidParent(
            DarkColumnarBroadleafInstances->GetMaterial(ColumnarTrunkMaterialSlot),
            V2TrunkWindMaterialPath) ||
        !HasExactMaterialOrRuntimeMidParent(
            DarkColumnarBroadleafInstances->GetMaterial(ColumnarBranchMaterialSlot),
            V2BranchWindMaterialPath) ||
        !HasExactMaterialOrRuntimeMidParent(
            DarkColumnarBroadleafInstances->GetMaterial(ColumnarLeafMaterialSlot),
            V3DarkLeafWindMaterialPath))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: the exact-transform dark columnar mesh/MinLOD/material continuity contract changed.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedDarkColumnarBroadleafCount; ++Index)
    {
        FTransform ReplacementWorldTransform;
        if (!DarkColumnarBroadleafInstances->GetInstanceTransform(
                Index, ReplacementWorldTransform, true) ||
            !ReplacementWorldTransform.Equals(
                PreservedV2ColumnarWorldTransforms[Index], 0.001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: a dark columnar replacement no longer matches its V2 world transform exactly.");
            return false;
        }
    }
    for (int32 Index = 0; Index < VisualHisMs.Num(); ++Index)
    {
        const UHierarchicalInstancedStaticMeshComponent* Component = VisualHisMs[Index];
        if (!Component || !Component->GetStaticMesh() ||
            Component->GetStaticMesh()->GetPathName() != Paths[Index] ||
            Component->GetInstanceCount() != Counts[Index] ||
            Component->GetNumMaterials() <= 0 ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->GetGenerateOverlapEvents() ||
            !IgnoresAllChannels(Component) ||
            !Component->IsVisible() || Component->bHiddenInGame)
        {
            OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: a visible vegetation HISM mesh/count/material/collision state changed.");
            return false;
        }
        for (int32 Instance = 0; Instance < Component->GetInstanceCount(); ++Instance)
        {
            FTransform Transform;
            if (!Component->GetInstanceTransform(Instance, Transform, false) ||
                Transform.ContainsNaN() || Transform.GetScale3D().GetMin() <= 0.001)
            {
                OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: a vegetation transform is missing, non-finite, or non-positive.");
                return false;
            }
            const FVector Meters = Transform.GetTranslation() / CentimetersPerMeter;
            const bool bFormalAxisRequiresClearWoodyPlanting =
                Component != AmbientCgNearTurfInstances;
            const bool bAxisIntrusion = bFormalAxisRequiresClearWoodyPlanting &&
                FMath::Abs(Meters.X) < 17.0 &&
                Meters.Y >= 0.0 && Meters.Y < 138.0;
            const bool bFountainIntrusion = FVector2D(
                Meters.X, Meters.Y - 95.0).SizeSquared() < FMath::Square(18.0);
            if (bAxisIntrusion || bFountainIntrusion)
            {
                OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: additive vegetation entered the protected formal axis/fountain clearance.");
                return false;
            }
            if (Component == AmbientCgNearTurfInstances)
            {
                const FVector Scale = Transform.GetScale3D();
                const bool bBuildingOrApron =
                    Meters.Y < 14.0 && FMath::Abs(Meters.X) < 82.0;
                const bool bEntranceWalk =
                    FMath::Abs(Meters.X) < 3.2 && Meters.Y < 70.0;
                const bool bFountainOrDrive = FVector2D(
                    Meters.X, Meters.Y - 95.0).SizeSquared() <
                    FMath::Square(25.0);
                if (Scale.Z < 0.50 - 0.001 || Scale.Z > 1.00 + 0.001 ||
                    FMath::Abs(Meters.X) > 97.0 || Meters.Y < 7.0 ||
                    Meters.Y > 221.0 || bBuildingOrApron ||
                    bEntranceWalk || bFountainOrDrive)
                {
                    OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: a 3-6 cm close-turf card escaped its lawn grid/building/hardscape/fountain exclusion contract.");
                    return false;
                }
            }
            else if (Component == BermudaGrassInstances)
            {
                const float PlacedHeightCm = Transform.GetScale3D().Z *
                    ExpectedBermudaImportedUpAxisSpanCm;
                if (PlacedHeightCm < MinimumBermudaPlacedHeightCm - 0.001f ||
                    PlacedHeightCm > MaximumBermudaPlacedHeightCm + 0.001f)
                {
                    OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: a Bermuda accent does not read back at the exact derived 3-6 cm placed height.");
                    return false;
                }
            }
        }
    }

    if (IslandTree01Instances->GetNumMaterials() != 3 ||
        !ImportedSlotHasExactMaterialOrMidParent(
            IslandTree01Instances,
            TEXT("island_tree_01"),
            V3MaterialPath(TEXT("M_IPVExploreV3_IslandTree01_Trunk_Wind"))) ||
        !ImportedSlotHasExactMaterialOrMidParent(
            IslandTree01Instances,
            TEXT("island_tree_01_branches"),
            V3MaterialPath(TEXT("M_IPVExploreV3_IslandTree01_Branches_Wind"))) ||
        !ImportedSlotHasExactMaterialOrMidParent(
            IslandTree01Instances,
            TEXT("island_tree_01_leaves"),
            V3MaterialPath(TEXT("M_IPVExploreV3_IslandTree01_Leaves_Wind"))) ||
        !AllSlotsHaveExactMaterialOrMidParent(
            Shrub02Instances,
            V3MaterialPath(TEXT("M_IPVExploreV3_Shrub02_Wind"))) ||
        !AllSlotsHaveExactMaterialOrMidParent(
            Fern02Instances,
            V3MaterialPath(TEXT("M_IPVExploreV3_Fern02_Wind"))) ||
        !AllSlotsHaveExactMaterialOrMidParent(
            Moss01Instances,
            V3MaterialPath(TEXT("M_IPVExploreV3_Moss01_Wind"))) ||
        !AllSlotsHaveExactMaterialOrMidParent(
            BermudaGrassInstances,
            V3MaterialPath(TEXT("M_IPVExploreV3_BermudaGrass_Wind"))) ||
        !AllSlotsHaveExactMaterialOrMidParent(
            AmbientCgNearTurfInstances,
            V3MaterialPath(TEXT("M_IPVExploreV3_CloseTurf_Wind"))))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: an exact vegetation material-slot/WPO base binding changed.");
        return false;
    }

    const UStaticMesh* IslandMesh = IslandTree01Instances->GetStaticMesh();
    const UStaticMesh* ShrubMesh = Shrub02Instances->GetStaticMesh();
    const UStaticMesh* FernMesh = Fern02Instances->GetStaticMesh();
    const UStaticMesh* MossMesh = Moss01Instances->GetStaticMesh();
    const UStaticMesh* BermudaMesh = BermudaGrassInstances->GetStaticMesh();
    const FStaticMeshRenderData* IslandRender = IslandMesh ? IslandMesh->GetRenderData() : nullptr;
    if (CountTriangles(IslandMesh, 0) !=
            ExpectedIslandImportedLod0Triangles ||
        CountTriangles(ShrubMesh, 0) !=
            ExpectedShrubImportedLod0Triangles ||
        CountTriangles(FernMesh, 0) !=
            ExpectedFernImportedLod0Triangles ||
        CountTriangles(MossMesh, 0) !=
            ExpectedMossImportedLod0Triangles ||
        CountTriangles(BermudaMesh, 0) !=
            ExpectedBermudaImportedLod0Triangles ||
        !BermudaMesh || !FMath::IsNearlyEqual(
            BermudaMesh->GetBounds().BoxExtent.Z * 2.0,
            static_cast<double>(ExpectedBermudaImportedUpAxisSpanCm),
            0.05))
    {
        OutReport = FString::Printf(
            TEXT("ISTANA_EXPLORE_V3_INVALID: UE5.5 imported LOD0 census drifted: Island expected %d actual %d; Shrub expected %d actual %d; Fern expected %d actual %d; Moss expected %d actual %d; Bermuda expected %d actual %d."),
            ExpectedIslandImportedLod0Triangles,
            CountTriangles(IslandMesh, 0),
            ExpectedShrubImportedLod0Triangles,
            CountTriangles(ShrubMesh, 0),
            ExpectedFernImportedLod0Triangles,
            CountTriangles(FernMesh, 0),
            ExpectedMossImportedLod0Triangles,
            CountTriangles(MossMesh, 0),
            ExpectedBermudaImportedLod0Triangles,
            CountTriangles(BermudaMesh, 0));
        return false;
    }
    if (!IslandMesh || IslandMesh->GetMinLODIdx() < 1 ||
        !IslandTree01Instances->bOverrideMinLOD ||
        IslandTree01Instances->MinLOD < 1 ||
        !IslandRender || IslandRender->LODResources.Num() < 4 ||
        CountTriangles(IslandMesh, 1) <= 0 || CountTriangles(IslandMesh, 1) > 150000 ||
        CountTriangles(IslandMesh, 2) <= 0 || CountTriangles(IslandMesh, 2) > 60000 ||
        CountTriangles(IslandMesh, 3) <= 0 || CountTriangles(IslandMesh, 3) > 20000)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: Island Tree runtime MinLOD/LOD1-3 caps no longer exclude the exact 1,599,403-triangle UE5.5 imported LOD0.");
        return false;
    }
    if (!MossMesh || MossMesh->GetMinLODIdx() < 1 ||
        !Moss01Instances->bOverrideMinLOD || Moss01Instances->MinLOD < 1 ||
        CountTriangles(MossMesh, 1) <= 0 || CountTriangles(MossMesh, 1) > 20000 ||
        CountTriangles(MossMesh, 2) <= 0 || CountTriangles(MossMesh, 2) > 6000 ||
        !BermudaMesh || BermudaMesh->GetMinLODIdx() < 1 ||
        !BermudaGrassInstances->bOverrideMinLOD || BermudaGrassInstances->MinLOD < 1 ||
        CountTriangles(BermudaMesh, 1) <= 0 || CountTriangles(BermudaMesh, 1) > 12000 ||
        CountTriangles(BermudaMesh, 2) <= 0 || CountTriangles(BermudaMesh, 2) > 3000)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: sparse moss/Bermuda accents must use bounded derived LOD1+; raw LOD0 is nonselectable.");
        return false;
    }
    int32 TurfStartCull = 0;
    int32 TurfEndCull = 0;
    int32 BermudaStartCull = 0;
    int32 BermudaEndCull = 0;
    AmbientCgNearTurfInstances->GetCullDistances(TurfStartCull, TurfEndCull);
    BermudaGrassInstances->GetCullDistances(BermudaStartCull, BermudaEndCull);
    if (CountTriangles(AmbientCgNearTurfInstances->GetStaticMesh(), 0) != 8 ||
        TurfStartCull != 800 || TurfEndCull != 10000 ||
        BermudaStartCull != 1200 || BermudaEndCull != 14000 ||
        ExpectedBermudaGrassCount > 24 || ExpectedMossCount > 24 ||
        ExpectedNearTurfCount != 18432 ||
        ExpectedNearTurfCount * 8 != 147456)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: close-turf card triangle/cull gate or sparse raw-source census changed.");
        return false;
    }

    if (!SupplementalTreePawnBlockers ||
        SupplementalTreePawnBlockers->GetInstanceCount() != ExpectedIslandTreeCount ||
        SupplementalTreePawnBlockers->GetCollisionEnabled() != ECollisionEnabled::QueryOnly ||
        SupplementalTreePawnBlockers->GetGenerateOverlapEvents() ||
        SupplementalTreePawnBlockers->IsVisible() ||
        !SupplementalTreePawnBlockers->bHiddenInGame)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: supplemental hidden Pawn-only blocker roster changed.");
        return false;
    }
    FCollisionResponseContainer ExpectedPawnOnlyResponses(ECR_Ignore);
    ExpectedPawnOnlyResponses.SetResponse(ECC_Pawn, ECR_Block);
    if (SupplementalTreePawnBlockers->GetCollisionResponseToChannels() !=
        ExpectedPawnOnlyResponses)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: a supplemental blocker affects a non-Pawn channel.");
        return false;
    }

    if (!HasExactMeshPath(PorticoV7RenderOnlyComponent, PorticoObjectPath) ||
        !PorticoV7RenderOnlyComponent->GetRelativeTransform().Equals(FTransform::Identity, 0.001f) ||
        !PorticoV7RenderOnlyComponent->GetComponentTransform().Equals(FTransform::Identity, 0.001f) ||
        PorticoV7RenderOnlyComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        PorticoV7RenderOnlyComponent->GetGenerateOverlapEvents() ||
        !IgnoresAllChannels(PorticoV7RenderOnlyComponent) ||
        CountTriangles(PorticoV7RenderOnlyComponent->GetStaticMesh(), 0) != 5816)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: V5-native V7 portico must remain an exact identity, 5,816-triangle, render-only sibling.");
        return false;
    }
    FString PorticoMaterialError;
    if (!ValidatePorticoMaterialTopology(
            PorticoV7RenderOnlyComponent,
            PorticoMaterialError))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: ") + PorticoMaterialError;
        return false;
    }

    const TArray<const UStaticMeshComponent*> ContextComponents = {
        LowFrequencyTerrainComponent,
        OsmPublicRoadsComponent,
        UraIndicativeRoadsComponent,
        OsmWaterComponent};
    int32 ContextMeshCount = 0;
    for (const UStaticMeshComponent* Component : ContextComponents)
    {
        ContextMeshCount += Component && Component->GetStaticMesh() ? 1 : 0;
        if (!Component ||
            Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
            Component->GetGenerateOverlapEvents() ||
            !IgnoresAllChannels(Component) ||
            !Component->GetRelativeTransform().Equals(FTransform::Identity, 0.001f))
        {
            OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: geospatial visual context collision/identity policy changed.");
            return false;
        }
    }
    if (LowFrequencyTerrainComponent->IsVisible() ||
        !LowFrequencyTerrainComponent->bHiddenInGame ||
        OsmPublicRoadsComponent->IsVisible() ||
        !OsmPublicRoadsComponent->bHiddenInGame ||
        UraIndicativeRoadsComponent->IsVisible() ||
        !UraIndicativeRoadsComponent->bHiddenInGame ||
        OsmWaterComponent->IsVisible() || !OsmWaterComponent->bHiddenInGame)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: coarse terrain/OSM/URA/water must remain hidden diagnostics until a separate registered gameplay/collision revision.");
        return false;
    }
    if ((bCompleteGeneratedContextConfigured &&
            (ContextMeshCount != 4 || GeneratedContextManifestSha256.Len() != 64)) ||
        (!bCompleteGeneratedContextConfigured &&
            (ContextMeshCount != 0 || !GeneratedContextManifestSha256.IsEmpty())))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V3_INVALID: generated context is partial or its manifest binding is absent.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("Validated additive Explore V3 supplement: %d CC0 island-tree mid-storey forms (runtime LOD1+ only), exactly %d darker non-botanical Jacaranda columnar replacements at preserved V2 transforms, %d shrub, %d fern, %d shaded moss, %d Bermuda edge-grass and %d ambientCG near-turf instances; %d separate new Pawn-only blockers while aligned V2 blockers remain preserved; identity render-only V7 portico; generated context %s and hidden diagnostic. V2 free-roam, terrain/collision, remaining wind composition and OSM/HDB layer must remain exact and visible under the map gate. This is not hyperreal; one-to-one, survey, exact botanical inventory and sensor-authority claims remain false."),
        ExpectedIslandTreeCount,
        ExpectedDarkColumnarBroadleafCount,
        ExpectedShrubCount,
        ExpectedFernCount,
        ExpectedMossCount,
        ExpectedBermudaGrassCount,
        ExpectedNearTurfCount,
        ExpectedIslandTreeCount,
        bCompleteGeneratedContextConfigured ? TEXT("complete") : TEXT("not staged"));
    return true;
}
